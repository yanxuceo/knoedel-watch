#!/usr/bin/env python3
"""Showcase pages of the decks, for sharing (a post, a talk), not for working on them:

  python3 tools/build_showcase.py            # every deck in content/library.json
  python3 tools/build_showcase.py --out DIR  # default: showcase/

One page per deck, like the web preview (tools/build_preview.py) but in English and
without draft status or animation ids still to implement, plus index.html, which
lists them all. Open a page in the browser; click a card to turn it.
tools/make_release.py puts the same pages on the installer site, under decks/.
"""
import argparse, html, json
from pathlib import Path
from build_preview import render
from deckfmt import ROOT, load_json

KIND = {"knoedel": "Knödel", "katze": "Brezel", "dinge": "the things themselves"}


def kind(deck):
    """What the deck teaches, from its cards."""
    cards = deck["cards"]
    if all(c.get("noun") for c in cards):
        return "nouns"
    if all(c.get("verb") for c in cards):
        return "verbs"
    if deck["character"] == "knoedel" and not any(c["headword"].rstrip().endswith(("!", "?")) for c in cards):
        return "adjectives"
    return "phrases"


def index(decks):
    rows = []
    for deck in decks:
        e = lambda s: html.escape(s or "")
        examples = ", ".join(e((c["noun"]["article"] + " " if c.get("noun") else "") + (c.get("lead", "") + " " if c.get("lead") else "")
                                 + c["headword"]).replace("{{", "").replace("}}", "").replace("|prefix", "").replace("|refl", "")
                             for c in deck["cards"][:3])
        rows.append(f"""<a class="deck" href="{deck['id']}.html">
  <span class="de">{e(deck['title']['de'])}</span>
  <span class="en">{e(deck['title'].get('en'))}</span>
  <span class="meta">{kind(deck)} · {deck['level'].replace('-', '–')} · {len(deck['cards'])} cards</span>
  <span class="ex">{examples} …</span>
</a>""")
    n = sum(len(d["cards"]) for d in decks)
    return f"""<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Knödel Watch decks</title>
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,500;12..96,800&display=swap" rel="stylesheet">
<style>
:root{{--bg:#EEF0F4;--fg:#1A1F2B;--muted:#5E6676;--card:#FFFFFF;--line:#D7DCE4;--accent:#5C63B8}}
@media (prefers-color-scheme: dark){{:root:not([data-theme="light"]){{--bg:#12151B;--fg:#E8EBF0;--muted:#98A1B0;--card:#1B1F27;--line:#2A303B;--accent:#9AA0F0}}}}
:root[data-theme="dark"]{{--bg:#12151B;--fg:#E8EBF0;--muted:#98A1B0;--card:#1B1F27;--line:#2A303B;--accent:#9AA0F0}}
*{{box-sizing:border-box}}
body{{margin:0;background:var(--bg);color:var(--fg);font-family:"Bricolage Grotesque",system-ui,sans-serif}}
.wrap{{max-width:1100px;margin:0 auto;padding:32px 16px 56px}}
h1{{margin:0;font-size:clamp(32px,6vw,52px);font-weight:800;letter-spacing:-.02em}}
p{{color:var(--muted);font-size:16px;line-height:1.6;max-width:720px}}
.grid{{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:14px;margin-top:24px}}
.deck{{display:flex;flex-direction:column;gap:4px;padding:16px 18px;background:var(--card);border:1px solid var(--line);border-radius:14px;color:inherit;text-decoration:none}}
.deck:hover{{border-color:var(--accent)}}
.de{{font-size:22px;font-weight:800}}
.en{{font-size:15px}}
.meta{{font-size:13px;color:var(--muted)}}
.ex{{font-size:13px;color:var(--accent);margin-top:4px}}
footer{{margin-top:32px;color:var(--muted);font-size:14px}}
footer a{{color:inherit}}
</style>
<div class="wrap">
  <h1>Knödel Watch decks</h1>
  <p>{len(decks)} decks, {n} German flashcards at level A2, acted out by Knödel the dumpling and Brezel the cat on a round watch screen. Open a deck to see its cards; click a card to turn it. More decks are on the way.</p>
  <main class="grid">
{chr(10).join(rows)}
  </main>
  <footer>Open source: <a href="https://github.com/yanxuceo/knoedel-watch">github.com/yanxuceo/knoedel-watch</a> · install on a watch: <a href="https://yanxuceo.github.io/knoedel-watch/">yanxuceo.github.io/knoedel-watch</a></footer>
</div>
"""


def build(out, library=ROOT / "content" / "library.json"):
    """Writes the deck pages and index.html to `out`; returns how many decks."""
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    decks = []
    for name in load_json(library)["decks"]:
        deck_dir = ROOT / "content" / name
        decks.append(load_json(deck_dir / "deck.json"))
        (out / f"{name}.html").write_text(render(deck_dir, showcase=True), encoding="utf-8")
    (out / "index.html").write_text(index(decks), encoding="utf-8")
    return len(decks)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--library", default=str(ROOT / "content" / "library.json"))
    ap.add_argument("--out", default=str(ROOT / "showcase"))
    a = ap.parse_args()
    print(f"wrote {build(a.out, a.library)} deck pages and index.html to {a.out}")


if __name__ == "__main__":
    main()
