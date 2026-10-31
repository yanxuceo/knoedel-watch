#include "notes_page.h"

#include <cJSON.h>
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "board.h"
#include "deck.h"
#include "memos.h"
#include "notes_sync.h"
#include "notes_ui.h"
#include "ui_strings.h"

namespace notes_page {

static uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

struct Task {
    std::string text, due;  // due: ISO date or ""
    bool done;
};

struct Entry {
    std::string id;
    bool waiting;    // a memo not yet a note
    float seconds;   // its length
    bool todo;
    std::vector<Task> tasks;
    std::string text;
};

static std::vector<Entry> entries;
static int pos;
static bool menu;
static std::string revealing;  // the note coming in
static uint32_t reveal_ms, cheer_ms;
static bool cheering;

// ---- dates -------------------------------------------------------------------------------

// Days since 1970-01-01 of a date (proleptic Gregorian).
static int days(int y, int m, int d)
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static int today()
{
    board::DateTime t = {};
    return board::get_time(t) ? days(t.year, t.month, t.day) : -1;
}

// "今天 14:32" from a memo's id "20261031-143208".
static std::string when(const std::string &id, const UiText &tx)
{
    int y, mo, d, h, mi;
    if (sscanf(id.c_str(), "%4d%2d%2d-%2d%2d", &y, &mo, &d, &h, &mi) != 5) {
        return "";
    }
    const int diff = today() - days(y, mo, d);
    char buf[40];
    if (diff == 0 || diff == 1) {
        snprintf(buf, sizeof buf, "%s %02d:%02d", diff == 0 ? tx.today : tx.yesterday, h, mi);
    } else {
        char date[24];
        snprintf(date, sizeof date, tx.date_short, mo, d);
        snprintf(buf, sizeof buf, "%s %02d:%02d", date, h, mi);
    }
    return buf;
}

// "明天", "周五" or "11月6日" for a task's day "2026-11-06".
static std::string due_label(const std::string &iso, const UiText &tx)
{
    int y, m, d;
    if (sscanf(iso.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) {
        return "";
    }
    const int n = days(y, m, d), diff = n - today();
    if (diff == 0) {
        return tx.today;
    }
    if (diff == 1) {
        return tx.tomorrow;
    }
    if (diff > 1 && diff < 7) {
        return tx.weekdays_short[((n % 7) + 11) % 7];  // 1970-01-01 was a Thursday
    }
    char buf[24];
    snprintf(buf, sizeof buf, tx.date_short, m, d);
    return buf;
}

// ---- the files ---------------------------------------------------------------------------

static bool parse(const std::string &json, Entry &e)
{
    cJSON *root = cJSON_Parse(json.c_str());
    if (root == nullptr) {
        return false;
    }
    const cJSON *note = cJSON_GetObjectItem(root, "note");
    const cJSON *kind = cJSON_GetObjectItem(note, "kind");
    e.todo = cJSON_IsString(kind) && strcmp(kind->valuestring, "todo") == 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItem(note, "items"))
    {
        const cJSON *t = cJSON_GetObjectItem(item, "text"), *due = cJSON_GetObjectItem(item, "due");
        e.tasks.push_back({cJSON_IsString(t) ? t->valuestring : "", cJSON_IsString(due) ? due->valuestring : "",
                           cJSON_IsTrue(cJSON_GetObjectItem(item, "done")) != 0});
    }
    const cJSON *text = cJSON_GetObjectItem(note, "text");
    e.text = cJSON_IsString(text) ? text->valuestring : "";
    if (e.todo && e.tasks.empty()) {  // nothing to tick: show it as what was said
        e.todo = false;
        const cJSON *tr = cJSON_GetObjectItem(root, "transcript");
        e.text = e.text.empty() && cJSON_IsString(tr) ? tr->valuestring : e.text;
    }
    cJSON_Delete(root);
    return true;
}

// Writes the ticks back into the note's file.
static void save(const Entry &e)
{
    cJSON *root = cJSON_Parse(memos::note(e.id).c_str());
    if (root == nullptr) {
        return;
    }
    const cJSON *items = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "note"), "items");
    int i = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, items)
    {
        if (i < (int)e.tasks.size()) {
            cJSON_DeleteItemFromObject(item, "done");
            cJSON_AddBoolToObject(item, "done", e.tasks[i].done);
        }
        i++;
    }
    char *json = cJSON_PrintUnformatted(root);
    memos::save_note(e.id, json);
    cJSON_free(json);
    cJSON_Delete(root);
}

void reload()
{
    const std::string at = pos < (int)entries.size() ? entries[pos].id : "";
    entries.clear();
    for (const std::string &id : memos::waiting()) {
        entries.push_back({id, true, memos::memo_size(id) / 2600.0f, false, {}, ""});
    }
    for (const std::string &id : memos::notes()) {
        Entry e = {id, false, 0, false, {}, ""};
        if (parse(memos::note(id), e)) {
            entries.push_back(e);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.id > b.id; });
    pos = 0;
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].id == at) {
            pos = (int)i;
        }
    }
    menu = false;
}

