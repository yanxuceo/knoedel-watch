#!/usr/bin/env python3
"""Build a content pack from card decks: check, audio, fit, pack. One command.

  python3 tools/import_decks.py                              # every deck in content/library.json
  python3 tools/import_decks.py content/gefuehle --locale en   # English is the starting language
  python3 tools/import_decks.py --paid                       # also generate the audio that is missing (billed)
  python3 tools/import_decks.py --audio edge                 # free, temporary audio for trying cards out
  python3 tools/import_decks.py --update-registry            # after adding animation ids to the firmware

For each deck (FORMAT.md, "Workflow"):
  1. tools/check_deck.py
  2. audio: generated for new or changed lines, by the billed provider only with --paid
  3. tools/check_deck.py --require-audio
  4. the web preview, content/<deck>/preview.html
Then for all decks together:
  5. registry/<character>.json is compared with what the firmware code implements
  6. every text of every locale is laid out with the real fonts by the firmware's own
     layout code (sim/), and a deck with text that does not fit is refused
  7. packs/<name>.bin is written, the content pack: decks_loc0.bin / decks_loc1.bin (two
     languages, switched in the watch's settings), audio.pak, font_*.vlw. Its files are
     also kept loose in packs/<name>/ for the simulator. tools/flash_content.py writes
     the pack to the watch; the firmware itself does not change.

Needs: pip install jsonschema pillow fonttools edge-tts, and cmake + SDL2 for steps 5 and 6.
"""
import argparse, json, re, shutil, struct, subprocess, sys
from pathlib import Path
import contentpack
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont
from deckfmt import (ROOT, strip, load_json, grammar_line, face3_pattern, spoken_lines, load_registry, missing_ids,
                     card_character, AUX_3SG)

TOOLS = ROOT / "tools"
FONTS = TOOLS / "fonts"
SIM = ROOT / "sim" / "build" / "card_sim"
WORK = ROOT / "sim" / "build" / "import"  # scratch space, next to the checker

LATIN = "".join(chr(c) for c in range(0x20, 0x7F)) + "ÄÖÜäöüßéè„“”’–…·°"
UI_TEXT = "Super! Toll! Klasse! Spitze! Wahnsinn! Prima! Pause! Weiter! Montag Dienstag Mittwoch Donnerstag Freitag Samstag Sonntag Januar Februar März April Mai Juni Juli August September Oktober November Dezember T I P P Karten + Akk Dat Gen Hatschi! 39° puh ! ? Berlin Grüß Gott! Wie geht's? BRRRR € Hütte Gipfel Heute Speck- knödel −5 kg 8 km/h ha"

BRICOLAGE, NOTO = "BricolageGrotesque.ttf", "NotoSansSC.ttf"
# name: (ttf, pixel size, variation axes). "loc" fonts depend on the locale.
DE_FONTS = {
    "de_xl": (BRICOLAGE, 54, {"opsz": 54, "wght": 800, "wdth": 100}),
    "de_l":  (BRICOLAGE, 44, {"opsz": 44, "wght": 800, "wdth": 100}),
    "de_m":  (BRICOLAGE, 28, {"opsz": 28, "wght": 500, "wdth": 100}),
    "de_mb": (BRICOLAGE, 28, {"opsz": 28, "wght": 800, "wdth": 100}),
    "de_s":  (BRICOLAGE, 21, {"opsz": 21, "wght": 700, "wdth": 100}),
    "de_xs": (BRICOLAGE, 13, {"opsz": 13, "wght": 700, "wdth": 100}),
    "clock": (BRICOLAGE, 88, {"opsz": 96, "wght": 800, "wdth": 100}),  # digits of the clock and timer only
}
LOC_FONTS = {
    "cjk":   {"loc_m": (NOTO, 26, {"wght": 500}), "loc_s": (NOTO, 22, {"wght": 400}),
              "loc_l": (NOTO, 38, {"wght": 700})},
    "latin": {"loc_m": (BRICOLAGE, 26, {"opsz": 26, "wght": 500, "wdth": 100}),
              "loc_s": (BRICOLAGE, 22, {"opsz": 22, "wght": 400, "wdth": 100}),
              "loc_l": (BRICOLAGE, 38, {"opsz": 38, "wght": 800, "wdth": 100})},
}
AXIS_ORDER = {"Optical size": "opsz", "Weight": "wght", "Width": "wdth"}
KASUS = {None: 0, "Akk": 1, "Dat": 2, "Gen": 3}
GRAMMAR = {"verb": 1, "noun": 2, "pattern": 3}
MAX_POSE, MAX_LAYERS = 24, 6


