#!/usr/bin/env python3
"""Automatic first layout sketches for every catalog ship (author 2026-11-10: design the ships from their equipment,
each maker by its own building principle). Rough, for the author to iterate on; the Cobrett-I has a hand-made sketch
(tools/ships/layout_sketch.py).

Per ship (Notes/html/ship_reference/data.js): the functional groups become compartments, packed along the keel from the
stern: propulsion -> power / jump -> payload -> systems -> command; hull-skin units (radars, masts, weapons, docking,
gear, heat shields, armour, RCS) go on the hull; radiators become the removable cover plates (zero thickness) over the
compartments. Every equipment instance is the real model (LOD2 / LOD3 for big counts). The hull follows the maker:
  monolithic - a smooth shell around the contents (Cobretty, Hyperion, Bastion, Euro-Habitat);
  modular    - one framed section per compartment on a spine (Agora, Harmony, Sol Mass Yards, Terra, Uhuru);
  truss      - an open keel truss, equipment exposed, plates only over the compartments (Tianhe, Kern, OFU, Bharat,
               Stone docks, Sol Reactor, others).
Output: assets/models/layout_sketches/<ship id>/LOD0 (untracked), hit_volumes.json + ship_structure.json (via
layout_sketch.ship_hits), renders to the reference workshop (hull:<ship id>), and sketch_sizes.json: content-driven
hull size vs the passport frame envelope (input for the passport revision).
usage: python tools/ships/auto_sketch.py [ship-id-substring ...] [--no-render]
"""
import json
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import layout_sketch as LS  # noqa: E402
from layout_sketch import Part, box, cyl, place, plate, quad, OUT, EQ, ROOT  # noqa: E402

PRINCIPLE = {"mfr.cobretty": "monolithic", "mfr.hyperion": "monolithic", "mfr.bastion": "monolithic", "mfr.eurohab": "monolithic",
             "mfr.agora_modular": "modular", "mfr.harmony_yards": "modular", "mfr.sol_mass_yards": "modular",
             "mfr.terra_habitat": "modular", "mfr.uhuru_bio": "modular"}
ZONE = {"drive": 0, "reactor": 0, "coolant_loop": 0,
        "service_reactor": 1, "jump_core": 1, "tank": 1, "cryo_tank": 1,
        "container_rack": 2, "bulk_hold": 2, "train_module": 2, "passenger_module": 2, "hangar": 2, "boat_bay": 2, "drone_bay": 2,
        "workshop": 2, "mining_rig": 2, "tow_rig": 2, "lifeboat": 2,
        "life_support": 3, "medbay": 3, "computer": 3, "comm": 3, "transponder": 3, "ecm": 3,
        "escape_capsule": 4}
SKIN = {"radar", "sensor_mast", "weapon", "docking_port", "landing_gear", "heat_shield", "armour", "rcs"}
C20 = (2.44, 2.59, 6.06)


def models():
    sys.path.insert(0, os.path.join(ROOT, "tools", "models"))
    import build_equipment_models as B
    return B.collect()[2]


def size_of(folder):
    """the model's extent from its hit volumes (x, y, z) and the centre offset"""
    d = json.load(open(os.path.join(EQ, folder, "hit_volumes.json"), encoding="utf-8"))
    lo, hi = [1e9] * 3, [-1e9] * 3
    for v in d["volumes"]:
        ext = v.get("half_size") or [v.get("radius", 0.0), v.get("half_height", 0.0), v.get("radius", 0.0)]
        R = LS._euler_xyz(v["rotation_deg"])
        for cx in (-1, 1):
            for cy in (-1, 1):
                for cz in (-1, 1):
                    c = LS.apply(R, (cx * ext[0], cy * ext[1], cz * ext[2]))
                    for k in range(3):
                        lo[k] = min(lo[k], v["position"][k] + c[k])
                        hi[k] = max(hi[k], v["position"][k] + c[k])
    return [hi[k] - lo[k] for k in range(3)], [(hi[k] + lo[k]) / 2 for k in range(3)]


