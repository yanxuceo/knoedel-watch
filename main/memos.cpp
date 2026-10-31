#include "memos.h"

#include <dirent.h>
#include <encoder/impl/esp_opus_enc.h>
#include <esp_heap_caps.h>
#include <esp_littlefs.h>
#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <new>

#include "board.h"

namespace memos {

static const char *TAG = "memos";
static const char *MEMOS = "/notes/memos", *DONE = "/notes/done";

// ---- the partition ------------------------------------------------------------

static bool make_task();

bool begin()
{
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = "/notes";
    conf.partition_label = "notes";
    conf.format_if_mount_failed = true;
    if (esp_vfs_littlefs_register(&conf) != ESP_OK) {
        ESP_LOGE(TAG, "the notes partition cannot be mounted");
        return false;
    }
    mkdir(MEMOS, 0775);
    mkdir(DONE, 0775);
    // A recording cut off by a restart (".part") and a note half written (".tmp") are dropped.
    for (const char *dir : {MEMOS, DONE}) {
        if (DIR *d = opendir(dir)) {
            while (dirent *e = readdir(d)) {
                const char *dot = strrchr(e->d_name, '.');
                if (dot != nullptr && (strcmp(dot, ".part") == 0 || strcmp(dot, ".tmp") == 0)) {
                    unlink((std::string(dir) + "/" + e->d_name).c_str());
                }
            }
            closedir(d);
        }
    }
    size_t used = 0, total = 0;
    usage(used, total);
    ESP_LOGI(TAG, "%u memos waiting, %u notes, %u of %u KB used", (unsigned)waiting().size(), (unsigned)notes().size(),
             (unsigned)(used / 1024), (unsigned)(total / 1024));
    return make_task();
}

void usage(size_t &used, size_t &total) { esp_littlefs_info("notes", &total, &used); }

// ---- gain ---------------------------------------------------------------------------

// Raw speech near the watch is about -45 dBFS (rms); x10 brings it to about
// -25 dBFS. Above LIMIT the samples are bent towards full scale instead of
// being cut off (a shout, the click of the key).
static constexpr float GAIN = 10, LIMIT = 20000, ROOM = 32767 - LIMIT;

void apply_gain(int16_t *pcm, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        float y = pcm[i] * GAIN;
        const float a = fabsf(y);
        if (a > LIMIT) {
            y = copysignf(LIMIT + ROOM * tanhf((a - LIMIT) / ROOM), y);
        }
        pcm[i] = (int16_t)lrintf(y);
    }
}

// ---- Ogg ------------------------------------------------------------------------
// Opus packets in Ogg pages (RFC 7845): a page with OpusHead, one with
// OpusTags, then the audio, about half a second per page.

static constexpr int PRE_SKIP = 312;              // the encoder's look-ahead, in 48 kHz samples
static constexpr int FRAME = RATE / 50;           // 20 ms
static constexpr int PACKETS_PER_PAGE = 25;
static constexpr size_t PAGE_BODY = 8192;

static uint32_t crc_table[256];

static uint32_t ogg_crc(uint32_t crc, const uint8_t *p, size_t n)
{
    if (crc_table[1] == 0) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t r = i << 24;
            for (int k = 0; k < 8; k++) {
                r = r & 0x80000000u ? r << 1 ^ 0x04C11DB7u : r << 1;
            }
            crc_table[i] = r;
        }
    }
    while (n--) {
        crc = crc << 8 ^ crc_table[(crc >> 24 ^ *p++) & 0xFF];
    }
    return crc;
}

struct Ogg {
    FILE *f = nullptr;
    uint32_t serial = 0, sequence = 0;
    uint64_t samples48 = 0;  // audio so far, at 48 kHz
    uint8_t body[PAGE_BODY];
    uint8_t lacing[255];
    size_t body_len = 0;
    int segments = 0, packets = 0;

