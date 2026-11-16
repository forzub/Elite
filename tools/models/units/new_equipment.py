"""New equipment (author request 2026-11-10, ACCEPTED by the author 2026-11-10 - passports in tools/colonization_lab/canon/ship_parts_unified.json):
  Sol Mass Yards LM-MP8   light external missile rack (8 cells, blow-out covers, end reload hatch, launch-control unit);
  Hyperion Yards RD-1   recon drone (gimballed camera turret, deployable antenna, power block, 4 field-coupled thrusters,
                        3-point kinematic docking latch);
  Agora Modular RM-2 "Mantide"  repair drone on four magnetic feet: arms with hands that fold like a mantis's, a
                        binocular head with brow plates, no tail - a compact power pack with the main thruster behind the
                        thorax and gimballed thruster pods on the knees (author 2026-11-15); every limb segment / joint
                        is its own part with its pivot at a joint (RM2_RIG below; the rig goes into the proposal passport
                        and into intact/assembly/rig.json with test clips for the mesh editor's player).
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)
from units.drone_toolbox import carry_set, take_from_saddle, tb_saddle, toolbox_prop, widen

register_materials({
    # Sol Mass Yards (serial school)
    "eq_smy_mp8_regolith": ((0.55, 0.55, 0.53), (0.15, 0.15, 0.15), 30),
    "eq_smy_mp8_slate": ((0.28, 0.34, 0.42), (0.20, 0.20, 0.22), 40),
    "eq_smy_mp8_missile": ((0.72, 0.72, 0.68), (0.25, 0.25, 0.25), 50),
    # Hyperion Yards (light aerospace)
    "eq_hyp_rd1_foil": ((0.80, 0.62, 0.25), (0.70, 0.60, 0.35), 110),
    "eq_hyp_rd1_carbon": ((0.15, 0.15, 0.16), (0.30, 0.30, 0.32), 80),
    "eq_hyp_rd1_white": ((0.87, 0.88, 0.89), (0.25, 0.25, 0.25), 50),
    "eq_hyp_rd1_lens": ((0.03, 0.04, 0.07), (0.90, 0.90, 0.95), 200),
    # Agora Modular (patchwork open standard)
    "eq_agora_rm2_white": ((0.86, 0.84, 0.78), (0.20, 0.20, 0.20), 40),
    "eq_agora_rm2_terracotta": ((0.62, 0.33, 0.22), (0.15, 0.12, 0.10), 30),
    "eq_agora_rm2_sea": ((0.18, 0.36, 0.52), (0.25, 0.25, 0.30), 60),
    "eq_agora_rm2_brass": ((0.72, 0.56, 0.28), (0.65, 0.55, 0.35), 100),
    "eq_agora_rm2_lens": ((0.03, 0.04, 0.06), (0.90, 0.90, 0.95), 200),
    "eq_agora_rm2_iris": ((0.25, 0.66, 0.80), (0.60, 0.70, 0.75), 150),
})


# ---------------------------------------------------------------- local vector / frame helpers
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _lerp(a, b, t): return tuple(a[k] + (b[k] - a[k]) * t for k in range(3))


def frame_along(p0, p1, up=(0.0, 1.0, 0.0)):
    """rotation whose local Z runs p0 -> p1 and local Y leans towards `up` (columns = local axes)"""
    d = norm(_sub(p1, p0))
    if abs(_dot(d, norm(up))) > 0.97:
        up = (1.0, 0.0, 0.0) if abs(d[0]) < 0.9 else (0.0, 0.0, 1.0)
    x = norm(_cross(up, d))
    y = _cross(d, x)
    return [[x[0], y[0], d[0]], [x[1], y[1], d[1]], [x[2], y[2], d[2]]]


def euler_xyz(R):
    """degrees (a, b, c) with R = Rx(a) Ry(b) Rz(c) - the hit-volume rotation convention"""
    b = math.asin(max(-1.0, min(1.0, R[0][2])))
    a = math.atan2(-R[1][2], R[2][2])
    c = math.atan2(-R[0][1], R[0][0])
    return (math.degrees(a), math.degrees(b), math.degrees(c))


def seg_lathe(part, mat, p0, p1, prof, seg, up=(0.0, 1.0, 0.0), a0=0.0, **kw):
    """revolve an (r, s) profile about the axis p0 -> p1 (s = metres from p0)"""
    lathe(part, mat, prof, seg, frame_along(p0, p1, up), p0, a0, a0 + 2 * math.pi, **kw)


def obox(part, mat, p0, p1, w, h, up=(0.0, 1.0, 0.0), chamfer=0.0, ext=0.0):
    """a box from p0 to p1 (cross-section w x h, h along `up`), ext = extra length at both ends"""
    L = math.dist(p0, p1) + 2 * ext
    box(part, mat, _lerp(p0, p1, 0.5), (w, h, L), frame_along(p0, p1, up), chamfer=chamfer)


def hobox(part, p0, p1, w, h, up=(0.0, 1.0, 0.0), ext=0.0):
    R = frame_along(p0, p1, up)
    return hbox(part, _lerp(p0, p1, 0.5), (w, h, math.dist(p0, p1) + 2 * ext), euler_xyz(R))


def ball(part, mat, c, r, seg, R=I3):
    n = max(3, seg // 2)
    prof = [(r * math.sin(math.pi * i / n), -r * math.cos(math.pi * i / n)) for i in range(n + 1)]
    lathe(part, mat, prof, seg, R, c)


def knuckle(part, mat, c, axis, r, w, seg, cap_mat=None):
    """a joint knuckle: cylinder of width w about `axis` through c (optionally contrasting end caps)"""
    a = norm(axis)
    p0, p1 = _sub(c, _mul(a, w / 2)), _add(c, _mul(a, w / 2))
    cyl(part, mat, p0, p1, r, seg, chamfer=min(0.012, r * 0.2))
    if cap_mat and close():
        for p, s in ((p0, -1), (p1, 1)):
            lathe(part, cap_mat, [(0, 0), (r * 0.55, 0), (r * 0.55, 0.008), (0, 0.008)], seg, axis_to(_mul(a, s)), p)


def hex_limb(part, mat, p0, p1, r0, r1, chamf=0.012, a0=0.0):
    """a hexagonal limb segment (Agora's hexagon language) tapering from r0 at p0 to r1 at p1"""
    L = math.dist(p0, p1)
    c = chamf if chamf >= min_feature() else 0.0
    prof = [(0, 0), (r0 - c, 0), (r0, c), (r1, L - c), (r1 - c, L), (0, L)] if c else [(0, 0), (r0, 0), (r1, L), (0, L)]
    seg_lathe(part, mat, p0, p1, prof, 6, a0=a0, smooth=False)


def pivoted(src, pivot, R):
    """a copy of a part rotated by R about `pivot`"""
    return placed(src, R, _sub(pivot, apply(R, pivot)))


# ================================================================ 1. Sol Mass Yards LM-MP8 light missile rack
# Hull-skin weapon: mounting face y = 0, the cells fire outward (+Y). 2 x 4 square cells on a 0.21 m pitch,
# one missile per cell (separate parts mp8_missile_1..8 so a cell can be shown fired), one frangible blow-out cover
# per cell (mp8_hatch_1..8), an end-loading reload hatch on +Z, the launch-control unit on -Z, SMY rail mount.
PB8 = dict(pitch=0.21, ix=0.21, iz=0.42, wall=0.025, div=0.016, y_floor=0.08, y_top=1.25, rail_x=0.265, rail_z=0.58)
PB8_CELLS = [(sx * 0.105, z) for z in (-0.315, -0.105, 0.105, 0.315) for sx in (-1, 1)]


def mp8_parts(lod):
    d = PB8
    small = small_for(lod)
    ch = (lambda c: c) if lod == 0 else (lambda c: 0.0)
    P = {}
    yf, yt, ix, iz, w = d["y_floor"], d["y_top"], d["ix"], d["iz"], d["wall"]
    ym, hh = (yf + yt) / 2, yt - yf
    ox, oz = ix + w, iz + w

    # --- SMY rectangular rail mount with 4 twist pins
    m = P["mp8_rail_mount"] = Part("mp8_rail_mount")
    rx, rz = d["rail_x"], d["rail_z"]
    for s in (-1, 1):
        box(m, "eq_smy_mp8_slate", (s * rx, 0.025, 0), (0.05, 0.05, 2 * rz), chamfer=ch(0.01))
    for z in (-rz + 0.03, 0.0, rz - 0.03):
        box(m, "eq_smy_mp8_slate", (0, 0.02, z), (2 * rx - 0.05, 0.04, 0.06))
    for s in (-1, 1):
        for zs in (-1, 1):
            x, z = s * rx, zs * 0.52
            cyl(m, "eq_steel", (x, 0.08, z), (x, 0.11, z), 0.028, small)
            if lod < 2:
                box(m, "eq_steel", (x, 0.122, z), (0.10, 0.024, 0.03), chamfer=ch(0.006))       # twist lever
    if lod == 0:
        for s in (-1, 1):
            for i in range(9):
                hex_bolt(m, "eq_steel", (s * rx, 0.05, -rz + 0.07 + i * (2 * rz - 0.14) / 8), (0, 1, 0), 0.03, 0.015)
        box(m, "eq_smy_mp8_regolith", (0.12, 0.055, -rz + 0.03), (0.12, 0.03, 0.05), chamfer=0.005)  # power / data socket

    # --- housing: base flange with pin lugs, outer walls on a rib grid, cell dividers (dark)
    hs = P["mp8_housing"] = Part("mp8_housing")
    box(hs, "eq_smy_mp8_slate", (0, 0.065, 0), (2 * rx + 0.05, 0.03, 2 * oz + 0.02), chamfer=ch(0.008))
    for s in (-1, 1):
        for zs in (-1, 1):
            box(hs, "eq_smy_mp8_slate", (s * rx, 0.065, zs * 0.50), (0.07, 0.03, 0.08))           # pin lugs
    for s in (-1, 1):
        box(hs, "eq_smy_mp8_regolith", (s * (ix + w / 2), ym, 0), (w, hh, 2 * oz))
        box(hs, "eq_smy_mp8_regolith", (0, ym, s * (iz + w / 2)), (2 * ix, hh, w))
    if lod < 2:
        box(hs, "eq_metal_band", (0, ym, 0), (d["div"], hh, 2 * iz))
        for z in (-0.21, 0.0, 0.21):
            box(hs, "eq_metal_band", (0, ym, z), (2 * ix, hh, d["div"]))
        box(hs, "eq_metal_band", (0, yf + 0.004, 0), (2 * ix, 0.008, 2 * iz))                   # cell floor plate
        # rib grid: vertical ribs on the long walls, horizontal belts all round, corner posts, top rim
        for s in (-1, 1):
            for z in (-0.315, -0.105, 0.105, 0.315):
                box(hs, "eq_smy_mp8_regolith", (s * (ox + 0.01), ym, z), (0.02, hh, 0.03), chamfer=ch(0.005))
            for y in (0.42, 0.80):
                box(hs, "eq_smy_mp8_regolith", (s * (ox + 0.01), y, 0), (0.02, 0.03, 2 * oz), chamfer=ch(0.005))
            for zs in (-1, 1):
                box(hs, "eq_smy_mp8_slate", (s * (ox - 0.005), ym, zs * (oz - 0.005)), (0.05, hh, 0.05), chamfer=ch(0.008))
        for s in (-1, 1):
            box(hs, "eq_smy_mp8_slate", (s * (ox + 0.012), yt - 0.02, 0), (0.024, 0.04, 2 * oz + 0.05), chamfer=ch(0.006))
            box(hs, "eq_smy_mp8_slate", (0, yt - 0.02, s * (oz + 0.012)), (2 * ox, 0.04, 0.024), chamfer=ch(0.006))
    else:
        box(hs, "eq_metal_band", (0, yt - 0.01, 0), (2 * ix, 0.02, 2 * iz))                    # far: dark deck only
    if lod == 0:
        for s in (-1, 1):                                                                       # stencilled panels (relief)
            for z in (-0.21, 0.21):
                box(hs, "eq_smy_mp8_slate", (s * (ox + 0.003), 1.02, z), (0.006, 0.08, 0.14))
                box(hs, "eq_smy_mp8_regolith", (s * (ox + 0.006), 1.02, z), (0.004, 0.05, 0.10))
        for s in (-1, 1):
            for zs in (-1, 1):
                for k in range(3):
                    hex_bolt(hs, "eq_steel", (s * rx + (k - 1) * 0.0, 0.08, zs * 0.50 + (k - 1) * 0.022), (0, 1, 0), 0.016, 0.008)

    # --- blow-out covers (one per cell) and missiles
    for i, (cx, cz) in enumerate(PB8_CELLS):
        hc = P[f"mp8_hatch_{i + 1}"] = Part(f"mp8_hatch_{i + 1}")
        box(hc, "eq_smy_mp8_slate", (cx, yt + 0.011, cz), (0.198, 0.022, 0.198), chamfer=ch(0.006))
        if lod < 2:
            box(hc, "eq_smy_mp8_regolith", (cx, yt + 0.024, cz), (0.08, 0.006, 0.08))                # frangible centre
        if lod == 0:
            for a in (math.pi / 4, -math.pi / 4):                                                  # scoring lines
                box(hc, "eq_metal_band", (cx, yt + 0.0225, cz), (0.24, 0.004, 0.006), rot_y(a))
            for sx in (-1, 1):
                for sz in (-1, 1):
                    hex_bolt(hc, "eq_steel", (cx + sx * 0.082, yt + 0.022, cz + sz * 0.082), (0, 1, 0), 0.014, 0.006)

        ms = P[f"mp8_missile_{i + 1}"] = Part(f"mp8_missile_{i + 1}")
        t = (cx, 0, cz)
        ns = small if lod == 0 else 6
        ylathe(ms, "eq_metal_band", [(0.0, 0.10), (0.042, 0.10), (0.05, 0.14), (0.0, 0.14)], ns, t=t)                  # nozzle
        ylathe(ms, "eq_smy_mp8_missile", [(0.05, 0.14), (0.068, 0.14), (0.068, 0.94), (0.0, 0.94)], ns, t=t)
        ylathe(ms, "eq_smy_mp8_slate", [(0.068, 0.94), (0.062, 1.02), (0.048, 1.09), (0.026, 1.15), (0.0, 1.18)], ns, t=t, soft=35)
        if lod == 0:
            ylathe(ms, "eq_status_amber", crect(0.067, 0.071, 0.88, 0.91), ns, t=t)              # warhead band
            for k in range(4):                                                                      # tail fins (diagonal)
                a = math.pi / 4 + k * math.pi / 2
                box(ms, "eq_smy_mp8_slate", (cx + 0.094 * math.cos(a), 0.25, cz + 0.094 * math.sin(a)), (0.058, 0.20, 0.008), rot_y(-a))
        if lod == 0:
            for k in range(4):                                                                      # canards
                a = math.pi / 4 + k * math.pi / 2
                box(ms, "eq_smy_mp8_slate", (cx + 0.083 * math.cos(a), 0.80, cz + 0.083 * math.sin(a)), (0.03, 0.06, 0.006), rot_y(-a))

    # --- launch-control unit (-Z end) with the ribbed cable duct to the cell igniters
    ig = P["mp8_ignition"] = Part("mp8_ignition")
    zf = -(oz + 0.06)
    box(ig, "eq_smy_mp8_regolith", (0, 0.40, zf), (0.38, 0.46, 0.12), chamfer=ch(0.015))
    box(ig, "eq_smy_mp8_slate", (-0.12, 0.92, -(oz + 0.03)), (0.08, 0.60, 0.06), chamfer=ch(0.008))     # duct up to the deck
    if lod < 2:
        box(ig, "eq_smy_mp8_slate", (0.10, 0.12, -(oz + 0.03)), (0.10, 0.08, 0.06))                    # duct to the rail socket
        for x in (-0.11, 0.0, 0.11):
            box(ig, "eq_smy_mp8_slate", (x, 0.40, zf - 0.065), (0.04, 0.40, 0.012))                      # cover ribs
    if lod == 0:
        for k in range(9):                                                                          # duct ribs
            box(ig, "eq_smy_mp8_slate", (-0.12, 0.66 + k * 0.06, -(oz + 0.03)), (0.09, 0.012, 0.07))
        box(ig, "eq_status_amber", (0.14, 0.58, zf - 0.072), (0.04, 0.02, 0.006))
        for x in (0.05, 0.10, 0.15):
            cyl(ig, "eq_metal_band", (x, 0.26, zf - 0.06), (x, 0.26, zf - 0.10), 0.016, 8)          # connectors
        box(ig, "eq_smy_mp8_slate", (-0.06, 0.24, zf - 0.068), (0.12, 0.06, 0.004))                    # stencil plate
        for sx in (-1, 1):
            for y in (0.20, 0.60):
                hex_bolt(ig, "eq_steel", (sx * 0.17, y, zf - 0.06), (0, 0, -1), 0.016, 0.008)
        harness(ig, bezier((0.10, 0.17, zf - 0.04), (0.10, 0.10, zf - 0.08), (0.12, 0.06, -0.60), (0.12, 0.07, -d["rail_z"] + 0.03), 8),
                0.01, 2, ("eq_metal_band", "eq_insulator"), tie_every=3)

    # --- reload hatch (+Z end): hinged on +X, two latch handles, stiffeners
    rh = P["mp8_reload_hatch"] = Part("mp8_reload_hatch")
    zh = oz + 0.0125
    box(rh, "eq_smy_mp8_slate", (0, 0.665, zh), (0.40, 1.07, 0.025), chamfer=ch(0.006))
    if lod < 2:
        for y in (0.33, 0.665, 1.0):
            box(rh, "eq_smy_mp8_regolith", (0, y, zh + 0.02), (0.34, 0.03, 0.02), chamfer=ch(0.005))
        for y in (0.30, 1.03):
            cyl(rh, "eq_steel", (0.215, y - 0.06, zh + 0.01), (0.215, y + 0.06, zh + 0.01), 0.02, small)  # hinges
        for y in (0.50, 0.83):
            box(rh, "eq_steel", (-0.15, y, zh + 0.045), (0.03, 0.12, 0.02))                        # handles
            for yy in (y - 0.05, y + 0.05):
                box(rh, "eq_steel", (-0.15, yy, zh + 0.025), (0.025, 0.02, 0.025))
    if lod == 0:
        for y in (0.50, 0.83):
            box(rh, "eq_smy_mp8_regolith", (-0.185, y, zh + 0.016), (0.03, 0.05, 0.008))               # latch dogs
        box(rh, "eq_smy_mp8_regolith", (0.05, 0.16, zh + 0.0145), (0.18, 0.06, 0.004))                # stencil plate
    return P


def mp8_hits():
    d = PB8
    yf, yt, ix, iz, w = d["y_floor"], d["y_top"], d["ix"], d["iz"], d["wall"]
    ox, oz = ix + w, iz + w
    H = [hbox("mp8_rail_mount", (s * d["rail_x"], 0.04, 0), (0.08, 0.08, 2 * d["rail_z"])) for s in (-1, 1)]
    H += [hbox("mp8_rail_mount", (0, 0.02, z), (2 * d["rail_x"], 0.04, 0.06)) for z in (-d["rail_z"] + 0.03, d["rail_z"] - 0.03)]
    H += [hbox("mp8_housing", (0, 0.065, 0), (2 * d["rail_x"] + 0.05, 0.03, 2 * oz + 0.02)),
          hbox("mp8_housing", (0, (yf + yt) / 2, 0), (2 * ox + 0.04, yt - yf, 2 * oz))]
    for i, (cx, cz) in enumerate(PB8_CELLS):
        H.append(hbox(f"mp8_hatch_{i + 1}", (cx, yt + 0.012, cz), (0.198, 0.024, 0.198)))
        H.append(hcyl(f"mp8_missile_{i + 1}", (cx, 0.10, cz), (cx, 1.18, cz), 0.07))
    H += [hbox("mp8_ignition", (0, 0.40, -(oz + 0.06)), (0.38, 0.46, 0.12)),
          hbox("mp8_ignition", (-0.12, 0.92, -(oz + 0.03)), (0.08, 0.60, 0.06)),
          hbox("mp8_reload_hatch", (0, 0.665, oz + 0.03), (0.44, 1.07, 0.06))]
    return H


# ================================================================ 2. Hyperion Yards RD-1 recon drone
# Free flyer, long axis Z, nose +Z, docking latch on top (+Y). Origin = fuselage centre. Shown with the antenna
# deployed (stowed: folded forward along the spine, rd1_antenna hinge at RD1["hinge"]).
RD1 = dict(hinge=(0.0, 0.11, -0.44), ant_tip=(0.0, 0.47, -0.68), cam=(0.0, -0.26, 0.36), yaw=(0.0, -0.12, 0.36),
           pods=[("fl", (0.40, 0.0, 0.28)), ("fr", (-0.40, 0.0, 0.28)), ("rl", (0.40, 0.0, -0.30)), ("rr", (-0.40, 0.0, -0.30))])


RD1_PROFILE = [(0.09, -0.52), (0.118, -0.38), (0.13, -0.18), (0.13, 0.12), (0.118, 0.34), (0.095, 0.46)]


def _rd1_r(z):
    """fuselage radius at z on the foil section (linear between the profile points)"""
    for (r0, z0), (r1, z1) in zip(RD1_PROFILE, RD1_PROFILE[1:]):
        if z0 <= z <= z1:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return RD1_PROFILE[0][0] if z < RD1_PROFILE[0][1] else RD1_PROFILE[-1][0]


def rd1_parts(lod):
    d = RD1
    seg, small = seg_for(lod), small_for(lod)
    ch = (lambda c: c) if lod == 0 else (lambda c: 0.0)
    P = {}

    # --- fuselage: slender MLI-wrapped body, white nose radome, carbon tail cone
    b = P["rd1_body"] = Part("rd1_body")
    lathe(b, "eq_hyp_rd1_carbon", [(0, -0.66), (0.05, -0.62), (0.09, -0.52)], seg, soft=40)
    lathe(b, "eq_hyp_rd1_foil", RD1_PROFILE, seg, soft=40)
    lathe(b, "eq_hyp_rd1_white", [(0.095, 0.46), (0.07, 0.55), (0.04, 0.62), (0.0, 0.66)], seg, soft=40)
    if lod < 2:
        for z in (-0.30, 0.28):                                                                     # carbon strut collars
            lathe(b, "eq_hyp_rd1_carbon", crect(0.12, 0.142, z - 0.04, z + 0.04, ch(0.008)), seg)
    if close():
        for z in (-0.44, -0.12, 0.05, 0.40):                                                        # foil seams
            rz = _rd1_r(z)
            lathe(b, "eq_hyp_rd1_foil", crect(rz - 0.004, rz + 0.003, z - 0.003, z + 0.003), seg)
        for k in range(6):                                                                          # longitudinal stitch seams
            a = math.pi / 6 + k * math.pi / 3
            tube(b, "eq_hyp_rd1_foil", [polz(_rd1_r(z / 100) + 0.001, a, z / 100) for z in range(-48, 45, 4)], 0.003, 4)
        lathe(b, "eq_hyp_rd1_lens", [(0.0, 0.66), (0.012, 0.656), (0.0, 0.67)], 12)                     # nose star sensor

    # --- carbon strut frame to the four thruster pods
    fr = P["rd1_frame"] = Part("rd1_frame")
    for _, (px, py, pz) in d["pods"]:
        s = 1 if px > 0 else -1
        for dz in (-0.07, 0.07):
            cyl(fr, "eq_hyp_rd1_carbon", (s * 0.12, 0.0, pz + dz), (px - s * 0.09, py, pz + dz * 0.3), 0.014 if lod < 2 else 0.02, 6)
        if lod < 2:
            cyl(fr, "eq_hyp_rd1_carbon", (s * 0.135, 0.03, pz), (px - s * 0.09, py, pz), 0.009, 6)          # upper tie
    if close():
        for _, (px, py, pz) in d["pods"]:                                                           # cable spirals on the struts
            s = 1 if px > 0 else -1
            pts = []
            for i in range(41):
                t = i / 40
                x = s * 0.12 + (px - s * 0.09 - s * 0.12) * t
                a = t * 6 * math.pi
                pts.append((x, 0.018 * math.cos(a), pz + 0.07 * (1 - 0.7 * t) + 0.018 * math.sin(a)))
            tube(fr, "eq_insulator", pts, 0.003, 4)

    # --- field-coupled thrusters (no propellant): coil ring, ceramic emitter faces, guard ring
    for name, (px, py, pz) in d["pods"]:
        th = P[f"rd1_thruster_{name}"] = Part(f"rd1_thruster_{name}")
        t = (px, py, pz)
        ylathe(th, "eq_hyp_rd1_white", [(0.0, -0.03), (0.055, -0.03), (0.06, -0.02), (0.06, 0.02), (0.055, 0.03), (0.0, 0.03)], small * 2 if lod < 2 else small, t=t)
        ylathe(th, "eq_field_coil", crect(0.058, 0.082, -0.018, 0.018, ch(0.006)), small * 2 if lod < 2 else small, t=t)
        if lod < 2:
            for sy in (-1, 1):
                ylathe(th, "eq_pad_ceramic", [(0.045, sy * 0.032), (0.0, sy * 0.032)] if sy > 0 else [(0.0, -0.032), (0.045, -0.032)], small * 2, t=t)
            ylathe(th, "eq_hyp_rd1_carbon", crect(0.088, 0.096, -0.012, 0.012), small * 2, t=t)              # guard ring
        if lod == 0:
            for k in range(6):                                                                      # coil feed terminals
                a = k * math.pi / 3
                p = _add(t, polar(0.07, a, 0.018))
                cyl(th, "eq_copper", p, _add(p, (0, 0.008, 0)), 0.006, 6)

    # --- power block under the mid-body with a radiator fin array
    pw = P["rd1_power"] = Part("rd1_power")
    box(pw, "eq_hyp_rd1_white", (0, -0.155, -0.08), (0.16, 0.07, 0.34), chamfer=ch(0.015))
    box(pw, "eq_hyp_rd1_carbon", (0, -0.12, -0.08), (0.10, 0.03, 0.30))                                 # saddle
    if lod < 2:
        for k in range(5):
            box(pw, "eq_hyp_rd1_carbon", (-0.06 + 0.03 * k, -0.205, -0.08), (0.006, 0.03, 0.28))
    if lod == 0:
        box(pw, "eq_status_amber", (0.081, -0.15, 0.04), (0.003, 0.015, 0.03))
        for zz in (-0.22, 0.06):
            hex_bolt(pw, "eq_steel", (0.0, -0.19, zz), (0, -1, 0), 0.015, 0.006)

    # --- gimbal yoke (yaw about Y) and the camera turret (pitch about X)
    gy = P["rd1_gimbal"] = Part("rd1_gimbal")
    cx, cy, cz = d["cam"]
    cyl(gy, "eq_hyp_rd1_white", (0, -0.11, cz), (0, -0.18, cz), 0.03, small)
    box(gy, "eq_hyp_rd1_carbon", (0, -0.185, cz), (0.20, 0.018, 0.05), chamfer=ch(0.006))
    for s in (-1, 1):
        box(gy, "eq_hyp_rd1_carbon", (s * 0.092, -0.235, cz), (0.016, 0.10, 0.045), chamfer=ch(0.005))
    if lod < 2:
        lathe(gy, "eq_hyp_rd1_white", crect(0.0, 0.04, -0.005, 0.005), small, rot_x(-math.pi / 2), (0, -0.112, cz))  # bearing
    cm = P["rd1_camera"] = Part("rd1_camera")
    ball(cm, "eq_hyp_rd1_white", (cx, cy, cz), 0.07, small * 2 if lod < 2 else small)
    seg_lathe(cm, "eq_hyp_rd1_carbon", (cx, cy, cz + 0.05), (cx, cy, cz + 0.11), [(0.0, 0.0), (0.04, 0.0), (0.042, 0.05), (0.036, 0.06)], small * 2 if lod < 2 else small)
    seg_lathe(cm, "eq_hyp_rd1_lens", (cx, cy, cz + 0.105), (cx, cy, cz + 0.12), [(0.036, 0.0), (0.02, 0.008), (0.0, 0.01)], small * 2 if lod < 2 else small)
    for s in (-1, 1):
        cyl(cm, "eq_hyp_rd1_carbon", (s * 0.068, cy, cz), (s * 0.084, cy, cz), 0.018, small)              # trunnions
    if lod < 2:
        seg_lathe(cm, "eq_hyp_rd1_lens", (0.035, cy + 0.035, cz + 0.055), (0.035, cy + 0.035, cz + 0.075), [(0.0, 0.0), (0.015, 0.0), (0.015, 0.02), (0.0, 0.02)], small)  # IR / laser
    if lod == 0:
        seg_lathe(cm, "eq_hyp_rd1_white", (-0.035, cy + 0.035, cz + 0.05), (-0.035, cy + 0.035, cz + 0.07), [(0.0, 0.0), (0.011, 0.0), (0.011, 0.02), (0.0, 0.02)], small)

    # --- deployable antenna: hinge knuckle on the tail spine, carbon boom, patch dish with feed
    an = P["rd1_antenna"] = Part("rd1_antenna")
    hp, tp = d["hinge"], d["ant_tip"]
    cyl(an, "eq_hyp_rd1_carbon", (-0.03, hp[1], hp[2]), (0.03, hp[1], hp[2]), 0.02, small)
    cyl(an, "eq_hyp_rd1_carbon", hp, tp, 0.011 if lod < 2 else 0.02, 6)
    dish_y = tp[1]
    ylathe(an, "eq_hyp_rd1_white", [(0.0, dish_y - 0.005), (0.09, dish_y + 0.02), (0.085, dish_y + 0.028), (0.0, dish_y + 0.006)], small * 2 if lod < 2 else small, t=(0, 0, tp[2]))
    if lod < 2:
        cyl(an, "eq_hyp_rd1_carbon", (0, dish_y + 0.006, tp[2]), (0, dish_y + 0.07, tp[2]), 0.006, 6)
        ball(an, "eq_hyp_rd1_foil", (0, dish_y + 0.075, tp[2]), 0.012, 8)
    if close():
        pts = []
        for i in range(61):
            t = i / 60
            p = _lerp(hp, tp, t)
            a = t * 10 * math.pi
            pts.append((p[0] + 0.016 * math.cos(a), p[1] + 0.016 * math.sin(a) * 0.7, p[2] + 0.016 * math.sin(a) * 0.7))
        tube(an, "eq_insulator", pts, 0.003, 4)

    # --- docking latch (Hyperion 3-point kinematic mount): pedestal, triangle plate, 3 balls, centre latch eye
    dl = P["rd1_dock_latch"] = Part("rd1_dock_latch")
    box(dl, "eq_hyp_rd1_carbon", (0, 0.12, 0.0), (0.08, 0.05, 0.14))
    tri = [(0.0, 0.13), (0.11, -0.09), (-0.11, -0.09)]
    prism(dl, "eq_hyp_rd1_white", tri, 0.145, 0.16)
    if lod < 2:
        for x, z in inset(tri, 0.03):
            ball(dl, "eq_steel", (x, 0.178, z), 0.018, small)
        lathe(dl, "eq_steel", crect(0.012, 0.024, -0.008, 0.008), small, axis_to((1, 0, 0)), (0, 0.19, 0))
    return P


def rd1_hits():
    d = RD1
    cx, cy, cz = d["cam"]
    H = [hcyl("rd1_body", (0, 0, -0.66), (0, 0, 0.66), 0.125)]
    for _, (px, py, pz) in d["pods"]:
        s = 1 if px > 0 else -1
        H.append(hbox("rd1_frame", ((s * 0.12 + px - s * 0.09) / 2, 0.01, pz), (abs(px) - 0.21, 0.05, 0.16)))
    for name, (px, py, pz) in d["pods"]:
        H.append(hcyl(f"rd1_thruster_{name}", (px, py - 0.032, pz), (px, py + 0.032, pz), 0.096))
    H += [hbox("rd1_power", (0, -0.165, -0.08), (0.16, 0.09, 0.34)),
          hbox("rd1_gimbal", (0, -0.20, cz), (0.20, 0.13, 0.05)),
          hcyl("rd1_camera", (-0.084, cy, cz), (0.084, cy, cz), 0.07),
          hcyl("rd1_camera", (0, cy, cz), (0, cy, cz + 0.12), 0.042),
          hobox("rd1_antenna", d["hinge"], d["ant_tip"], 0.04, 0.04),
          hcyl("rd1_antenna", (0, d["ant_tip"][1] - 0.005, d["ant_tip"][2]), (0, d["ant_tip"][1] + 0.08, d["ant_tip"][2]), 0.09),
          hbox("rd1_dock_latch", (0, 0.155, 0.0), (0.22, 0.07, 0.22))]
    return H


# ================================================================ 3. Agora Modular RM-2 "Mantide" repair drone
# Walking / flying repair drone, a mantis on four magnetic feet. OBJ frame: feet on the hull at y = 0, head forward (+Z),
# +X = the drone's left (claw arm), -X = its right (plasma cutter). Everything that moves is its own part, modelled in the
# rest pose with its pivot at the joint it hangs from (RM2_RIG: part -> parent, pivot point, axis, suggested range).
W_, T_, S_, B_ = "eq_agora_rm2_white", "eq_agora_rm2_terracotta", "eq_agora_rm2_sea", "eq_agora_rm2_brass"
UPV = (0.0, 1.0, 0.0)
RM2 = dict(th_y=0.60, th_r=0.19, th_z0=-0.24, th_z1=0.22, pro0=(0.0, 0.63, 0.16), pro1=(0.0, 0.80, 0.55),
           neck=(0.0, 0.80, 0.56), head=(0.0, 0.85, 0.64),
           legs={"fl": (1, 0.10, (1.10, 0.60)), "fr": (-1, 0.10, (-1.10, 0.60)),
                 "rl": (1, -0.16, (1.08, -0.80)), "rr": (-1, -0.16, (-1.08, -0.80))})


def rm2_leg(key):
    """hip H (coxa yaw), C (femur pitch), K (knee), A (ankle), F (foot pad centre on the hull), pitch axis"""
    s, zh, (fx, fz) = RM2["legs"][key]
    H = (s * 0.175, 0.55, zh)
    u = norm((fx - H[0], 0.0, fz - H[2]))
    C = _add(H, _mul(u, 0.13))
    K = _add(_add(C, _mul(u, 0.52)), (0.0, 0.36, 0.0))
    A = (fx, 0.15, fz)
    F = (fx, 0.0, fz)
    ax = norm(_cross(u, UPV))
    return H, C, K, A, F, ax


def rm2_arm(s):
    """shoulder S, elbow E, wrist W, tool tip T of a raptorial arm (s = +1 claw / left, -1 cutter / right)"""
    return (s * 0.11, 0.75, 0.46), (s * 0.17, 0.47, 0.72), (s * 0.19, 0.76, 1.00), (s * 0.19, 0.60, 1.18)


RM2_TOOL = ((0.0, 0.67, 0.50), (0.0, 0.52, 0.61), (0.0, 0.56, 0.79), (0.0, 0.51, 0.91))   # base, elbow, wrist, tip
RM2_HEAD = (0.0, 0.90, 0.71)                                    # centre of the binocular head housing
RM2_EYES = [(s, (s * 0.078, 0.905, 0.785), norm((0.0, -0.06, 1.0))) for s in (1, -1)]   # barrel base on the front, axis
RM2_BROW = {s: (s * 0.078, 0.969, 0.808) for s in (1, -1)}      # brow plate centre (over each barrel)
RM2_BROW_PIVOT = {s: (s * 0.135, 0.969, 0.808) for s in (1, -1)}   # hinge at the outer end, axis (0, 0, -s): + = inner end up
RM2_PALM = 0.075
RM2_SADDLE = (0.0, 0.857, -0.03)                                # std.drone.toolbox saddle: castings' tops, box along Z


def rm2_knee_pod(key):
    """the gimballed field thruster on the knee (author 2026-11-15: «мелкие двигатели в суставы лап - тогда будет летать
    как квадрокоптер»): strut from the knee, pod just outboard, emitter down in the rest pose"""
    Hp, C, K, A, F, ax = rm2_leg(key)
    u = norm(_sub(K, C))
    uh = norm((u[0], 0.0, u[2]))
    return K, _add(_add(K, _mul(uh, 0.075)), (0.0, 0.035, 0.0)), ax


def ray_pose(H, C, K, A, target):
    """flight pose of a walking leg (author 2026-11-16: «для полета ноги надо складывать так, что бы вид был как у
    квадрокоптера - вытягивать в лучи под 45 град»): the coxa yaws the leg onto the plan direction `target`, the femur
    and the tibia straighten into one horizontal ray. Returns (coxa yaw, femur pitch, tibia, level) in degrees; `level`
    turns a tibia child (foot pad, knee thruster) back to its rest attitude (pad flat, emitter down)."""
    u = norm((K[0] - H[0], 0.0, K[2] - H[2]))
    t = norm(target)
    c = _cross(u, t)
    yaw = math.atan2(c[1], u[0] * t[0] + u[2] * t[2])
    phf = math.atan2(K[1] - C[1], (K[0] - C[0]) * u[0] + (K[2] - C[2]) * u[2])
    pht = math.atan2(A[1] - K[1], (A[0] - K[0]) * u[0] + (A[2] - K[2]) * u[2])
    return tuple(round(math.degrees(a), 1) for a in (yaw, -phf, phf - pht, pht))


RM2_PACK = dict(z0=-0.20, z1=-0.42, y=0.60, w=0.30, h=0.22)   # the power pack behind the thorax (instead of the tail)


def rm2_hand_axes(s):
    """the hand's direction (wrist -> fingertips) and its back normal (the palm faces -nup)"""
    S, E, Wp, T = rm2_arm(s)
    dw = norm(_sub(T, Wp))
    return dw, norm(_cross(dw, (1, 0, 0)))


def rm2_fingers(s):
    """(id, knuckle, middle joint, tip) of the two fingers and the thumb (thumb on the inner side, -s X)"""
    S, E, Wp, T = rm2_arm(s)
    dw, nup = rm2_hand_axes(s)
    out = []
    for fid, x in (("f1", 0.022), ("f2", -0.022)):
        k0 = _add(_add(Wp, _mul(dw, RM2_PALM)), (x, 0.0, 0.0))
        k1 = _add(k0, _mul(dw, 0.05))
        k2 = _add(_add(k1, _mul(dw, 0.04)), _mul(nup, -0.012))
        out.append((fid, k0, k1, k2))
    k0 = _add(_add(_add(Wp, _mul(dw, 0.03)), (-s * 0.045, 0.0, 0.0)), _mul(nup, -0.008))
    k1 = _add(_add(k0, _mul(dw, 0.035)), (-s * 0.012, 0.0, 0.0))
    k2 = _add(_add(k1, _mul(dw, 0.03)), _add((s * 0.008, 0.0, 0.0), _mul(nup, -0.01)))
    out.append(("thumb", k0, k1, k2))
    return out


def rm2_cutter_axis():
    """the plasma cutter along the outer side of the right forearm: mount point and the nozzle end"""
    S, E, Wp, T = rm2_arm(-1)
    fd = norm(_sub(Wp, E))
    c0 = _add(_lerp(E, Wp, 0.30), (-0.075, 0.0, 0.0))
    return c0, _add(c0, _mul(fd, 0.29))

def _pod(part, attach, c, axis, small, lod):
    """Agora field-coupled thruster pod: sea-blue strut, white housing, coil ring, ceramic emitter face"""
    a = norm(axis)
    cyl(part, S_, attach, c, 0.02 if lod < 2 else 0.03, 6)
    p0 = _sub(c, _mul(a, 0.03))
    p1 = _add(c, _mul(a, 0.03))
    seg_lathe(part, W_, p0, p1, [(0.0, 0.0), (0.06, 0.0), (0.072, 0.012), (0.072, 0.045), (0.0, 0.045)], small * 2 if lod == 0 else small)
    if lod == 0:
        seg_lathe(part, "eq_field_coil", p0, p1, crect(0.055, 0.078, 0.045, 0.058), small * 2)
    seg_lathe(part, "eq_pad_ceramic", p0, p1, [(0.055, 0.06), (0.0, 0.06)], small * 2 if lod == 0 else small)
    if lod == 0:
        seg_lathe(part, B_, p0, p1, crect(0.072, 0.078, 0.014, 0.03), small * 2)                  # brass adapter ring


def rm2_parts(lod):
    d = RM2
    seg, small = seg_for(lod), small_for(lod)
    ks = small                       # knuckle segments
    ch = (lambda c: c) if lod == 0 else (lambda c: 0.0)
    P = {}

    def part(name):
        return P.setdefault(name, Part(name))

    # --- thorax: hexagonal can (flat top), prothorax rising to the neck, patchwork panels, hip bosses
    th = part("rm2_thorax")
    y0, r = d["th_y"], d["th_r"]
    z0, z1 = d["th_z0"], d["th_z1"]
    c = 0.03 if lod < 2 else 0.0
    lathe(th, W_, [(0, z0), (r - c, z0), (r, z0 + c), (r, z1 - c), (r - c, z1), (0, z1)], 6, I3, (0, y0, 0), smooth=False)
    hex_limb(th, W_, d["pro0"], d["pro1"], 0.12, 0.085, chamf=0.02)
    if lod < 2:
        top = y0 + r * math.sin(math.pi / 3)
        box(th, T_, (0, top + 0.006, -0.03), (0.15, 0.012, 0.34), chamfer=ch(0.004))                # terracotta roof panel
        for s in (-1, 1):                                                                           # sea-blue flank panels
            n = (s * math.cos(math.pi / 6), math.sin(math.pi / 6), 0.0)
            pc = _add((0, y0, -0.06), _mul(n, r * math.cos(math.pi / 6) + 0.006))
            box(th, S_, pc, (0.12, 0.012, 0.26), frame_along(pc, _add(pc, (0, 0, 1)), n), chamfer=0.0)
        for t in (0.3, 0.7):                                                                        # prothorax bands
            p = _lerp(d["pro0"], d["pro1"], t)
            seg_lathe(th, S_, _sub(p, (0, 0.012, 0.03)), _add(p, (0, 0.012, 0.03)), crect(0.0, 0.128 - 0.035 * t, 0.0, 0.065), 6, smooth=False)
        for key in d["legs"]:                                                                       # hip bosses
            H = rm2_leg(key)[0]
            cyl(th, S_, (H[0] * 0.75, H[1], H[2]), (H[0] * 0.98, H[1], H[2]), 0.06, ks)
    if lod == 0:
        for s in (-1, 1):                                                                           # panel fasteners
            for zz in (-0.17, 0.11):
                hex_bolt(th, B_, (s * 0.06, y0 + r * math.sin(math.pi / 3) + 0.012, zz), (0, 1, 0), 0.02, 0.008)
        box(th, S_, (0.0, y0 - r * math.sin(math.pi / 3) - 0.006, -0.02), (0.14, 0.012, 0.30))      # belly plate
        box(th, "eq_status_amber", (0.0, y0 + r * math.sin(math.pi / 3) + 0.014, 0.15), (0.03, 0.006, 0.02))
    if close():
        for s in (-1, 1):                                                                           # harnesses to the hips / arms
            harness(th, bezier((s * 0.08, y0 + 0.05, -0.18), (s * 0.17, y0 + 0.02, -0.10), (s * 0.17, y0 + 0.0, 0.05), (s * 0.11, 0.70, 0.40), 12),
                    0.008, 2, (S_, "eq_insulator"), ties=B_, tie_every=4)

    # --- the std.drone.toolbox saddle on the thorax roof (the box rides along the body, clear of the shoulder pods)
    box(th, S_, (0, RM2_SADDLE[1] - 0.081, RM2_SADDLE[2]), (0.19, 0.03, 0.34), chamfer=ch(0.004))
    tb_saddle(th, RM2_SADDLE, lod, rotated=True, plate=S_)

    # --- field thrusters on the thorax shoulders
    tf = part("rm2_thrusters_fore")
    for s in (-1, 1):
        _pod(tf, (s * 0.12, 0.74, 0.0), (s * 0.27, 0.84, 0.0), (s * 0.5, 0.86, 0.0), small, lod)

    # --- neck (yaw) and head (pitch): a binocular sensor head - two camera barrels side by side in one housing, a movable
    #     brow plate over each (author 2026-11-15: «голова похожа на бинокль, собранныей в одном корпусе и над камерами -
    #     пластины как брови»); no feelers, no mandible - nothing insect-like left on the head
    nk = part("rm2_neck")
    seg_lathe(nk, S_, d["pro1"], d["head"], [(0.0, 0.0), (0.06, 0.0), (0.055, 0.09), (0.0, 0.09)], ks)
    if lod == 0:
        knuckle(nk, B_, d["head"], (1, 0, 0), 0.042, 0.13, ks, cap_mat=S_)
    hd = part("rm2_head")
    hc = RM2_HEAD
    box(hd, W_, hc, (0.30, 0.11, 0.15), chamfer=0.035 if lod < 2 else 0.0)                         # one housing
    box(hd, T_, (0, hc[1] - 0.07, hc[2] - 0.01), (0.10, 0.04, 0.10), chamfer=ch(0.012))             # chin block (lidar)
    if lod < 2:
        box(hd, S_, (0, hc[1] + 0.058, hc[2] - 0.025), (0.20, 0.012, 0.09), chamfer=ch(0.004))      # sea-blue top panel
        box(hd, "eq_agora_rm2_lens", (0, hc[1] - 0.07, hc[2] + 0.041), (0.06, 0.014, 0.004))       # lidar window
        for s in (-1, 1):
            box(hd, S_, (s * 0.153, hc[1], hc[2] - 0.01), (0.008, 0.08, 0.11), chamfer=0.0)       # cheek plates
            cyl(hd, B_, (s * 0.157, hc[1], hc[2] - 0.01), (s * 0.172, hc[1], hc[2] - 0.01), 0.022, ks)   # side focus knobs
    es = small * 2 if lod == 0 else small
    for s, ec, de in RM2_EYES:
        p1 = _add(ec, de)
        seg_lathe(hd, W_, ec, p1, [(0.0, 0.0), (0.052, 0.0), (0.052, 0.05), (0.047, 0.06)], es, soft=40)   # barrel
        if lod == 2:
            seg_lathe(hd, "eq_agora_rm2_iris", ec, p1, [(0.047, 0.06), (0.0, 0.075)], es)
            continue
        seg_lathe(hd, "eq_agora_rm2_lens", ec, p1, [(0.047, 0.06), (0.04, 0.07), (0.03, 0.077)], es, soft=50)
        seg_lathe(hd, "eq_agora_rm2_iris", ec, p1, [(0.03, 0.077), (0.019, 0.083), (0.011, 0.086)], es, soft=50)
        seg_lathe(hd, "eq_agora_rm2_lens", ec, p1, [(0.011, 0.086), (0.0, 0.088)], es, soft=50)
        if lod < 2:
            seg_lathe(hd, B_, ec, p1, crect(0.052, 0.058, 0.032, 0.044), es)                         # focus ring
    if lod == 0:
        for s in (-1, 1):
            hex_bolt(hd, B_, (s * 0.12, hc[1] - 0.035, hc[2] + 0.075), (0, 0, 1), 0.016, 0.006)
    for s, side in ((1, "l"), (-1, "r")):                                                             # the brows
        br = part(f"rm2_brow_{side}")
        c = RM2_BROW[s]
        box(br, T_, c, (0.115, 0.014, 0.055), chamfer=ch(0.004))
        if lod < 2:
            cyl(br, B_, _add(RM2_BROW_PIVOT[s], (0, 0, -0.025)), _add(RM2_BROW_PIVOT[s], (0, 0, 0.025)), 0.009, 8)   # hinge pin

    # --- legs: coxa (yaw) -> femur -> tibia -> foot with magnetic pad
    for key in d["legs"]:
        H, C, K, A, F, ax = rm2_leg(key)
        cx_ = part(f"rm2_leg_{key}_coxa")
        if lod == 0:
            knuckle(cx_, B_, H, UPV, 0.05, 0.11, ks, cap_mat=S_)
        hex_limb(cx_, S_, H, C, 0.045, 0.042)
        fm = part(f"rm2_leg_{key}_femur")
        if lod == 0:
            knuckle(fm, B_, C, ax, 0.046, 0.12, ks, cap_mat=W_)
        hex_limb(fm, W_, C, K, 0.042, 0.034, a0=math.pi / 6)
        if lod == 0:                                                                                 # actuator on the femur
            off = _mul(norm(_cross(ax, _sub(K, C))), 0.055)
            a0p = _add(_lerp(C, K, 0.12), off)
            a1p = _add(_lerp(C, K, 0.55), off)
            a2p = _add(_lerp(C, K, 0.86), off)
            cyl(fm, S_, a0p, a1p, 0.02, 8 if lod == 0 else 6)
            cyl(fm, B_, a1p, a2p, 0.011, 6)
            obox(fm, T_, _add(_lerp(C, K, 0.2), _mul(ax, 0.037)), _add(_lerp(C, K, 0.75), _mul(ax, 0.037)), 0.045, 0.006, up=ax)  # patch stripe
            obox(fm, T_, _add(_lerp(C, K, 0.2), _mul(ax, -0.037)), _add(_lerp(C, K, 0.75), _mul(ax, -0.037)), 0.045, 0.006, up=ax)
        tb = part(f"rm2_leg_{key}_tibia")
        if lod == 0:
            knuckle(tb, B_, K, ax, 0.04, 0.10, ks, cap_mat=T_)
        hex_limb(tb, T_, K, A, 0.034, 0.024, a0=math.pi / 6)
        ft = part(f"rm2_leg_{key}_foot")
        if lod == 0:
            knuckle(ft, B_, A, ax, 0.03, 0.08, ks, cap_mat=S_)
        cyl(ft, S_, A, _add(F, (0, 0.05, 0)), 0.022, 6)
        ylathe(ft, "eq_metal_band", [(0.0, 0.0), (0.09, 0.0), (0.09, 0.03), (0.065, 0.05), (0.0, 0.05)], small * 2 if lod == 0 else small, t=F)
        if lod < 2:
            ylathe(ft, S_, crect(0.06, 0.092, 0.03, 0.045), small * 2, t=F)
        if lod == 0:
            ylathe(ft, "eq_field_coil", crect(0.03, 0.05, 0.05, 0.058), small * 2, t=F)              # magnet coil
        if close():
            harness(tb, [_add(_lerp(K, A, t), _mul(norm(_cross(ax, _sub(A, K))), -0.03)) for t in [i / 8 for i in range(1, 8)]],
                    0.006, 1, ("eq_insulator",), ties=B_, tie_every=2)
            for t in (0.25, 0.75):
                p = _lerp(C, K, t)
                lathe(fm, B_, crect(0.036, 0.046, -0.008, 0.008), 6, frame_along(C, K), p, smooth=False)
        kp = part(f"rm2_leg_{key}_thruster")
        k0, kc, kax = rm2_knee_pod(key)
        _pod(kp, k0, kc, (0.0, -1.0, 0.0), small, lod)
        if lod == 0:
            knuckle(kp, B_, kc, kax, 0.02, 0.1, ks)                                                 # gimbal trunnion

    # --- arms (author 2026-11-15: «передние лапы ... больше похожие на руки. но в сложенном состоянии пусть будут как у
    #     богомола»): shoulder, upper arm, forearm, a hand with two fingers and a thumb on each; the modelled pose is the
    #     mantis fold (forearm folded up against the upper arm, hands tucked), the test clip unfolds them. The plasma
    #     cutter rides on the right forearm.
    for s, side in ((1, "l"), (-1, "r")):
        S, E, Wp, T = rm2_arm(s)
        sh = part(f"rm2_arm_{side}_shoulder")
        ball(sh, W_, S, 0.06, ks)
        if lod == 0:
            knuckle(sh, B_, S, (1, 0, 0), 0.045, 0.14, ks, cap_mat=T_)
        up_ = part(f"rm2_arm_{side}_upper")
        hex_limb(up_, W_, S, E, 0.045, 0.040)
        if lod < 2:                                                                                 # biceps actuator
            off = (0.0, 0.0, 0.0)
            dn = norm(_cross((1, 0, 0), _sub(E, S)))
            cyl(up_, S_, _add(_lerp(S, E, 0.18), _mul(dn, -0.05)), _add(_lerp(S, E, 0.62), _mul(dn, -0.05)), 0.018, 8)
            cyl(up_, B_, _add(_lerp(S, E, 0.62), _mul(dn, -0.05)), _add(_lerp(S, E, 0.88), _mul(dn, -0.05)), 0.010, 6)
        fo = part(f"rm2_arm_{side}_fore")
        if lod == 0:
            knuckle(fo, B_, E, (1, 0, 0), 0.044, 0.11, ks, cap_mat=S_)
        hex_limb(fo, T_, E, Wp, 0.044, 0.036, a0=math.pi / 6)
        if lod < 2:                                                                                 # forearm guard plate
            fd = norm(_sub(Wp, E))
            gn = norm(_cross((1, 0, 0), fd))
            obox(fo, W_, _add(_lerp(E, Wp, 0.18), _mul(gn, 0.042)), _add(_lerp(E, Wp, 0.85), _mul(gn, 0.038)), 0.07, 0.012, up=gn, chamfer=ch(0.004))
        # the hand: wrist knuckle + palm (rm2_hand_<side>), fingers f1 / f2 and the thumb, each its own part
        dw, nup = rm2_hand_axes(s)
        hn = part(f"rm2_hand_{side}")
        if lod == 0:
            knuckle(hn, B_, Wp, (1, 0, 0), 0.034, 0.09, ks, cap_mat=W_)
        obox(hn, W_, Wp, _add(Wp, _mul(dw, RM2_PALM)), 0.08, 0.03, up=nup, chamfer=ch(0.008))
        if lod < 2:
            box(hn, S_, _add(_add(Wp, _mul(dw, RM2_PALM * 0.55)), _mul(nup, 0.016)), (0.06, 0.006, 0.045), frame_along(Wp, _add(Wp, dw), nup))
        for fid, k0, k1, k2 in rm2_fingers(s):
            fg = part(f"rm2_hand_{side}_{fid}")
            r = 0.013 if fid != "thumb" else 0.014
            if lod == 0:
                knuckle(fg, B_, k0, (1, 0, 0) if fid != "thumb" else nup, r + 0.003, 0.024, 8)
            hex_limb(fg, W_ if fid != "thumb" else T_, k0, k1, r, r * 0.9)
            hex_limb(fg, T_ if fid != "thumb" else W_, k1, k2, r * 0.9, r * 0.75)
            if lod < 2:
                ball(fg, "eq_metal_band", k2, r * 0.8, 6)                                           # grip tip
        if s < 0:
            ct = part("rm2_cutter")
            c0, c1 = rm2_cutter_axis()
            fd = norm(_sub(c1, c0))
            seg_lathe(ct, W_, c0, c1, [(0.0, 0.0), (0.03, 0.0), (0.034, 0.02), (0.034, 0.2), (0.026, 0.23), (0.0, 0.23)], ks)
            seg_lathe(ct, "eq_copper", c0, c1, [(0.0, 0.23), (0.02, 0.23), (0.015, 0.275), (0.008, 0.29), (0.0, 0.29)], ks)
            for t in (0.06, 0.17):                                                                  # brackets to the forearm
                p = _add(c0, _mul(fd, t))
                q = _lerp(E, Wp, 0.30 + t * 1.6)
                obox(ct, S_, p, q, 0.03, 0.03, chamfer=ch(0.004))
            if lod < 2:
                seg_lathe(ct, S_, c0, c1, crect(0.034, 0.04, 0.09, 0.12), ks)                       # sea-blue band
                cyl(ct, T_, _add(_add(c0, _mul(fd, 0.03)), (-0.045, 0.0, 0.0)), _add(_add(c0, _mul(fd, 0.13)), (-0.045, 0.0, 0.0)), 0.018, 8, chamfer=ch(0.005))
            if close():
                tube(ct, B_, bezier(_add(_add(c0, _mul(fd, 0.13)), (-0.045, 0.0, 0.0)), _add(_add(c0, _mul(fd, 0.17)), (-0.05, 0.0, 0.0)),
                                    _add(_add(c0, _mul(fd, 0.16)), (-0.025, 0.0, 0.0)), _add(c0, _mul(fd, 0.165)), 8), 0.006, 6)

    # --- fine-work tool arm under the head: upper, fore, tool head (driver, micro-gripper, work lamp)
    b0, m1, m2, tp = RM2_TOOL
    tu = part("rm2_tool_upper")
    if lod == 0:
        knuckle(tu, B_, b0, (1, 0, 0), 0.028, 0.07, ks, cap_mat=S_)
    hex_limb(tu, S_, b0, m1, 0.026, 0.022)
    tfo = part("rm2_tool_fore")
    if lod == 0:
        knuckle(tfo, B_, m1, (1, 0, 0), 0.025, 0.065, ks, cap_mat=W_)
    hex_limb(tfo, W_, m1, m2, 0.022, 0.019, a0=math.pi / 6)
    tl = part("rm2_tool_head")
    if lod == 0:
        knuckle(tl, B_, m2, (1, 0, 0), 0.022, 0.06, ks)
    dt = norm(_sub(tp, m2))
    seg_lathe(tl, S_, m2, tp, [(0.0, 0.0), (0.03, 0.0), (0.032, 0.05), (0.022, 0.07), (0.0, 0.07)], ks)
    cyl(tl, "eq_steel", _add(m2, _mul(dt, 0.07)), tp, 0.008, 6)                                       # driver bit
    if lod < 2:
        nt = norm(_cross(dt, (1, 0, 0)))
        for sx in (-1, 1):                                                                           # micro-gripper fingers
            q0 = _add(_add(m2, _mul(dt, 0.06)), (sx * 0.02, 0.0, 0.0))
            cyl(tl, B_, q0, _add(_add(q0, _mul(dt, 0.045)), (-sx * 0.01, 0.0, 0.0)), 0.005, 4)
        lp = _add(_add(m2, _mul(dt, 0.04)), _mul(nt, 0.032))
        seg_lathe(tl, "eq_status_amber", lp, _add(lp, nt), [(0.0, 0.0), (0.012, 0.0), (0.012, 0.006), (0.0, 0.008)], 8)  # work lamp

    # --- no tail (author 2026-11-15: «убери у него хвост»): a compact power pack right behind the thorax carries the
    #     main field thruster (emitter aft, pushes the drone forward) and the bay castings on its top
    pk = part("rm2_engine")
    k = RM2_PACK
    zc, L = (k["z0"] + k["z1"]) / 2, k["z0"] - k["z1"]
    box(pk, S_, (0, k["y"], zc), (k["w"], k["h"], L), chamfer=0.04 if lod < 2 else 0.0)
    p0, p1 = (0, k["y"], k["z1"] + 0.01), (0, k["y"], k["z1"] - 1.0)
    seg_lathe(pk, W_, p0, p1, [(0.0, 0.0), (0.10, 0.0), (0.115, 0.02), (0.115, 0.075), (0.0, 0.075)], small * 2 if lod == 0 else small)
    seg_lathe(pk, "eq_pad_ceramic", p0, p1, [(0.098, 0.076), (0.0, 0.076)], small * 2 if lod == 0 else small)
    if lod < 2:
        seg_lathe(pk, "eq_field_coil", p0, p1, crect(0.115, 0.13, 0.03, 0.055), small * 2)                  # coil ring
        box(pk, T_, (0, k["y"] + k["h"] / 2 + 0.006, zc), (0.10, 0.012, L - 0.06), chamfer=ch(0.004))       # terracotta stripe
        for sx in (-1, 1):                                                                               # bay castings
            for z in (k["z0"] - 0.04, k["z1"] + 0.04):
                box(pk, B_, (sx * 0.11, k["y"] + k["h"] / 2 + 0.02, z), (0.05, 0.04, 0.05), chamfer=ch(0.006))
    if lod == 0:
        seg_lathe(pk, B_, p0, p1, crect(0.115, 0.122, 0.0, 0.012), small * 2)                              # brass adapter
        for sx in (-1, 1):
            box(pk, W_, (sx * (k["w"] / 2 + 0.003), k["y"], zc), (0.006, 0.12, 0.14))                       # placards
            box(pk, "eq_status_amber", (sx * (k["w"] / 2 + 0.006), k["y"] + 0.03, zc + 0.03), (0.004, 0.015, 0.04))
        cyl(pk, B_, (0.0, k["y"] - k["h"] / 2, zc), (0.0, k["y"] - k["h"] / 2 - 0.02, zc), 0.025, ks)   # charge / data socket
    if close():
        harness(pk, bezier((0.06, 0.66, -0.16), (0.08, 0.70, -0.20), (0.09, 0.71, -0.24), (0.09, 0.70, -0.28), 6),
                0.007, 2, (S_, "eq_insulator"), ties=B_, tie_every=3)
    return P

def rm2_hits():
    d = RM2
    y0, r = d["th_y"], d["th_r"]
    H = [hhex("rm2_thorax", (0, y0, (d["th_z0"] + d["th_z1"]) / 2), (r, r * math.sin(math.pi / 3), (d["th_z1"] - d["th_z0"]) / 2), r / 2),
         hobox("rm2_thorax", d["pro0"], d["pro1"], 0.22, 0.20),
         hobox("rm2_neck", d["pro1"], d["head"], 0.12, 0.12),
         hbox("rm2_head", RM2_HEAD, (0.30, 0.11, 0.15)),
         hbox("rm2_head", (0, RM2_HEAD[1] - 0.07, RM2_HEAD[2] - 0.01), (0.10, 0.04, 0.10))]
    for s, ec, de in RM2_EYES:
        H.append(hobox("rm2_head", ec, _add(ec, _mul(de, 0.088)), 0.11, 0.11))
    for s, side in ((1, "l"), (-1, "r")):
        H.append(hbox(f"rm2_brow_{side}", RM2_BROW[s], (0.115, 0.02, 0.055)))
    for s in (1, -1):
        H.append(hbox("rm2_thrusters_fore", (s * 0.25, 0.83, 0.0), (0.17, 0.12, 0.16)))
    for key in d["legs"]:
        Hp, C, K, A, F, ax = rm2_leg(key)
        H += [hobox(f"rm2_leg_{key}_coxa", Hp, C, 0.10, 0.10, ext=0.03),
              hobox(f"rm2_leg_{key}_femur", C, K, 0.10, 0.10, ext=0.02),
              hobox(f"rm2_leg_{key}_tibia", K, A, 0.08, 0.08, ext=0.02),
              hobox(f"rm2_leg_{key}_foot", A, _add(F, (0, 0.05, 0)), 0.06, 0.06),
              hcyl(f"rm2_leg_{key}_foot", F, _add(F, (0, 0.05, 0)), 0.09)]
    for s, side in ((1, "l"), (-1, "r")):
        S, E, Wp, T = rm2_arm(s)
        dw, nup = rm2_hand_axes(s)
        H += [hobox(f"rm2_arm_{side}_shoulder", _sub(S, (0.07, 0, 0)), _add(S, (0.07, 0, 0)), 0.12, 0.12),
              hobox(f"rm2_arm_{side}_upper", S, E, 0.10, 0.10),
              hobox(f"rm2_arm_{side}_fore", E, Wp, 0.10, 0.10, ext=0.02),
              hobox(f"rm2_hand_{side}", Wp, _add(Wp, _mul(dw, RM2_PALM)), 0.09, 0.04, up=nup)]
        for fid, k0, k1, k2 in rm2_fingers(s):
            H.append(hobox(f"rm2_hand_{side}_{fid}", k0, k2, 0.03, 0.03, ext=0.01))
    c0, c1 = rm2_cutter_axis()
    H.append(hobox("rm2_cutter", c0, c1, 0.075, 0.075))
    b0, m1, m2, tp = RM2_TOOL
    H += [hobox("rm2_tool_upper", b0, m1, 0.06, 0.06), hobox("rm2_tool_fore", m1, m2, 0.05, 0.05),
          hobox("rm2_tool_head", m2, tp, 0.07, 0.07)]
    k = RM2_PACK
    H += [hbox("rm2_engine", (0, k["y"], (k["z0"] + k["z1"]) / 2), (k["w"] + 0.02, k["h"] + 0.06, k["z0"] - k["z1"])),
          hcyl("rm2_engine", (0, k["y"], k["z1"]), (0, k["y"], k["z1"] - 0.09), 0.13)]
    for key in d["legs"]:
        k0, kc, kax = rm2_knee_pod(key)
        H.append(hbox(f"rm2_leg_{key}_thruster", _add(kc, (0, -0.02, 0)), (0.16, 0.12, 0.16)))
    return H


def _j(part, parent, pivot, axis, rng, joint):
    return {"part": part, "parent": parent, "pivot_m": [round(x, 4) for x in pivot], "axis": [round(x, 4) for x in norm(axis)],
            "range_deg": list(rng), "joint": joint}


def rm2_rig():
    """pivots of every moving part (OBJ frame, rest pose = the modelled pose, angle 0); parent first"""
    d = RM2
    G = [_j("rm2_thorax", None, (0, d["th_y"], 0), UPV, (-180, 180), "root (body frame)"),
         _j("rm2_thrusters_fore", "rm2_thorax", (0, 0.80, 0.0), UPV, (0, 0), "fixed"),
         _j("rm2_engine", "rm2_thorax", (0, RM2_PACK["y"], RM2_PACK["z0"]), UPV, (0, 0), "fixed (power pack + main thruster)"),
         _j("rm2_neck", "rm2_thorax", d["neck"], UPV, (-60, 60), "neck yaw"),
         _j("rm2_head", "rm2_neck", d["head"], (1, 0, 0), (-35, 40), "head pitch (+ = look down)"),
         _j("rm2_brow_l", "rm2_head", RM2_BROW_PIVOT[1], (0, 0, -1), (-25, 25), "brow (+ = inner end up)"),
         _j("rm2_brow_r", "rm2_head", RM2_BROW_PIVOT[-1], (0, 0, 1), (-25, 25), "brow (+ = inner end up)")]
    for key in d["legs"]:
        Hp, C, K, A, F, ax = rm2_leg(key)
        G += [_j(f"rm2_leg_{key}_coxa", "rm2_thorax", Hp, UPV, (-45, 45), "hip yaw"),
              _j(f"rm2_leg_{key}_femur", f"rm2_leg_{key}_coxa", C, ax, (-50, 70), "hip pitch (fold up for the bay, down for the flight ray)"),
              _j(f"rm2_leg_{key}_tibia", f"rm2_leg_{key}_femur", K, ax, (-80, 130), "knee (straightens into the flight ray)"),
              _j(f"rm2_leg_{key}_foot", f"rm2_leg_{key}_tibia", A, ax, (-90, 45), "ankle (pad stays flat on the hull)"),
              _j(f"rm2_leg_{key}_thruster", f"rm2_leg_{key}_tibia", rm2_knee_pod(key)[1], ax, (-120, 90), "knee thruster gimbal")]
    for s, side in ((1, "l"), (-1, "r")):
        S, E, Wp, T = rm2_arm(s)
        dw, nup = rm2_hand_axes(s)
        G += [_j(f"rm2_arm_{side}_shoulder", "rm2_thorax", S, UPV, (-35, 35), "shoulder yaw"),
              _j(f"rm2_arm_{side}_upper", f"rm2_arm_{side}_shoulder", S, (1, 0, 0), (-70, 40), "shoulder pitch (- = arm forward / up)"),
              _j(f"rm2_arm_{side}_fore", f"rm2_arm_{side}_upper", E, (1, 0, 0), (-20, 110), "elbow (0 = the mantis fold, + = unfold)"),
              _j(f"rm2_hand_{side}", f"rm2_arm_{side}_fore", Wp, (1, 0, 0), (-60, 60), "wrist pitch")]
        for fid, k0, k1, k2 in rm2_fingers(s):
            if fid == "thumb":
                G.append(_j(f"rm2_hand_{side}_thumb", f"rm2_hand_{side}", k0, _mul(nup, s), (-10, 60), "thumb (+ = close)"))
            else:
                G.append(_j(f"rm2_hand_{side}_{fid}", f"rm2_hand_{side}", k0, (1, 0, 0), (-10, 90), "finger curl (+ = grip)"))
    c0, c1 = rm2_cutter_axis()
    G.append(_j("rm2_cutter", "rm2_arm_r_fore", c0, (1, 0, 0), (-40, 40), "torch pitch along the forearm"))
    b0, m1, m2, tp = RM2_TOOL
    G += [_j("rm2_tool_upper", "rm2_thorax", b0, (1, 0, 0), (-60, 50), "tool arm base pitch"),
          _j("rm2_tool_fore", "rm2_tool_upper", m1, (1, 0, 0), (-90, 60), "tool arm elbow"),
          _j("rm2_tool_head", "rm2_tool_fore", m2, (1, 0, 0), (-70, 70), "tool head pitch (bit spins about the head axis)")]
    return G


RM2_RIG = rm2_rig()


def _sym(left, right):
    return {**left, **right}


def rm2_clips():
    """test animations for the mesh editor's player (elite.rig.v1 clips; angles in degrees, 0 = the modelled pose)"""
    C = []
    # 1. arms: the mantis fold -> reach out, grip, back
    k = {}
    for side in ("l", "r"):
        k[f"rm2_arm_{side}_upper"] = [[0, 0], [1.4, -40], [2.8, -40], [4.0, 0]]
        k[f"rm2_arm_{side}_fore"] = [[0, 0], [1.4, 80], [2.8, 80], [4.0, 0]]
        k[f"rm2_hand_{side}"] = [[0, 0], [1.4, -15], [2.8, -15], [4.0, 0]]
        for f in ("f1", "f2"):
            k[f"rm2_hand_{side}_{f}"] = [[0, 0], [1.4, -5], [1.9, 70], [2.4, 70], [2.8, -5], [4.0, 0]]
        k[f"rm2_hand_{side}_thumb"] = [[0, 0], [1.4, -5], [1.9, 45], [2.4, 45], [2.8, -5], [4.0, 0]]
    k["rm2_cutter"] = [[0, 0], [1.4, 0], [2.0, -25], [2.6, -25], [3.2, 0], [4.0, 0]]
    C.append({"id": "arms_reach", "title": "Руки: из сложенных — вперёд, хват", "duration_s": 4.0, "loop": True, "keys": k})
    # 2. walking in place: diagonal pairs (fl + rr, fr + rl), the coxa swings, the femur lifts the leg, the knee bends
    k = {}
    T = 1.6
    for key, phase in (("fl", 0.0), ("rr", 0.0), ("fr", 0.5), ("rl", 0.5)):
        s = 1 if key[1] == "l" else -1
        sw = 14.0
        pts = [(0.0, s * sw, 0, 0), (0.25, 0.0, 24, -18), (0.5, -s * sw, 0, 0), (1.0, s * sw, 0, 0)]  # back, lift, forward, sweep back
        cx, fm, tb = [], [], []
        for u, a, b, c in pts:
            t = ((u + phase) % 1.0) * T
            cx.append([t, a]); fm.append([t, b]); tb.append([t, c])
        for lst in (cx, fm, tb):
            lst.sort()
            if lst[0][0] > 0:                                    # the loop's start / end from the wrap-around value
                lst.insert(0, [0.0, lst[-1][1] + (lst[0][1] - lst[-1][1]) * (T - lst[-1][0]) / (T - lst[-1][0] + lst[0][0])])
            lst.append([T, lst[0][1]])
        k[f"rm2_leg_{key}_coxa"], k[f"rm2_leg_{key}_femur"], k[f"rm2_leg_{key}_tibia"] = cx, fm, tb
    C.append({"id": "walk", "title": "Шаг на месте", "duration_s": T, "loop": True, "keys": k})
    # 3. looking around: the neck turns, the head nods, the brows move
    C.append({"id": "look", "title": "Осмотр: голова и брови", "duration_s": 4.0, "loop": True, "keys": {
        "rm2_neck": [[0, 0], [1.0, 40], [2.0, 0], [3.0, -40], [4.0, 0]],
        "rm2_head": [[0, 0], [1.0, 12], [2.0, -18], [3.0, 12], [4.0, 0]],
        "rm2_brow_l": [[0, 0], [0.8, 20], [1.4, 0], [2.6, -18], [3.2, 0], [4.0, 0]],
        "rm2_brow_r": [[0, 0], [0.8, 20], [1.4, 0], [2.6, -18], [3.2, 0], [4.0, 0]]}})
    # 4. stowing for the bay: legs fold up, arms tucked tighter, head down, tail down
    k = {"rm2_head": [[0, 0], [1.5, 30], [2.5, 30], [4.0, 0]]}
    for key in RM2["legs"]:
        k[f"rm2_leg_{key}_femur"] = [[0, 0], [1.5, 60], [2.5, 60], [4.0, 0]]
        k[f"rm2_leg_{key}_tibia"] = [[0, 0], [1.5, -75], [2.5, -75], [4.0, 0]]
        k[f"rm2_leg_{key}_foot"] = [[0, 0], [1.5, 20], [2.5, 20], [4.0, 0]]
    for side in ("l", "r"):
        k[f"rm2_arm_{side}_upper"] = [[0, 0], [1.5, 20], [2.5, 20], [4.0, 0]]
    C.append({"id": "stow", "title": "Сложиться в отсек", "duration_s": 4.0, "loop": True, "keys": k})
    # 5. flight: the four legs straighten into horizontal rays at 45 degrees (a quadcopter X), emitters down; the knee
    #    thrusters swing to steer (author 2026-11-16)
    k = {}
    for i, key in enumerate(RM2["legs"]):
        s = RM2["legs"][key][0]
        H, Cp, K, A, F, ax = rm2_leg(key)
        yaw, fm, tb, lv = ray_pose(H, Cp, K, A, (s, 0.0, 1.0 if key[0] == "f" else -1.0))
        k[f"rm2_leg_{key}_coxa"] = [[0, 0], [1.2, yaw], [4.8, yaw], [6.0, 0]]
        k[f"rm2_leg_{key}_femur"] = [[0, 0], [1.2, fm], [4.8, fm], [6.0, 0]]
        k[f"rm2_leg_{key}_tibia"] = [[0, 0], [1.2, tb], [4.8, tb], [6.0, 0]]
        k[f"rm2_leg_{key}_foot"] = [[0, 0], [1.2, lv], [4.8, lv], [6.0, 0]]
        sw = 1 if i % 2 == 0 else -1
        k[f"rm2_leg_{key}_thruster"] = [[0, 0], [1.2, lv], [2.2, lv + sw * 25], [3.2, lv - sw * 25], [4.2, lv + sw * 25],
                                        [4.8, lv], [6.0, 0]]
    k["rm2_head"] = [[0, 0], [1.2, -10], [4.8, -10], [6.0, 0]]
    C.append({"id": "fly", "title": "Полёт: ноги лучами под 45° (квадрокоптер), двигатели в коленях рулят", "duration_s": 6.0,
              "loop": True, "keys": k})
    # 6. capture and tow (the repair job: CapturingFragment -> towing back): reach, grip, pull the part to the chest
    k = {}
    for side in ("l", "r"):
        k[f"rm2_arm_{side}_upper"] = [[0, 0], [1.2, -45], [2.0, -45], [3.2, -10], [4.6, -10], [5.6, 0]]
        k[f"rm2_arm_{side}_fore"] = [[0, 0], [1.2, 85], [2.0, 85], [3.2, 40], [4.6, 40], [5.6, 0]]
        k[f"rm2_hand_{side}"] = [[0, 0], [1.2, -10], [3.2, 10], [4.6, 10], [5.6, 0]]
        for f in ("f1", "f2"):
            k[f"rm2_hand_{side}_{f}"] = [[0, 0], [1.2, -5], [1.8, 75], [4.6, 75], [5.6, 0]]
        k[f"rm2_hand_{side}_thumb"] = [[0, 0], [1.2, -5], [1.8, 50], [4.6, 50], [5.6, 0]]
    C.append({"id": "capture", "title": "Захват детали и буксировка", "duration_s": 6.0, "loop": True, "keys": k})
    return C


def rm2_attach():
    """the toolbox saddle on the thorax and a grip point in each hand (std.drone.toolbox)"""
    A = [{"id": "saddle", "part": "rm2_thorax", **tb_saddle(Part("probe"), RM2_SADDLE, 3, rotated=True)}]
    for s, side in ((1, "l"), (-1, "r")):
        S, E, Wp, T = rm2_arm(s)
        dw, nup = rm2_hand_axes(s)
        A.append({"id": f"grip_{side}", "part": f"rm2_hand_{side}", "point_m": [round(v, 4) for v in _add(Wp, _mul(dw, RM2_PALM + 0.05))],
                  "up": [round(v, 4) for v in nup], "forward": [round(v, 4) for v in dw], "fits": "grip",
                  "note": "between the fingers and the thumb (fingers closed ~70 deg)"})
    return A


def rm2_tool_clips():
    """tool clips (author 2026-11-16): the left hand (the cutter rides on the right forearm) reaches back over the
    shoulder into the box on the thorax saddle, takes the wrench, turns a nut in front, puts it back (IK)"""
    S, E, Wp, T = rm2_arm(1)
    dw, nup = rm2_hand_axes(1)
    arm = {"chain": ["rm2_arm_l_shoulder", "rm2_arm_l_upper", "rm2_arm_l_fore", "rm2_hand_l"],
           "bounds": {"rm2_arm_l_shoulder": (-110, 110), "rm2_arm_l_upper": (-160, 60), "rm2_arm_l_fore": (-60, 160),
                      "rm2_hand_l": (-90, 90)},
           "hand": "rm2_hand_l", "grip": _add(Wp, _mul(dw, RM2_PALM + 0.05)), "up": nup, "wrist": "rm2_hand_l",
           "fingers": {"rm2_hand_l_f1": 70, "rm2_hand_l_f2": 70, "rm2_hand_l_thumb": 45},
           "avoid": ("rm2_arm_l_fore", [_lerp(E, Wp, u) for u in (0.0, 0.3, 0.6)], 0.05)}
    nut = (0.12, 0.30, 1.20)
    clip, used, err = take_from_saddle(RM2_RIG, RM2_PROP, arm, "tb_tool_wrench",
                                [(0.9, nut), (0.5, "wrist", 35), (0.5, "wrist", -35), (0.5, "wrist", 35), (0.5, "wrist", -35)],
                                "wrench", "Ключ: достать из ящика на спине, повернуть гайку, убрать")
    widen(RM2_RIG, used)
    return [clip], err


RM2_PROP = toolbox_prop("rm2_thorax", tb_saddle(Part("probe"), RM2_SADDLE, 3, rotated=True))
RM2_TOOL_CLIPS, RM2_IK_MISS = rm2_tool_clips()
RIGS = {"agora_rm2_drone": lambda: {"joints": RM2_RIG, "clips": rm2_clips() + RM2_TOOL_CLIPS, "attach": rm2_attach(),
                                    "props": [RM2_PROP]}}

ASSETS = {"smy_mp8_pod": mp8_parts, "hyperion_rd1_drone": rd1_parts, "agora_rm2_drone": rm2_parts}
HITS = {"smy_mp8_pod": mp8_hits, "hyperion_rd1_drone": rd1_hits, "agora_rm2_drone": rm2_hits}
MODEL_ASSETS = {"sol.wpn.mp8": "smy_mp8_pod", "hyp.drone.rd1": "hyperion_rd1_drone", "agora.drone.rm2": "agora_rm2_drone"}

SETS = carry_set("agora_rm2_drone", "RM-2 Mantide", tb_saddle(Part("probe"), RM2_SADDLE, 3, rotated=True))
