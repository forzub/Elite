"""Hub district prototype (author 2026-11-16: «мы сможем сделать один такой огромный хаб - несколько десятков километров.
такой разросшийся город в метеоритном поясе? ... начинай прототип района хаба» + «сразу добавляй макеты в модули хаба для
эдитора» + «и где-то должна быть собственно - сборка хаба»).

A kit of big hub modules (the editor's hub modules, <assets_source>/models/hub/<id>) and the district assembled from
them by reference (hub_proto_district, kind "assembly"): an industrial city grown on a belt asteroid - a truss spine runs
out of the rock, branches grow in every direction (there is no "up" in free fall), habitat blocks, towers and
"mushroom" platforms sit on the branch ends, the far end is the industrial quarter (factories with vent stacks - the
local haze sources - and tank farms). Sizes in metres; every module's attachment point is its origin, its "out" axis +Y
(the truss runs along +Z). Windows and beacons are emissive materials (tools/materials/seed_material_library.py
EMISSIVE) - the thousands of lights that sell the scale.
"""
import math
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel

register_materials({
    "eq_hub_hull": ((0.30, 0.31, 0.33), (0.15, 0.15, 0.16), 30),
    "eq_hub_hull_light": ((0.50, 0.51, 0.52), (0.18, 0.18, 0.18), 30),
    "eq_hub_steel": ((0.17, 0.18, 0.19), (0.35, 0.35, 0.36), 70),
    "eq_hub_rock": ((0.26, 0.24, 0.22), (0.04, 0.04, 0.04), 8),
    "eq_hub_window_warm": ((1.00, 0.74, 0.42), (0.10, 0.10, 0.10), 20),
    "eq_hub_window_cool": ((0.60, 0.78, 1.00), (0.10, 0.10, 0.10), 20),
    "eq_hub_window_dark": ((0.05, 0.06, 0.07), (0.60, 0.60, 0.65), 120),
    "eq_hub_beacon": ((1.00, 0.12, 0.06), (0.10, 0.10, 0.10), 20),
    "eq_hub_flood": ((0.85, 0.92, 1.00), (0.10, 0.10, 0.10), 20),
    "eq_hub_hazard": ((0.78, 0.58, 0.10), (0.20, 0.20, 0.15), 30),
})
HULL, LIGHT, STEEL, ROCK = "eq_hub_hull", "eq_hub_hull_light", "eq_hub_steel", "eq_hub_rock"
WARM, COOL, DARK, BEACON, FLOOD, HAZ = ("eq_hub_window_warm", "eq_hub_window_cool", "eq_hub_window_dark", "eq_hub_beacon",
                                       "eq_hub_flood", "eq_hub_hazard")


def quad(part, mat, c, u, v, n):
    """a flat rectangle centred at c spanned by half-vectors u, v, facing n"""
    pts = [tuple(c[i] - u[i] - v[i] for i in range(3)), tuple(c[i] + u[i] - v[i] for i in range(3)),
           tuple(c[i] + u[i] + v[i] for i in range(3)), tuple(c[i] - u[i] + v[i] for i in range(3))]
    part.face(mat, [part.vert(p, n) for p in pts], n)


def beacon(part, p, s=1.0):
    box(part, BEACON, p, (s, s, s))


# ---------------------------------------------------------------- truss: the district's skeleton (100 m along +Z)
def truss_parts(lod):
    P = {"hubk_truss": Part("hubk_truss")}
    t = P["hubk_truss"]
    h, L = 4.0, 100.0
    bays = {0: 10, 1: 5, 2: 2}[lod]
    cs = {0: 8, 1: 6, 2: 4}[lod]
    for sx in (-1, 1):
        for sy in (-1, 1):
            cyl(t, STEEL, (sx * h, sy * h, 0), (sx * h, sy * h, L), 0.45, cs)
    for k in range(bays + 1):
        z = L * k / bays
        for a, b in (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h))):
            cyl(t, STEEL, (a[0], a[1], z), (b[0], b[1], z), 0.3, 6 if lod < 2 else 4)
        if k < bays and lod < 2:
            z1 = L * (k + 1) / bays
            for a, b in (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h))):
                cyl(t, STEEL, (a[0], a[1], z), (b[0], b[1], z1), 0.22, 6 if lod == 0 else 4)
    if lod < 2:
        box(t, HULL, (0, -h + 0.4, L / 2), (2 * h - 1.0, 0.4, L - 2.0))                             # service walkway
        for z in (2.0, L - 2.0):
            beacon(t, (h + 0.6, h + 0.6, z), 0.8)
    if lod == 0:
        for z in range(5, 100, 10):
            box(t, FLOOD, (0, -h + 0.8, z), (0.6, 0.2, 0.6))                                          # walkway lamps
    return P


