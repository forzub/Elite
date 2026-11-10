"""Arsenal «Bastion» kamikaze drones (author 2026-11-10: big military ships carry bays of kamikaze drones, like modern
FPV drones). Doctrine (docs/SHIP_ARCHITECTURE_DOCTRINE.md): no fragmentation - the warhead is a burn-through plasma
lance that the drone presses against a cover-plate seam, a pad or a hatch and burns the compartment's insides.
Bastion school: faceted armour grey / dark / hazard yellow, no exposed cables.
  bastion_klegg3_drone   - MJ-K3 Klegg (nb "horsefly") FPV-style kamikaze drone, ~1.1 m, 4 field-thruster rings, plasma lance nose
  bastion_bk24_cassette - MJ-BK24 Bikube (nb "beehive") armoured cassette: 24 vertical cells (drones stowed nose-up, arms folded), 6 sliding lids,
                          launch control; hull-skin unit: launches along +Y, mounting face at y = 0
Names follow the naming canon (tools/colonization_lab/canon/ship_naming.json: Bastion = Norwegian bokmal,
blueprint prefix MJ from the Mjolner yard). ACCEPTED (author 2026-11-10) - passports in tools/colonization_lab/canon/ship_parts_unified.json.
"""
import math

from eqlib import *  # noqa: F401,F403

register_materials({
    "eq_bastion_armour": ((0.22, 0.25, 0.28), (0.22, 0.22, 0.22), 45),
    "eq_bastion_dark": ((0.12, 0.13, 0.15), (0.18, 0.18, 0.18), 35),
    "eq_bastion_hazard": ((0.75, 0.62, 0.12), (0.20, 0.20, 0.20), 30),
})
ARM, DRK, HAZ, STL = "eq_bastion_armour", "eq_bastion_dark", "eq_bastion_hazard", "eq_steel"


def klegg_parts(lod):
    """flight pose, nose (the lance) towards -Z"""
    small = small_for(lod)
    P = {}
    b = P["klegg_body"] = Part("klegg_body")
    lathe(b, ARM, [(0.0, -0.45), (0.13, -0.40), (0.13, 0.30), (0.08, 0.45), (0.0, 0.47)], 6, I3, (0, 0, 0), smooth=False)
    if lod < 2:
        lathe(b, HAZ, crect(0.13, 0.135, -0.05, 0.03), 6, I3, (0, 0, 0), smooth=False)          # hazard band
        box(b, DRK, (0.0, 0.10, 0.05), (0.10, 0.06, 0.40), chamfer=0.01)                         # spine (armoured duct)
    w = P["klegg_warhead"] = Part("klegg_warhead")
    lathe(w, DRK, [(0.0, -0.80), (0.05, -0.74), (0.11, -0.48), (0.11, -0.44), (0.0, -0.44)], 6 if lod else 8, I3, (0, 0, 0), smooth=False)
    if lod < 2:
        lathe(w, HAZ, crect(0.105, 0.115, -0.52, -0.47), 6, I3, (0, 0, 0), smooth=False)
        cyl(w, STL, (0, 0, -0.80), (0, 0, -0.86), 0.012, 6)                                       # lance tip
    t = P["klegg_thrusters"] = Part("klegg_thrusters")
    for sx in (-1, 1):
        for sz in (-1, 1):
            c = (sx * 0.42, 0.0, sz * 0.38)
            box(t, ARM, (sx * 0.22, 0.0, sz * 0.20), (0.40, 0.035, 0.05), rot_y(-sx * sz * math.pi / 4))   # arm
            lathe(t, DRK, crect(0.10, 0.15, -0.025, 0.025), small, R_Y, c)                       # field ring
            if lod < 2:
                lathe(t, "eq_pad_ceramic", [(0.0, -0.01), (0.10, -0.01), (0.10, 0.0), (0.0, 0.0)], small, R_Y, c)
                if close():
                    for k in range(3):                                                           # ring clamps
                        a = 2 * math.pi * k / 3
                        box(t, STL, (c[0] + 0.15 * math.cos(a), 0.0, c[2] + 0.15 * math.sin(a)), (0.03, 0.06, 0.03))
    if lod < 2:
        cam = P["klegg_camera"] = Part("klegg_camera")
        lathe(cam, DRK, [(0.0, -0.05), (0.05, -0.04), (0.05, 0.03), (0.0, 0.05)], small, R_Y, (0.0, -0.13, -0.32))
        lathe(cam, "eq_srw_coil" if "eq_srw_coil" in MATERIALS else DRK, [(0.0, 0.0), (0.025, 0.0), (0.0, 0.01)], small,
              axis_to((0, 0, -1)), (0.0, -0.14, -0.37))
        pw = P["klegg_power"] = Part("klegg_power")
        box(pw, ARM, (0.0, -0.17, 0.10), (0.16, 0.08, 0.34), chamfer=0.015)
        an = P["klegg_antenna"] = Part("klegg_antenna")
        cyl(an, DRK, (0.0, 0.08, 0.42), (0.0, 0.30, 0.55), 0.008, 6)
    return P


def klegg_hits():
    H = [hbox("klegg_body", (0, 0, 0.0), (0.26, 0.26, 0.92)),
         hcyl("klegg_warhead", (0, 0, -0.86), (0, 0, -0.44), 0.11),
         hbox("klegg_power", (0.0, -0.17, 0.10), (0.16, 0.08, 0.34))]
    for sx in (-1, 1):
        for sz in (-1, 1):
            H.append(hcyl("klegg_thrusters", (sx * 0.42, -0.03, sz * 0.38), (sx * 0.42, 0.03, sz * 0.38), 0.15))
    return H


