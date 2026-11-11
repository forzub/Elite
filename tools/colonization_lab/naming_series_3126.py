"""Naming series 3126: the names the discoverers / first settlers gave to the stars and bodies (PROPOSAL,
author requests 2026-11-11). One series per system, in the language and myth of the people who named it,
the way Sol has Roman gods and Fortuna has the circle of Fortune (CANON 2026-11-06):
  * inhabited systems - their settlers (naming_series/inhabited_*.txt);
  * the systems nobody lives in - their first visitors from expeditions_3126.py (naming_series/explored_*.txt);
  * Fortuna - CANON, only the es / zh-Hans / ja forms are added (naming_series/fortuna_locales.txt).

Every name exists in every game locale (en, ru, zh-Hans, es, ja): translated where the name means
something (birds, minerals, virtues), transliterated where it is a proper name; some languages call a
body something else entirely (Fortuna is 命运之轮 / 運命の輪 «Wheel of Fate»; HD 102195 is 月门 «Moon
Gate» in Chinese and «World's Ass» for everybody else).

Data: naming_series/*.txt (format in inhabited_1.txt). `python naming_series_3126.py --write` merges them
into canon/naming.json (status PROPOSAL; a CANON entry only gains missing locale forms); then
`python naming.py --write` mirrors into the runtime table names_3126.json.
Captured moonlets below 1e-5 Earth masses keep their catalogue designation.
"""

import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
NAMING = os.path.join(HERE, "canon", "naming.json")
SERIES_DIR = os.path.join(HERE, "naming_series")
EXPEDITIONS = os.path.join(HERE, "out", "world_3126", "expeditions_3126.json")
SYSTEMS = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "systems")
MIN_MASS_EARTH = 1e-5
DATE = "2026-11-11"
LOCALES = ("ru", "en", "es", "zh-Hans", "ja")
KIND = {"planet": "planet", "moon": "moon", "dwarf_planet": "dwarf_planet"}


def system_dir(sid):
    hits = glob.glob(os.path.join(SYSTEMS, f"{sid:03d}_*"))
    return hits[0] if hits else None


def bodies_of(sid):
    out = {}
    for p in glob.glob(os.path.join(system_dir(sid), "generated", "bodies", "*.json")):
        with open(p, encoding="utf-8") as f:
            b = json.load(f)
        out[b["designation"]] = b
        out["id:" + b["id"]] = b
    return out


def parse():
    """-> {sid: dict(lang, named_by, theme, star=row|None, rows=[(designation, row)])}, problems"""
    series, bad = {}, []
    for path in sorted(glob.glob(os.path.join(SERIES_DIR, "*.txt"))):
        cur = None
        for n, line in enumerate(open(path, encoding="utf-8"), 1):
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            f = [x.strip() for x in line.split("|")]
            where = f"{os.path.basename(path)}:{n}"
            if f[0].startswith("@"):
                sid = int(f[0][1:])
                if sid in series:
                    bad.append(f"{where}: system {sid} twice")
                cur = series[sid] = dict(lang=f[1], named_by=f[2], theme=f[3], star=None, rows=[])
                continue
            if cur is None or len(f) < 8:
                bad.append(f"{where}: bad row")
                continue
            native, ru, en, es, zh, ja, meaning = f[1:8]
            row = dict(native=native, ru=native if ru == "=" else ru, en=en, es=en if es == "=" else es,
                       zh=native if zh == "=" else zh, ja=native if ja == "=" else ja, meaning=meaning,
                       translit=f[8] if len(f) > 8 and f[8] else None)
            if f[0] == "*":
                cur["star"] = row
            else:
                cur["rows"].append((f[0], row))
    return series, bad


def exonyms(row):
    return {"ru": row["ru"], "en": row["en"], "es": row["es"], "zh-Hans": row["zh"], "ja": row["ja"]}


