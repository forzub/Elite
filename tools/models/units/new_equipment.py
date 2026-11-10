"""New equipment proposals (author request 2026-11-10, not in the canon yet - passports in briefs/new_parts_proposal.json):
  Sol Mass Yards LM-MP8   light external missile rack (8 cells, blow-out covers, end reload hatch, launch-control unit);
  Hyperion Yards RD-1   recon drone (gimballed camera turret, deployable antenna, power block, 4 field-coupled thrusters,
                        3-point kinematic docking latch);
  Agora Modular RM-2 "Mantide"  repair drone, a mantis on four magnetic feet: every limb segment / joint is its own part
                        with its pivot at a joint (RM2_RIG below; the rig goes into the proposal passport).
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

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
           neck=(0.0, 0.80, 0.56), head=(0.0, 0.85, 0.64), abd=(0.0, 0.62, -0.24), abd_tilt=0.15,
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
RM2_EYES = [(s, (s * 0.12, 0.925, 0.735), norm((s * 0.35, 0.08, 1.0))) for s in (1, -1)]
RM2_ANT = {s: [(s * 0.045, 0.955, 0.735), (s * 0.06, 1.08, 0.78), (s * 0.13, 1.20, 0.84), (s * 0.22, 1.22, 0.96)] for s in (1, -1)}


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

    # --- field thrusters on the thorax shoulders
    tf = part("rm2_thrusters_fore")
    for s in (-1, 1):
        _pod(tf, (s * 0.12, 0.74, 0.0), (s * 0.27, 0.84, 0.0), (s * 0.5, 0.86, 0.0), small, lod)

    # --- neck (yaw) and head (pitch): skull, face plate, sensor eyes with brows, lidar mandible
    nk = part("rm2_neck")
    seg_lathe(nk, S_, d["pro1"], d["head"], [(0.0, 0.0), (0.06, 0.0), (0.055, 0.09), (0.0, 0.09)], ks)
    if lod == 0:
        knuckle(nk, B_, d["head"], (1, 0, 0), 0.042, 0.13, ks, cap_mat=S_)
    hd = part("rm2_head")
    box(hd, W_, (0, 0.89, 0.705), (0.24, 0.14, 0.15), chamfer=0.045 if lod < 2 else 0.0)
    box(hd, T_, (0, 0.80, 0.735), (0.10, 0.07, 0.09), chamfer=ch(0.02))                              # mandible
    if lod < 2:
        box(hd, S_, (0, 0.875, 0.783), (0.13, 0.085, 0.014), chamfer=ch(0.006))                      # face plate
        box(hd, "eq_agora_rm2_lens", (0, 0.795, 0.781), (0.075, 0.018, 0.006))                          # lidar window
        box(hd, T_, (0, 0.962, 0.70), (0.12, 0.012, 0.11), chamfer=ch(0.004))                         # crest panel
    for s, ec, de in RM2_EYES:
        p0 = _sub(ec, _mul(de, 0.045))
        p1 = _add(ec, de)
        es = small * 2 if lod == 0 else small
        seg_lathe(hd, W_, p0, p1, [(0.0, 0.0), (0.07, 0.0), (0.078, 0.02), (0.078, 0.05), (0.07, 0.058)], es, soft=40)
        if lod == 2:
            seg_lathe(hd, "eq_agora_rm2_iris", p0, p1, [(0.07, 0.058), (0.0, 0.09)], es)
            continue
        seg_lathe(hd, "eq_agora_rm2_lens", p0, p1, [(0.07, 0.058), (0.062, 0.072), (0.05, 0.082)], es, soft=50)
        seg_lathe(hd, "eq_agora_rm2_iris", p0, p1, [(0.05, 0.082), (0.034, 0.091), (0.02, 0.0955)], es, soft=50)
        seg_lathe(hd, "eq_agora_rm2_lens", p0, p1, [(0.02, 0.0955), (0.0, 0.098)], es, soft=50)
        if lod < 2:
            seg_lathe(hd, B_, p0, p1, crect(0.074, 0.084, 0.040, 0.052), es)                         # bezel ring
            bc = _add(ec, (s * 0.005, 0.072, 0.0))
            box(hd, T_, bc, (0.13, 0.018, 0.06), rot_z(-s * 0.22), chamfer=ch(0.006))                  # brow plate
    if lod == 0:
        for k in range(3):                                                                           # three ocelli
            p = ((k - 1) * 0.035, 0.955 - abs(k - 1) * 0.008, 0.775)
            seg_lathe(hd, "eq_agora_rm2_lens", p, _add(p, (0, 0.3, 1.0)), [(0.0, 0.0), (0.012, 0.0), (0.0, 0.008)], 8)
        for s in (-1, 1):
            hex_bolt(hd, B_, (s * 0.09, 0.835, 0.781), (0, 0, 1), 0.016, 0.006)

    # --- feelers (charm + near-field sensors): thin tubes with amber tip lamps
    for s, side in ((1, "l"), (-1, "r")):
        an = part(f"rm2_antenna_{side}")
        if lod < 2:
            tube(an, S_, bezier(*RM2_ANT[s], 10 if lod == 0 else 5), 0.008, 6 if lod == 0 else 4)
            ball(an, "eq_status_amber", RM2_ANT[s][3], 0.018, 8)
            cyl(an, B_, _sub(RM2_ANT[s][0], (0, 0.01, 0)), _add(RM2_ANT[s][0], (0, 0.02, 0)), 0.016, 8)

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

    # --- raptorial arms: shoulder housing, upper arm, forearm; left ends in the claw, right in the plasma cutter
    for s, side in ((1, "l"), (-1, "r")):
        S, E, Wp, T = rm2_arm(s)
        sh = part(f"rm2_arm_{side}_shoulder")
        ball(sh, W_, S, 0.06, ks)
        if lod == 0:
            knuckle(sh, B_, S, (1, 0, 0), 0.045, 0.14, ks, cap_mat=T_)
        up_ = part(f"rm2_arm_{side}_upper")
        hex_limb(up_, W_, S, E, 0.045, 0.038)
        fo = part(f"rm2_arm_{side}_fore")
        if lod == 0:
            knuckle(fo, B_, E, (1, 0, 0), 0.042, 0.11, ks, cap_mat=S_)
        hex_limb(fo, T_, E, Wp, 0.042, 0.034, a0=math.pi / 6)
        if lod < 2 and s > 0:                                                                       # mantis spines on the claw arm
            dn = norm(_cross((1, 0, 0), _sub(Wp, E)))
            for t in (0.3, 0.5, 0.7, 0.88):
                p = _add(_lerp(E, Wp, t), _mul(dn, 0.03))
                cyl(fo, W_, p, _add(p, _add(_mul(dn, 0.05), _mul(norm(_sub(Wp, E)), -0.02))), 0.009, 4)
        dw = norm(_sub(T, Wp))
        nup = norm(_cross(dw, (1, 0, 0)))
        if s > 0:
            wr = part("rm2_arm_l_wrist")
            if lod == 0:
                knuckle(wr, B_, Wp, (1, 0, 0), 0.036, 0.10, ks, cap_mat=W_)
            J = _add(Wp, _mul(dw, 0.075))
            seg_lathe(wr, S_, Wp, J, [(0.0, 0.0), (0.04, 0.0), (0.042, 0.06), (0.0, 0.075)], ks)
            for sgn, jn in ((1, "a"), (-1, "b")):
                jw = part(f"rm2_claw_jaw_{jn}")
                q0 = J
                q1 = _add(J, _add(_mul(dw, 0.08), _mul(nup, sgn * 0.045)))
                q2 = _add(J, _add(_mul(dw, 0.17), _mul(nup, sgn * 0.012)))
                obox(jw, W_, q0, q1, 0.035, 0.022, up=nup, chamfer=ch(0.005))
                obox(jw, T_, q1, q2, 0.03, 0.02, up=nup, chamfer=ch(0.005))
                if lod < 2:
                    box(jw, "eq_metal_band", _add(q2, _mul(nup, -sgn * 0.012)), (0.032, 0.008, 0.03), frame_along(q1, q2, nup))  # grip pad
                    knuckle(jw, B_, J, (1, 0, 0), 0.02, 0.05, ks)
        else:
            ct = part("rm2_cutter")
            if lod == 0:
                knuckle(ct, B_, Wp, (1, 0, 0), 0.036, 0.10, ks, cap_mat=W_)
            tip = _add(Wp, _mul(dw, 0.22))
            seg_lathe(ct, W_, Wp, tip, [(0.0, 0.0), (0.04, 0.0), (0.044, 0.02), (0.044, 0.12), (0.032, 0.15), (0.0, 0.15)], ks)
            seg_lathe(ct, "eq_copper", Wp, tip, [(0.0, 0.15), (0.024, 0.15), (0.018, 0.205), (0.009, 0.22), (0.0, 0.22)], ks)
            if lod < 2:
                seg_lathe(ct, S_, Wp, tip, crect(0.044, 0.05, 0.05, 0.08), ks)                          # sea-blue band
                side_off = (-0.055, 0.0, 0.0)                                                              # gas / power cartridge
                c0 = _add(_add(Wp, _mul(dw, 0.02)), side_off)
                c1 = _add(_add(Wp, _mul(dw, 0.12)), side_off)
                cyl(ct, T_, c0, c1, 0.022, 8, chamfer=ch(0.006))
            if close():
                tube(ct, B_, bezier(_add(_add(Wp, _mul(dw, 0.10)), (-0.055, 0.0, 0.0)), _add(_add(Wp, _mul(dw, 0.15)), (-0.06, 0.0, 0.0)),
                                    _add(_add(Wp, _mul(dw, 0.13)), (-0.03, 0.0, 0.0)), _add(Wp, _mul(dw, 0.135)), 8), 0.007, 6)
                seg_lathe(ct, "eq_steel", Wp, tip, crect(0.044, 0.05, 0.135, 0.145), ks)

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

    # --- abdomen (power pack) hinged at the thorax rear, tail slightly raised; aft field thrusters ride on it
    ab = Part("rm2_abdomen")
    ta = Part("rm2_thrusters_aft")
    ap = d["abd"]
    if lod == 0:
        knuckle(ab, B_, ap, (1, 0, 0), 0.06, 0.18, ks, cap_mat=S_)
    box(ab, S_, (0, 0.625, -0.38), (0.28, 0.23, 0.24), chamfer=0.03 if lod < 2 else 0.0)
    box(ab, S_, (0, 0.635, -0.63), (0.34, 0.27, 0.24), chamfer=0.03 if lod < 2 else 0.0)
    box(ab, T_, (0, 0.635, -0.88), (0.28, 0.22, 0.24), chamfer=0.03 if lod < 2 else 0.0)
    if lod < 2:
        for z in (-0.505, -0.755):                                                                  # white frame rings
            box(ab, W_, (0, 0.635, z), (0.35, 0.28, 0.04), chamfer=ch(0.01))
        box(ab, W_, (0, 0.635, -1.01), (0.20, 0.15, 0.03), chamfer=ch(0.01))                           # end cap
        cyl(ab, B_, (0, 0.635, -1.025), (0, 0.635, -1.055), 0.03, ks)                                  # charge / data socket
        for sx in (-1, 1):                                                                             # corner castings (bay twist locks)
            for z in (-0.54, -0.72):
                box(ab, B_, (sx * 0.15, 0.785, z), (0.05, 0.04, 0.05), chamfer=ch(0.006))
        box(ab, W_, (0, 0.773, -0.63), (0.18, 0.012, 0.16), chamfer=ch(0.004))                        # top hatch
    if lod == 0:
        for sx in (-1, 1):
            for z in (-0.54, -0.72):
                box(ab, "eq_metal_band", (sx * 0.15, 0.806, z), (0.025, 0.003, 0.015))                 # casting slots
            box(ab, W_, (sx * 0.172, 0.635, -0.63), (0.006, 0.12, 0.16))                               # side placards
            box(ab, "eq_status_amber", (sx * 0.176, 0.67, -0.60), (0.004, 0.015, 0.04))
        for sx in (-1, 1):
            for sz in (-1, 1):
                hex_bolt(ab, B_, (sx * 0.075, 0.779, -0.63 + sz * 0.065), (0, 1, 0), 0.016, 0.006)
    if close():
        harness(ab, bezier((0.06, 0.70, -0.25), (0.08, 0.76, -0.30), (0.10, 0.76, -0.40), (0.12, 0.75, -0.48), 8),
                0.008, 2, (S_, "eq_insulator"), ties=B_, tie_every=3)
    for s in (-1, 1):
        _pod(ta, (s * 0.13, 0.70, -0.88), (s * 0.24, 0.78, -0.88), (s * 0.5, 0.86, 0.0), small, lod)
    R = rot_x(d["abd_tilt"])
    P["rm2_abdomen"] = pivoted(ab, ap, R)
    P["rm2_thrusters_aft"] = pivoted(ta, ap, R)
    return P


def _rm2_abd_point(p):
    R = rot_x(RM2["abd_tilt"])
    ap = RM2["abd"]
    return _add(ap, apply(R, _sub(p, ap)))


def rm2_hits():
    d = RM2
    y0, r = d["th_y"], d["th_r"]
    H = [hbox("rm2_thorax", (0, y0, (d["th_z0"] + d["th_z1"]) / 2), (2 * r, 2 * r * math.sin(math.pi / 3), d["th_z1"] - d["th_z0"])),
         hobox("rm2_thorax", d["pro0"], d["pro1"], 0.22, 0.20),
         hobox("rm2_neck", d["pro1"], d["head"], 0.12, 0.12),
         hbox("rm2_head", (0, 0.89, 0.705), (0.24, 0.14, 0.15)),
         hbox("rm2_head", (0, 0.80, 0.735), (0.10, 0.07, 0.09))]
    for s, ec, de in RM2_EYES:
        H.append(hobox("rm2_head", _sub(ec, _mul(de, 0.045)), _add(ec, _mul(de, 0.055)), 0.16, 0.16))
    for s in (1, -1):
        H.append(hbox("rm2_thrusters_fore", (s * 0.25, 0.83, 0.0), (0.17, 0.12, 0.16)))
        a = RM2_ANT[s]
        H.append(hobox(f"rm2_antenna_{'l' if s > 0 else 'r'}", a[0], a[3], 0.04, 0.04))
    for key in d["legs"]:
        Hp, C, K, A, F, ax = rm2_leg(key)
        H += [hobox(f"rm2_leg_{key}_coxa", Hp, C, 0.10, 0.10, ext=0.03),
              hobox(f"rm2_leg_{key}_femur", C, K, 0.10, 0.10, ext=0.02),
              hobox(f"rm2_leg_{key}_tibia", K, A, 0.08, 0.08, ext=0.02),
              hobox(f"rm2_leg_{key}_foot", A, _add(F, (0, 0.05, 0)), 0.06, 0.06),
              hcyl(f"rm2_leg_{key}_foot", F, _add(F, (0, 0.05, 0)), 0.09)]
    for s, side in ((1, "l"), (-1, "r")):
        S, E, Wp, T = rm2_arm(s)
        dw = norm(_sub(T, Wp))
        H += [hobox(f"rm2_arm_{side}_shoulder", _sub(S, (0.07, 0, 0)), _add(S, (0.07, 0, 0)), 0.12, 0.12),
              hobox(f"rm2_arm_{side}_upper", S, E, 0.09, 0.09),
              hobox(f"rm2_arm_{side}_fore", E, Wp, 0.09, 0.12 if s > 0 else 0.09, ext=0.02)]
        if s > 0:
            J = _add(Wp, _mul(dw, 0.075))
            nup = norm(_cross(dw, (1, 0, 0)))
            H.append(hobox("rm2_arm_l_wrist", Wp, J, 0.09, 0.09))
            for sgn, jn in ((1, "a"), (-1, "b")):
                q1 = _add(J, _add(_mul(dw, 0.08), _mul(nup, sgn * 0.045)))
                q2 = _add(J, _add(_mul(dw, 0.17), _mul(nup, sgn * 0.012)))
                H += [hobox(f"rm2_claw_jaw_{jn}", J, q1, 0.04, 0.03, up=nup), hobox(f"rm2_claw_jaw_{jn}", q1, q2, 0.04, 0.03, up=nup)]
        else:
            H.append(hobox("rm2_cutter", Wp, _add(Wp, _mul(dw, 0.22)), 0.14, 0.09, up=(1, 0, 0)))
    b0, m1, m2, tp = RM2_TOOL
    H += [hobox("rm2_tool_upper", b0, m1, 0.06, 0.06), hobox("rm2_tool_fore", m1, m2, 0.05, 0.05),
          hobox("rm2_tool_head", m2, tp, 0.07, 0.07)]
    # abdomen + aft thrusters (tilted with the abdomen)
    R = rot_x(d["abd_tilt"])
    rot = euler_xyz(R)
    H += [hbox("rm2_abdomen", _rm2_abd_point((0, 0.635, -0.635)), (0.35, 0.28, 0.80), rot),
          hbox("rm2_abdomen", _rm2_abd_point((0, 0.635, -1.03)), (0.20, 0.15, 0.06), rot)]
    for s in (-1, 1):
        H.append(hbox("rm2_thrusters_aft", _rm2_abd_point((s * 0.23, 0.78, -0.88)), (0.17, 0.12, 0.16), rot))
    return H


def _j(part, parent, pivot, axis, rng, joint):
    return {"part": part, "parent": parent, "pivot_m": [round(x, 4) for x in pivot], "axis": [round(x, 4) for x in norm(axis)],
            "range_deg": list(rng), "joint": joint}


def rm2_rig():
    """pivots of every moving part (OBJ frame, rest pose = the modelled pose, angle 0); parent first"""
    d = RM2
    G = [_j("rm2_thorax", None, (0, d["th_y"], 0), UPV, (-180, 180), "root (body frame)"),
         _j("rm2_thrusters_fore", "rm2_thorax", (0, 0.80, 0.0), UPV, (0, 0), "fixed"),
         _j("rm2_abdomen", "rm2_thorax", d["abd"], (1, 0, 0), (-25, 35), "abdomen pitch (+ = tail up)"),
         _j("rm2_thrusters_aft", "rm2_abdomen", _rm2_abd_point((0, 0.78, -0.88)), UPV, (0, 0), "fixed"),
         _j("rm2_neck", "rm2_thorax", d["neck"], UPV, (-60, 60), "neck yaw"),
         _j("rm2_head", "rm2_neck", d["head"], (1, 0, 0), (-35, 40), "head pitch (+ = look down)"),
         _j("rm2_antenna_l", "rm2_head", RM2_ANT[1][0], (1, 0, 0), (-30, 45), "feeler sweep"),
         _j("rm2_antenna_r", "rm2_head", RM2_ANT[-1][0], (1, 0, 0), (-30, 45), "feeler sweep")]
    for key in d["legs"]:
        Hp, C, K, A, F, ax = rm2_leg(key)
        G += [_j(f"rm2_leg_{key}_coxa", "rm2_thorax", Hp, UPV, (-40, 40), "hip yaw"),
              _j(f"rm2_leg_{key}_femur", f"rm2_leg_{key}_coxa", C, ax, (-50, 70), "hip pitch (fold up for the bay)"),
              _j(f"rm2_leg_{key}_tibia", f"rm2_leg_{key}_femur", K, ax, (-80, 60), "knee"),
              _j(f"rm2_leg_{key}_foot", f"rm2_leg_{key}_tibia", A, ax, (-45, 45), "ankle (pad stays flat on the hull)")]
    for s, side in ((1, "l"), (-1, "r")):
        S, E, Wp, T = rm2_arm(s)
        dw = norm(_sub(T, Wp))
        G += [_j(f"rm2_arm_{side}_shoulder", "rm2_thorax", S, UPV, (-35, 35), "shoulder yaw"),
              _j(f"rm2_arm_{side}_upper", f"rm2_arm_{side}_shoulder", S, (1, 0, 0), (-70, 60), "shoulder pitch"),
              _j(f"rm2_arm_{side}_fore", f"rm2_arm_{side}_upper", E, (1, 0, 0), (-110, 40), "elbow (mantis fold)")]
        if s > 0:
            J = _add(Wp, _mul(dw, 0.075))
            G += [_j("rm2_arm_l_wrist", "rm2_arm_l_fore", Wp, dw, (-180, 180), "wrist roll (pitch about X also possible: -60..60)"),
                  _j("rm2_claw_jaw_a", "rm2_arm_l_wrist", J, (1, 0, 0), (-10, 40), "jaw open (+ = open)"),
                  _j("rm2_claw_jaw_b", "rm2_arm_l_wrist", J, (-1, 0, 0), (-10, 40), "jaw open (+ = open)")]
        else:
            G.append(_j("rm2_cutter", "rm2_arm_r_fore", Wp, (1, 0, 0), (-70, 70), "torch pitch (roll about the torch axis: -180..180)"))
    b0, m1, m2, tp = RM2_TOOL
    G += [_j("rm2_tool_upper", "rm2_thorax", b0, (1, 0, 0), (-60, 50), "tool arm base pitch"),
          _j("rm2_tool_fore", "rm2_tool_upper", m1, (1, 0, 0), (-90, 60), "tool arm elbow"),
          _j("rm2_tool_head", "rm2_tool_fore", m2, (1, 0, 0), (-70, 70), "tool head pitch (bit spins about the head axis)")]
    return G


RM2_RIG = rm2_rig()

ASSETS = {"smy_mp8_pod": mp8_parts, "hyperion_rd1_drone": rd1_parts, "agora_rm2_drone": rm2_parts}
HITS = {"smy_mp8_pod": mp8_hits, "hyperion_rd1_drone": rd1_hits, "agora_rm2_drone": rm2_hits}
MODEL_ASSETS = {"sol.wpn.mp8": "smy_mp8_pod", "hyp.drone.rd1": "hyperion_rd1_drone", "agora.drone.rm2": "agora_rm2_drone"}