BK = dict(cols=4, rows=6, cell=0.46, wall=0.04, depth=1.35, base=0.10)


def bk24_parts(lod):
    d = BK
    cw = d["cell"] + d["wall"]
    W, L = d["cols"] * cw + d["wall"], d["rows"] * cw + d["wall"]
    y0, y1 = d["base"], d["base"] + d["depth"]
    P = {}
    f = P["bk24_frame"] = Part("bk24_frame")
    for s in (-1, 1):                                                                # sloped armour skirts
        box(f, ARM, (s * (W / 2 + 0.06), (y0 + y1) / 2, 0.0), (0.12, y1 - y0, L + 0.24), chamfer=0.03 if lod == 0 else 0.0)
        box(f, ARM, (0.0, (y0 + y1) / 2, s * (L / 2 + 0.06)), (W, y1 - y0, 0.12), chamfer=0.03 if lod == 0 else 0.0)
    box(f, DRK, (0.0, y0 / 2, 0.0), (W + 0.24, y0, L + 0.24))                         # base plate
    if lod < 2:
        for i in range(1, d["cols"]):                                                # cell walls
            box(f, DRK, (-W / 2 + i * cw, (y0 + y1) / 2, 0.0), (d["wall"], y1 - y0, L))
        for j in range(1, d["rows"]):
            box(f, DRK, (0.0, (y0 + y1) / 2, -L / 2 + j * cw), (W, y1 - y0, d["wall"]))
        for s in (-1, 1):                                                            # hazard stripes on the skirts
            box(f, HAZ, (s * (W / 2 + 0.125), y1 - 0.12, 0.0), (0.005, 0.06, L))
    lids = P["bk24_lids"] = Part("bk24_lids")
    for j in range(d["rows"]):
        z = -L / 2 + (j + 0.5) * cw
        if j % 3 == 2:                                                               # one row shown slid open
            box(lids, ARM, (W / 2 - 0.05, y1 + 0.03, z), (W * 0.35, 0.05, cw - 0.03), chamfer=0.01 if lod == 0 else 0.0)
            continue
        box(lids, ARM, (0.0, y1 + 0.03, z), (W - 0.02, 0.05, cw - 0.03), chamfer=0.01 if lod == 0 else 0.0)
        if lod == 0:
            box(lids, HAZ, (0.0, y1 + 0.057, z), (W * 0.6, 0.004, 0.05))
    if lod < 2:                                                                      # stowed drones: lance tips up
        dr = P["bk24_drones"] = Part("bk24_drones")
        for i in range(d["cols"]):
            for j in range(d["rows"]):
                x, z = -W / 2 + (i + 0.5) * cw, -L / 2 + (j + 0.5) * cw
                lathe(dr, DRK, [(0.0, y1 - 0.75), (0.11, y1 - 0.70), (0.11, y1 - 0.22), (0.04, y1 - 0.08), (0.0, y1 - 0.06)],
                      6, R_Y, (x, 0.0, z), smooth=False)
                if lod == 0:
                    lathe(dr, HAZ, crect(0.11, 0.115, y1 - 0.30, y1 - 0.25), 6, R_Y, (x, 0.0, z), smooth=False)
    c = P["bk24_launch_ctl"] = Part("bk24_launch_ctl")
    box(c, ARM, (0.0, y0 + 0.30, L / 2 + 0.30), (W * 0.7, 0.50, 0.36), chamfer=0.04 if lod == 0 else 0.0)
    if lod == 0:
        box(c, HAZ, (0.0, y0 + 0.50, L / 2 + 0.485), (W * 0.5, 0.05, 0.01))
        box(c, DRK, (0.25, y0 + 0.30, L / 2 + 0.485), (0.22, 0.14, 0.012), chamfer=0.006)          # locked hatch
    m = P["bk24_mount"] = Part("bk24_mount")
    for sx in (-1, 1):
        for sz in (-1, 1):
            lathe(m, DRK, [(0.0, -0.06), (0.16, -0.06), (0.16, 0.0), (0.0, 0.0)], 6, R_Y, (sx * (W / 2 - 0.1), 0.0, sz * (L / 2 - 0.1)),
                  smooth=False)                                                      # hex toothed lugs
            if lod < 2:
                for k in range(6):
                    a = 2 * math.pi * k / 6
                    box(m, ARM, (sx * (W / 2 - 0.1) + 0.17 * math.cos(a), -0.03, sz * (L / 2 - 0.1) + 0.17 * math.sin(a)),
                        (0.05, 0.06, 0.05))
    return P


def bk24_hits():
    d = BK
    cw = d["cell"] + d["wall"]
    W, L = d["cols"] * cw + d["wall"], d["rows"] * cw + d["wall"]
    y1 = d["base"] + d["depth"]
    return [hbox("bk24_frame", (0.0, y1 / 2, 0.0), (W + 0.24, y1, L + 0.24)),
            hbox("bk24_lids", (0.0, y1 + 0.03, 0.0), (W, 0.06, L)),
            hbox("bk24_launch_ctl", (0.0, d["base"] + 0.30, L / 2 + 0.30), (W * 0.7, 0.50, 0.36)),
            hbox("bk24_mount", (0.0, -0.03, 0.0), (W, 0.06, L))]


ASSETS = {"bastion_klegg3_drone": klegg_parts, "bastion_bk24_cassette": bk24_parts}
HITS = {"bastion_klegg3_drone": klegg_hits, "bastion_bk24_cassette": bk24_hits}
MODEL_ASSETS = {"bastion.drone.klegg3": "bastion_klegg3_drone", "bastion.bay.bk24": "bastion_bk24_cassette"}
