"""Regenerate src/assets/data/galaxy/generation_ships.json from the canon.

Backward compatible by construction:
  * every legacy ship object and every legacy field is kept verbatim
    (legacy values are the OLD 'Universe' branch and are marked as such);
  * each object gains a `canon` block with the reconciled values;
  * canonical missions absent from the legacy file are appended with the same
    legacy field names, filled from the canon;
  * the file becomes valid JSON (the legacy file was several concatenated arrays
    after "ships": and could not be parsed by any JSON reader).

No runtime code reads this file today (GalaxyDatabase::loadGenerationShips is
commented out); the canon block is what a future loader should read.

usage: python export_runtime.py [--write]   (without --write: print a diff summary)
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RUNTIME = os.path.normpath(os.path.join(HERE, "..", "..", "src", "assets", "data", "galaxy", "generation_ships.json"))

LEGACY_TO_CANON = {"TRADER'S-GUILD-1": "TRADERS-GUILD-1"}

STATUS_3126 = {  # canon fate.status -> legacy-style display word
    "arrived": "КОЛОНИЯ", "arrived_colony_quarantined": "КАРАНТИН", "arrived_fragmented": "РАСКОЛ", "arrived_3122": "ПРИБЫЛ",
    "arrived_3126": "ПРИБЫЛ", "destroyed_unexplained": "ПРИЗРАК", "destroyed": "ПРИЗРАК", "missing": "ПРИЗРАК",
    "derelict_empty": "ПРИЗРАК", "derelict_found": "ПРИЗРАК", "in_flight": "В ПУТИ", "museum": "ХРОНИКИ", "impounded": "ХРОНИКИ",
}


def read_legacy(path):
    with open(path, encoding="utf-8") as f:
        s = f.read()
    try:
        d = json.loads(s)
        return d["ships"] if "ships" in d else d
    except ValueError:
        body = s[s.index('"ships"'):]
        body = body[body.index("["):]
        body = re.sub(r"\]\s*,\s*\[", ",", body)
        body = body[:body.rindex("]") + 1]
        return json.loads(body)


def canon_block(m, lab, world):
    v = lab["missions"].get(m["id"], {})
    fate = m["fate"]
    final = m.get("final_system")
    owner = None
    if m.get("colony"):
        col = next((c for c in world["colonies"] if c["id"] == m["colony"]), None)
        owner = col["polity"] if col else None
    alive = 0
    if fate["status"] in ("in_flight",):
        alive = {"FINAL-ARK-1": 14500, "KEPLER-1": 9300, "NOMAD-1": 450, "LEGACY-1": 0}.get(m["id"], 0)
    if fate["status"] in ("arrived_3122", "arrived_3126"):
        alive = next((a["pop_after"] for a in m["anchors"] if a["type"] == "arrival"), 0)
    return {
        "id": m["id"],
        "status": m["canon_status"],
        "knowledge": m.get("knowledge"),
        "field_status": m.get("field_status"),
        "aliases": m["aliases"],
        "wave": m["wave"],
        "sponsor": m["sponsor"],
        "launch_year": m["launch_year"],
        "source_system": m["source_system"],
        "target_system": m["target_system"],
        "final_system": final,
        "population": m["population"],
        "cryo_share": m["cryo_share"],
        "awake_watch": m["awake_watch"],
        "birth_policy": m["birth_policy"],
        "arrival_year": next((a["year"] for a in m["anchors"] if a["type"] == "arrival"), None),
        "arrival_population": next((a.get("pop_after") for a in m["anchors"] if a["type"] == "arrival"), None),
        "avg_speed_c": v.get("avg_c"),
        "motives": m["motives"],
        "fate_3126": fate["status"],
        "ship_state_3126": fate["ship"],
        "ship_note": fate.get("ship_note", ""),
        "location_3126": final if isinstance(final, int) and fate["status"] not in ("missing", "in_flight") else "unknown_or_moving",
        "living_aboard_3126": alive,
        "owner_3126": owner,
        "findable": fate.get("findable", False),
        "author_knows_truth": fate.get("author_knows", True),
        "colony": m.get("colony"),
        "sources": m["sources"],
    }


def build():
    with open(os.path.join(HERE, "canon", "missions.json"), encoding="utf-8") as f:
        MS = json.load(f)
    with open(os.path.join(HERE, "canon", "world.json"), encoding="utf-8") as f:
        W = json.load(f)
    lab_path = os.path.join(HERE, "out", "lab_state_3126.json")
    with open(lab_path, encoding="utf-8") as f:
        lab = json.load(f)
    canon = {m["id"]: m for m in MS["missions"]}
    retired = {r["id"]: r["reason"] for r in MS["retired"]}
    legacy = read_legacy(RUNTIME)
    out = []
    seen = set()
    for s in legacy:
        s = dict(s)
        cid = LEGACY_TO_CANON.get(s["id"], s["id"])
        s["legacy_values"] = True
        if cid in canon:
            s["canon"] = canon_block(canon[cid], lab, W)
            s["status_3126"] = STATUS_3126.get(canon[cid]["fate"]["status"], "ХРОНИКИ")
            seen.add(cid)
        else:
            merged = "GENESIS-1" if s["id"] == "GENESIS-2" else ("UHURU-2" if s["id"] == "UHURU-1" else None)
            s["canon"] = {"id": s["id"], "status": "retired", "reason": retired.get(s["id"], "не канон"), "merged_into": merged}
        out.append(s)
    for mid, m in sorted(canon.items(), key=lambda kv: kv[1]["launch_year"]):
        if mid in seen:
            continue
        arr = next((a for a in m["anchors"] if a["type"] == "arrival"), None)
        out.append({
            "id": mid, "wave": {"I": "early", "II": "early", "III": "middle", "IV": "late", "hyper": "late"}[m["wave"]],
            "launchYear": m["launch_year"],
            "arrival": {"year": arr["year"] if arr else None, "mode": "hyper" if m["wave"] == "hyper" else ("sublight" if arr else "unknown"), "note": None},
            "sender": m["sponsor"], "crew": m["population"], "target": str(m["target_system"]),
            "status_3126": STATUS_3126.get(m["fate"]["status"], "ХРОНИКИ"),
            "legacy_values": False, "canon": canon_block(m, lab, W),
        })
    doc = {
        "schema_note": "Legacy fields (wave, launchYear, arrival, sender, crew, target, status_3024, geometry, mass, fate...) are the OLD Universe branch and are kept only for backward compatibility. Authoritative values are in each ship's `canon` block, generated by tools/colonization_lab/export_runtime.py from tools/colonization_lab/canon/*.json. Do not hand-edit `canon`; edit the canon and regenerate. Validate: python tools/colonization_lab/lab.py --check-dataset",
        "canon_source": "tools/colonization_lab/canon/missions.json",
        "ships": out,
    }
    return doc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    doc = build()
    text = json.dumps(doc, ensure_ascii=False, indent=2) + "\n"
    if a.write:
        with open(RUNTIME, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"ships={len(doc['ships'])} canon={sum(1 for s in doc['ships'] if s['canon']['status'] != 'retired')} "
          f"retired={sum(1 for s in doc['ships'] if s['canon']['status'] == 'retired')} written={a.write}")


if __name__ == "__main__":
    sys.exit(main())
