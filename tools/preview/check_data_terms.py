"""Checks the data_terms tables (src/assets/localization/data/*.json) against the game data.

    python tools/preview/check_data_terms.py [--fix-report out.json]

- every table is valid JSON of kind data_terms; every entry has a non-empty "en" and "ru" == the key;
- English carries no Cyrillic and no guillemets; placeholders ({x}, %s, %d) and the digits of the source survive;
- coverage: the share of the data's distinct Cyrillic strings (tools/preview/collect_data_terms.py) with a translation;
- duplicate keys across tables with DIFFERENT English (inconsistent terminology)."""
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TABLES = os.path.join(ROOT, "src", "assets", "localization", "data")
cyr = re.compile(r"[Ѐ-ӿ]")
ph = re.compile(r"\{[A-Za-z0-9_]+\}|%[sd]")
digits = re.compile(r"\d+")

terms = {}
origin = {}
problems = []
for p in sorted(glob.glob(os.path.join(TABLES, "*.json"))):
    name = os.path.basename(p)
    try:
        j = json.load(open(p, encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        problems.append(f"{name}: BROKEN JSON {e}")
        continue
    if j.get("kind") != "data_terms" or not isinstance(j.get("terms"), dict):
        problems.append(f"{name}: not a data_terms table")
        continue
    for k, v in j["terms"].items():
        en = v.get("en", "") if isinstance(v, dict) else ""
        if not en:
            problems.append(f"{name}: no en for {k[:60]!r}")
            continue
        if v.get("ru") != k:
            problems.append(f"{name}: ru != key for {k[:60]!r}")
        if cyr.search(en) or "«" in en or "»" in en:
            problems.append(f"{name}: Cyrillic / guillemets left in en of {k[:60]!r}: {en[:80]!r}")
        if sorted(ph.findall(k)) != sorted(ph.findall(en)):
            problems.append(f"{name}: placeholders differ in {k[:60]!r}")
        if sorted(digits.findall(k)) != sorted(digits.findall(en)):
            problems.append(f"{name}: numbers differ in {k[:60]!r} -> {en[:60]!r}")
        if k in terms and terms[k] != en:
            problems.append(f"{name}: {k[:50]!r} also in {origin[k]} with another English")
        terms[k] = en
        origin[k] = name

collected = os.path.join(os.environ.get("TEMP", "."), "data_terms_now.json")
subprocess.run([sys.executable, os.path.join(ROOT, "tools", "preview", "collect_data_terms.py"), collected],
               capture_output=True)
data = json.load(open(collected, encoding="utf-8"))
missing = [t for t in data if t not in terms]
print(f"tables: {len(glob.glob(os.path.join(TABLES, '*.json')))}, translated terms: {len(terms)}")
print(f"coverage: {len(data) - len(missing)} of {len(data)} data strings ({100.0 * (len(data) - len(missing)) / max(1, len(data)):.1f} %)")
by_file = {}
for t in missing:
    for f in data[t]:
        by_file[f] = by_file.get(f, 0) + 1
for f, n in sorted(by_file.items(), key=lambda x: -x[1])[:12]:
    print(f"  missing {n:5d}  {f}")
print(f"problems: {len(problems)}")
for line in problems[:80]:
    print("  " + line)
if "--fix-report" in sys.argv:
    out = sys.argv[sys.argv.index("--fix-report") + 1]
    json.dump({"problems": problems, "missing": missing}, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
