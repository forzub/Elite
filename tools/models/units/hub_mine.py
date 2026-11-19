"""Asteroid mine kit (author 2026-11-19: «сделай несколько моделей для шахт: что-то вроде большого роторного экскаватора,
тележки, рельсы - открытые карьеры на больших астероидах, и норы. посмотри в интернете, как выглядит шахта и поселение
рядом с ней. какие-то жилые постройки, склады, рудные кучи пирамидками, терминалы для больших транспортов, небольшой
пирс для маленьких кораблей и кораблей охраны»; addition: «может, какая-то шахта будет снаружи выглядеть как буровая
платформа ... много ферм. индустриальный дизайн, ущелья, каньоны - места, где можно полетать»).

New hubq_* hub modules (the existing hub kits are not touched) + one assembled demo, hubq_mine_site. References, sizes and
the low-gravity reasoning: docs/MINE_MODELS.md. Frame of every module: metres, +Y away from the rock (the module stands
on y = 0, the rock surface), +Z along its long side / travel direction. Rails: gauge 2.4 m, rail top RT = 1.0 m above
the surface (ballast bed + sleepers), so rolling stock and the excavator are placed at y = 0 on a rail line.

Low / micro gravity (a big asteroid: 0.01-0.3 m/s2) - every mechanism gets a cause that does not need Earth weight:
  - nothing relies on weight for traction: rail bogies wrap the rail head (upstop rollers under it, as on a roller
    coaster), the excavator rides two anchored tracks and drives spud anchors into the bench while it cuts;
  - loose ore never moves in the open: the bucket wheel discharges into a shroud, conveyors are pipe conveyors (the
    belt rolled into a closed tube), wagons have clamshell lids, stockpiles lie under tether nets on anchor posts;
  - habitats are covered with regolith (radiation shielding) and reject heat through radiator fins;
  - shaft hoisting is a closed rope loop (friction hoist with a tail rope) - the cage is pulled both ways.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, ROCK, STEEL, WARM, beacon  # noqa: F401
from units.hub_belt import GRATE, MARK, RAD, CONT, ROAD, lamp  # noqa: F401
from units.kern_heavy import BLK, RST, YEL  # the Kern Heavy Mining palette (mining yellow, black, rust)
from units.hub_vista import FLARE

register_materials({
    "eq_hubq_regolith": ((0.20, 0.19, 0.18), (0.03, 0.03, 0.03), 6),      # bench floors, ground, berms (dark dust)
    "eq_hubq_rock_cut": ((0.38, 0.35, 0.31), (0.05, 0.05, 0.05), 10),     # fresh cut faces (lighter)
    "eq_hubq_ore_iron": ((0.40, 0.19, 0.11), (0.05, 0.04, 0.04), 8),      # iron / Fe-Ni ore (red-brown)
    "eq_hubq_ore_grey": ((0.30, 0.31, 0.30), (0.10, 0.10, 0.10), 20),     # nickel-PGM concentrate (grey, metallic)
    "eq_hubq_net": ((0.85, 0.42, 0.10), (0.10, 0.10, 0.10), 20),          # tether nets / lashings (orange)
    "eq_hubq_concrete": ((0.46, 0.45, 0.42), (0.06, 0.06, 0.06), 10),     # sintered-regolith concrete
})
REG, CUT, ORE, OREG, NET, CON = ("eq_hubq_regolith", "eq_hubq_rock_cut", "eq_hubq_ore_iron", "eq_hubq_ore_grey",
                                 "eq_hubq_net", "eq_hubq_concrete")
HOLE = "eq_hole"
TAU = 2 * math.pi
RT, GAUGE = 1.0, 2.4                     # rail top above the surface, track gauge


# ================================================================ local kernel
def merge(dst, src):
    off = len(dst.v)
    dst.v += src.v
    dst.n += src.n
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + off for i in f) for f in fl)


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _mul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _len(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def _lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def bar(p, mat, a, b, w, h=None):
    """a square bar (box) from a to b, w x h section (cheaper than a cylinder for lattice members)"""
    d = _sub(b, a)
    L = _len(d)
    if L < 1e-6:
        return
    box(p, mat, _lerp(a, b, 0.5), (w, h or w, L), axis_to(norm(d)))


def quad(p, mat, pts, hint):
    p.face(mat, [p.vert(q, hint) for q in pts], hint)


def tri_face(p, mat, a, b, c, up=(0.0, 1.0, 0.0)):
    """a flat triangle, normal turned towards `up`"""
    n = norm(_cross(_sub(b, a), _sub(c, a)))
    if n[0] * up[0] + n[1] * up[1] + n[2] * up[2] < 0:
        n = (-n[0], -n[1], -n[2])
    p.face(mat, [p.vert(a, n), p.vert(b, n), p.vert(c, n)], n)


def truss(p, mat, a, b, w, h, lod, up=(0.0, 1.0, 0.0), chord=None, bay=None, solid_mat=None, faces="all"):
    """a box lattice girder from a to b (w wide, h deep): 4 chords, verticals and Warren diagonals on each face;
    lod 2 = a plain box. faces: "all" or "sides" (a through truss: side trusses + bottom deck bracing)"""
    d = _sub(b, a)
    L = _len(d)
    if L < 1e-6:
        return
    dn = norm(d)
    side = _cross(dn, up)
    if _len(side) < 1e-6:
        side = _cross(dn, (1.0, 0.0, 0.0))
    side = norm(side)
    upv = norm(_cross(side, dn))
    if lod >= 2:
        R = [[side[0], upv[0], dn[0]], [side[1], upv[1], dn[1]], [side[2], upv[2], dn[2]]]
        box(p, solid_mat or mat, _lerp(a, b, 0.5), (w, h, L), R)
        return
    c = chord or max(0.25, min(w, h) * 0.09)
    bay = bay or max(w, h)
    n = max(1, int(round(L / bay)))
    corners = [(sx, sy) for sx in (-0.5, 0.5) for sy in (-0.5, 0.5)]
    off = {k: _add(_mul(side, k[0] * w), _mul(upv, k[1] * h)) for k in corners}
    for k in corners:
        bar(p, mat, _add(a, off[k]), _add(b, off[k]), c)
    if lod == 1:
        n = max(1, n // 2)
        for i in range(n):
            q, q1 = _lerp(a, b, i / n), _lerp(a, b, (i + 1) / n)
            for k0, k1 in (((-0.5, -0.5), (-0.5, 0.5)), ((0.5, -0.5), (0.5, 0.5))):
                bar(p, mat, _add(q, off[k0]), _add(q1, off[k1]), c * 0.7)
        return
    for i in range(n + 1):
        q = _lerp(a, b, i / n)
        ring = [(-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)]
        for j in range(4):
            k0, k1 = ring[j], ring[(j + 1) % 4]
            if faces == "sides" and j == 2:       # no top members over a through truss
                continue
            bar(p, mat, _add(q, off[k0]), _add(q, off[k1]), c * 0.8)
        if i < n:
            q1 = _lerp(a, b, (i + 1) / n)
            for j in range(4):
                if faces == "sides" and j == 2:
                    continue
                k0, k1 = ring[j], ring[(j + 1) % 4]
                if i % 2 == 0:
                    bar(p, mat, _add(q, off[k0]), _add(q1, off[k1]), c * 0.7)
                else:
                    bar(p, mat, _add(q, off[k1]), _add(q1, off[k0]), c * 0.7)


def lattice_tower(p, mat, base, y0, y1, h0, h1, lod, bays=None, chord=None):
    """a 4-leg lattice tower about the vertical through base (x, z): half widths h0 at y0, h1 at y1, X bracing"""
    bx, bz = base
    bays = bays or max(2, int((y1 - y0) / max(4.0, (h0 + h1))))
    c = chord or max(0.35, (h0 + h1) * 0.06)
    if lod >= 2:
        n = 4
        poly0 = [(bx + sx * h0, bz + sz * h0) for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        # a tapered solid: four sloped faces + top
        pts0 = [(x, y0, z) for x, z in poly0]
        pts1 = [(bx + sx * h1, y1, bz + sz * h1) for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        for k in range(n):
            a0, a1, b1, b0 = pts0[k], pts0[(k + 1) % n], pts1[(k + 1) % n], pts1[k]
            mid = _lerp(_lerp(a0, a1, 0.5), _lerp(b0, b1, 0.5), 0.5)
            hint = norm((mid[0] - bx, 0.0, mid[2] - bz))
            quad(p, mat, [a0, a1, b1, b0], hint)
        quad(p, mat, pts1, (0, 1, 0))
        return
    if lod == 1:
        bays = max(2, bays // 2)
    legs = []
    for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        legs.append(((bx + sx * h0, y0, bz + sz * h0), (bx + sx * h1, y1, bz + sz * h1)))
    for a, b in legs:
        bar(p, mat, a, b, c)
    for i in range(bays + 1):
        t = i / bays
        ring = [_lerp(a, b, t) for a, b in legs]
        for k in range(4):
            bar(p, mat, ring[k], ring[(k + 1) % 4], c * 0.75)
        if i < bays:
            ring1 = [_lerp(a, b, (i + 1) / bays) for a, b in legs]
            for k in range(4):
                bar(p, mat, ring[k], ring1[(k + 1) % 4], c * 0.6)
                if lod == 0:
                    bar(p, mat, ring[(k + 1) % 4], ring1[k], c * 0.6)


def extrude_z(p, prof, mats, z0, z1, base_y, caps=True):
    """a rock body: the (x, y) top profile (ordered from +x to -x, a function y(x)) extruded along Z from z0 to z1, down
    to base_y; mats[k] = material of top segment k; the ends, sides and bottom in mats[-1]"""
    for k in range(len(prof) - 1):
        (x0, y0), (x1, y1) = prof[k], prof[k + 1]
        tx, ty = x1 - x0, y1 - y0
        n = norm((ty, -tx, 0.0))
        quad(p, mats[k], [(x0, y0, z0), (x1, y1, z0), (x1, y1, z1), (x0, y0, z1)], n)
    side = mats[-1]
    if caps:
        for z, s in ((z0, -1.0), (z1, 1.0)):
            for k in range(len(prof) - 1):
                (x0, y0), (x1, y1) = prof[k], prof[k + 1]
                quad(p, side, [(x0, base_y, z), (x1, base_y, z), (x1, y1, z), (x0, y0, z)], (0, 0, s))
    xa, ya = prof[0]
    xb, yb = prof[-1]
    quad(p, side, [(xa, base_y, z0), (xa, ya, z0), (xa, ya, z1), (xa, base_y, z1)], (1 if xa > xb else -1, 0, 0))
    quad(p, side, [(xb, base_y, z0), (xb, yb, z0), (xb, yb, z1), (xb, base_y, z1)], (-1 if xa > xb else 1, 0, 0))
    quad(p, side, [(xa, base_y, z0), (xb, base_y, z0), (xb, base_y, z1), (xa, base_y, z1)], (0, -1, 0))


def _hash(*a):
    """a deterministic pseudo-random number in [0, 1) from integers / floats (no global RNG state)"""
    h = 2166136261
    for v in a:
        for ch in repr(round(v, 3) if isinstance(v, float) else v):
            h = ((h ^ ord(ch)) * 16777619) & 0xFFFFFFFF
    return (h % 100000) / 100000.0


def boulder(p, mat, c, s, k):
    """a chamfered, turned box as a rock lump"""
    R = mat_mul(rot_y(TAU * _hash(k, 1)), rot_x(0.4 * (_hash(k, 2) - 0.5)))
    box(p, mat, c, (s * (0.8 + 0.5 * _hash(k, 3)), s * (0.5 + 0.3 * _hash(k, 4)), s * (0.8 + 0.5 * _hash(k, 5))), R,
        chamfer=s * 0.18)


# ================================================================ rails, rolling stock
RAIL_L = 50.0


def sleeper_track(p, lod, pts, heads=True):
    """a track along a polyline of (x, z, heading) points (heading: tangent angle, rot_y), surface at y 0"""
    for i in range(len(pts) - 1):
        (xa, za, ha), (xb, zb, hb) = pts[i], pts[i + 1]
        a, b = (xa, 0.0, za), (xb, 0.0, zb)
        L = _len(_sub(b, a))
        h = (ha + hb) / 2
        R = rot_y(h)
        mid = _lerp(a, b, 0.5)
        box(p, REG, _add(mid, (0, 0.15, 0)), (4.6, 0.3, L + 0.02), R)                       # ballast / sintered bed
        for sx in (-1, 1):
            off = apply(R, (sx * GAUGE / 2, 0, 0))
            box(p, STEEL, _add(mid, _add(off, (0, RT - 0.08, 0))), (0.36, 0.16, L + 0.01), R)      # rail head
            if lod < 2:
                box(p, STEEL, _add(mid, _add(off, (0, 0.72, 0))), (0.10, 0.40, L + 0.01), R)       # web
                box(p, STEEL, _add(mid, _add(off, (0, 0.53, 0))), (0.40, 0.06, L + 0.01), R)       # foot
        if lod < 2:                                                                          # the third rail (power)
            off = apply(R, (2.05, 0, 0))
            box(p, COOL if lod == 0 else STEEL, _add(mid, _add(off, (0, 0.78, 0))), (0.22, 0.12, L), R)
    if lod < 2:
        # sleepers + rock anchors: every 1.5 m (close) / 3 m
        step = 1.5 if (lod == 0 and close()) else 3.0
        acc = 0.0
        for i in range(len(pts) - 1):
            (xa, za, ha), (xb, zb, hb) = pts[i], pts[i + 1]
            L = math.hypot(xb - xa, zb - za)
            t = acc
            while t < L:
                q = (xa + (xb - xa) * t / L, 0.0, za + (zb - za) * t / L)
                R = rot_y(ha + (hb - ha) * t / L)
                box(p, STEEL if lod == 0 else LIGHT, _add(q, (0, 0.4, 0)), (3.4, 0.2, 0.3), R)
                if close():
                    for sx in (-1, 1):
                        box(p, HAZ, _add(q, apply(R, (sx * 1.55, 0.53, 0))), (0.12, 0.08, 0.12))  # anchor bolt heads
                    box(p, LIGHT, _add(q, apply(R, (2.05, 0.6, 0))), (0.16, 0.25, 0.16))         # third-rail insulator
                t += step
            acc = t - L


def rail_parts(lod):
    P = {"hubq_rail": Part("hubq_rail")}
    sleeper_track(P["hubq_rail"], lod, [(0.0, 0.0, 0.0), (0.0, RAIL_L, 0.0)])
    return P


CURVE_R, CURVE_DEG = 100.0, 15.0


def curve_points(n):
    """the curve piece: start at the origin heading +Z, centre of curvature at (+R, 0, 0); heading grows by CURVE_DEG"""
    out = []
    for i in range(n + 1):
        f = math.radians(CURVE_DEG) * i / n
        out.append((CURVE_R - CURVE_R * math.cos(f), CURVE_R * math.sin(f), f))
    return out


def curve_end():
    f = math.radians(CURVE_DEG)
    return (CURVE_R - CURVE_R * math.cos(f), 0.0, CURVE_R * math.sin(f))


def rail_curve_parts(lod):
    P = {"hubq_rail_curve": Part("hubq_rail_curve")}
    sleeper_track(P["hubq_rail_curve"], lod, curve_points({0: 12, 1: 6, 2: 3}[lod]))
    return P


def rail_end_parts(lod):
    """a buffer stop: a track stub (z 0..8) and a braced frame with two buffers facing -Z, red lamp"""
    P = {"hubq_rail_end": Part("hubq_rail_end")}
    e = P["hubq_rail_end"]
    sleeper_track(e, lod, [(0.0, 0.0, 0.0), (0.0, 8.0, 0.0)])
    box(e, CON, (0, 1.2, 9.0), (5.0, 2.4, 2.0))                                           # anchored block
    for sx in (-1, 1):
        bar(e, YEL, (sx * 1.2, 1.0, 6.5), (sx * 1.2, 2.6, 6.5), 0.4)
        bar(e, YEL, (sx * 1.2, 2.6, 6.5), (sx * 1.2, 1.0, 8.6), 0.35)
    box(e, YEL, (0, 2.4, 6.5), (3.4, 0.6, 0.6))
    for sx in (-1, 1):
        cyl(e, BLK, (sx * 0.9, 2.4, 6.2), (sx * 0.9, 2.4, 5.3), 0.3, small_for(lod))
    beacon(e, (0, 3.0, 6.5), 0.4)
    if lod < 2:
        box(e, HAZ, (0, 1.6, 7.95), (4.8, 0.6, 0.1))
    return P


def bogie(p, lod, z, wheel_r=0.45):
    """a two-axle bogie straddling the rails at RT; upstop rollers under the rail head hold it down"""
    box(p, BLK, (0, RT + 0.75, z), (GAUGE + 0.6, 0.5, 2.8))
    seg = small_for(lod)
    for dz in (-0.9, 0.9):
        for sx in (-1, 1):
            x = sx * GAUGE / 2
            cyl(p, STEEL, (x - 0.12, RT + wheel_r, z + dz), (x + 0.12, RT + wheel_r, z + dz), wheel_r, seg)
            if lod < 2:
                box(p, BLK, (sx * (GAUGE / 2 + 0.32), RT + 0.25, z + dz), (0.14, 0.9, 0.5))      # upstop arm
                cyl(p, STEEL, (x - 0.1, RT - 0.3, z + dz), (x + 0.1, RT - 0.3, z + dz), 0.12, 6)  # roller under the head
        if lod < 2:
            cyl(p, STEEL, (-GAUGE / 2 - 0.2, RT + wheel_r, z + dz), (GAUGE / 2 + 0.2, RT + wheel_r, z + dz), 0.12, 6)


def wagon_parts(lod):
    """ore wagon 12.6 m over couplers: a lidded hopper 3.2 x 2.9 x 11.4 (~75 m3, ~100 t of ore - the LKAB 100 t class
    of the Kiruna-Narvik line); clamshell lids so nothing floats off, bottom-dump doors"""
    P = {"hubq_ore_wagon": Part("hubq_ore_wagon")}
    w = P["hubq_ore_wagon"]
    for z in (-4.2, 4.2):
        bogie(w, lod, z)
    box(w, BLK, (0, RT + 1.2, 0), (2.6, 0.5, 12.0))                                       # underframe
    y0 = RT + 1.45
    box(w, RST, (0, y0 + 0.9, 0), (2.0, 1.8, 10.6))                                       # hopper bottom
    for sx in (-1, 1):                                                                    # sloped hopper sides
        box(w, RST, (sx * 1.15, y0 + 1.25, 0), (0.7, 1.5, 10.4), rot_z(sx * -0.5))
    box(w, RST, (0, y0 + 2.35, 0), (3.2, 1.3, 11.4))                                      # upper body
    for sx in (-1, 1):                                                                    # clamshell lids, peaked
        box(w, YEL, (sx * 0.8, y0 + 3.15, 0), (1.7, 0.18, 11.5), rot_z(sx * -0.12))
    if lod < 2:
        box(w, BLK, (0, y0 + 3.3, 0), (0.3, 0.2, 11.5))                                   # hinge spine
        for z in [-5.0 + 1.25 * k for k in range(9)]:                                     # side stakes
            for sx in (-1, 1):
                box(w, BLK, (sx * 1.63, y0 + 2.3, z), (0.12, 1.4, 0.25))
        for sz in (-1, 1):
            box(w, GRATE, (0, RT + 1.5, sz * 5.95), (2.8, 0.1, 0.8))                       # end platforms
            cyl(w, BLK, (0, RT + 1.2, sz * 6.0), (0, RT + 1.2, sz * 6.3), 0.2, 6)          # couplers
        box(w, HAZ, (1.62, y0 + 2.6, 0), (0.06, 0.4, 4.0))
        box(w, HAZ, (-1.62, y0 + 2.6, 0), (0.06, 0.4, 4.0))
    if close():
        for z in (-3.0, 0.0, 3.0):
            cyl(w, BLK, (1.0, y0 + 3.0, z), (1.0, y0 + 2.0, z + 0.6), 0.08, 6)            # lid rams
            cyl(w, BLK, (-1.0, y0 + 3.0, z), (-1.0, y0 + 2.0, z + 0.6), 0.08, 6)
    return P


def loco_parts(lod):
    """a small electric shunting locomotive 14 m: cab at the -Z end (pressurised, windows), equipment hood, battery
    boxes, third-rail shoes, lights"""
    P = {"hubq_loco": Part("hubq_loco")}
    l = P["hubq_loco"]
    for z in (-4.2, 4.2):
        bogie(l, lod, z, 0.5)
    box(l, BLK, (0, RT + 1.25, 0), (3.0, 0.6, 13.4))                                      # frame
    box(l, YEL, (0, RT + 3.0, -4.6), (3.2, 2.9, 4.0), chamfer=0.25)                        # cab
    box(l, YEL, (0, RT + 2.6, 2.4), (2.6, 2.1, 9.4), chamfer=0.2)                          # long hood
    box(l, DARK, (0, RT + 3.6, -6.61), (2.6, 1.0, 0.05))                                   # front windscreen
    for sx in (-1, 1):
        box(l, DARK, (sx * 1.61, RT + 3.6, -4.6), (0.05, 0.9, 2.6))
    if lod < 2:
        for z in (0.0, 2.4, 4.8):
            box(l, RAD, (0, RT + 3.68, z), (1.8, 0.06, 1.8))                               # roof radiators
        for sx in (-1, 1):
            box(l, BLK, (sx * 1.35, RT + 1.95, 2.5), (0.6, 0.8, 6.0))                      # battery boxes
            box(l, HAZ, (sx * 1.61, RT + 1.6, 0), (0.05, 0.3, 13.0))
        box(l, STEEL, (2.05, RT - 0.1, 0.0), (0.4, 0.15, 1.2))                               # third-rail shoe
        for sx in (-1, 1):
            box(l, FLOOD, (sx * 1.0, RT + 2.0, -6.75), (0.5, 0.35, 0.1))                    # headlights
        box(l, GRATE, (0, RT + 1.6, -6.9), (3.0, 0.1, 0.8))
    beacon(l, (0, RT + 4.6, -4.6), 0.35)
    if close():
        box(l, STEEL, (0, RT + 4.5, -3.8), (0.1, 0.8, 0.1))                                # antenna
        for sx in (-1, 1):
            bar(l, STEEL, (sx * 1.6, RT + 1.6, -6.7), (sx * 1.6, RT + 3.4, -6.7), 0.06)    # grab rails
    return P


# ================================================================ enclosed (pipe) conveyor and conveyor bridge
CONV_Y = 6.0          # belt centre height above the surface


def pipe_conveyor(p, lod, a, b, legs=True, gallery=True):
    """a pipe conveyor from a to b (belt centre points): carrying and return tubes (0.7 m), hexagonal idler panels,
    a walkway; legs (A-frame trestles anchored in the rock) every 25 m down to y = a[1] - CONV_Y"""
    d = _sub(b, a)
    L = _len(d)
    dn = norm(d)
    side = norm(_cross(dn, (0.0, 1.0, 0.0)))
    upv = norm(_cross(side, dn))
    seg = small_for(lod)
    for o in (0.55, -0.55):
        off = _mul(upv, o)
        if lod < 2:
            cyl(p, LIGHT, _add(a, off), _add(b, off), 0.36, seg, caps=False)
        else:
            bar(p, LIGHT, _add(a, off), _add(b, off), 0.7, 0.7)
    if lod < 2:
        n = int(L / (2.5 if (lod == 0 and close()) else 5.0))
        for i in range(n + 1):
            q = _lerp(a, b, i / max(1, n))
            R = axis_to(dn)
            lathe(p, STEEL, crect(0.5, 0.62, -0.08, 0.08), 6, R, _add(q, _mul(upv, 0.55)), smooth=False)
            lathe(p, STEEL, crect(0.5, 0.62, -0.08, 0.08), 6, R, _add(q, _mul(upv, -0.55)), smooth=False)
            bar(p, STEEL, _add(q, _add(_mul(side, -0.8), _mul(upv, -1.3))), _add(q, _add(_mul(side, -0.8), _mul(upv, 1.3))), 0.14)
        if gallery:
            box(p, GRATE, _add(_lerp(a, b, 0.5), _add(_mul(side, 1.3), _mul(upv, -1.15))), (1.4, 0.1, L),
                [[side[0], upv[0], dn[0]], [side[1], upv[1], dn[1]], [side[2], upv[2], dn[2]]])
            bar(p, HAZ, _add(a, _add(_mul(side, 2.0), _mul(upv, -0.1))), _add(b, _add(_mul(side, 2.0), _mul(upv, -0.1))), 0.08)
    # the box girder carrying it all (a light truss)
    truss(p, YEL, _add(a, _mul(upv, -1.6)), _add(b, _mul(upv, -1.6)), 3.4, 1.0, max(lod, 1) if lod < 2 else 2,
          solid_mat=YEL)
    if legs:
        n = max(1, int(round(L / 25.0)))
        for i in range(n):
            q = _lerp(a, b, i / n)
            foot_y = q[1] - CONV_Y if legs is True else legs
            for s in (-1, 1):
                top = _add(q, _add(_mul(side, s * 1.6), _mul(upv, -2.1)))
                bot = (q[0] + side[0] * s * 3.2, foot_y, q[2] + side[2] * s * 3.2)
                bar(p, YEL, bot, top, 0.35)
                if lod < 2:
                    box(p, CON, (bot[0], foot_y + 0.2, bot[2]), (1.4, 0.4, 1.4))
            if lod < 2:
                bar(p, YEL, _add(q, _add(_mul(side, -2.2), (0, -3.6, 0))), _add(q, _add(_mul(side, 2.2), (0, -3.6, 0))), 0.25)


def conveyor_parts(lod):
    P = {"hubq_conveyor": Part("hubq_conveyor")}
    c = P["hubq_conveyor"]
    pipe_conveyor(c, lod, (0.0, CONV_Y, 0.0), (0.0, CONV_Y, 50.0))
    if lod < 2:
        lamp(c, 2.3, CONV_Y - 1.1, 12.5, 2.0)
    return P


BRIDGE_L, BRIDGE_H, BRIDGE_W = 200.0, 14.0, 12.0


def bridge_parts(lod):
    """a through-truss conveyor bridge, 200 m span along +Z (abutments at z 0 and 200, deck at y 0): Warren side
    trusses 14 m deep, deck bracing, a pipe conveyor and a walkway on the deck - spans a canyon, ships fly under it"""
    P = {"hubq_bridge": Part("hubq_bridge")}
    b = P["hubq_bridge"]
    W, H, L = BRIDGE_W, BRIDGE_H, BRIDGE_L
    for sx in (-1, 1):
        if lod < 2:
            n = 20 if lod == 0 else 10
            for i in range(n + 1):
                z = L * i / n
                bar(b, YEL, (sx * W / 2, 0.0, z), (sx * W / 2, H, z), 0.6)
                if i < n:
                    z1 = L * (i + 1) / n
                    if i % 2 == 0:
                        bar(b, YEL, (sx * W / 2, 0.0, z), (sx * W / 2, H, z1), 0.5)
                    else:
                        bar(b, YEL, (sx * W / 2, H, z), (sx * W / 2, 0.0, z1), 0.5)
                    bar(b, YEL, (-W / 2, 0.0, z), (W / 2, 0.0, z1), 0.35)               # deck bracing
                    if i % 2 == 0:
                        bar(b, YEL, (-W / 2, H, z), (W / 2, H, z), 0.4)                    # top struts
            for y in (0.0, H):
                bar(b, YEL, (sx * W / 2, y, 0.0), (sx * W / 2, y, L), 0.9)               # chords
        else:
            box(b, YEL, (sx * W / 2, H / 2, L / 2), (0.8, H, L))
    box(b, GRATE, (0, 0.2, L / 2), (W - 1.0, 0.3, L))                                     # deck
    pipe_conveyor(b, lod, (-2.5, 2.4, 0.0), (-2.5, 2.4, L), legs=False, gallery=False)
    for z in (0.0, L):                                                                    # abutments
        box(b, CON, (0, -3.0, z), (W + 6.0, 6.0, 10.0))
    if lod < 2:
        for z in range(10, int(L), 40):
            for sx in (-1, 1):
                box(b, FLOOD, (sx * W / 2, -0.6, float(z)), (0.8, 0.5, 0.8))
            box(b, BEACON, (0, -0.8, float(z)), (0.6, 0.6, 0.6))                         # underside: air-gap marks
    for z in (L * 0.25, L * 0.5, L * 0.75):
        for sx in (-1, 1):
            beacon(b, (sx * W / 2, H + 0.8, z), 0.9)
    return P


def bridge_hits():
    return [hbox("hubq_bridge", (0, H2, BRIDGE_L / 2), (BRIDGE_W + 1.0, BRIDGE_H + 1.0, BRIDGE_L))
            for H2 in (BRIDGE_H / 2,)] + [hbox("hubq_bridge", (0, -3.0, z), (BRIDGE_W + 6.0, 6.0, 10.0)) for z in (0.0, BRIDGE_L)]


# ================================================================ bucket-wheel excavator
# Reference: Bagger 288 (220 m, 96 m high, wheel 21.6 m, 18 buckets x 6.6 m3, 13 500 t, 240 000 m3/day). Ours is a
# medium "SRs 1300"-class machine scaled for asteroid benches: wheel 18 m, 14 buckets, wheel boom 45 m.
# Low gravity: the machine weighs ~1/40..1/500 of its Earth weight, so cutting reactions go into anchored rails (two
# standard tracks under it, bogies with upstop rollers) and four spud anchors driven into the bench; the wheel discharges
# into a shroud and a pipe conveyor (no free fall chute); the discharge boom fills lidded wagons beside the machine.
BWE_SLEW = 69.0          # rot_y of the superstructure: the wheel boom 21 deg off the travel axis towards +X (the face)
BWE_REACH, BWE_WR, BWE_WY = 45.0, 9.0, 10.0
BWE_TRACK = 8.0          # the two tracks under it: x = +-8
BWE_DISCHARGE = -16.0    # the loading chute over a track at x = -16


def bwe_super(lod):
    """the slewing superstructure, boom along local +X, slew centre at the origin, slew ring top y 9"""
    s = Part("sup")
    seg = seg_for(lod)
    small = small_for(lod)
    y0 = 9.0
    box(s, YEL, (-3.0, y0 + 6.0, 0), (16.0, 12.0, 13.0), chamfer=0.4)                     # machinery house
    box(s, BLK, (-3.0, y0 + 12.4, 0), (14.0, 0.8, 11.0))
    if lod < 2:
        for z in (-6.55, 6.55):
            for x in (-8.0, -4.0, 0.0, 4.0):
                box(s, DARK, (x, y0 + 8.0, z), (2.4, 1.2, 0.1))
        box(s, RAD, (-3.0, y0 + 13.6, 0), (10.0, 1.6, 0.4))
    # mast (A-frame pylon) and its head
    apex = (-6.0, 46.0, 0.0)
    for sz in (-1, 1):
        truss(s, YEL, (-1.0, y0 + 12.0, sz * 5.0), apex, 1.8, 1.8, lod, up=(1.0, 0.0, 0.0), solid_mat=YEL)
        truss(s, YEL, (-11.0, y0 + 12.0, sz * 4.0), apex, 1.6, 1.6, lod, up=(1.0, 0.0, 0.0), solid_mat=YEL)
    box(s, YEL, apex, (4.0, 3.0, 4.0))
    beacon(s, _add(apex, (0, 2.2, 0)), 1.0)
    # wheel boom: a lattice girder from the house to the wheel
    wc = (BWE_REACH, BWE_WY, 0.0)
    root = (5.0, y0 + 7.0, 0.0)
    truss(s, YEL, root, (BWE_REACH - 6.0, BWE_WY + 1.5, 0.0), 4.2, 4.2, lod, solid_mat=YEL)
    # the boom's pipe conveyor (from the shroud back to the slew centre)
    if lod < 2:
        cyl(s, LIGHT, (BWE_REACH - 6.5, BWE_WY + 4.6, 0.0), (0.0, y0 + 10.4, 0.0), 0.55, small)
        cyl(s, LIGHT, (0.0, y0 + 10.4, 0.0), (0.0, y0 - 1.0, 0.0), 0.7, small)            # down the slew centre
    # operator cab under the boom near the wheel
    box(s, YEL, (BWE_REACH - 14.0, BWE_WY - 2.0, 3.4), (5.0, 3.0, 3.0), chamfer=0.2)
    box(s, DARK, (BWE_REACH - 11.45, BWE_WY - 1.6, 3.4), (0.1, 1.4, 2.6))
    if lod < 2:
        box(s, DARK, (BWE_REACH - 14.0, BWE_WY - 1.6, 4.91), (4.0, 1.4, 0.1))
    # the wheel (plane = local XY, axis local Z)
    box(s, BLK, (BWE_REACH - 3.0, BWE_WY, 2.3), (6.0, 2.0, 1.4))                           # gearbox / drive
    cyl(s, STEEL, (BWE_REACH, BWE_WY, -1.6), (BWE_REACH, BWE_WY, 3.2), 1.4, small)          # hub + shaft
    lathe(s, STEEL, crect(BWE_WR - 1.6, BWE_WR - 0.9, -1.3, 1.3), seg, I3, wc, smooth=False)  # rim ring
    nb = 14
    if lod < 2:
        for k in range(7):                                                                 # spokes
            a = TAU * k / 7
            bar(s, STEEL, _add(wc, (1.2 * math.cos(a), 1.2 * math.sin(a), 0.0)),
                _add(wc, ((BWE_WR - 1.4) * math.cos(a), (BWE_WR - 1.4) * math.sin(a), 0.0)), 0.6, 0.9)
    for k in range(nb):
        a = TAU * k / nb
        R = rot_z(a)
        c = _add(wc, ((BWE_WR - 0.4) * math.cos(a), (BWE_WR - 0.4) * math.sin(a), 0.0))
        box(s, YEL if lod < 2 else STEEL, c, (1.6, 2.2, 2.6), R)                           # bucket
        if lod < 2:
            lip = _add(wc, ((BWE_WR + 0.35) * math.cos(a - 0.08), (BWE_WR + 0.35) * math.sin(a - 0.08), 0.0))
            box(s, BLK, lip, (0.3, 1.0, 2.7), rot_z(a - 0.08))
            if close():
                for tz in (-1.0, 0.0, 1.0):
                    box(s, STEEL, _add(lip, (0.0, 0.0, tz)), (0.6, 0.25, 0.25), rot_z(a - 0.08))   # teeth
    # the shroud: an annular hood over the upper-rear part of the wheel (catches the discharged ore)
    a0, a1 = math.radians(40.0), math.radians(200.0)
    lathe(s, BLK, crect(BWE_WR + 1.0, BWE_WR + 1.5, -1.9, 1.9), seg // 2 or 8, I3, wc, a0, a1, smooth=False, cap=True)
    for z0, z1 in ((1.9, 2.2), (-2.2, -1.9)):
        lathe(s, BLK, crect(BWE_WR - 2.6, BWE_WR + 1.5, z0, z1), seg // 2 or 8, I3, wc, a0, a1, smooth=False, cap=True)
    box(s, BLK, (BWE_REACH - 6.0, BWE_WY + 4.4, 0.0), (3.0, 2.4, 3.6), rot_z(0.4))         # discharge chute
    # counterweight boom
    cw = (-36.0, y0 + 6.0, 0.0)
    truss(s, YEL, (-10.0, y0 + 6.0, 0.0), cw, 3.4, 3.4, lod, solid_mat=YEL)
    box(s, RST, _add(cw, (-2.0, 0.0, 0.0)), (9.0, 9.0, 10.0), chamfer=0.3)
    # stays from the mast head to the wheel boom tip and to the counterweight
    for tgt in ((BWE_REACH - 7.0, BWE_WY + 3.5, 0.0), (BWE_REACH * 0.5, y0 + 7.0, 0.0), _add(cw, (0.0, 4.5, 0.0)),
                (-22.0, y0 + 8.0, 0.0)):
        for sz in (-0.6, 0.6):
            cyl(s, STEEL, _add(apex, (0, 0, sz)), _add(tgt, (0, 0, sz)), 0.18, 6)
    if lod < 2:
        box(s, HAZ, (-3.0, y0 + 0.6, 6.55), (14.0, 0.8, 0.1))
        box(s, HAZ, (-3.0, y0 + 0.6, -6.55), (14.0, 0.8, 0.1))
        for x in (10.0, 20.0, 30.0):
            box(s, FLOOD, (x, y0 + 4.5 + (x - 5.0) * -0.08, 2.4), (0.8, 0.6, 0.8))
        beacon(s, (BWE_REACH - 6.0, BWE_WY + 4.5, 2.6), 0.6)
        beacon(s, _add(cw, (-2.0, 5.0, 0.0)), 0.7)
    return s


def bwe_parts(lod):
    P = {"hubq_bucket_wheel": Part("hubq_bucket_wheel")}
    e = P["hubq_bucket_wheel"]
    small = small_for(lod)
    # travelling gear: three bogie pairs on each of the two tracks
    for sx in (-1, 1):
        x = sx * BWE_TRACK
        for z in (-14.0, 0.0, 14.0):
            box(e, BLK, (x, RT + 1.5, z), (GAUGE + 1.0, 1.6, 9.0))
            for dz in (-3.0, -1.0, 1.0, 3.0):
                for wx in (-1, 1):
                    cyl(e, STEEL, (x + wx * GAUGE / 2 - 0.15, RT + 0.6, z + dz), (x + wx * GAUGE / 2 + 0.15, RT + 0.6, z + dz), 0.6, small)
                    if lod < 2:
                        box(e, BLK, (x + wx * (GAUGE / 2 + 0.4), RT + 0.2, z + dz), (0.2, 1.4, 0.6))    # upstop arms
            box(e, BLK, (x, RT + 3.0, z), (2.0, 1.6, 2.0))                                 # equaliser pin
        truss(e, BLK, (x, RT + 4.3, -20.0), (x, RT + 4.3, 20.0), 2.6, 2.0, lod, solid_mat=BLK)   # side girder
    box(e, BLK, (0, 6.2, 0), (2 * BWE_TRACK + 2.6, 1.6, 14.0))                            # cross girder (portal)
    ylathe(e, BLK, [(0.0, 7.0), (9.0, 7.0), (9.0, 9.0), (0.0, 9.0)], seg_for(lod))         # slew ring
    if lod < 2:
        ylathe(e, YEL, [(9.0, 7.6), (9.3, 7.6), (9.3, 8.4), (9.0, 8.4)], seg_for(lod))
    # spud anchors: hydraulic legs drilled into the bench
    for sx in (-1, 1):
        for sz in (-1, 1):
            x, z = sx * (BWE_TRACK + 3.2), sz * 18.0
            box(e, BLK, (x, RT + 4.3, z), (2.2, 1.6, 2.2))
            cyl(e, STEEL, (x, RT + 3.5, z), (x, -2.0, z), 0.5, small)
            cyl(e, YEL, (x, RT + 5.1, z), (x, RT + 1.0, z), 0.75, small)
            box(e, BLK, (x, 0.15, z), (2.4, 0.3, 2.4))
    # cable reel (trailing power cable, as on the real machines)
    cyl(e, YEL, (-3.0, 5.0, 22.0), (3.0, 5.0, 22.0), 2.6, seg_for(lod))
    cyl(e, BLK, (-2.2, 5.0, 22.0), (2.2, 5.0, 22.0), 2.8, seg_for(lod) // 2 or 8)
    box(e, BLK, (0, 4.0, 20.0), (5.0, 1.0, 4.0))
    # discharge boom (does not slew with the wheel here): a lattice to the loading chute over the -X track
    tip = (BWE_DISCHARGE, 9.8, 0.0)
    truss(e, YEL, (-3.0, 9.0, 0.0), tip, 2.4, 2.0, lod, solid_mat=YEL)
    cyl(e, LIGHT, (0.0, 8.0, 0.0), (BWE_DISCHARGE, 9.0, 0.0), 0.5, small)
    box(e, BLK, (BWE_DISCHARGE, 8.6, 0.0), (2.4, 1.6, 2.4))
    cyl(e, BLK, (BWE_DISCHARGE, 7.8, 0.0), (BWE_DISCHARGE, 6.2, 0.0), 0.8, small)        # telescopic sealed spout
    if lod < 2:
        cyl(e, HAZ, (BWE_DISCHARGE, 6.4, 0.0), (BWE_DISCHARGE, 6.0, 0.0), 1.0, small)     # lid-seal collar
        lamp(e, BWE_DISCHARGE + 1.2, 9.0, 1.2, 1.5)
    merge(e, placed(bwe_super(lod), rot_y(math.radians(BWE_SLEW)), (0.0, 0.0, 0.0)))
    return P


def bwe_hits():
    R = rot_y(math.radians(BWE_SLEW))
    wc = apply(R, (BWE_REACH, BWE_WY, 0.0))
    bm = apply(R, ((BWE_REACH + 5.0) / 2, 15.0, 0.0))
    cw = apply(R, (-30.0, 15.0, 0.0))
    return [hbox("hubq_bucket_wheel", (0, 5.0, 0), (2 * BWE_TRACK + 6.0, 10.0, 46.0)),
            hbox("hubq_bucket_wheel", (0, 30.0, 0), (24.0, 34.0, 24.0)),
            hbox("hubq_bucket_wheel", wc, (2 * BWE_WR + 3.0, 2 * BWE_WR + 3.0, 2 * BWE_WR + 3.0), (0.0, BWE_SLEW, 0.0)),
            hbox("hubq_bucket_wheel", bm, (BWE_REACH - 6.0, 6.0, 5.0), (0.0, BWE_SLEW, 0.0)),
            hbox("hubq_bucket_wheel", cw, (16.0, 12.0, 11.0), (0.0, BWE_SLEW, 0.0)),
            hbox("hubq_bucket_wheel", (BWE_DISCHARGE / 2, 8.0, 0.0), (-BWE_DISCHARGE + 3.0, 5.0, 3.0))]


# ================================================================ open pit: benches, corners, floor, the cut
# Bench geometry from open-pit practice: benches 12-15 m high, 20-40 m wide (equipment-sized). Ours: 3 benches 15 m
# high, 40 m wide (the bucket-wheel machine + a track on each), batters 6 m wide (68 deg - low gravity, little slope
# failure), a 60 m plateau at the rim, pit floor 45 m down.
BH, BW, BB, PLAT, FLOOR_X, BASE_Y = 15.0, 40.0, 6.0, 60.0, -130.0, -60.0
PIT_R = 140.0            # corner radius (to the rim) of the pit kit; a corner's floor edge lies 10 m from its centre


def pit_profile():
    """(x, y) from the plateau's outer edge (+PLAT) to the floor edge; pit side = -X; returns points + materials"""
    pts, mats = [(PLAT, 0.0), (0.0, 0.0)], [REG]
    x, y = 0.0, 0.0
    for k in range(3):
        x, y = x - BB, y - BH
        pts.append((x, y))
        mats.append(CUT)
        x2 = x - BW if k < 2 else FLOOR_X
        pts.append((x2, y))
        mats.append(REG)
        x = x2
    return pts, mats


