#!/usr/bin/env python3
"""Equipment models from the equipment passports: geometry only (no textures), colours are MTL materials.

Kernel: tools/models/eqlib.py (primitives, LOD policy, shared builders, hit-volume helpers, OBJ/MTL writers).
Units:  tools/models/units/<maker>.py - one module per maker, each with ASSETS = {folder: parts_fn(lod)},
        HITS = {folder: hits_fn()}, RIGS = {folder: rig_fn() -> {"joints": [...], "clips": [...]}} (optional: the skeleton
        and test animations, intact/assembly/rig.json, eqlib.rig_text) and MODEL_ASSETS = {part_id: folder}. Design schools: tools/models/DESIGN_SCHOOLS.md.
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
    RIGS.clear()
    SETS.clear()
    ROOTS.clear()
    for m in sorted(pkgutil.iter_modules(units.__path__), key=lambda x: x.name):
        mod = importlib.import_module(f"units.{m.name}")
        for k, v in getattr(mod, "ASSETS", {}).items():
            if k in assets:
                raise ValueError(f"asset folder {k} defined twice ({m.name})")
            assets[k] = v
        hits.update(getattr(mod, "HITS", {}))
        RIGS.update(getattr(mod, "RIGS", {}))
        SETS.update(getattr(mod, "SETS", {}))
        ROOTS.update(getattr(mod, "ROOTS", {}))
        PASSPORTS.update(getattr(mod, "PASSPORTS", {}))
        for pid, folder in getattr(mod, "MODEL_ASSETS", {}).items():
            if pid in models:
                raise ValueError(f"part {pid} has two models")
            models[pid] = folder
    return assets, hits, models


RIGS = {}        # model folder -> rig_fn() (collect)
SETS = {}        # set folder -> spec of whole models by reference (collect)
ROOTS = {}       # model folder -> source sub-root when not "equipment" (collect): "hub" = the editor's hub modules


def out_dir(asset):
    """<assets_source>/models/<root>/<asset>: equipment by default, a unit's ROOTS entry otherwise"""
    return os.path.join(eqlib.SOURCE_MODELS, ROOTS.get(asset, "equipment"), asset)
PASSPORTS = {}   # model id -> (title, kind, description) of a model the split creates (pads, sets)


def _short(asset):
    """the unit's short title for its pad / set ("CR-1 · Cobretty CR-1 Vector Main Drive" -> ("CR-1", the rest))"""
    p = source_layout.passport(out_dir(asset), asset, write=False)
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
            base = os.path.join(source_layout.lod_dir(out_dir(asset), lod), name)
            files[base + ".obj"] = eqlib.obj_text(part, name + ".mtl")
            files[base + ".mtl"] = eqlib.mtl_text(part.faces.keys())
            total += part.tris()
        stats[(asset, lod)] = (total, {k: v.tris() for k, v in parts.items() if v.tris()})

    def put(asset, name, text):
        files[os.path.join(source_layout.assembly_dir(out_dir(asset)), name)] = text

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
        if asset in RIGS:
            put(asset, "rig.json", eqlib.rig_text(asset, RIGS[asset]()))
        if ms:
            put(asset, "mounts.json", pads.mounts_text(asset, ms, asset + "_pad"))
            put(asset + "_pad", "mounts.json", pads.pad_mounts_text(asset + "_pad", asset))
            short, rest = _short(asset)
            what = pads.TITLES[kind]
            PASSPORTS[asset + "_pad"] = (f"{what} · {short}" + (f" · {rest}" if rest else ""), "module",
                                         f"{what} {short}: сменная расходная часть, отдельный модуль (MESH_EDITOR_SPEC 18.11).")
            PASSPORTS[asset + "_set"] = (f"{short} + пятак" + (f" · {rest}" if rest else ""), "assembly",
                                         f"Сборка: {short} и его пятак по ссылкам (MESH_EDITOR_SPEC 18.11).")
    # sets of whole models by reference declared by the units (SETS = {set folder: {title, description, nodes: [(node id,
    # model folder, position, rotation_deg, role, parent)]}}, e.g. a repair drone + std.drone.toolbox on its saddle):
    # the view meshes are the models placed, the package takes the referenced modules' own meshes
    for sid, spec in SETS.items():
        if only and only not in sid and not any(only in n[1] for n in spec["nodes"]):
            continue
        # a big assembly (a hub district: hundreds of nodes) views only some LOD levels, from the modules' coarse levels
        # (spec "view_lods": {set LOD: module lod}); every node's placed copy merges into one part per module folder
        for lod, mlod in spec.get("view_lods", {0: 0, 1: 0, 2: 1, 3: 2}).items():
            eqlib._CLOSE[0] = lod == 0 and "view_lods" not in spec
            try:
                view, cache = {}, {}
                for nid, folder, pos, rot, role, parent in spec["nodes"]:
                    if folder not in cache:
                        m = pads.merged(folder, list(assets[folder](mlod).values()))
                        if spec.get("view_mode") == "boxes":
                            # thousands of nodes (a megastructure): the editor's view of each node is its module's
                            # bounding box - the package still instances the modules' own LODs
                            lo = [min(v[i] for v in m.v) for i in range(3)]
                            hi = [max(v[i] for v in m.v) for i in range(3)]
                            m = eqlib.Part(folder)
                            eqlib.box(m, "eq_hub_hull", tuple((a + b) / 2 for a, b in zip(lo, hi)),
                                      tuple(max(b - a, 0.5) for a, b in zip(lo, hi)))
                        cache[folder] = m
                    view.setdefault(folder, []).append(eqlib.placed(cache[folder], pads.euler(rot), pos))
                view = {k: pads.merged(k, v) for k, v in view.items()}
            finally:
                eqlib._CLOSE[0] = False
            emit(sid, lod, view)
        category_of = lambda f: {"hub": "hub_modules"}.get(ROOTS.get(f, "equipment"), "equipment_ship")
        # node groups with a simplified far mesh each (spec "proxies": {module: (Part in the set frame, node id)},
        # spec "modules": {node id: module}) -> <set>/intact/assembly/proxies/<module>.obj
        mp = []
        for mod, (part, rep) in sorted(spec.get("proxies", {}).items()):
            put(sid, f"proxies/{mod}.obj", eqlib.obj_text(part, f"{mod}.mtl"))
            put(sid, f"proxies/{mod}.mtl", eqlib.mtl_text(part.faces.keys()))
            mp.append({"module": mod, "obj": f"proxies/{mod}.obj", "node": rep})
        put(sid, "assembly.json", pads.ref_set_doc(sid, spec["nodes"], {n[0]: hits[n[1]]() for n in spec["nodes"] if n[1] in hits},
                                                   category_of, spec.get("modules"), mp, spec.get("proxy_error_m", 40.0)))
        PASSPORTS[sid] = (spec["title"], "assembly", spec["description"])
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
        for dp, _, fs in os.walk(source_layout.assembly_dir(out_dir(asset))):
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
        source_layout.passport(out_dir(asset), asset, *PASSPORTS.get(asset, ()))
    print(f"written {len(stale)} of {len(files)} files, removed {len(orphans)} orphans")


if __name__ == "__main__":
    main()
