# Licensing

This repository contains three kinds of material, licensed separately.

## 1. Source code: MIT

Everything that is not listed in the sections below (firmware in `main/`, the
tools in `tools/`, the desktop preview in `sim/`, build files) is licensed
under the MIT License. See [LICENSE](LICENSE).

## 2. Learning content: CC BY-NC-SA 4.0

The card decks in `content/` (words, meanings, collocations, example sentences,
translations and tips), the curriculum notes in `curriculum/`, and the copy of
the deck text that the importer writes into the content packs (`packs/`), are licensed
under Creative Commons
Attribution-NonCommercial-ShareAlike 4.0 International.
See [content/LICENSE](content/LICENSE).

You may share and adapt the content for non-commercial purposes, as long as you
credit "Deutsch mit Knödel" and release your adaptations under the same
license. For commercial use, please ask first.

## 3. The characters and "Deutsch mit Knödel": all rights reserved

The characters Knödel (the dumpling) and Brezel (the ginger cat), and the name
"Deutsch mit Knödel", are not covered by the licenses above. The code that
draws them (`main/knoedel.cpp`, `main/knoedel_bubbles.cpp`, `main/katze.cpp`,
`tools/preview/knoedel.js`, `tools/preview/katze.js`) is MIT like the rest of
the source, and you are welcome to build, run and modify this project with the
characters in it. You may not use the characters or the name for your own
brand, product or merchandise without permission.

## Audio clips

The MP3 clips in `content/*/audio/` are synthetic speech: they were generated
from the deck texts with Microsoft Azure Speech (standard tier, prebuilt neural
voices de-DE-KatjaNeural and de-DE-FlorianMultilingualNeural) and are not
recordings of real people. Microsoft lets the output be used and distributed
when the input text is one's own, as it is here. The clips are not covered by
the licenses above. Please report problems with them as an issue.

## Third-party components

| Component | Where | License |
|---|---|---|
| M5Unified, M5GFX | fetched into `managed_components/` at build time | MIT |
| M5Unified's button and touch state machines | `main/board/waveshare_175c/` (Waveshare board only) | MIT, see `LICENSE.M5Stack` there |
| esp_lcd_co5300, esp_codec_dev, esp_lcd_touch_cst9217 | fetched into `managed_components/` for the Waveshare board only | Apache 2.0 |
| minimp3 | `main/third_party/minimp3.h` | CC0 1.0 |
| Bricolage Grotesque | `tools/fonts/`, glyphs embedded in the content packs (`font_de_*.vlw`, `font_clock.vlw`, English `font_loc*.vlw`) | SIL OFL 1.1, see `tools/fonts/OFL-Bricolage.txt` |
| Noto Sans SC | `tools/fonts/`, glyphs embedded in the content packs (Chinese `font_loc*.vlw`) | SIL OFL 1.1, see `tools/fonts/OFL-NotoSansSC.txt` |
