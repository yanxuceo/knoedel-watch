#include "audio_mix.h"
#include "board.h"
#include "board_pins.h"
#include "display.h"
#include <atomic>
#include <cstdlib>
#include <driver/i2s_std.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/task.h>

namespace {
constexpr size_t chunk = 256;
SemaphoreHandle_t control = nullptr;
TaskHandle_t audio_task = nullptr;
esp_codec_dev_handle_t codec = nullptr;
std::atomic<uint8_t> volume{128};
ws::AudioMix mix;
int16_t *owned_pcm = nullptr;
bool amp_on = false;

// Called under control, just like every codec write. Four silent DMA buffers
// drain any queued samples before muting, including when stop_sound() returns.
void silence() {
    if (!amp_on)
        return;
    int16_t zero[chunk] = {};
    for (int i = 0; i < 4; ++i)
        esp_codec_dev_write(codec, zero, sizeof(zero));
    esp_codec_dev_set_out_mute(codec, true);
    gpio_set_level(WS_PA_EN, 0);
    amp_on = false;
}
void worker(void *) {
    int16_t pcm[chunk];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            xSemaphoreTake(control, portMAX_DELAY);
            if (!mix.active()) {
                silence();
                xSemaphoreGive(control);
                break;
            }
            if (!amp_on) {
                gpio_set_level(WS_PA_EN, 1);
                esp_codec_dev_set_out_mute(codec, false);
                amp_on = true;
            }
            mix.render(pcm, chunk, volume.load());
            bool ok = esp_codec_dev_write(codec, pcm, sizeof(pcm)) == ESP_CODEC_DEV_OK;
            if (mix.position == mix.samples || !ok) {
                free(owned_pcm);
                owned_pcm = nullptr;
                mix.set_pcm(nullptr, 0);
            }
            if (!ok) {
                mix.set_tone(0, 0);
                ESP_LOGE("ws-audio", "codec write failed");
            }
            xSemaphoreGive(control);
            vTaskDelay(1); // Allow the UI to stop/replace audio between chunks.
        }
    }
}
} // namespace
namespace ws {
bool speaker_begin() {
    gpio_reset_pin(WS_PA_EN);
    gpio_set_direction(WS_PA_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(WS_PA_EN, 0);
    i2s_chan_handle_t tx = nullptr;
    i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    cfg.auto_clear = true;
    cfg.dma_desc_num = 4;
    cfg.dma_frame_num = 256;
    if (i2s_new_channel(&cfg, &tx, nullptr) != ESP_OK)
        return false;
    i2s_std_config_t standard = {};
    standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(ws::audio_rate);
    standard.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    standard.gpio_cfg.mclk = WS_I2S_MCLK;
    standard.gpio_cfg.bclk = WS_I2S_BCLK;
    standard.gpio_cfg.ws = WS_I2S_WS;
    standard.gpio_cfg.dout = WS_I2S_DOUT;
    standard.gpio_cfg.din = GPIO_NUM_NC;
    if (i2s_channel_init_std_mode(tx, &standard) != ESP_OK || i2s_channel_enable(tx) != ESP_OK)
        return false;
    audio_codec_i2s_cfg_t data = {};
    data.port = I2S_NUM_1;
    data.tx_handle = tx;
    audio_codec_i2c_cfg_t ctrl = {};
    ctrl.port = I2C_NUM_0;
    ctrl.addr = ES8311_CODEC_DEFAULT_ADDR;
    ctrl.bus_handle = i2c_bus();
    es8311_codec_cfg_t es = {};
    es.ctrl_if = audio_codec_new_i2c_ctrl(&ctrl);
    es.gpio_if = audio_codec_new_gpio();
    es.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es.pa_pin = -1;
    es.master_mode = false;
    es.use_mclk = true;
    es.hw_gain.pa_voltage = 5.0;
    es.hw_gain.codec_dac_voltage = 3.3;
    esp_codec_dev_cfg_t dev = {};
    dev.dev_type = ESP_CODEC_DEV_TYPE_OUT;
    dev.codec_if = es8311_codec_new(&es);
    dev.data_if = audio_codec_new_i2s_data(&data);
    if (!dev.codec_if || !dev.data_if)
        return false;
    codec = esp_codec_dev_new(&dev);
    esp_codec_dev_sample_info_t format = {};
    format.sample_rate = ws::audio_rate;
    format.channel = 1;
    format.bits_per_sample = 16;
    if (!codec || esp_codec_dev_open(codec, &format) != ESP_CODEC_DEV_OK)
        return false;
    // This board's DAC/amp was verified at codec volume 100. User volume is a
    // software gain, preserving the known-good analog gain configuration.
    esp_codec_dev_set_out_vol(codec, 100);
    esp_codec_dev_set_out_mute(codec, true);
    control = xSemaphoreCreateMutex();
    if (!control || xTaskCreate(worker, "ws-speaker", 4096, nullptr, 5, &audio_task) != pdPASS)
        return false;
    ESP_LOGI("ws-audio", "ES8311 ready, 24kHz mono, MCLK GPIO16, PA GPIO46");
    return true;
}
} // namespace ws
namespace board {
void set_volume(uint8_t v) { volume.store(v); }
void stop_sound() {
    if (!audio_task)
        return;
    xSemaphoreTake(control, portMAX_DELAY);
    mix = {};
    free(owned_pcm);
    owned_pcm = nullptr;
    silence();
    xSemaphoreGive(control);
}
void play_pcm(const int16_t *pcm, size_t count, uint32_t input_rate, bool stereo) {
    if (!audio_task || !pcm || !count || input_rate < 8000 || input_rate > 96000)
        return;
    size_t frames = count / (stereo ? 2 : 1);
    if (!frames || frames > input_rate * 60ULL)
        return;
    size_t output_frames = (uint64_t(frames) * ws::audio_rate + input_rate - 1) / input_rate;
    auto *output =
        static_cast<int16_t *>(heap_caps_malloc(output_frames * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!output) {
        ESP_LOGW("ws-audio", "No memory for %u samples", unsigned(output_frames));
        return;
    }
    for (size_t i = 0; i < output_frames; ++i)
        output[i] = ws::resample(pcm, frames, stereo, input_rate, i);
    // The decoder can reuse its buffer immediately: only this copy is played.
    // A tone has its own state and continues alongside the replacement speech.
    xSemaphoreTake(control, portMAX_DELAY);
    free(owned_pcm);
    owned_pcm = output;
    mix.set_pcm(output, output_frames);
    xSemaphoreGive(control);
    xTaskNotifyGive(audio_task);
}
void tone(uint16_t hz, uint16_t ms) {
    if (!audio_task)
        return;
    xSemaphoreTake(control, portMAX_DELAY);
    mix.set_tone(hz, ms);
    xSemaphoreGive(control);
    xTaskNotifyGive(audio_task);
}
} // namespace board
