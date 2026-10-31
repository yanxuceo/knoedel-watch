#pragma once
// The sentence book (main menu > Sentences): sentences the learner collects
// (from a podcast, an article) and adds on the settings site, not built into a
// content pack. Each has the meaning (Chinese and/or English), the German, the
// pattern it shows, and a topic. Kept as one JSON file in the notes partition,
// /notes/sentences.json, written whole (a temporary file, then a rename).

#include <string>
#include <vector>

namespace sentences {

struct Sentence {
    std::string id;  // when it was added, "20261031-161342", or the id it came with
    std::string zh, en, de, pattern, topic;
};

// The sentences, in the order they were added (read from the file once, then
// kept): a copy, as the settings site may change them meanwhile over USB.
std::vector<Sentence> all();

// Adds one from its JSON ({"zh", "en", "de", "pattern", "topic"}, "de" and one
// of "zh"/"en" needed); an "id" given is kept (an import). The id, or "".
std::string add(const std::string &json);
// Changes the one with the JSON's "id". False when there is none.
bool update(const std::string &json);
bool remove(const std::string &id);
void clear();

// One sentence as JSON, as add() takes it.
std::string to_json(const Sentence &s);

}  // namespace sentences