def pit_rock_extras(p, lod, along, length, key):
    """boulders on the bench floors, toe spill strips, a rim berm, rail-side lamp posts (on a straight piece: along(z))"""
    if lod >= 2:
        return
    pts, _ = pit_profile()
    box(p, REG, (2.0, 0.6, length / 2), (2.4, 1.2, length))                               # rim safety berm
    nb = 6 if lod == 0 else 3
    for k in range(3):
        xt = pts[2 + 2 * k][0]
        y = pts[2 + 2 * k][1]
        box(p, CUT, (xt - 1.0, y + 0.4, length / 2), (2.0, 0.8, length))                   # spill at the toe
        for i in range(nb):
            z = (i + 0.5) * length / nb + (_hash(key, k, i) - 0.5) * 6.0
            x = xt - 3.0 - _hash(key, k, i, 7) * (8.0 if k < 2 else 20.0)
            boulder(p, CUT, (x, y + 0.6, z), 1.5 + 2.5 * _hash(key, k, i, 9), (key, k, i))


def pit_side_parts(lod):
    P = {"hubq_pit_side": Part("hubq_pit_side")}
    s = P["hubq_pit_side"]
    pts, mats = pit_profile()
    extrude_z(s, pts, mats + [CUT], 0.0, 100.0, BASE_Y)
    pit_rock_extras(s, lod, None, 100.0, 1)
    return P