    void page(uint8_t flags, uint64_t granule)
    {
        uint8_t h[27];
        memcpy(h, "OggS", 4);
        h[4] = 0;
        h[5] = flags;
        for (int i = 0; i < 8; i++) {
            h[6 + i] = (uint8_t)(granule >> (8 * i));
        }
        for (int i = 0; i < 4; i++) {
            h[14 + i] = (uint8_t)(serial >> (8 * i));
            h[18 + i] = (uint8_t)(sequence >> (8 * i));
            h[22 + i] = 0;
        }
        h[26] = (uint8_t)segments;
        uint32_t crc = ogg_crc(0, h, 27);
        crc = ogg_crc(crc, lacing, segments);
        crc = ogg_crc(crc, body, body_len);
        for (int i = 0; i < 4; i++) {
            h[22 + i] = (uint8_t)(crc >> (8 * i));
        }
        fwrite(h, 1, 27, f);
        fwrite(lacing, 1, segments, f);
        fwrite(body, 1, body_len, f);
        sequence++;
        body_len = 0;
        segments = packets = 0;
    }

    // Adds a packet to the page being filled (flushing it first when full).
    void packet(const uint8_t *data, size_t len, uint64_t granule_if_flushed)
    {
        const int needed = (int)(len / 255) + 1;
        if (packets >= PACKETS_PER_PAGE || segments + needed > 255 || body_len + len > PAGE_BODY) {
            page(0, granule_if_flushed);
        }
        memcpy(body + body_len, data, len);
        body_len += len;
        for (size_t left = len;; left -= 255) {
            lacing[segments++] = (uint8_t)(left >= 255 ? 255 : left);
            if (left < 255) {
                break;
            }
        }
        packets++;
    }

    bool open(const char *path)
    {
        f = fopen(path, "wb");
        if (f == nullptr) {
            return false;
        }
        serial = esp_random();
        sequence = 0;
        samples48 = 0;
        static const uint8_t head[19] = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 1, PRE_SKIP & 0xFF, PRE_SKIP >> 8,
                                         RATE & 0xFF, (RATE >> 8) & 0xFF, 0, 0, 0, 0, 0};
        packet(head, sizeof head, 0);
        page(0x02, 0);  // beginning of stream
        static const char vendor[] = "knoedel-watch";
        uint8_t tags[8 + 4 + sizeof vendor - 1 + 4];
        memcpy(tags, "OpusTags", 8);
        const uint32_t vl = sizeof vendor - 1;
        memcpy(tags + 8, &vl, 4);
        memcpy(tags + 12, vendor, vl);
        memset(tags + 12 + vl, 0, 4);  // no comments
        packet(tags, sizeof tags, 0);
        page(0, 0);
        return true;
    }

    void audio(const uint8_t *data, size_t len)
    {
        packet(data, len, PRE_SKIP + samples48);
        samples48 += FRAME * (48000 / RATE);
    }

    void close()
    {
        if (f == nullptr) {
            return;
        }
        if (packets > 0) {
            page(0x04, PRE_SKIP + samples48);  // end of stream
        }
        fclose(f);
        f = nullptr;
    }
};

// ---- recording ----------------------------------------------------------------------

static constexpr size_t CHUNK = RATE / 10;  // 100 ms per microphone request
static constexpr int BUFFERS = 4;

static struct {
    bool on;
    std::atomic<bool> started, stop, keep, full;
    SemaphoreHandle_t signal;  // the task has started, and has finished
    std::string result;        // the id kept
    Ogg *ogg;
    void *enc;
    uint8_t *out;
    int out_size;
    int16_t *bufs[BUFFERS];
    int head, tail;
    std::atomic<int> inflight;
    std::atomic<size_t> samples;
    std::atomic<float> level;
    char id[24], path[48];
} rec;

static std::string new_id()
{
    board::DateTime t = {};
    char base[40];
    if (board::get_time(t)) {
        snprintf(base, sizeof base, "%04d%02d%02d-%02d%02d%02d", t.year, t.month, t.day, t.hour, t.minute, t.second);
    } else {
        snprintf(base, sizeof base, "00000000-000000");
    }
    std::string id = base;
    struct stat st;
    for (int n = 2; stat((std::string(MEMOS) + "/" + id + ".ogg").c_str(), &st) == 0 ||
                    stat((std::string(DONE) + "/" + id + ".json").c_str(), &st) == 0;
         n++) {
        id = std::string(base) + "-" + std::to_string(n);
    }
    return id;
}

