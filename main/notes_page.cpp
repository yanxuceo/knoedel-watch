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
    std::string text, due, done_on;  // due, done_on: ISO dates or ""
    bool done;
};

struct Note {
    std::string id;
    bool todo;
    std::vector<Task> tasks;
    std::string text;
};

struct Ref {
    int note, task;  // task -1: an idea
};

static std::vector<Note> notes;               // newest first
static std::vector<Ref> task_rows, idea_rows, done_rows;  // the lists, sorted
static bool loaded;
static constexpr int KEEP_DONE_DAYS = 90;
static notes_ui::List list = notes_ui::TASKS;
static int page;
static bool menu;
static int held = -1;    // the row (on this page) the menu is for
static int opened = -1;  // the idea shown whole
static std::string revealing;  // the note whose rows come in
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

static int iso_days(const std::string &iso)
{
    int y, m, d;
    return sscanf(iso.c_str(), "%4d-%2d-%2d", &y, &m, &d) == 3 ? days(y, m, d) : -1;
}

static int today()
{
    board::DateTime t = {};
    return board::get_time(t) ? days(t.year, t.month, t.day) : -1;
}

static std::string today_iso()
{
    board::DateTime t = {};
    char buf[40] = "";
    if (board::get_time(t)) {
        snprintf(buf, sizeof buf, "%04d-%02d-%02d", t.year, t.month, t.day);
    }
    return buf;
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

// "今天", "昨天" or "10月29日" for the day a task was done.
static std::string day_label(const std::string &iso, const UiText &tx)
{
    int y, m, d;
    if (sscanf(iso.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) {
        return "";
    }
    const int ago = today() - days(y, m, d);
    if (ago == 0 || ago == 1) {
        return ago == 0 ? tx.today : tx.yesterday;
    }
    char buf[24];
    snprintf(buf, sizeof buf, tx.date_short, m, d);
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

static bool parse(const std::string &json, Note &n)
{
    cJSON *root = cJSON_Parse(json.c_str());
    if (root == nullptr) {
        return false;
    }
    const cJSON *note = cJSON_GetObjectItem(root, "note");
    const cJSON *kind = cJSON_GetObjectItem(note, "kind");
    n.todo = cJSON_IsString(kind) && strcmp(kind->valuestring, "todo") == 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItem(note, "items"))
    {
        const cJSON *t = cJSON_GetObjectItem(item, "text"), *due = cJSON_GetObjectItem(item, "due"),
                    *on = cJSON_GetObjectItem(item, "done_on");
        n.tasks.push_back({cJSON_IsString(t) ? t->valuestring : "", cJSON_IsString(due) ? due->valuestring : "",
                           cJSON_IsString(on) ? on->valuestring : "",
                           cJSON_IsTrue(cJSON_GetObjectItem(item, "done")) != 0});
    }
    const cJSON *text = cJSON_GetObjectItem(note, "text");
    n.text = cJSON_IsString(text) ? text->valuestring : "";
    if (n.todo && n.tasks.empty()) {  // nothing to tick: what was said, as an idea
        n.todo = false;
        const cJSON *tr = cJSON_GetObjectItem(root, "transcript");
        n.text = n.text.empty() && cJSON_IsString(tr) ? tr->valuestring : n.text;
    }
    cJSON_Delete(root);
    return true;
}

// Writes a note's tasks back (ticks, a task deleted); with none left, the note goes.
static void save(const Note &n)
{
    if (n.todo && n.tasks.empty()) {
        memos::remove_note(n.id);
        return;
    }
    cJSON *root = cJSON_Parse(memos::note(n.id).c_str());
    cJSON *note = root ? cJSON_GetObjectItem(root, "note") : nullptr;
    if (note == nullptr) {
        cJSON_Delete(root);
        return;
    }
    cJSON *items = cJSON_CreateArray();
    for (const Task &t : n.tasks) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "text", t.text.c_str());
        if (t.due.empty()) {
            cJSON_AddNullToObject(item, "due");
        } else {
            cJSON_AddStringToObject(item, "due", t.due.c_str());
        }
        cJSON_AddBoolToObject(item, "done", t.done);
        if (t.done) {
            cJSON_AddStringToObject(item, "done_on", t.done_on.c_str());
        }
        cJSON_AddItemToArray(items, item);
    }
    cJSON_ReplaceItemInObject(note, "items", items);
    char *json = cJSON_PrintUnformatted(root);
    memos::save_note(n.id, json);
    cJSON_free(json);
    cJSON_Delete(root);
}

