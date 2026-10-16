// Apps for a round watch: a main menu, German vocabulary cards, settings.
//
//   previous, next : the item before or after
//   select         : open / next face of a card / change a setting
//   back           : back, everywhere
//
//   Which key or gesture gives which, and all other hardware: main/board/.
//   On the M5Stack StopWatch: KEY A or a swipe right is previous, KEY B or a
//   swipe left next, a tap selects, holding KEY A goes back.
//
//   main menu > Learn German > deck > cards (face 1: Knödel acts out the word,
//   face 2: grammar line, example and translation, face 3: tip)
//   main menu > Practice: The character acts a word out, the meaning
//   follows as a hint, any key shows the answer; then next: knew it,
//   previous: not yet. Cards seen in a deck come back at growing intervals.
//   main menu > Settings: language, volume, brightness, power off
//   (menus are written in the learner's language, main/ui_strings.h)
//
// The watch remembers where it was and what was set (NVS).
// Decks, fonts and audio are not part of the program: they come from the content
// pack in its own flash partition (content.h), built by tools/import_decks.py
// and written to the watch by tools/flash_content.py.

#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <esp_random.h>
#include <nvs_flash.h>

#include <string>

#include "audio_player.h"
#include "board.h"
#include "card_ui.h"
#include "content.h"
#include "deck.h"
#include "motion.h"
#include "progress.h"
#include "character.h"
#include "ui_strings.h"

static const char *TAG = "cards";

static uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }
static void sleep_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

// The two languages of the content pack: deck pack and the fonts for its script.
struct Language {
    ContentFile pack, font_m, font_s, font_l;
};
static Language LANGUAGES[2];

static constexpr uint8_t VOLUMES[] = {0, 100, 160, 220, 255};  // speaker volume, 0..255
static constexpr uint8_t BRIGHTNESSES[] = {60, 120, 200, 255};
static constexpr uint8_t BRIGHTNESS_DEFAULT = 1;  // shown as 2 / 4
static constexpr uint8_t BRIGHTNESS_IDLE = 40;
static constexpr uint32_t IDLE_DIM_MS = 60 * 1000;
static constexpr uint32_t IDLE_OFF_MS = 5 * 60 * 1000;  // on battery; on USB power the watch stays on
static constexpr uint32_t SAVE_AFTER_MS = 2000;
static constexpr uint32_t FRAME_MS = 33;
static constexpr int FLIP_STEPS = 5;

enum Screen : uint8_t { HOME, DECKS, CARDS, SETTINGS, PRACTICE, CLOCK, TIMER };
enum HomeItem { ITEM_CLOCK, ITEM_LEARN, ITEM_PRACTICE, ITEM_TIMER, ITEM_SETTINGS, HOME_ITEMS };
enum Setting { SET_LANGUAGE, SET_TIME, SET_VOLUME, SET_BRIGHTNESS, SET_POWER_OFF, SETTING_COUNT };

// Everything the watch remembers across power cycles.
struct State {
    uint8_t screen = HOME, language = 0, volume = 3, brightness = BRIGHTNESS_DEFAULT;
    uint8_t home_item = ITEM_LEARN, deck = 0, card = 0, setting = 0;  // a new watch opens on "Learn German"
};
static State st;
static bool st_dirty = false;

// The screens are drawn for a ui::SCREEN pixel wide circle, into fb. A board's
// screen may be a few pixels smaller or larger: fb is centred on it, at (ox, oy).
static M5Canvas fb(&board::display());
static int ox, oy;
static int face = 0;
static uint32_t face_shown_ms = 0;
static bool two_languages = false;

// One practice session. It is not saved: a restart ends it, the answers given stay.
// CHEER: a little celebration after every third right answer in a row.
enum PracticePhase { ASK, ANSWER, CHEER, DONE, NOTHING_SEEN };
static constexpr int CHEER_EVERY = 3;
static constexpr uint32_t CHEER_MS = 2600;
static constexpr int PRACTICE_QUEUE = 24;       // the session's cards plus those asked again
static constexpr uint32_t HINT_AFTER_MS = 2000; // the meaning appears this long into a question
static struct {
    progress::Pick queue[PRACTICE_QUEUE];
    int count, pos;
    PracticePhase phase;
    bool hint;
    int streak, cheer_variant;
    const char *cheer_word;
    uint32_t cheer_ms;
} pr;

// The focus timer runs on in the background; when a phase ends the watch
// shows the timer and calls.
static constexpr int FOCUS_S = 25 * 60, BREAK_S = 5 * 60;
static struct {
    bool focus = true, running = false, alert = false;
    int remaining = FOCUS_S;  // while paused
    uint32_t end_ms, alert_ms;
} tm;

// The clock face: the word of the day, and the setting of the time.
static constexpr uint32_t ACT_MS = 7400, WORD_MS = 4500;  // the word's turns on the clock
static constexpr uint32_t GROW_MS = 380, SHRINK_MS = 300, FADE_MS = 280;  // and the changes between them
static struct {
    int deck = 0, card = -1;
    int day = -1;  // the date the word was chosen for
    bool word_shown;
    uint32_t turn_ms;
} cw;
static struct {
    bool on;
    int field;  // hour, minute, day, month, year
    board::DateTime dt;
} edit;

// ---- sounds -------------------------------------------------------------------