void step(int dir)
{
    menu = false;
    pos = std::max(0, std::min((int)entries.size() - 1, pos + dir));
}

// ---- drawing -----------------------------------------------------------------------------

static int reveal_chars()
{
    if (revealing.empty() || pos >= (int)entries.size() || entries[pos].id != revealing) {
        return -1;
    }
    const Entry &e = entries[pos];
    size_t total = e.text.size();
    for (const Task &t : e.tasks) {
        total += t.text.size();
    }
    const int n = (int)((millis() - reveal_ms) / 12);  // bytes: about 25 Chinese characters a second
    if (n >= (int)total) {
        revealing.clear();
        return -1;
    }
    return n;
}

void draw(lgfx::LovyanGFX *g, float clock)
{
    const UiText &tx = ui_text(deck_locale());
    notes_ui::Status status = {notes_sync::busy(), (int)memos::waiting().size(), notes_sync::last().limited, false};
    if (entries.empty()) {
        notes_ui::draw_page(g, nullptr, 0, 0, status, clock);
        return;
    }
    const Entry &e = entries[pos];
    const std::string w = when(e.id, tx);
    std::vector<std::string> dues;
    std::vector<notes_ui::Item> items;
    for (const Task &t : e.tasks) {
        dues.push_back(due_label(t.due, tx));
    }
    for (size_t i = 0; i < e.tasks.size(); i++) {
        items.push_back({e.tasks[i].text.c_str(), dues[i].empty() ? nullptr : dues[i].c_str(), e.tasks[i].done});
    }
    const notes_ui::Kind kind = e.waiting ? (status.online ? notes_ui::SENDING : notes_ui::WAITING)
                                          : e.todo ? notes_ui::TODO : notes_ui::IDEA;
    const notes_ui::Card card = {kind, w.c_str(), items.data(), (int)items.size(), e.text.c_str(), reveal_chars(),
                                 e.seconds};
    notes_ui::draw_page(g, &card, pos, (int)entries.size(), status, clock);
    if (menu) {
        notes_ui::draw_menu(g, kind == notes_ui::TODO);
    }
    if (cheering) {
        const float t = (millis() - cheer_ms) / 1000.0f;
        if (t < notes_ui::CHEER_SECONDS) {
            notes_ui::draw_cheer(g, t, clock);
        } else {
            cheering = false;
        }
    }
}

// ---- taps ----------------------------------------------------------------------------------

static void cheer_if_all_done(const Entry &e, bool were_all_done)
{
    const bool all = std::all_of(e.tasks.begin(), e.tasks.end(), [](const Task &t) { return t.done; });
    if (all && !were_all_done && !e.tasks.empty()) {
        cheering = true;
        cheer_ms = millis();
    }
}

bool tap(int x, int y)
{
    if (entries.empty()) {
        return false;
    }
    Entry &e = entries[pos];
    const bool all_done = std::all_of(e.tasks.begin(), e.tasks.end(), [](const Task &t) { return t.done; });
    if (menu) {
        menu = false;
        switch (notes_ui::menu_at(e.todo && !e.waiting, x, y)) {
        case notes_ui::ALL_DONE:
            for (Task &t : e.tasks) {
                t.done = true;
            }
            save(e);
            cheer_if_all_done(e, all_done);
            break;
        case notes_ui::REMOVE:
            if (e.waiting) {
                memos::remove_memo(e.id);
            } else {
                memos::remove_note(e.id);
            }
            entries.erase(entries.begin() + pos);
            pos = std::max(0, std::min(pos, (int)entries.size() - 1));
            break;
        default:
            break;
        }
        return true;
    }
    const int at = notes_ui::item_at(x, y);
    if (at >= 0 && e.todo && at < (int)e.tasks.size()) {
        e.tasks[at].done = !e.tasks[at].done;
        save(e);
        cheer_if_all_done(e, all_done);
        return true;
    }
    if (at == -1) {
        menu = true;
        return true;
    }
    return false;
}

bool menu_open() { return menu; }

void close_menu() { menu = false; }

bool animating() { return cheering || !revealing.empty() || (notes_sync::busy() && !memos::waiting().empty()); }

void arrived(const std::string &note_json)
{
    std::string id;
    if (cJSON *root = cJSON_Parse(note_json.c_str())) {
        const cJSON *i = cJSON_GetObjectItem(root, "id");
        id = cJSON_IsString(i) ? i->valuestring : "";
        cJSON_Delete(root);
    }
    reload();
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].id == id) {
            pos = (int)i;
            revealing = id;
            reveal_ms = millis();
        }
    }
}

std::string summary(const std::string &note_json)
{
    Entry e;
    if (!parse(note_json, e)) {
        return "";
    }
    if (e.todo && !e.tasks.empty()) {
        return e.tasks[0].text + (e.tasks.size() > 1 ? "  +" + std::to_string(e.tasks.size() - 1) : "");
    }
    return e.text;
}

}  // namespace notes_page
