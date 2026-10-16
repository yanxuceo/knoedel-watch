#include "board.h"
#include "Button_Class.inl"
#include "Touch_Class.inl"
#include "board_pins.h"
#include "display.h"
#include <cstdlib>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>

namespace ws {
void clock_begin();
void save_clock();
} // namespace ws
namespace {
constexpr const char *TAG = "ws-board";
i2c_master_bus_handle_t shared_bus = nullptr;
i2c_master_dev_handle_t pmu = nullptr, imu = nullptr;
bool imu_ready = false, power_pressed = false;
uint32_t key_until = 0, key_poll_at = 0;
LGFX_Waveshare175C screen;
m5::Button_Class boot, power_key;
m5::Touch_Class touch;

bool read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t count = 1) {
    return dev && i2c_master_transmit_receive(dev, &reg, 1, data, count, 50) == ESP_OK;
}
bool write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value) {
    uint8_t data[] = {reg, value};
    return dev && i2c_master_transmit(dev, data, 2, 50) == ESP_OK;
}
bool change_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t clear, uint8_t set) {
    uint8_t value = 0;
    return read_reg(dev, reg, &value) && write_reg(dev, reg, (value & ~clear) | set);
}
i2c_master_dev_handle_t device(uint8_t address) {
    if (i2c_master_probe(shared_bus, address, 50) != ESP_OK)
        return nullptr;
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = address;
    cfg.scl_speed_hz = 400000;
    i2c_master_dev_handle_t handle = nullptr;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(shared_bus, &cfg, &handle));
    return handle;
}
} // namespace
namespace ws {
i2c_master_bus_handle_t i2c_bus() { return shared_bus; }
} // namespace ws
namespace board {
void begin() {
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = WS_I2C_SDA;
    cfg.scl_io_num = WS_I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &shared_bus));
    pmu = device(0x34);
    uint8_t id = 0;
    bool power_ready = read_reg(pmu, 3, &id) && id == 0x4a;
    if (!power_ready) {
        ESP_LOGE(TAG, "AXP2101 unavailable (id=%02x)", id);
        abort();
    }
    // Enable ADC reporting and the board's 3.3 V analog-audio rail. Preserve charge limits and all other rails.
    if (!change_reg(pmu, 0x30, 0, 5) || !change_reg(pmu, 0x92, 0x1f, 28) || !change_reg(pmu, 0x90, 0, 1))
        abort();
    if (!change_reg(pmu, 0x41, 0, 8) || !write_reg(pmu, 0x49, 8))
        abort();
    ESP_LOGI(TAG, "AXP2101 ready, ALDO1 3.3V enabled");
    ws::clock_begin();
    if (!screen.init()) {
        ESP_LOGE(TAG, "display init failed");
        abort();
    }
    screen.setBrightness(180);
    screen.fillScreen(TFT_BLACK);
    if (!screen.touchReady()) {
        ESP_LOGE(TAG, "touch init failed");
        abort();
    }
    touch.begin(&screen);
    gpio_config_t btn = {};
    btn.pin_bit_mask = 1ULL << WS_BTN_BOOT;
    btn.mode = GPIO_MODE_INPUT;
    btn.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&btn));
    imu = device(0x6b);
    if (!imu)
        imu = device(0x6a);
    if (read_reg(imu, 0, &id) && id == 5) {
        write_reg(imu, 0x60, 0xb0);
        vTaskDelay(pdMS_TO_TICKS(25));
        imu_ready = change_reg(imu, 2, 0, 0x40) && write_reg(imu, 8, 0) && write_reg(imu, 3, 0x16) &&
                    change_reg(imu, 6, 0x0f, 1) && write_reg(imu, 8, 1);
    }
    ESP_LOGI(TAG, "QMI8658 %s (4g, 125Hz)", imu_ready ? "ready" : "unavailable");
    if (!ws::speaker_begin())
        ESP_LOGE(TAG, "speaker unavailable");
}
lgfx::LGFX_Device &display() { return screen; }
Event update() {
    uint32_t now = esp_timer_get_time() / 1000;
    boot.setRawState(now, gpio_get_level(WS_BTN_BOOT) == 0);
    if (now - key_poll_at >= 25) {
        key_poll_at = now;
        uint8_t irq = 0;
        if (read_reg(pmu, 0x49, &irq) && (irq & 8)) {
            write_reg(pmu, 0x49, 8);
            key_until = now + 70;
            power_pressed = true;
        }
    }
    if (power_pressed && int32_t(now - key_until) >= 0)
        power_pressed = false;
    power_key.setRawState(now, power_pressed);
    touch.update(now);
    const auto detail = touch.getDetail();
    if (boot.wasHold())
        return Event::BACK;
    if (boot.wasClicked())
        return Event::PREVIOUS;
    if (power_key.wasClicked())
        return Event::NEXT;
    if (detail.wasFlicked() && abs(detail.distanceX()) > abs(detail.distanceY()))
        return detail.distanceX() < 0 ? Event::NEXT : Event::PREVIOUS;
    if (detail.wasClicked())
        return Event::SELECT;
    return Event::NONE;
}
bool touched() { return boot.isPressed() || power_key.isPressed() || touch.getDetail().isPressed(); }
bool has_motion() { return imu_ready; }
bool acceleration(float &x, float &y, float &z) {
    uint8_t data[6];
    if (!imu_ready || !read_reg(imu, 0x35, data, 6))
        return false;
    x = int16_t(data[0] | data[1] << 8) / 8192.f;
    y = int16_t(data[2] | data[3] << 8) / 8192.f;
    z = int16_t(data[4] | data[5] << 8) / 8192.f;
    return true;
}
bool on_usb() {
    uint8_t status = 0;
    // A failed PMIC read must not trigger an attempted battery shutdown.
    return !read_reg(pmu, 0, &status) || (status & 0x20);
}
void power_off() {
    ws::save_clock();
    stop_sound();
    if (on_usb())
        return;
    if (!change_reg(pmu, 0x10, 2, 1)) {
        ESP_LOGE(TAG, "AXP2101 shutdown request failed");
        return;
    }
    // The write only requests shutdown. Give the PMIC time to remove power
    // before the app is allowed to wake the display again. If USB is inserted
    // meanwhile, or power is still present after two seconds, shutdown failed.
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGW(TAG, "AXP2101 did not remove power");
}
} // namespace board