def is_cjk_locale(locale):
    return locale.split("-")[0] in ("zh", "ja")


def is_cjk(ch):
    """Same rule as is_cjk() in main/card_ui.cpp: drawn with the locale font."""
    cp = ord(ch)
    return cp >= 0x2E80 or (0x2014 <= cp <= 0x2026 and cp != 0x2022)


def run(args, **kw):
    return subprocess.run([str(a) for a in args], cwd=ROOT, text=True, **kw)


# ---- fonts -------------------------------------------------------------------

def load_font(ttf, size, axes):
    font = ImageFont.truetype(str(FONTS / ttf), size)
    names = [AXIS_ORDER[a["name"].decode()] for a in font.get_variation_axes()]
    font.set_variation_by_axes([axes[n] for n in names])
    return font


def build_vlw(font, size, chars):
    """Processing VLW: 24-byte header, 28 bytes per glyph, then 8-bit alpha bitmaps."""
    ascent, descent = font.getmetrics()
    records, bitmaps = [], []
    for ch in sorted(set(chars)):
        l, t, r, b = font.getbbox(ch, anchor="ls") if ch != " " else (0, 0, 1, 1)
        w, h = max(r - l, 1), max(b - t, 1)
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-l, -t), ch, font=font, fill=255, anchor="ls")
        adv = int(round(font.getlength(ch)))
        if w > 255 or adv > 255:
            sys.exit(f"glyph {ch!r} too wide for VLW at {size}px")
        records.append(struct.pack(">7i", ord(ch), h, w, adv, -t, l, 0))
        bitmaps.append(img.tobytes())
    head = struct.pack(">6i", len(records), 11, size, 0, ascent, descent)
    return head + b"".join(records) + b"".join(bitmaps)


_cmaps = {}


def missing_glyphs(ttf, chars):
    if ttf not in _cmaps:
        _cmaps[ttf] = TTFont(str(FONTS / ttf), lazy=True).getBestCmap()
    return sorted(c for c in set(chars) if ord(c) not in _cmaps[ttf])


# ---- deck pack (layout documented in main/deck.cpp) ------------------------------------

class Strings:
    def __init__(self):
        self.blob, self.seen = bytearray(b"\0"), {"": 0}

    def add(self, s):
        s = s or ""
        if s not in self.seen:
            self.seen[s] = len(self.blob)
            self.blob += s.encode("utf-8") + b"\0"
        return self.seen[s]


def card_fields(card, locale):
    """The strings a card shows, with markup. Articles and auxiliaries become markup roles."""
    head = card["headword"]
    if card.get("noun"):
        art = card["noun"]["article"]
        head = "{{%s|%s}} %s" % (art, art, head)
    kind, grammar = 0, ""
    g = grammar_line(card)
    if g:
        kind = GRAMMAR[g[0]]
        if g[0] == "verb":
            v = card["verb"]
            aux = v["perfekt"]["aux"]
            grammar = "%s · {{%s|%s}} %s" % (v["praeteritum"], AUX_3SG[aux], aux, v["perfekt"]["rest"])
        elif g[0] == "noun":
            # What changes in the plural ({{ä|pl}}) is drawn in the colour of the noun's gender.
            art = card["noun"]["article"]
            grammar = "die " + re.sub(r"\{\{([^}|]*)\|pl\}\}", r"{{\1|%s}}" % art, card["noun"]["plural"])
        else:
            grammar = g[1]
    pattern = card.get("pattern") or {}
    p3 = face3_pattern(card) or ""
    return {
        "lead": card.get("lead", ""), "headword": head, "gloss": card["gloss"][locale],
        "grammar": grammar, "example": card["example"]["de"], "translation": card["example"]["translations"][locale],
        "tip": (card.get("tip") or {}).get(locale, ""), "pattern3": p3,
        "grammar_kind": kind,
        "kasus2": KASUS[pattern.get("case")] if kind == GRAMMAR["pattern"] else 0,
        "kasus3": KASUS[pattern.get("case")] if p3 else 0,
    }


