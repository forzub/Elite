"""Localization completeness report (audit #7 D7.8 / D7.9).

    python tools/preview/localization_report.py [--strict]

Reads every ui_strings / catalog table under src/assets/localization and the C++ sources, and reports:
  1. per supported language: keys translated / missing (ui strings + catalog entries);
  2. placeholder mismatches: a translation whose {names} differ from the English template's;
  3. keys the code asks for (tr("k"...), .text("k"...), .format("k"...), Message("k")) that no table has;
  4. Cyrillic string literals left in the C++ (tools/preview/find_cyrillic_literals.py), logs apart.
--strict: exit 1 when (2) or (3) is non-empty or English misses a key."""
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOC = os.path.join(ROOT, "src", "assets", "localization")
strict = "--strict" in sys.argv

langs = json.load(open(os.path.join(LOC, "languages.json"), encoding="utf-8"))
order = langs.get("locale_order", ["en"])

strings = {}     # key -> {locale: text}
sources = {}
for p in glob.glob(os.path.join(LOC, "**", "*.json"), recursive=True):
    try:
        j = json.load(open(p, encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        print(f"BROKEN JSON {p}: {e}")
        continue
    kind = j.get("kind")
    if kind == "ui_strings":
        for k, v in j.get("strings", {}).items():
            if k in strings and sources.get(k) != p:
                print(f"DUPLICATE key {k}: {os.path.relpath(sources[k], ROOT)} and {os.path.relpath(p, ROOT)}")
            strings[k] = v
            sources[k] = p
    elif kind == "catalog":
        dom = j.get("domain", "?")
        for k, v in j.get("entries", {}).items():
            strings[f"catalog:{dom}:{k}"] = v.get("names", v) if isinstance(v, dict) else {}

ph = re.compile(r"\{([A-Za-z0-9_]+)\}")
print(f"== {len(strings)} keys, languages {order}")
missing_en = 0
for loc in order:
    have = sum(1 for v in strings.values() if isinstance(v, dict) and v.get(loc))
    miss = len(strings) - have
    if loc == "en":
        missing_en = miss
    print(f"  {loc:8s} {have:6d} translated, {miss:6d} missing ({100.0 * have / max(1, len(strings)):.1f} %)")

mism = []
for k, v in strings.items():
    if not isinstance(v, dict) or "en" not in v:
        continue
    want = set(ph.findall(v["en"]))
    for loc, t in v.items():
        if loc != "en" and isinstance(t, str) and set(ph.findall(t)) != want:
            mism.append((k, loc, sorted(want), sorted(set(ph.findall(t)))))
print(f"== placeholder mismatches: {len(mism)}")
for k, loc, a, b in mism[:40]:
    print(f"  {k} [{loc}]: en {a} vs {b}")

use = re.compile(r'(?:\btr|\.text|\.format|\bMessage)\(\s*"([a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+)"')
used = {}
for p in glob.glob(os.path.join(ROOT, "src", "**", "*.*"), recursive=True):
    if not p.endswith((".cpp", ".h", ".hpp")):
        continue
    try:
        text = open(p, encoding="utf-8").read()
    except UnicodeDecodeError:
        continue
    for m in use.finditer(text):
        used.setdefault(m.group(1), os.path.relpath(p, ROOT))
unknown = sorted(k for k in used if k not in strings)
print(f"== keys used in code: {len(used)}, missing from the tables: {len(unknown)}")
for k in unknown[:60]:
    print(f"  {k}  ({used[k]})")

r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "preview", "find_cyrillic_literals.py"),
                    os.path.join(ROOT, "src"), "--summary"], capture_output=True, text=True, encoding="utf-8",
                   errors="replace")
print("== Cyrillic literals in C++: " + (r.stdout.strip().splitlines() or ["?"])[-1])
sys.exit(1 if strict and (mism or unknown or missing_en) else 0)