def truss_hits():
    return [hbox("hubk_truss", (0, 0, 50.0), (9.0, 9.0, 100.0))]


# ---------------------------------------------------------------- habitat blocks with window grids
def hab_builder(w, hgt, d, seed, roof=True):
    def fn(lod):
        rng = random.Random(seed)
        P = {}
        b = P.setdefault("hubk_hab", Part("hubk_hab"))
        box(b, HULL, (0, hgt / 2, 0), (w, hgt, d), chamfer=0.8 if lod == 0 else 0.0)
        box(b, LIGHT, (0, 1.5, 0), (w + 1.0, 3.0, d + 1.0))                                           # plinth / clamp
        floors = int((hgt - 6) / 3.6)
        for face, (n, u_ax, width) in enumerate((((0, 0, 1), (1, 0, 0), w), ((0, 0, -1), (-1, 0, 0), w),
                                                 ((1, 0, 0), (0, 0, -1), d), ((-1, 0, 0), (0, 0, 1), d))):
            cols = int(width / 3.0)
            off = (n[0] * (w / 2 + 0.06), 0, n[2] * (d / 2 + 0.06))
            for f in range(floors):
                y = 5.0 + f * 3.6
                row = [rng.random() for _ in range(cols)]
                lit = [WARM if r < 0.33 else COOL if r < 0.42 else DARK for r in row]
                if lod == 0:
                    for c, m in enumerate(lit):
                        x = -width / 2 + (c + 0.5) * width / cols
                        cc = (off[0] + u_ax[0] * x, y, off[2] + u_ax[2] * x)
                        quad(b, m, cc, tuple(a * 0.9 for a in u_ax), (0, 0.65, 0), n)
                else:                       # runs of lit windows merged into one strip (dark ones are just hull)
                    c = 0
                    while c < cols:
                        if lit[c] == DARK or (lod == 2 and f % 2):
                            c += 1
                            continue
                        e = c
                        while e + 1 < cols and lit[e + 1] == lit[c]:
                            e += 1
                        x0 = -width / 2 + c * width / cols + 0.15
                        x1 = -width / 2 + (e + 1) * width / cols - 0.15
                        xm, hw = (x0 + x1) / 2, (x1 - x0) / 2
                        cc = (off[0] + u_ax[0] * xm, y, off[2] + u_ax[2] * xm)
                        quad(b, lit[c], cc, tuple(a * hw for a in u_ax), (0, 0.65 if lod == 1 else 1.3, 0), n)
                        c = e + 1
        if roof and lod < 2:
            for k in range(3):                                                                        # roof plant
                box(b, LIGHT, (rng.uniform(-w / 3, w / 3), hgt + 2.0, rng.uniform(-d / 3, d / 3)), (6.0, 4.0, 5.0))
            cyl(b, STEEL, (w / 3, hgt, -d / 3), (w / 3, hgt + 18.0, -d / 3), 0.4, 6)                    # antenna mast
            beacon(b, (w / 3, hgt + 18.5, -d / 3), 1.0)
        return P
    return fn


def hab_hits(w, hgt, d):
    return lambda: [hbox("hubk_hab", (0, hgt / 2, 0), (w + 1.0, hgt, d + 1.0))]


