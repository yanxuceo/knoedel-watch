// Board pins of the Waveshare ESP32-S3-Touch-AMOLED-1.75C.
//
// Values from the official BSP (waveshare/esp32_s3_touch_amoled_1_75c v3.0.0,
// include/bsp/esp32_s3_touch_amoled_1_75c.h) cross-checked against the Waveshare Wiki pin table. The board has no separate
// backlight pin: the AMOLED brightness is set through panel command 0x51.
#pragma once

#include <driver/gpio.h>

// CO5300 AMOLED, 480x480 memory, 466x466 used, QSPI.
constexpr gpio_num_t WS_LCD_SCLK = GPIO_NUM_38;
constexpr gpio_num_t WS_LCD_DATA0 = GPIO_NUM_4;  // SDIO0 / MOSI
constexpr gpio_num_t WS_LCD_DATA1 = GPIO_NUM_5;
constexpr gpio_num_t WS_LCD_DATA2 = GPIO_NUM_6;
constexpr gpio_num_t WS_LCD_DATA3 = GPIO_NUM_7;
constexpr gpio_num_t WS_LCD_CS = GPIO_NUM_12;
constexpr gpio_num_t WS_LCD_RST = GPIO_NUM_1;

// Shared I2C bus: touch CST9217, IMU QMI8658, codec ES8311, PMU AXP2101.
constexpr gpio_num_t WS_I2C_SDA = GPIO_NUM_15;
constexpr gpio_num_t WS_I2C_SCL = GPIO_NUM_14;

// Touch.
constexpr gpio_num_t WS_TOUCH_INT = GPIO_NUM_11;
constexpr gpio_num_t WS_TOUCH_RST = GPIO_NUM_2;

// Audio: ES8311 codec (playback), ES7210 (microphones), speaker via class-D PA.
constexpr gpio_num_t WS_I2S_MCLK = GPIO_NUM_16;
constexpr gpio_num_t WS_I2S_BCLK = GPIO_NUM_9;
constexpr gpio_num_t WS_I2S_WS = GPIO_NUM_45;   // LRCK
constexpr gpio_num_t WS_I2S_DOUT = GPIO_NUM_8;  // ESP32-S3 -> ES8311 DAC
constexpr gpio_num_t WS_I2S_DSIN = GPIO_NUM_10; // ES7210 -> ESP32-S3 (unused here)
constexpr gpio_num_t WS_PA_EN = GPIO_NUM_46;    // speaker power amp enable

// Usable user button: BOOT (GPIO0). PWR is handled by the AXP2101.
constexpr gpio_num_t WS_BTN_BOOT = GPIO_NUM_0;