def build(ship, M, sizes):
    P = {}
    principle = PRINCIPLE.get(ship.get("manufacturer"), "truss")
    L_p, W_p, H_p = (ship.get("frame") or {}).get("envelope_m", [20, 10, 5])
    width = max(4.0, W_p * 0.9)
    inst = {z: [] for z in range(5)}
    skin, radiators, containers = [], 0, 0
    for g in ship.get("functional_groups", []):
        for it in g.get("items", []):
            pid, qty, cat = it.get("part_id"), int(it.get("qty") or 1), it.get("category")
            if cat == "radiator":
                radiators += qty
                continue
            if cat == "container_rack":
                containers += qty
                continue
            folder = M.get(pid)
            if not folder:
                continue
            if cat in SKIN:
                skin += [(pid, folder, cat)] * min(qty, 12)
                continue
            inst[ZONE.get(cat, 2)] += [(pid, folder, cat)] * min(qty, 24)
    lod = 3 if sum(len(v) for v in inst.values()) + containers > 80 else 2
    z = 0.0                                   # the stern face; the ship grows towards -Z (forward)
    comps, hmax = [], 0.0
    for zone in range(5):
        items = sorted(inst[zone], key=lambda t: -sizes[t[1]][0][0] * sizes[t[1]][0][2])
        if zone == 2 and containers:
            per_row = max(1, int(width // (C20[0] + 0.1)))
            layers = max(1, min(6, int((H_p * 0.85) // (C20[1] + 0.05))))
            rows = math.ceil(containers / (per_row * layers))
            k = 0
            for r in range(rows):
                for layer in range(layers):
                    for c in range(per_row):
                        if k >= containers:
                            break
                        x = (c - (per_row - 1) / 2) * (C20[0] + 0.1)
                        cz = z - r * (C20[2] + 0.2) - C20[2] / 2
                        pc = P.setdefault("c20_cargo", Part("c20_cargo"))
                        box(pc, "sk_c20", (x, C20[1] / 2 + layer * (C20[1] + 0.05), cz), C20)
                        k += 1
            depth = rows * (C20[2] + 0.2)
            comps.append(("cargo", z, z - depth, layers * C20[1]))
            hmax = max(hmax, layers * (C20[1] + 0.05))
            z -= depth + 0.6
        if not items:
            continue
        # 3D shelves: rows across the width, then layers upwards (up to the passport height), then the next slab aft->fore
        x, y, row_depth, row_h, slab_depth, z0 = -width / 2, 0.0, 0.0, 0.0, 0.0, z
        h_cap = max(3.0, H_p * 0.85)
        for i, (pid, folder, cat) in enumerate(items):
            (sx, sy, sz), c = sizes[folder]
            if x + sx > width / 2 and x > -width / 2:             # row full -> next layer up, or the next slab
                x = -width / 2
                if y + row_h + 0.3 + sy <= h_cap:
                    y += row_h + 0.3
                else:
                    z -= max(slab_depth, row_depth) + 0.4
                    y, slab_depth = 0.0, 0.0
                slab_depth = max(slab_depth, row_depth)
                row_depth, row_h = 0.0, 0.0
            pos = (x + sx / 2 - c[0], y + sy / 2 - c[1], z - sz / 2 - c[2])
            place(P, f"eq_{zone}_{i}_{cat}", folder, pos, 0.0, lod)
            x += sx + 0.4
            row_depth = max(row_depth, sz)
            row_h = max(row_h, sy)
            hmax = max(hmax, y + sy)
        z -= max(slab_depth, row_depth) + 0.6
        comps.append((("propulsion", "power", "payload", "systems", "command")[zone], z0, z, hmax))
    length = -z
    H = max(hmax, 2.0)
    # hull-skin units along the top centre line and the sides
    for i, (pid, folder, cat) in enumerate(skin):
        (sx, sy, sz), c = sizes[folder]
        zz = -length * (0.15 + 0.7 * (i + 0.5) / max(1, len(skin)))
        if cat in ("landing_gear",):
            continue
        place(P, f"skin_{i}_{cat}", folder, (((i % 3) - 1) * width * 0.3 - c[0], H + 0.05 - (c[1] - sy / 2), zz - c[2]), 0.0, lod)
    # hull by principle
    h = P["ghost_hull"] = Part("ghost_hull")
    if principle == "monolithic":
        LS.hull_loft(P, [(-width * 0.35, -length - 2.0), (width * 0.35, -length - 2.0), (width / 2 + 0.6, -length * 0.75),
                         (width / 2 + 0.6, 0.6), (-width / 2 - 0.6, 0.6), (-width / 2 - 0.6, -length * 0.75)],
                     lambda x_, z_: H + 0.4, lambda x_, z_: -0.4)
    elif principle == "modular":
        for name, za, zb, hh in comps:                          # one framed section per compartment
            for sxx in (-1, 1):
                for syy in (0, 1):
                    cyl(h, "sk_hull", (sxx * (width / 2 + 0.3), syy * (hh + 0.3), za), (sxx * (width / 2 + 0.3), syy * (hh + 0.3), zb), 0.18, 8)
            for zz in (za, zb):
                box(h, "sk_hull", (0, (hh + 0.3) / 2, zz), (width + 0.8, hh + 0.9, 0.3))
        cyl(h, "sk_hull", (0, -0.6, 0.5), (0, -0.6, -length - 0.5), 0.5, 12)            # spine
    else:                                                       # open truss keel
        for sxx in (-1, 1):
            for syy in (-0.3, H + 0.3):
                cyl(h, "sk_hull", (sxx * (width / 2 + 0.4), syy, 0.5), (sxx * (width / 2 + 0.4), syy, -length - 0.5), 0.16, 8)
        n = max(2, int(length // 6))
        for i in range(n + 1):
            zz = -length * i / n
            for a, b in (((-1, -0.3), (1, -0.3)), ((-1, H + 0.3), (1, H + 0.3)), ((-1, -0.3), (-1, H + 0.3)), ((1, -0.3), (1, H + 0.3))):
                cyl(h, "sk_hull", (a[0] * (width / 2 + 0.4), a[1], zz), (b[0] * (width / 2 + 0.4), b[1], zz), 0.1, 6)
            if i < n:
                zn = -length * (i + 1) / n
                for sxx in (-1, 1):
                    cyl(h, "sk_hull", (sxx * (width / 2 + 0.4), -0.3, zz), (sxx * (width / 2 + 0.4), H + 0.3, zn), 0.07, 6)
    # radiator cover plates over the compartments (zero thickness)
    k = 0
    for name, za, zb, hh in comps:
        if name == "propulsion":
            continue
        nx = max(1, int(width // 4))
        for i in range(nx):
            x0 = -width / 2 + i * width / nx
            plate(P, f"cover_{k}", "sk_cover", x0 + 0.04, x0 + width / nx - 0.04, zb + 0.04, za - 0.04, H + 0.401 if principle == "monolithic" else hh + 0.6)
            k += 1
    return P, principle, (round(length + 2.0, 1), round(width + 1.2, 1), round(H + 0.8, 1)), radiators, comps


def render(folder_rel, out_png, view, hidden=None, mode=None):
    B = "C:/Program Files/Blender Foundation/Blender 5.0/blender.exe"
    cmd = [B, "-b", "--factory-startup", "-P", "tools/models/render_preview.py", "--", folder_rel, "0", out_png, view, hidden or "-"]
    if mode:
        cmd.append(mode)
    subprocess.run(cmd, stdin=subprocess.DEVNULL, capture_output=True, text=True, cwd=ROOT)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    do_render = "--no-render" not in sys.argv
    txt = open(os.path.join(ROOT, "Notes", "html", "ship_reference", "data.js"), encoding="utf-8").read()
    D = json.loads(txt.split("=", 1)[1].rstrip().rstrip(";"))
    M = models()
    sizes = {f: size_of(f) for f in set(M.values())}
    report = {}
    skip = {"kb.ship.cobrett-i.frnu-inta.a3"}                 # hand-made sketch
    skip |= {s["id"] for s in D["ships"] if s["id"].startswith("aux.craft.")}   # small craft: their own models (drones, boats)
    for ship in D["ships"]:
        if ship["id"] in skip or (args and not any(a in ship["id"] for a in args)):
            continue
        P, principle, size, radiators, comps = build(ship, M, sizes)
        LS.INSTALLS.clear()
        LS.PLATES.clear()
        name = ship["id"]
        d = os.path.join(OUT, name, "LOD0")
        os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            os.remove(os.path.join(d, f))
        for pname, part in P.items():
            if not part.tris():
                continue
            open(os.path.join(d, pname + ".obj"), "w", encoding="utf-8", newline="\n").write(LS.obj_text(part, pname + ".mtl"))
            open(os.path.join(d, pname + ".mtl"), "w", encoding="utf-8", newline="\n").write(LS.mtl_text(part.faces.keys()))
        report[name] = {"principle": principle, "sketch_size_LWH_m": size, "passport_frame_LWH_m": (ship.get("frame") or {}).get("envelope_m"),
                        "radiator_panels_as_cover_plates": radiators, "compartments": [c[0] for c in comps]}
        print(f"{name:40s} {principle:10s} sketch {size}  passport {report[name]['passport_frame_LWH_m']}")
        if do_render:
            rel = os.path.relpath(os.path.join(OUT, name), ROOT)
            covers = ",".join(p for p in P if p.startswith("cover_"))
            render(rel, os.path.join(OUT, name, "auto_out.png"), "pad")
            render(rel, os.path.join(OUT, name, "auto_in.png"), "top", "ghost_hull," + covers if covers else "ghost_hull")
    path = os.path.join(OUT, "sketch_sizes.json")
    old = json.load(open(path, encoding="utf-8")) if os.path.exists(path) else {}
    old.update(report)
    open(path, "w", encoding="utf-8", newline="\n").write(json.dumps(old, ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()