// The two lists from the notes: open tasks by day, those without a day after
// them (newest note first), then today's ticked ones; ideas newest first.
static void build_lists()
{
    task_rows.clear();
    idea_rows.clear();
    done_rows.clear();
    const int now = today();
    for (int i = 0; i < (int)notes.size(); i++) {
        const Note &n = notes[i];
        if (!n.todo) {
            idea_rows.push_back({i, -1});
            continue;
        }
        for (int k = 0; k < (int)n.tasks.size(); k++) {
            const Task &t = n.tasks[k];
            if (t.done) {
                done_rows.push_back({i, k});
            }
            if (t.done && (t.done_on.empty() || iso_days(t.done_on) < now)) {
                continue;  // ticked before today: put away
            }
            task_rows.push_back({i, k});
        }
    }
    std::stable_sort(done_rows.begin(), done_rows.end(), [](const Ref &a, const Ref &b) {
        return iso_days(notes[a.note].tasks[a.task].done_on) > iso_days(notes[b.note].tasks[b.task].done_on);
    });
    std::stable_sort(task_rows.begin(), task_rows.end(), [](const Ref &a, const Ref &b) {
        const Task &x = notes[a.note].tasks[a.task], &y = notes[b.note].tasks[b.task];
        if (x.done != y.done) {
            return !x.done;
        }
        const int dx = x.due.empty() ? 1 << 30 : iso_days(x.due), dy = y.due.empty() ? 1 << 30 : iso_days(y.due);
        return dx < dy;  // equal days keep the notes' order, newest first
    });
}

static std::vector<Ref> &rows() { return list == notes_ui::TASKS ? task_rows : list == notes_ui::IDEAS ? idea_rows : done_rows; }

static int row_pages() { return ((int)rows().size() + notes_ui::ROWS_PER_PAGE - 1) / notes_ui::ROWS_PER_PAGE; }

// The tasks end with the page of what was done.
static int pages() { return list == notes_ui::TASKS ? row_pages() + 1 : std::max(1, row_pages()); }

static bool on_stats() { return list == notes_ui::TASKS && page == row_pages(); }

// What was done this week (Monday first) and the days in a row up to today.
static notes_ui::Stats stats()
{
    notes_ui::Stats st = {};
    const int now = today();
    st.today = ((now % 7) + 10) % 7;  // 1970-01-01 was a Thursday: Monday is 0
    std::vector<int> per_day(KEEP_DONE_DAYS + 1, 0);
    for (const Ref &r : done_rows) {
        const int ago = now - iso_days(notes[r.note].tasks[r.task].done_on);
        if (ago >= 0 && ago <= KEEP_DONE_DAYS) {
            per_day[ago]++;
        }
    }
    for (int d = 0; d <= st.today; d++) {
        st.week[d] = per_day[st.today - d];
    }
    // Today without anything done yet does not break the run.
    for (int ago = per_day[0] ? 0 : 1; ago <= KEEP_DONE_DAYS && per_day[ago]; ago++) {
        st.streak++;
    }
    st.tasks = (int)task_rows.size();
    st.ideas = (int)idea_rows.size();
    st.page = page;
    st.pages = pages();
    return st;
}

