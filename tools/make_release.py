#!/usr/bin/env python3
"""Make a release: the web installer and a single image for esptool, for every board.

It packages what is built; build first, each step in its own environment:

  idf.py build                       # the M5Stack StopWatch (ESP-IDF)
  idf.py -B build-waveshare_175c -D SDKCONFIG=sdkconfig.waveshare_175c \\
         -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.waveshare_175c" build
  python3 tools/import_decks.py      # the content pack, the same for every board
  python3 tools/make_release.py      # this; needs esptool (pip install esptool, or ESP-IDF's Python)

It writes:
  release/site/      a static web page with ESP Web Tools: open it in Chrome or Edge,
                     choose the board, plug it in, press Install. Publish it with GitHub
                     Pages (or any HTTPS host); the firmware files sit next to the page.
                     site/decks/ shows the pack's cards (tools/build_showcase.py).
  release/knoedel-watch-<version>-installer.zip
                     the same page to run offline: unzip, python3 -m http.server -d <folder>,
                     open http://localhost:8000 in Chrome or Edge.
  release/knoedel-watch-<version>-<board>-full.bin
                     everything in one image, for a fresh install with esptool:
                       python3 -m esptool --chip esp32s3 write-flash 0x0 <file>
                     It erases what the watch remembers (progress, settings).

The version comes from PROJECT_VER in the top CMakeLists.txt.
"""
import argparse, importlib.util, json, os, re, shutil, subprocess, sys
from pathlib import Path
import build_showcase, contentpack

ROOT = contentpack.ROOT
OUT = ROOT / "release"

# Every board of main/Kconfig.projbuild: its build folder, and its name on the page.
BOARDS = {
    "m5_stopwatch": {"build": ROOT / "build", "name": "M5Stack StopWatch"},
    "waveshare_175c": {"build": ROOT / "build-waveshare_175c", "name": "Waveshare ESP32-S3-Touch-AMOLED-1.75C"},
}


def run(cmd):
    print("$", " ".join(str(c) for c in cmd))
    if subprocess.run(cmd, cwd=ROOT).returncode:
        sys.exit(f"failed: {cmd[1] if len(cmd) > 1 else cmd[0]}")


def firmware(board, version):
    """The parts of a board's build with their offsets, and its flash size."""
    build = BOARDS[board]["build"]
    if not (build / "flash_args").exists():
        sys.exit(f"no build for {board} in {build.relative_to(ROOT)}: build it first (see above)")
    # A build in the wrong folder would put one board's firmware on another.
    config = json.loads((build / "config" / "sdkconfig.json").read_text())
    if not config.get(f"KNOEDEL_BOARD_{board.upper()}"):
        sys.exit(f"{build.relative_to(ROOT)} is not a build for {board}")
    built = json.loads((build / "project_description.json").read_text())["project_version"]
    if built != version:
        sys.exit(f"{build.relative_to(ROOT)} holds version {built}, not {version}: build it again")
    lines = (build / "flash_args").read_text().splitlines()
    settings = lines[0].replace("_", "-").split()  # --flash-mode dio --flash-freq 80m --flash-size 16MB
    parts = []
    for line in lines[1:]:
        offset, path = line.split()
        parts.append((int(offset, 16), build / path))
    return parts, settings[settings.index("--flash-size") + 1]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--library", default=str(ROOT / "content" / "library.json"), help="which content pack to ship")
    a = ap.parse_args()

    version = re.search(r'set\(PROJECT_VER "([^"]+)"\)', (ROOT / "CMakeLists.txt").read_text()).group(1)
    pack_name = json.loads(Path(a.library).read_text())["pack"]
    builds = {board: firmware(board, version) for board in BOARDS}
    # esptool from this Python, or else from ESP-IDF's.
    esptool_python = sys.executable
    if importlib.util.find_spec("esptool") is None:
        idf_python = Path(os.environ.get("IDF_PYTHON_ENV_PATH", "")) / "bin" / "python"
        if not idf_python.exists():
            sys.exit("esptool is missing: pip install esptool")
        esptool_python = str(idf_python)

    pack = ROOT / "packs" / f"{pack_name}.bin"
    if not pack.exists():
        sys.exit(f"{pack} does not exist: run tools/import_decks.py")
    contentpack.read(pack.read_bytes())  # refuses a damaged pack
    pack_offset = contentpack.partition()[0]

    if OUT.exists():
        shutil.rmtree(OUT)
    site = OUT / "site"
    (site / "firmware").mkdir(parents=True)
    # One content pack for all boards.
    pack_file = f"firmware/content-{pack_name}.bin"
    shutil.copy(pack, site / pack_file)
    summary = []
    for board, (parts, flash_size) in builds.items():
        (site / "firmware" / board).mkdir()
        manifest_parts = []
        for offset, path in parts:
            shutil.copy(path, site / "firmware" / board / path.name)
            manifest_parts.append({"path": f"firmware/{board}/{path.name}", "offset": offset})
        manifest_parts.append({"path": pack_file, "offset": pack_offset})
        manifest = {
            "name": f"Knödel Watch for {BOARDS[board]['name']}",
            "version": version,
            # Asked at every install: erase for a fresh start, keep to update without
            # losing progress and settings.
            "new_install_prompt_erase": True,
            "builds": [{"chipFamily": "ESP32-S3", "parts": manifest_parts}],
        }
        (site / f"manifest-{board}.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                                                     encoding="utf-8")
        full = OUT / f"knoedel-watch-{version}-{board}-full.bin"
        cmd = [esptool_python, "-m", "esptool", "--chip", "esp32s3", "merge-bin", "-o", full,
               "--flash-mode", "dio", "--flash-size", flash_size]
        for offset, path in parts + [(pack_offset, pack)]:
            cmd += [hex(offset), path]
        run(cmd)
        summary.append(f"  {full.name}  {full.stat().st_size // 1024} KB")

    page = (ROOT / "web" / "index.html").read_text(encoding="utf-8")
    (site / "index.html").write_text(page.replace("{{VERSION}}", version).replace("{{PACK}}", pack_name), encoding="utf-8")
    if (ROOT / "web" / "img").exists():
        shutil.copytree(ROOT / "web" / "img", site / "img")
    # The cards of the pack, to browse before installing (built from the same library).
    n_decks = build_showcase.build(site / "decks", Path(a.library))

    # The installer page as a zip, for installing without a network.
    installer = OUT / f"knoedel-watch-{version}-installer"
    shutil.make_archive(str(installer), "zip", root_dir=OUT, base_dir="site")

    size = sum(p.stat().st_size for p in (site / "firmware").rglob("*") if p.is_file())
    print(f"\nrelease {version} in {OUT.relative_to(ROOT)}/:")
    print(f"  site/            web installer for {len(builds)} boards, {size // 1024} KB; decks/: {n_decks} deck pages")
    print("\n".join(summary))
    print(f"  {installer.name}.zip  {(OUT / (installer.name + '.zip')).stat().st_size // 1024} KB")
    print("Try the installer locally: python3 -m http.server -d release/site 8000, then open http://localhost:8000"
          " (Web Serial works on localhost without HTTPS).")


if __name__ == "__main__":
    main()
