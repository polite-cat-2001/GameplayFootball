#!/usr/bin/env python3
# Fetches crisp national flags and normalizes them onto the uniform canvas the
# game's team-select UI expects.
#
# The Transfermarkt scraper delivers 12x7px flags, which look blurry in the
# country picker. This tool re-downloads every country flag at high resolution
# (flagcdn.com w1280 for ISO countries, Wikimedia Commons for the four UK
# constituent nations that flagcdn does not carry) and writes
# <country_id>.png at a uniform 512x512 canvas with the flag centered, its own
# proportions preserved, and transparent margins.
#
# The canvas is square because the GUI icon is square: the menu's virtual
# aspect ratio is 5:4 (src/main.cpp:185), so an 8% x 10% icon maps to
# w/h = 0.8 * (5/4) = 1.0.
#
# Usage:  python tools/fetch_flags.py [data_dir] [build_dir]
#   data_dir  (default data/databases/default)  -- also written next to binary
#   build_dir (optional, e.g. build/Release/databases/default)
#
# Requires: Pillow. Network access to flagcdn.com and commons.wikimedia.org.

import argparse
import concurrent.futures
import io
import json
import os
import sqlite3
import time
import urllib.request

CANVAS = 512  # square canvas, matches the square carousel icon
WIDTH = 1280  # source width to download
UA = "GameplayFootball-dev/1.0 (personal project)"

# country id -> source. ("iso", code) = flagcdn alpha-2; ("wm", file) = Wikimedia file.
SOURCES = {
    2: ("iso", "am"), 3: ("iso", "mk"), 4: ("iso", "my"), 5: ("iso", "mt"),
    6: ("iso", "ma"), 7: ("iso", "et"), 8: ("iso", "mx"), 9: ("iso", "md"),
    10: ("iso", "mm"), 11: ("iso", "au"), 12: ("iso", "nz"), 13: ("iso", "ni"),
    14: ("iso", "nl"), 15: ("iso", "ng"), 16: ("iso", "no"), 17: ("iso", "om"),
    18: ("iso", "at"), 19: ("iso", "az"), 20: ("iso", "pa"), 21: ("iso", "py"),
    22: ("iso", "pe"), 23: ("iso", "ph"), 24: ("iso", "pl"), 25: ("iso", "pt"),
    26: ("iso", "qa"), 27: ("iso", "ro"), 28: ("iso", "ru"), 29: ("iso", "sm"),
    30: ("iso", "sa"), 31: ("iso", "se"), 32: ("iso", "ch"), 33: ("iso", "sn"),
    34: ("iso", "sg"), 35: ("iso", "sk"), 36: ("iso", "si"), 37: ("iso", "es"),
    38: ("iso", "za"), 39: ("iso", "tw"), 40: ("iso", "th"), 41: ("iso", "cz"),
    42: ("iso", "tn"), 43: ("iso", "tr"), 44: ("iso", "ug"), 45: ("iso", "ua"),
    46: ("iso", "hu"), 47: ("iso", "uy"), 48: ("iso", "by"), 49: ("iso", "uz"),
    50: ("iso", "ve"), 51: ("iso", "ae"), 52: ("iso", "us"), 53: ("iso", "vn"),
    54: ("iso", "cy"), 55: ("wm", "Flag_of_England.svg"), 56: ("iso", "be"),
    57: ("wm", "Flag_of_Scotland.svg"), 58: ("wm", "Flag_of_Wales_(1959).svg"),
    59: ("wm", "Flag_of_Northern_Ireland_(1953%E2%80%931972).svg"),
    60: ("iso", "eg"), 61: ("iso", "fo"), 62: ("iso", "rs"), 63: ("iso", "me"),
    64: ("iso", "hk"), 65: ("iso", "pr"), 66: ("iso", "bo"), 67: ("iso", "xk"),
    68: ("iso", "br"), 69: ("iso", "gi"), 70: ("iso", "bg"), 71: ("iso", "al"),
    72: ("iso", "cl"), 73: ("iso", "cn"), 74: ("iso", "cr"), 75: ("iso", "hr"),
    76: ("iso", "dk"), 77: ("iso", "dz"), 78: ("iso", "de"), 79: ("iso", "ec"),
    80: ("iso", "ee"), 81: ("iso", "fi"), 82: ("iso", "ad"), 83: ("iso", "fr"),
    84: ("iso", "ge"), 85: ("iso", "gh"), 86: ("iso", "gr"), 87: ("iso", "in"),
    88: ("iso", "id"), 89: ("iso", "iq"), 90: ("iso", "ir"), 91: ("iso", "ie"),
    92: ("iso", "is"), 93: ("iso", "il"), 94: ("iso", "it"), 95: ("iso", "jm"),
    96: ("iso", "jp"), 97: ("iso", "jo"), 98: ("iso", "kh"), 99: ("iso", "ca"),
    100: ("iso", "kz"), 101: ("iso", "co"), 102: ("iso", "ar"), 103: ("iso", "lv"),
    104: ("iso", "lb"), 105: ("iso", "ly"), 106: ("iso", "lt"), 107: ("iso", "lu"),
}


def country_names(db_path):
    con = sqlite3.connect(db_path)
    try:
        rows = con.execute("select id, name from countries")
        return {str(r[0]): r[1] for r in rows}
    finally:
        con.close()


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read()


def source_url(cid):
    kind, code = SOURCES[cid]
    if kind == "iso":
        return "https://flagcdn.com/w%d/%s.png" % (WIDTH, code)
    return "https://commons.wikimedia.org/w/thumb.php?f=%s&width=%d" % (code, WIDTH)


def normalize(raw):
    from PIL import Image
    img = Image.open(io.BytesIO(raw)).convert("RGBA")
    scale = min(CANVAS / img.width, CANVAS / img.height)
    nw, nh = max(1, round(img.width * scale)), max(1, round(img.height * scale))
    img = img.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    canvas.paste(img, ((CANVAS - nw) // 2, (CANVAS - nh) // 2), img)
    out = io.BytesIO()
    canvas.save(out, "PNG", optimize=True)
    return out.getvalue()


def work(cid):
    try:
        return cid, normalize(fetch(source_url(cid)))
    except Exception as exc:  # noqa: BLE001
        return cid, "ERR: %s" % exc


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("data_dir", nargs="?", default="data/databases/default")
    ap.add_argument("build_dir", nargs="?")
    args = ap.parse_args()

    names = country_names(os.path.join(args.data_dir, "database.sqlite"))
    ids = sorted(SOURCES)

    t0 = time.time()
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        for cid, png in pool.map(work, ids):
            results.append((cid, png))

    target_dirs = [os.path.join(args.data_dir, "images_countries")]
    if args.build_dir:
        target_dirs.append(os.path.join(args.build_dir, "images_countries"))

    fails = []
    for cid, png in results:
        if isinstance(png, str):
            fails.append((cid, png))
            continue
        for d in target_dirs:
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, "%d.png" % cid), "wb") as f:
                f.write(png)
        print("ok %3d %-24s %d bytes" % (cid, names.get(str(cid), "?"), len(png)))

    print("fails: %s" % fails)
    print("wrote %d flags to %s in %.1fs"
          % (len(results) - len(fails), target_dirs, time.time() - t0))


if __name__ == "__main__":
    main()