void reload()
{
    notes.clear();
    const int now = today();
    for (const std::string &id : memos::notes()) {
        Note n = {id, false, {}, ""};
        if (!parse(memos::note(id), n)) {
            continue;
        }
        // What was done more than 90 days ago goes; a note with nothing left goes too.
        const size_t before = n.tasks.size();
        n.tasks.erase(std::remove_if(n.tasks.begin(), n.tasks.end(),
                                     [now](const Task &t) {
                                         return t.done && now >= 0 && iso_days(t.done_on) >= 0 &&
                                                now - iso_days(t.done_on) > KEEP_DONE_DAYS;
                                     }),
                      n.tasks.end());
        if (n.tasks.size() != before) {
            save(n);
            if (n.tasks.empty()) {
                continue;
            }
        }
        notes.push_back(n);
    }
    loaded = true;
    build_lists();
    page = std::min(page, pages() - 1);
    menu = false;
    held = opened = -1;
}

void step(int dir)
{
    menu = false;
    held = -1;
    if (opened >= 0) {  // an idea shown whole: the next or previous idea
        opened = std::max(0, std::min((int)idea_rows.size() - 1, opened + dir));
        return;
    }
    if (list == notes_ui::DONE) {
        page = std::max(0, std::min(page + dir, pages() - 1));
        return;
    }
    page += dir;
    if (page >= pages() && list == notes_ui::TASKS) {
        list = notes_ui::IDEAS;
        page = 0;
    } else if (page < 0 && list == notes_ui::IDEAS) {
        list = notes_ui::TASKS;
        page = pages() - 1;
    }
    page = std::max(0, std::min(page, pages() - 1));
}

// ---- drawing -----------------------------------------------------------------------------

static int reveal_for(const Note &n)
{
    if (revealing.empty() || n.id != revealing) {
        return -1;
    }
    const int shown = (int)((millis() - reveal_ms) / 40);  // characters: 25 a second
    size_t longest = n.text.size();
    for (const Task &t : n.tasks) {
        longest = std::max(longest, t.text.size());
    }
    if (shown >= (int)longest) {
        revealing.clear();
        return -1;
    }
    return shown;
}