# ---------------------------------------------------------------- lattice tower (160 m) with platforms, beacons, floods
def tower_parts(lod):
    P = {"hubk_tower": Part("hubk_tower")}
    t = P["hubk_tower"]
    H = 160.0
    r = lambda y: 6.0 - 3.0 * y / H
    cs = {0: 8, 1: 6, 2: 4}[lod]
    for sx in (-1, 1):
        for sz in (-1, 1):
            cyl(t, STEEL, (sx * r(0), 0, sz * r(0)), (sx * r(H), H, sz * r(H)), 0.5, cs)
    step = {0: 10.0, 1: 20.0, 2: 40.0}[lod]
    y = 0.0
    while y <= H + 1e-6:
        c = r(y)
        for a, b in ((((-c, -c), (c, -c))), (((c, -c), (c, c))), (((c, c), (-c, c))), (((-c, c), (-c, -c)))):
            cyl(t, STEEL, (a[0], y, a[1]), (b[0], y, b[1]), 0.3, 4)
        if lod == 0 and y + step <= H:
            c1 = r(y + step)
            for (a, b), (a1, b1) in zip((((-c, -c), (c, -c)), ((c, -c), (c, c)), ((c, c), (-c, c)), ((-c, c), (-c, -c))),
                                        (((-c1, -c1), (c1, -c1)), ((c1, -c1), (c1, c1)), ((c1, c1), (-c1, c1)), ((-c1, c1), (-c1, -c1)))):
                cyl(t, STEEL, (a[0], y, a[1]), (b1[0], y + step, b1[1]), 0.2, 4)
        y += step
    for py, s in ((60.0, 22.0), (115.0, 16.0)):
        box(t, HULL, (0, py, 0), (s, 2.0, s))
        if lod < 2:
            for sx in (-1, 1):
                for sz in (-1, 1):
                    beacon(t, (sx * s / 2, py + 1.5, sz * s / 2), 0.8)
            box(t, FLOOD, (s / 2 + 0.3, py - 1.2, 0), (0.6, 1.2, 3.0))                                # floodlight bank
            if lod == 0:
                box(t, WARM, (0, py + 2.2, s / 2 - 0.5), (s * 0.6, 1.4, 0.2))                         # control cabin window
    cyl(t, STEEL, (0, H, 0), (0, H + 25.0, 0), 0.6, cs)
    beacon(t, (0, H + 25.5, 0), 1.6)
    return P


def tower_hits():
    return [hbox("hubk_tower", (0, 80.0, 0), (12.0, 160.0, 12.0)), hbox("hubk_tower", (0, 60.0, 0), (22.0, 2.0, 22.0)),
            hbox("hubk_tower", (0, 115.0, 0), (16.0, 2.0, 16.0)), hcyl("hubk_tower", (0, 160.0, 0), (0, 186.0, 0), 1.0)]


# ---------------------------------------------------------------- "mushroom": a stem and a wide platform with a light ring
def mushroom_parts(lod):
    P = {"hubk_mushroom": Part("hubk_mushroom")}
    m = P["hubk_mushroom"]
    seg = {0: 32, 1: 20, 2: 12}[lod]
    ylathe(m, HULL, [(0, 0), (9.0, 0), (7.0, 6.0), (7.0, 70.0), (20.0, 84.0), (48.0, 86.0), (48.0, 94.0), (0, 94.0)], seg)
    if lod < 2:                                                                                   # window ring, lit / dark
        n = {0: 48, 1: 24}[lod]
        for k in range(n):
            a0, a1 = 2 * math.pi * k / n + 0.01, 2 * math.pi * (k + 1) / n - 0.01
            ylathe(m, WARM if k % 3 else COOL if k % 2 else DARK, crect(48.0, 48.15, 88.5, 91.0), 2, a0, a1, cap=True)
        for k in range(12):                                                                       # underside floods
            a = 2 * math.pi * k / 12
            box(m, FLOOD, (40.0 * math.cos(a), 85.3, 40.0 * math.sin(a)), (1.4, 0.4, 1.4))
        for x, z, s in ((-18.0, 6.0, 14.0), (14.0, -12.0, 18.0), (6.0, 20.0, 10.0)):                # rooftop blocks
            box(m, LIGHT, (x, 94.0 + s / 2, z), (s, s, s * 0.8))
            box(m, WARM, (x, 94.0 + s / 2, z + s * 0.4 + 0.05), (s * 0.8, 1.0, 0.1))
    else:
        ylathe(m, WARM, crect(48.0, 48.2, 88.5, 91.0), seg)
    beacon(m, (0, 94.5, 0), 1.5)
    return P