struct Note {
    uint16_t hz, ms;  // hz 0: a rest; ms 0: the end
};
static const Note CHEER_TUNE[] = {{1047, 90}, {1319, 90}, {1568, 90}, {2093, 220}, {0, 0}};
static const Note FOCUS_DONE_TUNE[] = {{1568, 150}, {1319, 150}, {1047, 320}, {0, 260}, {1568, 150}, {1319, 150}, {1047, 320}, {0, 0}};
static const Note BREAK_DONE_TUNE[] = {{1047, 140}, {1319, 140}, {1568, 140}, {2093, 360}, {0, 0}};
static const Note *s_tune;
static int s_note;
static uint32_t s_note_ms;

static void play_tune(const Note *tune)
{
    s_tune = tune;
    s_note = 0;
    s_note_ms = 0;
}

static void tune_tick()
{
    if (s_tune == nullptr || millis() < s_note_ms) {
        return;
    }
    const Note n = s_tune[s_note++];
    if (n.ms == 0) {
        s_tune = nullptr;
        return;
    }
    if (n.hz) {
        board::tone(n.hz, n.ms);
    }
    s_note_ms = millis() + n.ms;
}

// ---- settings -----------------------------------------------------------------

static void load_state()
{
    nvs_handle_t h;
    if (nvs_open("watch", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t size = sizeof(st);
    State saved;
    if (nvs_get_blob(h, "state", &saved, &size) == ESP_OK && size == sizeof(st)) {
        st = saved;
    }
    nvs_close(h);
}

static void save_state()
{
    nvs_handle_t h;
    if (nvs_open("watch", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "state", &st, sizeof(st));
        nvs_commit(h);
        nvs_close(h);
    }
    progress::save();
    st_dirty = false;
}

static void apply_language()
{
    const Language &lang = LANGUAGES[st.language];
    deck_init(lang.pack.data, lang.pack.size);  // checked by load_content()
    ui::set_locale(lang.font_m.data, lang.font_s.data, lang.font_l.data);
}

// Saved positions may point past the end after a firmware with other decks was flashed.
static void clamp_state()
{
    st.language = two_languages ? st.language & 1 : 0;
    st.volume = st.volume < sizeof(VOLUMES) ? st.volume : 3;
    st.brightness = st.brightness < sizeof(BRIGHTNESSES) ? st.brightness : BRIGHTNESS_DEFAULT;
    // A practice session and the timer are not resumed; the clock is.
    st.screen = st.screen <= SETTINGS || st.screen == CLOCK ? st.screen : (uint8_t)HOME;
    st.home_item %= HOME_ITEMS;
    st.setting %= SETTING_COUNT;
}

static void clamp_position()
{
    st.deck = st.deck < deck_count() ? st.deck : 0;
    st.card = st.card < deck_info(st.deck).cards ? st.card : 0;
}

// ---- drawing ------------------------------------------------------------------

static bool animated()
{
    return st.screen == HOME || st.screen == DECKS || st.screen == PRACTICE || (st.screen == CLOCK && !cw.word_shown) ||
           st.screen == TIMER || (st.screen == CARDS && face == 0);
}

// Only the square of the animated picture changes from frame to frame.
static void push_stage()
{
    int x, y, size;
    ui::stage_rect(x, y, size);
    board::display().setClipRect(ox + x, oy + y, size, size);
    fb.pushSprite(ox, oy);
    board::display().clearClipRect();
}

// ---- clock --------------------------------------------------------------------


static int days_in_month(int year, int month)
{
    static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 29 : DAYS[month - 1];
}

// Days since 2000-01-01 (civil calendar); 2000-01-01 was a Saturday.
static int day_number(int year, int month, int day)
{
    const int y = year - (month <= 2), era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 730425;
}

static board::DateTime now_dt()
{
    board::DateTime dt = {};
    if (!board::get_time(dt)) {
        dt = {2000, 1, 1, 0, 0, 0};
    }
    return dt;
}

// A word for the day: the same all day, another tomorrow; a shake picks another.
static void pick_word(bool another)
{
    int total = 0;
    for (int d = 0; d < deck_count(); d++) {
        total += deck_info(d).cards;
    }
    if (total == 0) {
        cw.card = -1;
        return;
    }
    const board::DateTime dt = now_dt();
    const int today = day_number(dt.year, dt.month, dt.day);
    uint32_t n = another ? esp_random() : (uint32_t)today * 2654435761u;
    n %= (uint32_t)total;
    for (int d = 0; d < deck_count(); d++) {
        if ((int)n < deck_info(d).cards) {
            cw.deck = d;
            cw.card = (int)n;
            break;
        }
        n -= deck_info(d).cards;
    }
    cw.day = today;
    cw.word_shown = false;  // a new word starts with its acting
    cw.turn_ms = millis();
    face_shown_ms = cw.turn_ms;
}

static ui::ClockFace clock_face(std::string &date)
{
    const board::DateTime dt = now_dt();
    const int wd = ((day_number(dt.year, dt.month, dt.day) % 7) + 13) % 7;  // 0: Sunday
    const UiText &tx = ui_text(deck_locale());
    char buf[64];
    const char *month = tx.months[(dt.month + 11) % 12];
    if (strncmp(deck_locale(), "zh", 2) == 0) {
        snprintf(buf, sizeof(buf), "%s%d日 %s", month, dt.day, tx.weekdays[wd]);  // 10月15日 星期四
    } else {
        snprintf(buf, sizeof(buf), "%s, %d %s", tx.weekdays[wd], dt.day, month);  // Thursday, 15 October
    }
    date = buf;
    // Where in its turn the word is: the picture pops up and shrinks away,
    // the word fades in and out.
    const float e = (float)(millis() - cw.turn_ms);
    float scale = 1, alpha = 1;
    if (cw.word_shown) {
        alpha = fminf(e / FADE_MS, ((float)WORD_MS - e) / FADE_MS);
    } else if (e < GROW_MS) {
        const float u = e / GROW_MS - 1;
        scale = 1 + 2.0f * u * u * u + 1.0f * u * u;  // back-out: about 4 % past full size, then back
    } else if (e > ACT_MS - SHRINK_MS) {
        const float u = fminf(1, (e - (ACT_MS - SHRINK_MS)) / SHRINK_MS);
        scale = 1 - u * u;
    }
    return {dt.hour, dt.minute, dt.second, date.c_str(), cw.deck, cw.card, cw.word_shown,
            alpha < 0 ? 0 : alpha > 1 ? 1 : alpha, scale < 0 ? 0 : scale};
}

// ---- focus timer --------------------------------------------------------------

static int timer_left()
{
    if (!tm.running) {
        return tm.remaining;
    }
    const int32_t ms = (int32_t)(tm.end_ms - millis());
    return ms > 0 ? (ms + 999) / 1000 : 0;
}

static ui::TimerFace timer_face()
{
    const UiText &tx = ui_text(deck_locale());
    const bool fresh = !tm.running && tm.remaining == (tm.focus ? FOCUS_S : BREAK_S);
    return {timer_left(), tm.focus ? FOCUS_S : BREAK_S, tm.focus, tm.running, tm.focus ? tx.focus : tx.pause,
            tm.running ? tx.running_hint : fresh ? tx.start_hint : tx.paused_hint};
}

static void render_stage()
{
    uint32_t now = millis();
    float t = fmodf((now - face_shown_ms) / 1000.0f, character::LOOP_SECONDS), clock = now / 1000.0f;
    if (st.screen == HOME) {
        static const ui::MenuIcon ICONS[HOME_ITEMS] = {ui::ICON_CLOCK, ui::ICON_KNOEDEL, ui::ICON_BREZEL, ui::ICON_TIMER,
                                                       ui::ICON_GEAR};
        ui::draw_menu_stage(&fb, ICONS[st.home_item], t, clock);
    } else if (st.screen == CLOCK) {
        std::string date;
        ui::draw_clock_stage(&fb, clock_face(date), t, clock);
    } else if (st.screen == TIMER) {
        if (tm.alert) {
            ui::draw_celebration_stage(&fb, tm.focus ? 4 : 0, t, clock);
        } else {
            ui::draw_timer_stage(&fb, timer_face(), t, clock);
        }
    } else if (st.screen == PRACTICE) {
        if (pr.phase == ASK || pr.phase == ANSWER) {
            ui::draw_stage(&fb, pr.queue[pr.pos].deck, pr.queue[pr.pos].card, t, clock);
        } else if (pr.phase == CHEER) {
            ui::draw_celebration_stage(&fb, pr.cheer_variant, t, clock);
        } else {
            ui::draw_menu_stage(&fb, pr.phase == DONE ? ui::ICON_BREZEL : ui::ICON_KNOEDEL, t, clock);
        }
    } else {
        ui::draw_stage(&fb, st.deck, st.screen == DECKS ? ui::COVER : st.card, t, clock);
    }
}

static void render_setting()
{
    const UiText &tx = ui_text(deck_locale());
    std::string value;
    const char *name = "", *hint = tx.tap_change;
    switch (st.setting) {
    case SET_LANGUAGE:
        name = tx.language;
        value = tx.language_name;
        hint = two_languages ? tx.tap_change : "";
        break;
    case SET_TIME: {
        name = tx.time;
        const board::DateTime dt = edit.on ? edit.dt : now_dt();
        char buf[40];
        const int f = edit.on ? edit.field : -1;
        auto mark = [f](int field) { return f == field ? "{{" : ""; };
        auto end = [f](int field) { return f == field ? "}}" : ""; };
        if (f >= 2) {  // the date: 14.10.2026
            snprintf(buf, sizeof(buf), "%s%d%s.%s%d%s.%s%d%s", mark(2), dt.day, end(2), mark(3), dt.month, end(3),
                     mark(4), dt.year, end(4));
        } else {
            snprintf(buf, sizeof(buf), "%s%d%s:%s%02d%s", mark(0), dt.hour, end(0), mark(1), dt.minute, end(1));
        }
        value = buf;
        hint = edit.on ? tx.edit_hint : tx.tap_change;
        if (edit.on) {
            ui::draw_setting_edit(&fb, st.setting, SETTING_COUNT, name, value.c_str(), hint);
            return;
        }
        break;
    }
    case SET_VOLUME:
        name = tx.volume;
        value = st.volume == 0 ? tx.off : std::to_string(st.volume) + " / " + std::to_string(sizeof(VOLUMES) - 1);
        break;
    case SET_BRIGHTNESS:
        name = tx.brightness;
        value = std::to_string(st.brightness + 1) + " / " + std::to_string(sizeof(BRIGHTNESSES));
        break;
    default:
        name = tx.power_off;
        hint = tx.tap_off;
        break;
    }
    ui::draw_setting(&fb, st.setting, SETTING_COUNT, name, st.setting == SET_POWER_OFF ? nullptr : value.c_str(), hint);
}

static void render()
{
    const UiText &tx = ui_text(deck_locale());
    switch (st.screen) {
    case HOME: {
        const char *const TITLES[HOME_ITEMS] = {tx.clock, tx.learn, tx.practice, tx.timer, tx.settings};
        ui::draw_menu(&fb, st.home_item, HOME_ITEMS, TITLES[st.home_item]);
        break;
    }
    case CLOCK: {
        std::string date;
        ui::draw_clock(&fb, clock_face(date));
        break;
    }
    case TIMER:
        if (tm.alert) {
            // The phase that just ended is still set: focus over means a break.
            ui::draw_message(&fb, tm.focus ? "Pause!" : "Weiter!", tm.focus ? tx.break_now : tx.back_to_work,
                             tm.focus ? 0x3E8E2Fu : 0xE5484Du);
        } else {
            ui::draw_timer(&fb, timer_face());
        }
        break;
    case DECKS:
        ui::draw_face(&fb, st.deck, ui::COVER, 0);
        break;
    case CARDS:
        ui::draw_face(&fb, st.deck, st.card, face);
        break;
    case SETTINGS:
        render_setting();
        break;
    case PRACTICE:
        if (pr.phase == ASK) {
            ui::draw_question(&fb, pr.queue[pr.pos].deck, pr.queue[pr.pos].card, pr.pos, pr.count, pr.hint);
        } else if (pr.phase == ANSWER) {
            ui::draw_answer(&fb, pr.queue[pr.pos].deck, pr.queue[pr.pos].card, pr.pos, pr.count);
        } else if (pr.phase == CHEER) {
            char line[32];
            snprintf(line, sizeof(line), tx.in_a_row, pr.streak);
            ui::draw_message(&fb, pr.cheer_word, line, 0xE0A526);
        } else if (pr.phase == DONE) {
            ui::draw_menu(&fb, 0, 1, tx.well_done);
        } else {
            ui::draw_menu(&fb, 0, 1, tx.learn_first);
        }
        break;
    }
    if (animated()) {
        render_stage();
    }
}

// Reads aloud what a card face shows: every German line that has a clip.
static void speak()
{
    if (st.screen != CARDS) {
        audio_stop();
        return;
    }
    const Card c = deck_card(st.deck, st.card);
    static const int8_t FACE_CLIPS[3][2] = {{0, -1}, {1, 2}, {3, -1}};
    Clip clips[2];
    int n = 0;
    for (int i : FACE_CLIPS[face]) {
        if (i >= 0 && c.clips[i].size > 0) {
            clips[n++] = c.clips[i];
        }
    }
    audio_play(clips, n);
}

// Shows the current screen from scratch.
static void show()
{
    face = 0;
    face_shown_ms = millis();
    if (st.screen == CARDS) {
        progress::mark_seen(st.deck, st.card);  // from now on practice may ask for it
    }
    render();
    fb.pushSprite(ox, oy);
    speak();
    st_dirty = true;
}

// Draws the frame buffer squeezed horizontally, as a card seen edge-on.
static void push_squashed(float zoom_x)
{
    int half = (int)(ui::SCREEN * zoom_x / 2);
    board::display().startWrite();
    board::display().fillRect(ox, oy, ui::CX - half, ui::SCREEN, TFT_BLACK);
    board::display().fillRect(ox + ui::CX + half, oy, ui::SCREEN - (ui::CX + half), ui::SCREEN, TFT_BLACK);
    fb.pushRotateZoom(&board::display(), ox + ui::CX, oy + ui::CY, 0, zoom_x, 1.0f);
    board::display().endWrite();
}

static void next_face()
{
    audio_stop();
    for (int i = 1; i <= FLIP_STEPS; i++) {
        push_squashed(cosf(i * 1.5708f / (FLIP_STEPS + 0.6f)));
    }
    face = (face + 1) % ui::face_count(st.deck, st.card);
    face_shown_ms = millis();
    render();
    speak();
    for (int i = FLIP_STEPS; i >= 1; i--) {
        push_squashed(cosf(i * 1.5708f / (FLIP_STEPS + 0.6f)));
    }
    fb.pushSprite(ox, oy);
}

// ---- practice -----------------------------------------------------------------

// A day number from the real-time clock, or -1. Only the passing of days
// matters, so the clock is never set: see progress.h.
static int clock_day()
{
    board::DateTime d;
    if (!board::has_clock()) {
        return -1;
    }
    if (!board::get_time(d)) {
        // A clock that never ran, or lost power, holds no valid date. Any date
        // will do to start it: progress.cpp carries on from the last day it saw.
        // Starting it overwrites the time the user set, so the clock is first
        // read once more, a moment later: one that only failed to answer keeps it.
        sleep_ms(300);
        if (board::get_time(d)) {
            ESP_LOGW(TAG, "the clock answered only on a second look");
        } else if (board::set_time({2000, 1, 1, 0, 0, 0}) && board::get_time(d)) {
            ESP_LOGW(TAG, "the clock held no date and was started");
        } else {
            return -1;
        }
    }
    const int day = day_number(d.year, d.month, d.day);
    return day >= 0 && day < 36500 ? day : -1;
}

// Redraws the screen without restarting its animation.
static void redraw()
{
    render();
    fb.pushSprite(ox, oy);
}

static void practice_start()
{
    progress::set_clock_day(clock_day());
    pr.count = progress::pick_session(pr.queue, progress::SESSION_CARDS);
    pr.pos = 0;
    pr.hint = false;
    pr.streak = 0;
    pr.phase = pr.count > 0 ? ASK : NOTHING_SEEN;
    st.screen = PRACTICE;
    show();
}

static void practice_reveal()
{
    pr.phase = ANSWER;
    redraw();
    const Card c = deck_card(pr.queue[pr.pos].deck, pr.queue[pr.pos].card);
    if (c.clips[0].size > 0) {
        audio_play(&c.clips[0], 1);
    }
}

// After a celebration, or an answer: the next question, or the end.
static void practice_next()
{
    pr.hint = false;
    if (pr.pos >= pr.count) {
        pr.phase = DONE;
        progress::session_done();
    } else {
        pr.phase = ASK;
    }
    show();
}

static void practice_answer(bool knew)
{
    const progress::Pick card = pr.queue[pr.pos];
    progress::answer(card.deck, card.card, knew);
    if (!knew && pr.count < PRACTICE_QUEUE) {
        pr.queue[pr.count++] = card;  // once more before the session ends
    }
    pr.pos++;
    pr.streak = knew ? pr.streak + 1 : 0;
    if (knew && pr.streak % CHEER_EVERY == 0) {
        // Praise in German, a celebration chosen at random, a little tune.
        static const char *const WORDS[] = {"Super!", "Toll!", "Klasse!", "Spitze!", "Wahnsinn!", "Prima!"};
        static int last_variant = -1;
        int v = (int)(esp_random() % ui::CELEBRATIONS);
        v = v == last_variant ? (v + 1) % ui::CELEBRATIONS : v;
        last_variant = v;
        pr.cheer_variant = v;
        pr.cheer_word = WORDS[esp_random() % (sizeof(WORDS) / sizeof(WORDS[0]))];
        pr.cheer_ms = millis();
        pr.phase = CHEER;
        show();
        play_tune(CHEER_TUNE);
        return;
    }
    practice_next();
}

static void practice_leave()
{
    audio_stop();
    st.screen = HOME;
    show();
}

// Every key and gesture during practice. `dir`: +1 right key or swipe left,
// -1 left key or swipe right, 0 a tap.
static void practice_input(int dir)
{
    if (pr.phase == ASK) {
        practice_reveal();
    } else if (pr.phase == CHEER) {
        practice_next();
    } else if (pr.phase == ANSWER) {
        if (dir == 0) {
            const Card c = deck_card(pr.queue[pr.pos].deck, pr.queue[pr.pos].card);
            audio_play(&c.clips[0], 1);  // hear it again
        } else {
            practice_answer(dir > 0);
        }
    } else {
        practice_leave();
    }
}

static uint8_t wrap(int value, int count) { return (uint8_t)((value % count + count) % count); }

// ---- focus timer and clock: actions ----------------------------------------------

static void timer_start()
{
    tm.end_ms = millis() + tm.remaining * 1000;
    tm.running = true;
}

static void timer_pause()
{
    tm.remaining = timer_left();
    tm.running = false;
}

// A phase is over (or skipped): the other one is set up; after a focus phase
// the break starts at once.
static void timer_next_phase(bool start)
{
    tm.focus = !tm.focus;
    tm.remaining = tm.focus ? FOCUS_S : BREAK_S;
    tm.running = false;
    tm.alert = false;
    if (start) {
        timer_start();
    }
}

static void timer_input(int dir)
{
    if (tm.alert) {
        timer_next_phase(true);
    } else if (dir > 0) {
        timer_next_phase(false);  // skip to the other phase
    } else if (dir < 0) {
        tm.running = false;  // back to the full time of this phase
        tm.remaining = tm.focus ? FOCUS_S : BREAK_S;
    } else if (tm.running) {
        timer_pause();
    } else {
        timer_start();
    }
    redraw();
}

// Called every frame: the timer runs on whatever the screen shows.
static void timer_tick()
{
    if (!tm.running || timer_left() > 0) {
        return;
    }
    tm.running = false;
    tm.remaining = 0;
    tm.alert = true;
    tm.alert_ms = millis();
    play_tune(tm.focus ? FOCUS_DONE_TUNE : BREAK_DONE_TUNE);
    st.screen = TIMER;
    board::display().setBrightness(BRIGHTNESSES[st.brightness]);
    show();
}

static void speak_word()
{
    if (cw.card >= 0) {
        const Card c = deck_card(cw.deck, cw.card);
        if (c.clips[0].size > 0) {
            audio_play(&c.clips[0], 1);
        }
    }
}

// Setting the time: KEY A and B change the highlighted field, a tap moves on;
// after the year the clock is set.
static void edit_time(int dir)
{
    board::DateTime &dt = edit.dt;
    switch (edit.field) {
    case 0:
        dt.hour = wrap(dt.hour + dir, 24);
        break;
    case 1:
        dt.minute = wrap(dt.minute + dir, 60);
        break;
    case 2:
        dt.day = 1 + wrap(dt.day - 1 + dir, days_in_month(dt.year, dt.month));
        break;
    case 3:
        dt.month = 1 + wrap(dt.month - 1 + dir, 12);
        break;
    default:
        dt.year = dt.year + dir < 2024 ? 2099 : dt.year + dir > 2099 ? 2024 : dt.year + dir;
        break;
    }
    dt.day = dt.day > days_in_month(dt.year, dt.month) ? days_in_month(dt.year, dt.month) : dt.day;
    redraw();
}

static void edit_time_next()
{
    if (!edit.on) {
        edit.on = true;
        edit.field = 0;
        edit.dt = now_dt();
        if (edit.dt.year < 2024) {
            edit.dt.year = 2026;
            edit.dt.month = edit.dt.day = 1;
        }
        edit.dt.second = 0;
    } else if (++edit.field > 4) {
        // Set the clock; practice keeps counting days from where it was.
        const board::DateTime old = now_dt();
        board::set_time(edit.dt);
        progress::clock_moved(day_number(old.year, old.month, old.day),
                              day_number(edit.dt.year, edit.dt.month, edit.dt.day));
        progress::save();
        edit.on = false;
        cw.day = -1;
    }
    redraw();
}

// ---- input --------------------------------------------------------------------

static void step(int dir)
{
    if (st.screen == PRACTICE) {
        practice_input(dir);
        return;
    }
    if (st.screen == TIMER) {
        timer_input(dir);
        return;
    }
    if (st.screen == CLOCK) {
        pick_word(true);
        redraw();
        speak_word();
        return;
    }
    if (st.screen == SETTINGS && edit.on) {
        edit_time(dir);
        return;
    }
    switch (st.screen) {
    case HOME:
        st.home_item = wrap(st.home_item + dir, HOME_ITEMS);
        break;
    case DECKS:
        st.deck = wrap(st.deck + dir, deck_count());
        st.card = 0;
        break;
    case CARDS:
        st.card = wrap(st.card + dir, deck_info(st.deck).cards);
        break;
    case SETTINGS:
        st.setting = wrap(st.setting + dir, SETTING_COUNT);
        break;
    default:
        break;
    }
    show();
}

static void power_off()
{
    save_state();
    audio_stop();
    board::display().fillScreen(TFT_BLACK);
    board::display().setBrightness(0);
    board::display().sleep();
    sleep_ms(100);
    board::power_off();  // does not return on battery; on USB power the board stays on
    sleep_ms(500);
    board::display().wakeup();
    board::display().setBrightness(BRIGHTNESSES[st.brightness]);
    show();
}

static void change_setting()
{
    switch (st.setting) {
    case SET_LANGUAGE:
        if (two_languages) {
            st.language ^= 1;
            apply_language();
        }
        break;
    case SET_VOLUME:
        st.volume = wrap(st.volume + 1, sizeof(VOLUMES));
        board::set_volume(VOLUMES[st.volume]);
        break;
    case SET_BRIGHTNESS:
        st.brightness = wrap(st.brightness + 1, sizeof(BRIGHTNESSES));
        board::display().setBrightness(BRIGHTNESSES[st.brightness]);
        break;
    case SET_TIME:
        edit_time_next();
        return;
    default:
        power_off();
        return;
    }
    show();
    if (st.setting == SET_VOLUME && st.volume > 0) {
        const Card sample = deck_card(0, 0);
        audio_play(&sample.clips[0], 1);  // so the new volume can be heard
    }
}

static void tap()
{
    switch (st.screen) {
    case HOME:
        if (st.home_item == ITEM_PRACTICE) {
            practice_start();
            break;
        }
        st.screen = st.home_item == ITEM_CLOCK ? CLOCK : st.home_item == ITEM_LEARN ? DECKS : st.home_item == ITEM_TIMER ? TIMER : SETTINGS;
        if (st.screen == CLOCK) {
            pick_word(false);
        }
        show();
        break;
    case CLOCK:
        // Show the word now (the picture shrinks away first), and say it.
        if (!cw.word_shown && millis() - cw.turn_ms < ACT_MS - SHRINK_MS) {
            cw.turn_ms = millis() - (ACT_MS - SHRINK_MS);
        }
        speak_word();
        break;
    case TIMER:
        timer_input(0);
        break;
    case DECKS:
        st.screen = CARDS;
        show();
        break;
    case CARDS:
        next_face();
        break;
    case SETTINGS:
        change_setting();
        break;
    case PRACTICE:
        practice_input(0);
        break;
    }
}

static void back()
{
    if (st.screen == HOME) {
        return;
    }
    audio_stop();
    if (edit.on) {
        edit.on = false;  // leave the time as it was
        redraw();
        return;
    }
    if (st.screen == TIMER && tm.alert) {
        timer_next_phase(false);
    }
    st.screen = st.screen == CARDS ? DECKS : HOME;
    show();
}

// Takes decks, fonts and audio from the content pack. False if the pack lacks
// a file or was built for another firmware.
static bool load_content()
{
    static const char *const FONTS[] = {"font_de_xl.vlw", "font_de_l.vlw", "font_de_m.vlw", "font_de_mb.vlw",
                                        "font_de_s.vlw", "font_de_xs.vlw", "font_clock.vlw"};
    static const char *const SLOTS[2][4] = {{"decks_loc0.bin", "font_loc0_m.vlw", "font_loc0_s.vlw", "font_loc0_l.vlw"},
                                            {"decks_loc1.bin", "font_loc1_m.vlw", "font_loc1_s.vlw", "font_loc1_l.vlw"}};
    ContentFile de[7];
    for (int i = 0; i < 7; i++) {
        de[i] = content_file(FONTS[i]);
        if (de[i].data == nullptr) {
            return false;
        }
    }
    for (int i = 0; i < 2; i++) {
        LANGUAGES[i] = {content_file(SLOTS[i][0]), content_file(SLOTS[i][1]), content_file(SLOTS[i][2]), content_file(SLOTS[i][3])};
        if (LANGUAGES[i].pack.data == nullptr || LANGUAGES[i].font_m.data == nullptr || LANGUAGES[i].font_s.data == nullptr ||
            LANGUAGES[i].font_l.data == nullptr) {
            return false;
        }
    }
    const ContentFile audio = content_file("audio.pak");
    // Slot 1 repeats slot 0 when the pack was built with one language only.
    if (audio.data == nullptr || !deck_init(LANGUAGES[0].pack.data, LANGUAGES[0].pack.size)) {
        return false;
    }
    const std::string first = deck_locale();
    if (!deck_init(LANGUAGES[1].pack.data, LANGUAGES[1].pack.size)) {
        return false;
    }
    two_languages = first != deck_locale();
    ui::init({de[0].data, de[1].data, de[2].data, de[3].data, de[4].data, de[5].data, de[6].data});
    audio_init(audio.data);
    return true;
}

// Without content there is nothing to show but how to get some.
static void no_content_screen(bool wrong_version)
{
    auto &d = board::display();
    d.setBrightness(BRIGHTNESSES[BRIGHTNESS_DEFAULT]);
    d.fillScreen(TFT_BLACK);
    d.setTextDatum(middle_center);
    d.setTextColor(0xE8C990u);
    d.setFont(&fonts::DejaVu40);
    d.drawString("No content", d.width() / 2, d.height() / 2 - 70);
    d.setTextColor(0xC8CCD4u);
    d.setFont(&fonts::DejaVu24);
    d.drawString(wrong_version ? "Content pack does not" : "No content pack", d.width() / 2, d.height() / 2 - 10);
    d.drawString(wrong_version ? "match this firmware" : "on this watch yet", d.width() / 2, d.height() / 2 + 24);
    d.setTextColor(0x8A93A3u);
    d.setFont(&fonts::DejaVu18);
    d.drawString("tools/flash_content.py", d.width() / 2, d.height() / 2 + 84);
    d.drawString("(see README)", d.width() / 2, d.height() / 2 + 112);
    while (true) {
        if (board::update() == board::Event::BACK) {
            board::power_off();
        }
        sleep_ms(50);
    }
}

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    board::begin();
    load_state();

    const bool mounted = content_mount();
    if (!mounted || !load_content()) {
        no_content_screen(mounted);
    }
    clamp_state();

    board::display().setBrightness(BRIGHTNESSES[st.brightness]);
    board::display().fillScreen(TFT_BLACK);
    board::set_volume(VOLUMES[st.volume]);

    fb.setPsram(true);
    fb.setColorDepth(16);
    if (fb.createSprite(ui::SCREEN, ui::SCREEN) == nullptr) {
        ESP_LOGE(TAG, "no memory for the frame buffer");
        return;
    }
    fb.setPivot(ui::CX, ui::CY);
    ox = ((int)board::display().width() - ui::SCREEN) / 2;
    oy = ((int)board::display().height() - ui::SCREEN) / 2;

    apply_language();
    clamp_position();
    if (st.screen == CLOCK) {
        pick_word(false);
    }
    progress::load(clock_day());
    board::DateTime now = {};
    const bool has_clock = board::get_time(now);
    ESP_LOGI(TAG, "practice: day %d, %d cards seen, clock %s (%04d-%02d-%02d %02d:%02d:%02d)", progress::today(),
             progress::known(), !board::has_clock() ? "not found" : has_clock ? "running" : "not usable", now.year,
             now.month, now.day, now.hour, now.minute, now.second);
    ESP_LOGI(TAG, "knoedel-watch %s", esp_app_get_description()->version);
    ESP_LOGI(TAG, "display %dx%d, screens drawn at %d,%d; pack \"%s\": %d decks, language %s%s", (int)board::display().width(),
             (int)board::display().height(), ox, oy, content_name(), deck_count(), deck_locale(),
             two_languages ? " (two in the pack)" : "");
    show();
    st_dirty = false;

    // last_input_ms: keys and touch, for switching off; last_wake_ms: also
    // picking the watch up, for dimming.
    uint32_t last_input_ms = millis(), last_wake_ms = millis(), fps_ms = millis();
    int frames = 0;
    uint32_t render_ms = 0, push_ms = 0;
    bool dimmed = false, lying_face_down = false;
    int shown_second = -1, alert_calls = 0;
    while (true) {
        uint32_t frame_start = millis();
        bool input = true;
        switch (board::update()) {
        case board::Event::BACK:
            back();
            break;
        case board::Event::PREVIOUS:
            step(-1);
            break;
        case board::Event::NEXT:
            step(1);
            break;
        case board::Event::SELECT:
            tap();
            break;
        default:
            input = board::touched();
            break;
        }

        motion::update();
        if (motion::shaken() && st.screen == CLOCK) {
            pick_word(true);  // shake for another word
            redraw();
            speak_word();
            input = true;
        }
        // Face down, the screen goes dark; on the timer this also starts it.
        if (motion::face_down() != lying_face_down) {
            lying_face_down = !lying_face_down;
            if (lying_face_down && st.screen == TIMER && !tm.running && !tm.alert) {
                timer_start();
                redraw();
            }
            board::display().setBrightness(lying_face_down ? 0 : dimmed ? BRIGHTNESS_IDLE : BRIGHTNESSES[st.brightness]);
            last_wake_ms = millis();
        }
        const bool picked_up = motion::moved() && !lying_face_down;

        if (input || (picked_up && dimmed)) {
            last_wake_ms = millis();
            last_input_ms = input ? millis() : last_input_ms;
            if (dimmed && !lying_face_down) {
                board::display().setBrightness(BRIGHTNESSES[st.brightness]);
            }
            dimmed = false;
        } else if (!dimmed && millis() - last_wake_ms > IDLE_DIM_MS) {
            // On USB power the watch is a desk clock: left alone, it shows the time.
            if (board::on_usb() && st.screen != CLOCK && st.screen != TIMER && !edit.on) {
                st.screen = CLOCK;
                pick_word(false);
                show();
            }
            if (!lying_face_down) {
                board::display().setBrightness(BRIGHTNESS_IDLE);
            }
            dimmed = true;
        } else if (millis() - last_input_ms > IDLE_OFF_MS && !tm.running && !tm.alert) {
            // Left alone: switch off, so that the battery (and with it the clock) never runs flat.
            if (!board::on_usb()) {
                ESP_LOGI(TAG, "idle for %u minutes: switching off", (unsigned)(IDLE_OFF_MS / 60000));
                power_off();
            }
            last_input_ms = millis();  // on USB power: look again in five minutes
        }
        // Written a moment after the last change, so that paging through cards costs one write.
        if (st_dirty && millis() - last_input_ms > SAVE_AFTER_MS) {
            save_state();
        }

        tune_tick();
        timer_tick();
        if (st.screen == PRACTICE && pr.phase == CHEER && millis() - pr.cheer_ms > CHEER_MS) {
            practice_next();
        }
        // A timer that has run out calls twice more, then waits quietly.
        if (tm.alert && millis() - tm.alert_ms > 6000 && alert_calls < 2) {
            play_tune(tm.focus ? FOCUS_DONE_TUNE : BREAK_DONE_TUNE);
            tm.alert_ms = millis();
            alert_calls++;
        } else if (!tm.alert) {
            alert_calls = 0;
        }
        // On the clock the word's acting and the word itself take turns.
        if (st.screen == CLOCK && millis() - cw.turn_ms > (cw.word_shown ? WORD_MS : ACT_MS)) {
            cw.word_shown = !cw.word_shown;
            cw.turn_ms = millis();
            face_shown_ms = cw.turn_ms;  // the acting starts from its beginning
            redraw();
        }
        // While the word fades in or out, its part of the screen is redrawn every frame.
        if (st.screen == CLOCK && cw.word_shown) {
            const uint32_t e = millis() - cw.turn_ms;
            if (e < FADE_MS + 40 || e > WORD_MS - FADE_MS - 40) {
                render();
                board::display().setClipRect(ox + 60, oy + 230, ui::SCREEN - 120, 160);
                fb.pushSprite(ox, oy);
                board::display().clearClipRect();
            }
        }
        // The clock and the timer redraw once a second; the word changes at midnight.
        if (st.screen == CLOCK || st.screen == TIMER || (st.screen == HOME && st.home_item == ITEM_CLOCK)) {
            const board::DateTime dt = now_dt();
            ui::set_clock(dt.hour, dt.minute, dt.second);
            const int second = st.screen == TIMER ? timer_left() : dt.second;
            if (second != shown_second && st.screen != HOME) {
                if (st.screen == CLOCK && cw.day != day_number(dt.year, dt.month, dt.day)) {
                    pick_word(false);
                }
                redraw();
            }
            shown_second = second;
        }
        if (lying_face_down) {
            sleep_ms(FRAME_MS);  // nothing to draw on a dark screen
            continue;
        }

        // The meaning follows the acting as a hint.
        if (st.screen == PRACTICE && pr.phase == ASK && !pr.hint && millis() - face_shown_ms > HINT_AFTER_MS) {
            pr.hint = true;
            redraw();
        }
        if (animated()) {
            uint32_t t0 = millis();
            render_stage();
            uint32_t t1 = millis();
            push_stage();
            render_ms += t1 - t0;
            push_ms += millis() - t1;
            frames++;
        }
        if (millis() - fps_ms >= 5000) {
            if (animated() && frames > 0) {
                ESP_LOGI(TAG, "screen %d deck %d card %d: %.1f fps (draw %lu ms, push %lu ms per frame)", st.screen,
                         st.deck, st.card, frames * 1000.0f / (millis() - fps_ms), (unsigned long)(render_ms / frames),
                         (unsigned long)(push_ms / frames));
            }
            frames = 0;
            render_ms = push_ms = 0;
            fps_ms = millis();
        }

        uint32_t spent = millis() - frame_start;
        sleep_ms(spent < FRAME_MS ? FRAME_MS - spent : 1);
    }
}