CUT_L, CUT_W = 200.0, 60.0     # the pit side with a canyon cut: 200 m long, a 60 m slot through its middle


def pit_cut_parts(lod):
    """a 200 m pit side with a 60 m wide slot cut down to the pit floor through the whole rim (z 70..130): the mouth
    of a canyon (hubq_canyon) into the pit"""
    P = {"hubq_pit_cut": Part("hubq_pit_cut")}
    s = P["hubq_pit_cut"]
    pts, mats = pit_profile()
    z0, z1 = (CUT_L - CUT_W) / 2, (CUT_L + CUT_W) / 2
    extrude_z(s, pts, mats + [CUT], 0.0, z0, BASE_Y)
    extrude_z(s, pts, mats + [CUT], z1, CUT_L, BASE_Y)
    box(s, REG, ((PLAT + FLOOR_X) / 2, (BASE_Y - 3 * BH) / 2, CUT_L / 2),
        (PLAT - FLOOR_X, -3 * BH - BASE_Y, CUT_W))                                         # the slot floor
    pit_rock_extras(s, lod, None, z0, 2)
    if lod < 2:
        for z in (z0 - 2.0, z1 + 2.0):                                                     # marker lights on the cut edges
            for x in (PLAT - 5.0, 20.0, -20.0):
                beacon(s, (x, 1.0, z), 1.0)
    return P


