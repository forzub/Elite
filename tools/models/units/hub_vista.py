"""Landmark structures after the reference video (author 2026-11-17: «просмотри еще раз видео ... на 4:39:40 видны очень
интересные конструкции и дальше тоже. их надо повторить»; the whole 8 h video reviewed - a pool of ~60 shots, catalogue
D:/tmp/yt/catalog.jpg). Free-fall versions: every module stands on its own mounting plate at its origin, "out" = +Y,
so the settlement generator (hub_belt.py) hangs them on district spines like the rest of the kit.

  hubv_crown_spire    crowned spire (4:39:42): skirt platform with hanging pipework, lattice shaft, crown of arcs + rings
  hubv_saucer_tower   "mushroom" (2:00): stalk with a lit disc, radial truss arms, searchlights down the stalk
  hubv_rig            offshore-rig platform (7:12, 1:09): pyramid lattice base, stacked decks, derrick, cranes, pads
  hubv_cage_gantry    lattice sphere in a portal gantry (0:34)
  hubv_ring_wheel     lit ring wheel with spokes (4:39:00)
  hubv_water_tower    hyperboloid lattice tower with a bowl cabin (1:31)
  hubv_gallery_tower  cylinder tower ringed with balconies (0:23)
  hubv_mast           tall lattice mast, red / white bands (0:16)
  hubv_cantilever     curved truss boom (4:39:57) - a 500 m arm with lamps along it
  hubv_refinery       columns, pipe racks, a flare stack with an open flame (0:15) - haze source
  hubv_pad_star       landing pad with radial light lines (0:30)
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, COOL, DARK, beacon  # noqa: F401
from units.hub_mega import FACADE, FACADE_L  # noqa: F401
from units.hub_belt import GRATE, _seg, sphere  # noqa: F401

register_materials({
    "eq_hub_flare": ((1.00, 0.48, 0.14), (0.10, 0.10, 0.10), 20),        # an open flare / furnace glow (emissive)
    "eq_hub_mast_red": ((0.55, 0.12, 0.09), (0.20, 0.20, 0.20), 30),
})
FLARE, RED = "eq_hub_flare", "eq_hub_mast_red"


def ring_y(p, mat, c, r, w, t, seg):
    """a flat ring (annulus band) around the Y axis through c: radius r, width w (radial), thickness t"""
    ylathe(p, mat, [(r, c[1] - t / 2), (r + w, c[1] - t / 2), (r + w, c[1] + t / 2), (r, c[1] + t / 2), (r, c[1] - t / 2)],
           seg, t=(c[0], 0, c[2]))


def lattice_column(p, mat, y0, y1, h0, h1, bays, chord=1.2, brace=0.6, cs=6, cx=0.0, cz=0.0):
    """a square lattice column along Y (axis at cx, cz) from y0 (half width h0) to y1 (half width h1): 4 chords, rings,
    diagonal braces"""
    def at(y, sx, sz):
        t = (y - y0) / (y1 - y0)
        h = h0 + (h1 - h0) * t
        return (cx + sx * h, y, cz + sz * h)
    corners = ((-1, -1), (1, -1), (1, 1), (-1, 1))
    for sx, sz in corners:
        cyl(p, mat, at(y0, sx, sz), at(y1, sx, sz), chord, cs)
    for k in range(bays + 1):
        y = y0 + (y1 - y0) * k / bays
        for i in range(4):
            a, b = corners[i], corners[(i + 1) % 4]
            cyl(p, mat, at(y, *a), at(y, *b), brace, 4)
        if k < bays:
            y2 = y0 + (y1 - y0) * (k + 1) / bays
            for i in range(4):
                a, b = corners[i], corners[(i + 1) % 4]
                cyl(p, mat, at(y, *a), at(y2, *b), brace * 0.8, 4)


def arc_tube(p, mat, centre, r, a0, a1, plane_angle, rt, n=14, seg=6, sy=1.0):
    """a tube along a circular arc of radius r (stretched sy times vertically) in the vertical plane at plane_angle
    (radians about Y) through centre"""
    pts = []
    ca, sa = math.cos(plane_angle), math.sin(plane_angle)
    for k in range(n + 1):
        a = a0 + (a1 - a0) * k / n
        u, v = r * math.cos(a), r * math.sin(a) * sy
        pts.append((centre[0] + u * ca, centre[1] + v, centre[2] + u * sa))
    tube(p, mat, pts, rt, seg)


# ---------------------------------------------------------------- 1 crowned spire (rework 2026-11-17, author: «начинай с
# диска-потолка и шпиля»): the video's spires (4:39:42) are dense - a wide skirt platform heaped with plant and hanging
# pipe "roots", a lattice shaft with boxy clusters ringing it at mid height, and a heavy crown: a cage of arcs round a
# core, stacked rings, a vertical wheel on top, needles. Three height variants (a / b / c) so a group of them differs.
import random as _random  # noqa: E402


def crown_spire_builder(name, H, seed):
    """H: the shaft top (m); the crown sits above it"""
    def build(lod):
        P = {name: Part(name)}
        s = P[name]
        rng = _random.Random(seed)
        seg = _seg(lod, 24)
        hi = lod < 2
        box(s, STEEL, (0, 4.0, 0), (44.0, 8.0, 44.0))
        cyl(s, HULL, (0, 8.0, 0), (0, 60.0, 0), 16.0, max(8, seg // 2))                   # the root stem
        # two dishes: a small lower one, the wide skirt platform above
        ylathe(s, HULL, [(0.0, 34.0), (16.0, 34.0), (40.0, 46.0), (42.0, 50.0), (0.0, 50.0)], seg, soft=30)
        ylathe(s, HULL, [(0.0, 52.0), (18.0, 52.0), (62.0, 74.0), (76.0, 80.0), (76.0, 87.0), (0.0, 87.0)], seg, soft=30)
        if lod < 3:
            n = 40 if hi else 14
            for k in range(n):                                                       # hanging pipe "roots"
                a = 2 * math.pi * (k + rng.random() * 0.5) / n
                r = rng.uniform(22.0, 72.0)
                y0 = 52.0 + 22.0 * max(0.0, r - 18.0) / 58.0
                L = rng.uniform(14.0, 60.0)
                cyl(s, STEEL, (r * math.cos(a), y0, r * math.sin(a)), (r * math.cos(a), y0 - L, r * math.sin(a)),
                    rng.uniform(0.8, 2.2), 6 if hi else 4)
                if k % 4 == 0:
                    box(s, WARM, (r * math.cos(a), y0 - L - 1.2, r * math.sin(a)), (2.0, 2.0, 2.0))
            for k in range(16 if hi else 6):                                         # plant heaped on the skirt
                a = rng.uniform(0, 2 * math.pi)
                r = rng.uniform(24.0, 66.0)
                w, h, d = rng.uniform(6, 18), rng.uniform(5, 20), rng.uniform(6, 14)
                box(s, rng.choice((LIGHT, HULL, STEEL)), (r * math.cos(a), 87.0 + h / 2, r * math.sin(a)), (w, h, d), rot_y(-a))
                if hi and k % 2 == 0:
                    box(s, WARM, ((r + d / 2 + 0.1) * math.cos(a), 87.0 + h * 0.6, (r + d / 2 + 0.1) * math.sin(a)), (2.0, 1.4, 2.0))
            if hi:
                for k in range(4):                                                   # tanks on the skirt
                    a = math.pi / 2 * k + 0.6
                    cyl(s, LIGHT, (54.0 * math.cos(a), 87.0, 54.0 * math.sin(a)), (54.0 * math.cos(a), 105.0, 54.0 * math.sin(a)), 6.0, 10)
        if hi:
            for k in range(32):
                a = 2 * math.pi * k / 32
                box(s, WARM, (75.0 * math.cos(a), 84.0, 75.0 * math.sin(a)), (3.0, 2.0, 3.0))   # rim lights
            ring_y(s, GRATE, (0, 88.0, 0), 74.0, 2.0, 1.2, seg)                      # rail
        # the shaft and its mid-height clusters
        lattice_column(s, STEEL, 87.0, H, 19.0, 12.0, 10 if hi else 4, 1.8, 0.8)
        cyl(s, HULL, (0, 87.0, 0), (0, H, 0), 8.0, 8)                                  # a lift core inside the lattice
        for yc, rr, nb in ((87.0 + (H - 87.0) * 0.45, 20.0, 10), (87.0 + (H - 87.0) * 0.62, 17.0, 8)):
            ring_y(s, HULL, (0, yc - 10.0, 0), 8.0, rr, 3.0, seg)
            for k in range(nb if lod < 3 else nb // 2):
                a = 2 * math.pi * k / nb
                w = rng.uniform(8, 13)
                box(s, rng.choice((HULL, LIGHT)), ((rr - 2) * math.cos(a), yc, (rr - 2) * math.sin(a)), (w, rng.uniform(14, 22), 9.0), rot_y(-a))
                if hi:
                    box(s, WARM if k % 3 else COOL, ((rr + 2.6) * math.cos(a), yc + 2.0, (rr + 2.6) * math.sin(a)), (1.6, 1.6, 1.6))
        # the crown: a core, stacked rings, a cage of arcs, a vertical wheel on top, needles
        C = H + 34.0
        cyl(s, HULL, (0, H - 6.0, 0), (0, C + 26.0, 0), 9.0, max(8, seg // 2))
        for dy, rr in ((-30.0, 26.0), (-14.0, 32.0), (2.0, 34.0), (18.0, 30.0), (32.0, 22.0)):
            ring_y(s, STEEL, (0, C + dy, 0), rr - 2.0, 3.0, 2.4, seg)
        if lod < 3:
            narc = 8 if hi else 4
            for k in range(narc):
                arc_tube(s, STEEL, (0, C + 10.0, 0), 34.0, -1.1, math.pi + 1.1, math.pi * k / narc, 1.4, 18 if hi else 8, sy=1.7)
            for k in range(6):                                                       # boxes inside the cage
                a = 2 * math.pi * k / 6 + 0.3
                box(s, LIGHT, (16.0 * math.cos(a), C + rng.uniform(-18, 14), 16.0 * math.sin(a)), (8.0, 10.0, 6.0), rot_y(-a))
            # the vertical wheel on top, facing out (+Z)
            lathe(s, STEEL, [(20.0, -1.0), (23.0, -1.0), (23.0, 1.0), (20.0, 1.0), (20.0, -1.0)], seg, t=(0.0, C + 64.0, 0.0))
            for k in range(8):
                a = 2 * math.pi * k / 8
                cyl(s, STEEL, (0, C + 64.0, 0), (21.0 * math.cos(a), C + 64.0 + 21.0 * math.sin(a), 0), 0.6, 4)
            cyl(s, STEEL, (0, C + 26.0, 0), (0, C + 43.0, 0), 2.0, 6)
            for k in range(8):                                                       # needles
                a = 2 * math.pi * k / 8
                r0 = 30.0
                cyl(s, STEEL, (r0 * math.cos(a), C + 30.0, r0 * math.sin(a)),
                    (r0 * 0.6 * math.cos(a), C + 60.0 + 18.0 * (k % 3), r0 * 0.6 * math.sin(a)), 0.6, 4)
                if hi:
                    box(s, WARM, (34.5 * math.cos(a), C + 2.0, 34.5 * math.sin(a)), (1.8, 1.8, 1.8))
        cyl(s, STEEL, (0, C + 86.0, 0), (0, C + 140.0, 0), 1.0, 6)                       # the central needle
        beacon(s, (0, C + 141.0, 0), 2.2)
        return P
    return build


def crown_spire_hits_for(name, H):
    C = H + 34.0
    return lambda: [hcyl(name, (0, 34.0, 0), (0, 87.0, 0), 76.0), hbox(name, (0, (87.0 + H) / 2, 0), (40.0, H - 87.0, 40.0)),
                    hcyl(name, (0, C - 34.0, 0), (0, C + 88.0, 0), 36.0)]


crown_spire_parts = crown_spire_builder("hubv_crown_spire", 250.0, 31)
crown_spire_hits = crown_spire_hits_for("hubv_crown_spire", 250.0)


# ---------------------------------------------------------------- 2 saucer tower
def saucer_tower_parts(lod):
    P = {"hubv_saucer_tower": Part("hubv_saucer_tower")}
    s = P["hubv_saucer_tower"]
    seg = _seg(lod, 32)
    ylathe(s, HULL, [(0.0, 0.0), (34.0, 0.0), (16.0, 40.0), (12.0, 250.0), (0.0, 250.0)], seg, soft=20)   # stalk
    ylathe(s, HULL, [(0.0, 236.0), (14.0, 236.0), (96.0, 262.0), (100.0, 268.0), (100.0, 276.0), (70.0, 286.0),
                     (0.0, 290.0)], seg, soft=25)                                                          # the disc
    ylathe(s, WARM, [(100.05, 268.5), (100.05, 275.5)], seg, ends=None)                                     # window band
    for k in range(4):                                                                   # radial truss arms under it
        a = math.pi / 2 * k + math.pi / 4
        p0 = (12.0 * math.cos(a), 200.0, 12.0 * math.sin(a))
        p1 = (140.0 * math.cos(a), 262.0, 140.0 * math.sin(a))
        cyl(s, STEEL, p0, p1, 1.6, 6)
        cyl(s, STEEL, (12.0 * math.cos(a), 240.0, 12.0 * math.sin(a)), p1, 1.2, 6)
        if lod < 3:
            beacon(s, (p1[0], p1[1] + 1.5, p1[2]), 1.4)
    if lod < 2:
        for k in range(32):
            a = 2 * math.pi * k / 32
            box(s, FLOOD, (98.0 * math.cos(a), 262.0, 98.0 * math.sin(a)), (2.2, 1.0, 2.2))  # the lit rim (from below)
        for k in range(6):                                                               # searchlights down the stalk
            a = 2 * math.pi * k / 6
            box(s, FLOOD, (40.0 * math.cos(a), 240.0, 40.0 * math.sin(a)), (3.0, 1.5, 3.0))
    cyl(s, STEEL, (0, 290.0, 0), (0, 340.0, 0), 1.5, 6)
    beacon(s, (0, 341.0, 0), 2.0)
    return P


def saucer_tower_hits():
    return [hcyl("hubv_saucer_tower", (0, 0.0, 0), (0, 250.0, 0), 16.0), hcyl("hubv_saucer_tower", (0, 236.0, 0), (0, 290.0, 0), 100.0)]


# ---------------------------------------------------------------- 3 offshore-rig platform
def rig_parts(lod):
    P = {"hubv_rig": Part("hubv_rig")}
    r = P["hubv_rig"]
    seg = _seg(lod, 16)
    box(r, STEEL, (0, 3.0, 0), (180.0, 6.0, 180.0))
    lattice_column(r, STEEL, 6.0, 120.0, 80.0, 46.0, 5 if lod < 2 else 3, 3.0, 1.2)            # the pyramid base
    box(r, HULL, (0, 124.0, 0), (130.0, 8.0, 130.0))                                          # main deck
    box(r, HULL, (-10.0, 150.0, -10.0), (100.0, 6.0, 100.0))                                  # upper deck
    box(r, LIGHT, (30.0, 137.0, 30.0), (50.0, 18.0, 40.0))                                    # modules on the main deck
    box(r, LIGHT, (-35.0, 135.0, 35.0), (40.0, 14.0, 36.0))
    box(r, HULL, (-20.0, 163.0, -25.0), (44.0, 20.0, 34.0))
    if lod < 2:
        for z in range(-50, 60, 10):
            box(r, WARM, (55.05, 137.0, float(z) * 0.3 + 30.0), (0.2, 2.0, 3.0))
        for x in range(-50, -15, 8):
            box(r, WARM, (float(x), 163.0, -42.05), (3.0, 2.0, 0.2))
    lattice_column(r, STEEL, 153.0, 280.0, 12.0, 4.0, 6 if lod < 2 else 3, 1.0, 0.5)          # the derrick
    beacon(r, (0, 282.0, 0), 1.8)
    for sx, sz, yaw in ((1, -1, 0.4), (-1, 1, 2.6)):                                          # two crane booms
        base = (sx * 55.0, 128.0, sz * 55.0)
        tip = (base[0] + 110.0 * math.cos(yaw), 200.0, base[2] + 110.0 * math.sin(yaw))
        cyl(r, STEEL, base, (base[0], 160.0, base[2]), 3.0, 8)
        cyl(r, RED, (base[0], 160.0, base[2]), tip, 1.6, 6)
        cyl(r, STEEL, (base[0], 175.0, base[2]), tip, 0.4, 4)
        if lod < 3:
            beacon(r, tip, 1.2)
    # two pads cantilevered out of the main deck
    for sx in (-1, 1):
        c = (sx * 90.0, 126.0, -sx * 40.0)
        ylathe(r, LIGHT, [(0.0, c[1] - 1.5), (24.0, c[1] - 1.5), (24.0, c[1] + 1.0), (0.0, c[1] + 1.0)], seg, t=(c[0], 0, c[2]))
        box(r, STEEL, (sx * 74.0, 120.0, -sx * 40.0), (24.0, 3.0, 4.0))
        if lod < 2:
            for k in range(12):
                a = 2 * math.pi * k / 12
                box(r, FLOOD, (c[0] + 22.0 * math.cos(a), c[1] + 1.2, c[2] + 22.0 * math.sin(a)), (1.0, 0.4, 1.0))
            box(r, HAZ, (c[0], c[1] + 1.1, c[2]), (12.0, 0.2, 2.0))
            box(r, HAZ, (c[0], c[1] + 1.1, c[2]), (2.0, 0.2, 12.0))
    if lod < 2:
        for k in range(8):
            a = 2 * math.pi * k / 8
            box(r, FLOOD, (64.0 * math.cos(a), 128.5, 64.0 * math.sin(a)), (2.0, 1.0, 2.0))
    return P


def rig_hits():
    return [hbox("hubv_rig", (0, 63.0, 0), (130.0, 126.0, 130.0)), hbox("hubv_rig", (0, 145.0, 0), (130.0, 50.0, 130.0)),
            hbox("hubv_rig", (0, 215.0, 0), (24.0, 130.0, 24.0))]


# ---------------------------------------------------------------- 4 lattice sphere in a portal gantry (rework 2026-11-17):
# the video's sphere (0:34, 3:05) is lumpy - a faceted core shingled with modules and wrapped in scaffolding, held in a
# heavy portal whose truss beam runs on past one leg as a crane jib.
def cage_gantry_parts(lod):
    P = {"hubv_cage_gantry": Part("hubv_cage_gantry")}
    g = P["hubv_cage_gantry"]
    rng = _random.Random(23)
    hi = lod < 2
    seg = _seg(lod, 24)
    box(g, STEEL, (0, 4.0, 0), (380.0, 8.0, 90.0))
    for sx in (-1, 1):                                                                    # the legs
        lattice_column(g, STEEL, 8.0, 300.0, 20.0, 16.0, 10 if hi else 4, 2.2, 1.0, cx=sx * 170.0)
    # the portal beam: a box truss 380 m + a jib 200 m past the left leg, tie rods from a king post
    for y in (292.0, 316.0):
        for z in (-12.0, 12.0):
            box(g, STEEL, (-100.0, y, z), (580.0, 3.0, 3.0))
    if lod < 3:
        for k in range(30 if hi else 12):
            x0 = -390.0 + 580.0 * k / (30 if hi else 12)
            x1 = x0 + 580.0 / (30 if hi else 12)
            for z in (-12.0, 12.0):
                cyl(g, STEEL, (x0, 292.0, z), (x1, 316.0, z), 0.8, 4)
                cyl(g, STEEL, (x0, 292.0, z), (x0, 316.0, z), 0.6, 4)
    cyl(g, STEEL, (-170.0, 316.0, 0), (-170.0, 370.0, 0), 2.0, 6)                           # king post + ties
    cyl(g, STEEL, (-170.0, 370.0, 0), (-390.0, 316.0, 0), 0.6, 4)
    cyl(g, STEEL, (-170.0, 370.0, 0), (60.0, 316.0, 0), 0.6, 4)
    box(g, LIGHT, (-330.0, 300.0, 0), (24.0, 14.0, 30.0))                                  # the jib trolley
    cyl(g, STEEL, (-330.0, 292.0, 0), (-330.0, 200.0, 0), 0.5, 4)
    box(g, HAZ, (-330.0, 198.0, 0), (6.0, 4.0, 6.0))
    beacon(g, (-390.0, 318.0, 0), 2.0)
    # the sphere: a faceted core shingled with modules, wrapped in scaffold
    C, R = (0.0, 160.0, 0.0), 132.0
    sphere(g, HULL, C, R - 10.0, 10 if hi else 8)
    nm = 110 if hi else 30
    for k in range(nm):
        z = 2 * ((k + 0.5) / nm) - 1
        a = k * 2.39996
        n = (math.sqrt(1 - z * z) * math.cos(a), z, math.sqrt(1 - z * z) * math.sin(a))
        rr = R - 10.0 + rng.uniform(-2.0, 6.0)
        c = (C[0] + n[0] * rr, C[1] + n[1] * rr, C[2] + n[2] * rr)
        w = rng.uniform(16.0, 34.0)
        box(g, rng.choice((HULL, LIGHT, STEEL)), c, (w, w * rng.uniform(0.5, 1.0), rng.uniform(5.0, 14.0)), axis_to(n))
        if hi and rng.random() < 0.4:
            box(g, WARM if rng.random() < 0.8 else COOL, tuple(c[i] + n[i] * 7.5 for i in range(3)), (2.0, 2.0, 0.4), axis_to(n))
    if lod < 3:
        n = 10 if hi else 5
        for k in range(n):                                                            # scaffold meridians
            arc_tube(g, STEEL, C, R + 4.0, -math.pi / 2, math.pi / 2, math.pi * k / n, 1.0, 16 if hi else 8)
            arc_tube(g, STEEL, C, R + 4.0, math.pi / 2, 3 * math.pi / 2, math.pi * k / n, 1.0, 16 if hi else 8)
        for y in (-110.0, -88.0, -66.0, -44.0, -22.0, 0.0, 22.0, 44.0, 66.0, 88.0, 110.0):
            rr = math.sqrt((R + 4.0) ** 2 - y * y)
            ring_y(g, STEEL, (0, C[1] + y, 0), rr - 0.8, 1.6, 1.6, seg)
        if hi:                                                                        # straight scaffold poles on one side
            for k in range(14):
                x = -60.0 + 120.0 * k / 13
                h = math.sqrt(max(0.0, (R + 10.0) ** 2 - x * x))
                cyl(g, STEEL, (x, C[1] - h * 0.9, -R * 0.55), (x, C[1] + h * 0.9, -R * 0.55), 0.5, 4)
    for sx in (-1, 1):                                                                # the sphere's holders on the legs
        cyl(g, STEEL, (sx * 152.0, C[1], 0), (sx * (R - 6.0), C[1], 0), 4.0, 8)
        cyl(g, STEEL, (sx * 150.0, C[1] + 70.0, 0), (sx * (R - 30.0), C[1] + 50.0, 0), 1.6, 6)
    for sx in (-1, 1):
        beacon(g, (sx * 170.0, 318.0, 0), 2.0)
    return P


def cage_gantry_hits():
    return [hbox("hubv_cage_gantry", (0, 4.0, 0), (380.0, 8.0, 90.0)), hbox("hubv_cage_gantry", (-100.0, 304.0, 0), (580.0, 28.0, 28.0))] + [
        hbox("hubv_cage_gantry", (sx * 170.0, 154.0, 0), (42.0, 292.0, 42.0)) for sx in (-1, 1)] + [
        hcyl("hubv_cage_gantry", (0, 28.0, 0), (0, 292.0, 0), 138.0)]


# ---------------------------------------------------------------- 5 lit ring wheel
def ring_wheel_parts(lod):
    P = {"hubv_ring_wheel": Part("hubv_ring_wheel")}
    w = P["hubv_ring_wheel"]
    seg = _seg(lod, 40)
    C, R = (0.0, 90.0, 0.0), 70.0
    box(w, STEEL, (0, 4.0, 0), (60.0, 8.0, 24.0))
    box(w, HULL, (0, 12.0, 0), (24.0, 16.0, 12.0))
    lathe(w, STEEL, [(R - 3.0, -2.0), (R + 3.0, -2.0), (R + 3.0, 2.0), (R - 3.0, 2.0), (R - 3.0, -2.0)], seg, t=C)   # the rim
    lathe(w, STEEL, [(R * 0.55 - 1.5, -1.5), (R * 0.55 + 1.5, -1.5), (R * 0.55 + 1.5, 1.5), (R * 0.55 - 1.5, 1.5),
                     (R * 0.55 - 1.5, -1.5)], seg, t=C)
    cyl(w, HULL, (C[0], C[1], -5.0), (C[0], C[1], 5.0), 9.0, 12)                                  # the hub
    for k in range(8):                                                                            # spokes
        a = 2 * math.pi * k / 8
        cyl(w, STEEL, (C[0] + 9.0 * math.cos(a), C[1] + 9.0 * math.sin(a), 0), (C[0] + (R - 3) * math.cos(a), C[1] + (R - 3) * math.sin(a), 0), 1.0, 6)
    cyl(w, STEEL, (0, 20.0, 0), (0, C[1] - R + 3.0, 0), 2.4, 8)                                    # the stem
    if lod < 3:
        n = 72 if lod < 2 else 24
        for k in range(n):                                                                        # the dotted lights
            a = 2 * math.pi * k / n
            box(w, WARM, (C[0] + (R + 3.5) * math.cos(a), C[1] + (R + 3.5) * math.sin(a), 0), (1.6, 1.6, 1.6))
            if lod < 2 and k % 2 == 0:
                box(w, WARM, (C[0] + R * 0.55 * math.cos(a), C[1] + R * 0.55 * math.sin(a), 2.0), (1.2, 1.2, 1.2))
    return P


def ring_wheel_hits():
    return [hbox("hubv_ring_wheel", (0, 90.0, 0), (150.0, 150.0, 10.0)), hbox("hubv_ring_wheel", (0, 10.0, 0), (60.0, 20.0, 24.0))]


# ---------------------------------------------------------------- 6 hyperboloid water tower
def water_tower_parts(lod):
    P = {"hubv_water_tower": Part("hubv_water_tower")}
    t = P["hubv_water_tower"]
    seg = _seg(lod, 24)
    n = 16 if lod < 2 else 8
    r0, r1, H, tw = 42.0, 24.0, 150.0, math.radians(70.0)
    ring_y(t, STEEL, (0, 3.0, 0), r0 - 3.0, 6.0, 6.0, seg)
    for k in range(n):                                                    # straight members, twisted: a hyperboloid
        a = 2 * math.pi * k / n
        for sgn in (1, -1):
            cyl(t, STEEL, (r0 * math.cos(a), 0.0, r0 * math.sin(a)),
                (r1 * math.cos(a + sgn * tw), H, r1 * math.sin(a + sgn * tw)), 0.8, 4)
    for y in (40.0, 80.0, 120.0):
        rr = r0 + (r1 - r0) * y / H - 4.0
        ring_y(t, STEEL, (0, y, 0), rr, 1.6, 1.6, seg)
    ylathe(t, HULL, [(0.0, H - 4.0), (r1, H - 4.0), (46.0, H + 22.0), (48.0, H + 30.0), (0.0, H + 32.0)], seg, soft=20)  # the bowl
    if lod < 2:
        for k in range(24):
            a = 2 * math.pi * k / 24
            box(t, WARM if k % 4 else COOL, (40.5 * math.cos(a), H + 16.0, 40.5 * math.sin(a)), (3.0, 3.0, 3.0))
        ring_y(t, GRATE, (0, H + 30.5, 0), 46.0, 4.0, 1.0, seg)
    cyl(t, STEEL, (0, H + 32.0, 0), (0, H + 70.0, 0), 1.0, 6)
    beacon(t, (0, H + 71.0, 0), 1.5)
    return P


def water_tower_hits():
    return [hcyl("hubv_water_tower", (0, 0.0, 0), (0, 150.0, 0), 42.0), hcyl("hubv_water_tower", (0, 146.0, 0), (0, 182.0, 0), 48.0)]


# ---------------------------------------------------------------- 7 gallery tower (rework 2026-11-17, author: «давай,
# продолжай с башней и сферой»): the video's tower (0:23, 1:01, 5:01) is an onion pod of stacked decks widening upward
# to an overhanging lip, bands of lit windows under every deck, machinery between the tiers, all threaded on a heavy
# lattice mast that rises on above the pod with antennas; a smaller pod lower down.
def gallery_tower_parts(lod):
    P = {"hubv_gallery_tower": Part("hubv_gallery_tower")}
    t = P["hubv_gallery_tower"]
    rng = _random.Random(11)
    hi = lod < 2
    seg = _seg(lod, 32)
    box(t, STEEL, (0, 4.0, 0), (90.0, 8.0, 90.0))
    lattice_column(t, STEEL, 8.0, 480.0, 22.0, 15.0, 12 if hi else 5, 2.2, 1.0)            # the mast through the pods
    cyl(t, HULL, (0, 8.0, 0), (0, 470.0, 0), 9.0, 8)

    def pod(yb, tiers, lip):
        """tiers: [(radius, height)] from the bottom up; a cone under them, a lip deck + roof plant on top"""
        r0 = tiers[0][0]
        ylathe(t, HULL, [(0.0, yb - 16.0), (r0 * 0.55, yb - 16.0), (r0 * 0.85, yb - 8.0), (r0, yb)], seg, soft=40)   # rounded bottom
        y = yb
        for i, (r, h) in enumerate(tiers):
            ylathe(t, FACADE, [(0.0, y), (r, y), (r, y + h), (0.0, y + h)], seg)
            ring_y(t, HULL, (0, y + h + 1.0, 0), r - 1.0, 5.0, 2.0, seg)                    # the deck slab (overhang)
            if hi:
                n = int(2 * math.pi * r / 7.0)
                for k in range(n):                                                     # the lit band under the slab
                    if rng.random() < 0.55:
                        a = 2 * math.pi * k / n
                        box(t, WARM if rng.random() < 0.85 else COOL, ((r + 0.2) * math.cos(a), y + h - 2.5, (r + 0.2) * math.sin(a)),
                            (1.4, 1.6, 2.6), rot_y(-a))
                for k in range(10):                                                    # machinery on the deck edge
                    a = rng.uniform(0, 2 * math.pi)
                    box(t, rng.choice((STEEL, LIGHT)), ((r + 1.5) * math.cos(a), y + h + 4.0, (r + 1.5) * math.sin(a)),
                        (rng.uniform(3, 7), rng.uniform(3, 6), rng.uniform(3, 6)), rot_y(-a))
                ring_y(t, GRATE, (0, y + h + 3.2, 0), r + 3.6, 0.5, 1.2, seg)          # rail
                for k in range(16):                                                    # posts between the tiers
                    a = 2 * math.pi * (k + 0.5) / 16
                    box(t, STEEL, ((r + 2.0) * math.cos(a), y + h / 2, (r + 2.0) * math.sin(a)), (1.4, h, 1.4))
            y += h + 2.0
        ylathe(t, HULL, [(0.0, y), (lip, y), (lip, y + 6.0), (lip - 12.0, y + 10.0), (0.0, y + 10.0)], seg)   # the lip deck
        if hi:
            n = int(2 * math.pi * lip / 6.0)
            for k in range(n):
                a = 2 * math.pi * k / n
                box(t, FLOOD if k % 3 == 0 else WARM, ((lip + 0.2) * math.cos(a), y + 3.0, (lip + 0.2) * math.sin(a)), (1.2, 1.2, 1.2))
        for k in range(8 if hi else 4):                                                # roof plant + antennas
            a = 2 * math.pi * k / (8 if hi else 4) + 0.2
            rr = lip * rng.uniform(0.45, 0.8)
            box(t, LIGHT, (rr * math.cos(a), y + 15.0, rr * math.sin(a)), (rng.uniform(6, 12), 10.0, rng.uniform(6, 10)), rot_y(-a))
            cyl(t, STEEL, ((lip - 4.0) * math.cos(a), y + 10.0, (lip - 4.0) * math.sin(a)),
                ((lip - 6.0) * math.cos(a), y + 10.0 + rng.uniform(20.0, 45.0), (lip - 6.0) * math.sin(a)), 0.7, 4)
        return y + 10.0

    top = pod(290.0, [(60.0, 14.0), (68.0, 16.0), (72.0, 16.0), (74.0, 16.0), (72.0, 16.0), (68.0, 12.0)], 84.0)   # a barrel
    pod(150.0, [(30.0, 10.0), (34.0, 10.0), (34.0, 10.0)], 42.0)                          # the smaller pod lower down
    # the mast frame above the pod: four posts, cross beams, beacons
    for sx in (-1, 1):
        for sz in (-1, 1):
            cyl(t, STEEL, (sx * 16.0, top, sz * 16.0), (sx * 10.0, top + 120.0, sz * 10.0), 1.6, 6)
            beacon(t, (sx * 10.0, top + 121.0, sz * 10.0), 1.4)
    for y in (top + 40.0, top + 80.0, top + 118.0):
        ring_y(t, STEEL, (0, y, 0), 12.0, 2.0, 1.5, 8)
    cyl(t, STEEL, (0, top + 118.0, 0), (0, top + 190.0, 0), 1.2, 6)
    beacon(t, (0, top + 191.0, 0), 2.0)
    return P


def gallery_tower_hits():
    return [hbox("hubv_gallery_tower", (0, 240.0, 0), (44.0, 480.0, 44.0)), hcyl("hubv_gallery_tower", (0, 270.0, 0), (0, 400.0, 0), 86.0),
            hcyl("hubv_gallery_tower", (0, 120.0, 0), (0, 190.0, 0), 44.0)]


# ---------------------------------------------------------------- 8 lattice mast
def mast_parts(lod):
    P = {"hubv_mast": Part("hubv_mast")}
    m = P["hubv_mast"]
    box(m, STEEL, (0, 3.0, 0), (40.0, 6.0, 40.0))
    bands = 8
    for k in range(bands):
        y0, y1 = 6.0 + k * 55.0, 6.0 + (k + 1) * 55.0
        h0, h1 = 14.0 - 9.0 * k / bands, 14.0 - 9.0 * (k + 1) / bands
        lattice_column(m, RED if k % 2 == 0 else LIGHT, y0, y1, h0, h1, 3 if lod < 2 else 1, 0.9, 0.45, 6 if lod < 2 else 4)
        if lod < 3:
            for sx in (-1, 1):
                beacon(m, (sx * (h1 + 0.8), y1, 0), 1.0)
    beacon(m, (0, 6.0 + bands * 55.0 + 2.0, 0), 2.0)
    return P


def mast_hits():
    return [hbox("hubv_mast", (0, 225.0, 0), (28.0, 440.0, 28.0))]


# ---------------------------------------------------------------- 9 curved cantilever boom
def cantilever_parts(lod):
    P = {"hubv_cantilever": Part("hubv_cantilever")}
    c = P["hubv_cantilever"]
    L, sag, H0, H1 = 500.0, 70.0, 40.0, 8.0
    n = 20 if lod < 2 else 8
    box(c, HULL, (0, 30.0, 20.0), (80.0, 60.0, 40.0))                                         # the root block
    if lod < 2:
        for y in range(10, 60, 10):
            box(c, WARM, (0, float(y), 40.1), (60.0, 1.5, 0.2))
    def at(t, top, side):
        z = 40.0 + L * t
        y = 50.0 - sag * t * t + (H0 + (H1 - H0) * t) * (0.5 if top else -0.5)
        x = side * (14.0 - 8.0 * t)
        return (x, y, z)
    for top in (True, False):
        for side in (-1, 1):
            tube(c, STEEL, [at(k / n, top, side) for k in range(n + 1)], 1.6 if not top else 1.2, 6)
    for k in range(n + 1):
        t = k / n
        for side in (-1, 1):
            cyl(c, STEEL, at(t, True, side), at(t, False, side), 0.7, 4)
            if k < n:
                cyl(c, STEEL, at(t, True, side), at((k + 1) / n, False, side), 0.5, 4)
        cyl(c, STEEL, at(t, True, -1), at(t, True, 1), 0.5, 4)
        cyl(c, STEEL, at(t, False, -1), at(t, False, 1), 0.5, 4)
        if lod < 3 and k % 2 == 0:
            box(c, WARM, at(t, False, 1), (1.4, 1.4, 1.4))
    tip = at(1.0, False, 0)
    if lod < 3:
        for dx in (-2.0, 0.0, 2.0):
            box(c, FLOOD, (tip[0] + dx, tip[1] - 3.0, tip[2]), (1.5, 1.5, 1.5))
    beacon(c, at(1.0, True, 0), 1.6)
    return P


def cantilever_hits():
    return [hbox("hubv_cantilever", (0, 30.0, 20.0), (80.0, 60.0, 40.0)), hbox("hubv_cantilever", (0, 15.0, 290.0), (30.0, 90.0, 500.0))]


# ---------------------------------------------------------------- 10 refinery
REFINERY_FLARE = (60.0, 182.0, -50.0)


def refinery_parts(lod):
    P = {"hubv_refinery": Part("hubv_refinery")}
    f = P["hubv_refinery"]
    seg = _seg(lod, 16)
    box(f, STEEL, (0, 4.0, 0), (220.0, 8.0, 160.0))
    cols = [(-70.0, -40.0, 10.0, 140.0), (-40.0, -40.0, 7.0, 110.0), (-70.0, 20.0, 8.0, 120.0), (-35.0, 25.0, 12.0, 90.0),
            (0.0, -45.0, 6.0, 80.0), (20.0, 30.0, 14.0, 60.0)]
    for x, z, r, h in cols:
        cyl(f, LIGHT, (x, 8.0, z), (x, 8.0 + h, z), r, seg)
        if lod < 2:
            for y in range(30, int(h), 25):
                ring_y(f, GRATE, (x, 8.0 + y, z), r, 2.5, 0.6, seg)
                for k in range(4):
                    a = math.pi / 2 * k + 0.4
                    box(f, WARM, (x + (r + 2.3) * math.cos(a), 9.5 + y, z + (r + 2.3) * math.sin(a)), (0.8, 0.8, 0.8))
        if lod < 3:
            beacon(f, (x, 9.0 + h, z), 1.0)
    if lod < 2:                                                                     # pipe racks
        for y in (20.0, 26.0):
            for zz in (-10.0, -4.0, 2.0):
                cyl(f, STEEL, (-100.0, y, zz), (100.0, y, zz), 1.0, 6)
        for x in range(-90, 100, 20):
            box(f, STEEL, (float(x), 16.0, -4.0), (1.5, 16.0, 16.0))
    box(f, HULL, (60.0, 20.0, 40.0), (60.0, 24.0, 50.0))                            # the control / furnace house
    if lod < 2:
        for x in range(36, 86, 8):
            box(f, FLARE, (float(x), 14.0, 65.05), (3.0, 3.0, 0.2))                 # furnace glow in its windows
    # the flare stack: a thin tower, an open flame on top
    fx, fy, fz = REFINERY_FLARE
    lattice_column(f, STEEL, 8.0, fy - 12.0, 5.0, 2.0, 6 if lod < 2 else 3, 0.6, 0.3, 4, cx=fx, cz=fz)
    for k in range(3):
        box(f, FLARE, (fx, fy - 8.0 + k * 7.0, fz), (6.0 - k * 1.6, 7.0, 6.0 - k * 1.6))
    return P


def refinery_hits():
    return [hbox("hubv_refinery", (0, 4.0, 0), (220.0, 8.0, 160.0)), hbox("hubv_refinery", (-30.0, 80.0, -10.0), (110.0, 150.0, 100.0)),
            hcyl("hubv_refinery", (60.0, 8.0, -50.0), (60.0, 190.0, -50.0), 6.0)]


# ---------------------------------------------------------------- 11 star pad
def pad_star_parts(lod):
    P = {"hubv_pad_star": Part("hubv_pad_star")}
    p = P["hubv_pad_star"]
    seg = 6
    R = 80.0
    ylathe(p, HULL, [(0.0, -6.0), (R, -6.0), (R, 0.0), (0.0, 0.0)], seg)                         # a hexagonal deck
    ylathe(p, LIGHT, [(0.0, 0.0), (R * 0.45, 0.0), (R * 0.45, 0.2), (0.0, 0.2)], 24 if lod < 2 else 12)
    if lod < 3:
        n = 24 if lod < 2 else 12
        for k in range(n):                                                                        # the starburst lines
            a = 2 * math.pi * k / n
            for j in range(4 if k % 2 == 0 else 2):
                r = R * (0.5 + 0.12 * j)
                box(p, FLOOD, (r * math.cos(a), 0.6, r * math.sin(a)), (1.4, 0.6, 1.4))
        for k in range(36 if lod < 2 else 12):                                                    # the inner ring
            a = 2 * math.pi * k / (36 if lod < 2 else 12)
            box(p, FLOOD, (R * 0.42 * math.cos(a), 0.5, R * 0.42 * math.sin(a)), (1.0, 0.4, 1.0))
    for k in range(6):
        a = math.pi / 3 * k
        beacon(p, (R * 0.9 * math.cos(a), 1.5, R * 0.9 * math.sin(a)), 1.4)
    return P


def pad_star_hits():
    return [hcyl("hubv_pad_star", (0, -6.0, 0), (0, 0.0, 0), 80.0)]


# ---------------------------------------------------------------- 12 the disc canopy ("диск-потолок", video 0:03 / 4:02)
# A lattice trumpet rises out of the district and flares into a disc 1.4 km across: its underside coffered (radial ribs,
# rings, lit panels) with towers hanging from it, a quarter of skyscrapers on its top, lights round the rim.
CANOPY_R, CANOPY_Y = 700.0, 600.0


def canopy_parts(lod):
    P = {"hubv_canopy": Part("hubv_canopy")}
    c = P["hubv_canopy"]
    rng = _random.Random(7)
    hi = lod < 2
    seg = _seg(lod, 48)
    R, Y = CANOPY_R, CANOPY_Y
    box(c, STEEL, (0, 5.0, 0), (130.0, 10.0, 130.0))
    # the solid trumpet core
    ylathe(c, HULL, [(0.0, 10.0), (42.0, 10.0), (26.0, 260.0), (34.0, 380.0), (70.0, 470.0), (150.0, 545.0),
                     (250.0, 590.0), (262.0, Y), (0.0, Y)], seg, soft=35)
    # the lattice skin of the trumpet: curved, twisted members + stiffening rings
    rad = lambda y: 52.0 - 22.0 * (y / 260.0) if y < 260.0 else 30.0 + 240.0 * ((y - 260.0) / 340.0) ** 2.2
    nm = 32 if hi else 12
    for k in range(nm):
        for sgn in ((1, -1) if hi else (1,)):
            pts = []
            for j in range(0, 21):
                y = 10.0 + (Y - 14.0) * j / 20.0
                a = 2 * math.pi * k / nm + sgn * 0.9 * (y / Y)
                rr = rad(y) + 4.0
                pts.append((rr * math.cos(a), y, rr * math.sin(a)))
            tube(c, STEEL, pts, 1.6 if hi else 2.2, 4)
    if lod < 3:
        for y in (60.0, 140.0, 220.0, 300.0, 380.0, 450.0, 510.0, 560.0):
            ring_y(c, STEEL, (0, y, 0), rad(y) + 3.0, 3.0, 3.0, seg)
            if hi:
                for k in range(12):
                    a = 2 * math.pi * k / 12 + y
                    box(c, WARM, ((rad(y) + 6.5) * math.cos(a), y + 2.0, (rad(y) + 6.5) * math.sin(a)), (2.0, 2.0, 2.0))
    # the disc
    ylathe(c, HULL, [(0.0, Y), (R - 40.0, Y), (R, Y + 18.0), (R, Y + 40.0), (R - 30.0, Y + 50.0), (0.0, Y + 50.0)], seg, soft=20)
    # its underside: radial ribs, rings, coffer panels (some lit), hanging towers
    nr = 48 if hi else 16
    for k in range(nr):
        a = 2 * math.pi * k / nr
        mid = (R + 262.0) / 2
        box(c, STEEL, (mid * math.cos(a), Y - 5.0, mid * math.sin(a)), (R - 262.0, 10.0, 4.0), rot_y(-a))
    if lod < 3:
        for rr in (330.0, 420.0, 510.0, 600.0, 680.0):
            ring_y(c, STEEL, (0, Y - 5.0, 0), rr, 5.0, 10.0, seg)
    if hi:
        bands = [(262.0, 330.0), (335.0, 420.0), (425.0, 510.0), (515.0, 600.0), (605.0, 680.0)]
        for k in range(nr):
            for r0, r1 in bands:
                if rng.random() < 0.35:
                    a = 2 * math.pi * (k + 0.5) / nr
                    rm = (r0 + r1) / 2
                    w = 2 * math.pi * rm / nr * 0.7
                    box(c, WARM if rng.random() < 0.8 else COOL, (rm * math.cos(a), Y - 0.6, rm * math.sin(a)),
                        ((r1 - r0) * 0.75, 0.4, w), rot_y(-a))
    nt = 20 if hi else 8
    for k in range(nt):                                                         # towers hanging under the disc
        a = rng.uniform(0, 2 * math.pi)
        rr = rng.uniform(330.0, 640.0)
        h = rng.uniform(50.0, 170.0)
        w = rng.uniform(26.0, 44.0)
        box(c, FACADE, (rr * math.cos(a), Y - 10.0 - h / 2, rr * math.sin(a)), (w, h, w * rng.uniform(0.7, 1.2)), rot_y(-a))
        if lod < 3:
            beacon(c, (rr * math.cos(a), Y - 12.0 - h, rr * math.sin(a)), 1.6)
    # the quarter on top
    nb = 46 if hi else 18
    for k in range(nb):
        a = rng.uniform(0, 2 * math.pi)
        rr = rng.uniform(40.0, 600.0) ** 1.0
        h = rng.uniform(30.0, 120.0) * (1.6 - rr / R)
        w = rng.uniform(30.0, 60.0)
        box(c, FACADE if k % 4 else FACADE_L, (rr * math.cos(a), Y + 50.0 + h / 2, rr * math.sin(a)), (w, h, w * rng.uniform(0.6, 1.3)),
            rot_y(-a))
    cyl(c, STEEL, (0, Y + 50.0, 0), (0, Y + 330.0, 0), 3.0, 8)                       # the mast on the axis
    beacon(c, (0, Y + 332.0, 0), 3.0)
    if lod < 3:
        n = 96 if hi else 24
        for k in range(n):                                                      # rim lights
            a = 2 * math.pi * k / n
            box(c, FLOOD if k % 2 else WARM, ((R + 0.5) * math.cos(a), Y + 30.0, (R + 0.5) * math.sin(a)), (3.0, 3.0, 3.0))
        for k in range(12):
            a = 2 * math.pi * k / 12
            beacon(c, ((R - 6.0) * math.cos(a), Y + 46.0, (R - 6.0) * math.sin(a)), 3.0)
    return P


def canopy_hits():
    return [hcyl("hubv_canopy", (0, 10.0, 0), (0, 470.0, 0), 70.0), hcyl("hubv_canopy", (0, 470.0, 0), (0, CANOPY_Y, 0), 262.0),
            hcyl("hubv_canopy", (0, CANOPY_Y - 180.0, 0), (0, CANOPY_Y + 50.0, 0), CANOPY_R)]


KIT = {"hubv_crown_spire": (crown_spire_parts, crown_spire_hits, "Шпиль с короной 425 м (по ролику, 4:39:42)"),
       "hubv_crown_spire_b": (crown_spire_builder("hubv_crown_spire_b", 340.0, 57), crown_spire_hits_for("hubv_crown_spire_b", 340.0),
                              "Шпиль с короной, высокий 515 м"),
       "hubv_crown_spire_c": (crown_spire_builder("hubv_crown_spire_c", 180.0, 83), crown_spire_hits_for("hubv_crown_spire_c", 180.0),
                              "Шпиль с короной, низкий 355 м"),
       "hubv_saucer_tower": (saucer_tower_parts, saucer_tower_hits, "Башня-гриб со светящимся диском 200 м"),
       "hubv_rig": (rig_parts, rig_hits, "Буровая платформа-пирамида"),
       "hubv_cage_gantry": (cage_gantry_parts, cage_gantry_hits, "Решётчатая сфера в портальном кране"),
       "hubv_ring_wheel": (ring_wheel_parts, ring_wheel_hits, "Кольцо-колесо с огнями 150 м"),
       "hubv_water_tower": (water_tower_parts, water_tower_hits, "Решётчатая водонапорная башня (гиперболоид)"),
       "hubv_gallery_tower": (gallery_tower_parts, gallery_tower_hits, "Башня-капсула с ярусами-галереями на мачте 650 м"),
       "hubv_mast": (mast_parts, mast_hits, "Решётчатая мачта 450 м"),
       "hubv_cantilever": (cantilever_parts, cantilever_hits, "Консольная ферма-стрела 500 м"),
       "hubv_refinery": (refinery_parts, refinery_hits, "НПЗ: колонны, эстакады, факел"),
       "hubv_pad_star": (pad_star_parts, pad_star_hits, "Площадка-звезда с огнями 160 м"),
       "hubv_canopy": (canopy_parts, canopy_hits, "Диск-потолок 1,4 км на раструбе 600 м (по ролику, 0:03)")}

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Ориентир поселения по референс-ролику (tools/models/units/hub_vista.py).")
             for k, v in KIT.items()}
