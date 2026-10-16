# Porting Knödel Watch to another board

The app reaches the hardware only through one header,
[`main/board/board.h`](../main/board/board.h). A new board is one more
implementation of that header, chosen in Kconfig. The app, the characters and
the content pack stay as they are.

## What a board needs

- an **ESP32-S3** with **PSRAM** (the frame buffer and audio live there) and at
  least 16 MB of flash (3 MB for the program, the rest for the content pack)
- a **round touch screen** of about **460 to 480 px**, 16-bit colour, that
  M5GFX/LovyanGFX can drive. If no panel class fits, a panel of your own can
  feed the screen from a frame buffer.
- **at least two keys**, besides any power key
- a **speaker**

A motion sensor and a battery-backed real-time clock are optional. Without a
motion sensor there is no raise-to-wake, face-down or shake. Without a clock,
the clock face and practice need the time set after each power loss.

Boards without a touch screen are not supported: a tap is how the user selects,
and the keys alone cannot replace it without a worse experience everywhere.

## What the board provides

`board.h` has five parts:

| Part | Functions | Notes |
|---|---|---|
| Screen | `begin()`, `display()` | `display()` is an `lgfx::LGFX_Device&` whose `width()` and `height()` are the visible screen. Brightness, `sleep()` and `wakeup()` must work. |
| Input | `update()`, `touched()` | `update()` runs once per frame and returns one event: `PREVIOUS`, `NEXT`, `SELECT`, `BACK` or `NONE`. |
| Sound | `set_volume()`, `play_pcm()`, `stop_sound()`, `tone()` | 16-bit PCM at the clip's rate (22.05 or 24 kHz); tones play alongside speech. |
| Motion (optional) | `has_motion()`, `acceleration()` | in g, with z pointing out of the screen |
| Clock (optional) | `has_clock()`, `get_time()`, `set_time()` | local time, no time zone; `get_time()` is false until a valid time is known |
| Power | `on_usb()`, `power_off()` | `power_off()` returns only where the board cannot switch off, such as on USB power |

The comments in `board.h` are the specification. Read them before you start.

### Input

The app never sees keys or touch points, only the four events. Map them like
the M5Stack StopWatch, so that the README's controls hold for every board:

| Event | M5Stack StopWatch |
|---|---|
| `PREVIOUS` | left key, or a swipe to the right |
| `NEXT` | right key, or a swipe to the left |
| `SELECT` | a tap on the screen |
| `BACK` | holding the left key |

In practice mode `NEXT` means "knew it" and `PREVIOUS` "not yet", so keep
"next" on the right.

### Sound

`play_pcm()` replaces whatever speech was playing. The board may read the
caller's buffer until the sound ends or `stop_sound()` returns. After that the
buffer is reused at once, so `stop_sound()` must not return while a DMA
transfer still reads it.

### Power

On a board without a power key, `power_off()` can put the chip into deep sleep
and wake it with one of the keys (ESP32-S3 ext0/ext1 wake-up).

## Adding a board, step by step

1. Add the board to the "Target board" choice in `main/Kconfig.projbuild`,
   e.g. `KNOEDEL_BOARD_MY_BOARD`.
2. Put its code in `main/board/my_board.cpp`, or a folder `main/board/my_board/`
   for several files, and add those files to `main/CMakeLists.txt` under that
   option. See `main/board/m5_stopwatch.cpp`: the M5 board is a thin layer
   over M5Unified.
3. Add the board's ESP-IDF components to `main/idf_component.yml` so that only
   builds for that board fetch them. The builds for other boards, and their
   `dependencies.lock`, must not change. A rule on the board's Kconfig option
   does this:

   ```yaml
   espressif/esp_lcd_co5300:
     version: "^2.2.0"
     rules:
       - if: "$CONFIG{KNOEDEL_BOARD_MY_BOARD} == True"
   ```

   Such rules make the build configure twice, with a CMake warning "Missing
   kconfig option. Re-run the build process": that is expected.
4. Put the board's settings (the Kconfig choice, flash size) in
   `sdkconfig.defaults.my_board`, and build with
   `idf.py -B build-my_board -D SDKCONFIG=sdkconfig.my_board -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.my_board" build`.
   Such a build keeps its components in `dependencies.lock.my_board`: commit
   that file with the board.
5. Add the board to the matrix in `.github/workflows/build.yml`, with
   `defaults: sdkconfig.defaults;sdkconfig.defaults.my_board`. Every push and
   pull request then builds it.
6. Write a short `docs/boards/my-board.md`: pins, how to build and flash, what
   does not work yet.

Keep everything for the board inside its own files: no `#ifdef` for one board
in the app, no changes to the content.

### Screen size

Every screen is laid out for a 468 px circle (`ui::SCREEN` in
`main/card_ui.h`), and the app centres that picture on the board's display. On
a 480 px screen it has a black rim of 6 px; on a 466 px screen the ring loses
its outermost pixel. Text keeps inside a circle of 416 px, so it is never cut,
and one content pack fits every board: its layout check needs no board of its
own.

Screens much smaller or larger than 468 px would need a layout of their own,
and fonts of other sizes in the content pack. That is not supported yet.

## Not done yet

- **Releases.** The web installer and `tools/make_release.py` build for the
  M5Stack StopWatch only. They will offer one image per board, built from the
  same commit.

## Getting a board into the project

A board joins the main repository when someone with the hardware maintains it:
they test it before each release, and answer when `board.h` changes. Open an
issue or a draft pull request early. A short look at the plan saves rework.
