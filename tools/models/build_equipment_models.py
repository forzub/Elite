#!/usr/bin/env python3
"""Equipment models from the equipment passports: geometry only (no textures), colours are MTL materials.

Kernel: tools/models/eqlib.py (primitives, LOD policy, shared builders, hit-volume helpers, OBJ/MTL writers).
Units:  tools/models/units/<maker>.py - one module per maker, each with ASSETS = {folder: parts_fn(lod)},
        HITS = {folder: hits_fn()} and MODEL_ASSETS = {part_id: folder}. Design schools: tools/models/DESIGN_SCHOOLS.md.
Output: <assets_source>/models/equipment/<folder>/intact/assembly/LOD0..LOD3/<part>.obj + .mtl and
        <folder>/intact/assembly/hit_volumes.json (the source layout: tools/models/source_layout.py, MESH_EDITOR_SPEC 18),
        plus a minimal passport <folder>/model.json when there is none. LOD0 = close inspection (eqlib.close() True),
        LOD1 = the 20-30 m look, LOD2, LOD3. Only the intact assembly is the generator's: shells and damaged variants in
        the same folder are never touched.
Pads:   a unit with a pad part (a drive's engine_hot_emitter_annulus, a reactor's reactor_cooling_pad) is split into
        <unit> (+ intact/assembly/mounts.json), <unit>_pad (the pad in its own frame) and <unit>_set (kind assembly:
        the unit + pad(s) by reference) - tools/models/pads.py, MESH_EDITOR_SPEC 18.11.

usage: python tools/models/build_equipment_models.py [--check] [--only <folder-substring>]
"""
import argparse
import importlib
import os
import pkgutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eqlib  # noqa: E402
import pads  # noqa: E402
import source_layout  # noqa: E402
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


PASSPORTS = {}   # model id -> (title, kind, description) of a model the split creates (pads, sets)


def _short(asset):
    """the unit's short title for its pad / set ("CR-1 · Cobretty CR-1 Vector Main Drive" -> ("CR-1", the rest))"""
    p = source_layout.passport(os.path.join(eqlib.OUT, asset), asset, write=False)
    t = p.get("title", asset)
    return tuple(t.split(" · ", 1)) if " · " in t else (t, "")


def build(only=None):
    assets, hits, _ = collect()
    files, stats = {}, {}

    def emit(asset, lod, parts):
        total = 0
        for name, part in parts.items():
            if not part.tris():          # a detail dropped at this LOD: no empty OBJ
                continue
            base = os.path.join(source_layout.lod_dir(os.path.join(eqlib.OUT, asset), lod), name)
            files[base + ".obj"] = eqlib.obj_text(part, name + ".mtl")
            files[base + ".mtl"] = eqlib.mtl_text(part.faces.keys())
            total += part.tris()
        stats[(asset, lod)] = (total, {k: v.tris() for k, v in parts.items() if v.tris()})

    def put(asset, name, text):
        files[os.path.join(source_layout.assembly_dir(os.path.join(eqlib.OUT, asset)), name)] = text

    for asset, fn in assets.items():
        if only and only not in asset:
            continue
        ms = pid = None
        for lod in (0, 1, 2, 3):
            eqlib._CLOSE[0] = lod == 0
            try:
                parts = fn(max(0, lod - 1))
            finally:
                eqlib._CLOSE[0] = False
            if lod == 0:
                pid, kind = pads.pad_part(parts)
                if pid:
                    ms = pads.mounts(kind, parts[pid])
            if not pid or pid not in parts:
                emit(asset, lod, parts)
                continue
            # the pad goes to its own module; the set shows the unit and its pad(s) where they sit
            pad = parts.pop(pid)
            emit(asset, lod, parts)
            emit(asset + "_pad", lod, {pid: pads.to_pad(pad, ms[0])})
            emit(asset + "_set", lod, {asset: pads.merged(asset, list(parts.values())), asset + "_pad": pads.merged(asset + "_pad", [pad])})
        if asset in hits:
            vols = hits[asset]()
            if ms:
                unit_v, pad_v = pads.split_hits(vols, pid, ms)
                put(asset, "hit_volumes.json", eqlib.hits_text(asset, lambda: unit_v))
                put(asset + "_pad", "hit_volumes.json", eqlib.hits_text(asset + "_pad", lambda: pad_v))
                put(asset + "_set", "assembly.json", pads.set_doc(asset, ms, unit_v, pad_v))
            else:
                put(asset, "hit_volumes.json", eqlib.hits_text(asset, lambda: vols))
        if ms:
            put(asset, "mounts.json", pads.mounts_text(asset, ms, asset + "_pad"))
            put(asset + "_pad", "mounts.json", pads.pad_mounts_text(asset + "_pad", asset))
            short, rest = _short(asset)
            what = pads.TITLES[kind]
            PASSPORTS[asset + "_pad"] = (f"{what} · {short}" + (f" · {rest}" if rest else ""), "module",
                                         f"{what} {short}: сменная расходная часть, отдельный модуль (MESH_EDITOR_SPEC 18.11).")
            PASSPORTS[asset + "_set"] = (f"{short} + пятак" + (f" · {rest}" if rest else ""), "assembly",
                                         f"Сборка: {short} и его пятак по ссылкам (MESH_EDITOR_SPEC 18.11).")
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
        for dp, _, fs in os.walk(source_layout.assembly_dir(os.path.join(eqlib.OUT, asset))):
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
    for asset in {k[0] for k in stats}:
        source_layout.passport(os.path.join(eqlib.OUT, asset), asset, *PASSPORTS.get(asset, ()))
    print(f"written {len(stale)} of {len(files)} files, removed {len(orphans)} orphans")


if __name__ == "__main__":
    main()