def build():
    """-> (entities to set, problems)."""
    series, bad = parse()
    ents = {}
    for sid, ser in sorted(series.items()):
        bodies = bodies_of(sid)
        seen = {lg: {} for lg in LOCALES}
        covered = set()
        for des, row in ser["rows"]:
            b = bodies.get(des)
            if not b:
                bad.append(f"system {sid}: no body '{des}'")
                continue
            covered.add(b["id"])
            for lg, v in exonyms(row).items():
                if not v:
                    bad.append(f"system {sid}: '{des}' has no {lg}")
                elif v in seen[lg] and seen[lg][v] != des:
                    bad.append(f"system {sid}: duplicate {lg} '{v}' ({seen[lg][v]}, {des})")
                seen[lg].setdefault(v, des)
            intl = dict(text=row["native"], lang=ser["lang"])
            if row["translit"]:
                intl["translit"] = row["translit"]
            e = dict(type=KIND.get(b["kind"], b["kind"]), system=f"system:{sid}", status="PROPOSAL")
            if not des.startswith("id:"):
                e["catalog"] = b["designation"]
            e["names"] = dict(international=intl, exonyms=exonyms(row))
            e["naming_history"] = dict(status="PROPOSAL", named_by=ser["named_by"],
                                       origin=f"серия системы ({DATE}): {ser['theme']}. {row['meaning']}")
            ents["body:" + b["id"]] = e
        for b in {v["id"]: v for v in bodies.values()}.values():
            if b["kind"] in KIND and b["physical"]["mass_earth"] >= MIN_MASS_EARTH and b["id"] not in covered:
                bad.append(f"system {sid}: unnamed body '{b['designation']}'")
        star = ser["star"]
        ent = dict(named_by=ser["named_by"], theme=ser["theme"])
        if star:
            loc = dict(text=star["native"], lang=ser["lang"])
            if star["translit"]:
                loc["translit"] = star["translit"]
            ent.update(local=loc, exonyms=exonyms(star), meaning=star["meaning"])
        ents[f"system:{sid}"] = ent
    # every system of the atlas is named (17 Cygni has no bodies: the star only); Sol keeps its own names
    for p in glob.glob(os.path.join(SYSTEMS, "*", "system.json")):
        sid = json.load(open(p, encoding="utf-8"))["id"]
        if sid != 0 and f"system:{sid}" not in ents:
            bad.append(f"system {sid}: no series")
    if os.path.exists(EXPEDITIONS):
        for r in json.load(open(EXPEDITIONS, encoding="utf-8"))["systems"]:
            if f"system:{r['system']}" not in ents:
                bad.append(f"explored system {r['system']} without a series")
    return ents, bad


def merge(N, ents):
    E = N["entities"]
    added = changed = 0
    for eid, e in ents.items():
        cur = E.get(eid)
        if eid.startswith("body:"):
            if cur and cur.get("status") == "CANON":
                ex = cur.setdefault("names", {}).setdefault("exonyms", {})
                for lg, v in e["names"]["exonyms"].items():
                    if lg not in ex:
                        ex[lg] = v
                        changed += 1
                continue
            added += cur is None
            changed += cur is not None and cur != e
            E[eid] = e
            continue
        sid = int(eid.split(":")[1])
        if cur is None:
            atlas = json.load(open(os.path.join(system_dir(sid), "system.json"), encoding="utf-8"))
            cur = E[eid] = dict(type="system", catalog=atlas["name"].split(" (")[0], verdict="KEEP")
            added += 1
        if cur.get("status") == "CANON":
            ex = cur.setdefault("names", {}).setdefault("exonyms", {})
            for lg, v in (e.get("exonyms") or {}).items():
                ex.setdefault(lg, v)
            continue
        if "local" in e:
            cur["local"] = e["local"]
            cur["exonyms"] = dict(e["exonyms"])
            cur["meaning"] = e["meaning"]
        nh = cur.setdefault("naming_history", {})
        nh.setdefault("status", "PROPOSAL")
        nh["named_by"] = e["named_by"]
        nh["body_series"] = f"{e['theme']} (PROPOSAL {DATE})"
        changed += 1
    return added, changed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    ents, bad = build()
    nb = sum(1 for k in ents if k.startswith("body:"))
    ns = sum(1 for k in ents if k.startswith("system:"))
    print(f"series: {ns} systems, {nb} bodies, locales {', '.join(LOCALES)}")
    for b in bad:
        print("  !", b)
    if bad:
        return 1
    if a.write:
        with open(NAMING, encoding="utf-8") as f:
            N = json.load(f)
        added, changed = merge(N, ents)
        with open(NAMING, "w", encoding="utf-8", newline="\n") as f:
            f.write(json.dumps(N, ensure_ascii=False, indent=1) + "\n")
        print(f"naming.json: {added} added, {changed} updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