def mushroom_hits():
    return [hcyl("hubk_mushroom", (0, 0, 0), (0, 70.0, 0), 9.0), hcyl("hubk_mushroom", (0, 70.0, 0), (0, 86.0, 0), 20.0, 48.0),
            hcyl("hubk_mushroom", (0, 86.0, 0), (0, 112.0, 0), 48.0)]


# ---------------------------------------------------------------- factory: hall, tanks, pipe rack, two vent stacks
FACTORY_VENTS = [(-24.0, 62.0, -14.0), (-12.0, 62.0, -14.0)]       # stack tops: the haze sources of the district


def factory_parts(lod):
    P = {"hubk_factory": Part("hubk_factory")}
    f = P["hubk_factory"]
    seg = {0: 20, 1: 12, 2: 8}[lod]
    box(f, HULL, (0, 14.0, 0), (70.0, 28.0, 45.0), chamfer=0.6 if lod == 0 else 0.0)
    box(f, LIGHT, (28.0, 9.0, 26.0), (25.0, 18.0, 12.0))                                           # annex
    for x in (14.0, 24.0, 34.0):                                                                  # process tanks
        ylathe(f, LIGHT, [(0, 28.0), (6.0, 28.0), (6.0, 52.0), (4.5, 56.0), (0, 57.0)], seg, t=(x - 30.0, 0, 8.0))
    for (x, y, z) in FACTORY_VENTS:                                                               # vent stacks
        cyl(f, STEEL, (x, 28.0, z), (x, y, z), 2.2, seg)
        if lod < 2:
            cyl(f, HAZ, (x, y - 6.0, z), (x, y - 3.0, z), 2.35, seg)
            beacon(f, (x, y + 0.6, z), 1.0)
    if lod < 2:
        for y in (20.0, 23.0):                                                                    # pipe rack
            cyl(f, STEEL, (-35.0, y, 24.0), (35.0, y, 24.0), 0.9, 8 if lod == 0 else 6)
        for x in (-30.0, -10.0, 10.0, 30.0):
            box(f, STEEL, (x, 12.0, 24.0), (1.0, 24.0, 1.0))
        for x in range(-30, 31, 12):                                                              # hall window band
            box(f, WARM if x % 24 else COOL, (x, 20.0, 22.56), (8.0, 2.0, 0.1))
        for x in (-34.0, 34.0):
            box(f, FLOOD, (x, 28.5, 0), (1.0, 0.8, 6.0))
    if lod == 0:
        for x in range(-33, 34, 6):
            box(f, HAZ, (x, 1.0, 22.55), (3.0, 2.0, 0.1))                                             # hazard chevrons
    return P


def factory_hits():
    H = [hbox("hubk_factory", (0, 14.0, 0), (70.0, 28.0, 45.0)), hbox("hubk_factory", (28.0, 9.0, 26.0), (25.0, 18.0, 12.0))]
    for x in (14.0, 24.0, 34.0):
        H.append(hcyl("hubk_factory", (x - 30.0, 28.0, 8.0), (x - 30.0, 57.0, 8.0), 6.0))
    for (x, y, z) in FACTORY_VENTS:
        H.append(hcyl("hubk_factory", (x, 28.0, z), (x, y, z), 2.3))
    return H


# ---------------------------------------------------------------- tank farm: four spheres on a frame
def tanks_parts(lod):
    P = {"hubk_tanks": Part("hubk_tanks")}
    t = P["hubk_tanks"]
    seg = {0: 20, 1: 12, 2: 8}[lod]
    box(t, STEEL, (0, 2.0, 0), (50.0, 4.0, 30.0))
    for x in (-15.0, 15.0):
        for z in (-8.0, 8.0):
            ylathe(t, LIGHT, [(0, 4.0)] + [(9.0 * math.sin(math.pi * k / 8), 13.0 - 9.0 * math.cos(math.pi * k / 8)) for k in range(1, 8)]
                   + [(0, 22.0)], seg, t=(x, 0, z), soft=40)
            if lod < 2:
                beacon(t, (x, 22.4, z), 0.7)
    if lod < 2:
        cyl(t, STEEL, (-15.0, 13.0, 0), (15.0, 13.0, 0), 1.0, 8)
    return P


