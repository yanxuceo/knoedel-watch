#include "display.h"
#include "board_pins.h"
#include <algorithm>
#include <cstring>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_co5300.h>
#include <esp_lcd_touch_cst9217.h>
#include <esp_log.h>

namespace ws {
namespace {
constexpr int size = 466, stripe_rows = 16;
// Waveshare BSP 3.0.0, including panel-specific sleep-out delays.
const co5300_lcd_init_cmd_t commands[] = {
    {0xFE, (const uint8_t[]){0x20}, 1, 0},
    {0x19, (const uint8_t[]){0x10}, 1, 0},
    {0x1C, (const uint8_t[]){0xA0}, 1, 0},
    {0xFE, (const uint8_t[]){0}, 1, 0},
    {0xC4, (const uint8_t[]){0x80}, 1, 0},
    {0x3A, (const uint8_t[]){0x55}, 1, 0},
    {0x35, (const uint8_t[]){0}, 1, 0},
    {0x53, (const uint8_t[]){0x20}, 1, 0},
    {0x51, (const uint8_t[]){0xFF}, 1, 0},
    {0x63, (const uint8_t[]){0xFF}, 1, 0},
    {0x2A, (const uint8_t[]){0, 6, 1, 0xD7}, 4, 0},
    {0x2B, (const uint8_t[]){0, 0, 1, 0xD1}, 4, 600},
    {0x11, nullptr, 0, 600},
    {0x29, nullptr, 0, 0},
};
} // namespace
Panel_Waveshare175C::Panel_Waveshare175C() {
    _cfg.panel_width = _cfg.memory_width = size;
    _cfg.panel_height = _cfg.memory_height = size;
    _cfg.pin_cs = _cfg.pin_rst = _cfg.pin_busy = -1;
    _cfg.readable = true;
    _write_depth = _read_depth = lgfx::rgb565_2Byte;
    _auto_display = true;
}
lgfx::color_depth_t Panel_Waveshare175C::setColorDepth(lgfx::color_depth_t) {
    _write_depth = _read_depth = lgfx::rgb565_2Byte;
    return _write_depth;
}
bool Panel_Waveshare175C::transferred(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *ctx) {
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<Panel_Waveshare175C *>(ctx)->_done, &wake);
    return wake == pdTRUE;
}
bool Panel_Waveshare175C::init(bool) {
    if (_lcd)
        return true;
    _frame = static_cast<uint8_t *>(heap_caps_calloc(size * size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    _stripe = static_cast<uint8_t *>(heap_caps_malloc(size * stripe_rows * 2, MALLOC_CAP_DMA));
    _done = xSemaphoreCreateBinary();
    if (!_frame || !_stripe || !_done)
        return false;
    for (int y = 0; y < size; ++y)
        _rows[y] = _frame + y * size * 2;
    _lines_buffer = _rows;
    spi_bus_config_t bus = {};
    bus.sclk_io_num = WS_LCD_SCLK;
    bus.mosi_io_num = WS_LCD_DATA0;
    bus.miso_io_num = WS_LCD_DATA1;
    bus.quadwp_io_num = WS_LCD_DATA2;
    bus.quadhd_io_num = WS_LCD_DATA3;
    bus.max_transfer_sz = size * stripe_rows * 2;
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t io = {};
    io.cs_gpio_num = WS_LCD_CS;
    io.dc_gpio_num = GPIO_NUM_NC;
    io.spi_mode = 0;
    io.pclk_hz = 40000000;
    io.on_color_trans_done = transferred;
    io.user_ctx = this;
    io.lcd_cmd_bits = 32;
    io.lcd_param_bits = 8;
    io.flags.quad_mode = true;
    io.trans_queue_depth = 1;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &_io));
    co5300_vendor_config_t vendor = {};
    vendor.init_cmds = commands;
    vendor.init_cmds_size = sizeof(commands) / sizeof(commands[0]);
    vendor.flags.use_qspi_interface = true;
    esp_lcd_panel_dev_config_t panel = {};
    panel.reset_gpio_num = WS_LCD_RST;
    panel.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel.bits_per_pixel = 16;
    panel.vendor_config = &vendor;
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(_io, &panel, &_lcd));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(_lcd));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_lcd));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(_lcd, 6, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_lcd, true));
    if (!Panel_FrameBufferBase::init(false))
        return false;
    ESP_LOGI("ws-display", "official CO5300 ready: 466x466, PSRAM framebuffer, synchronous DMA stripes");
    return true;
}
void Panel_Waveshare175C::display(uint_fast16_t, uint_fast16_t, uint_fast16_t w, uint_fast16_t h) {
    // Drawing maintains a physical dirty rectangle. An explicit region request
    // gets a full refresh, covering all rotations without a second transform.
    if (w && h) {
        _range_mod.left = 0;
        _range_mod.top = 0;
        _range_mod.right = size - 1;
        _range_mod.bottom = size - 1;
    }
    if (!_lcd || _range_mod.empty() || _sleeping)
        return;
    int left = std::max(0, int(_range_mod.left) & ~1);
    int right = std::min(size - 1, int(_range_mod.right) | 1);
    int top = std::max(0, int(_range_mod.top)), bottom = std::min(size - 1, int(_range_mod.bottom));
    int width = right - left + 1;
    for (int row = top; row <= bottom; row += stripe_rows) {
        int rows = std::min(stripe_rows, bottom - row + 1);
        for (int n = 0; n < rows; ++n)
            memcpy(_stripe + n * width * 2, _rows[row + n] + left * 2, width * 2);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(_lcd, left, row, right + 1, row + rows, _stripe));
        if (xSemaphoreTake(_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGE("ws-display", "DMA completion timeout");
            abort(); // Never reuse a buffer still owned by DMA.
        }
    }
    _range_mod.top = _range_mod.left = INT16_MAX;
    _range_mod.right = _range_mod.bottom = 0;
}
void Panel_Waveshare175C::setBrightness(uint8_t value) {
    if (_io)
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(_io, 0x02005100, &value, 1));
}
void Panel_Waveshare175C::setSleep(bool sleep) {
    if (!_lcd || _sleeping == sleep)
        return;
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_lcd, !sleep));
    _sleeping = sleep;
    if (!sleep)
        display(0, 0, size, size);
}
Touch_CST9217::Touch_CST9217() {
    _cfg.x_min = _cfg.y_min = 0;
    _cfg.x_max = _cfg.y_max = size - 1;
    _cfg.bus_shared = false;
}
bool Touch_CST9217::init() {
    if (_touch)
        return true;
    esp_lcd_panel_io_i2c_config_t io = {};
    io.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST9217_ADDRESS;
    io.control_phase_bytes = 1;
    io.lcd_cmd_bits = 8;
    io.flags.disable_control_phase = true;
    io.scl_speed_hz = 400000;
    esp_lcd_panel_io_handle_t handle = nullptr;
    esp_lcd_touch_config_t cfg = {};
    cfg.x_max = cfg.y_max = size;
    cfg.rst_gpio_num = WS_TOUCH_RST;
    cfg.int_gpio_num = WS_TOUCH_INT;
    cfg.flags.mirror_x = cfg.flags.mirror_y = true;
    if (esp_lcd_new_panel_io_i2c(i2c_bus(), &io, &handle) != ESP_OK)
        return false;
    if (esp_lcd_touch_new_i2c_cst9217(handle, &cfg, &_touch) != ESP_OK) {
        esp_lcd_panel_io_del(handle);
        return false;
    }
    _inited = true;
    ESP_LOGI("ws-touch", "CST9217 ready");
    return true;
}
uint_fast8_t Touch_CST9217::getTouchRaw(lgfx::touch_point_t *out, uint_fast8_t count) {
    if (!_touch || !count || esp_lcd_touch_read_data(_touch) != ESP_OK)
        return 0;
    esp_lcd_touch_point_data_t point = {};
    uint8_t n = 0;
    if (esp_lcd_touch_get_data(_touch, &point, &n, 1) != ESP_OK || !n)
        return 0;
    out[0].x = std::min<int>(point.x, size - 1);
    out[0].y = std::min<int>(point.y, size - 1);
    out[0].id = 0;
    out[0].size = 1;
    return 1;
}
} // namespace ws