def rgb(hexstr):
    return int(hexstr.lstrip("#"), 16)


def build_pack(decks, locale, clips):
    """decks: [(deck, dir)]. clips: {(deck_id, card_id): [(offset, size) x 4]} or None."""
    theme = load_json(ROOT / "theme.json")["dark"]
    colours = [theme["gender"][k] for k in ("der", "die", "das")] + [theme["prefix"], theme["refl"]]
    colours += [theme["aux"][k] for k in ("haben", "sein")] + [theme["case"][k] for k in ("Akk", "Dat", "Gen")]
    st, deck_recs, card_recs, texts = Strings(), [], [], []
    for deck, _ in decks:
        title_local = deck["title"].get(locale, "")
        texts += [deck["title"]["de"], title_local, deck["level"]]
        deck_recs.append(struct.pack("<9I", st.add(deck["id"]), st.add(deck["title"]["de"]), st.add(title_local),
                                     st.add(deck["level"]), len(card_recs), len(deck["cards"]),
                                     0, deck.get("episode", 0),
                                     st.add(deck["character"])))
        for card in deck["cards"]:
            f, a = card_fields(card, locale), card.get("anim", {})
            pose, motion, fx = list(a.get("pose", {}).items()), a.get("motion", []), a.get("fx", [])
            props = a.get("props", [])
            if len(pose) > MAX_POSE or max(len(motion), len(fx), len(props)) > MAX_LAYERS:
                sys.exit(f"{deck['id']}/{card['id']}: more pose values, motions, effects or props than the firmware holds")
            texts += [f[k] for k in ("lead", "headword", "gloss", "grammar", "example", "translation", "tip", "pattern3")]
            rec = struct.pack("<12I8BI", st.add(card["id"]), st.add(f["lead"]), st.add(f["headword"]), st.add(f["gloss"]),
                              st.add(f["grammar"]), st.add(f["example"]), st.add(f["translation"]), st.add(f["tip"]),
                              st.add(f["pattern3"]), rgb(card["style"]["color"]), rgb(card["style"]["background"]),
                              rgb(a["tint"]) if a.get("tint") else 0,
                              f["grammar_kind"], f["kasus2"], f["kasus3"], len(pose), len(motion), len(fx),
                              1 if a.get("tint") else 0, len(props), st.add(a.get("bubble") or ""))
            rec += b"".join(struct.pack("<I", st.add(m)) for m in motion + [""] * (MAX_LAYERS - len(motion)))
            rec += b"".join(struct.pack("<I", st.add(x)) for x in fx + [""] * (MAX_LAYERS - len(fx)))
            rec += b"".join(struct.pack("<I", st.add(x)) for x in props + [""] * (MAX_LAYERS - len(props)))
            rec += b"".join(struct.pack("<If", st.add(k), v) for k, v in pose) + b"\0" * 8 * (MAX_POSE - len(pose))
            for off, size in (clips or {}).get((deck["id"], card["id"]), [(0, 0)] * 4):
                rec += struct.pack("<2I", off, size)
            rec += struct.pack("<I", st.add(a.get("character") or ""))  # empty: the deck's character
            card_recs.append(rec)
    head_size = 8 * 4 + 10 * 4
    decks_off = head_size
    cards_off = decks_off + sum(map(len, deck_recs))
    strings_off = cards_off + sum(map(len, card_recs))
    head = b"KDP1" + struct.pack("<7I", 4, len(deck_recs), len(card_recs), decks_off, cards_off, strings_off, st.add(locale))
    head += struct.pack("<10I", *map(rgb, colours))
    return head + b"".join(deck_recs) + b"".join(card_recs) + bytes(st.blob), texts


def ui_chars(locale):
    """Characters of the menu text for this locale, from main/ui_strings.h."""
    src = (ROOT / "main" / "ui_strings.h").read_text(encoding="utf-8")
    block = re.search(r"\b%s = \{(.*?)\};" % ("ZH" if locale.startswith("zh") else "EN"), src, re.S)
    if not block:
        sys.exit("main/ui_strings.h: language block not found")
    return set("".join(re.findall(r'"([^"]*)"', block.group(1))))


