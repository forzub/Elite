"""Collect the Cyrillic string values of the game data (src/assets/data/**/*.json) - the names / descriptions the
client shows as they come from data (polities, goods, ships, manufacturers, sellers, fiscal styles, hubs ...).

    python tools/preview/collect_data_terms.py [out.json] [--summary]

Writes {"term": ["file", ...]} for every distinct Cyrillic string (keys whose name suggests an id are skipped)."""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA = os.path.join(ROOT, "src", "assets", "data")
cyr = re.compile(r"[Ѐ-ӿ]")
skip_keys = re.compile(r"(^|_)(id|key|code|note|notes|comment|comments|todo|source|author|desc_dev|dev)$", re.I)
terms = {}


def walk(o, path, key=""):
    if isinstance(o, dict):
        for k, v in o.items():
            walk(v, path, k)
    elif isinstance(o, list):
        for v in o:
            walk(v, path, key)
    elif isinstance(o, str) and cyr.search(o) and not skip_keys.search(key or ""):
        terms.setdefault(o, set()).add(os.path.relpath(path, DATA))


for dirpath, _, files in os.walk(DATA):
    for f in files:
        if f.endswith(".json"):
            p = os.path.join(dirpath, f)
            try:
                walk(json.load(open(p, encoding="utf-8")), p)
            except Exception:
                pass
out = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else None
by_file = {}
for t, fs in terms.items():
    for f in fs:
        by_file[f] = by_file.get(f, 0) + 1
if "--summary" in sys.argv or not out:
    for f, n in sorted(by_file.items(), key=lambda x: -x[1])[:40]:
        print(f"{n:6d}  {f}")
    print("distinct terms:", len(terms), " total chars:", sum(len(t) for t in terms))
if out:
    json.dump({t: sorted(fs) for t, fs in sorted(terms.items())}, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