// The encoder, the file and the microphone (in the recording task).
static bool open_all()
{
    esp_opus_enc_config_t cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    cfg.sample_rate = RATE;
    cfg.channel = 1;
    cfg.bitrate = 20000;  // speech, about 2.5 KB a second
    cfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS;
    cfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP;
    cfg.complexity = 2;
    cfg.enable_vbr = true;
    int in_size = 0;
    if (esp_opus_enc_open(&cfg, sizeof cfg, &rec.enc) != ESP_AUDIO_ERR_OK ||
        esp_opus_enc_get_frame_size(rec.enc, &in_size, &rec.out_size) != ESP_AUDIO_ERR_OK ||
        in_size != FRAME * 2) {
        ESP_LOGE(TAG, "the Opus encoder did not start");
        return false;
    }
    rec.out = (uint8_t *)malloc(rec.out_size);
    strcpy(rec.id, new_id().c_str());
    snprintf(rec.path, sizeof rec.path, "%s/%s.ogg.part", MEMOS, rec.id);
    if (rec.out == nullptr || !rec.ogg->open(rec.path) || !board::mic_begin()) {
        ESP_LOGE(TAG, "the recording did not start");
        rec.ogg->close();
        esp_opus_enc_close(rec.enc);
        free(rec.out);
        return false;
    }
    rec.head = rec.tail = rec.inflight = 0;
    rec.samples = 0;
    rec.level = 0;
    return true;
}

// Gain, level and encoding for one filled chunk.
static void take(int16_t *pcm)
{
    apply_gain(pcm, CHUNK);
    int peak = 0;
    for (size_t i = 0; i < CHUNK; i++) {
        peak = std::max(peak, abs(pcm[i]));
    }
    rec.level = peak / 32767.0f;
    for (size_t at = 0; at + FRAME <= CHUNK; at += FRAME) {
        esp_audio_enc_in_frame_t in = {(uint8_t *)(pcm + at), FRAME * 2};
        esp_audio_enc_out_frame_t out = {rec.out, (uint32_t)rec.out_size, 0, 0};
        if (esp_opus_enc_process(rec.enc, &in, &out) == ESP_AUDIO_ERR_OK && out.encoded_bytes > 0) {
            rec.ogg->audio(rec.out, out.encoded_bytes);
        }
    }
    rec.samples += CHUNK;
}

// Asks the microphone for more and takes what it filled.
static void pump()
{
    while (rec.inflight < 2 && rec.samples + (rec.inflight + 1) * CHUNK <= MAX_SECONDS * RATE &&
           board::mic_record(rec.bufs[rec.tail], CHUNK, RATE)) {
        rec.tail = (rec.tail + 1) % BUFFERS;
        rec.inflight++;
    }
    // The microphone fills the requests in order: those no longer busy are done.
    for (int done = rec.inflight - board::mic_busy(); done > 0; done--) {
        take(rec.bufs[rec.head]);
        rec.head = (rec.head + 1) % BUFFERS;
        rec.inflight--;
    }
}

// Closes the recording: kept under its final name, or deleted.
static void close_all(bool keep)
{
    board::mic_end();  // waits for the requests still being filled
    while (rec.inflight > 0) {
        take(rec.bufs[rec.head]);
        rec.head = (rec.head + 1) % BUFFERS;
        rec.inflight--;
    }
    rec.ogg->close();
    esp_opus_enc_close(rec.enc);
    free(rec.out);
    rec.result.clear();
    if (!keep) {
        unlink(rec.path);
        return;
    }
    const std::string final_path = std::string(MEMOS) + "/" + rec.id + ".ogg";
    if (rename(rec.path, final_path.c_str()) != 0) {
        ESP_LOGE(TAG, "the memo could not be kept");
        unlink(rec.path);
        return;
    }
    struct stat st = {};
    stat(final_path.c_str(), &st);
    ESP_LOGI(TAG, "memo %s: %.1f s, %ld bytes", rec.id, rec.samples.load() / (float)RATE, (long)st.st_size);
    rec.result = rec.id;
}

// The Opus encoder needs about 40 KB of stack, twice what the main task has:
// the recording runs in a task of its own, with its stack in internal RAM (it
// writes to flash). The task is made at start and stays: made for each
// recording, it found no 40 KB in one piece once Wi-Fi and TLS had been used.
static constexpr uint32_t TASK_STACK = 40 * 1024;
static TaskHandle_t task_handle;

