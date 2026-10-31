// Desktop build of the card faces: the same drawing code the watch runs.
//
//   card_sim render <assets> <out> [slot]       every screen and card face, as PPM files
//   card_sim anim <assets> <out> <deck> <card> N   N animation frames of one card's face 1
//   card_sim check <assets> [slot]              does every text fit? (exit code 1 if not)
//   card_sim ids                                what the firmware can draw, as JSON
//   card_sim schedule <assets>                  three weeks of practice, to check the scheduler
//
// <assets> is a directory written by tools/import_decks.py; slot 0 or 1 picks the language.

#include <M5GFX.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "card_ui.h"
#include "deck.h"
#include "progress.h"
#include "character.h"
#include "notes_ui.h"
#include "ui_strings.h"

static std::vector<uint8_t> read_file(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fprintf(stderr, "cannot read %s\n", path.c_str());
        exit(2);
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

static void save(lgfx::LGFX_Sprite &sp, const std::string &path)
{
    // Grey out everything outside the round glass, as on the real panel.
    for (int y = 0; y < ui::SCREEN; y++) {
        for (int x = 0; x < ui::SCREEN; x++) {
            float dx = x + 0.5f - ui::CX, dy = y + 0.5f - ui::CY;
            if (dx * dx + dy * dy > ui::CX * ui::CX) {
                sp.drawPixel(x, y, (uint32_t)0x202020);
            }
        }
    }
    std::vector<uint8_t> rgb(ui::SCREEN * ui::SCREEN * 3);
    for (int y = 0, i = 0; y < ui::SCREEN; y++) {
        for (int x = 0; x < ui::SCREEN; x++) {
            auto c = sp.readPixelRGB(x, y);
            rgb[i++] = c.r;
            rgb[i++] = c.g;
            rgb[i++] = c.b;
        }
    }
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << ui::SCREEN << " " << ui::SCREEN << "\n255\n";
    f.write((const char *)rgb.data(), rgb.size());
}

static void print_ids()
{
    static const char *const KEYS[] = {"pose", "motion", "fx", "prop", "bubble"};
    printf("{\n");
    for (int who = 0; who < character::count(); who++) {
        printf("  \"%s\": {\n", character::id(who));
        for (int k = 0; k < character::KIND_COUNT; k++) {
            const character::Kind kind = (character::Kind)k;
            printf("    \"%s\": %c", KEYS[k], k == 0 ? '{' : '[');
            for (int i = 0; i < character::count(who, kind); i++) {
                printf("%s\"%s\"", i ? ", " : "", character::name(who, kind, i));
                if (k == 0) {
                    printf(": %g", character::pose_neutral(who, i));
                }
            }
            printf("%c%s\n", k == 0 ? '}' : ']', k + 1 < character::KIND_COUNT ? "," : "");
        }
        printf("  }%s\n", who + 1 < character::count() ? "," : "");
    }
    printf("}\n");
}

// Plays three weeks of practice with a learner who knows about four cards in
// five, and prints what the scheduler does.
static int schedule_test()
{
    progress::load(100);
    int seen = 0;
    for (int d = 0; d < deck_count(); d++) {
        for (int c = 0; c < deck_info(d).cards && seen < 20; c++, seen++) {
            progress::mark_seen(d, c);
        }
    }
    srand(7);
    for (int day = 100; day < 121; day++) {
        progress::set_clock_day(day == 110 ? 3 : day < 110 ? day : day - 107);  // the clock is reset on day 110
        progress::Pick picks[progress::SESSION_CARDS];
        const int n = progress::pick_session(picks, progress::SESSION_CARDS);
        int due = 0, right = 0;
        for (int i = 0; i < n; i++) {
            const bool was_due = progress::due_of(picks[i].deck, picks[i].card) <= progress::today();
            const bool knew = rand() % 5 != 0;
            due += was_due;
            right += knew;
            progress::answer(picks[i].deck, picks[i].card, knew);
        }
        progress::session_done();
        int boxes[8] = {};
        for (int d = 0; d < deck_count(); d++) {
            for (int c = 0; c < deck_info(d).cards; c++) {
                if (progress::box_of(d, c) >= 0) {
                    boxes[progress::box_of(d, c)]++;
                }
            }
        }
        printf("day %3d  asked %d (due %d, right %d)  boxes", progress::today(), n, due, right);
        for (int b : boxes) {
            printf(" %2d", b);
        }
        printf("\n");
    }
    return progress::known() == seen ? 0 : 1;
}

int main(int argc, char **argv)
{
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "ids") {
        print_ids();
        return 0;
    }
    if (argc < 3 || (mode != "render" && mode != "anim" && mode != "check" && mode != "schedule" && mode != "clockpop" && mode != "notes")) {
        fprintf(stderr, "usage: card_sim render|anim|check|ids ... (see sim_main.cpp)\n");
        return 2;
    }
    const std::string dir = argv[2], out = argc > 3 ? argv[3] : ".";
    const char *slot_arg = mode == "check" ? (argc > 3 ? argv[3] : "0") : mode == "render" || mode == "notes" ? (argc > 4 ? argv[4] : "0") : "0";
    const std::string slot = std::string("loc") + slot_arg;
    static auto pack = read_file(dir + "/decks_" + slot + ".bin");
    static auto de_xl = read_file(dir + "/font_de_xl.vlw"), de_l = read_file(dir + "/font_de_l.vlw"),
                de_m = read_file(dir + "/font_de_m.vlw"), de_mb = read_file(dir + "/font_de_mb.vlw"),
                de_s = read_file(dir + "/font_de_s.vlw"), de_xs = read_file(dir + "/font_de_xs.vlw"), loc_m = read_file(dir + "/font_" + slot + "_m.vlw"),
                loc_s = read_file(dir + "/font_" + slot + "_s.vlw"), loc_l = read_file(dir + "/font_" + slot + "_l.vlw"),
                clock_font = read_file(dir + "/font_clock.vlw");
    if (!deck_init(pack.data(), pack.size())) {
        fprintf(stderr, "%s: not a deck pack this build understands\n", dir.c_str());
        return 2;
    }
    ui::init({de_xl.data(), de_l.data(), de_m.data(), de_mb.data(), de_s.data(), de_xs.data(), clock_font.data()});
    ui::set_locale(loc_m.data(), loc_s.data(), loc_l.data());

    if (mode == "schedule") {
        return schedule_test();
    }

    lgfx::LGFX_Sprite fb;
    fb.setColorDepth(16);
    fb.createSprite(ui::SCREEN, ui::SCREEN);
    char name[96];

    if (mode == "clockpop") {
        // The clock's picture at the sizes it passes through when it pops up.
        const float scales[] = {0.05f, 0.2f, 0.4f, 0.6f, 0.8f, 0.95f, 1.03f, 1.0f};
        for (int i = 0; i < 8; i++) {
            const ui::ClockFace face = {14, 35, 20, "Thursday, 15 October", 0, 4, false, 1.0f, scales[i]};
            ui::draw_clock(&fb, face);
            ui::draw_clock_stage(&fb, face, 0.1f, 10.0f);
            snprintf(name, sizeof(name), "/pop_%d.ppm", i);
            save(fb, out + name);
        }
        return 0;
    }
    if (mode == "notes") {
        // Every state of the notes' screens (notes_ui.h), for a look at the design.
        static auto notes_font = read_file(dir + "/font_notes.vlw");
        notes_ui::init(notes_font.data());
        notes_ui::set_locale(deck_locale());
        const UiText &t = ui_text(deck_locale());
        const bool zh = strncmp(deck_locale(), "zh", 2) == 0;
        const notes_ui::Item todo[] = {{zh ? "给房东打电话 wegen der Heizung" : "Call the landlord wegen der Heizung", t.tomorrow, false},
                                       {zh ? "买 Milch 和鸡蛋" : "Buy Milch and eggs", nullptr, true},
                                       {zh ? "交 Quartalsbericht" : "Hand in the Quartalsbericht", t.weekdays_short[5], false}};
        const notes_ui::Item many[] = {todo[0], todo[1], todo[2], {"Bürgeramt Termin buchen", nullptr, false},
                                       {"Pick up the parcel at DHL", nullptr, false}};
        const char *idea = zh ? "手表可以做一个德语口语陪练：每天五分钟，用卡片里学过的词聊天，说错了轻轻纠正。"
                              : "The watch could talk German with me five minutes a day, with the words of the cards.";
        const std::string today = std::string(t.today) + " 14:32";
        const notes_ui::Status none = {false, 0, false, false}, offline = {false, 2, false, false}, online = {true, 1, false, false};
        const notes_ui::Card c_todo = {notes_ui::TODO, today.c_str(), todo, 3, nullptr, -1, 0};
        const notes_ui::Card c_many = {notes_ui::TODO, today.c_str(), many, 5, nullptr, -1, 0};
        const notes_ui::Card c_reveal = {notes_ui::TODO, today.c_str(), todo, 3, nullptr, 20, 0};
        const std::string yesterday = std::string(t.yesterday) + " 22:10";
        const notes_ui::Card c_idea = {notes_ui::IDEA, yesterday.c_str(), nullptr, 0, idea, -1, 0};
        const notes_ui::Card c_wait = {notes_ui::WAITING, today.c_str(), nullptr, 0, nullptr, -1, 9};
        const notes_ui::Card c_send = {notes_ui::SENDING, today.c_str(), nullptr, 0, nullptr, -1, 9};
        int n = 0;
        auto shot = [&](const char *what) {
            snprintf(name, sizeof(name), "/notes_%02d_%s.ppm", n++, what);
            save(fb, out + name);
        };
        notes_ui::draw_recording(&fb, 7.3f, 60, 0.45f, 3.1f);
        shot("recording");
        notes_ui::draw_recording(&fb, 53.6f, 60, 0.7f, 5.2f);
        shot("recording_late");
        notes_ui::draw_message(&fb, notes_ui::KEPT_ONLINE, 0.18f);
        shot("kept_closing");
        notes_ui::draw_message(&fb, notes_ui::KEPT_OFFLINE, 1);
        shot("kept_offline");
        notes_ui::draw_message(&fb, notes_ui::TOO_SHORT, 1);
        shot("too_short");
        notes_ui::draw_page(&fb, &c_todo, 1, 5, none, 0);
        shot("todo");
        notes_ui::draw_page(&fb, &c_many, 0, 5, none, 0);
        notes_ui::draw_menu(&fb, true);
        shot("menu");
        notes_ui::draw_page(&fb, &c_idea, 2, 5, none, 0);
        shot("idea");
        notes_ui::draw_page(&fb, &c_wait, 0, 5, offline, 0);
        shot("waiting");
        notes_ui::draw_page(&fb, &c_send, 0, 5, online, 0.2f);
        shot("sending");
        notes_ui::draw_page(&fb, &c_reveal, 0, 5, none, 0);
        shot("coming_in");
        notes_ui::draw_page(&fb, nullptr, 0, 0, none, 0);
        shot("empty");
        ui::draw_menu(&fb, 5, 6, t.notes);
        ui::draw_menu_stage(&fb, ui::ICON_NOTES, 0, 2.5f);
        shot("home");
        const ui::ClockFace face = {14, 35, 20, zh ? "星期六 · 10月31日" : "Saturday, 31 October", 0, 4, false, 1.0f, 1.0f};
        ui::draw_clock(&fb, face);
        ui::draw_clock_stage(&fb, face, 0.1f, 10.0f);
        notes_ui::draw_toast(&fb, (std::string(t.noted) + todo[0].text + "  +2").c_str());
        shot("toast");
        notes_ui::draw_page(&fb, &c_todo, 1, 5, none, 0);
        notes_ui::draw_cheer(&fb, 0.8f, 4.0f);
        shot("cheer");
        return 0;
    }
    if (mode == "anim") {
        if (argc < 7) {
            return 2;
        }
        const int deck = atoi(argv[4]), card = atoi(argv[5]), frames = atoi(argv[6]);
        for (int i = 0; i < frames; i++) {
            float t = character::LOOP_SECONDS * i / frames;
            ui::draw_face(&fb, deck, card, 0);
            ui::draw_stage(&fb, deck, card, t, 10.0f + t);
            snprintf(name, sizeof(name), "/anim_%d_%02d_%02d.ppm", deck, card, i);
            save(fb, out + name);
        }
        return 0;
    }

    int misfits = 0;
    // Menus, the messages of the practice mode, settings, clock, timer and celebrations.
    const UiText &tx = ui_text(deck_locale());
    const char *const menu[7] = {tx.clock, tx.learn, tx.practice, tx.timer, tx.settings, tx.well_done, tx.learn_first};
    const ui::MenuIcon icons[7] = {ui::ICON_CLOCK, ui::ICON_KNOEDEL, ui::ICON_BREZEL, ui::ICON_TIMER, ui::ICON_GEAR,
                                   ui::ICON_BREZEL, ui::ICON_KNOEDEL};
    const char *const settings[5][3] = {{tx.language, tx.language_name, tx.tap_change}, {tx.time, "14:35", tx.tap_change},
                                        {tx.volume, "4 / 5", tx.tap_change}, {tx.brightness, "3 / 4", tx.tap_change},
                                        {tx.power_off, nullptr, tx.tap_off}};
    char streak[32];
    snprintf(streak, sizeof(streak), tx.in_a_row, 6);
    ui::set_clock(14, 35, 20);
    for (int i = 0; i < 22; i++) {
        bool ok = true;
        if (i < 7) {
            ok = ui::draw_menu(&fb, i < 5 ? i : 0, i < 5 ? 5 : 1, menu[i]);
            if (mode == "render") {
                ui::draw_menu_stage(&fb, icons[i], 4.2f, 10.0f);
            }
        } else if (i < 12) {
            ok = ui::draw_setting(&fb, i - 7, 5, settings[i - 7][0], settings[i - 7][1], settings[i - 7][2]);
        } else if (i < 14) {
            ok = ui::draw_setting_edit(&fb, 1, 5, tx.time, i == 12 ? "{{14}}:35" : "14.{{10}}.2026", tx.edit_hint);
        } else if (i < 16) {
            const ui::ClockFace face = {14, 35, 20, strncmp(deck_locale(), "zh", 2) == 0 ? "10月15日 星期四" : "Thursday, 15 October",
                                        i == 14 ? 0 : 4, i == 14 ? 4 : 6, i == 15, 1.0f, 1.0f};
            ok = ui::draw_clock(&fb, face);
            if (mode == "render") {
                ui::draw_clock_stage(&fb, face, 4.2f, 10.0f);
            }
        } else if (i < 19) {
            const ui::TimerFace face = {i == 16 ? 1500 : i == 17 ? 1123 : 187, i == 18 ? 300 : 1500, i != 18, i == 17,
                                        i != 18 ? tx.focus : tx.pause, i == 16 ? tx.start_hint : i == 17 ? tx.running_hint : tx.paused_hint};
            ok = ui::draw_timer(&fb, face);
            if (mode == "render") {
                ui::draw_timer_stage(&fb, face, 4.2f, 10.0f);
            }
        } else {
            const char *const german[3] = {"Wahnsinn!", "Pause!", "Weiter!"};
            const char *const local[3] = {streak, tx.break_now, tx.back_to_work};
            ok = ui::draw_message(&fb, german[i - 19], local[i - 19], 0xE0A526);
            if (mode == "render") {
                ui::draw_celebration_stage(&fb, i - 19 + 3, 4.2f, 10.0f);
            }
        }
        if (!ok) {
            printf("MISFIT screen %d\n", i);
            misfits++;
        }
        if (mode == "render") {
            snprintf(name, sizeof(name), "/screen_%02d.ppm", i);
            save(fb, out + name);
        }
    }
    if (mode == "render") {
        for (int v = 0; v < ui::CELEBRATIONS; v++) {
            ui::draw_message(&fb, "Super!", streak, 0xE0A526);
            ui::draw_celebration_stage(&fb, v, 4.2f, 10.0f);
            snprintf(name, sizeof(name), "/cheer_%d.ppm", v);
            save(fb, out + name);
        }
    }
    for (int d = 0; d < deck_count(); d++) {
        const DeckInfo info = deck_info(d);
        for (int c = ui::COVER; c < info.cards; c++) {
            const char *id = c == ui::COVER ? "cover" : deck_card(d, c).id;
            for (int face = 0; face < ui::face_count(d, c); face++) {
                if (!ui::draw_face(&fb, d, c, face)) {
                    printf("MISFIT %s %s face %d\n", info.id, id, face + 1);
                    misfits++;
                }
                if (mode == "render") {
                    if (face == 0) {
                        ui::draw_stage(&fb, d, c, 4.2f, 10.0f + c * 0.37f);
                    }
                    snprintf(name, sizeof(name), "/%s_%02d_%d.ppm", info.id, c + 1, face + 1);
                    save(fb, out + name);
                }
            }
            // Practice: the question with its hint, and the answer.
            for (int step = 0; step < 2 && c != ui::COVER; step++) {
                const bool ok = step == 0 ? ui::draw_question(&fb, d, c, 2, 8, true) : ui::draw_answer(&fb, d, c, 2, 8);
                if (!ok) {
                    printf("MISFIT %s %s practice %s\n", info.id, id, step == 0 ? "question" : "answer");
                    misfits++;
                }
                if (mode == "render" && c < 2) {
                    ui::draw_stage(&fb, d, c, 4.2f, 10.0f + c * 0.37f);
                    snprintf(name, sizeof(name), "/%s_%02d_practice_%d.ppm", info.id, c + 1, step);
                    save(fb, out + name);
                }
            }
        }
    }
    return mode == "check" && misfits ? 1 : 0;
}
