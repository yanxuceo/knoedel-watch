# Knödel Watch

[![build](https://github.com/yanxuceo/knoedel-watch/actions/workflows/build.yml/badge.svg)](https://github.com/yanxuceo/knoedel-watch/actions/workflows/build.yml)

English | [中文](README.zh.md)

**Little animated characters act out every word. Learn a language on a tiny round screen.**

> 🇩🇪 This first version teaches **German** (level A2), with English and Chinese translations. Its course is called *Deutsch mit Knödel*. More languages can follow: the deck format is not tied to German.

<p align="center"><img src="docs/img/hero.gif" width="798" alt="Three watches: Knödel celebrates with confetti, Brezel vacuums, Brezel rides a bike"></p>

Animated vocabulary cards for the M5Stack StopWatch Dev Kit (ESP32-S3, round AMOLED touch screen, speaker, motion sensor). Two characters act out every word: **Knödel**, a dumpling, shows adjectives and expressions; **Brezel**, a ginger cat, does the verbs. Nouns show the thing itself, on a disc in the colour of its gender.

Each card has up to three faces: the word with its animation, the grammar line with an example sentence and its translation, and a tip. Every German line is spoken, in synthetic voices (Microsoft Azure neural text-to-speech, not recordings of real people); short dialogues are spoken in two voices. Around the cards the watch has a practice mode with spaced repetition, a clock with a word of the day, and a focus timer.

> **About the content.** The cards were written by an AI and have not been proofread by a native speaker yet. If you find a mistake, please open an issue: it will be fixed in the next content pack.

## Meet Knödel and Brezel

The words are not just shown, they are acted out. Every card has its own little scene, drawn live on the watch from data: a pose, motions, effects, props and a thought bubble.

**Knödel**, a Bavarian dumpling, does feelings, states and reactions: it beams, sulks, yawns, sneezes, blushes and thinks of what it misses in a bubble.

<p align="center"><img src="docs/img/knoedel.png" width="974" alt="Knödel on eight cards: glücklich, traurig, sauer, überrascht, müde, erkältet, lustig, Herzlichen Glückwunsch"></p>

**Brezel**, a ginger cat, does the verbs, and brings props: a vacuum, a watering can, a bike, an umbrella in the rain, a bus that drives off without her, a card terminal, a piggy bank, the door of a surgery.

<p align="center"><img src="docs/img/brezel.png" width="974" alt="Brezel on eight cards: staubsaugen, Blumen gießen, Fahrrad fahren, mitnehmen, verpassen, bezahlen, sparen, vorbeikommen"></p>

Around the cards: example dialogues in two voices, a practice mode that asks with the acting alone and cheers you on, a clock with a word of the day, and a focus timer.

<p align="center"><img src="docs/img/screens.png" width="1210" alt="A dialogue example, a practice question, a celebration, the clock, the focus timer, the clock in the main menu"></p>

## Hardware

One [M5Stack StopWatch Dev Kit](https://openelab.io/products/m5stack-stopwatch-dev-kit-esp32-s3?sca_ref=12520936.Zn0RxZ6jdFG) and a USB-C cable. Nothing else is needed.

That is an affiliate link: if you buy through it, the author of this project earns a commission, at no extra cost to you. The kit is also sold by M5Stack directly and by other shops.

### Other boards

The firmware reaches the hardware through a small board layer, [`main/board/board.h`](main/board/board.h), so it runs on other ESP32-S3 boards with a round touch screen of about 460 to 480 px, two keys and a speaker. Supported so far:

| Board | Status |
|---|---|
| M5Stack StopWatch Dev Kit | the main board, tested before every release |
| [Waveshare ESP32-S3-Touch-AMOLED-1.75C](docs/boards/waveshare-175c.md) | ported and maintained by [@pockegar](https://github.com/pockegar), tested on their board as time allows |

To port it to yours, see [docs/PORTING.md](docs/PORTING.md).

## Install

No development tools needed: open the **[web installer](https://yanxuceo.github.io/knoedel-watch/)** in Chrome or Edge on a computer, connect the watch with a USB-C data cable and press *Install*. On the first install, let it erase the watch; when updating later, keep the data and your progress stays.

Prefer the command line? Each [release](https://github.com/yanxuceo/knoedel-watch/releases) also has one image with everything: `pip install esptool`, then `python3 -m esptool --chip esp32s3 write-flash 0x0 knoedel-watch-<version>-full.bin` (a fresh install: it erases progress and settings).

To build it yourself, see [Build and flash](#build-and-flash).

## First start

The watch starts in English. To switch to Chinese: *Settings* → *Language*, tap. Then set the clock: *Settings* → *Time*, tap, and set hour, minute, day, month and year in turn (keys change the value, a tap moves on, the last tap saves). Learning works without the clock; only the clock face needs it.

## The main menu

| | |
|---|---|
| **Clock** | the time and a word of the day |
| **Learn German** | the decks, card by card |
| **Practice** | a few cards you have seen, asked again |
| **Focus timer** | 25 minutes of focus, 5 of break |
| **Settings** | language, time, volume, brightness, power off |

Menus are in the language chosen in the settings. The watch remembers where you were.

## Controls

| | |
|---|---|
| KEY B (blue, right) or swipe left | next |
| KEY A (yellow, left) or swipe right | previous |
| Tap the screen | open, next face of a card, change a setting |
| Hold KEY A | back |

The motion sensor adds three gestures: **pick the watch up** to wake a dimmed screen, **turn it face down** to darken it (on the focus timer this also starts the timer), and **shake it** on the clock for another word.

On battery the screen dims after a minute and the watch switches itself off after five idle minutes. On USB power it stays on and, left alone, shows the clock.

## Learn German

Main menu → *Learn German* → a deck → its cards. Tap to turn a card over:

1. the character acts the word out, with the word and its meaning;
2. the grammar line (past tense and perfect, plural, or a pattern with its case) and an example with its translation;
3. a tip.

## Practice

Main menu → *Practice* asks you a handful of the cards you have already seen, a minute or two at a time.

1. The character acts the word out, with no text. After two seconds the meaning appears as a hint.
2. Say the German word to yourself, then press any key or tap to see and hear the answer.
3. KEY B (blue, ✓): you knew it. KEY A (yellow, ✗): not yet; the card comes back before the session ends.

Three right answers in a row earn a short celebration. Words you know return after 1, 2, 4 … 64 days; a word you missed starts again. There is no backlog to work off and nothing to configure.

## Clock

The time, the date, and a word of the day: its character acts it out, then the word and its meaning take the picture's place, in turns. Tap to hear the word; a key or a shake picks another one. The seconds run round the edge.

## Focus timer

The ring around the screen runs down with the time left: red while you focus, green during the break. Brezel writes along while you work; Knödel hums during the break. Tap to start or pause, KEY B skips to the other phase, KEY A starts the phase again. Or simply turn the watch face down: the timer starts and the screen goes dark.

When a phase ends, the watch calls with a short tune and a German *Pause!* or *Weiter!*, even if you have left the timer screen. A tap starts the next phase.

## What is on the watch

The program and the learning content are separate:

| | Program (firmware) | Content pack |
|---|---|---|
| Holds | menus, characters, clock, timer, audio player | decks, fonts, audio |
| Changes | when the code changes | when decks are added or corrected |
| Written with | `idf.py flash` | `python3 tools/flash_content.py` |

So new content does not need a new firmware, and a watch can be moved from one pack to another (say, from A2 to B1) in under a minute.

The first pack, `a2-start`, has ten decks of 12 cards at level A2: moods and emotions, body and health, personality, household chores, on the go, quick reactions, shopping, appointments, at home (nouns), at the doctor's (nouns). Translations are in English and Chinese.

## Build and flash

You need [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v6.0, and for the content pack Python 3 with `pip install jsonschema pillow fonttools esptool`, plus `cmake` and SDL2 (`brew install sdl2`) for the layout check.

```bash
# 1. the program
idf.py -p <port> flash

# 2. the content pack
python3 tools/import_decks.py          # builds packs/a2-start.bin from content/
python3 tools/flash_content.py         # writes it to the watch
```

A watch without a content pack says "No content" and waits for step 2.

`import_decks.py` checks every deck, lays out every text of every language with the firmware's own drawing code, and refuses a pack with text that does not fit the round screen. To skip that check (no cmake or SDL2), pass `--skip-fit`. `--locale zh-Hans` makes Chinese the language a new watch starts in.

### Making a release

```bash
idf.py build && python3 tools/import_decks.py && python3 tools/make_release.py
```

`release/site/` is the web installer (ESP Web Tools: page, manifest, the four parts of the flash); publish it with GitHub Pages. `release/knoedel-watch-<version>-full.bin` goes to the GitHub release. The version is `PROJECT_VER` in `CMakeLists.txt`. To try the installer before publishing: `python3 -m http.server -d release/site 8000`, then open http://localhost:8000.

### Audio

The repository's audio was generated with Azure Speech. You only need a speech service if you change or add German text:

- `--paid` generates the missing clips with Azure (`SPEECH_KEY` and `SPEECH_REGION` in the environment). Without it the importer only reports what would be generated.
- `--audio edge` uses the free edge-tts instead (`pip install edge-tts`), good enough for trying your own cards. Such clips are kept out of git.

## Make your own decks

A deck is one folder, `content/<deck-id>/`, with a `deck.json`. The format is described in [FORMAT.md](FORMAT.md) and checked by `tools/check_deck.py`; `tools/build_preview.py` gives you a web preview of a deck with the same animations.

`content/library.json` names a content pack and lists its decks. For another pack, write another library file and pass it with `--library`.

Animations are data: a card names a pose, motions, effects, props and a thought bubble from `registry/<character>.json`. What those names draw is in `main/knoedel.cpp`, `main/knoedel_bubbles.cpp` and `main/katze.cpp`, with `tools/preview/*.js` as the drawing reference.

## Repository

```
main/          firmware (ESP-IDF, M5GFX)
main/board/    the hardware of each board, behind board.h
sim/           desktop build of the screens, used for the layout check
tools/         importer, deck checker, audio, web preview, flashing
content/       decks and their audio
registry/      what each character can do
curriculum/    plan of the decks to come, and every word taught so far
FORMAT.md      the deck format
docs/PORTING.md  how to port the firmware to another board
```

## Licenses

- Source code: MIT ([LICENSE](LICENSE)).
- Learning content (`content/`, `curriculum/`): CC BY-NC-SA 4.0 ([content/LICENSE](content/LICENSE)). Share and adapt it for non-commercial use, with credit and under the same license.
- The characters Knödel and Brezel and the name "Deutsch mit Knödel": all rights reserved. Build, run and modify the project with them; do not use them for your own brand, product or merchandise.

Details and third-party components: [LICENSES.md](LICENSES.md).