def pit_corner_parts(lod):
    """a 90-degree corner of the pit (rim radius 140 m about the origin, from +X round to +Z); pit side = towards the
    origin. Open ends meet hubq_pit_side's caps."""
    P = {"hubq_pit_corner": Part("hubq_pit_corner")}
    c = P["hubq_pit_corner"]
    pts, mats = pit_profile()
    seg = {0: 24, 1: 12, 2: 6}[lod]
    prof = [(PIT_R + PLAT, BASE_Y)] + [(PIT_R + x, y) for x, y in pts] + [(PIT_R + FLOOR_X, BASE_Y), (PIT_R + PLAT, BASE_Y)]
    pm = [CUT] + mats + [CUT, CUT]
    for k in range(len(prof) - 1):
        ylathe(c, pm[k], [prof[k], prof[k + 1]], seg, 0.0, math.pi / 2, smooth=False)
    # the plateau filler out to the square corner (x, z <= PIT_R + PLAT): the corner meets the square tile grid
    R2 = PIT_R + PLAT
    arc = [(R2 * math.cos(math.pi / 2 * i / seg), 0.0, R2 * math.sin(math.pi / 2 * i / seg)) for i in range(seg + 1)]
    for i in range(seg):
        tri_face(c, REG, (R2, 0.0, R2), arc[i], arc[i + 1])
    quad(c, CUT, [(R2, BASE_Y, 0.0), (R2, 0.0, 0.0), (R2, 0.0, R2), (R2, BASE_Y, R2)], (1, 0, 0))
    quad(c, CUT, [(0.0, BASE_Y, R2), (0.0, 0.0, R2), (R2, 0.0, R2), (R2, BASE_Y, R2)], (0, 0, 1))
    if lod < 2:
        nb = 5 if lod == 0 else 3
        for k in range(3):
            xt, y = pts[2 + 2 * k]
            for i in range(nb):
                th = (i + 0.5) / nb * math.pi / 2
                r = PIT_R + xt - 3.0 - _hash(3, k, i) * (8.0 if k < 2 else 16.0)
                boulder(c, CUT, (r * math.cos(th), y + 0.6, r * math.sin(th)), 1.5 + 2.0 * _hash(3, k, i, 9), (3, k, i))
    return P


def pit_floor_parts(lod):
    """the pit floor plate (230 x 230, top at y 0 - placed at the floor level), a drainage sump, boulders"""
    P = {"hubq_pit_floor": Part("hubq_pit_floor")}
    f = P["hubq_pit_floor"]
    box(f, REG, (0, -7.5, 0), (230.0, 15.0, 230.0))
    if lod < 2:
        for i in range(14 if lod == 0 else 6):
            x = (_hash(5, i) - 0.5) * 200.0
            z = (_hash(5, i, 1) - 0.5) * 200.0
            boulder(f, CUT, (x, 0.6, z), 1.5 + 3.0 * _hash(5, i, 2), (5, i))
        box(f, DARK, (60.0, 0.05, -60.0), (14.0, 0.1, 14.0))                               # sump grating
        box(f, HAZ, (60.0, 0.1, -60.0), (15.0, 0.08, 1.0))
    return P


# ---------------------------------------------------------------- canyon (a trench cut between the pit and the plain)
CAN_L, CAN_HALF = 100.0, 30.0


def canyon_profile():
    """+X wall: from the floor edge (x = 30, y -45) up three steep steps to the top edge (x 100, y 0)"""
    return [(100.0, 0.0), (68.0, 0.0), (62.0, -15.0), (52.0, -15.0), (46.0, -30.0), (36.0, -30.0), (30.0, -45.0)]


def canyon_parts(lod):
    """a straight canyon / haul trench 100 m along +Z: floor 60 m wide 45 m down, stepped walls (top width 136 m),
    floor 0..y -45; fly-through for small ships; marker lights on the wall tops, a rail on the floor"""
    P = {"hubq_canyon": Part("hubq_canyon")}
    c = P["hubq_canyon"]
    prof = canyon_profile()
    mats = [REG, CUT, REG, CUT, REG, CUT]
    for sx in (-1, 1):
        pts = [(sx * x, y) for x, y in prof]
        if sx < 0:
            pts = [(x, y) for x, y in pts]
        _canyon_wall(c, pts, mats, sx)
    box(c, REG, (0, -45.0 - 7.5, CAN_L / 2), (2 * CAN_HALF, 15.0, CAN_L))                  # floor
    if lod < 2:
        for i in range(4 if lod == 0 else 2):
            boulder(c, CUT, ((_hash(7, i) - 0.5) * 40.0, -44.4, (i + 0.5) * CAN_L / 4), 1.5 + 2.0 * _hash(7, i, 1), (7, i))
        for sx in (-1, 1):
            beacon(c, (sx * 69.0, 1.0, 10.0), 1.2)
            beacon(c, (sx * 31.0, -44.0, 60.0), 0.9)
            box(c, FLOOD, (sx * 47.0, -29.5, 35.0), (1.0, 1.0, 1.0))
    return P


def _canyon_wall(p, pts, mats, sx):
    """one canyon wall: pts from the top edge inwards, mirrored for -X"""
    for k in range(len(pts) - 1):
        (x0, y0), (x1, y1) = pts[k], pts[k + 1]
        tx, ty = x1 - x0, y1 - y0
        n = norm((ty, -tx, 0.0))
        if n[1] < 0 or (abs(n[1]) < 1e-6 and n[0] * sx > 0):
            n = (-n[0], -n[1], 0.0)
        quad(p, mats[k], [(x0, y0, 0.0), (x1, y1, 0.0), (x1, y1, CAN_L), (x0, y0, CAN_L)], n)
    # outer side and bottom (closed volume; caps are left open - the pieces chain end to end)
    xa = pts[0][0]
    quad(p, CUT, [(xa, BASE_Y, 0.0), (xa, 0.0, 0.0), (xa, 0.0, CAN_L), (xa, BASE_Y, CAN_L)], (sx, 0, 0))
    xb = pts[-1][0]
    quad(p, CUT, [(xa, BASE_Y, 0.0), (xb, BASE_Y, 0.0), (xb, BASE_Y, CAN_L), (xa, BASE_Y, CAN_L)], (0, -1, 0))
    for z, s in ((0.0, -1.0), (CAN_L, 1.0)):
        for k in range(len(pts) - 1):
            (x0, y0), (x1, y1) = pts[k], pts[k + 1]
            quad(p, CUT, [(x0, BASE_Y, z), (x1, BASE_Y, z), (x1, y1, z), (x0, y0, z)], (0, 0, s))


def canyon_hits():
    out = [hbox("hubq_canyon", (0, -52.5, CAN_L / 2), (2 * CAN_HALF, 15.0, CAN_L))]
    for sx in (-1, 1):
        out.append(hbox("hubq_canyon", (sx * 84.0, -30.0, CAN_L / 2), (32.0, 60.0, CAN_L)))
        out.append(hbox("hubq_canyon", (sx * 57.0, -37.5, CAN_L / 2), (22.0, 45.0, CAN_L)))
        out.append(hbox("hubq_canyon", (sx * 41.0, -45.0, CAN_L / 2), (10.0, 30.0, CAN_L)))
    return out


# ---------------------------------------------------------------- ground (a 100 m tile of the asteroid surface)
def ground_parts(lod):
    """a 100 x 100 m tile of regolith, top y 0 at the edges (tiles chain), gentle undulation inside, a few boulders"""
    P = {"hubq_ground": Part("hubq_ground")}
    g = P["hubq_ground"]
    n = {0: 10, 1: 5, 2: 1}[lod]
    S = 100.0
    def h(i, j):
        if i in (0, n) or j in (0, n):
            return 0.0
        u, v = i / n, j / n
        # bumps only (never below y 0): a dip next to a shared edge would bare the neighbour tile's side wall
        return 1.2 * math.sin(math.pi * u) * math.sin(math.pi * v) * (0.5 + 0.3 * math.sin(5.1 * u + 2.0 * v) + 0.2 * math.cos(3.7 * v - 1.3 * u))
    pts = [[(-S / 2 + S * i / n, h(i, j), -S / 2 + S * j / n) for j in range(n + 1)] for i in range(n + 1)]
    for i in range(n):
        for j in range(n):
            a, b, c, d = pts[i][j], pts[i + 1][j], pts[i + 1][j + 1], pts[i][j + 1]
            tri_face(g, REG, a, b, c)
            tri_face(g, REG, a, c, d)
    for sx in (-1, 1):
        quad(g, REG, [(sx * S / 2, -12.0, -S / 2), (sx * S / 2, 0.0, -S / 2), (sx * S / 2, 0.0, S / 2), (sx * S / 2, -12.0, S / 2)], (sx, 0, 0))
        quad(g, REG, [(-S / 2, -12.0, sx * S / 2), (-S / 2, 0.0, sx * S / 2), (S / 2, 0.0, sx * S / 2), (S / 2, -12.0, sx * S / 2)], (0, 0, sx))
    quad(g, REG, [(-S / 2, -12.0, -S / 2), (S / 2, -12.0, -S / 2), (S / 2, -12.0, S / 2), (-S / 2, -12.0, S / 2)], (0, -1, 0))
    if lod < 2:
        for i in range(3):
            boulder(g, CUT, ((_hash(9, i) - 0.5) * 80.0, 0.5, (_hash(9, i, 1) - 0.5) * 80.0), 1.2 + 1.8 * _hash(9, i, 2), (9, i))
    return P


# ================================================================ burrow: tunnel portal, headframe over a shaft
PORTAL_W, PORTAL_R = 12.0, 6.0          # the opening: 12 m wide, walls 6 m + an arch of radius 6 (12 m high)