void draw(lgfx::LovyanGFX *g, float clock)
{
    const UiText &tx = ui_text(deck_locale());
    const notes_ui::Status status = {notes_sync::busy(), (int)memos::waiting().size(), notes_sync::last().limited, false};
    if (on_stats()) {
        notes_ui::draw_stats(g, stats(), status);
    } else if (opened >= 0 && opened < (int)idea_rows.size()) {
        const Note &n = notes[idea_rows[opened].note];
        const std::string w = when(n.id, tx);
        const notes_ui::Card card = {notes_ui::IDEA, w.c_str(), nullptr, 0, n.text.c_str(), -1, 0};
        notes_ui::draw_page(g, &card, opened, (int)idea_rows.size(), status, clock);
    } else {
        std::vector<std::string> labels;
        std::vector<notes_ui::Row> shown;
        const int first = page * notes_ui::ROWS_PER_PAGE;
        const int last = std::min((int)rows().size(), first + notes_ui::ROWS_PER_PAGE);
        for (int i = first; i < last; i++) {
            const Ref &r = rows()[i];
            // The done list tells the day it was done; the tasks, the day it is due.
            labels.push_back(r.task < 0 ? ""
                             : list == notes_ui::DONE ? day_label(notes[r.note].tasks[r.task].done_on, tx)
                                                      : due_label(notes[r.note].tasks[r.task].due, tx));
        }
        for (int i = first, k = 0; i < last; i++, k++) {
            const Ref &r = rows()[i];
            const Note &n = notes[r.note];
            if (r.task >= 0) {
                const Task &t = n.tasks[r.task];
                shown.push_back({t.text.c_str(), labels[k].empty() ? nullptr : labels[k].c_str(), t.done, reveal_for(n)});
            } else {
                shown.push_back({n.text.c_str(), nullptr, false, reveal_for(n)});
            }
        }
        const notes_ui::ListPage lp = {list, (int)task_rows.size(), (int)idea_rows.size(), (int)done_rows.size(),
                                       shown.data(), (int)shown.size(), page, pages(), menu ? held : -1};
        notes_ui::draw_list(g, lp, status, clock);
        if (menu && held >= 0 && held < (int)shown.size()) {
            notes_ui::draw_menu(g, list == notes_ui::DONE ? notes_ui::UNDO : notes_ui::NOTHING, shown[held].text);
        }
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

static Ref *ref_on_page(int row)
{
    const int i = page * notes_ui::ROWS_PER_PAGE + row;
    return row >= 0 && i < (int)rows().size() ? &rows()[i] : nullptr;
}

bool tap(int x, int y)
{
    if (menu) {
        const Ref *r = ref_on_page(held);
        const notes_ui::Choice c =
            notes_ui::menu_at(list == notes_ui::DONE ? notes_ui::UNDO : notes_ui::NOTHING, x, y);
        if (c == notes_ui::UNDO && r != nullptr) {
            Note &n = notes[r->note];
            n.tasks[r->task].done = false;
            n.tasks[r->task].done_on.clear();
            save(n);
            reload();
        } else if (c == notes_ui::REMOVE && r != nullptr) {
            Note &n = notes[r->note];
            if (r->task >= 0) {
                n.tasks.erase(n.tasks.begin() + r->task);
                save(n);
            } else {
                memos::remove_note(n.id);
            }
            reload();
        }
        menu = false;
        held = -1;
        return true;
    }
    if (opened >= 0) {  // a tap on an idea shown whole: back to the list
        opened = -1;
        return true;
    }
    int tab;
    const int row = notes_ui::row_at(x, y, tab);
    if (on_stats() && tab < 0) {  // what was done: the list of it
        list = notes_ui::DONE;
        page = 0;
        return true;
    }
    if (tab >= 0) {
        if (tab != (int)list) {
            list = (notes_ui::List)tab;
            page = 0;
        }
        return true;
    }
    Ref *r = ref_on_page(row);
    if (r == nullptr) {
        return false;
    }
    if (r->task < 0) {
        opened = page * notes_ui::ROWS_PER_PAGE + row;
        return true;
    }
    if (list == notes_ui::DONE) {
        return false;  // the done list changes only through its menu
    }
    Note &n = notes[r->note];
    Task &t = n.tasks[r->task];
    t.done = !t.done;
    t.done_on = t.done ? today_iso() : "";
    save(n);
    if (t.done && std::all_of(n.tasks.begin(), n.tasks.end(), [](const Task &x) { return x.done; })) {
        cheering = true;
        cheer_ms = millis();
    }
    return true;  // the row keeps its place until the page is opened again
}

bool hold(int x, int y)
{
    if (menu || opened >= 0 || on_stats()) {
        return false;
    }
    int tab;
    const int row = notes_ui::row_at(x, y, tab);
    if (ref_on_page(row) == nullptr) {
        return false;
    }
    held = row;
    menu = true;
    return true;
}

bool menu_open() { return menu || opened >= 0 || list == notes_ui::DONE; }

void close_menu()
{
    if (!menu && opened < 0 && list == notes_ui::DONE) {  // back from what was done to its page
        list = notes_ui::TASKS;
        page = row_pages();
        return;
    }
    menu = false;
    held = opened = -1;
}

int done_today()
{
    if (!loaded) {
        reload();
    }
    const int now = today();
    int n = 0;
    for (const Ref &r : done_rows) {
        n += iso_days(notes[r.note].tasks[r.task].done_on) == now;
    }
    return n;
}

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
    // Show the page where the new note's first row landed, its text coming in.
    for (const notes_ui::List l : {notes_ui::TASKS, notes_ui::IDEAS}) {
        const std::vector<Ref> &rs = l == notes_ui::TASKS ? task_rows : idea_rows;
        for (int i = 0; i < (int)rs.size(); i++) {
            if (notes[rs[i].note].id == id) {
                list = l;
                page = i / notes_ui::ROWS_PER_PAGE;
                revealing = id;
                reveal_ms = millis();
                return;
            }
        }
    }
}

std::string summary(const std::string &note_json)
{
    Note n;
    if (!parse(note_json, n)) {
        return "";
    }
    if (n.todo && !n.tasks.empty()) {
        return n.tasks[0].text + (n.tasks.size() > 1 ? "  +" + std::to_string(n.tasks.size() - 1) : "");
    }
    return n.text;
}

}  // namespace notes_page
