#include "openai.h"

#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <string.h>

#include "credentials.h"

namespace openai {

static const char *TAG = "openai";
static const char *TRANSCRIBE_MODEL = "gpt-4o-mini-transcribe";
static const char *TIDY_MODEL = "gpt-5-mini";
// Describes the mix, so that the model writes each part as it was spoken. An example sentence
// here was given back as the transcript when the start of a memo was unclear: never use one.
static const char *TRANSCRIBE_PROMPT = "A voice memo. The speaker mixes Chinese, German and English, often within one sentence; write each word in the language it is spoken in.";

// Tried on real memos (tools/mic_test.py). Without the rule on reasons,
// "给房东打电话 wegen der Heizung" became two tasks, one made up; asked to tidy
// more freely, it wrote Milch as Milk; "早晨七点就得去" became a task of its own
// and "厕所的厕所的那个纸" stayed as said until the rule on times and repeated
// words. Reasoning "low" was 2-4 times slower and dropped a task.
// Days: counting them itself, the model put a Friday three weeks late, and even
// with a calendar to look in, "下周三" said on a Saturday came out a week late
// now and then. So it only says how a day was named ("when"), the same every
// time, and notes_sync.cpp counts.
static const char *TIDY_INSTRUCTIONS =
    "You turn a short voice memo into a note for a small watch screen. The memo may mix Chinese, German "
    "and English, even within one sentence. Copy the speaker's words exactly as spoken, each in its own "
    "language: never translate a single word (Milch stays Milch), never rephrase, never add a word. "
    "kind is 'todo' when the memo asks for things to do, else 'idea'. For a todo: one item per task, "
    "only the task itself: leave out filler and reminder words (嗯, 那个, 这个, 记得, 要, 然后, äh, um, I need to), "
    "the day words once they are in due (明天, 下周一, 之前, Freitag, tomorrow), and background that is "
    "not a task. A reason (wegen …, 因为 …) or a time of day (早晨七点, um 7 Uhr, at 9) stays in its task, it is "
    "not a task of its own. Inside a task, drop words said twice and fillers in the middle too "
    "(买一卷那个厕所的厕所的那个纸 becomes 买一卷厕所的纸); keep every other word. when: how the memo names "
    "that task's day, nothing more: type 'days' for 今天/明天/后天/heute/morgen/übermorgen/today/tomorrow (days "
    "0, 1, 2); 'weekday' for a day of the week (weekday 1 = Monday .. 7 = Sunday; next_week true only when the "
    "memo says 下周/下个星期/nächste Woche/next week, not for 这周/this/plain weekdays); 'date' for a calendar "
    "date (date as ISO, the year from today); 'none' when no day is named. A day said before several tasks "
    "joined by 然后/und/and counts for each of them. Never compute dates from weekdays yourself. text is ''. "
    "For an idea: items is empty "
    "and text is the memo without filler words.";

static const char *TIDY_SCHEMA =
    R"({"type":"object","additionalProperties":false,"required":["kind","items","text"],"properties":{)"
    R"("kind":{"type":"string","enum":["todo","idea"]},)"
    R"("items":{"type":"array","items":{"type":"object","additionalProperties":false,"required":["text","when"],)"
    R"("properties":{"text":{"type":"string"},"when":{"type":"object","additionalProperties":false,)"
    R"("required":["type","days","weekday","next_week","date"],"properties":{)"
    R"("type":{"type":"string","enum":["none","days","weekday","date"]},"days":{"type":["integer","null"]},)"
    R"("weekday":{"type":["integer","null"]},"next_week":{"type":"boolean"},"date":{"type":["string","null"]}}}}}},)"
    R"("text":{"type":"string"}}})";

// api.openai.com is checked against a few roots of our own, not the bundle:
// its chain ends with GTS Root R4 cross-signed by GlobalSign Root CA, which the
// bundle lacks, and the bundle's look-up by the last issuer then failed (it
// passed now and then with CROSS_SIGNED_VERIFY, then failed four times in a
// row). With the roots given, mbedTLS finds GTS Root R4 itself.
extern const char API_ROOTS[] asm("_binary_api_roots_pem_start");

static int last_status;
static bool last_spend_limited;
static bool in_session;
static esp_http_client_handle_t session;  // the connection kept between the requests of a session

void begin_session() { in_session = true; }

void end_session()
{
    if (session != nullptr) {
        esp_http_client_cleanup(session);
        session = nullptr;
    }
    in_session = false;
}

int status() { return last_status; }
bool spend_limited() { return last_spend_limited; }

// One POST: the body in parts (written one after the other), the answer back.
static std::string post(const char *path, const char *content_type, const void *const *parts, const size_t *sizes,
                        int count)
{
    last_status = 0;
    last_spend_limited = false;
    char key[256];
    if (!credentials::openai_key(key, sizeof key)) {
        ESP_LOGW(TAG, "no OpenAI key on the watch");
        return "";
    }
    std::string url = std::string("https://api.openai.com/v1/") + path;
    std::string auth = std::string("Bearer ") + key;
    memset(key, 0, sizeof key);

    size_t total = 0;
    for (int i = 0; i < count; i++) {
        total += sizes[i];
    }
    std::string answer;
    // In a session the connection of the last request is used again; one the
    // server has closed meanwhile fails, and the request goes once more on a new one.
    for (int attempt = 0; attempt < 2; attempt++) {
        const bool reused = session != nullptr;
        esp_http_client_handle_t client = session;
        if (client == nullptr) {
            esp_http_client_config_t cfg = {};
            cfg.url = url.c_str();
            cfg.method = HTTP_METHOD_POST;
            cfg.cert_pem = API_ROOTS;
            cfg.timeout_ms = 60000;
            cfg.buffer_size = 4096;
            cfg.buffer_size_tx = 2048;
            cfg.keep_alive_enable = true;
            client = esp_http_client_init(&cfg);
        } else {
            esp_http_client_set_url(client, url.c_str());
            esp_http_client_set_method(client, HTTP_METHOD_POST);
        }
        esp_http_client_set_header(client, "Authorization", auth.c_str());
        esp_http_client_set_header(client, "Content-Type", content_type);
        answer.clear();
        last_status = 0;
        if (esp_http_client_open(client, (int)total) != ESP_OK) {
            ESP_LOGW(TAG, "%s: no connection", path);
        } else {
            bool sent = true;
            for (int i = 0; i < count && sent; i++) {
                const char *p = (const char *)parts[i];
                for (size_t done = 0; done < sizes[i];) {
                    const int w = esp_http_client_write(client, p + done, (int)(sizes[i] - done));
                    if (w <= 0) {
                        sent = false;
                        break;
                    }
                    done += w;
                }
            }
            if (sent && esp_http_client_fetch_headers(client) >= 0) {
                last_status = esp_http_client_get_status_code(client);
                char buf[512];
                int r;
                while ((r = esp_http_client_read(client, buf, sizeof buf)) > 0) {
                    answer.append(buf, r);
                }
            } else {
                ESP_LOGW(TAG, "%s: the request broke off", path);
            }
        }
        if (in_session && last_status != 0) {
            session = client;
        } else {
            esp_http_client_cleanup(client);
            session = nullptr;
        }
        if (last_status != 0 || !reused) {
            break;
        }
    }
    if (last_status != 200) {
        last_spend_limited = last_status == 429 && answer.find("spend_limit") != std::string::npos;
        ESP_LOGW(TAG, "%s: HTTP %d %.200s", path, last_status, answer.c_str());
        return "";
    }
    return answer;
}

// The audio file in `parts` (one or more pieces) as a form upload.
static std::string transcribe_parts(const char *filename, const char *mime, const void *const *file_parts,
                                    const size_t *file_sizes, int file_count)
{
    const char *boundary = "knoedelwatchmemo7Hq2";
    std::string head;
    // The personal words (places, names) help the model spell what it hears.
    std::string prompt = TRANSCRIBE_PROMPT;
    std::string words = credentials::words();
    if (!words.empty()) {
        for (char &c : words) {
            c = c == '\n' ? ',' : c;
        }
        prompt += " Words that may come up: " + words + ".";
    }
    const char *fields[][2] = {{"model", TRANSCRIBE_MODEL}, {"prompt", prompt.c_str()}};
    for (const auto &f : fields) {
        head += std::string("--") + boundary + "\r\nContent-Disposition: form-data; name=\"" + f[0] + "\"\r\n\r\n" +
                f[1] + "\r\n";
    }
    head += std::string("--") + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + filename +
            "\"\r\nContent-Type: " + mime + "\r\n\r\n";
    const std::string tail = std::string("\r\n--") + boundary + "--\r\n";

    const void *parts[6] = {head.data()};
    size_t sizes[6] = {head.size()};
    for (int i = 0; i < file_count; i++) {
        parts[1 + i] = file_parts[i];
        sizes[1 + i] = file_sizes[i];
    }
    parts[1 + file_count] = tail.data();
    sizes[1 + file_count] = tail.size();
    const std::string content_type = std::string("multipart/form-data; boundary=") + boundary;
    const std::string answer = post("audio/transcriptions", content_type.c_str(), parts, sizes, file_count + 2);
    std::string text;
    if (cJSON *json = cJSON_Parse(answer.c_str())) {
        const cJSON *t = cJSON_GetObjectItem(json, "text");
        if (cJSON_IsString(t)) {
            text = t->valuestring;
        }
        cJSON_Delete(json);
    }
    return text;
}

std::string transcribe_ogg(const uint8_t *ogg, size_t size)
{
    const void *parts[] = {ogg};
    const size_t sizes[] = {size};
    return transcribe_parts("memo.ogg", "audio/ogg", parts, sizes, 1);
}

std::string transcribe(const int16_t *pcm, size_t samples, uint32_t rate)
{
    // A 44-byte WAV header for 16-bit mono.
    const uint32_t bytes = samples * 2;
    uint8_t wav[44];
    auto u32 = [&](int at, uint32_t v) { memcpy(wav + at, &v, 4); };
    auto u16 = [&](int at, uint16_t v) { memcpy(wav + at, &v, 2); };
    memcpy(wav, "RIFF", 4);
    u32(4, 36 + bytes);
    memcpy(wav + 8, "WAVEfmt ", 8);
    u32(16, 16);
    u16(20, 1);
    u16(22, 1);
    u32(24, rate);
    u32(28, rate * 2);
    u16(32, 2);
    u16(34, 16);
    memcpy(wav + 36, "data", 4);
    u32(40, bytes);

    const void *parts[] = {wav, pcm};
    const size_t sizes[] = {sizeof wav, bytes};
    return transcribe_parts("memo.wav", "audio/wav", parts, sizes, 2);
}

int check_key()
{
    const char *body = R"({"model":"gpt-5-mini","input":"ok","reasoning":{"effort":"minimal"},"max_output_tokens":16})";
    const void *parts[] = {body};
    const size_t sizes[] = {strlen(body)};
    post("responses", "application/json", parts, sizes, 1);
    return last_status;
}

std::string tidy(const std::string &transcript, const char *context)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "model", TIDY_MODEL);
    cJSON_AddStringToObject(body, "instructions", TIDY_INSTRUCTIONS);
    cJSON_AddStringToObject(body, "input", (std::string(context) + "\nMemo: " + transcript).c_str());
    cJSON_AddStringToObject(cJSON_AddObjectToObject(body, "reasoning"), "effort", "minimal");
    cJSON *format = cJSON_AddObjectToObject(cJSON_AddObjectToObject(body, "text"), "format");
    cJSON_AddStringToObject(format, "type", "json_schema");
    cJSON_AddStringToObject(format, "name", "note");
    cJSON_AddBoolToObject(format, "strict", true);
    cJSON_AddItemToObject(format, "schema", cJSON_Parse(TIDY_SCHEMA));
    char *json = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    const void *parts[] = {json};
    const size_t sizes[] = {strlen(json)};
    const std::string answer = post("responses", "application/json", parts, sizes, 1);
    cJSON_free(json);

    // The note is the text of the "message" item in "output".
    std::string note;
    if (cJSON *out = cJSON_Parse(answer.c_str())) {
        const cJSON *item;
        cJSON_ArrayForEach(item, cJSON_GetObjectItem(out, "output"))
        {
            const cJSON *type = cJSON_GetObjectItem(item, "type");
            if (!cJSON_IsString(type) || strcmp(type->valuestring, "message") != 0) {
                continue;
            }
            const cJSON *c;
            cJSON_ArrayForEach(c, cJSON_GetObjectItem(item, "content"))
            {
                const cJSON *t = cJSON_GetObjectItem(c, "text");
                if (cJSON_IsString(t)) {
                    note = t->valuestring;
                }
            }
        }
        cJSON_Delete(out);
    }
    return note;
}

}  // namespace openai