def portal_parts(lod):
    """a tunnel portal ("нора"): a rock outcrop 76 x 40 x 34 m with an arched mouth facing -Z (the tunnel runs into +Z,
    12 x 12 m, 34 m deep to the inner airlock door); concrete collar, outer airlock doors half open, a pipe conveyor
    coming out, flood masts. Rails are laid by the layout (hubq_rail) into the mouth."""
    P = {"hubq_portal": Part("hubq_portal")}
    p = P["hubq_portal"]
    seg = seg_for(lod)
    D = 34.0
    hw = PORTAL_W / 2
    ay = PORTAL_R                                   # arch springing height
    # the rock: a hill profile (depth into the rock, height) extruded across the width, the middle part only above the
    # mouth; built in a frame whose x is the depth (world +Z) and z the width (world -X), turned into place
    prof = [(D + 14.0, 0.0), (D + 4.0, 16.0), (D - 8.0, 26.0), (16.0, 30.0), (6.0, 26.0), (0.0, 20.0), (0.0, 0.0)]
    rock = Part("rock")
    mats = [ROCK, ROCK, ROCK, ROCK, CUT, CUT, ROCK]
    extrude_z(rock, prof, mats, -30.0, -(hw + 1.2), 0.0)
    extrude_z(rock, prof, mats, hw + 1.2, 30.0, 0.0)
    low = [(x * 0.85 + 4.0, y * 0.55) for x, y in prof]                     # stepped-down shoulders on both sides
    extrude_z(rock, low, mats, -42.0, -30.0, 0.0)
    extrude_z(rock, low, mats, 30.0, 42.0, 0.0)
    mid = [(D + 14.0 - 10.0 * 13.0 / 16.0, 13.0)] + [q for q in prof[1:-2]] + [(0.0, 24.0), (0.0, 13.0)]
    extrude_z(rock, mid, mats[1:], -(hw + 1.2), hw + 1.2, 13.0)
    merge(p, placed(rock, rot_y(-math.pi / 2), (0.0, 0.0, 0.0)))
    crags = [(-22.0, 28.0, 18.0, 11.0), (8.0, 30.0, 22.0, 9.0), (26.0, 26.0, 12.0, 8.0), (-10.0, 29.0, 34.0, 8.0),
             (-36.0, 12.0, 8.0, 9.0), (37.0, 13.0, 10.0, 8.0), (18.0, 5.0, 0.5, 6.0), (-20.0, 4.0, 0.0, 5.0),
             (-28.0, 16.0, 3.0, 7.0), (24.0, 18.0, 4.0, 6.0), (0.0, 22.0, 6.0, 8.0)]
    for i, (x, y, z, s) in enumerate(crags if lod < 2 else crags[:3]):
        boulder(p, ROCK, (x, y, z), s, (11, i))
    # the tunnel lining: walls + a half-barrel vault (inner faces)
    for sx in (-1, 1):
        quad(p, CON, [(sx * hw, 0.0, -2.0), (sx * hw, ay, -2.0), (sx * hw, ay, D), (sx * hw, 0.0, D)], (-sx, 0, 0))
        box(p, ROCK, (sx * (hw + 0.6), ay / 2, D / 2), (1.2, ay, D + 4.0))
    lathe(p, CON, [(PORTAL_R, D), (PORTAL_R, -2.0)], seg, I3, (0.0, ay, 0.0), 0.0, math.pi, smooth=True)
    lathe(p, ROCK, [(PORTAL_R + 1.2, -2.0), (PORTAL_R + 1.2, D)], seg // 2 or 8, I3, (0.0, ay, 0.0), 0.0, math.pi)
    box(p, CON, (0.0, -0.25, D / 2 - 6.0), (2 * hw, 0.5, D + 12.0))                       # tunnel floor + apron
    quad(p, HOLE, [(-hw, 0.0, D), (hw, 0.0, D), (hw, 2 * ay, D), (-hw, 2 * ay, D)], (0, 0, -1))   # inner door (dark)
    # the concrete collar (headwall ring) round the mouth
    lathe(p, CON, crect(PORTAL_R, PORTAL_R + 4.0, -3.0, 0.0), seg, I3, (0.0, ay, 0.0), 0.0, math.pi, smooth=False, cap=True)
    for sx in (-1, 1):
        box(p, CON, (sx * (hw + 2.0), ay / 2, -1.5), (4.0, ay, 3.0))
    if lod < 2:
        lathe(p, HAZ, crect(PORTAL_R + 0.1, PORTAL_R + 0.7, -3.1, -2.9), seg, I3, (0.0, ay, 0.0), 0.0, math.pi, smooth=False, cap=True)
        # outer airlock doors, slid half open (the burrow is pressurised further in)
        for sx in (-1, 1):
            box(p, YEL, (sx * (hw - 1.6), ay, 6.0), (3.2, 2 * ay - 0.4, 0.6))
            box(p, HAZ, (sx * (hw - 1.6), 1.0, 5.6), (3.2, 0.6, 0.1))
        box(p, BLK, (0.0, 2 * ay - 0.3, 6.0), (2 * hw, 0.6, 1.0))                          # door track beam
        for z in (4.0, 14.0, 24.0):                                                        # tunnel lights
            box(p, FLOOD, (0.0, 2 * ay - 0.2, z), (3.0, 0.3, 0.6))
        # flood masts at the mouth
        for sx in (-1, 1):
            lattice_tower(p, STEEL, (sx * (hw + 9.0), -6.0), 0.0, 18.0, 0.9, 0.5, lod)
            box(p, FLOOD, (sx * (hw + 9.0), 18.6, -6.0), (2.4, 1.2, 1.2))
    beacon(p, (0.0, 2 * ay + 4.6, -3.2), 1.0)
    # a pipe conveyor out of the mouth (ore from the burrow), rising over the apron
    pipe_conveyor(p, lod, (hw - 3.0, 9.0, D - 2.0), (hw - 3.0, 9.0, -16.0), legs=False, gallery=False)
    return P


def portal_hits():
    hw = PORTAL_W / 2
    return [hbox("hubq_portal", (-(hw + 16.0), 20.0, 17.0), (32.0, 40.0, 34.0)),
            hbox("hubq_portal", (hw + 16.0, 20.0, 17.0), (32.0, 40.0, 34.0)),
            hbox("hubq_portal", (0.0, 26.5, 17.0), (2 * hw + 4.0, 27.0, 34.0))]


HF_H = 48.0


def headframe_parts(lod):
    """a shaft headframe over a burrow shaft (steel lattice, 48 m to the sheave deck - Earth headframes 30-87 m):
    4-leg tower, raking back legs to the winding house 40 m away, two 7 m sheaves, the collar house (airlock landing)
    on a sintered collar pad, ropes in a closed loop (friction hoist with tail rope: the cage is pulled both ways)"""
    P = {"hubq_headframe": Part("hubq_headframe")}
    h = P["hubq_headframe"]
    seg = seg_for(lod)
    box(h, CON, (0, 1.0, 6.0), (30.0, 2.0, 42.0))                                         # collar pad
    ylathe(h, CON, [(4.6, 2.0), (6.0, 2.0), (6.0, 2.6), (4.6, 2.6)], seg)
    ylathe(h, HOLE, [(4.6, 2.05), (0.0, 2.05)], seg)                                       # the shaft mouth
    lattice_tower(h, YEL, (0.0, 0.0), 2.0, HF_H, 7.0, 3.2, lod, bays=8)
    box(h, YEL, (0, HF_H + 0.6, 0), (9.0, 1.2, 9.0))                                      # sheave deck
    box(h, BLK, (0, HF_H + 7.6, 0), (10.0, 1.0, 10.0))                                    # crane beam / roof
    for sx in (-1, 1):
        bar(h, YEL, (sx * 4.2, HF_H + 1.2, -4.2), (sx * 4.2, HF_H + 7.2, -4.2), 0.6)
        bar(h, YEL, (sx * 4.2, HF_H + 1.2, 4.2), (sx * 4.2, HF_H + 7.2, 4.2), 0.6)
    # back legs to the winding-house side (+Z)
    for sx in (-1, 1):
        truss(h, YEL, (sx * 3.2, HF_H, 3.2), (sx * 6.0, 2.0, 26.0), 1.6, 1.6, lod, up=(1.0, 0.0, 0.0), solid_mat=YEL)
    # sheaves (axis X), side by side
    for sx in (-1.6, 1.6):
        c = (sx, HF_H + 4.6, 0.0)
        lathe(h, STEEL, crect(2.8, 3.5, -0.35, 0.35), seg, rot_y(math.pi / 2), c, smooth=False)
        cyl(h, BLK, (sx - 0.5, c[1], 0), (sx + 0.5, c[1], 0), 0.6, 8)
        if lod < 2:
            for k in range(6):
                a = TAU * k / 6
                bar(h, STEEL, c, _add(c, (0.0, 2.9 * math.sin(a), 2.9 * math.cos(a))), 0.25)
        # ropes: down the shaft and back to the winding house drums
        cyl(h, BLK, (sx, c[1], -3.4), (sx, 2.0, -3.4), 0.12, 6)
        cyl(h, BLK, (sx, c[1] + 3.4, 0.0), (sx * 0.8, 12.0, 29.0), 0.12, 6)
    # collar house (the airlock landing round the shaft) and the winding house
    box(h, LIGHT, (0, 7.0, 0), (12.0, 10.0, 12.0))
    box(h, HOLE, (0, 5.0, -6.05), (5.0, 6.0, 0.1))
    box(h, YEL, (0, 8.5, 38.0), (24.0, 13.0, 18.0), chamfer=0.4)
    box(h, BLK, (0, 15.4, 38.0), (24.6, 0.8, 18.6))
    if lod < 2:
        for x in (-8.0, -4.0, 0.0, 4.0, 8.0):
            box(h, WARM, (x, 10.0, 28.95), (2.4, 2.0, 0.1))
        for sx in (-1, 1):
            box(h, RAD, (sx * 13.0, 9.0, 38.0), (0.4, 10.0, 14.0))                         # radiators
        box(h, HAZ, (0, 12.5, -6.05), (12.0, 0.8, 0.1))
        box(h, HOLE, (0, 12.0, 28.95), (4.0, 2.0, 0.1))                                    # rope opening
    beacon(h, (0, HF_H + 8.6, 0), 1.2)
    if lod < 2:
        for sx in (-1, 1):
            beacon(h, (sx * 12.0, 15.6, 46.5), 0.8)
            box(h, FLOOD, (sx * 4.0, HF_H + 0.0, -4.6), (1.0, 0.8, 0.8))
    return P


def headframe_hits():
    return [hbox("hubq_headframe", (0, HF_H / 2 + 4.0, 0), (16.0, HF_H + 8.0, 16.0)),
            hbox("hubq_headframe", (0, 8.5, 38.0), (24.0, 13.0, 18.0)),
            hbox("hubq_headframe", (0, 1.0, 6.0), (30.0, 2.0, 42.0)),
            hbox("hubq_headframe", (0, 26.0, 14.0), (14.0, 40.0, 24.0))]


# ================================================================ settlement
def hab_block_parts(lod):
    """a regolith-shielded habitat row: 4 pressurised cylinders (9 m x 36 m) on cradles side by side, buried under a
    trapezoid regolith berm (2-3 m of cover = radiation shielding), lit end faces with airlock porches on -Z,
    radiator fins on the berm (a buried habitat must dump its heat), a comm mast. 52 x 15 x 42 m"""
    P = {"hubq_hab_block": Part("hubq_hab_block")}
    b = P["hubq_hab_block"]
    seg = seg_for(lod)
    xs = (-15.0, -5.0, 5.0, 15.0)
    for x in xs:
        cyl(b, LIGHT, (x, 4.8, -18.0), (x, 4.8, 18.0), 4.5, seg)
        if lod < 2:
            for k in range(3):                                                            # window rows on the ends
                for dx in (-2.0, 0.0, 2.0):
                    box(b, WARM if _hash(x, k, dx) > 0.3 else DARK, (x + dx, 3.0 + 2.0 * k, -18.05), (1.2, 0.8, 0.1))
            box(b, LIGHT, (x, 2.0, -20.0), (3.0, 3.6, 4.0))                                # airlock porch
            box(b, DARK, (x, 1.9, -22.05), (1.6, 2.6, 0.1))
            box(b, FLOOD, (x, 4.0, -22.1), (0.8, 0.3, 0.2))
    prof = [(26.0, 0.0), (18.0, 14.5), (-18.0, 14.5), (-26.0, 0.0)]
    extrude_z(b, prof, [REG, REG, REG, REG], -16.0, 16.0, 0.0)
    if lod < 2:
        for sx in (-1, 1):                                                                # sandbag courses on the slopes
            for k in range(3):
                box(b, CUT, (sx * (24.0 - 2.6 * k), 1.5 + 4.6 * k, 0.0), (1.2, 0.8, 32.2), rot_z(sx * 0.6))
        for z in (-8.0, 0.0, 8.0):
            box(b, RAD, (0.0, 19.5, z), (24.0, 10.0, 0.3))
            box(b, STEEL, (0.0, 14.8, z), (24.0, 0.6, 0.8))
    lattice_tower(b, STEEL, (16.0, 10.0), 14.5, 30.0, 0.7, 0.3, lod, bays=4)
    beacon(b, (16.0, 30.8, 10.0), 0.6)
    return P


def hab_block_hits():
    return [hbox("hubq_hab_block", (0, 7.25, 0), (52.0, 14.5, 32.0)), hbox("hubq_hab_block", (0, 4.8, 0), (40.0, 9.6, 40.0)),
            hbox("hubq_hab_block", (0, 19.5, 0), (24.0, 10.0, 17.0))]


HT_W, HT_D, HT_FLOORS, HT_FH = 26.0, 20.0, 8, 4.2


def hab_tower_parts(lod):
    """the company dormitory: 8 floors (4.2 m) 26 x 20 m, water-wall shield cassettes between window columns (the
    water doubles as radiation shielding and thermal mass), stair / lift towers at both ends, a storm-shelter water
    tank on the roof, entrance airlock with a canopy"""
    P = {"hubq_hab_tower": Part("hubq_hab_tower")}
    t = P["hubq_hab_tower"]
    H = HT_FLOORS * HT_FH
    box(t, CON, (0, 0.75, 0), (HT_W + 8.0, 1.5, HT_D + 8.0))
    box(t, LIGHT, (0, 1.5 + H / 2, 0), (HT_W, H, HT_D))
    cols = 8
    for sz in (-1, 1):
        zf = sz * (HT_D / 2 + 0.05)
        for f in range(HT_FLOORS):
            y = 1.5 + f * HT_FH + 2.2
            for cidx in range(cols):
                x = -HT_W / 2 + (cidx + 0.5) * HT_W / cols
                lit = _hash(sz, f, cidx) > 0.35
                if lod < 2 or cidx % 2 == 0:
                    box(t, WARM if lit else DARK, (x, y, zf), (1.6, 1.6, 0.1))
        if lod < 2:
            for cidx in range(cols + 1):                                                  # shield cassettes
                x = -HT_W / 2 + cidx * HT_W / cols
                box(t, HULL, (x, 1.5 + H / 2, sz * (HT_D / 2 + 0.6)), (1.0, H, 1.2))
            for f in range(HT_FLOORS + 1):
                box(t, HULL, (0, 1.5 + f * HT_FH, sz * (HT_D / 2 + 0.4)), (HT_W + 1.0, 0.5, 0.8))
    for sx in (-1, 1):
        box(t, HULL, (sx * (HT_W / 2 + 3.0), 1.5 + (H + 6.0) / 2, 0), (6.0, H + 6.0, 8.0))
        if lod < 2:
            box(t, COOL, (sx * (HT_W / 2 + 6.05), 1.5 + H / 2, 0), (0.1, H - 2.0, 1.2))
    cyl(t, TANK_W_MAT, (0, 1.5 + H, 0), (0, 1.5 + H + 7.0, 0), 7.0, seg_for(lod))
    if lod < 2:
        box(t, RAD, (0, 1.5 + H + 4.0, 8.5), (20.0, 7.0, 0.3))
        box(t, LIGHT, (0, 3.5, -(HT_D / 2 + 3.0)), (8.0, 4.0, 6.0))                         # entrance airlock
        box(t, DARK, (0, 3.0, -(HT_D / 2 + 6.05)), (3.0, 3.0, 0.1))
        box(t, HULL, (0, 6.2, -(HT_D / 2 + 4.5)), (12.0, 0.4, 9.0))                         # canopy
    beacon(t, (0, 1.5 + H + 8.0, 0), 0.8)
    for sx in (-1, 1):
        beacon(t, (sx * (HT_W / 2 + 3.0), 1.5 + H + 6.6, 0), 0.6)
    return P


TANK_W_MAT = "eq_hub_tank_white"


def hab_tower_hits():
    H = HT_FLOORS * HT_FH
    return [hbox("hubq_hab_tower", (0, 1.5 + H / 2, 0), (HT_W + 2.0, H, HT_D + 2.4)),
            hbox("hubq_hab_tower", (0, 1.5 + (H + 6.0) / 2, 0), (HT_W + 12.0, H + 6.0, 8.0)),
            hcyl("hubq_hab_tower", (0, 1.5 + H, 0), (0, 1.5 + H + 7.0, 0), 7.0)]


ST_L, ST_R = 60.0, 14.0


def store_parts(lod):
    """a mine store: a vaulted hall (half-barrel, r 14, 60 m long - the cheapest pressure-free span to build from
    curved panels) with end doors, ribs, and an open lashing yard of containers under tether nets on its +X side"""
    P = {"hubq_store": Part("hubq_store")}
    s = P["hubq_store"]
    seg = seg_for(lod)
    box(s, CON, (6.0, 0.5, 0), (2 * ST_R + 34.0, 1.0, ST_L + 10.0))
    lathe(s, LIGHT, [(ST_R, -ST_L / 2), (ST_R, ST_L / 2)], seg, I3, (0.0, 1.0, 0.0), 0.0, math.pi, smooth=True)
    for sz in (-1, 1):
        lathe(s, LIGHT, [(0.0, sz * ST_L / 2), (ST_R, sz * ST_L / 2)][::-sz], seg, I3, (0.0, 1.0, 0.0), 0.0, math.pi)
        box(s, DARK, (0, 5.5, sz * (ST_L / 2 + 0.05)), (12.0, 9.0, 0.1))
        if lod < 2:
            box(s, HAZ, (0, 10.4, sz * (ST_L / 2 + 0.1)), (13.0, 0.8, 0.1))
            box(s, FLOOD, (0, 11.5, sz * (ST_L / 2 + 0.3)), (1.4, 0.6, 0.6))
    if lod < 2:
        for k in range(7):                                                                # ribs
            z = -ST_L / 2 + 5.0 + k * (ST_L - 10.0) / 6
            lathe(s, STEEL, crect(ST_R, ST_R + 0.5, z - 0.4, z + 0.4), seg, I3, (0.0, 1.0, 0.0), 0.0, math.pi, smooth=False, cap=True)
        # the yard: container stacks under orange nets with anchor posts
        for i in range(3):
            x = ST_R + 6.0 + i * 5.0
            for j in range(4):
                z = -ST_L / 2 + 8.0 + j * 14.0
                hgt = 1 + (i + 2 * j) % 3
                for k in range(hgt):
                    box(s, CONT[(i + j + k) % 3], (x, 2.3 + k * 2.6, z), (2.5, 2.6, 12.0))
                box(s, NET, (x, 1.0 + hgt * 2.6 + 0.15, z), (2.7, 0.08, 12.2))
                for sz in (-1, 1):
                    bar(s, NET, (x, 1.0 + hgt * 2.6 + 0.15, z + sz * 6.0), (x + 1.8, 1.0, z + sz * 7.0), 0.08)
    beacon(s, (0, ST_R + 1.6, 0), 0.8)
    return P


def store_hits():
    return [hbox("hubq_store", (0, (ST_R + 1.0) / 2, 0), (2 * ST_R, ST_R + 1.0, ST_L)),
            hbox("hubq_store", (ST_R + 11.0, 4.5, 0), (16.0, 9.0, ST_L))]


# ---------------------------------------------------------------- ore stockpiles (pyramids under nets)
def ore_pile_parts(lod, name, B, H, mat, feed=True):
    """a square-based ore pile, apex height H over base B (side slopes ~34 deg - the angle of repose of crushed ore,
    which barely depends on gravity), a flattened top, lumpy faces; a tether net with anchor posts round it (on a big
    asteroid the pile holds its shape, but impacts / static would loft fines); a feed boom from a lattice post (the
    stacker drops the ore slowly from a sealed spout at the apex)"""
    P = {name: Part(name)}
    p = P[name]
    n = {0: 12, 1: 6, 2: 2}[lod]
    half = B / 2

    def hgt(x, z, i, j):
        base = H * max(0.0, 1.0 - max(abs(x), abs(z)) / half)
        base = min(base, H * 0.92)
        if 0 < i < n and 0 < j < n and lod < 2:
            base += (_hash(B, i, j) - 0.5) * H * 0.06
        return max(0.0, base)

    pts = [[(-half + B * i / n, 0.0, -half + B * j / n) for j in range(n + 1)] for i in range(n + 1)]
    pts = [[(x, hgt(x, z, i, j), z) for (x, _, z), j in zip(row, range(n + 1))] for row, i in zip(pts, range(n + 1))]
    for i in range(n):
        for j in range(n):
            a, b, c, d = pts[i][j], pts[i + 1][j], pts[i + 1][j + 1], pts[i][j + 1]
            mx, mz = (a[0] + c[0]) / 2, (a[2] + c[2]) / 2
            if mx * mz > 0:
                tri_face(p, mat, a, b, c)
                tri_face(p, mat, a, c, d)
            else:
                tri_face(p, mat, a, b, d)
                tri_face(p, mat, b, c, d)
    if lod < 2:
        # the net: lines over the pile both ways, 0.2 m above the surface
        m = n
        step = 2 if lod == 1 else 1
        for i in range(1, m, 2 * step if lod == 1 else 2):
            line = [(pts[i][j][0], pts[i][j][1] + 0.25, pts[i][j][2]) for j in range(n + 1)]
            tube(p, NET, line, 0.12, 4)
            line = [(pts[j][i][0], pts[j][i][1] + 0.25, pts[j][i][2]) for j in range(n + 1)]
            tube(p, NET, line, 0.12, 4)
        for sx in (-1, 0, 1):                                                              # anchor posts
            for sz in (-1, 0, 1):
                if sx == 0 and sz == 0:
                    continue
                x, z = sx * (half + 3.0), sz * (half + 3.0)
                box(p, YEL, (x, 2.0, z), (0.8, 4.0, 0.8))
                box(p, CON, (x, 0.3, z), (2.0, 0.6, 2.0))
                bar(p, NET, (x, 3.6, z), (sx * half * 0.92, 0.6, sz * half * 0.92), 0.1)
    if feed:
        post = (half + 10.0, 0.0)
        lattice_tower(p, YEL, post, 0.0, H + 12.0, 2.2, 1.2, lod)
        truss(p, YEL, (half + 10.0, H + 11.0, 0.0), (0.0, H + 9.0, 0.0), 2.0, 2.0, lod, solid_mat=YEL)
        cyl(p, BLK, (0.0, H + 8.0, 0.0), (0.0, H + 2.5, 0.0), 0.7, small_for(lod))         # spout
        beacon(p, (half + 10.0, H + 13.0, 0.0), 0.8)
    return P


def ore_pile_hits(name, B, H, feed=True):
    out = [hbox(name, (0, H * 0.25, 0), (B, H * 0.5, B)), hbox(name, (0, H * 0.7, 0), (B * 0.5, H * 0.45, B * 0.5))]
    if feed:
        out.append(hbox(name, (B / 2 + 10.0, (H + 12.0) / 2, 0), (5.0, H + 12.0, 5.0)))
        out.append(hbox(name, (B / 4 + 5.0, H + 10.0, 0), (B / 2 + 12.0, 3.0, 2.5)))
    return out


# ---------------------------------------------------------------- bulk transport loading terminal
# Reference: iron-ore export terminals (Port Hedland / Narvik): surge bins, a conveyor gallery, a travelling shiploader
# on a 580-620 m berth, 10-13 kt/h. In free fall nothing can be poured into a hold: the ore leaves the silos through a
# sealed pipe conveyor up a lattice tower to a berth girder; a travelling loader couples a sealed arm to the ship's
# bulk port (like an oil loading arm), mooring clamp arms hold the ship alongside. Berth for a T-5000 / HT-20K class
# carrier (hull to 260 m) lying along the +X side of the girder.
TB_Y, TB_L = 90.0, 300.0


def terminal_parts(lod):
    P = {"hubq_bulk_terminal": Part("hubq_bulk_terminal")}
    t = P["hubq_bulk_terminal"]
    seg = seg_for(lod)
    small = small_for(lod)
    # ground slab, four surge silos (r 9 m, 40 m) with hopper feeders
    box(t, CON, (-40.0, 1.0, -60.0), (40.0, 2.0, 110.0))
    for z in (-105.0, -75.0, -45.0, -15.0):
        cyl(t, TANK_W_MAT, (-40.0, 8.0, z), (-40.0, 48.0, z), 9.0, seg)
        ylathe(t, TANK_W_MAT, [(9.0, 48.0), (0.0, 52.0)], seg, t=(-40.0, 0.0, z))
        ylathe(t, STEEL, [(2.0, 3.0), (9.0, 8.0)], seg, t=(-40.0, 0.0, z))
        for k in range(4 if lod < 2 else 0):
            a = TAU * k / 4 + math.pi / 4
            bar(t, STEEL, (-40.0 + 8.0 * math.cos(a), 2.0, z + 8.0 * math.sin(a)), (-40.0 + 8.6 * math.cos(a), 12.0, z + 8.6 * math.sin(a)), 0.8)
        if lod < 2:
            ylathe(t, GRATE, [(9.0, 48.2), (10.6, 48.2), (10.6, 48.8), (9.0, 48.8)], seg, t=(-40.0, 0.0, z))
            box(t, HAZ, (-40.0, 30.0, z + 9.02), (6.0, 1.2, 0.1))
            beacon(t, (-40.0, 52.8, z), 0.8)
    # collector conveyor over the silo tops, then the inclined gallery up to the tower head
    truss(t, YEL, (-40.0, 54.0, -118.0), (-40.0, 54.0, 0.0), 4.0, 3.0, lod, solid_mat=YEL)
    pipe_conveyor(t, lod, (-40.0, 58.0, -118.0), (-40.0, 58.0, 0.0), legs=False)
    a, b = (-40.0, 58.0, 0.0), (-8.0, TB_Y + 4.0, 0.0)
    truss(t, YEL, _add(a, (0, -3.0, 0)), _add(b, (0, -3.0, 0)), 4.0, 4.0, lod, up=(0.0, 1.0, 0.0), solid_mat=YEL)
    pipe_conveyor(t, lod, a, b, legs=False)
    # two lattice towers carrying the berth girder
    for z in (-110.0, 110.0):
        lattice_tower(t, YEL, (0.0, z), 0.0, TB_Y - 4.0, 9.0, 4.0, lod, bays=10)
        box(t, CON, (0.0, 1.0, z), (22.0, 2.0, 22.0))
        if lod < 2:
            for sx in (-1, 1):                                                              # ground anchors (tethers)
                for sz in (-1, 1):
                    cyl(t, STEEL, (sx * 4.0, TB_Y - 6.0, z + sz * 4.0), (sx * 24.0, 0.0, z + sz * 24.0), 0.3, 6)
                    box(t, CON, (sx * 24.0, 1.0, z + sz * 24.0), (4.0, 2.0, 4.0))
    # the berth girder 300 m along Z
    truss(t, YEL, (0.0, TB_Y, -TB_L / 2), (0.0, TB_Y, TB_L / 2), 10.0, 8.0, lod, solid_mat=YEL)
    box(t, GRATE, (0.0, TB_Y + 4.2, 0.0), (10.0, 0.4, TB_L))
    pipe_conveyor(t, lod, (-2.5, TB_Y + 6.0, -TB_L / 2 + 10.0), (-2.5, TB_Y + 6.0, TB_L / 2 - 10.0), legs=False, gallery=False)
    # mooring clamp arms + fenders along the +X side every 50 m
    for z in [-125.0 + 50.0 * k for k in range(6)]:
        truss(t, BLK, (5.0, TB_Y, z), (24.0, TB_Y - 4.0, z), 2.0, 2.0, lod, solid_mat=BLK)
        box(t, HAZ, (25.0, TB_Y - 4.0, z), (2.0, 5.0, 6.0))
        if lod < 2:
            box(t, BLK, (25.6, TB_Y - 4.0, z), (0.6, 4.0, 5.0))                            # clamp pad
            beacon(t, (24.0, TB_Y - 0.5, z), 0.9)
    # the travelling loader: a trolley on the girder, an articulated sealed loading arm out to the ship's port (+X)
    zl = 30.0
    box(t, YEL, (0.0, TB_Y + 9.0, zl), (14.0, 8.0, 16.0), chamfer=0.4)
    box(t, DARK, (5.0, TB_Y + 10.5, zl - 8.05), (3.0, 2.0, 0.1))
    knee = (14.0, TB_Y + 16.0, zl)
    head = (30.0, TB_Y + 2.0, zl)
    truss(t, YEL, (6.0, TB_Y + 10.0, zl), knee, 2.4, 2.4, lod, up=(0.0, 0.0, 1.0), solid_mat=YEL)
    truss(t, YEL, knee, head, 2.0, 2.0, lod, up=(0.0, 0.0, 1.0), solid_mat=YEL)
    cyl(t, LIGHT, (6.0, TB_Y + 11.5, zl + 1.6), _add(knee, (0, 0, 1.6)), 0.8, small)
    cyl(t, LIGHT, _add(knee, (0, 0, 1.6)), _add(head, (0, 1.0, 1.6)), 0.8, small)
    cyl(t, HAZ, (head[0], head[1], head[2] + 0.0), (head[0] + 3.0, head[1] - 1.0, head[2]), 2.0, small)   # coupling head
    # control building at the foot, lights, beacons at the girder ends
    box(t, LIGHT, (-12.0, 6.0, 40.0), (16.0, 10.0, 24.0))
    if lod < 2:
        for z in (32.0, 36.0, 40.0, 44.0, 48.0):
            box(t, WARM, (-3.95, 7.0, z), (0.1, 2.0, 2.4))
        for z in range(-140, 150, 30):
            box(t, FLOOD, (5.5, TB_Y - 4.6, float(z)), (0.8, 0.6, 0.8))
    for z in (-TB_L / 2, TB_L / 2):
        beacon(t, (0.0, TB_Y + 5.6, z), 1.6)
    return P


def terminal_hits():
    out = [hbox("hubq_bulk_terminal", (0.0, TB_Y, 0.0), (10.0, 8.0, TB_L)),
           hbox("hubq_bulk_terminal", (-40.0, 26.0, -60.0), (20.0, 52.0, 110.0)),
           hbox("hubq_bulk_terminal", (0.0, 9.0 + TB_Y, 30.0), (14.0, 8.0, 16.0)),
           hbox("hubq_bulk_terminal", (18.0, TB_Y + 9.0, 30.0), (26.0, 16.0, 4.0)),
           hbox("hubq_bulk_terminal", (15.0, TB_Y - 2.0, 0.0), (20.0, 6.0, 260.0)),
           hbox("hubq_bulk_terminal", (-24.0, 76.0, 0.0), (36.0, 40.0, 6.0), (0.0, 0.0, 48.0)),
           hbox("hubq_bulk_terminal", (-12.0, 6.0, 40.0), (16.0, 10.0, 24.0))]
    for z in (-110.0, 110.0):
        out.append(hbox("hubq_bulk_terminal", (0.0, (TB_Y - 4.0) / 2, z), (18.0, TB_Y - 4.0, 18.0)))
    return out


# ---------------------------------------------------------------- small pier: small ships + guard ships
SP_L, SP_Y, SP_W = 150.0, 22.0, 18.0


def pier_small_parts(lod):
    """a small pier raised on lattice legs: deck 150 x 18 m at 22 m, along +Z from the root (z 0). -X side: two berths
    for small ships (to ~45 m) with boarding tubes and a fuel arm; +X side: a guard-ship berth (to ~90 m) with clamp
    arms, the guard house and a point-defence turret on a mast at the pier head; a lift tower at the root and a small
    control tower"""
    P = {"hubq_pier_small": Part("hubq_pier_small")}
    d = P["hubq_pier_small"]
    small = small_for(lod)
    W, L, Y = SP_W, SP_L, SP_Y
    box(d, HULL, (0, Y - 1.5, L / 2), (W, 3.0, L))
    truss(d, STEEL, (0.0, Y - 5.0, 0.0), (0.0, Y - 5.0, L), 8.0, 4.0, lod, solid_mat=STEEL)        # keel truss
    for z in (25.0, 75.0, 125.0):                                                          # lattice legs
        lattice_tower(d, STEEL, (0.0, z), 0.0, Y - 7.0, 6.0, 3.0, lod, bays=3)
        box(d, CON, (0, 0.5, z), (14.0, 1.0, 14.0))
    box(d, GRATE, (0, Y + 0.1, L / 2), (6.0, 0.2, L))                                      # walkway / cart lane
    for sx in (-1, 1):
        box(d, HAZ, (sx * (W / 2 - 0.4), Y + 0.15, L / 2), (0.8, 0.3, L))
    # -X: two small-ship berths (boarding tubes, fuel arm)
    for z in (35.0, 90.0):
        cyl(d, LIGHT, (-W / 2, Y + 2.0, z), (-W / 2 - 9.0, Y + 2.0, z), 1.6, small)
        lathe(d, HAZ, crect(1.6, 2.2, -0.4, 0.4), small, rot_y(-math.pi / 2), (-W / 2 - 9.0, Y + 2.0, z), smooth=False)
        cyl(d, STEEL, (-W / 2 + 1.0, Y, z + 12.0), (-W / 2 + 1.0, Y + 8.0, z + 12.0), 0.5, 6)
        cyl(d, STEEL, (-W / 2 + 1.0, Y + 8.0, z + 12.0), (-W / 2 - 10.0, Y + 5.0, z + 12.0), 0.4, 6)
        for dz in (-22.0, 22.0):
            box(d, BEACON if lod >= 2 else FLOOD, (-W / 2 - 24.0, Y, z + dz), (1.0, 1.0, 1.0))
        if lod < 2:
            box(d, MARK, (-W / 2 - 0.1, Y - 1.5, z), (0.2, 2.0, 44.0))
    # +X: the guard berth with clamp arms
    for z in (45.0, 85.0, 125.0):
        truss(d, BLK, (W / 2, Y - 2.0, z), (W / 2 + 14.0, Y - 2.0, z), 1.6, 1.6, lod, solid_mat=BLK)
        box(d, HAZ, (W / 2 + 14.5, Y - 2.0, z), (1.2, 3.0, 4.0))
    box(d, HULL, (W / 2 - 4.0, Y + 3.0, 60.0), (8.0, 6.0, 14.0), chamfer=0.3)            # guard house (armoured)
    box(d, DARK, (W / 2 + 0.05, Y + 4.5, 60.0), (0.1, 0.8, 10.0))
    # pier head: point-defence turret on a mast
    lattice_tower(d, STEEL, (0.0, L - 6.0), Y, Y + 14.0, 1.6, 1.0, lod, bays=3)
    ylathe(d, HULL, [(0.0, Y + 14.0), (2.4, Y + 14.0), (2.0, Y + 16.4), (0.0, Y + 16.4)], small, t=(0.0, 0.0, L - 6.0))
    for sx in (-0.5, 0.5):
        cyl(d, BLK, (sx, Y + 15.4, L - 6.0), (sx, Y + 16.6, L - 12.0), 0.25, 6)
    # the root: a lift tower and the small control tower
    box(d, HULL, (0, Y / 2, -3.0), (8.0, Y + 6.0, 8.0))
    lattice_tower(d, STEEL, (W / 2 + 6.0, 6.0), 0.0, Y + 18.0, 2.4, 1.6, lod, bays=6)
    box(d, LIGHT, (W / 2 + 6.0, Y + 20.5, 6.0), (7.0, 4.0, 7.0))
    for sz in (-1, 1):
        box(d, COOL, (W / 2 + 6.0, Y + 21.0, 6.0 + sz * 3.55), (6.0, 1.6, 0.1))
    for sx in (-1, 1):
        box(d, COOL, (W / 2 + 6.0 + sx * 3.55, Y + 21.0, 6.0), (0.1, 1.6, 6.0))
    beacon(d, (W / 2 + 6.0, Y + 23.2, 6.0), 0.8)
    if lod < 2:
        for z in range(10, int(L), 20):
            lamp(d, W / 2 - 1.0, Y, float(z), 3.0)
            lamp(d, -W / 2 + 1.0, Y, float(z), 3.0)
    beacon(d, (0, Y + 1.0, L + 0.6), 1.4)
    return P


def pier_small_hits():
    return [hbox("hubq_pier_small", (0, SP_Y - 3.5, SP_L / 2), (SP_W, 7.0, SP_L)),
            hbox("hubq_pier_small", (SP_W / 2 - 4.0, SP_Y + 3.0, 60.0), (8.0, 6.0, 14.0)),
            hbox("hubq_pier_small", (SP_W / 2 + 7.0, SP_Y - 2.0, 85.0), (14.0, 3.0, 84.0)),
            hbox("hubq_pier_small", (SP_W / 2 + 6.0, (SP_Y + 23.0) / 2, 6.0), (7.0, SP_Y + 23.0, 7.0)),
            hbox("hubq_pier_small", (0.0, SP_Y + 8.0, SP_L - 6.0), (4.8, 17.0, 4.8))] + [
        hbox("hubq_pier_small", (0.0, (SP_Y - 7.0) / 2, z), (12.0, SP_Y - 7.0, 12.0)) for z in (25.0, 75.0, 125.0)]


# ================================================================ the drilling-platform mine (borehole mining rig)
# Author: «шахта снаружи как буровая платформа ... много ферм». An offshore-jacket look over a borehole: hydraulic
# borehole mining (a real method: a jet monitor breaks ore at depth and the slurry is lifted up the riser) - on an
# asteroid the slurry is volatile-bearing (water ice / carbonaceous rock), so the rig also carries a volatiles capture
# stack (its "flare" is the vent of the gas separator, emissive). Four battered lattice legs anchored in the rock
# (a jacket), two decks at 60 / 72 m, the derrick, pipe racks, process modules, two pedestal cranes, a cantilevered
# landing pad for small ships, escape capsules; ships fly through the 60 m gap under the decks.
RG_Y, RG_LEG, RG_TOPLEG = 60.0, 42.0, 22.0


def rig_parts(lod):
    P = {"hubq_drill_rig": Part("hubq_drill_rig")}
    r = P["hubq_drill_rig"]
    seg = seg_for(lod)
    small = small_for(lod)
    Y = RG_Y
    # the jacket: four battered lattice legs (box trusses) from rock anchors to the deck corners, braced in X panels
    feet = [(sx * RG_LEG, sz * RG_LEG) for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    tops = [(sx * RG_TOPLEG, sz * RG_TOPLEG) for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for (fx, fz), (tx, tz) in zip(feet, tops):
        truss(r, YEL, (fx, 0.0, fz), (tx, Y, tz), 4.0, 4.0, lod, up=(fx, 0.0, -fz), solid_mat=YEL)
        box(r, CON, (fx, 1.0, fz), (10.0, 2.0, 10.0))
        cyl(r, STEEL, (fx, 2.0, fz), (fx, -6.0, fz), 1.2, small)                           # grouted anchor pile
    levels = (20.0, 40.0)
    for i in range(4):
        (fx, fz), (tx, tz) = feet[i], tops[i]
        (gx, gz), (ux, uz) = feet[(i + 1) % 4], tops[(i + 1) % 4]
        pa = lambda y, x0, z0, x1, z1: (x0 + (x1 - x0) * y / Y, y, z0 + (z1 - z0) * y / Y)
        prev = None
        for y in (6.0,) + levels + (Y - 4.0,):
            p0, p1 = pa(y, fx, fz, tx, tz), pa(y, gx, gz, ux, uz)
            truss(r, YEL, p0, p1, 1.8, 1.8, max(lod, 1), solid_mat=YEL)
            if prev and lod < 2:
                bar(r, YEL, prev[0], p1, 0.9)
                bar(r, YEL, prev[1], p0, 0.9)
            prev = (p0, p1)
    # decks
    D = RG_TOPLEG + 8.0
    box(r, HULL, (0, Y + 1.0, 0), (2 * D, 2.0, 2 * D))
    truss(r, STEEL, (-D, Y - 2.0, 0.0), (D, Y - 2.0, 0.0), 4.0, 4.0, lod, solid_mat=STEEL)
    truss(r, STEEL, (0.0, Y - 2.0, -D), (0.0, Y - 2.0, D), 4.0, 4.0, lod, solid_mat=STEEL)
    box(r, HULL, (-6.0, Y + 13.0, -4.0), (2 * D - 16.0, 1.6, 2 * D - 12.0))                # upper deck
    for sx in (-1, 1):
        for sz in (-1, 1):
            bar(r, STEEL, (sx * (D - 10.0), Y + 2.0, sz * (D - 8.0)), (sx * (D - 10.0), Y + 12.2, sz * (D - 8.0)), 1.4)
    # process modules (boxes) on the main deck and their roofs
    mods = [(-20.0, -18.0, 18.0, 9.0, 14.0, LIGHT), (-20.0, 8.0, 18.0, 9.0, 16.0, HULL), (14.0, -20.0, 14.0, 10.0, 12.0, RST),
            (18.0, 18.0, 12.0, 8.0, 14.0, LIGHT)]
    for x, z, w, h, l, m in mods:
        box(r, m, (x, Y + 2.0 + h / 2, z), (w, h, l), chamfer=0.3)
        if lod < 2:
            box(r, WARM if _hash(x, z) > 0.4 else DARK, (x + w / 2 + 0.05, Y + 2.0 + h * 0.6, z), (0.1, 1.2, l * 0.7))
    # the derrick (lattice tower) on the drill floor, crown block, travelling block, drill string
    dx, dz = 4.0, 2.0
    box(r, BLK, (dx, Y + 15.5, dz), (16.0, 3.0, 16.0))                                    # drill floor / substructure
    lattice_tower(r, YEL, (dx, dz), Y + 17.0, Y + 82.0, 6.0, 1.8, lod, bays=12)
    box(r, YEL, (dx, Y + 83.0, dz), (5.0, 2.4, 5.0))
    beacon(r, (dx, Y + 85.0, dz), 1.2)
    if lod < 2:
        box(r, BLK, (dx, Y + 55.0, dz), (2.0, 4.0, 2.0))                                   # travelling block
        cyl(r, STEEL, (dx, Y + 82.0, dz), (dx, Y + 57.0, dz), 0.2, 6)
        for k in range(3):
            beacon(r, (dx + 6.0 - 4.2 * (k + 1) / 4, Y + 17.0 + 16.0 * (k + 1), dz + 6.0 - 4.2 * (k + 1) / 4), 0.6)
    cyl(r, STEEL, (dx, Y + 15.0, dz), (dx, 2.0, dz), 0.8, small)                           # riser / conductor
    if lod < 2:
        for y in (12.0, 28.0, 44.0):                                                       # riser guides from the jacket
            bar(r, YEL, (dx, y, dz), (-RG_LEG + (RG_LEG - RG_TOPLEG) * y / Y, y, -RG_LEG + (RG_LEG - RG_TOPLEG) * y / Y), 0.5)
    # the wellhead on the rock: a BOP-like stack with a guard frame
    box(r, CON, (dx, 0.75, dz), (12.0, 1.5, 12.0))
    cyl(r, RST, (dx, 1.5, dz), (dx, 9.0, dz), 2.2, small)
    box(r, BLK, (dx, 5.0, dz), (6.0, 2.0, 6.0))
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                bar(r, YEL, (dx + sx * 4.0, 1.5, dz + sz * 4.0), (dx + sx * 4.0, 11.0, dz + sz * 4.0), 0.5)
    # pipe racks along the -X edge of the upper deck (drill pipe, horizontal bundles)
    for k in range(5 if lod < 2 else 1):
        for j in range(3 if lod < 2 else 1):
            cyl(r, STEEL if lod < 2 else BLK, (-D + 4.0 + k * 1.2, Y + 15.4 + j * 1.2, -20.0), (-D + 4.0 + k * 1.2, Y + 15.4 + j * 1.2, 16.0), 0.5, small)
    # the vent / volatiles flare boom: a lattice boom out over the +X / +Z corner
    fb0, fb1 = (D - 4.0, Y + 16.0, D - 4.0), (D + 36.0, Y + 40.0, D + 36.0)
    truss(r, YEL, fb0, fb1, 2.4, 2.4, lod, solid_mat=YEL)
    cyl(r, STEEL, fb1, _add(fb1, (2.0, 4.0, 2.0)), 0.8, small)
    box(r, FLARE, _add(fb1, (2.6, 6.0, 2.6)), (3.0, 4.0, 3.0))
    # cantilevered landing pad (octagon) for small ships off the -Z side, with its support truss
    pc = (-10.0, Y + 3.0, -(D + 22.0))
    ylathe(r, HULL, [(0.0, pc[1] - 1.0), (20.0, pc[1] - 1.0), (20.0, pc[1]), (0.0, pc[1])], 8, t=(pc[0], 0.0, pc[2]))
    if lod < 2:
        ylathe(r, HAZ, [(18.8, pc[1] + 0.05), (17.5, pc[1] + 0.05)], 8, t=(pc[0], 0.0, pc[2]))
        box(r, MARK, (pc[0], pc[1] + 0.06, pc[2]), (12.0, 0.05, 2.0))
        box(r, MARK, (pc[0], pc[1] + 0.06, pc[2]), (2.0, 0.05, 12.0))
        for k in range(8):
            a = TAU * k / 8 + math.pi / 8
            box(r, FLOOD, (pc[0] + 19.0 * math.cos(a), pc[1] + 0.4, pc[2] + 19.0 * math.sin(a)), (0.8, 0.5, 0.8))
    for sx in (-1, 1):
        truss(r, STEEL, (pc[0] + sx * 12.0, Y - 3.0, -D), (pc[0] + sx * 8.0, pc[1] - 3.0, pc[2] + 8.0), 2.0, 3.0, lod, solid_mat=STEEL)
        bar(r, STEEL, (pc[0] + sx * 12.0, Y - 14.0, -D + 2.0), (pc[0] + sx * 10.0, pc[1] - 2.0, pc[2] + 2.0), 0.8)
    # two pedestal cranes with lattice booms
    for cx, cz, ang in ((D - 6.0, -D + 6.0, -0.6), (-D + 6.0, D - 6.0, 2.4)):
        cyl(r, YEL, (cx, Y + 2.0, cz), (cx, Y + 14.0, cz), 1.6, small)
        box(r, YEL, (cx, Y + 15.5, cz), (4.0, 3.0, 4.0))
        tip = (cx + 34.0 * math.cos(ang), Y + 34.0, cz + 34.0 * math.sin(ang))
        truss(r, YEL, (cx, Y + 15.0, cz), tip, 1.6, 1.6, lod, up=(0.0, 1.0, 0.0), solid_mat=YEL)
        cyl(r, STEEL, tip, (tip[0], Y + 12.0, tip[2]), 0.12, 6)
        box(r, HAZ, (tip[0], Y + 11.0, tip[2]), (1.2, 1.4, 1.2))
    # escape capsules along the +X side
    if lod < 2:
        for k in range(3):
            z = -10.0 + 8.0 * k
            box(r, HAZ, (D + 1.2, Y + 4.0, z), (2.4, 3.0, 6.0), chamfer=0.4)
    # corner beacons and floodlights under the deck (lighting the fly-through)
    for sx in (-1, 1):
        for sz in (-1, 1):
            beacon(r, (sx * D, Y + 2.6, sz * D), 1.2)
            if lod < 2:
                box(r, FLOOD, (sx * (D - 2.0), Y - 0.6, sz * (D - 2.0)), (1.2, 0.8, 1.2))
    return P


def rig_hits():
    D = RG_TOPLEG + 8.0
    out = [hbox("hubq_drill_rig", (0, RG_Y + 6.0, 0), (2 * D, 12.0, 2 * D)),
           hbox("hubq_drill_rig", (4.0, RG_Y + 50.0, 2.0), (12.0, 66.0, 12.0)),
           hbox("hubq_drill_rig", (4.0, RG_Y / 2 + 4.0, 2.0), (3.0, RG_Y, 3.0)),
           hbox("hubq_drill_rig", (4.0, 5.0, 2.0), (12.0, 10.0, 12.0)),
           hbox("hubq_drill_rig", (-10.0, RG_Y + 2.5, -(D + 22.0)), (40.0, 3.0, 40.0)),
           hbox("hubq_drill_rig", (D + 18.0, RG_Y + 28.0, D + 18.0), (6.0, 6.0, 54.0), (0.0, 45.0, 0.0))]
    for sx in (-1, 1):
        for sz in (-1, 1):
            out.append(hbox("hubq_drill_rig", (sx * (RG_LEG + RG_TOPLEG) / 2, RG_Y / 2, sz * (RG_LEG + RG_TOPLEG) / 2),
                            (8.0, RG_Y + 2.0, 8.0)))
    return out


# ================================================================ the kit
KIT = {
    "hubq_bucket_wheel": (bwe_parts, bwe_hits, "Роторный экскаватор: колесо 18 м, 14 ковшей, стрела 45 м, на двух путях с анкерами"),
    "hubq_rail": (rail_parts, lambda: [hbox("hubq_rail", (0, 0.5, RAIL_L / 2), (4.6, 1.0, RAIL_L))],
                  "Рельсовый путь 50 м (колея 2,4 м, третий рельс, анкеры)"),
    "hubq_rail_curve": (rail_curve_parts, lambda: [hbox("hubq_rail_curve", _lerp((0, 0.5, 0), _add(curve_end(), (0, 0.5, 0)), 0.5),
                                                        (6.0, 1.0, CURVE_R * math.radians(CURVE_DEG)), (0.0, CURVE_DEG / 2, 0.0))],
                        "Рельсовый путь, кривая R 100 м, 15°"),
    "hubq_rail_end": (rail_end_parts, lambda: [hbox("hubq_rail_end", (0, 1.5, 6.0), (5.0, 3.0, 12.0))], "Тупиковый упор"),
    "hubq_ore_wagon": (wagon_parts, lambda: [hbox("hubq_ore_wagon", (0, 3.4, 0), (3.4, 4.8, 12.6))],
                       "Рудный вагон-хоппер 12,6 м с крышками (~100 т)"),
    "hubq_loco": (loco_parts, lambda: [hbox("hubq_loco", (0, 3.0, 0), (3.4, 4.6, 14.0))], "Рудничный электровоз 14 м"),
    "hubq_conveyor": (conveyor_parts, lambda: [hbox("hubq_conveyor", (0, CONV_Y - 0.5, 25.0), (6.0, 4.0, 50.0))],
                      "Трубчатый (закрытый) конвейер 50 м на эстакаде"),
    "hubq_bridge": (bridge_parts, bridge_hits, "Ферменный конвейерный мост 200 м (пролёт над каньоном)"),
    "hubq_pit_side": (pit_side_parts, lambda: [hbox("hubq_pit_side", ((PLAT + FLOOR_X) / 2, (BASE_Y - 15.0) / 2, 50.0), (PLAT - FLOOR_X, -BASE_Y - 15.0, 100.0)),
                                               hbox("hubq_pit_side", (PLAT / 2 - 23.0, -7.5, 50.0), (PLAT + 46.0, 15.0, 100.0))],
                      "Борт карьера 100 м: 3 уступа по 15 м, бермы 40 м"),
    "hubq_pit_cut": (pit_cut_parts, lambda: [hbox("hubq_pit_cut", ((PLAT + FLOOR_X) / 2, (BASE_Y - 45.0) / 2, CUT_L / 2), (PLAT - FLOOR_X, -BASE_Y - 45.0, CUT_L))] + [
                         hbox("hubq_pit_cut", (PLAT / 2 - 23.0, -22.5, z), (PLAT + 46.0, 45.0, (CUT_L - CUT_W) / 2)) for z in ((CUT_L - CUT_W) / 4, CUT_L - (CUT_L - CUT_W) / 4)],
                     "Борт карьера 200 м с прорезью 60 м (устье каньона)"),
    "hubq_pit_corner": (pit_corner_parts, lambda: [hbox("hubq_pit_corner", (PIT_R * 0.75, -30.0, PIT_R * 0.75), (PIT_R * 0.9, 30.0, PIT_R * 0.9), (0.0, 0.0, 0.0))],
                        "Угол карьера 90°, радиус по бровке 140 м"),
    "hubq_pit_floor": (pit_floor_parts, lambda: [hbox("hubq_pit_floor", (0, -7.5, 0), (230.0, 15.0, 230.0))], "Дно карьера 230x230"),
    "hubq_canyon": (canyon_parts, canyon_hits, "Каньон-траншея 100 м: дно 60 м, глубина 45 м"),
    "hubq_ground": (ground_parts, lambda: [hbox("hubq_ground", (0, -6.0, 0), (100.0, 12.0, 100.0))], "Грунт астероида, плита 100x100"),
    "hubq_portal": (portal_parts, portal_hits, "Нора: портал штольни 12x12 в скальном выходе, шлюз"),
    "hubq_headframe": (headframe_parts, headframe_hits, "Копёр над стволом 48 м, подъёмная машина"),
    "hubq_hab_block": (hab_block_parts, hab_block_hits, "Жилой блок под реголитом: 4 модуля 9x36 м, радиаторы"),
    "hubq_hab_tower": (hab_tower_parts, hab_tower_hits, "Общежитие рудника: 8 этажей, водяная защита"),
    "hubq_store": (store_parts, store_hits, "Склад-ангар 60 м (свод) и двор контейнеров под сетями"),
    "hubq_ore_pile": (lambda lod: ore_pile_parts(lod, "hubq_ore_pile", 60.0, 20.0, ORE),
                      lambda: ore_pile_hits("hubq_ore_pile", 60.0, 20.0), "Рудный штабель-пирамида 60x60x20 м под сетью"),
    "hubq_ore_pile_s": (lambda lod: ore_pile_parts(lod, "hubq_ore_pile_s", 36.0, 12.0, OREG),
                        lambda: ore_pile_hits("hubq_ore_pile_s", 36.0, 12.0), "Штабель концентрата 36x36x12 м"),
    "hubq_bulk_terminal": (terminal_parts, terminal_hits, "Терминал навалочных грузов: силосы, башни, причал 300 м на высоте 90 м"),
    "hubq_pier_small": (pier_small_parts, pier_small_hits, "Малый пирс 150 м: 2 места малым кораблям, место охраны, турель"),
    "hubq_drill_rig": (rig_parts, rig_hits, "Буровая платформа над скважиной: ферменные опоры 60 м, вышка, площадка"),
}

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Рудник на астероиде (tools/models/units/hub_mine.py, docs/MINE_MODELS.md).")
             for k, v in KIT.items()}


# ================================================================ the demo: one mine site as an assembly
def _ry(a_deg):
    return rot_y(math.radians(a_deg))


def mine_site():
    """nodes [(id, folder, pos, rot_deg, role, parent)] of the demo site (pit centre at the origin, rim at y 0):
    a 540 m pit (3 benches) with the bucket-wheel machine and a train on the east bench, a curved track round the NE
    corner to a burrow portal on the north bench, a canyon cut north out of the pit (60 m floor, 45 m deep, 300 m) under
    two truss bridges, the headframe west of the canyon, a pipe conveyor over the bridge east to the stockyard, the bulk
    terminal at the east edge, the settlement between, the small pier and the drilling rig in the south-east."""
    nodes = []
    cnt = {}

    def add(folder, pos, rot=(0.0, 0.0, 0.0), role="structure"):
        k = cnt.get(folder, 0)
        cnt[folder] = k + 1
        nodes.append((f"{folder[5:]}_{k}", folder, tuple(float(x) for x in pos), tuple(float(x) for x in rot), role, None))

    rim = PIT_R + 100.0                         # 240: the straight sides' rim line
    for k in range(4):
        a = -90.0 * k
        R = _ry(a)
        if k == 3:                              # north: the canyon cut (a 200 m piece) instead of two plain sides
            add("hubq_pit_cut", apply(R, (rim, 0.0, -100.0)), (0.0, a, 0.0), "pit")
        else:
            for j in range(2):
                add("hubq_pit_side", apply(R, (rim, 0.0, -100.0 + 100.0 * j)), (0.0, a, 0.0), "pit")
        add("hubq_pit_corner", apply(R, (100.0, 0.0, 100.0)), (0.0, a, 0.0), "pit")
    add("hubq_pit_floor", (0.0, -45.0, 0.0), role="pit")
    # the canyon north from the cut (canyon pieces along +Z turned to run -Z)
    for i in range(3):
        add("hubq_canyon", (0.0, 0.0, -(rim + PLAT) - 100.0 * i), (0.0, 180.0, 0.0), "canyon")
    # ground tiles: a 100 m grid (centres at 50 + 100 i) outside the pit square and the canyon
    for ix in range(-4, 10):
        for iz in range(-6, 6):
            x, z = 50.0 + 100.0 * ix, 50.0 + 100.0 * iz
            if abs(x) < 300.0 and abs(z) < 300.0:
                continue
            if abs(x) < 100.0 and z < -300.0:
                continue
            add("hubq_ground", (x, 0.0, z), role="ground")
    # --- east bench 1 (y -15): train track at x 200 and the excavator's two tracks at x 208 / 224 (straight z -100..100)
    yb = -15.0
    ex = 216.0
    for z in (-100.0, -50.0, 0.0, 50.0):
        add("hubq_rail", (200.0, yb, z), role="rail")
        add("hubq_rail", (ex - BWE_TRACK, yb, z), role="rail")
        add("hubq_rail", (ex + BWE_TRACK, yb, z), role="rail")
    add("hubq_rail_end", (200.0, yb, 100.0), role="rail")
    add("hubq_bucket_wheel", (ex, yb, 40.0), role="machine")
    # the train under the excavator's spout (spout at z 40): loco + 6 wagons heading -Z
    zc = 40.0
    for i in range(6):
        add("hubq_ore_wagon", (200.0, yb, zc - 12.8 * (i - 1)), role="vehicle")
    add("hubq_loco", (200.0, yb, zc - 12.8 * 5 - 13.6), role="vehicle")
    # the track round the NE corner (6 x 15 deg, R 100 about (100, -100)) and west along the north bench to the portal
    pos, hd = (200.0, yb, -100.0), 180.0       # heading -Z
    for i in range(6):                          # left turn: heading 180 -> 270 (west)
        add("hubq_rail_curve", pos, (0.0, hd, 0.0), "rail")
        e = apply(_ry(hd), curve_end())
        pos, hd = _add(pos, e), hd + CURVE_DEG
    for i in range(1):
        add("hubq_rail", pos, (0.0, hd, 0.0), "rail")
        pos = _add(pos, apply(_ry(hd), (0.0, 0.0, RAIL_L)))
    for i in range(2):                          # right turn (pieces laid in reverse): heading 270 -> 240
        b = hd - CURVE_DEG - 180.0
        npos = _sub(pos, apply(_ry(b), curve_end()))
        add("hubq_rail_curve", npos, (0.0, b, 0.0), "rail")
        pos, hd = npos, hd - CURVE_DEG
    add("hubq_rail", pos, (0.0, hd, 0.0), "rail")              # runs 17 m on into the tunnel
    mouth = _add(pos, apply(_ry(hd), (0.0, 0.0, 33.0)))
    add("hubq_portal", mouth, (0.0, hd, 0.0), "burrow")
    # a second train waiting on the north straight
    add("hubq_loco", (58.0, yb, -200.0), (0.0, 270.0, 0.0), "vehicle")
    for i in range(2):
        add("hubq_ore_wagon", (58.0 + 13.6 + 12.8 * i, yb, -200.0), (0.0, 270.0, 0.0), "vehicle")
    # --- the burrow shaft (headframe) west of the canyon, its conveyor east over the canyon bridge to the stockyard
    add("hubq_headframe", (-262.0, 0.0, -450.0), (0.0, -90.0, 0.0), "burrow")
    zc = -450.0
    x = -250.0
    while x < -110.0:
        add("hubq_conveyor", (x, 0.0, zc), (0.0, 90.0, 0.0), "conveyor")
        x += 50.0
    add("hubq_bridge", (-100.0, 0.0, zc), (0.0, 90.0, 0.0), "bridge")
    add("hubq_bridge", (-100.0, 0.0, -560.0), (0.0, 90.0, 0.0), "bridge")       # a second (service) bridge further out
    x = 100.0
    while x < 380.0:
        add("hubq_conveyor", (x, 0.0, zc), (0.0, 90.0, 0.0), "conveyor")
        x += 50.0
    # stockyard (fed by the conveyor), piles in rows
    for i, (px, pz) in enumerate(((470.0, -450.0), (560.0, -450.0), (650.0, -450.0), (470.0, -360.0))):
        add("hubq_ore_pile", (px, 0.0, pz), (0.0, 180.0 if i == 0 else 0.0, 0.0), "stockpile")
    for px, pz in ((560.0, -370.0), (620.0, -370.0)):
        add("hubq_ore_pile_s", (px, 0.0, pz), (0.0, 180.0, 0.0), "stockpile")
    # reclaim conveyor from the stockyard south to the terminal silos
    z = -430.0
    while z < -230.0:
        add("hubq_conveyor", (750.0, 0.0, z), role="conveyor")
        z += 50.0
    for x in (750.0, 800.0):                       # east along z -230 to the silos' collector
        add("hubq_conveyor", (x, 0.0, -225.0), (0.0, 90.0, 0.0), "conveyor")
    add("hubq_bulk_terminal", (880.0, 0.0, -100.0), role="terminal")
    # --- the settlement
    for i, (hx, hz) in enumerate(((380.0, 40.0), (380.0, 100.0), (450.0, 40.0), (450.0, 100.0), (380.0, 160.0))):
        add("hubq_hab_block", (hx, 0.0, hz), (0.0, 0.0 if i % 2 == 0 else 180.0, 0.0), "habitat")
    for hx, hz in ((530.0, 60.0), (530.0, 130.0)):
        add("hubq_hab_tower", (hx, 0.0, hz), (0.0, 90.0, 0.0), "habitat")
    for hx, hz in ((400.0, -120.0), (480.0, -120.0), (600.0, -120.0)):
        add("hubq_store", (hx, 0.0, hz), role="storage")
    # the small pier (root in the settlement, deck out east) and the drilling rig in the south-east
    add("hubq_pier_small", (640.0, 0.0, 250.0), (0.0, 90.0, 0.0), "pier")
    add("hubq_drill_rig", (520.0, 0.0, 430.0), (0.0, 20.0, 0.0), "rig")
    return nodes


SETS = {"hubq_mine_site": {
    "title": "Рудник на астероиде (демо-сборка)",
    "description": "Карьер 540 м с роторным экскаватором и поездом, нора-портал, каньон с ферменными мостами, копёр, "
                   "трубчатые конвейеры, рудный двор, терминал навалочных грузов, поселение, малый пирс, буровая платформа "
                   "(tools/models/units/hub_mine.py, docs/MINE_MODELS.md).",
    "nodes": mine_site(),
    "view_lods": {0: 0, 1: 1, 2: 2}}}
ROOTS["hubq_mine_site"] = "hub"