def write_assets(out, decks, locales, clips=None):
    """Device assets for up to two locales (slots 0 and 1): decks_loc<slot>.bin,
    font_loc<slot>_*.vlw, and the German fonts shared by both. Returns (problems, sizes)."""
    out.mkdir(parents=True, exist_ok=True)
    problems, sizes, de_chars = [], {}, set(LATIN) | set(UI_TEXT)

    def font(name, spec, wanted, label):
        ttf, size, axes = spec
        wanted = {c for c in wanted if c >= " "}  # line breaks of dialogues are not glyphs
        gone = missing_glyphs(ttf, wanted)
        if gone:
            problems.append(f"font {ttf} has no glyph for {' '.join(gone)}  ({label}, {name})")
        data = build_vlw(load_font(ttf, size, axes), size, wanted - set(gone))
        (out / f"{name}.vlw").write_bytes(data)
        sizes[f"{name}.vlw"] = len(data)

    slots = (list(locales) + [locales[0]])[:2]  # one locale only: both slots hold it
    for slot, locale in enumerate(slots):
        pack, texts = build_pack(decks, locale, clips)
        (out / f"decks_loc{slot}.bin").write_bytes(pack)
        sizes[f"decks_loc{slot}.bin"] = len(pack)
        cjk = is_cjk_locale(locale)
        chars = set("".join(strip(t) for t in texts)) | ui_chars(locale)
        german = {c for c in chars if not (cjk and is_cjk(c))}
        de_chars |= german
        # CJK locales draw translations, glosses and menus entirely in the locale font.
        local = set("".join(strip(t) for d, _ in decks for c in d["cards"] for t in (
            c["gloss"][locale], c["example"]["translations"][locale], (c.get("tip") or {}).get(locale, ""))))
        local |= set("".join(d["title"].get(locale, "") for d, _ in decks)) | ui_chars(locale)
        wanted = (local | {c for c in chars if is_cjk(c)} | set("0123456789 /")) if cjk else german | set(LATIN)
        for name, spec in LOC_FONTS["cjk" if cjk else "latin"].items():
            # The large size is for menu titles only (main/ui_strings.h).
            chars_for = ui_chars(locale) if name == "loc_l" else wanted
            font(name.replace("loc", f"font_loc{slot}"), spec, chars_for, f"locale {locale}")
    for name, spec in DE_FONTS.items():
        font(f"font_{name}", spec, set("0123456789:") if name == "clock" else de_chars, "German and menu text")
    return problems, sizes


def pack_audio(decks):
    pak, clips = bytearray(), {}
    for deck, deck_dir in decks:
        for card in deck["cards"]:
            entry = []
            for n in range(4):
                f = deck_dir / "audio" / f"{card['id']}_{n}.mp3"
                if n in spoken_lines(card) and f.exists():
                    blob = f.read_bytes()
                    entry.append((len(pak), len(blob)))
                    pak += blob
                else:
                    entry.append((0, 0))
            clips[(deck["id"], card["id"])] = entry
    return bytes(pak), clips


# ---- steps -------------------------------------------------------------------

def build_sim():
    if shutil.which("cmake") is None:
        return "cmake not found"
    for cmd in (["cmake", "-S", "sim", "-B", "sim/build"], ["cmake", "--build", "sim/build", "-j"]):
        r = run(cmd, capture_output=True)
        if r.returncode:
            return (r.stdout + r.stderr)[-1500:]
    return None


KINDS = ("pose", "motion", "fx", "prop", "bubble")


def firmware_ids():
    """{character: {pose: {name: neutral}, motion: [...], fx, prop, bubble}}, from the code."""
    return json.loads(run([SIM, "ids"], capture_output=True, check=True).stdout)


