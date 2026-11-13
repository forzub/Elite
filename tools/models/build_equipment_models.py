#!/usr/bin/env python3
"""Equipment models from the equipment passports: geometry only (no textures), colours are MTL materials.

Kernel: tools/models/eqlib.py (primitives, LOD policy, shared builders, hit-volume helpers, OBJ/MTL writers).
Units:  tools/models/units/<maker>.py - one module per maker, each with ASSETS = {folder: parts_fn(lod)},
        HITS = {folder: hits_fn()} and MODEL_ASSETS = {part_id: folder}. Design schools: tools/models/DESIGN_SCHOOLS.md.
Output: <assets_source>/models/equipment/<folder>/LOD0..LOD3/<part>.obj + .mtl (src/assets/models/SOURCE_LAYOUT.md) and
        <folder>/hit_volumes.json. LOD0 = close inspection (eqlib.close() True), LOD1 = the 20-30 m look, LOD2, LOD3.

usage: python tools/models/build_equipment_models.py [--check] [--only <folder-substring>]
"""
import argparse
import importlib
import os
import pkgutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eqlib  # noqa: E402
import units  # noqa: E402


def collect():
    assets, hits, models = {}, {}, {}
    for m in sorted(pkgutil.iter_modules(units.__path__), key=lambda x: x.name):
        mod = importlib.import_module(f"units.{m.name}")
        for k, v in getattr(mod, "ASSETS", {}).items():
            if k in assets:
                raise ValueError(f"asset folder {k} defined twice ({m.name})")
            assets[k] = v
        hits.update(getattr(mod, "HITS", {}))
        for pid, folder in getattr(mod, "MODEL_ASSETS", {}).items():
            if pid in models:
                raise ValueError(f"part {pid} has two models")
            models[pid] = folder
    return assets, hits, models


def build(only=None):
    assets, hits, _ = collect()
    files, stats = {}, {}
    for asset, fn in assets.items():
        if only and only not in asset:
            continue
        for lod in (0, 1, 2, 3):
            eqlib._CLOSE[0] = lod == 0
            try:
                parts = fn(max(0, lod - 1))
            finally:
                eqlib._CLOSE[0] = False
            total = 0
            for name, part in parts.items():
                if not part.tris():          # a detail dropped at this LOD: no empty OBJ
                    continue
                base = os.path.join(eqlib.OUT, asset, f"LOD{lod}", name)
                files[base + ".obj"] = eqlib.obj_text(part, name + ".mtl")
                files[base + ".mtl"] = eqlib.mtl_text(part.faces.keys())
                total += part.tris()
            stats[(asset, lod)] = (total, {k: v.tris() for k, v in parts.items()})
        if asset in hits:
            files[os.path.join(eqlib.OUT, asset, "hit_volumes.json")] = eqlib.hits_text(asset, hits[asset])
    return files, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--only")
    a = ap.parse_args()
    files, stats = build(a.only)
    stale = [p for p, t in files.items() if not os.path.exists(p) or open(p, encoding="utf-8", newline="").read() != t]
    # files of a LOD level no longer produced (a dropped part) are stale too
    produced = {os.path.normcase(p) for p in files}
    orphans = []
    for asset in {k[0] for k in stats}:
        for dp, _, fs in os.walk(os.path.join(eqlib.OUT, asset)):
            for f in fs:
                p = os.path.join(dp, f)
                if f.endswith((".obj", ".mtl")) and os.path.normcase(p) not in produced:
                    orphans.append(p)
    for (asset, lod), (total, per) in sorted(stats.items()):
        print(f"{asset} LOD{lod}: {total} triangles  " + ", ".join(f"{k} {v}" for k, v in per.items()))
    if a.check:
        if stale or orphans:
            print("STALE:", *[os.path.relpath(p, eqlib.ROOT) for p in stale + orphans], sep="\n  ")
            sys.exit(1)
        print("equipment models up to date")
        return
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, "w", encoding="utf-8", newline="\n").write(files[p])
    for p in orphans:
        os.remove(p)
    print(f"written {len(stale)} of {len(files)} files, removed {len(orphans)} orphans")


if __name__ == "__main__":
    main()
