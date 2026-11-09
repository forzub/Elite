"""Firmament Armour (Tianlei, HD 40307; PRC third ark military province): BK-1 ceramic armour plate, KP-30 coilgun,
PT-2 point-defence laser turret.

School (DESIGN_SCHOOLS.md): military, blunt - thick slabs, tile armour, rivet lines, chamfered heavy plates; grey-green
and gun grey; interface = square bolted plate with 16 bolts (5 per side); close-up: armour tile edges, conduit in
armoured channels. Hull-skin units: outward +Y, mounting face y = 0; turrets rest with the barrel / emitter on +Z.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_fa_green": ((0.24, 0.28, 0.22), (0.18, 0.18, 0.18), 30),
    "eq_fa_gun": ((0.40, 0.40, 0.38), (0.30, 0.30, 0.30), 60),
    "eq_fa_tile": ((0.30, 0.34, 0.27), (0.10, 0.10, 0.10), 20),
    "eq_fa_dark": ((0.10, 0.11, 0.10), (0.10, 0.10, 0.10), 20),
    "eq_fa_lens": ((0.08, 0.12, 0.16), (0.80, 0.80, 0.80), 200),
})
_GR, _GU, _TI, _DA = "eq_fa_green", "eq_fa_gun", "eq_fa_tile", "eq_fa_dark"


def _poly(part, mat, pts, hint):
    n = facet_normal(pts) or hint
    if n[0] * hint[0] + n[1] * hint[1] + n[2] * hint[2] < 0:
        n = (-n[0], -n[1], -n[2])
    part.face(mat, [part.vert(p, n) for p in pts], n)


def _xslab(part, mat, yz, x0, x1):
    """a heavy plate: convex (y, z) outline extruded along X from x0 to x1 (chamfered slab edges come from the outline)"""
    A = [(x0, y, z) for y, z in yz]
    B = [(x1, y, z) for y, z in yz]
    m = lambda Q: tuple(sum(p[k] for p in Q) / len(Q) for k in range(3))
    c = m(A + B)
    _poly(part, mat, A, (-1, 0, 0))
    _poly(part, mat, B, (1, 0, 0))
    for i in range(len(A)):
        j = (i + 1) % len(A)
        q = [A[i], A[j], B[j], B[i]]
        mq = m(q)
        _poly(part, mat, q, tuple(mq[k] - c[k] for k in range(3)))


def _cham(y0, y1, z0, z1, c):
    """(y, z) rectangle with chamfered corners for _xslab"""
    return [(y0, z0 + c), (y0 + c, z0), (y1 - c, z0), (y1, z0 + c), (y1, z1 - c), (y1 - c, z1), (y0 + c, z1), (y0, z1 - c)]


def _rivets(part, p0, p1, n, normal, size=0.035):
    """a rivet line (low hex heads) from p0 to p1; culled below the visibility floor like any fastener"""
    for i in range(n):
        t = (i + 0.5) / n
        hex_bolt(part, _GU, tuple(p0[k] + (p1[k] - p0[k]) * t for k in range(3)), normal, size, size * 0.35)


def _sq_mount(part, side, t, lod, bolt=0.07):
    """Firmament interface: square bolted plate (chamfered), 16 bolts (5 per side) on its top face"""
    box(part, _GU, (0, t / 2, 0), (side, t, side), chamfer=0.02 if lod == 0 else 0.0)
    if lod == 0:
        e = side / 2 - 0.07
        pts = set()
        for i in range(5):
            q = -e + 2 * e * i / 4
            pts |= {(q, -e), (q, e), (-e, q), (e, q)}
        for x, z in sorted(pts):
            hex_bolt(part, "eq_steel", (x, t, z), (0, 1, 0), bolt, bolt * 0.6, washer=True)


# ---------------------------------------------------------------- BK-1 ceramic armour plate (std.armour.ceramic)
# Passport envelope [1.567, 1.045, 0.078] (W, H, L). A hull-skin plate faces outward along +Y, so the thin 0.078 m is
# built on Y: 1.567 (X) x 0.078 (Y) x 1.045 (Z). 5 x 3 chamfered ceramic tiles on a gun-grey backing slab inside a
# riveted edge frame (SRU plate); 16 through-bolts of the Firmament square pattern in the frame and two hat rails
# underneath (SRU mount).
BK = dict(W=1.567, T=0.078, L=1.045, fw=0.06)


def bk1_parts(lod):
    W, T, L, fw = BK["W"], BK["T"] - 0.012, BK["L"], BK["fw"]           # bolt heads end at the envelope
    P = {}
    pl = P["bk1_plate"] = Part("bk1_plate")
    y0, y1 = 0.014, 0.034                                               # backing slab
    box(pl, _GU, (0, (y0 + y1) / 2, 0), (W, y1 - y0, L))
    for s in (-1, 1):                                                    # edge frame
        box(pl, _GU, (s * (W / 2 - fw / 2), (y1 + T) / 2, 0), (fw, T - y1, L), chamfer=0.01 if lod == 0 else 0.0)
        box(pl, _GU, (0, (y1 + T) / 2, s * (L / 2 - fw / 2)), (W - 2 * fw, T - y1, fw), chamfer=0.01 if lod == 0 else 0.0)
    nx, nz = 5, 3
    ix0, iz0 = W / 2 - fw, L / 2 - fw
    if lod < 2:
        g = 0.014
        for i in range(nx):
            for j in range(nz):
                cx = -ix0 + (i + 0.5) * 2 * ix0 / nx
                cz = -iz0 + (j + 0.5) * 2 * iz0 / nz
                box(pl, _TI, (cx, (y1 + T - 0.006) / 2, cz), (2 * ix0 / nx - g, T - 0.006 - y1, 2 * iz0 / nz - g),
                    chamfer=0.010 if lod == 0 else 0.0)
    else:
        box(pl, _TI, (0, (y1 + T - 0.006) / 2, 0), (2 * ix0, T - 0.006 - y1, 2 * iz0))
    if close():                                                          # rivet lines along the frame
        for s in (-1, 1):
            _rivets(pl, (s * (W / 2 - fw * 0.30), T, -L / 2 + 0.1), (s * (W / 2 - fw * 0.30), T, L / 2 - 0.1), 14, (0, 1, 0), 0.022)
            _rivets(pl, (-W / 2 + 0.1, T, s * (L / 2 - fw * 0.30)), (W / 2 - 0.1, T, s * (L / 2 - fw * 0.30)), 20, (0, 1, 0), 0.022)

    mt = P["bk1_mount"] = Part("bk1_mount")
    for s in (-1, 1):                                                    # hat rails under the slab
        box(mt, _GR, (0, 0.007, s * 0.30), (W - 0.12, 0.014, 0.12))
    e_x, e_z = W / 2 - fw / 2, L / 2 - fw / 2
    pts = set()
    for i in range(5):                                                   # 16 bolts, 5 per side (square pattern)
        pts |= {(-e_x + 2 * e_x * i / 4, -e_z), (-e_x + 2 * e_x * i / 4, e_z), (-e_x, -e_z + 2 * e_z * i / 4), (e_x, -e_z + 2 * e_z * i / 4)}
    for x, z in sorted(pts):
        if lod < 2:
            lathe(mt, "eq_steel", [(0, T - 0.004), (0.032, T - 0.004), (0.032, T), (0, T)], 6 if lod else 8, R_Y, (x, 0, z))
        if lod == 0:
            hex_bolt(mt, "eq_steel", (x, T, z), (0, 1, 0), 0.045 if close() else 0.06, 0.012)
    return P


def bk1_hits():
    W, T, L = BK["W"], BK["T"], BK["L"]
    return [hbox("bk1_plate", (0, (0.014 + T) / 2, 0), (W, T - 0.014, L)),
            hbox("bk1_mount", (0, 0.007, -0.30), (W - 0.12, 0.014, 0.12)),
            hbox("bk1_mount", (0, 0.007, 0.30), (W - 0.12, 0.014, 0.12))]


# ---------------------------------------------------------------- KP-30 coilgun turret (fa.wpn.coil30)
# Envelope 1.984 x 1.191 x 3.175: low turret on a square 16-bolt plate; traverse drum and deck, two thick chamfered
# trunnion cheeks with the elevation drive (SRU mount), a long square-section barrel with eight blunt coil packs behind
# a heavy mantlet (SRU coil), an armoured capacitor box with tile armour at the rear (SRU caps), the shell magazine and
# feed chute on the -X cheek (SRU feed).
KP_Y, KP_Z = 0.80, -0.62                                                 # elevation axis height, trunnion z


def _handle_bar(part, a, b, out, h):
    """a grab bar standing off a slab"""
    a1 = tuple(a[k] + out[k] * h for k in range(3))
    b1 = tuple(b[k] + out[k] * h for k in range(3))
    cyl(part, "eq_steel", a, a1, 0.018, 6, caps=False)
    cyl(part, "eq_steel", b, b1, 0.018, 6, caps=False)
    cyl(part, "eq_steel", a1, b1, 0.018, 6)


def kp30_parts(lod):
    small = small_for(lod)
    ch = 0.03 if lod == 0 else 0.0
    fb = 0.03 if close() else 0.06
    P = {}
    m = P["kp30_mount"] = Part("kp30_mount")
    _sq_mount(m, 1.30, 0.08, lod)
    lathe(m, _GU, [(0, 0.08), (0.64, 0.08), (0.64, 0.19), (0.60, 0.23), (0, 0.23)], 8, R_Y, (0, 0, 0), smooth=False)   # traverse drum
    _xslab(m, _GR, _cham(0.23, 0.33, -1.52, 0.10, 0.04), -0.66, 0.66)                                 # deck
    cheek = [(0.33, -1.14), (0.33, -0.02), (0.86, -0.02), (1.17, -0.30), (1.17, -0.95), (0.98, -1.14)]
    for sx in (-1, 1):                                                   # trunnion cheeks: sloped-front heavy plates
        _xslab(m, _GR, cheek, min(sx * 0.53, sx * 0.71), max(sx * 0.53, sx * 0.71))
        cyl(m, _GU, (sx * 0.71, KP_Y, KP_Z), (sx * 0.78, KP_Y, KP_Z), 0.16, small)                     # trunnion cap
    box(m, _GU, (0.84, 0.66, KP_Z - 0.20), (0.20, 0.42, 0.52), chamfer=ch)                              # elevation drive
    if lod < 2:
        cyl(m, _GU, (0.84, 0.87, KP_Z - 0.20), (0.84, 0.95, KP_Z - 0.20), 0.07, small)
        for sx in (-1, 1):                                               # armour tiles on the cheek outer faces
            for z0, z1 in ((-1.08, -0.62), (-0.58, -0.10)):
                if sx > 0 and z0 < -0.9:
                    continue
                box(m, _TI, (sx * 0.72, 0.62, (z0 + z1) / 2), (0.02, 0.44, z1 - z0 - 0.02))
    if lod == 0:
        for sx in (-1, 1):
            _rivets(m, (sx * 0.71, 1.10, -1.05), (sx * 0.71, 1.10, -0.35), 8, (sx, 0, 0), fb)
            _rivets(m, (sx * 0.71, 0.40, -1.05), (sx * 0.71, 0.40, -0.10), 9, (sx, 0, 0), fb)

    c = P["kp30_coil"] = Part("kp30_coil")
    _xslab(c, _GR, _cham(KP_Y - 0.30, KP_Y + 0.30, -0.14, 0.18, 0.06), -0.50, 0.50)                   # mantlet
    box(c, _GU, (0, KP_Y, -0.62), (0.80, 0.46, 0.96), chamfer=ch)                                     # breech housing
    box(c, _GU, (0, KP_Y, 0.85), (0.20, 0.20, 1.34))                                                  # rail housing
    for i in range(8):                                                   # coil packs
        z = 0.30 + i * 0.158
        box(c, _GR, (0, KP_Y, z), (0.36, 0.36, 0.10), chamfer=0.02 if lod == 0 else 0.0)
        if lod == 0:
            box(c, "eq_copper", (0, KP_Y + 0.181, z), (0.22, 0.004, 0.06))
    box(c, _GU, (0, KP_Y, 1.54), (0.30, 0.30, 0.08), chamfer=0.02 if lod == 0 else 0.0)              # muzzle block
    if lod < 2:
        box(c, _DA, (0, KP_Y, 1.582), (0.09, 0.09, 0.004))                                            # bore
        for sy in (-1, 1):                                               # cable channels along the barrel
            box(c, _GU, (0, KP_Y + sy * 0.125, 0.86), (0.07, 0.05, 1.26))
    if close():
        for sy in (-1, 1):
            _rivets(c, (-0.42, KP_Y + sy * 0.30, 0.18), (0.42, KP_Y + sy * 0.30, 0.18), 9, (0, 0, 1), 0.03)

    cp = P["kp30_caps"] = Part("kp30_caps")
    _xslab(cp, _GU, _cham(0.33, 1.00, -1.52, -1.16, 0.07), -0.66, 0.66)
    if lod < 2:
        for sx in (-1, 1):                                               # tile armour on the top
            box(cp, _TI, (sx * 0.32, 1.015, -1.34), (0.60, 0.03, 0.32), chamfer=0.01 if lod == 0 else 0.0)
        for i in range(5):                                               # vent slots on the back face
            box(cp, _DA, (0, 0.52 + i * 0.08, -1.522), (0.90, 0.03, 0.006))
    if lod == 0:
        _handle_bar(cp, (-0.35, 0.62, -1.53), (0.35, 0.62, -1.53), (0, 0, -1), 0.05)
        box(cp, "eq_status_amber", (0.50, 0.90, -1.523), (0.05, 0.03, 0.006))

    fd = P["kp30_feed"] = Part("kp30_feed")
    _xslab(fd, _GU, _cham(0.33, 0.98, -1.12, -0.12, 0.05), -0.93, -0.73)                              # magazine
    box(fd, _GR, (-0.62, 0.72, -0.55), (0.24, 0.22, 0.30))                                             # chute through the cheek
    if lod < 2:
        box(fd, _TI, (-0.935, 0.66, -0.62), (0.012, 0.50, 0.86))
    if lod == 0:
        for z in (-0.95, -0.30):
            _handle_bar(fd, (-0.94, 0.54, z), (-0.94, 0.80, z), (-1, 0, 0), 0.04)
    if close():                                                          # conduit in an armoured channel: caps -> breech
        hz = P["kp30_conduit"] = Part("kp30_conduit")
        box(hz, _GU, (0.40, 1.03, -1.05), (0.10, 0.06, 0.22))
        tube(hz, "eq_insulator", bezier((0.40, 1.02, -1.10), (0.40, 1.08, -0.95), (0.30, 1.08, -0.85), (0.30, KP_Y + 0.23, -0.80), 8), 0.025, 8)
    return P


def kp30_hits():
    return [hbox("kp30_mount", (0, 0.04, 0), (1.30, 0.08, 1.30)), hcyl("kp30_mount", (0, 0.08, 0), (0, 0.23, 0), 0.64),
            hbox("kp30_mount", (0, 0.28, -0.71), (1.32, 0.10, 1.62)),
            hbox("kp30_mount", (-0.62, 0.75, -0.58), (0.18, 0.84, 1.12)), hbox("kp30_mount", (0.62, 0.75, -0.58), (0.18, 0.84, 1.12)),
            hbox("kp30_mount", (0.84, 0.66, KP_Z - 0.20), (0.20, 0.42, 0.52)),
            hbox("kp30_coil", (0, KP_Y, 0.02), (1.00, 0.60, 0.32)), hbox("kp30_coil", (0, KP_Y, -0.62), (0.80, 0.46, 0.96)),
            hbox("kp30_coil", (0, KP_Y, 0.88), (0.36, 0.36, 1.40)),
            hbox("kp30_caps", (0, 0.665, -1.34), (1.32, 0.67, 0.36)),
            hbox("kp30_feed", (-0.83, 0.655, -0.62), (0.20, 0.65, 1.00))]


# ---------------------------------------------------------------- PT-2 point-defence laser turret (fa.wpn.pd2)
# Envelope 1.575 x 0.945 x 2.52: a low wedge hull (sloped glacis, tile skirts, rivet lines) on the 16-bolt square plate
# with the traverse ring (SRU mount); on top the boxy armoured emitter head with a recessed lens in a square hood frame,
# fins in an armoured channel and the tracker window (SRU emitter). No long barrel - the opposite silhouette of KP-30.
PT_Y = 0.62                                                             # emitter axis height


def pt2_parts(lod):
    ch = 0.03 if lod == 0 else 0.0
    fb = 0.03 if close() else 0.06
    P = {}
    m = P["pt2_mount"] = Part("pt2_mount")
    _sq_mount(m, 1.20, 0.07, lod, 0.06)
    lathe(m, _GU, [(0, 0.07), (0.56, 0.07), (0.56, 0.14), (0, 0.14)], 8, R_Y, (0, 0, 0), smooth=False)       # traverse ring
    hull = [(0.14, -1.16), (0.14, 0.72), (0.30, 0.88), (0.50, 0.66), (0.50, -0.94), (0.38, -1.16)]          # wedge (y, z)
    _xslab(m, _GR, hull, -0.72, 0.72)
    for sx in (-1, 1):                                                   # tile skirts on the sides
        if lod < 2:
            for z0, z1 in ((-1.10, -0.48), (-0.45, 0.13), (0.16, 0.72)):
                box(m, _TI, (sx * 0.735, 0.32, (z0 + z1) / 2), (0.03, 0.30, z1 - z0 - 0.02), chamfer=0.01 if lod == 0 else 0.0)
        else:
            box(m, _TI, (sx * 0.735, 0.32, -0.19), (0.03, 0.30, 1.82))
    if lod == 0:
        for sx in (-1, 1):
            _rivets(m, (sx * 0.72, 0.49, -0.90), (sx * 0.72, 0.49, 0.62), 12, (sx, 0, 0), fb)
        _rivets(m, (-0.65, 0.42, 0.785), (0.65, 0.42, 0.785), 10, (0, 0.6, 0.8), fb)
        _handle_bar(m, (-0.25, 0.28, -1.165), (0.25, 0.28, -1.165), (0, 0, -1), 0.05)

    e = P["pt2_emitter"] = Part("pt2_emitter")
    _xslab(e, _GU, _cham(PT_Y - 0.17, PT_Y + 0.22, -0.40, 0.92, 0.06), -0.30, 0.30)                    # emitter head
    for sx in (-1, 1):                                                   # trunnion plates to the hull top
        _xslab(e, _GR, _cham(0.48, PT_Y + 0.12, -0.25, 0.35, 0.04), min(sx * 0.30, sx * 0.40), max(sx * 0.30, sx * 0.40))
    yh = PT_Y + 0.02
    for sx, sy, w, hh in ((-1, 0, 0.06, 0.34), (1, 0, 0.06, 0.34), (0, -1, 0.22, 0.06), (0, 1, 0.22, 0.06)):   # lens hood frame
        box(e, _GU, (sx * 0.14, yh + sy * 0.14, 1.09), (w, hh, 0.34), chamfer=ch)
    box(e, "eq_fa_lens" if lod < 2 else _DA, (0, yh, 1.00), (0.22, 0.22, 0.16))                           # recessed lens
    if lod < 2:
        for i in range(7):                                               # cooling fins in an armoured channel
            box(e, _GR, (0, PT_Y + 0.27, -0.28 + i * 0.13), (0.38, 0.08, 0.03))
        for sx in (-1, 1):
            box(e, _GU, (sx * 0.22, PT_Y + 0.27, 0.11), (0.04, 0.10, 0.86))
        box(e, _GU, (-0.36, PT_Y + 0.02, 0.62), (0.12, 0.16, 0.22), chamfer=ch)                          # tracker
        box(e, "eq_fa_lens", (-0.36, PT_Y + 0.02, 0.732), (0.08, 0.10, 0.004))
    if close():
        _rivets(e, (-0.31, PT_Y + 0.05, -0.30), (-0.31, PT_Y + 0.05, 0.85), 10, (-1, 0, 0), 0.025)
        _rivets(e, (0.31, PT_Y + 0.05, -0.30), (0.31, PT_Y + 0.05, 0.85), 10, (1, 0, 0), 0.025)
        tube(e, "eq_insulator", bezier((0.25, 0.53, -0.75), (0.25, 0.60, -0.55), (0.20, PT_Y - 0.20, -0.45), (0.20, PT_Y - 0.17, -0.30), 8), 0.025, 8)
        box(e, _GU, (0.25, 0.525, -0.82), (0.10, 0.05, 0.22))           # armoured conduit channel on the hull roof
    return P


def pt2_hits():
    return [hbox("pt2_mount", (0, 0.035, 0), (1.20, 0.07, 1.20)), hbox("pt2_mount", (0, 0.32, -0.22), (1.50, 0.36, 1.88)),
            hbox("pt2_mount", (0, 0.25, 0.80), (1.44, 0.22, 0.16)),
            hbox("pt2_emitter", (0, PT_Y + 0.025, 0.26), (0.60, 0.39, 1.32)), hbox("pt2_emitter", (0, PT_Y + 0.02, 1.09), (0.34, 0.34, 0.34)),
            hbox("pt2_emitter", (0, PT_Y + 0.27, 0.11), (0.48, 0.10, 0.86)), hbox("pt2_emitter", (0, 0.55, 0.05), (0.80, 0.14, 0.60))]


ASSETS = {"firmament_bk1_armour": bk1_parts, "firmament_kp30_coilgun": kp30_parts, "firmament_pt2_pointdefence": pt2_parts}
HITS = {"firmament_bk1_armour": bk1_hits, "firmament_kp30_coilgun": kp30_hits, "firmament_pt2_pointdefence": pt2_hits}
MODEL_ASSETS = {"std.armour.ceramic": "firmament_bk1_armour", "fa.wpn.coil30": "firmament_kp30_coilgun", "fa.wpn.pd2": "firmament_pt2_pointdefence"}
