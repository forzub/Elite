#!/usr/bin/env python3
"""Ship layout sketch (author 2026-11-10): a rough 3D hull with the REAL equipment models placed inside, to design the
ship around its equipment before the exterior. Not a game asset: output goes to assets/models/layout_sketches/<ship>/
LOD0 (untracked) and the renders to the reference workshop.

Frame: metres, +X right, +Y up, -Z forward (the project basis; the ship catalog uses +Z aft too).
Parts:
  ghost_hull            - the hull shell, rendered as a wire cage (render_preview draws ghost_* as wire);
  eq_<slot>             - equipment instances: the LOD2 OBJ of the unit's model (assets/models/equipment/<folder>);
  c20_<n>               - C20 containers (6.06 x 2.44 x 2.59 m) on their cargo-deck cells;
  cover_<n>             - removable radiator cover plates over the compartments: zero-thickness polygons;
  hatch_<n>             - ventral cargo hatches (zero-thickness outlines);
  shaft_*               - inspection shaft and cross passages with ladders and handrails;
  skid_*                - folding landing skids, deployed pose; proxy_* - items without a model yet.

usage: python tools/ships/layout_sketch.py [cobrett-i]
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "models"))
from eqlib import (Part, box, cyl, lathe, crect, rot_x, rot_y, rot_z, apply, norm, obj_text, mtl_text, MATERIALS,  # noqa: E402
                   register_materials, I3, ROOT)
import importlib  # noqa: E402
import pkgutil  # noqa: E402
import units  # noqa: E402

for m in pkgutil.iter_modules(units.__path__):            # register every maker palette (the OBJ copies reference them)
    importlib.import_module(f"units.{m.name}")

register_materials({
    "sk_hull": ((0.55, 0.62, 0.70), (0.1, 0.1, 0.1), 10),
    "sk_c20": ((0.62, 0.36, 0.20), (0.1, 0.1, 0.1), 10),
    "sk_cover": ((0.16, 0.18, 0.20), (0.1, 0.1, 0.1), 10),
    "sk_hatch": ((0.85, 0.70, 0.15), (0.1, 0.1, 0.1), 10),
    "sk_shaft": ((0.30, 0.60, 0.75), (0.1, 0.1, 0.1), 10),
    "sk_proxy": ((0.70, 0.30, 0.60), (0.1, 0.1, 0.1), 10),
    "sk_glass": ((0.10, 0.30, 0.42), (0.6, 0.6, 0.6), 120),
    "sk_skid": ((0.85, 0.75, 0.20), (0.1, 0.1, 0.1), 10),
})

# raw model sources live outside the repository (author 2026-11-13): ELITE_ASSETS_SOURCE or <repo>/../assets_source
SOURCE_MODELS = os.path.join(os.environ.get("ELITE_ASSETS_SOURCE") or os.path.normpath(os.path.join(ROOT, "..", "assets_source")), "models")
EQ = os.path.join(SOURCE_MODELS, "equipment")
OUT = os.path.join(SOURCE_MODELS, "layout_sketches")


def load_obj(folder, lod=2, only=None, skip=None):
    """parts of an equipment model at one LOD (all, or only / except the named OBJ parts), merged"""
    d = os.path.join(EQ, folder, f"LOD{lod}")
    V, N, F = [], [], {}
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(".obj"):
            continue
        if (only and fn[:-4] not in only) or (skip and fn[:-4] in skip):
            continue
        base_v, base_n = len(V), len(N)
        mat = "sk_proxy"
        for line in open(os.path.join(d, fn), encoding="utf-8"):
            if line.startswith("v "):
                V.append(tuple(map(float, line.split()[1:4])))
            elif line.startswith("vn "):
                N.append(tuple(map(float, line.split()[1:4])))
            elif line.startswith("usemtl "):
                mat = line.split()[1].strip()
            elif line.startswith("f "):
                F.setdefault(mat, []).append([base_v + int(t.split("//")[0]) - 1 for t in line.split()[1:]])
    return V, N, F


_CACHE = {}
INSTALLS = []        # (instance name, equipment folder, rotation matrix, position) - for the ship hit volumes
PLATES = []          # (plate name, x0, x1, z0, z1, y) - removable radiator cover plates


PADS = ("reactor_cooling_pad",)       # hull-skin parts kept as their own sketch part (hidden in the interior view)


def place(P, name, folder, pos, yaw_deg=0.0, lod=2, _sub=None):
    """an equipment model instance: rotated about Y by yaw, translated to pos; a reactor pad becomes <name>_pad"""
    if _sub is None and os.path.exists(os.path.join(EQ, folder, f"LOD{lod}", PADS[0] + ".obj")):
        place(P, name + "_pad", folder, pos, yaw_deg, lod, _sub="pad")
    key = (folder, lod, _sub)
    if key not in _CACHE:
        _CACHE[key] = load_obj(folder, lod, only=PADS if _sub == "pad" else None, skip=None if _sub == "pad" else PADS)
    V, N, F = _CACHE[key]
    R = rot_y(math.radians(yaw_deg))
    if _sub is None:
        INSTALLS.append((name, folder, R, tuple(pos)))
    part = P[name] = Part(name)
    part.v = [tuple(a + b for a, b in zip(apply(R, v), pos)) for v in V]
    part.n = [norm(apply(R, n)) for n in N] + [(0, 1, 0)] * max(0, len(V) - len(N))
    part.faces = {m: [tuple(f) for f in fl] for m, fl in F.items()}
    return part


def place_flipped(P, name, folder, pos, lod=2):
    """a hull-skin unit (outward +Y) installed on the belly: rotated 180 deg about Z, then moved to pos"""
    part = place(P, name, folder, (0.0, 0.0, 0.0), 0.0, lod)
    INSTALLS[-1] = (name, folder, rot_z(math.pi), tuple(pos))
    part.v = [(-x + pos[0], -y + pos[1], z + pos[2]) for x, y, z in part.v]
    part.n = [(-a, -b, c) for a, b, c in part.n]
    return part


def quad(part, mat, pts, normal):
    ids = [part.vert(p, normal) for p in pts]
    part.face(mat, ids, normal)


def plate(P, name, mat, x0, x1, z0, z1, y, up=True):
    """a zero-thickness horizontal plate (radiator cover / hatch)"""
    part = P.setdefault(name, Part(name))
    if mat == "sk_cover":
        PLATES.append((name, x0, x1, z0, z1, y))
    n = (0, 1, 0) if up else (0, -1, 0)
    quad(part, mat, [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)], n)
    return part


def hull_loft(P, outline, top, bottom):
    """the hull shell: plan outline [(x, z)] (convex-ish, clockwise from the nose) with a top / bottom height function"""
    h = P["ghost_hull"] = Part("ghost_hull")
    n = len(outline)
    cx = sum(p[0] for p in outline) / n
    cz = sum(p[1] for p in outline) / n
    T = [(x, top(x, z), z) for x, z in outline]
    B = [(x, bottom(x, z), z) for x, z in outline]
    for i in range(n):
        j = (i + 1) % n
        quad(h, "sk_hull", [B[i], B[j], T[j], T[i]], norm((outline[i][0] - cx, 0, outline[i][1] - cz)))
        # top and bottom as fans to the centre line
        h.face("sk_hull", [h.vert(T[i], (0, 1, 0)), h.vert(T[j], (0, 1, 0)), h.vert((cx, top(cx, cz), cz), (0, 1, 0))], (0, 1, 0))
        h.face("sk_hull", [h.vert(B[i], (0, -1, 0)), h.vert(B[j], (0, -1, 0)), h.vert((cx, bottom(cx, cz), cz), (0, -1, 0))], (0, -1, 0))


# ---------------------------------------------------------------- Cobrett-I A3 (Assisted: fore + aft drive per bay)
# v2 (author 2026-11-10): the Cobretty bay is dia 3.5 m (CR-1 with a 1.3 m conical pad, ER-5 3.5 m with its 11 m pad on
# the back) -> the hull is 3.5 m thick and ~25 x 23 m, close to the passport 26 x 22.2 x 5. The inspection walkway runs
# in the spine beside the containers (a C20 is 2.59 m high: no walkway fits above it any more).
H2 = 1.75                                            # half hull thickness


def cobrett_i():
    P = {}
    outline = [(-2.6, -12.8), (2.6, -12.8), (5.4, -9.6), (10.2, -5.2), (12.6, 1.0), (12.6, 5.5), (10.2, 10.4),
               (-10.2, 10.4), (-12.6, 5.5), (-12.6, 1.0), (-10.2, -5.2), (-5.4, -9.6)]

    def thick(x, z):
        ax = abs(x)
        t = H2 if ax <= 10.2 else H2 - (ax - 10.2) * 0.42
        if z < -5.5:                       # the nose tapers into the escape cockpit
            t = min(t, 1.45 + (z + 12.8) * 0.05)
        return t
    hull_loft(P, outline, lambda x, z: thick(x, z), lambda x, z: -thick(x, z))

    # propulsion bays at x = +-8.4: fore CR-1 (cone forward) - ER-5 (pad on the back) - aft CR-1 (cone aft)
    ze = 2.4
    for s_, side in ((-1, "l"), (1, "r")):
        x = s_ * 8.4
        place(P, f"eq_er5_{side}", "cobretty_er5_reactor", (x, 0.0, ze))
        place(P, f"eq_cr1_aft_{side}", "cobretty_cr1_vector", (x, 0.0, ze + 1.75))
        place(P, f"eq_cr1_fore_{side}", "cobretty_cr1_vector", (x, 0.0, ze - 1.75), 180.0)
    # escape cockpit = the nose module with its motor (proxy until its model): big canopy, part of the hull shape
    place(P, "eq_escape_cockpit", "cobretty_ec2_cockpit", (0.0, 0.0, -5.8))                   # EC-2: the nose module
    # spine (x +-1.3): jump core and tanks under the inspection walkway
    place(P, "eq_core", "zeropoint_z05_jump_core", (0.0, -1.0, -3.8))
    for i, z in enumerate((-1.9, -0.2, 1.5)):
        place(P, f"eq_jumpcharge_{i}", "agora_bz1_jumptank", (0.0, -1.25, z))
    for i, z in enumerate((3.3, 4.9)):
        place(P, f"eq_pellets_{i}", "agora_bt1_pellettank", (0.0, -1.3, z))
    place(P, "eq_apu", "cobretty_sr150_service_reactor", (0.0, -0.9, 7.2))
    place(P, "eq_coolant", "cobretty_cl180_coolant_loop", (0.0, -1.45, 9.4))
    # avionics and life support behind the cockpit, beside the spine
    place(P, "eq_computer", "xinzhi_mcc1_computer", (-3.4, -1.3, -4.6))
    place(P, "eq_comm", "xinzhi_c1_comm", (-3.4, -0.4, -3.4))
    place(P, "eq_transponder", "zeropoint_t1_transponder", (-2.0, -1.5, -4.6))
    place(P, "eq_radar", "xinzhi_n2_radar", (0.0, 1.45, -7.0))
    place(P, "eq_life_support", "eurohab_l4_lifesupport", (3.5, -1.15, -4.2))
    place(P, "eq_dock", "agora_su1_collar", (0.0, H2, -1.6))
    place(P, "eq_pd1", "cobretty_pd1_turret", (0.0, H2, 6.2))
    # cargo deck: 6 C20 in two L groups (the author's prototype topology), ventral unloading hatches under each
    k = 0
    for s_ in (-1, 1):
        for x, z in ((s_ * 2.6, -2.6), (s_ * 2.6, 3.5), (s_ * 5.1, 3.5)):
            c = P[f"c20_{k}"] = Part(f"c20_{k}")
            box(c, "sk_c20", (x, -H2 + 0.05 + 1.295, z + 3.03), (2.44, 2.59, 6.06))
            plate(P, f"hatch_{k}", "sk_hatch", x - 1.25, x + 1.25, z - 0.05, z + 6.11, -H2 - 0.001, up=False)
            k += 1
    # drone bays beside the forward cargo cells: repair drone left, recon drone right
    place(P, "eq_repair_drone", "agora_rm2_drone", (-5.1, -1.1, -2.0))
    place(P, "eq_recon_drone", "hyperion_rd1_drone", (5.1, -1.0, -2.0))
    # weapons: two forward lasers in the cockpit shoulders, two 2-missile racks in the wings outboard of the bays
    for s_ in (-1, 1):
        side = "l" if s_ < 0 else "r"
        place(P, f"eq_laser_{side}", "cobretty_fl2_laser", (s_ * 4.9, 0.0, -4.2))              # FL-2, fires forward
        place(P, f"eq_missiles_{side}", "cobretty_wr2_wing_rack", (s_ * 11.3, 0.0, 5.3))       # WR-2, 2 x Aspid-10
    # jump-field emitters on the wingtips (author 2026-11-10, ACCEPTED): ZP-FE1, the pair of the Z-0.5 core; mount face on the
    # wingtip, pod outboard (the left one yawed 180 deg - the pod is fore / aft symmetric)
    for s_ in (-1, 1):
        side = "l" if s_ < 0 else "r"
        place(P, f"eq_field_emitter_{side}", "zeropoint_zpfe1_field_emitter", (s_ * 12.6, 0.0, 3.25), 0.0 if s_ > 0 else 180.0)   # flat wingtip x 12.6, z 1.0..5.5
    # inspection walkway in the spine above the core and tanks, aft cross passage to the bays, ladders, handrails
    sh = P["shaft_spine"] = Part("shaft_spine")
    z0, z1 = -5.0, 10.0
    for x in (-0.6, 0.6):
        cyl(sh, "sk_shaft", (x, 1.25, z0), (x, 1.25, z1), 0.035, 6)
    for zz in [z0 + i * 0.5 for i in range(int((z1 - z0) / 0.5) + 1)]:
        box(sh, "sk_shaft", (0, 0.25, zz), (1.2, 0.04, 0.06))
    box(sh, "sk_shaft", (0, 0.22, (z0 + z1) / 2), (0.06, 0.04, z1 - z0))
    zx = 9.75
    for zz in (zx - 0.55, zx + 0.55):
        cyl(sh, "sk_shaft", (-6.6, 0.9, zz), (6.6, 0.9, zz), 0.035, 6)
    box(sh, "sk_shaft", (0, -0.1, zx), (13.2, 0.04, 1.1))
    for x, z in ((0.0, -4.6), (-6.4, zx), (6.4, zx)):
        for dx in (-0.22, 0.22):
            cyl(sh, "sk_shaft", (x + dx, -H2 + 0.1, z), (x + dx, 0.25, z), 0.03, 6)
        for i in range(4):
            cyl(sh, "sk_shaft", (x - 0.22, -1.45 + i * 0.45, z), (x + 0.22, -1.45 + i * 0.45, z), 0.02, 6)
    # radiator cover plates over the compartments (dorsal, zero thickness); the ER-5 pads cover the bays
    k = 0
    for x0, x1 in ((-6.5, -3.9), (-3.9, -1.3), (-1.3, 1.3), (1.3, 3.9), (3.9, 6.5)):
        for z0_, z1_ in ((-5.2, -2.7), (-2.7, 3.4), (3.4, 9.6)):
            plate(P, f"cover_{k}", "sk_cover", x0 + 0.04, x1 - 0.04, z0_ + 0.04, z1_ - 0.04, H2 + 0.001)
            k += 1
    # landing skids: folded, they are flush panels of the belly (the author's preference); shown deployed here
    for i, (x, z) in enumerate(((0.0, -8.0), (-8.4, -2.0), (8.4, -2.0), (-8.4, 7.4), (8.4, 7.4))):
        place_flipped(P, f"eq_skid_{i}", "cobretty_ls1_skid", (x, -H2, z))                    # LS-1, deployed
    return P


SHIPS = {"cobrett-i": cobrett_i}


# ---------------------------------------------------------------- ship hit volumes (author 2026-11-10)
# Plates: a main volume + one volume per seam; destroying all seams of a plate frees it (it can be torn off by a laser or
# a robot and the compartment emptied). Equipment: its own hit volumes moved to the install, plus mount points; destroying
# all mounts of a module frees it - it flies apart from the ship. Written next to the sketch as hit_volumes.json (render
# with render_preview ... hits) and ship_structure.json (the release rules).
def _mat(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def _euler_xyz(deg):
    rx, ry, rz = (math.radians(a) for a in deg)
    return _mat(_mat(rot_x(rx), rot_y(ry)), rot_z(rz))


def _to_euler_xyz(R):
    sy = max(-1.0, min(1.0, R[0][2]))
    ry = math.asin(sy)
    if abs(sy) < 0.9999:
        rx = math.atan2(-R[1][2], R[2][2])
        rz = math.atan2(-R[0][1], R[0][0])
    else:
        rx = math.atan2(R[2][1], R[1][1])
        rz = 0.0
    return [round(math.degrees(a), 4) + 0.0 for a in (rx, ry, rz)]


def ship_hits():
    import json
    vols, rules = [], {"plates": [], "modules": []}
    for name, x0, x1, z0, z1, y in PLATES:
        cx, cz, w, l = (x0 + x1) / 2, (z0 + z1) / 2, x1 - x0, z1 - z0
        vols.append({"id": f"{name}.main", "part": name, "role": "plate", "shape": "box", "position": [cx, y - 0.03, cz],
                     "rotation_deg": [0, 0, 0], "half_size": [w / 2, 0.03, l / 2]})
        seams = []
        for edge, c, hs in (("fore", (cx, y - 0.05, z0), (w / 2, 0.05, 0.06)), ("aft", (cx, y - 0.05, z1), (w / 2, 0.05, 0.06)),
                            ("left", (x0, y - 0.05, cz), (0.06, 0.05, l / 2)), ("right", (x1, y - 0.05, cz), (0.06, 0.05, l / 2))):
            sid = f"{name}.seam_{edge}"
            seams.append(sid)
            vols.append({"id": sid, "part": name, "role": "seam", "shape": "box", "position": list(c), "rotation_deg": [0, 0, 0],
                         "half_size": list(hs)})
        rules["plates"].append({"plate": name, "seams": seams, "rule": "all seams destroyed -> the plate is free (can be torn off)"})
    for name, folder, R, pos in INSTALLS:
        hp = os.path.join(EQ, folder, "hit_volumes.json")
        if not os.path.exists(hp):
            continue
        lo, hi = [1e9] * 3, [-1e9] * 3
        for v in json.load(open(hp, encoding="utf-8"))["volumes"]:
            q = dict(v)
            q["id"] = f"{name}.{v['id']}"
            q["part"] = name
            q["role"] = "equipment"
            q["module_part"] = v["part"]
            q["position"] = [a + b for a, b in zip(apply(R, v["position"]), pos)]
            q["rotation_deg"] = _to_euler_xyz(_mat(R, _euler_xyz(v["rotation_deg"])))
            ext = v.get("half_size") or [v.get("radius", 0.0), v.get("half_height", 0.0), v.get("radius", 0.0)]
            RW = _mat(R, _euler_xyz(v["rotation_deg"]))
            for cx in (-1, 1):                                   # true corners of the volume's local box
                for cy in (-1, 1):
                    for cz in (-1, 1):
                        c = apply(RW, (cx * ext[0], cy * ext[1], cz * ext[2]))
                        for k in range(3):
                            lo[k] = min(lo[k], q["position"][k] + c[k])
                            hi[k] = max(hi[k], q["position"][k] + c[k])
            vols.append(q)
        # mount points: four small blocks at the base corners of the module (its mounting side = lowest Y here)
        mounts = []
        ins = 0.18 * min(hi[0] - lo[0], hi[2] - lo[2])
        for i, (sx, sz) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
            c = [((lo[0] + hi[0]) / 2) + sx * ((hi[0] - lo[0]) / 2 - ins), lo[1] + 0.12, ((lo[2] + hi[2]) / 2) + sz * ((hi[2] - lo[2]) / 2 - ins)]
            mid = f"{name}.mount_{i + 1}"
            mounts.append(mid)
            vols.append({"id": mid, "part": name, "role": "mount", "shape": "box", "position": c, "rotation_deg": [0, 0, 0],
                         "half_size": [0.15, 0.12, 0.15]})
        rules["modules"].append({"module": name, "equipment": folder, "mounts": mounts,
                                 "rule": "all mounts destroyed -> the module is free (flies apart from the ship)"})
    for v in vols:
        for k in ("position", "half_size"):
            if k in v:
                v[k] = [round(a, 4) + 0.0 for a in v[k]]
    return vols, rules


def main():
    names = sys.argv[1:] or list(SHIPS)
    for name in names:
        P = SHIPS[name]()
        d = os.path.join(OUT, name, "LOD0")
        os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            os.remove(os.path.join(d, f))
        tri = 0
        for pname, part in P.items():
            open(os.path.join(d, pname + ".obj"), "w", encoding="utf-8", newline="\n").write(obj_text(part, pname + ".mtl"))
            open(os.path.join(d, pname + ".mtl"), "w", encoding="utf-8", newline="\n").write(mtl_text(part.faces.keys()))
            tri += part.tris()
        import json
        vols, rules = ship_hits()
        base = os.path.join(OUT, name)
        open(os.path.join(base, "hit_volumes.json"), "w", encoding="utf-8", newline=chr(10)).write(json.dumps(
            {"ship": name, "frame": "metres, +Y up, -Z forward; rotation_deg T*Rx*Ry*Rz", "volumes": vols}, ensure_ascii=False, indent=1))
        open(os.path.join(base, "ship_structure.json"), "w", encoding="utf-8", newline=chr(10)).write(json.dumps(rules, ensure_ascii=False, indent=1))
        print(f"{name}: {len(P)} parts, {tri} triangles, {len(vols)} hit volumes ({len(rules['plates'])} plates, "
              f"{len(rules['modules'])} modules with mounts) -> {d}")
        INSTALLS.clear()
        PLATES.clear()


if __name__ == "__main__":
    main()
