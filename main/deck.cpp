#include "deck.h"

#include <string.h>

namespace {

// Layout written by tools/import_decks.py; all fields little-endian.
constexpr uint32_t PACK_VERSION = 3;
constexpr int MAX_POSE = character::MAX_POSE;

struct __attribute__((packed)) RawHeader {
    char magic[4];  // "KDP1"
    uint32_t version, deck_count, card_count, decks_off, cards_off, strings_off, locale;
    uint32_t theme[10];
};
struct __attribute__((packed)) RawDeck {
    uint32_t id, title, title_local, level, first_card, card_count, flags, episode, character;
};
struct __attribute__((packed)) RawCard {
    uint32_t id, lead, headword, gloss, grammar, example, translation, tip, pattern3;
    uint32_t color, background, tint;
    uint8_t grammar_kind, kasus2, kasus3, n_pose, n_motion, n_fx, has_tint, n_prop;
    uint32_t bubble;
    uint32_t motion[character::MAX_LAYERS], fx[character::MAX_LAYERS], prop[character::MAX_LAYERS];
    struct __attribute__((packed)) {
        uint32_t name;
        float value;
    } pose[MAX_POSE];
    Clip clips[4];
    uint32_t character;  // empty: the deck's character
};

const uint8_t *s_pack;
const RawHeader *s_head;
Theme s_theme;

const char *str(uint32_t offset) { return (const char *)s_pack + s_head->strings_off + offset; }
const RawDeck &raw_deck(int deck) { return ((const RawDeck *)(s_pack + s_head->decks_off))[deck]; }

}  // namespace

bool deck_init(const uint8_t *pack, size_t size)
{
    const RawHeader *h = (const RawHeader *)pack;
    if (size < sizeof(RawHeader) || memcmp(h->magic, "KDP1", 4) != 0 || h->version != PACK_VERSION ||
        h->deck_count == 0) {
        return false;
    }
    s_pack = pack;
    s_head = h;
    uint32_t t[10];
    memcpy(t, h->theme, sizeof(t));
    s_theme = {{t[0], t[1], t[2]}, t[3], t[4], {t[5], t[6]}, {t[7], t[8], t[9]}};
    return true;
}

const char *deck_locale() { return str(s_head->locale); }
const Theme &deck_theme() { return s_theme; }
int deck_count() { return s_head ? (int)s_head->deck_count : 0; }

DeckInfo deck_info(int deck)
{
    const RawDeck &d = raw_deck(deck);
    // A pack for a character this firmware does not have is drawn with the first one.
    const int who = character::find(str(d.character));
    return {str(d.id), str(d.title), str(d.title_local), str(d.level), (int)d.card_count, (int)d.episode, who < 0 ? 0 : who};
}

const char *deck_card_id(int deck, int index)
{
    return str(((const RawCard *)(s_pack + s_head->cards_off))[raw_deck(deck).first_card + index].id);
}

Card deck_card(int deck, int index)
{
    const RawCard &r = ((const RawCard *)(s_pack + s_head->cards_off))[raw_deck(deck).first_card + index];
    // A card may be drawn by another character than its deck's (the cat on some noun cards).
    const int own = r.character ? character::find(str(r.character)) : -1;
    const int who = own >= 0 ? own : deck_info(deck).character;
    Card c;
    c.id = str(r.id);
    c.lead = str(r.lead);
    c.headword = str(r.headword);
    c.gloss = str(r.gloss);
    c.grammar = str(r.grammar);
    c.example = str(r.example);
    c.translation = str(r.translation);
    c.pattern3 = str(r.pattern3);
    c.tip = str(r.tip);
    c.grammar_kind = (GrammarKind)r.grammar_kind;
    c.kasus2 = (Kasus)r.kasus2;
    c.kasus3 = (Kasus)r.kasus3;
    c.color = r.color;
    c.background = r.background;

    // Ids the firmware does not know are skipped: the card still reads well
    // without them, and the importer lists them.
    character::anim_reset(c.anim, who);
    for (int i = 0; i < r.n_pose && i < MAX_POSE; i++) {
        character::anim_set_pose(c.anim, str(r.pose[i].name), r.pose[i].value);
    }
    c.anim.has_tint = r.has_tint != 0;
    c.anim.tint = r.tint;
    for (int i = 0; i < r.n_motion; i++) {
        character::anim_add_motion(c.anim, str(r.motion[i]));
    }
    for (int i = 0; i < r.n_fx; i++) {
        character::anim_add_fx(c.anim, str(r.fx[i]));
    }
    for (int i = 0; i < r.n_prop; i++) {
        character::anim_add_prop(c.anim, str(r.prop[i]));
    }
    if (r.bubble) {
        character::anim_set_bubble(c.anim, str(r.bubble));
    }
    memcpy(c.clips, r.clips, sizeof(c.clips));
    return c;
}