static void task(void *)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // record_start()
        rec.started = open_all();
        xSemaphoreGive(rec.signal);
        if (!rec.started) {
            continue;
        }
        while (!rec.stop) {
            pump();
            rec.full = rec.samples + rec.inflight * CHUNK >= MAX_SECONDS * RATE;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        close_all(rec.keep);
        xSemaphoreGive(rec.signal);
    }
}

static bool make_task()
{
    rec.ogg = (Ogg *)heap_caps_calloc(1, sizeof(Ogg), MALLOC_CAP_SPIRAM);
    for (auto &b : rec.bufs) {
        b = (int16_t *)heap_caps_malloc(CHUNK * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    }
    rec.signal = xSemaphoreCreateBinary();
    if (rec.ogg == nullptr || rec.bufs[BUFFERS - 1] == nullptr || rec.signal == nullptr) {
        return false;
    }
    new (rec.ogg) Ogg();
    if (xTaskCreatePinnedToCore(task, "memo", TASK_STACK, nullptr, 4, &task_handle, 1) != pdPASS) {
        ESP_LOGE(TAG, "no memory for the recording task");
        return false;
    }
    return true;
}

bool record_start()
{
    if (task_handle == nullptr) {
        return false;
    }
    rec.stop = rec.full = false;
    xTaskNotifyGive(task_handle);
    xSemaphoreTake(rec.signal, portMAX_DELAY);
    rec.on = rec.started;
    return rec.on;
}

bool record_poll() { return rec.on && !rec.full; }

float record_seconds() { return (rec.samples + rec.inflight * CHUNK) / (float)RATE; }

float level() { return rec.level; }

std::string record_stop(bool keep)
{
    if (!rec.on) {
        return "";
    }
    rec.keep = keep;
    rec.stop = true;
    xSemaphoreTake(rec.signal, portMAX_DELAY);
    rec.on = false;
    return rec.result;
}

// ---- what is stored -----------------------------------------------------------------

static std::vector<std::string> list(const char *dir, const char *ext)
{
    std::vector<std::string> ids;
    if (DIR *d = opendir(dir)) {
        while (dirent *e = readdir(d)) {
            const size_t n = strlen(e->d_name), m = strlen(ext);
            if (n > m && strcmp(e->d_name + n - m, ext) == 0) {
                ids.emplace_back(e->d_name, n - m);
            }
        }
        closedir(d);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> waiting() { return list(MEMOS, ".ogg"); }

std::vector<std::string> notes()
{
    auto ids = list(DONE, ".json");
    std::reverse(ids.begin(), ids.end());
    return ids;
}

uint8_t *read(const std::string &id, size_t &size)
{
    FILE *f = fopen((std::string(MEMOS) + "/" + id + ".ogg").c_str(), "rb");
    if (f == nullptr) {
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = (uint8_t *)heap_caps_malloc(size ? size : 1, MALLOC_CAP_SPIRAM);
    if (data != nullptr && fread(data, 1, size, f) != size) {
        free(data);
        data = nullptr;
    }
    fclose(f);
    return data;
}

bool save_note(const std::string &id, const std::string &note_json)
{
    const std::string tmp = std::string(DONE) + "/" + id + ".json.tmp", path = std::string(DONE) + "/" + id + ".json";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool written = fwrite(note_json.data(), 1, note_json.size(), f) == note_json.size();
    if (fclose(f) != 0 || !written || rename(tmp.c_str(), path.c_str()) != 0) {
        unlink(tmp.c_str());
        return false;
    }
    return true;
}

bool finish(const std::string &id, const std::string &note_json)
{
    if (!save_note(id, note_json)) {
        return false;
    }
    unlink((std::string(MEMOS) + "/" + id + ".ogg").c_str());
    return true;
}

void remove_note(const std::string &id) { unlink((std::string(DONE) + "/" + id + ".json").c_str()); }

void remove_memo(const std::string &id) { unlink((std::string(MEMOS) + "/" + id + ".ogg").c_str()); }

size_t memo_size(const std::string &id)
{
    struct stat st = {};
    return stat((std::string(MEMOS) + "/" + id + ".ogg").c_str(), &st) == 0 ? (size_t)st.st_size : 0;
}

std::string note(const std::string &id)
{
    std::string s;
    if (FILE *f = fopen((std::string(DONE) + "/" + id + ".json").c_str(), "rb")) {
        char buf[256];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
            s.append(buf, n);
        }
        fclose(f);
    }
    return s;
}

}  // namespace memos
