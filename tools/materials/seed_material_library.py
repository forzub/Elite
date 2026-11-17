#!/usr/bin/env python3
"""Seed / refresh the global material library (MESH_EDITOR_SPEC 16.4): assets/materials/material_library.json.

One entry per visual material id used anywhere in the sources: the equipment generator palette (tools/models/eqlib.py
+ every unit's register_materials), and every `newmtl` in <assets_source>/models/**.mtl and assets/models/**.mtl.
Existing entries are never changed (the author edits them in the mesh editor); only new ids are added.

Entry: {id, name, baseColor [r,g,b], metallic, roughness, emissive [r,g,b], twoSided, physical, source}
  physical - the physical passport (src/game/physics/UniverseMaterials.h, elite.material.*) when one is linked.
usage: python tools/materials/seed_material_library.py
"""
import glob
import importlib
import json
import os
import pkgutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools", "models"))
import eqlib  # noqa: E402
import units  # noqa: E402

LIB = os.path.join(ROOT, "assets", "materials", "material_library.json")
PASSPORTS = {   # mesh material id -> physical passport (UniverseMaterials.h)
    "mat_radiator_aster_c_sic": "elite.material.radiator.c-sic.v1",
    "mat_engine_vanta_hfc_c": "elite.material.engine.hfc-c.v1",
}
# new ids that glow (window lights, beacons, floodlights of the hub district, 2026-11-16): emissive colour on first seed
EMISSIVE = {
    "eq_hub_window_warm": [1.0, 0.72, 0.40], "eq_hub_window_cool": [0.55, 0.75, 1.0],
    "eq_hub_beacon": [1.0, 0.10, 0.05], "eq_hub_flood": [0.85, 0.92, 1.0],
    "eq_hub_flare": [1.0, 0.48, 0.14],
}


def from_phong(kd, ks, ns):
    spec = sum(ks) / 3.0
    return {"baseColor": [round(c, 4) for c in kd], "metallic": 0.7 if spec > 0.4 else 0.15 if spec > 0.15 else 0.0,
            "roughness": round(max(0.08, min(1.0, 1.0 - ns / 250.0)), 3)}


def main():
    for m in pkgutil.iter_modules(units.__path__):
        importlib.import_module(f"units.{m.name}")   # units register their palettes on import
    lib = {"schema": "elite.material_library.v1", "materials": []}
    if os.path.exists(LIB):
        lib = json.load(open(LIB, encoding="utf-8"))
    have = {m["id"] for m in lib["materials"]}
    added = 0

    def add(mid, phong, source):
        nonlocal added
        if mid in have:
            return
        e = {"id": mid, "name": mid.replace("eq_", "").replace("_", " "), **from_phong(*phong), "emissive": EMISSIVE.get(mid, [0, 0, 0]),
             "twoSided": False, "physical": PASSPORTS.get(mid), "source": source}
        lib["materials"].append(e)
        have.add(mid)
        added += 1

    for mid, phong in sorted(eqlib.MATERIALS.items()):
        add(mid, phong, "tools/models/eqlib.py")
    src = os.environ.get("ELITE_ASSETS_SOURCE", os.path.join(os.path.dirname(ROOT), "assets_source"))
    for f in sorted(glob.glob(os.path.join(src, "models", "**", "*.mtl"), recursive=True) +
                    glob.glob(os.path.join(ROOT, "assets", "models", "**", "*.mtl"), recursive=True)):
        cur, kd, ks, ns = None, (0.6, 0.6, 0.6), (0.2, 0.2, 0.2), 40.0
        def flush():
            if cur:
                add(cur, (kd, ks, ns), os.path.relpath(f, os.path.dirname(ROOT)).replace("\\", "/"))
        for line in open(f, encoding="utf-8", errors="replace"):
            w = line.split()
            if not w:
                continue
            if w[0] == "newmtl":
                flush()
                cur, kd, ks, ns = w[1] if len(w) > 1 else "default", (0.6, 0.6, 0.6), (0.2, 0.2, 0.2), 40.0
            elif w[0] == "Kd":
                kd = tuple(float(x) for x in w[1:4])
            elif w[0] == "Ks":
                ks = tuple(float(x) for x in w[1:4])
            elif w[0] == "Ns":
                ns = float(w[1])
        flush()
    if "default" not in have:
        lib["materials"].insert(0, {"id": "default", "name": "по умолчанию", "baseColor": [0.6, 0.62, 0.65], "metallic": 0.1,
                                    "roughness": 0.7, "emissive": [0, 0, 0], "twoSided": False, "physical": None, "source": "seed"})
        added += 1
    os.makedirs(os.path.dirname(LIB), exist_ok=True)
    with open(LIB, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(lib, fh, ensure_ascii=False, indent=1)
        fh.write("\n")
    print(f"material library: {len(lib['materials'])} materials ({added} new) -> {LIB}")


if __name__ == "__main__":
    main()
