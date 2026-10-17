"""Build the web preview of a deck: content/<deck>/preview.html

  python3 tools/build_preview.py content/a2-gesundheit-zustaende

The preview simulates the round screen (tap to flip faces) and is the drawing
reference for any animation id the firmware has not implemented yet.
It is generated from deck.json, so text is never edited in the preview.
"""
import json, sys
from pathlib import Path
from deckfmt import ROOT, load_json, load_registry

TEMPLATES = {"knoedel": ("player.html", ["knoedel.js"]), "katze": ("player.html", ["katze.js"]),
             "dinge": ("player.html", ["katze.js", "dinge.js"])}  # dinge cards may be drawn by the cat


def render(deck_dir, showcase=False):
    """The preview page of a deck, as HTML. `showcase`: the version for people who
    are not working on the deck (tools/build_showcase.py)."""
    deck = load_json(Path(deck_dir) / "deck.json")
    if deck["character"] not in TEMPLATES:
        sys.exit(f"no preview template for character '{deck['character']}' yet")
    page, libs = TEMPLATES[deck["character"]]
    chars = {deck["character"]} | {c["anim"]["character"] for c in deck["cards"] if c.get("anim", {}).get("character")}
    pv = ROOT / "tools" / "preview"
    html = (pv / page).read_text(encoding="utf-8")
    dump = lambda o: json.dumps(o, ensure_ascii=False)
    html = (html.replace("/*__TITLE__*/", deck["title"]["de"])
                .replace("/*__DECK__*/", dump(deck))
                .replace("/*__THEME__*/", dump(load_json(ROOT / "theme.json")))
                .replace("/*__REGISTRY__*/", dump({ch: load_registry(ch) for ch in chars}))
                .replace("/*__CHARACTER__*/", "\n".join((pv / lib).read_text(encoding="utf-8") for lib in libs)))
    if showcase:
        html = html.replace("false /*__SHOWCASE__*/", "true")
    return html


def main(deck_dir):
    deck_dir = Path(deck_dir)
    html = render(deck_dir)
    out = deck_dir / "preview.html"
    out.write_text(html, encoding="utf-8")
    print(f"wrote {out} ({len(html) // 1024} KB)")


if __name__ == "__main__":
    main(sys.argv[1])
