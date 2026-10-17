"""Shared rules of the deck format. Used by check_deck.py, make_audio.py, the firmware
importer and the web preview, so markup, grammar lines and spoken text are defined once."""
import json, re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MARK = re.compile(r"\{\{([^}|]*)(\|[a-z]+)?\}\}")
AUX_3SG = {"haben": "hat", "sein": "ist"}


def strip(text):
    """Remove {{markup}} and keep the visible text."""
    return MARK.sub(r"\1", text)


def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def grammar_line(card):
    """The grammar line on face 2: ("verb" | "noun" | "pattern", plain text) or None.
    verb    -> 'stand auf · ist aufgestanden'
    noun    -> 'die Wolken'  (None if the noun has no plural)
    other   -> pattern text  (None if no pattern)"""
    if card.get("verb"):
        v = card["verb"]
        return "verb", f"{strip(v['praeteritum'])} · {AUX_3SG[v['perfekt']['aux']]} {strip(v['perfekt']['rest'])}"
    if card.get("noun"):
        plural = card["noun"].get("plural")
        return ("noun", f"die {strip(plural)}") if plural else None
    if card.get("pattern"):
        return "pattern", card["pattern"]["text"]
    return None


def face3_pattern(card):
    """Pattern shown on face 3: only for verbs and nouns, whose face 2 shows their forms."""
    if (card.get("verb") or card.get("noun")) and card.get("pattern"):
        return card["pattern"]["text"]
    return None


def spoken_lines(card):
    """{index: text} for the audio files <card-id>_<index>.mp3. See FORMAT.md, Audio.
    _0 headword, _1 the face-2 grammar line, _2 example, _3 the face-3 pattern."""
    head = strip(card["headword"])
    if card.get("noun"):
        head = f"{card['noun']['article']} {head}"
    if card.get("lead"):
        head = f"{card['lead']} {head}"
    lines = {0: head if head[-1:] in ".!?" else head + "."}
    g = grammar_line(card)
    if g:
        kind, text = g
        lines[1] = (text.replace(" · ", ". ") if kind == "verb" else text) + "."
    lines[2] = strip(card["example"]["de"]).replace("\n", " ")  # dialogue line breaks are not spoken
    p3 = face3_pattern(card)
    if p3:
        lines[3] = p3 + "."
    return lines


def card_character(card, deck):
    """The character that draws this card: anim.character, else the deck's."""
    return card.get("anim", {}).get("character") or deck["character"]


def load_registry(character):
    path = ROOT / "registry" / f"{character}.json"
    return load_json(path) if path.exists() else None


def missing_ids(card, registry):
    """Ids/params this card uses that the registry does not list: [(kind, id)]."""
    a = card.get("anim", {})
    out = [("pose", k) for k in a.get("pose", {}) if k not in registry["pose"]]
    out += [("motion", m) for m in a.get("motion", []) if m not in registry["motion"]]
    out += [("fx", f) for f in a.get("fx", []) if f not in registry["fx"]]
    out += [("prop", x) for x in a.get("props", []) if x not in registry.get("prop", [])]
    if a.get("bubble") and a["bubble"] not in registry["bubble"]:
        out.append(("bubble", a["bubble"]))
    return out
