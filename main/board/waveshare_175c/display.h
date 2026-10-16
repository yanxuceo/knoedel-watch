#pragma once
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_touch.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <lgfx/v1/LGFXBase.hpp>
#include <lgfx/v1/Touch.hpp>
#include <lgfx/v1/panel/Panel_FrameBufferBase.hpp>

namespace ws {
i2c_master_bus_handle_t i2c_bus();
bool speaker_begin();
class Touch_CST9217 final : public lgfx::ITouch {
  public:
    Touch_CST9217();
    bool init() override;
    bool isEnable() override { return _inited; }
    void wakeup() override {}
    void sleep() override {}
    uint_fast8_t getTouchRaw(lgfx::touch_point_t *points, uint_fast8_t count) override;

  private:
    esp_lcd_touch_handle_t _touch = nullptr;
};
// M5GFX renders into PSRAM. Only esp_lcd owns the panel and SPI transport.
// The DMA stripe remains owned by the transfer until completion is signalled.
class Panel_Waveshare175C final : public lgfx::Panel_FrameBufferBase {
  public:
    Panel_Waveshare175C();
    bool init(bool use_reset) override;
    lgfx::color_depth_t setColorDepth(lgfx::color_depth_t) override;
    void display(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h) override;
    void setBrightness(uint8_t value) override;
    void setSleep(bool sleep) override;

  private:
    static bool transferred(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *ctx);
    esp_lcd_panel_handle_t _lcd = nullptr;
    esp_lcd_panel_io_handle_t _io = nullptr;
    SemaphoreHandle_t _done = nullptr;
    uint8_t *_frame = nullptr, *_stripe = nullptr;
    uint8_t *_rows[466] = {};
    bool _sleeping = false;
};
} // namespace ws
class LGFX_Waveshare175C : public lgfx::LGFX_Device {
  public:
    LGFX_Waveshare175C() {
        _panel.setTouch(&_touch);
        setPanel(&_panel);
    }
    bool touchReady() { return _touch.isEnable(); }

  private:
    ws::Panel_Waveshare175C _panel;
    ws::Touch_CST9217 _touch;
};
