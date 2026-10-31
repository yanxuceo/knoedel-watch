#include "sentences.h"

#include <cJSON.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <mutex>

#include "board.h"

namespace sentences {

static const char *PATH = "/notes/sentences.json", *TMP = "/notes/sentences.json.tmp";
static std::vector<Sentence> list;
static bool loaded;
static std::mutex lock;  // the screen reads, the USB commands write

static std::string field(const cJSON *o, const char *name)
{
    const cJSON *v = cJSON_GetObjectItem(o, name);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static bool from_json(const cJSON *o, Sentence &s)
{
    s.id = field(o, "id");
    s.zh = field(o, "zh");
    s.en = field(o, "en");
    s.de = field(o, "de");
    s.pattern = field(o, "pattern");
    s.topic = field(o, "topic");
    return !s.de.empty() && (!s.zh.empty() || !s.en.empty());
}

static cJSON *object(const Sentence &s)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", s.id.c_str());
    cJSON_AddStringToObject(o, "zh", s.zh.c_str());
    cJSON_AddStringToObject(o, "en", s.en.c_str());
    cJSON_AddStringToObject(o, "de", s.de.c_str());
    cJSON_AddStringToObject(o, "pattern", s.pattern.c_str());
    cJSON_AddStringToObject(o, "topic", s.topic.c_str());
    return o;
}

std::string to_json(const Sentence &s)
{
    cJSON *o = object(s);
    char *text = cJSON_PrintUnformatted(o);
    std::string json = text;
    cJSON_free(text);
    cJSON_Delete(o);
    return json;
}

static void load()
{
    loaded = true;
    list.clear();
    FILE *f = fopen(PATH, "rb");
    if (f == nullptr) {
        return;
    }
    std::string text;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        text.append(buf, n);
    }
    fclose(f);
    cJSON *root = cJSON_Parse(text.c_str());
    const cJSON *o;
    cJSON_ArrayForEach(o, root)
    {
        Sentence s;
        if (from_json(o, s)) {
            list.push_back(s);
        }
    }
    cJSON_Delete(root);
}

static bool save()
{
    cJSON *root = cJSON_CreateArray();
    for (const Sentence &s : list) {
        cJSON_AddItemToArray(root, object(s));
    }
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    FILE *f = fopen(TMP, "wb");
    bool ok = f != nullptr && fwrite(text, 1, strlen(text), f) == strlen(text);
    if (f != nullptr) {
        ok &= fclose(f) == 0;
    }
    cJSON_free(text);
    ok = ok && rename(TMP, PATH) == 0;
    if (!ok) {
        unlink(TMP);
    }
    return ok;
}

static void ensure_loaded()
{
    if (!loaded) {
        load();
    }
}

std::vector<Sentence> all()
{
    std::lock_guard<std::mutex> hold(lock);
    ensure_loaded();
    return list;
}

static std::string new_id()
{
    board::DateTime t = {};
    char base[40] = "00000000-000000";
    if (board::get_time(t)) {
        snprintf(base, sizeof base, "%04d%02d%02d-%02d%02d%02d", t.year, t.month, t.day, t.hour, t.minute, t.second);
    }
    std::string id = base;
    for (int n = 2;; n++) {
        bool taken = false;
        for (const Sentence &s : list) {
            taken |= s.id == id;
        }
        if (!taken) {
            return id;
        }
        id = std::string(base) + "-" + std::to_string(n);
    }
}

std::string add(const std::string &json)
{
    std::lock_guard<std::mutex> hold(lock);
    ensure_loaded();
    cJSON *o = cJSON_Parse(json.c_str());
    Sentence s;
    const bool ok = o != nullptr && from_json(o, s);
    cJSON_Delete(o);
    if (!ok) {
        return "";
    }
    bool taken = s.id.empty();
    for (const Sentence &x : list) {
        taken |= x.id == s.id;
    }
    if (taken) {
        s.id = new_id();
    }
    list.push_back(s);
    if (!save()) {
        list.pop_back();
        return "";
    }
    return s.id;
}

bool update(const std::string &json)
{
    std::lock_guard<std::mutex> hold(lock);
    ensure_loaded();
    cJSON *o = cJSON_Parse(json.c_str());
    Sentence s;
    const bool ok = o != nullptr && from_json(o, s);
    cJSON_Delete(o);
    if (!ok) {
        return false;
    }
    for (Sentence &x : list) {
        if (x.id == s.id) {
            x = s;
            return save();
        }
    }
    return false;
}

bool remove(const std::string &id)
{
    std::lock_guard<std::mutex> hold(lock);
    ensure_loaded();
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i].id == id) {
            list.erase(list.begin() + i);
            return save();
        }
    }
    return false;
}

void clear()
{
    std::lock_guard<std::mutex> hold(lock);
    list.clear();
    loaded = true;
    save();
}

}  // namespace sentences
