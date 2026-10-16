# Waveshare ESP32-S3-Touch-AMOLED-1.75C

ESP32-S3, 32 MB flash, 8 MB octal PSRAM, 466 × 466 CO5300 AMOLED,
CST9217 touch, ES8311 speaker DAC, QMI8658 motion sensor and AXP2101 PMIC.
The common 468 px canvas is centred on the display; its outermost pixel on
each edge is cropped. Content packs are shared with the M5Stack board.

## Build

With ESP-IDF 6.0.1 activated, from the project root:

```sh
idf.py -B build-waveshare_175c -D SDKCONFIG=sdkconfig.waveshare_175c \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.waveshare_175c' build
idf.py -B build-waveshare_175c -p PORT flash monitor
```

Replace `PORT` with this board's serial port. Install a content pack using the
project's usual instructions. The existing partition layout is unchanged;
flash beyond its 16 MB layout is unused. The firmware image declares the actual
32 MB flash size. The three Waveshare driver dependencies are conditional on
`KNOEDEL_BOARD_WAVESHARE_175C`; a default M5Stack build does not need them.

## Controls and pins

BOOT: previous; hold BOOT: back. A short PWR press: next. Swipe left/right:
next/previous. Tap: select. PWR long-press behaviour is controlled by the PMIC.

| Connection | GPIOs |
| --- | --- |
| CO5300 QSPI clock, D0/D1/D2/D3, CS, reset | 38, 4/5/6/7, 12, 1 |
| Shared I2C SDA, SCL | 15, 14 |
| CST9217 interrupt, reset | 11, 2 |
| ES8311 MCLK, BCLK, WS, data out | 16, 9, 45, 8 |
| Speaker amplifier enable | 46 |
| BOOT | 0 |

Pin assignments and panel initialization follow the [Waveshare board
BSP](https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_1_75c/versions/3.0.0).
The framebuffer lives in PSRAM; transfers use an internal DMA stripe which is
reused only after the panel transfer completes. Speech is copied and resampled
to 24 kHz mono; tones mix alongside speech. Microphones are not used.

## Clock and power limits

This port uses the ESP32 system clock, with no imposed timezone. Before a time
has been set or restored, `get_time()` returns false. The app can then initialize
its calendar or the user can set the clock. The last local time is saved in NVS
when set and during menu shutdown. After a restart it resumes that saved time;
it cannot account for time spent powered off. Set the clock again after a long
power loss. Upgrading from the earlier timezone-specific port also requires
setting it again. This is not a battery-backed RTC.

Menu shutdown asks the AXP2101 to remove power and waits for it. On USB power it
returns so the app can wake the display again. If the PMIC fails to remove power
after two seconds, it logs the failure and returns. Raw GPIO input and the PMIC's
short-press interrupt feed the vendored M5Stack button/touch state machines;
their MIT licence is included in the board directory.