def update_registry():
    """Rewrite registry/<character>.json from the code. For the firmware side, after adding an id."""
    def rows(xs, per=6):
        if not xs:
            return "[]"
        lines = [", ".join(json.dumps(x) for x in xs[i:i + per]) for i in range(0, len(xs), per)]
        return "[\n    " + ",\n    ".join(lines) + "\n  ]"

    for who, code in firmware_ids().items():
        path = ROOT / "registry" / f"{who}.json"
        note = "firmware: list only what is implemented on the device"
        pose = list(code["pose"].items())
        pose_rows = ",\n    ".join(", ".join(f"{json.dumps(k)}: {json.dumps(v)}" for k, v in pose[i:i + 5]) for i in range(0, len(pose), 5))
        text = ("{\n  \"character\": %s,\n  \"maintained_by\": %s,\n  \"pose\": {\n    %s\n  },\n"
                "  \"motion\": %s,\n  \"fx\": %s,\n  \"prop\": %s,\n  \"bubble\": %s\n}\n") % (
            json.dumps(who), json.dumps(note), pose_rows,
            rows(code["motion"]), rows(code["fx"]), rows(code["prop"]), rows(code["bubble"]))
        json.loads(text)
        path.write_text(text, encoding="utf-8")
        print(f"registry/{who}.json rewritten: " + ", ".join(f"{len(code[k])} {k}" for k in KINDS))


