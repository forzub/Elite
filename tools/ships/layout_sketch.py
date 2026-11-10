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
from eqlib import (Part, box, cyl, lathe, crect, rot_y, rot_z, apply, norm, obj_text, mtl_text, MATERIALS,  # noqa: E402
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

EQ = os.path.join(ROOT, "assets", "models", "equipment")
OUT = os.path.join(ROOT, "assets", "models", "layout_sketches")


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
    part = P[name] = Part(name)
    part.v = [tuple(a + b for a, b in zip(apply(R, v), pos)) for v in V]
    part.n = [norm(apply(R, n)) for n in N] + [(0, 1, 0)] * max(0, len(V) - len(N))
    part.faces = {m: [tuple(f) for f in fl] for m, fl in F.items()}
    return part


def quad(part, mat, pts, normal):
    ids = [part.vert(p, normal) for p in pts]
    part.face(mat, ids, normal)


def plate(P, name, mat, x0, x1, z0, z1, y, up=True):
    """a zero-thickness horizontal plate (radiator cover / hatch)"""
    part = P.setdefault(name, Part(name))
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
def cobrett_i():
    P = {}
    # hull: wedge nose (escape cockpit), body x +-13.5 is 5 m thick (the drive bays are dia 5), thin wings to x +-17
    outline = [(-3.0, -16.0), (3.0, -16.0), (6.5, -12.0), (13.5, -7.0), (17.5, 1.0), (17.5, 6.0), (13.5, 13.2),
               (-13.5, 13.2), (-17.5, 6.0), (-17.5, 1.0), (-13.5, -7.0), (-6.5, -12.0)]

    def thick(x, z):
        ax = abs(x)
        t = 2.5 if ax <= 13.5 else 2.5 - (ax - 13.5) * 0.45
        if z < -7.0:                       # the nose tapers into the cockpit
            t = min(t, 1.9 + (z + 16.0) * 0.07)
        return t
    hull_loft(P, outline, lambda x, z: thick(x, z), lambda x, z: -thick(x, z))

    # propulsion bays: fore CR-1 (pad forward) - ER-5 - aft CR-1 (pad aft), coaxial with Z at x = +-10.75
    for s, side in ((-1, "l"), (1, "r")):
        x = s * 10.75
        place(P, f"eq_er5_{side}", "cobretty_er5_reactor", (x, 0.0, 3.5))
        place(P, f"eq_cr1_aft_{side}", "cobretty_cr1_vector", (x, 0.0, 6.0))            # flange at the ER-5 aft face
        place(P, f"eq_cr1_fore_{side}", "cobretty_cr1_vector", (x, 0.0, 1.0), 180.0)    # turned to face forward
    # escape cockpit (the Cobretty escape system is the cockpit itself with a motor - proxy until the new model)
    ck = P["proxy_escape_cockpit"] = Part("proxy_escape_cockpit")
    box(ck, "sk_proxy", (0.0, 0.1, -12.0), (8.6, 3.4, 7.6))
    quad(ck, "sk_glass", [(-3.6, 1.0, -15.82), (3.6, 1.0, -15.82), (3.0, 1.75, -14.2), (-3.0, 1.75, -14.2)], (0, 0.7, -0.7))
    box(ck, "sk_proxy", (0.0, -0.4, -7.9), (2.2, 1.6, 0.8))                               # its escape motor
    # spine: jump core, tanks, service reactor, coolant loop
    place(P, "eq_core", "zeropoint_z05_jump_core", (0.0, -1.0, -6.6))
    for i, z in enumerate((-4.4, -2.6, -0.8)):
        place(P, f"eq_jumpcharge_{i}", "agora_bz1_jumptank", (0.0, -1.2, z))
    for i, z in enumerate((1.2, 2.9)):
        place(P, f"eq_pellets_{i}", "agora_bt1_pellettank", (0.0, -1.3, z))
    place(P, "eq_apu", "cobretty_sr150_service_reactor", (0.0, -1.0, 6.2))
    place(P, "eq_coolant", "cobretty_cl180_coolant_loop", (0.0, -1.8, 10.0))
    # avionics and life support between the cockpit and the cargo deck
    place(P, "eq_computer", "xinzhi_mcc1_computer", (-4.2, -1.6, -7.2))
    place(P, "eq_comm", "xinzhi_c1_comm", (-4.2, -0.6, -6.0))
    place(P, "eq_transponder", "zeropoint_t1_transponder", (-2.6, -1.8, -7.2))
    place(P, "eq_radar", "xinzhi_n2_radar", (0.0, 2.5, -9.4))
    place(P, "eq_life_support", "eurohab_l4_lifesupport", (4.3, -1.7, -6.6))
    place(P, "eq_dock", "agora_su1_collar", (0.0, 2.5, -4.0))
    place(P, "eq_pd1", "cobretty_pd1_turret", (0.0, 2.5, 4.0))
    # cargo deck: 6 C20 in two L-shaped groups (author's prototype topology), on the hull floor, ventral hatches below
    k = 0
    for s in (-1, 1):
        for x, z in ((s * 2.75, -2.9), (s * 2.75, 3.3), (s * 5.3, 3.3)):
            c = P[f"c20_{k}"] = Part(f"c20_{k}")
            box(c, "sk_c20", (x, -1.15, z + 3.03), (2.44, 2.59, 6.06))
            plate(P, f"hatch_{k}", "sk_hatch", x - 1.25, x + 1.25, z - 0.05, z + 6.11, -2.501, up=False)
            k += 1
    # drone bay at the stern between the cargo groups and the bays: repair drone left, recon drone right
    place(P, "eq_repair_drone", "agora_rm2_drone", (-4.0, -1.8, 11.0), 0.0)
    place(P, "eq_recon_drone", "hyperion_rd1_drone", (4.0, -1.6, 11.2))
    # weapons: two forward lasers in the cockpit shoulders, two 2-missile racks in the wings outboard of the bays
    for s in (-1, 1):
        wl = P[f"proxy_laser_{'l' if s < 0 else 'r'}"] = Part("proxy_laser")
        cyl(wl, "sk_proxy", (s * 6.2, 0.0, -6.5), (s * 6.2, 0.0, -12.6), 0.32, 12)
        mr = P[f"proxy_missiles_{'l' if s < 0 else 'r'}"] = Part("proxy_missiles")
        box(mr, "sk_proxy", (s * 15.0, 0.0, 2.6), (1.4, 0.8, 4.2))
        for dx in (-0.35, 0.35):
            cyl(mr, "sk_c20", (s * 15.0 + dx, 0.0, 0.6), (s * 15.0 + dx, 0.0, 4.6), 0.16, 10)
    # inspection shaft along the spine above the tanks, cross passages to the bays over the cargo deck, ladders, rails
    sh = P["shaft_spine"] = Part("shaft_spine")
    for z0, z1 in ((-7.6, 12.6),):
        for x in (-0.65, 0.65):                       # handrails
            cyl(sh, "sk_shaft", (x, 1.6, z0), (x, 1.6, z1), 0.035, 6)
        for zz in [z0 + i * 0.5 for i in range(int((z1 - z0) / 0.5) + 1)]:   # walkway grating ribs
            box(sh, "sk_shaft", (0, 0.55, zz), (1.4, 0.04, 0.06))
        box(sh, "sk_shaft", (0, 0.5, (z0 + z1) / 2), (0.06, 0.04, z1 - z0))
    for z in (-1.0, 9.0):                                # cross passages
        for zz in (z - 0.65, z + 0.65):
            cyl(sh, "sk_shaft", (-8.2, 1.6, zz), (8.2, 1.6, zz), 0.035, 6)
        box(sh, "sk_shaft", (0, 0.5, z), (16.4, 0.04, 1.4))
    for x, z in ((0.0, -7.4), (-8.0, 9.0), (8.0, 9.0), (0.0, 12.4)):      # ladders down to the deck
        for dx in (-0.25, 0.25):
            cyl(sh, "sk_shaft", (x + dx, -2.4, z), (x + dx, 0.5, z), 0.03, 6)
        for i in range(6):
            cyl(sh, "sk_shaft", (x - 0.25, -2.2 + i * 0.5, z), (x + 0.25, -2.2 + i * 0.5, z), 0.02, 6)
    # radiator cover plates (Cobretty CS-panels): one per compartment field, dorsal and ventral, zero thickness;
    # over the bays the ER-5 pads are the skin
    k = 0
    for x0, x1 in ((-8.0, -4.0), (-4.0, -1.4), (-1.4, 1.4), (1.4, 4.0), (4.0, 8.0)):
        for z0, z1 in ((-7.6, -3.0), (-3.0, 3.2), (3.2, 9.4), (9.4, 13.2)):
            plate(P, f"cover_{k}", "sk_cover", x0 + 0.04, x1 - 0.04, z0 + 0.04, z1 - 0.04, 2.501)
            k += 1
    # folding landing skids (deployed): one under the cockpit, two under each bay
    for x, z in ((0.0, -10.5), (-10.75, -3.0), (10.75, -3.0), (-10.75, 9.0), (10.75, 9.0)):
        sk = P.setdefault("skid_set", Part("skid_set"))
        cyl(sk, "sk_skid", (x, -2.5, z), (x, -4.1, z + 0.6), 0.16, 10)
        box(sk, "sk_skid", (x, -4.15, z + 0.6), (0.5, 0.1, 2.2))
    return P


SHIPS = {"cobrett-i": cobrett_i}


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
        print(f"{name}: {len(P)} parts, {tri} triangles -> {d}")


if __name__ == "__main__":
    main()
