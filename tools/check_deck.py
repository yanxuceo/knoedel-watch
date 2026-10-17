"""Check a deck before handoff. Run by content authors and by the firmware importer.

  pip install jsonschema
  python3 tools/check_deck.py content/gefuehle                   # author check
  python3 tools/check_deck.py content/gefuehle --require-audio   # importer check
  python3 tools/check_deck.py content/gefuehle --strict          # budgets become errors

Errors (exit code 1): schema, duplicate ids, missing locales, unknown character,
audio missing or out of date (with --require-audio, or whenever audio/ exists).
Warnings: text budgets (see FORMAT.md; the importer measures pixels).
Info: animation ids the firmware has not implemented yet.
"""
import argparse, re, sys
from pathlib import Path
from jsonschema import Draft202012Validator
from deckfmt import ROOT, strip, load_json, spoken_lines, load_registry, missing_ids, card_character

BUDGET = {
    "headword": 12, "headword_noun": 16, "lead": 16, "pattern": 18, "example.de": 85,
    "gloss": {"zh-Hans": 8, "en": 20},
    "translation": {"zh-Hans": 24, "en": 60},
    "tip": {"zh-Hans": 36, "en": 70},
}
DEFAULT = {"gloss": 20, "translation": 60, "tip": 70}
TEMPORARY = {"edge"}  # audio providers for trying cards out only


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("deck", help="content/<deck-id> or its deck.json")
    ap.add_argument("--strict", action="store_true", help="text budgets are errors")
    ap.add_argument("--require-audio", action="store_true", help="audio/ must exist and match")
    a = ap.parse_args()

    path = Path(a.deck)
    deck_dir = path.parent if path.suffix == ".json" else path
    deck = load_json(deck_dir / "deck.json")
    errors, warnings, info = [], [], []

    for e in Draft202012Validator(load_json(ROOT / "deck.schema.json")).iter_errors(deck):
        errors.append(f"schema {'/'.join(map(str, e.path))}: {e.message}")
    if errors:
        return report(errors, warnings, info)

    ids = [c["id"] for c in deck["cards"]]
    for d in sorted({i for i in ids if ids.count(i) > 1}):
        errors.append(f"duplicate card id: {d}")

    registry = load_registry(deck["character"])
    if registry is None:
        errors.append(f"no registry/{deck['character']}.json: the firmware cannot draw this character")

    todo = {}
    for c in deck["cards"]:
        cid = c["id"]

        def budget(name, text, limit):
            n = len(strip(text))
            if n > limit:
                warnings.append(f"{cid}: {name} {n} > {limit}")

        head = strip(c["headword"]) + (f" {c['noun']['article']}" if c.get("noun") else "")
        budget("headword", head, BUDGET["headword_noun"] if c.get("noun") else BUDGET["headword"])
        if c.get("lead"):
            budget("lead", c["lead"], BUDGET["lead"])
        if c.get("pattern"):
            budget("pattern", c["pattern"]["text"], BUDGET["pattern"])
        budget("example.de", c["example"]["de"], BUDGET["example.de"])
        if c.get("pattern"):
            # the collocation on the card must be visible in the example sentence (da-compounds count)
            ex = set(re.findall(r"[a-zäöüß]+", strip(c["example"]["de"]).lower()))
            words = [w for w in c["pattern"]["text"].lower().split() if w not in ("sich", "jemandem", "mir", "dir")]
            unseen = [w for w in words if not ({w, "da" + w, "dar" + w} & ex)]
            if unseen:
                warnings.append(f"{cid}: pattern '{c['pattern']['text']}' is not visible in the example (missing: {', '.join(unseen)})")
        if c["pos"] == "verb" and not c.get("verb"):
            errors.append(f"{cid}: pos is verb but 'verb' forms are missing")
        if c["pos"] == "noun" and not c.get("noun"):
            errors.append(f"{cid}: pos is noun but 'noun' (article) is missing")

        for loc in deck["locales"]:
            for field, src in (("gloss", c["gloss"]), ("translation", c["example"]["translations"]), ("tip", c.get("tip"))):
                if src is None:
                    continue  # tip is optional
                if loc not in src:
                    errors.append(f"{cid}: missing {field}.{loc}")
                    continue
                budget(f"{field}.{loc}", src[loc], BUDGET[field].get(loc, DEFAULT[field]))

        reg = registry if card_character(c, deck) == deck["character"] else load_registry(card_character(c, deck))
        if reg is None:
            errors.append(f"{cid}: no registry/{card_character(c, deck)}.json")
        if reg:
            for kind, x in missing_ids(c, reg):
                todo.setdefault(f"{kind}:{x}", []).append(cid)

    if todo:
        info.append("not implemented in the firmware yet:")
        info += [f"  {k:<24} used by {', '.join(v)}" for k, v in sorted(todo.items())]

    check_audio(deck, deck_dir, a.require_audio, errors, info)
    if a.strict:
        errors += warnings
        warnings = []
    return report(errors, warnings, info, len(deck["cards"]))


def check_audio(deck, deck_dir, required, errors, info):
    audio = deck_dir / "audio"
    manifest_path = audio / "manifest.json"
    if not manifest_path.exists():
        (errors if required else info).append("audio: no audio/manifest.json yet (run tools/make_audio.py)")
        return
    files = load_json(manifest_path).get("files", {})
    stale = 0
    for c in deck["cards"]:
        for n, text in spoken_lines(c).items():
            name = f"{c['id']}_{n}.mp3"
            entry = files.get(name)
            if not (audio / name).exists() or entry is None:
                errors.append(f"audio: {name} missing")
                stale += 1
            elif entry.get("text") != text:
                errors.append(f"audio: {name} is out of date\n    audio: {entry.get('text')}\n    deck:  {text}")
                stale += 1
    if not stale:
        info.append(f"audio: {len(files)} files, all match deck.json")


def report(errors, warnings, info, n_cards=None):
    for line in info:
        print(f"info  {line}")
    for line in warnings:
        print(f"warn  {line}")
    for line in errors:
        print(f"ERROR {line}")
    tail = f"{n_cards} cards, " if n_cards is not None else ""
    print(f"\n{tail}{len(errors)} error(s), {len(warnings)} warning(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