def tanks_hits():
    return [hbox("hubk_tanks", (0, 2.0, 0), (50.0, 4.0, 30.0))] + [
        hbox("hubk_tanks", (x, 13.0, z), (18.0, 18.0, 18.0)) for x in (-15.0, 15.0) for z in (-8.0, 8.0)]


# ---------------------------------------------------------------- the anchor asteroid (radius ~350 m)
def rock_parts(lod):
    P = {"hubk_rock": Part("hubk_rock")}
    r = P["hubk_rock"]
    rng = random.Random(11)
    bumps = [(norm((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))), rng.uniform(0.04, 0.12)) for _ in range(14)]
    def rad(v):
        return 350.0 * (1.0 + sum(h * max(0.0, sum(a * b for a, b in zip(v, d)) - 0.6) / 0.4 for d, h in bumps)
                        - 0.06 * math.sin(3 * v[0] + 2 * v[1]) * math.cos(2 * v[2]))
    # an icosphere, subdivided
    t = (1 + 5 ** 0.5) / 2
    V = [norm(p) for p in ((-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
                           (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1))]
    F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
         (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for _ in range({0: 4, 1: 3, 2: 2}[lod]):
        mid, F2 = {}, []
        def m(a, b):
            k = (min(a, b), max(a, b))
            if k not in mid:
                V.append(norm(tuple((V[a][i] + V[b][i]) / 2 for i in range(3))))
                mid[k] = len(V) - 1
            return mid[k]
        for a, b, c in F:
            ab, bc, ca = m(a, b), m(b, c), m(c, a)
            F2 += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        F = F2
    P3 = [tuple(x * rad(v) for x in v) for v in V]
    for a, b, c in F:
        tri = [P3[a], P3[b], P3[c]]
        n = facet_normal(tri) or (0, 1, 0)
        r.face(ROCK, [r.vert(p, n) for p in tri], n)
    return P


def rock_hits():
    return [hcyl("hubk_rock", (0, -330.0, 0), (0, 330.0, 0), 330.0)]


KIT = {"hubk_truss": (truss_parts, truss_hits, "Ферма-балка 100 м"),
       "hubk_hab_a": (hab_builder(40.0, 90.0, 30.0, 3), hab_hits(40.0, 90.0, 30.0), "Жилой блок 40x90x30"),
       "hubk_hab_b": (hab_builder(24.0, 130.0, 24.0, 5), hab_hits(24.0, 130.0, 24.0), "Жилая башня 24x130"),
       "hubk_hab_c": (hab_builder(60.0, 40.0, 36.0, 7), hab_hits(60.0, 40.0, 36.0), "Жилой корпус 60x40x36"),
       "hubk_tower": (tower_parts, tower_hits, "Решётчатая вышка 160 м"),
       "hubk_mushroom": (mushroom_parts, mushroom_hits, "Гриб-платформа на стволе"),
       "hubk_factory": (factory_parts, factory_hits, "Завод со сбросными трубами"),
       "hubk_tanks": (tanks_parts, tanks_hits, "Резервуарный парк"),
       "hubk_rock": (rock_parts, rock_hits, "Астероид-якорь 700 м")}


# ---------------------------------------------------------------- the district layout (assembly by reference)
def _rot_to(axis):
    """rotation_deg (T * Rx * Ry * Rz) taking the module's +Y ("out") to a world axis"""
    return {(0, 1, 0): (0.0, 0.0, 0.0), (0, -1, 0): (180.0, 0.0, 0.0), (0, 0, 1): (90.0, 0.0, 0.0),
            (0, 0, -1): (-90.0, 0.0, 0.0), (1, 0, 0): (0.0, 0.0, -90.0), (-1, 0, 0): (0.0, 0.0, 90.0)}[axis]


def _truss_rot(axis):
    """rotation_deg taking the truss' +Z to a world axis"""
    return {(1, 0, 0): (0.0, 90.0, 0.0), (-1, 0, 0): (0.0, -90.0, 0.0), (0, 1, 0): (-90.0, 0.0, 0.0),
            (0, -1, 0): (90.0, 0.0, 0.0), (0, 0, 1): (0.0, 0.0, 0.0), (0, 0, -1): (0.0, 180.0, 0.0)}[axis]


def district_layout(seed=2026, segs=20):
    """nodes (id, folder, position, rotation_deg, role, parent) + haze sources: a spine of trusses from the rock along +X,
    a branch every 100 m in each of the four side directions (1..4 segments), a module on every branch segment (all
    facing +X - each fills the 100 m gap to the next branch, no overlaps) and one on the branch tip facing out; houses near
    the rock, the industrial quarter at the far end"""
    rng = random.Random(seed)
    N, haze = [("rock", "hubk_rock", (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), "anchor", None)], []
    x0 = 330.0
    end = x0 + 100.0 * segs
    for k in range(segs):
        N.append((f"spine_{k}", "hubk_truss", (x0 + 100.0 * k, 0.0, 0.0), _truss_rot((1, 0, 0)), "truss", "rock"))
    sides = [(0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)]

    def pick(u, tip):
        if u < 0.55:                                   # houses near the rock
            pool = ("hubk_hab_a", "hubk_hab_b", "hubk_hab_c", "hubk_hab_a") + (("hubk_mushroom", "hubk_tower") if tip else ())
        elif u < 0.7:                                  # mixed
            pool = ("hubk_hab_c", "hubk_tanks", "hubk_hab_a") + (("hubk_tower", "hubk_mushroom") if tip else ())
        else:                                          # industrial
            pool = ("hubk_factory", "hubk_factory", "hubk_tanks") + (("hubk_tower",) if tip else ())
        return rng.choice(pool)

    for i, x in enumerate(range(int(x0 + 100), int(end), 100)):
        u = (x - x0) / (end - x0)
        for si, ax in enumerate(sides):
            if rng.random() < 0.12:
                continue
            nseg = rng.choice((1, 2, 2, 3, 3, 4))
            for j in range(nseg):
                p = (float(x), ax[1] * (6.0 + 100.0 * j), ax[2] * (6.0 + 100.0 * j))
                N.append((f"br_{i}_{si}_{j}", "hubk_truss", p, _truss_rot(ax), "truss", "rock"))
                if rng.random() < 0.85:
                    mid = (float(x) + 6.0, ax[1] * (56.0 + 100.0 * j), ax[2] * (56.0 + 100.0 * j))
                    kind = pick(u, False)
                    N.append((f"mod_{i}_{si}_{j}", kind, mid, _rot_to((1, 0, 0)), "module", "rock"))
                    if kind == "hubk_factory":
                        haze += _vents(mid, _rot_to((1, 0, 0)))
            tip = (float(x), ax[1] * (6.0 + 100.0 * nseg), ax[2] * (6.0 + 100.0 * nseg))
            kind = pick(u, True)
            N.append((f"tip_{i}_{si}", kind, tip, _rot_to(ax), "module", "rock"))
            if kind == "hubk_factory":
                haze += _vents(tip, _rot_to(ax))
    N.append(("spine_end_tower", "hubk_tower", (end + 2.0, 0.0, 0.0), _rot_to((1, 0, 0)), "module", "rock"))
    return N, haze


def _vents(pos, rot):
    import pads
    R = pads.euler(rot)
    return [{"position": [round(a + b, 2) for a, b in zip(apply(R, v), pos)], "radius_m": 260.0, "density": 1.0}
            for v in FACTORY_VENTS]


DISTRICT_NODES, DISTRICT_HAZE = district_layout()

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
ROOTS["hub_proto_district"] = "hub"
PASSPORTS = {k: (v[2] + " (макет района хаба)", "hub_module", "Модуль прототипа района хаба (tools/models/units/hub_district.py).")
             for k, v in KIT.items()}
SETS = {"hub_proto_district": {
    "title": "Прототип района хаба (сборка)",
    "description": f"Сборка района: {len(DISTRICT_NODES)} узлов-модулей по ссылкам на астероиде; дымка у {len(DISTRICT_HAZE)} сбросных труб.",
    "nodes": DISTRICT_NODES,
    # the district drawn as ONE object (live check 2026-11-16: 404 modules as separate world objects cost ~11 ms of
    # CPU a frame - replication, navigation, radar, scene prep - while their drawing cost 4 ms of GPU): its own LODs
    # merge the modules' mid / coarse levels
    "view_lods": {0: 1, 1: 2}}}