def check_registry(decks):
    """registry/<character>.json must list exactly what the code implements."""
    problems, notes, ids = [], [], firmware_ids()
    for who, code in ids.items():
        reg = load_registry(who)
        if reg is None:
            problems.append(f"registry/{who}.json is missing (run with --update-registry)")
            continue
        for kind in KINDS:
            listed, done = set(reg.get(kind, [])), set(code[kind])
            for x in sorted(listed - done):
                problems.append(f"registry/{who}.json lists {kind}:{x}, which the firmware does not implement")
            for x in sorted(done - listed):
                problems.append(f"the firmware implements {kind}:{x}, which registry/{who}.json does not list")
        for x, v in code["pose"].items():
            if x in reg["pose"] and abs(reg["pose"][x] - v) > 1e-6:
                problems.append(f"registry/{who}.json: neutral value of pose:{x} is {reg['pose'][x]}, the firmware uses {v}")
    for deck, _ in decks:
        todo = {}
        for card in deck["cards"]:
            who = card_character(card, deck)
            reg = load_registry(who)
            if reg is None or who not in ids:
                problems.append(f"{deck['id']}/{card['id']}: the firmware has no character '{who}'")
                continue
            for kind, x in missing_ids(card, reg):
                todo.setdefault(f"{kind}:{x}", []).append(card["id"])
        for k, v in sorted(todo.items()):
            notes.append(f"{deck['id']}: {k} is not implemented yet (used by {', '.join(v)}); the card plays without it")
    return problems, notes


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("decks", nargs="*", help="content/<deck-id> folders; default: content/library.json")
    ap.add_argument("--locale", default="en",
                    help="language the watch starts in; a second one is built in too and chosen in the settings")
    ap.add_argument("--paid", action="store_true", help="allow audio generation by a billed provider")
    ap.add_argument("--audio", metavar="PROVIDER", help="audio provider for every deck (see tools/make_audio.py)")
    ap.add_argument("--no-audio", action="store_true", help="do not generate audio; missing clips stay silent")
    ap.add_argument("--update-registry", action="store_true",
                    help="firmware side: rewrite registry/<character>.json from what the code implements")
    ap.add_argument("--skip-fit", action="store_true", help="skip the registry and pixel-fit checks (no cmake/SDL2)")
    ap.add_argument("--library", default=str(ROOT / "content" / "library.json"),
                    help="which decks make up the pack, and its name")
    ap.add_argument("--name", help="name of the pack; default: from the library, or 'custom' for decks given by hand")
    ap.add_argument("--out", default=str(ROOT / "packs"), help="folder the pack is written to")
    a = ap.parse_args()
    sys.stdout.reconfigure(line_buffering=True)  # keep our lines in order with the tools we run

    library = load_json(a.library)
    dirs = [Path(d).resolve() for d in a.decks] or [ROOT / "content" / d for d in library["decks"]]
    pack_name = a.name or ("custom" if a.decks else library["pack"])
    decks, failed = [], False
    for d in dirs:
        deck = load_json(d / "deck.json")
        name = deck["id"]
        print(f"\n== {name}  (version {deck['version']}, {len(deck['cards'])} cards)")
        if d.name != name:
            sys.exit(f"folder {d.name} holds deck id '{name}': they must match")
        if a.locale not in deck["locales"]:
            sys.exit(f"{name} has no locale {a.locale} (it has {', '.join(deck['locales'])})")
        # Audio is made in the next step, so its errors do not count yet.
        lines = run([sys.executable, TOOLS / "check_deck.py", d], capture_output=True).stdout.splitlines()
        lines = [l for l in lines if not l.startswith(("ERROR audio:", "    audio:", "    deck:", "info  audio:"))][:-2]
        print("\n".join(lines))
        if any(l.startswith("ERROR") for l in lines):
            sys.exit(f"{name}: fix the errors above first")
        if not a.no_audio:
            cmd = [sys.executable, TOOLS / "make_audio.py", d] + (["--paid"] if a.paid else [])
            cmd += ["--provider", a.audio] if a.audio else []
            if run(cmd).returncode:
                sys.exit(f"{name}: audio generation failed")
            r = run([sys.executable, TOOLS / "check_deck.py", d, "--require-audio"], capture_output=True)
            if r.returncode:
                print("\n".join(l for l in r.stdout.splitlines() if l.startswith(("ERROR", "    "))))
                sys.exit(f"{name}: audio is missing or out of date" + ("" if a.paid or a.audio else
                         ". Run again with --paid to generate it (billed), or with --audio edge for temporary audio."))
        run([sys.executable, TOOLS / "build_preview.py", d], capture_output=True)
        decks.append((deck, d))

    print("\n== firmware")
    problems = []
    # Every locale all decks have is measured; the firmware carries two of them.
    locales = sorted(set.intersection(*(set(d["locales"]) for d, _ in decks)), key=lambda l: (l != a.locale, l))
    built = locales[:2]
    if a.skip_fit:
        print("skipped: registry and pixel-fit checks")
    else:
        err = build_sim()
        if err:
            sys.exit(f"cannot build the layout checker (sim/):\n{err}\nInstall cmake and SDL2, or pass --skip-fit.")
        if a.update_registry:
            update_registry()
        reg_problems, notes = check_registry(decks)
        problems += reg_problems
        for n in notes:
            print(f"note  {n}")
        # Every locale is measured, not only the one being built.
        for i in range(0, len(locales), 2):
          pair = locales[i:i + 2]
          problems += write_assets(WORK, decks, pair)[0]
          for slot, loc in enumerate(pair):
            r = run([SIM, "check", WORK, slot], capture_output=True)
            misfits = [l for l in r.stdout.splitlines() if l.startswith("MISFIT")]
            problems += [f"text does not fit the screen ({loc}): {l[7:]}" for l in misfits]
            if r.returncode not in (0, 1):
                problems.append(f"layout check failed for {loc}: {r.stderr.strip()}")
            print(f"fit   {loc}: {'ok' if not misfits else f'{len(misfits)} face(s) do not fit'}")
    if problems:
        for p in problems:
            print(f"ERROR {p}")
        sys.exit(f"\n{len(problems)} problem(s): nothing written to {a.out}")

    out = Path(a.out) / pack_name
    pak, clips = pack_audio(decks)
    glyph_problems, sizes = write_assets(out, decks, built, clips)
    for p in glyph_problems:
        print(f"ERROR {p}")
    (out / "audio.pak").write_bytes(pak)
    sizes["audio.pak"] = len(pak)
    for name, size in sorted(sizes.items()):
        print(f"  {name:<16} {size / 1024:8.1f} KB")
    if glyph_problems:
        sys.exit(1)
    pack = contentpack.build(pack_name, {name: (out / name).read_bytes() for name in sorted(sizes)})
    room = contentpack.partition()[1]
    n_clips = sum(1 for c in clips.values() for _, size in c if size)
    print(f"  {'pack':<16} {len(pack) / 1024:8.1f} KB of {room // 1024} KB   ({len(decks)} decks, {n_clips} clips, languages {' + '.join(built)})")
    if len(pack) > room:
        sys.exit("ERROR the pack does not fit the content partition: take decks out of the library")
    pack_path = out.with_suffix(".bin")
    pack_path.write_bytes(pack)
    shown = pack_path.relative_to(ROOT) if pack_path.is_relative_to(ROOT) else pack_path
    print(f"\ncontent pack \"{pack_name}\" written to {shown}. Put it on the watch with:\n  python3 tools/flash_content.py {shown}")


if __name__ == "__main__":
    main()
