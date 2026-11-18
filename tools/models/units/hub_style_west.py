"""National / political styles of the hub city - the "west" share (author 2026-11-18: «сгенерируй модули для хабов для
других политических систем согласно их духу. сейчас постройки довольно изящные, где-то сделать тупо практичные, где-то
с намеком на древнюю архитектуру, Китайских потомков можно селить в ультрасовременные здания, американцев - возможно в
более компактные домики - территория будет большая. Может какие-то здания будут в национальном стиле. главное, что бы
по прибытию все эти хабы визуально различались, но что бы имели одинаковый слой механики» + «фермы везде обязательно,
хотя бы в качестве мачт или опорных столбов»).

Three styles, the same roles (the same mechanics layer: every module is a hub module on a 600 x 600 m block, base on
y 0, +Y up - hung upside down under the plane as well; facades are eq_hub_facade* so the shader draws their windows):
  practical  - the Citadel compact / corporate prospectors: brutalist concrete and armour, bunker slabs, container-stack
               dormitories, exposed risers and cable trays, gun and sensor masts on lattice trusses;
  belt_clans - the free belt clans: habitats welded from tanks and hull sections, rock-shielded domes, scaffolding,
               cable webs, uneven stacks;
  american   - Homestead (the North American consortium): a suburb of small houses on a lane grid (a whole 250 m block),
               a strip mall / office park, a brick walk-up with rooftop water tanks, an art-deco skyscraper, a glass
               condo tower, a capitol.
Every module carries visible lattice trusses (masts, frames, legs). Ids hubx_<style>_<role>.
"""
import math
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import (BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, ROCK, STEEL, WARM, beacon,  # noqa: F401
                                rock_parts)
from units.hub_mega import FACADE, FACADE_L  # noqa: F401
from units.hub_belt import CONT, GRATE, MARK, ROAD, lamp

register_materials({
    # facades (ids start with eq_hub_facade: the shader keeps their lit window grid)
    "eq_hub_facade_wconcrete": ((0.38, 0.38, 0.36), (0.10, 0.10, 0.10), 15),       # raw concrete
    "eq_hub_facade_wrust": ((0.34, 0.21, 0.14), (0.20, 0.18, 0.16), 30),           # rusted salvaged plating
    "eq_hub_facade_wpatch": ((0.26, 0.29, 0.23), (0.20, 0.20, 0.20), 30),          # olive patch plates
    "eq_hub_facade_wsiding": ((0.80, 0.78, 0.72), (0.08, 0.08, 0.08), 15),         # white clapboard
    "eq_hub_facade_wsiding_b": ((0.56, 0.66, 0.74), (0.08, 0.08, 0.08), 15),       # pale blue clapboard
    "eq_hub_facade_wsiding_c": ((0.80, 0.71, 0.50), (0.08, 0.08, 0.08), 15),       # cream / yellow clapboard
    "eq_hub_facade_wbrick": ((0.47, 0.22, 0.16), (0.06, 0.06, 0.06), 10),          # red brick
    "eq_hub_facade_wlimestone": ((0.72, 0.68, 0.60), (0.12, 0.12, 0.12), 20),      # art-deco / capitol stone
    "eq_hub_facade_wglass": ((0.14, 0.24, 0.31), (0.60, 0.62, 0.66), 120),         # curtain wall
    # plain materials
    "eq_hubx_w_armor": ((0.21, 0.23, 0.21), (0.30, 0.30, 0.30), 50),               # armour plating
    "eq_hubx_w_scrap": ((0.40, 0.30, 0.22), (0.25, 0.22, 0.20), 30),               # salvaged hull scrap
    "eq_hubx_w_roof_red": ((0.43, 0.17, 0.13), (0.06, 0.06, 0.06), 10),
    "eq_hubx_w_roof_slate": ((0.20, 0.21, 0.24), (0.08, 0.08, 0.08), 10),
    "eq_hubx_w_roof_brown": ((0.35, 0.26, 0.19), (0.06, 0.06, 0.06), 10),
    "eq_hubx_w_lawn": ((0.20, 0.35, 0.14), (0.02, 0.02, 0.02), 5),
    "eq_hubx_w_tree": ((0.11, 0.25, 0.10), (0.02, 0.02, 0.02), 5),
    "eq_hubx_w_white": ((0.86, 0.86, 0.84), (0.20, 0.20, 0.20), 40),               # trim, columns, domes
    "eq_hubx_w_gold": ((0.72, 0.56, 0.22), (0.70, 0.60, 0.40), 120),               # deco crown
    "eq_hubx_w_wood": ((0.42, 0.29, 0.18), (0.05, 0.05, 0.05), 10),                # rooftop water tanks
    "eq_hubx_w_concrete": ((0.52, 0.52, 0.50), (0.06, 0.06, 0.06), 10),            # driveways, kerbs, plazas
})
CONC, RUST, PATCH = "eq_hub_facade_wconcrete", "eq_hub_facade_wrust", "eq_hub_facade_wpatch"
SIDING = ("eq_hub_facade_wsiding", "eq_hub_facade_wsiding_b", "eq_hub_facade_wsiding_c", "eq_hub_facade_wbrick")
BRICK, STONE, GLASS = "eq_hub_facade_wbrick", "eq_hub_facade_wlimestone", "eq_hub_facade_wglass"
ARMOR, SCRAP, WHITE, GOLD, WOOD, PAVE = ("eq_hubx_w_armor", "eq_hubx_w_scrap", "eq_hubx_w_white", "eq_hubx_w_gold",
                                         "eq_hubx_w_wood", "eq_hubx_w_concrete")
ROOFS = ("eq_hubx_w_roof_red", "eq_hubx_w_roof_slate", "eq_hubx_w_roof_brown")
LAWN, TREE = "eq_hubx_w_lawn", "eq_hubx_w_tree"


# ---------------------------------------------------------------- small vector helpers
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _len(a): return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def strut(p, mat, a, b, r):
    """a square bar from a to b (half-thickness r): 12 triangles - the member of every lattice here"""
    d = _sub(b, a)
    L = _len(d)
    if L < 1e-6:
        return
    box(p, mat, _mul(_add(a, b), 0.5), (2 * r, 2 * r, L), axis_to(_mul(d, 1.0 / L)))


def truss(p, a, b, h, lod, mat=STEEL, bay=None, r=None, diag=(0, 1, 2, 3)):
    """a square lattice truss from a to b, half-width h: 4 chords, a frame every bay (LOD0; every second bay at LOD1,
    the ends only at LOD2), Warren diagonals on the faces in diag (LOD0) - the visible trusses the author asked for
    («фермы везде обязательно, хотя бы в качестве мачт или опорных столбов»)"""
    d = _sub(b, a)
    L = _len(d)
    d = _mul(d, 1.0 / L)
    up = (0.0, 1.0, 0.0) if abs(d[1]) < 0.9 else (1.0, 0.0, 0.0)
    s = norm(_cross(d, up))
    u = _cross(s, d)
    r = r or max(0.25, h * 0.08)
    bay = bay or 3.0 * h
    n = max(1, int(round(L / bay)))
    cs = ((1, 1), (-1, 1), (-1, -1), (1, -1))
    C = lambda t, k: _add(_add(a, _mul(d, t)), _add(_mul(s, h * cs[k][0]), _mul(u, h * cs[k][1])))
    for k in range(4):
        strut(p, mat, C(0.0, k), C(L, k), r)
    nb = n if lod == 0 else max(1, n // 2) if lod == 1 else 1
    for i in range(nb + 1):
        t = L * i / nb
        for k in range(4):
            strut(p, mat, C(t, k), C(t, (k + 1) % 4), r * 0.7)
    if lod == 0:
        for i in range(nb):
            t0, t1 = L * i / nb, L * (i + 1) / nb
            for k in diag:
                if i % 2 == 0:
                    strut(p, mat, C(t0, k), C(t1, (k + 1) % 4), r * 0.6)
                else:
                    strut(p, mat, C(t0, (k + 1) % 4), C(t1, k), r * 0.6)


def mast(p, x, z, y0, y1, h, lod, top=True):
    """a vertical lattice mast with a beacon on top"""
    truss(p, (x, y0, z), (x, y1, z), h, lod)
    if top and lod < 3:
        beacon(p, (x, y1 + 1.0, z), max(1.0, h * 0.5))


def _poly(p, mat, pts, centre):
    """a flat polygon facing away from centre"""
    n = facet_normal(pts)
    if n is None:
        return
    m = _mul(_add(_add(pts[0], pts[1]), pts[2]), 1.0 / 3.0)
    if sum((m[i] - centre[i]) * n[i] for i in range(3)) < 0:
        n = _mul(n, -1.0)
    p.face(mat, [p.vert(q, n) for q in pts], n)


def gable(p, mat, cx, y, cz, w, d, h, along_x=True):
    """a pitched roof: a triangular prism over w (x) x d (z) from y, ridge h above along x (along_x) or z"""
    if not along_x:                                      # swap the axes through a mirror-free relabel
        x0, x1, z0, z1 = cx - w / 2, cx + w / 2, cz - d / 2, cz + d / 2
        r0, r1 = (cx, y + h, z0), (cx, y + h, z1)
        e = [(x0, y, z0), (x0, y, z1), (x1, y, z1), (x1, y, z0)]
        c = (cx, y + h / 3, cz)
        _poly(p, mat, [e[0], e[1], r1, r0], c)
        _poly(p, mat, [e[3], e[2], r1, r0], c)
        _poly(p, mat, [e[0], e[3], r0], c)
        _poly(p, mat, [e[1], e[2], r1], c)
        _poly(p, mat, e, c)
        return
    x0, x1, z0, z1 = cx - w / 2, cx + w / 2, cz - d / 2, cz + d / 2
    r0, r1 = (x0, y + h, cz), (x1, y + h, cz)
    e = [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)]
    c = (cx, y + h / 3, cz)
    _poly(p, mat, [e[0], e[1], r1, r0], c)
    _poly(p, mat, [e[3], e[2], r1, r0], c)
    _poly(p, mat, [e[0], e[3], r0], c)
    _poly(p, mat, [e[1], e[2], r1], c)
    _poly(p, mat, e, c)


def hip(p, mat, cx, y, cz, w, d, h):
    """a hip roof over w x d: the ridge along the longer side, inset half the shorter side"""
    x0, x1, z0, z1 = cx - w / 2, cx + w / 2, cz - d / 2, cz + d / 2
    e = [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)]
    c = (cx, y + h / 3, cz)
    if w >= d:
        i = d / 2
        r0, r1 = (x0 + i, y + h, cz), (x1 - i, y + h, cz)
        _poly(p, mat, [e[0], e[1], r1, r0], c)
        _poly(p, mat, [e[3], e[2], r1, r0], c)
        _poly(p, mat, [e[0], e[3], r0], c)
        _poly(p, mat, [e[1], e[2], r1], c)
    else:
        i = w / 2
        r0, r1 = (cx, y + h, z0 + i), (cx, y + h, z1 - i)
        _poly(p, mat, [e[0], e[3], r1, r0], c)
        _poly(p, mat, [e[1], e[2], r1, r0], c)
        _poly(p, mat, [e[0], e[1], r0], c)
        _poly(p, mat, [e[3], e[2], r1], c)
    _poly(p, mat, e, c)


def wedge(p, mat, x0, x1, zw, zo, y0, y1):
    """an armour glacis along x: full height y1 at the wall edge z = zw, sloping down to y0 at z = zo"""
    pts_w = [(x0, y0, zw), (x1, y0, zw), (x1, y1, zw), (x0, y1, zw)]
    c = (0.5 * (x0 + x1), y0 + (y1 - y0) / 3, zw + (zo - zw) / 3)
    _poly(p, mat, pts_w, c)
    _poly(p, mat, [(x0, y0, zw), (x1, y0, zw), (x1, y0, zo), (x0, y0, zo)], c)
    _poly(p, mat, [(x0, y1, zw), (x1, y1, zw), (x1, y0, zo), (x0, y0, zo)], c)
    _poly(p, mat, [(x0, y0, zw), (x0, y1, zw), (x0, y0, zo)], c)
    _poly(p, mat, [(x1, y0, zw), (x1, y1, zw), (x1, y0, zo)], c)


def _merge(dst, src, f):
    """copy src's faces into dst, its vertices through f (a near-rigid map)"""
    base = len(dst.v)
    dst.v += [f(v) for v in src.v]
    dst.n += list(src.n)
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + base for i in fc) for fc in fl)


def rock_cap(p, lod, radius, seed, cy, sy=1.0, t=(0.0, 0.0)):
    """a lumpy asteroid boulder (hub_district.rock_parts) centred cy above the ground, squashed by sy, the part under
    y 0 left out (the faces straddling the ground clamped to it)"""
    src = rock_parts(lod, radius, seed, "_rock")["_rock"]
    keep = Part("_k")
    keep.v, keep.n = src.v, src.n
    for m, fl in src.faces.items():
        keep.faces[m] = [fc for fc in fl if max(cy + src.v[i][1] * sy for i in fc) > 0.0]
    _merge(p, keep, lambda v: (v[0] + t[0], max(0.0, cy + v[1] * sy), v[2] + t[1]))


def _road_x(p, x0, x1, z0, z1, lod, y=0.0):
    """a roadway along x (cf. hub_belt.road_strip along z)"""
    box(p, ROAD, ((x0 + x1) / 2, y + 0.05, (z0 + z1) / 2), (x1 - x0, 0.1, z1 - z0))
    if lod < 2:
        x = x0 + 6.0
        while x < x1 - 6.0:
            box(p, MARK, (x, y + 0.12, (z0 + z1) / 2), (6.0, 0.05, 0.3))
            x += 12.0


def _road_z(p, x0, x1, z0, z1, lod, y=0.0):
    box(p, ROAD, ((x0 + x1) / 2, y + 0.05, (z0 + z1) / 2), (x1 - x0, 0.1, z1 - z0))
    if lod < 2:
        z = z0 + 6.0
        while z < z1 - 6.0:
            box(p, MARK, ((x0 + x1) / 2, y + 0.12, z), (0.3, 0.05, 6.0))
            z += 12.0


def _one(name, fn):
    """a builder of a single-part module"""
    def build(lod):
        P = {name: Part(name)}
        fn(P[name], lod)
        return P
    return build


# ================================================================ PRACTICAL: the Citadel compact / corporate prospectors
def practical_tower(p, lod):
    """tall residential: a raw-concrete slab tower 64 x 64 x 240 on an armoured plinth, armour bands every 40 m, corner
    armour pilasters, exposed risers and a cable tray, an external lattice service column and a sensor mast"""
    box(p, ARMOR, (0, 6.0, 0), (100.0, 12.0, 100.0))
    box(p, CONC, (0, 132.0, 0), (64.0, 240.0, 64.0))
    for y in (52.0, 92.0, 132.0, 172.0, 212.0):
        box(p, ARMOR, (0, y, 0), (68.0, 4.0, 68.0))
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(p, ARMOR, (sx * 31.0, 132.0, sz * 31.0), (8.0, 240.0, 8.0))
    box(p, HULL, (0, 256.0, 0), (44.0, 8.0, 44.0))
    if lod < 2:
        for z in (-12.0, -4.0, 4.0, 12.0):                                    # risers on +x
            cyl(p, STEEL, (34.0, 12.0, z), (34.0, 258.0, z), 1.6, 6 if lod == 0 else 4)
        box(p, STEEL, (-10.0, 132.0, -33.5), (6.0, 240.0, 1.2))               # the cable tray on -z
        for y in range(30, 250, 30):
            box(p, HULL, (-33.0, float(y), 20.0), (4.0, 3.0, 10.0))           # vent boxes
        for sx in (-1, 1):
            box(p, FLOOD, (sx * 51.0, 12.5, 0), (1.0, 1.0, 20.0))
    truss(p, (-38.0, 12.0, 0), (-38.0, 262.0, 0), 3.0, lod)                     # the external service column
    for y in (92.0, 172.0, 252.0):
        truss(p, (-35.0, y, 0), (-32.0, y, 0), 1.5, min(lod + 1, 2), bay=3.0)   # brackets into the wall
    mast(p, 10.0, 10.0, 260.0, 330.0, 2.5, lod)                                # the sensor mast
    box(p, HULL, (10.0, 318.0, 10.0), (14.0, 3.0, 4.0))                        # radar slab
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(p, (sx * 21.0, 261.0, sz * 21.0), 1.0)


def practical_tower_hits():
    n = "hubx_practical_tower"
    return [hbox(n, (0, 6.0, 0), (100.0, 12.0, 100.0)), hbox(n, (0, 136.0, 0), (72.0, 248.0, 72.0)),
            hbox(n, (-38.0, 137.0, 0), (7.0, 250.0, 7.0)), hcyl(n, (10.0, 260.0, 10.0), (10.0, 330.0, 10.0), 3.0)]


def practical_block(p, lod):
    """residential block: two bunker slabs (200 x 34 x 54, armour glacis at the foot) round a container-stack
    dormitory in a lattice frame, pipe bridges on trusses across, sensor masts on the slabs"""
    rng = random.Random(31)
    for z, side in ((-72.0, -1), (72.0, 1)):
        box(p, CONC, (0, 27.0, z), (200.0, 54.0, 34.0))
        box(p, ARMOR, (0, 56.0, z), (204.0, 4.0, 38.0))                      # roof armour
        wedge(p, ARMOR, -100.0, 100.0, z + side * 17.0, z + side * 29.0, 0.0, 14.0)
        for x in (-60.0, 0.0, 60.0):
            box(p, HULL, (x, 62.0, z), (16.0, 8.0, 14.0))                    # roof plant
        if lod < 2:
            for k in range(3):
                cyl(p, STEEL, (-100.0, 50.0 - 3.0 * k, z - side * 18.5), (100.0, 50.0 - 3.0 * k, z - side * 18.5), 1.0, 4)
        mast(p, side * 80.0, z, 58.0, 110.0, 1.8, lod)
    # the container dormitory: 6 tiers of 16 x 4 x 4 m hab containers, two rows, in a lattice frame
    H = [[rng.randint(4, 6) for _ in range(11)] for _ in range(2)]
    for row, z in enumerate((-12.0, 12.0)):
        for i in range(11):
            x = -88.0 + i * 17.6
            if lod >= 2:
                box(p, CONT[(i + row) % 3], (x, H[row][i] * 2.0, z), (16.0, H[row][i] * 4.0, 18.0))
                continue
            for t in range(H[row][i]):
                c = CONT[rng.randrange(3)]
                box(p, c, (x, 2.0 + 4.0 * t, z), (16.0, 3.8, 18.0))
                if lod == 0 and rng.random() < 0.7:
                    box(p, WARM if rng.random() < 0.4 else DARK, (x, 2.3 + 4.0 * t, z + 9.05 * (1 if row else -1)),
                        (10.0, 1.2, 0.1))
    for x in (-96.0, -48.0, 0.0, 48.0, 96.0):                                  # the frame columns
        for z in (-24.0, 24.0):
            truss(p, (x, 0.0, z), (x, 28.0, z), 1.2, lod, bay=4.0)
    truss(p, (-96.0, 28.0, -24.0), (96.0, 28.0, -24.0), 1.2, lod, bay=8.0)
    truss(p, (-96.0, 28.0, 24.0), (96.0, 28.0, 24.0), 1.2, lod, bay=8.0)
    for x in (-60.0, 20.0):                                                    # pipe bridges slab - frame - slab
        for z0, z1 in ((-55.0, -25.0), (25.0, 55.0)):
            truss(p, (x, 36.0, z0), (x, 36.0, z1), 2.0, lod, bay=6.0)
            if lod < 2:
                cyl(p, STEEL, (x, 39.0, z0), (x, 39.0, z1), 0.9, 4)


def practical_block_hits():
    n = "hubx_practical_block"
    return [hbox(n, (0, 29.0, -72.0), (204.0, 58.0, 38.0)), hbox(n, (0, 29.0, 72.0), (204.0, 58.0, 38.0)),
            hbox(n, (0, 14.0, 0), (196.0, 28.0, 50.0))]


def practical_office(p, lod):
    """office / HQ: a stepped brutalist command block (3 tiers) under armour shields, turret domes on the corners, a
    helipad and two lattice comm masts with dishes"""
    seg = {0: 12, 1: 8, 2: 6}[lod]
    box(p, CONC, (0, 15.0, 0), (170.0, 30.0, 120.0))
    box(p, ARMOR, (0, 31.5, 0), (178.0, 3.0, 128.0))
    box(p, CONC, (0, 53.0, 0), (130.0, 40.0, 90.0))
    box(p, ARMOR, (0, 74.5, 0), (136.0, 3.0, 96.0))
    box(p, CONC, (0, 96.0, 0), (90.0, 40.0, 60.0))
    box(p, ARMOR, (0, 117.0, 0), (94.0, 2.0, 64.0))
    box(p, HULL, (-20.0, 119.0, 0), (36.0, 2.0, 36.0))                          # helipad
    if lod < 2:
        box(p, HAZ, (-20.0, 120.1, 0), (24.0, 0.2, 3.0))
        box(p, HAZ, (-20.0, 120.1, 0), (3.0, 0.2, 24.0))
        for sx in (-1, 1):                                                     # a blast wall at the entrance
            wedge(p, ARMOR, -40.0, 40.0, sx * 60.0, sx * 70.0, 0.0, 10.0)
    for sx in (-1, 1):
        for sz in (-1, 1):
            c = (sx * 75.0, 33.0, sz * 50.0)
            ylathe(p, ARMOR, [(0, 0), (8.0, 0)] + [(8.0 * math.cos(a), 8.0 * math.sin(a)) for a in (0.5, 1.0)] + [(0, 8.0)],
                   seg, t=c)
            if lod < 2:
                strut(p, STEEL, (c[0], c[1] + 4.0, c[2]), (c[0] + sx * 16.0, c[1] + 6.0, c[2]), 0.5)
    for x in (25.0, 38.0):
        mast(p, x, 0.0, 118.0, 180.0 if x < 30 else 160.0, 1.6, lod)
    ylathe(p, LIGHT, [(0, 0), (6.0, 1.5), (6.2, 2.2), (0, 1.0)], seg, t=(25.0, 165.0, 0.0))   # dish
    if lod < 2:
        for x in range(-80, 81, 20):
            box(p, FLOOD, (float(x), 30.0, 60.6), (2.0, 0.8, 0.6))


def practical_office_hits():
    n = "hubx_practical_office"
    return [hbox(n, (0, 16.5, 0), (178.0, 33.0, 128.0)), hbox(n, (0, 54.5, 0), (136.0, 43.0, 96.0)),
            hbox(n, (0, 98.0, 0), (94.0, 44.0, 64.0)), hcyl(n, (25.0, 118.0, 0), (25.0, 180.0, 0), 2.5),
            hcyl(n, (38.0, 118.0, 0), (38.0, 160.0, 0), 2.5)]


def practical_downtown(p, lod):
    """downtown tower: a 72 x 72 x 340 concrete shaft held by an exoskeleton - four lattice corner columns and
    zig-zag mega-braces on the faces, armour crown, a sensor mast (top ~395 m)"""
    H = 340.0
    box(p, CONC, (0, 6.0, 0), (110.0, 12.0, 110.0))
    box(p, CONC, (0, H / 2, 0), (72.0, H, 72.0))
    for y in (85.0, 170.0, 255.0):
        box(p, ARMOR, (0, y, 0), (80.0, 5.0, 80.0))                             # transfer floors
    box(p, ARMOR, (0, H + 6.0, 0), (84.0, 12.0, 84.0))
    box(p, HULL, (0, H + 16.0, 0), (50.0, 8.0, 50.0))
    o = 42.0
    for sx in (-1, 1):
        for sz in (-1, 1):
            truss(p, (sx * o, 0.0, sz * o), (sx * o, H + 12.0, sz * o), 4.0, lod, bay=14.0)
    for f, (nx, nz) in enumerate(((1, 0), (0, 1), (-1, 0), (0, -1))):          # mega-braces, zig-zag up each face
        for k in range(4):
            y0, y1 = k * 85.0, (k + 1) * 85.0
            a = -o if (k + f) % 2 == 0 else o
            p0 = (nx * (o - 1.0) + (a if nz else 0.0), y0 + 4.0, nz * (o - 1.0) + (a if nx else 0.0))
            p1 = (nx * (o - 1.0) + (-a if nz else 0.0), y1 - 4.0, nz * (o - 1.0) + (-a if nx else 0.0))
            truss(p, p0, p1, 1.6, lod, bay=10.0, diag=(0, 2))
    mast(p, 0.0, 0.0, H + 20.0, H + 55.0, 2.0, lod)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(p, (sx * o, H + 13.0, sz * o), 1.6)
                box(p, FLOOD, (sx * 56.0, 12.5, sz * 56.0), (2.0, 1.0, 2.0))


def practical_downtown_hits():
    n = "hubx_practical_downtown"
    return [hbox(n, (0, 6.0, 0), (110.0, 12.0, 110.0)), hbox(n, (0, 176.0, 0), (94.0, 352.0, 94.0)),
            hcyl(n, (0, 352.0, 0), (0, 395.0, 0), 3.0)]


def practical_landmark(p, lod):
    """landmark: the bastion - an octagonal armoured ring wall with glacis and corner bastion towers, barracks inside,
    a concrete keep and a big lattice radar tower; gun masts on two bastions"""
    seg = {0: 12, 1: 8, 2: 6}[lod]
    R = 105.0
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        c = (R * math.cos(a), 15.0, R * math.sin(a))
        L = 2 * R * math.tan(math.pi / 8)
        box(p, CONC, c, (L, 30.0, 12.0), rot_y(-a + math.pi / 2))
        box(p, ARMOR, (c[0], 31.0, c[2]), (L, 2.0, 14.0), rot_y(-a + math.pi / 2))
        # glacis: an armour wedge from the wall top down to the ground outside
        ca, sa = math.cos(a), math.sin(a)
        Q = lambda t, rr, y: (rr * ca - t * sa, y, rr * sa + t * ca)
        cen = Q(0.0, R + 10.0, 8.0)
        for t0, t1 in ((-L / 2, L / 2),):
            _poly(p, ARMOR, [Q(t0, R + 6.0, 0.0), Q(t1, R + 6.0, 0.0), Q(t1, R + 6.0, 26.0), Q(t0, R + 6.0, 26.0)], cen)
            _poly(p, ARMOR, [Q(t0, R + 6.0, 26.0), Q(t1, R + 6.0, 26.0), Q(t1, R + 30.0, 0.0), Q(t0, R + 30.0, 0.0)], cen)
            _poly(p, ARMOR, [Q(t0, R + 6.0, 0.0), Q(t1, R + 6.0, 0.0), Q(t1, R + 30.0, 0.0), Q(t0, R + 30.0, 0.0)], cen)
            _poly(p, ARMOR, [Q(t0, R + 6.0, 0.0), Q(t0, R + 6.0, 26.0), Q(t0, R + 30.0, 0.0)], cen)
            _poly(p, ARMOR, [Q(t1, R + 6.0, 0.0), Q(t1, R + 6.0, 26.0), Q(t1, R + 30.0, 0.0)], cen)
        tx, tz = R / math.cos(math.pi / 8) * math.cos(a + math.pi / 8), R / math.cos(math.pi / 8) * math.sin(a + math.pi / 8)
        cyl(p, CONC, (tx, 0.0, tz), (tx, 42.0, tz), 13.0, seg)                  # bastion tower
        ylathe(p, ARMOR, [(0, 0), (9.0, 0), (6.5, 6.0), (0, 7.0)], seg, t=(tx, 42.0, tz))
        if lod < 2:
            strut(p, STEEL, (tx, 46.0, tz), (tx * 1.12, 47.0, tz * 1.12), 0.6)   # the gun
            beacon(p, (tx, 49.5, tz), 1.2)
        if k in (1, 5):
            mast(p, tx, tz, 49.0, 100.0, 1.6, lod)
    for x, z in ((-50.0, -40.0), (50.0, -40.0), (-50.0, 40.0), (50.0, 40.0)):  # barracks
        box(p, CONC, (x, 12.0, z), (50.0, 24.0, 26.0))
        box(p, ARMOR, (x, 25.0, z), (52.0, 2.0, 28.0))
    box(p, CONC, (0, 50.0, 0), (60.0, 100.0, 60.0))                              # the keep
    box(p, ARMOR, (0, 103.0, 0), (66.0, 6.0, 66.0))
    for sx in (-1, 1):
        for sz in (-1, 1):
            truss(p, (sx * 22.0, 106.0, sz * 22.0), (sx * 10.0, 230.0, sz * 10.0), 2.0, lod, bay=10.0)
    for y in (140.0, 180.0, 220.0):
        c = 22.0 - 12.0 * (y - 106.0) / 124.0
        for (ax, az), (bx, bz) in (((-c, -c), (c, -c)), ((c, -c), (c, c)), ((c, c), (-c, c)), ((-c, c), (-c, -c))):
            strut(p, STEEL, (ax, y, az), (bx, y, bz), 0.5)
    box(p, HULL, (0, 232.0, 0), (26.0, 4.0, 26.0))
    box(p, LIGHT, (0, 240.0, 0), (40.0, 8.0, 3.0))                               # the radar array
    mast(p, 0.0, 0.0, 244.0, 270.0, 1.0, lod)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(p, FLOOD, (sx * 31.0, 106.6, sz * 31.0), (2.0, 1.0, 2.0))


def practical_landmark_hits():
    n = "hubx_practical_landmark"
    return [hcyl(n, (0, 0, 0), (0, 32.0, 0), 120.0), hbox(n, (0, 51.5, 0), (66.0, 103.0, 66.0)),
            hbox(n, (0, 170.0, 0), (46.0, 130.0, 46.0)), hcyl(n, (0, 236.0, 0), (0, 270.0, 0), 20.0)]


# ================================================================ BELT CLANS: the free belt clans
def _tank(p, mat, a, b, r, lod):
    """a salvaged tank (capped cylinder with domed-ish ends) lying from a to b, weld bands at lod 0"""
    seg = {0: 10, 1: 8, 2: 6}[lod]
    cyl(p, mat, a, b, r, seg)
    if lod == 0:
        d = _sub(b, a)
        for t in (0.15, 0.85):
            c0 = _add(a, _mul(d, t - 0.01))
            c1 = _add(a, _mul(d, t + 0.01))
            cyl(p, SCRAP, c0, c1, r * 1.04, seg)


def belt_tower(p, lod):
    """tall residential: a lattice spine 230 m with tanks and hull sections welded round it at random, scaffolding,
    guy cables to ground anchors"""
    rng = random.Random(71)
    truss(p, (0, 0, 0), (0, 235.0, 0), 6.0, lod, bay=14.0)
    y = 6.0
    while y < 210.0:
        a = rng.uniform(0, 2 * math.pi)
        rr = rng.uniform(14.0, 24.0)
        x, z = rr * math.cos(a), rr * math.sin(a)
        u = rng.random()
        mat = RUST if rng.random() < 0.7 else PATCH
        if u < 0.45:                                                           # an upright tank
            r, h = rng.uniform(8.0, 13.0), rng.uniform(18.0, 30.0)
            _tank(p, mat, (x, y, z), (x, y + h, z), r, lod)
            top = y + h
        elif u < 0.8:                                                          # a lying tank
            r, L = rng.uniform(6.0, 9.0), rng.uniform(30.0, 46.0)
            ang = rng.uniform(0, math.pi)
            dx, dz = math.cos(ang) * L / 2, math.sin(ang) * L / 2
            _tank(p, mat, (x - dx, y + r, z - dz), (x + dx, y + r, z + dz), r, lod)
            top = y + 2 * r
        else:                                                                  # a hull section, tilted
            w, h, d = rng.uniform(16, 26), rng.uniform(10, 16), rng.uniform(20, 34)
            box(p, mat, (x, y + h / 2, z), (w, h, d), rot_y(rng.uniform(0, math.pi)), chamfer=1.0 if lod == 0 else 0.0)
            top = y + h
        strut(p, STEEL, (x * 0.3, y + 3.0, z * 0.3), (x * 0.7, y + 3.0, z * 0.7), 0.7)   # welded to the spine
        if lod < 2 and rng.random() < 0.5:
            beacon(p, (x, top + 0.8, z), 0.8)
        y += rng.uniform(9.0, 20.0)
    for k in range(3):                                                         # scaffolding round the stack
        a = 2 * math.pi * k / 3 + 0.4
        x, z = 40.0 * math.cos(a), 40.0 * math.sin(a)
        top = rng.uniform(70.0, 150.0)
        truss(p, (x, 0.0, z), (x, top, z), 2.0, lod, bay=6.0)
        truss(p, (x, top - 4.0, z), (x * 0.4, top - 4.0, z * 0.4), 1.2, min(lod + 1, 2), bay=5.0)
        if lod < 2:
            box(p, GRATE, (x, top, z), (8.0, 0.6, 8.0))
    for k in range(4):                                                         # guy cables
        a = 2 * math.pi * k / 4 + 0.8
        strut(p, STEEL, (0, 225.0, 0), (62.0 * math.cos(a), 0.5, 62.0 * math.sin(a)), 0.35)
        box(p, SCRAP, (62.0 * math.cos(a), 1.5, 62.0 * math.sin(a)), (5.0, 3.0, 5.0))
    box(p, SCRAP, (0, 238.0, 0), (16.0, 6.0, 16.0))
    mast(p, 0.0, 0.0, 241.0, 262.0, 1.0, lod)


def belt_tower_hits():
    n = "hubx_belt_clans_tower"
    return [hbox(n, (0, 120.0, 0), (76.0, 240.0, 76.0)), hcyl(n, (0, 240.0, 0), (0, 262.0, 0), 2.0)]


def belt_block(p, lod):
    """residential block: stacks of lying hab tanks (2-3 high, uneven, crossing each other), container shacks on top,
    scaffolding towers at the ends, catwalk trusses between the stacks, cable webs, boulders"""
    rng = random.Random(83)
    stacks = [(-70.0, -55.0, 0), (5.0, -60.0, 1), (75.0, -45.0, 0), (-60.0, 40.0, 1), (20.0, 55.0, 0), (85.0, 50.0, 1)]
    tops = []
    for sx, sz, ax in stacks:
        y = 0.0
        n = rng.choice((2, 3, 3))
        for t in range(n):
            r = rng.uniform(8.0, 11.0)
            L = rng.uniform(44.0, 60.0)
            o = rng.uniform(-6.0, 6.0)
            al = (ax + t) % 2
            a = (sx - L / 2, y + r, sz + o) if al == 0 else (sx + o, y + r, sz - L / 2)
            b = (sx + L / 2, y + r, sz + o) if al == 0 else (sx + o, y + r, sz + L / 2)
            _tank(p, RUST if rng.random() < 0.6 else PATCH, a, b, r, lod)
            if lod < 2:                                                        # cradles
                for f in (-0.3, 0.3):
                    c = _add(_mul(_add(a, b), 0.5), (f * L if al == 0 else 0.0, -r + 1.0, 0.0 if al == 0 else f * L))
                    box(p, SCRAP, c, (4.0 if al == 0 else 2 * r, 2.0, 2 * r if al == 0 else 4.0))
            y += 2 * r - 1.0
        for k in range(rng.randint(1, 3)):                                     # shacks on top
            box(p, CONT[rng.randrange(3)], (sx + rng.uniform(-14, 14), y + 1.3, sz + rng.uniform(-6, 6)), (12.2, 2.6, 2.44),
                rot_y(rng.choice((0.0, math.pi / 2, 0.3))))
        if lod < 2:
            beacon(p, (sx, y + 4.0, sz), 1.0)
        tops.append((sx, y, sz))
    for x, z in ((-110.0, -90.0), (110.0, -90.0), (-110.0, 90.0), (110.0, 90.0), (-20.0, 0.0)):
        top = rng.uniform(45.0, 70.0)
        truss(p, (x, 0.0, z), (x, top, z), 2.5, lod, bay=7.5)
        if lod < 2:
            box(p, GRATE, (x, top, z), (10.0, 0.8, 10.0))
            lamp(p, x, top, z, 3.0)
    for (ax_, ay, az), (bx, by, bz) in ((tops[0], tops[1]), (tops[1], tops[2]), (tops[3], tops[4]), (tops[4], tops[5]),
                                         (tops[1], tops[4])):
        yy = min(ay, by) - 4.0
        truss(p, (ax_, yy, az), (bx, yy, bz), 1.5, lod, bay=5.0, diag=(0, 2))   # catwalks
        if lod < 3:
            strut(p, STEEL, (ax_, max(ay, by) + 8.0, az), (bx, max(ay, by) + 2.0, bz), 0.25)   # a cable
    for k in range(6):                                                          # boulders round the yard
        a = rng.uniform(0, 2 * math.pi)
        s = rng.uniform(8.0, 16.0)
        box(p, ROCK, (115.0 * math.cos(a) * 0.9, s * 0.4, 95.0 * math.sin(a) * 0.9), (s, s * 0.8, s * 1.2), rot_y(a))
    if lod < 2:
        for k in range(14):
            sx, y, sz = tops[k % len(tops)]
            box(p, WARM if k % 3 else COOL, (sx + rng.uniform(-20, 20), rng.uniform(4.0, max(6.0, y - 4.0)), sz + rng.uniform(-12, 12)),
                (2.5, 1.0, 2.5))


def belt_block_hits():
    n = "hubx_belt_clans_block"
    return [hbox(n, (0, 30.0, 0), (240.0, 60.0, 190.0))]


def belt_office(p, lod):
    """office / clan hall: a market hall under a shield of piled asteroid rock (a lumpy dome) on a welded ring wall,
    hab blocks punched out through the rock, a lattice gantry crane over it, air locks"""
    rng = random.Random(97)
    seg = {0: 20, 1: 14, 2: 10}[lod]
    ylathe(p, RUST, [(0, 0), (82.0, 0), (82.0, 14.0), (0, 14.0)], seg)
    rock_cap(p, lod, 72.0, 19, 6.0, 0.62)
    for k in range(7):                                                         # blocks through the rock
        a = 2 * math.pi * k / 7 + 0.2
        r = 58.0
        h = rng.uniform(10.0, 16.0)
        box(p, RUST if k % 2 else PATCH, (r * math.cos(a), 22.0 + h / 2, r * math.sin(a)), (26.0, h, 18.0),
            rot_y(-a + math.pi / 2))
    for k in range(4):                                                         # air locks
        a = 2 * math.pi * k / 4
        box(p, HULL, (88.0 * math.cos(a), 6.0, 88.0 * math.sin(a)), (12.0, 12.0, 12.0), rot_y(-a))
        if lod < 2:
            box(p, FLOOD, (95.0 * math.cos(a), 12.5, 95.0 * math.sin(a)), (2.0, 1.0, 2.0))
    for x in (-92.0, 92.0):                                                    # the gantry crane
        truss(p, (x, 0.0, -20.0), (x, 78.0, -20.0), 3.0, lod, bay=9.0)
    truss(p, (-92.0, 81.0, -20.0), (92.0, 81.0, -20.0), 3.0, lod, bay=9.0)
    box(p, HAZ, (20.0, 76.0, -20.0), (8.0, 6.0, 8.0))                          # the trolley
    if lod < 3:
        strut(p, STEEL, (20.0, 73.0, -20.0), (20.0, 55.0, -20.0), 0.3)
    mast(p, 0.0, 0.0, 50.0, 95.0, 1.6, lod)
    if lod < 2:
        for x in (-92.0, 92.0):
            beacon(p, (x, 85.0, -20.0), 1.2)


def belt_office_hits():
    n = "hubx_belt_clans_office"
    return [hcyl(n, (0, 0, 0), (0, 55.0, 0), 84.0), hbox(n, (0, 42.0, -20.0), (190.0, 84.0, 8.0))]


def belt_downtown(p, lod):
    """downtown tower: a salvaged ship hull stood on its tail (260 m, nose up), welded-on pods and patch plates, a
    lattice scaffolding tower beside it with catwalk bridges, guy cables"""
    rng = random.Random(113)
    seg = {0: 14, 1: 10, 2: 8}[lod]
    prof = [(0, 0), (36.0, 0), (38.0, 30.0), (38.0, 170.0), (32.0, 220.0), (20.0, 252.0), (6.0, 264.0), (0, 266.0)]
    ylathe(p, RUST, prof, seg)
    for k in range(3):                                                         # the tail nozzles as feet
        a = 2 * math.pi * k / 3
        c = (40.0 * math.cos(a), 0.0, 40.0 * math.sin(a))
        ylathe(p, SCRAP, [(0, 0), (14.0, 0), (9.0, 22.0), (0, 22.0)], seg, t=c)
    for k in range(9):                                                         # pods and patches
        a = rng.uniform(0, 2 * math.pi)
        y = rng.uniform(30.0, 200.0)
        if k % 3 == 0:
            r = rng.uniform(6.0, 9.0)
            c = (44.0 * math.cos(a), y, 44.0 * math.sin(a))
            _tank(p, PATCH, (c[0], y - 12.0, c[2]), (c[0], y + 12.0, c[2]), r, lod)
        else:
            box(p, PATCH if k % 2 else SCRAP, (38.5 * math.cos(a), y, 38.5 * math.sin(a)), (rng.uniform(12, 22), rng.uniform(12, 30), 5.0),
                rot_y(-a + math.pi / 2))
    sx, sz = 62.0, 0.0
    truss(p, (sx, 0.0, sz), (sx, 285.0, sz), 5.0, lod, bay=15.0)              # the scaffolding tower
    for y in (60.0, 120.0, 180.0, 240.0):
        r = 38.0 if y < 200 else 26.0
        truss(p, (sx - 5.0, y, sz), (r - 1.0, y, sz), 1.5, lod, bay=5.0, diag=(0, 2))
    for k in range(3):                                                         # guy cables
        a = 2 * math.pi * k / 3 + 1.0
        strut(p, STEEL, (sx, 280.0, sz), (sx + 70.0 * math.cos(a), 0.5, sz + 70.0 * math.sin(a)), 0.35)
    mast(p, 0.0, 0.0, 264.0, 300.0, 1.2, lod)
    if lod < 2:
        beacon(p, (sx, 287.0, sz), 1.6)
        for k in range(12):
            a = rng.uniform(0, 2 * math.pi)
            box(p, WARM, (39.0 * math.cos(a), rng.uniform(20, 200), 39.0 * math.sin(a)), (3.0, 1.2, 3.0))


def belt_downtown_hits():
    n = "hubx_belt_clans_downtown"
    return [hcyl(n, (0, 0, 0), (0, 266.0, 0), 46.0), hbox(n, (62.0, 142.5, 0), (12.0, 285.0, 12.0)),
            hcyl(n, (0, 266.0, 0), (0, 300.0, 0), 2.0)]


def belt_landmark(p, lod):
    """landmark: the clan rock - a big boulder with hab blocks poking out of it, a wrecked ship impaled on top, a
    crude lattice beacon mast with guy cables and clan banners"""
    rng = random.Random(131)
    rock_cap(p, lod, 105.0, 47, 30.0, 0.8)
    for k in range(5):                                                         # rubble piled at the foot
        a = 2 * math.pi * k / 5 + 0.5
        rock_cap(p, 2, rng.uniform(22.0, 34.0), 60 + k, 0.0, 0.8, (112.0 * math.cos(a), 112.0 * math.sin(a)))
    for k in range(9):                                                         # habs half-buried in the rock
        a = 2 * math.pi * k / 9 + rng.uniform(-0.2, 0.2)
        y = rng.uniform(15.0, 90.0)
        rr = math.sqrt(max(0.0, 105.0 ** 2 - ((y - 30.0) / 0.8) ** 2)) * 0.98
        w, h = rng.uniform(24.0, 40.0), rng.uniform(12.0, 24.0)
        box(p, RUST if k % 3 else PATCH, (rr * math.cos(a), y, rr * math.sin(a)), (w, h, 26.0), rot_y(-a + math.pi / 2))
        if lod < 2:
            beacon(p, ((rr + 13.0) * math.cos(a), y + h / 2 + 0.6, (rr + 13.0) * math.sin(a)), 0.8)
    # the wreck: a fuselage stuck into the top at an angle, its bridge, a broken wing
    seg = {0: 12, 1: 8, 2: 6}[lod]
    a0, a1 = (-20.0, 110.0, -10.0), (70.0, 175.0, 30.0)
    cyl(p, SCRAP, a0, a1, 14.0, seg)
    box(p, RUST, (60.0, 175.0, 26.0), (20.0, 10.0, 16.0), rot_y(-0.4))
    box(p, SCRAP, (25.0, 140.0, 8.0), (70.0, 3.0, 18.0), mat_mul(rot_y(0.4), rot_z(0.3)))
    mast(p, -30.0, 20.0, 100.0, 245.0, 3.0, lod)
    for k in range(4):
        a = 2 * math.pi * k / 4 + 0.3
        strut(p, STEEL, (-30.0, 240.0, 20.0), (-30.0 + 120.0 * math.cos(a), 0.5, 20.0 + 120.0 * math.sin(a)), 0.4)
        box(p, SCRAP, (-30.0 + 120.0 * math.cos(a), 2.0, 20.0 + 120.0 * math.sin(a)), (6.0, 4.0, 6.0))
    if lod < 2:
        for k, y in enumerate((150.0, 175.0, 200.0)):                          # clan banners
            box(p, CONT[k], (-30.0, y, 20.0 + 9.0), (0.4, 18.0, 12.0))
        for k in range(16):
            a = rng.uniform(0, 2 * math.pi)
            y = rng.uniform(10.0, 120.0)
            rr = math.sqrt(max(0.0, 105.0 ** 2 - ((y - 30.0) / 0.8) ** 2)) * 1.03
            box(p, WARM if k % 4 else COOL, (rr * math.cos(a), y, rr * math.sin(a)), (3.0, 1.5, 3.0))


def belt_landmark_hits():
    n = "hubx_belt_clans_landmark"
    return [hcyl(n, (0, 0, 0), (0, 125.0, 0), 112.0), hbox(n, (25.0, 142.0, 10.0), (100.0, 70.0, 40.0)),
            hcyl(n, (-30.0, 100.0, 20.0), (-30.0, 245.0, 20.0), 4.0)]


# ================================================================ AMERICAN: Homestead
HS_W = 250.0                    # the houses cluster: 250 x 250 m


def _house(p, rng, lod, x, z, face):
    """one suburban house on a lot centred (x, z), its front (driveway, garage) facing face = +-1 (z): 2-3 storeys of
    clapboard / brick under a gable or hip roof, a garage, a driveway, a porch, a chimney, a tree"""
    w, d = rng.uniform(10.0, 14.0), rng.uniform(9.0, 12.0)
    fl = rng.choice((2, 2, 3))
    h = 4.0 * fl
    wall = rng.choice(SIDING)
    roof = rng.choice(ROOFS)
    kind = rng.random()
    hz = z - face * 2.0
    hx = x - 3.0
    box(p, wall, (hx, h / 2, hz), (w, h, d))
    if kind < 0.6:
        gable(p, roof, hx, h, hz, w + 1.2, d + 1.2, rng.uniform(3.5, 5.0), along_x=rng.random() < 0.7)
    else:
        hip(p, roof, hx, h, hz, w + 1.2, d + 1.2, rng.uniform(3.0, 4.2))
    gx = hx + w / 2 + 3.6
    box(p, WHITE if lod < 2 else wall, (gx, 1.7, hz + face * 1.0), (7.0, 3.4, 7.5))      # the garage
    hz2 = hz + face * 1.0
    if lod < 2:
        gable(p, roof, gx, 3.4, hz2, 7.6, 8.1, 1.6, along_x=False)
        box(p, PAVE, (gx, 0.45, z + face * 11.0), (5.0, 0.1, 13.0))                       # the driveway
    if lod == 0:
        box(p, DARK, (gx, 1.4, hz2 + face * 3.78), (5.4, 2.8, 0.05))                     # the garage door
        box(p, WHITE, (hx - w / 4, 0.4, hz + face * (d / 2 + 1.5)), (5.0, 0.6, 3.0))     # porch
        box(p, WHITE, (hx - w / 4, 3.6, hz + face * (d / 2 + 1.5)), (5.4, 0.3, 3.4))
        if rng.random() < 0.6:
            box(p, BRICK, (hx - w / 2 + 1.5, h + 3.0, hz), (1.4, 6.0, 1.4))              # chimney
        tx, tz = x + rng.uniform(-8.0, 8.0), z + face * rng.uniform(-14.0, -10.0)
        box(p, WOOD, (tx, 1.5, tz), (0.6, 3.0, 0.6))
        box(p, TREE, (tx, 5.0, tz), (rng.uniform(4.0, 6.0), rng.uniform(4.0, 6.0), rng.uniform(4.0, 6.0)), rot_y(rng.uniform(0, 1.5)))


def american_houses(lod):
    """the houses cluster: 250 x 250 m of suburb - three bands of two lot rows facing a lane, a cross street, 24 x 35 m
    lots, ~56 houses, a park with a lattice-leg water tower (the truss), street lamps. LOD2: each lot row merged into
    one long block under one long roof"""
    name = "hubx_american_houses"
    P = {name: Part(name)}
    p = P[name]
    rng = random.Random(151)
    box(p, LAWN, (0, 0.2, 0), (HS_W, 0.4, HS_W))
    lane = 12.0
    lots_x = [-122.0 + 11.6 + 23.2 * i for i in range(5)] + [6.0 + 11.6 + 23.2 * i for i in range(5)]   # 23.2 m lots
    _road_z(p, -6.0, 6.0, -HS_W / 2, HS_W / 2, lod, 0.43)                       # the cross street
    park = (1, 0, 4)                                                           # band, row, first lot index of the park
    count = 0
    for band in range(3):
        zc = -HS_W / 2 + 41.0 + band * 84.0                                    # the lane down the band's middle
        _road_x(p, -HS_W / 2, HS_W / 2, zc - lane / 2, zc + lane / 2, lod, 0.4)
        if lod == 0:
            for x in range(-110, 111, 44):
                lamp(p, float(x), 0.4, zc + lane / 2 + 1.0, 7.0)
        for row, face in ((0, 1), (1, -1)):                                    # row 0 south of the lane, faces +z
            z = zc - face * (lane / 2 + 17.5)
            if lod >= 2:
                for x0, x1 in ((-HS_W / 2 + 3.0, -8.0), (8.0, HS_W / 2 - 3.0)):
                    if (band, row) == park[:2] and x0 > 0:
                        x0 = 76.0                                              # the park leaves the row's end
                    box(p, SIDING[(band + row) % 4], ((x0 + x1) / 2, 4.5, z - face * 2.0), (x1 - x0 - 6.0, 9.0, 11.0))
                    gable(p, ROOFS[(band * 2 + row) % 3], (x0 + x1) / 2, 9.0, z - face * 2.0, x1 - x0 - 5.0, 12.0, 4.0)
                continue
            for i, x in enumerate(lots_x):
                if (band, row) == park[:2] and i >= 5 and i <= 7:
                    continue
                _house(p, random.Random(1000 + band * 100 + row * 20 + i), lod, x, z, face)
                count += 1
    # the park: a water tower on four lattice legs
    zc = -HS_W / 2 + 41.0 + 84.0
    px, pz = 6.0 + 11.6 + 23.2 * 1, zc - (lane / 2 + 17.5)
    box(p, PAVE, (px, 0.45, pz), (16.0, 0.1, 16.0))
    for sx in (-1, 1):
        for sz in (-1, 1):
            truss(p, (px + sx * 9.0, 0.4, pz + sz * 9.0), (px + sx * 4.5, 32.0, pz + sz * 4.5), 0.7, lod, bay=4.0)
    seg = {0: 16, 1: 10, 2: 8}[lod]
    ylathe(p, WHITE, [(0, 0), (4.0, 0), (8.0, 3.0), (8.0, 11.0), (4.0, 14.5), (0, 15.0)], seg, t=(px, 31.0, pz))
    if lod < 2:
        cyl(p, STEEL, (px, 0.4, pz), (px, 31.0, pz), 0.8, 6)
        beacon(p, (px, 46.6, pz), 0.6)
        for k in range(6):                                                     # park trees
            box(p, TREE, (px + (-22.0, -14.0, 20.0)[k % 3], 4.0, pz + (k // 3 - 0.5) * 14.0), (6.0, 6.0, 6.0))
    return P


def american_houses_hits():
    n = "hubx_american_houses"
    H = [hbox(n, (0, 0.2, 0), (HS_W, 0.4, HS_W))]
    for band in range(3):
        zc = -HS_W / 2 + 41.0 + band * 84.0
        for face in (1, -1):
            z = zc - face * 6.0 - face * 17.5 - face * 2.0
            H.append(hbox(n, (-62.0, 8.5, z), (110.0, 17.0, 14.0)))
            H.append(hbox(n, (62.0, 8.5, z), (110.0, 17.0, 14.0)))
    return H


def american_office(p, lod):
    """office: a low office park / strip mall - an L of one-storey shops under a canopy with lit signs, two glass office
    boxes, a parking lot with stalls and lamps, a lattice pylon sign"""
    box(p, ROAD, (0, 0.1, 30.0), (230.0, 0.2, 110.0))                           # the parking lot
    if lod == 0:
        for x in range(-108, 109, 4):
            for z in (5.0, 55.0):
                box(p, MARK, (float(x), 0.22, z), (0.2, 0.04, 9.0))
    if lod < 2:
        for x in range(-100, 101, 40):
            for z in (-5.0, 30.0, 65.0):
                lamp(p, float(x), 0.2, z, 9.0)
    box(p, BRICK, (-30.0, 4.0, -45.0), (170.0, 8.0, 26.0))                     # the strip
    box(p, BRICK, (95.0, 4.0, -15.0), (30.0, 8.0, 86.0))
    box(p, WHITE, (-30.0, 6.5, -29.5), (170.0, 1.0, 6.0))                       # the canopy
    box(p, WHITE, (78.0, 6.5, -15.0), (6.0, 1.0, 86.0))
    if lod < 2:
        for k in range(8):
            box(p, WARM if k % 3 else COOL, (-105.0 + k * 21.0, 7.6, -31.95), (14.0, 1.4, 0.2))   # shop signs
            box(p, WHITE, (-105.0 + k * 21.0 + 10.5, 3.0, -31.0), (0.6, 6.0, 0.6))              # canopy posts
    for x in (-70.0, 10.0):
        box(p, GLASS, (x, 8.0, -95.0), (60.0, 16.0, 36.0))                     # office boxes, 4 floors
        box(p, WHITE, (x, 16.5, -95.0), (62.0, 1.0, 38.0))
        if lod < 2:
            box(p, LIGHT, (x + 15.0, 18.5, -95.0), (10.0, 3.0, 8.0))
    truss(p, (-110.0, 0.2, 80.0), (-110.0, 36.0, 80.0), 1.2, lod, bay=4.0)      # the pylon sign
    box(p, WARM, (-110.0, 40.0, 80.0), (14.0, 8.0, 1.2))
    box(p, WHITE, (-110.0, 44.6, 80.0), (14.6, 1.0, 1.6))
    if lod < 2:
        beacon(p, (-110.0, 45.6, 80.0), 0.6)


def american_office_hits():
    n = "hubx_american_office"
    return [hbox(n, (-30.0, 4.0, -45.0), (170.0, 8.0, 26.0)), hbox(n, (95.0, 4.0, -15.0), (30.0, 8.0, 86.0)),
            hbox(n, (-70.0, 8.5, -95.0), (62.0, 17.0, 38.0)), hbox(n, (10.0, 8.5, -95.0), (62.0, 17.0, 38.0)),
            hbox(n, (0, 0.1, 30.0), (230.0, 0.2, 110.0)), hcyl(n, (-110.0, 0, 80.0), (-110.0, 45.0, 80.0), 7.0)]


def _water_tank(p, x, y, z, lod):
    """a New York rooftop water tank: a wooden barrel with a cone hat on lattice stilts"""
    seg = {0: 10, 1: 8, 2: 6}[lod]
    for sx in (-1, 1):
        for sz in (-1, 1):
            strut(p, STEEL, (x + sx * 2.6, y, z + sz * 2.6), (x + sx * 2.6, y + 6.0, z + sz * 2.6), 0.2)
            if lod == 0:
                strut(p, STEEL, (x + sx * 2.6, y, z + sz * 2.6), (x - sx * 2.6, y + 6.0, z + sz * 2.6), 0.12)
    box(p, STEEL, (x, y + 6.1, z), (6.4, 0.3, 6.4))
    ylathe(p, WOOD, [(0, 0), (3.3, 0), (3.3, 6.5), (3.6, 6.6), (0, 8.8)], seg, t=(x, y + 6.25, z))


def american_apartments(p, lod):
    """residential block: a nine-storey red-brick walk-up round a courtyard (a U of three wings), stone cornices, fire
    escapes, rooftop water tanks on lattice stilts"""
    H = 36.0
    wings = [((0.0, -40.0), (150.0, 22.0)), ((-64.0, 14.0), (22.0, 86.0)), ((64.0, 14.0), (22.0, 86.0))]
    box(p, PAVE, (0, 0.1, 0), (170.0, 0.2, 120.0))
    box(p, LAWN, (0, 0.25, 22.0), (100.0, 0.1, 70.0))
    for (x, z), (w, d) in wings:
        box(p, BRICK, (x, H / 2, z), (w, H, d))
        box(p, STONE, (x, H + 1.0, z), (w + 1.6, 2.0, d + 1.6))                  # the cornice
        box(p, STONE, (x, 2.0, z), (w + 0.6, 4.0, d + 0.6))                       # the stone base
    if lod < 2:
        for x in (-48.0, -16.0, 16.0, 48.0):                                    # fire escapes on the street face
            for y in range(6, int(H), 4):
                box(p, STEEL, (x, float(y), -51.6), (8.0, 0.25, 1.4))
            if lod == 0:
                strut(p, STEEL, (x - 3.5, 4.0, -52.2), (x + 3.5, H - 2.0, -52.2), 0.12)
    for x, z in ((-40.0, -40.0), (30.0, -42.0), (-64.0, 30.0), (64.0, 0.0)):
        _water_tank(p, x, H + 2.0, z, lod)
    for x, z in ((0.0, -40.0), (-64.0, -10.0), (64.0, 40.0)):
        box(p, BRICK, (x, H + 4.0, z), (8.0, 6.0, 6.0))                          # stair bulkheads
    mast(p, 50.0, -40.0, H + 2.0, H + 30.0, 0.8, lod)                           # a radio mast


def american_apartments_hits():
    n = "hubx_american_apartments"
    return [hbox(n, (0.0, 19.0, -40.0), (152.0, 38.0, 24.0)), hbox(n, (-64.0, 19.0, 14.0), (24.0, 38.0, 88.0)),
            hbox(n, (64.0, 19.0, 14.0), (24.0, 38.0, 88.0)), hbox(n, (0, 0.1, 0), (170.0, 0.2, 120.0))]


DECO = [(0.0, 64.0, 92.0), (64.0, 180.0, 70.0), (180.0, 268.0, 54.0), (268.0, 318.0, 38.0)]   # (y0, y1, width) setbacks


def american_downtown(p, lod):
    """downtown tower: an art-deco skyscraper - limestone setbacks (92 / 70 / 54 / 38 m), vertical piers, a stepped gold
    crown and a lattice mooring mast (top ~385 m), floodlights washing the setbacks"""
    for y0, y1, w in DECO:
        box(p, STONE, (0, (y0 + y1) / 2, 0), (w, y1 - y0, w))
        box(p, WHITE, (0, y1 + 0.6, 0), (w + 1.2, 1.2, w + 1.2))
        if lod < 2:
            n = int(w // 9)
            for i in range(n):
                t = -w / 2 + (i + 0.5) * w / n
                for nx, nz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    c = (nx * (w / 2 + 0.4) + (t if nz else 0.0), (y0 + y1) / 2, nz * (w / 2 + 0.4) + (t if nx else 0.0))
                    box(p, WHITE, c, (0.8 if nx else 1.4, y1 - y0, 0.8 if nz else 1.4))
            for sx in (-1, 1):
                for sz in (-1, 1):
                    box(p, FLOOD, (sx * (w / 2 - 2.0), y0 + 0.8 if y0 else 1.0, sz * (w / 2 - 2.0)), (1.6, 0.8, 1.6))
    y = 318.0
    for k, w in enumerate((32.0, 26.0, 20.0, 14.0, 8.0)):                       # the stepped crown
        box(p, GOLD if k % 2 else STEEL, (0, y + 3.0, 0), (w, 6.0, w))
        if lod < 2 and k < 3:
            for sx in (-1, 1):
                for sz in (-1, 1):
                    box(p, GOLD, (sx * w / 2, y + 7.0, sz * w / 2), (1.2, 8.0, 1.2))
        y += 6.0
    truss(p, (0, y, 0), (0, y + 36.0, 0), 2.2, lod, bay=6.0)                    # the mooring mast
    ylathe(p, STEEL, [(0, 0), (1.5, 0), (0.3, 12.0), (0, 12.0)], 6, t=(0, y + 36.0, 0))
    beacon(p, (0, y + 48.6, 0), 1.2)


def american_downtown_hits():
    n = "hubx_american_downtown"
    H = [hbox(n, (0, (y0 + y1) / 2, 0), (w + 2.0, y1 - y0, w + 2.0)) for y0, y1, w in DECO]
    return H + [hbox(n, (0, 333.0, 0), (32.0, 30.0, 32.0)), hcyl(n, (0, 348.0, 0), (0, 397.0, 0), 3.0)]


def _octagon(r):
    return [(r * math.cos(math.pi / 8 + k * math.pi / 4), r * math.sin(math.pi / 8 + k * math.pi / 4)) for k in range(8)]


def american_tower(p, lod):
    """tall residential: a glass condo tower - a stone podium, two octagonal glass shafts (280 / 220 m) joined by a sky
    bridge, an open lattice crown cube over the taller one, a spire"""
    box(p, STONE, (0, 12.0, 0), (110.0, 24.0, 80.0))
    box(p, WHITE, (0, 24.5, 0), (112.0, 1.0, 82.0))
    for x, h in ((-22.0, 280.0), (28.0, 220.0)):
        poly = [(x + a, b) for a, b in _octagon(24.0 if h > 250 else 20.0)]
        prism(p, GLASS, poly, 24.0, h)
        prism(p, WHITE, [(x + a, b) for a, b in _octagon(25.0 if h > 250 else 21.0)], h, h + 2.0)
        if lod < 2:
            for y in range(64, int(h), 40):
                prism(p, WHITE, [(x + a, b) for a, b in _octagon(24.6 if h > 250 else 20.6)], float(y), y + 1.0)
    box(p, GLASS, (3.0, 180.0, 0), (14.0, 8.0, 12.0))                            # the sky bridge
    c, y0, y1 = 18.0, 282.0, 318.0                                               # the lattice crown over the tall shaft
    for sx in (-1, 1):
        for sz in (-1, 1):
            truss(p, (-22.0 + sx * c, y0, sz * c), (-22.0 + sx * c * 0.6, y1, sz * c * 0.6), 1.2, lod, bay=4.0)
    for y, cc in ((y0, c), (y1, c * 0.6)):
        for (ax, az), (bx, bz) in (((-cc, -cc), (cc, -cc)), ((cc, -cc), (cc, cc)), ((cc, cc), (-cc, cc)), ((-cc, cc), (-cc, -cc))):
            truss(p, (-22.0 + ax, y, az), (-22.0 + bx, y, bz), 1.0, min(lod + 1, 2), bay=4.0)
    mast(p, -22.0, 0.0, y1, y1 + 42.0, 1.2, lod)
    if lod < 2:
        for x in (-22.0, 28.0):
            box(p, FLOOD, (x, 25.5, -30.0), (6.0, 1.0, 1.0))


def american_tower_hits():
    n = "hubx_american_tower"
    return [hbox(n, (0, 12.0, 0), (112.0, 24.0, 82.0)), hcyl(n, (-22.0, 24.0, 0), (-22.0, 320.0, 0), 25.0),
            hcyl(n, (28.0, 24.0, 0), (28.0, 222.0, 0), 21.0), hcyl(n, (-22.0, 320.0, 0), (-22.0, 362.0, 0), 2.0)]


def american_landmark(p, lod):
    """landmark: the capitol - a white-stone centre block and two wings, a columned portico under a pediment, a drum
    ringed by columns, a ribbed dome with a lantern and a statue; the mall in front with a reflecting pool and an
    obelisk; lattice floodlight masts light it at night"""
    seg = {0: 28, 1: 18, 2: 12}[lod]
    box(p, PAVE, (0, 0.5, 40.0), (240.0, 1.0, 330.0))                             # the terrace / mall paving
    box(p, LAWN, (-55.0, 1.05, 110.0), (60.0, 0.1, 170.0))
    box(p, LAWN, (55.0, 1.05, 110.0), (60.0, 0.1, 170.0))
    box(p, DARK, (0, 1.1, 110.0), (30.0, 0.2, 140.0))                              # the reflecting pool
    box(p, STONE, (0, 16.0, -70.0), (80.0, 30.0, 60.0))                            # the centre block
    for sx in (-1, 1):
        box(p, STONE, (sx * 77.0, 13.0, -70.0), (74.0, 24.0, 44.0))                # the wings
        box(p, WHITE, (sx * 77.0, 25.5, -70.0), (76.0, 1.0, 46.0))
        gable(p, WHITE, sx * 77.0, 26.0, -48.0 + 2.0, 20.0, 3.0, 5.0)               # the wings' pediments
    box(p, WHITE, (0, 31.5, -70.0), (82.0, 1.0, 62.0))
    for k in range(8):                                                             # the portico
        x = -24.5 + k * 7.0
        cyl(p, WHITE, (x, 1.0, -36.0), (x, 25.0, -36.0), 1.4, 8 if lod < 2 else 6)
    box(p, WHITE, (0, 26.5, -38.0), (56.0, 3.0, 8.0))
    gable(p, WHITE, 0, 28.0, -38.0, 56.0, 8.0, 8.0)
    box(p, STONE, (0, 0.6 + 2.0, -26.0), (56.0, 4.0, 12.0))                         # the steps
    ylathe(p, STONE, [(0, 0), (24.0, 0), (24.0, 22.0), (0, 22.0)], seg, t=(0, 32.0, -70.0))   # the drum
    ylathe(p, WHITE, [(0, 0), (26.0, 0), (26.0, 1.5), (0, 1.5)], seg, t=(0, 54.0, -70.0))
    if lod < 2:
        n = 24 if lod == 0 else 12
        for k in range(n):
            a = 2 * math.pi * k / n
            cyl(p, WHITE, (26.0 * math.cos(a), 32.0, -70.0 + 26.0 * math.sin(a)),
                (26.0 * math.cos(a), 54.0, -70.0 + 26.0 * math.sin(a)), 0.9, 6)
    prof = [(0, 0)] + [(23.0 * math.cos(t), 30.0 * math.sin(t)) for t in [k * math.pi / 2 / 8 for k in range(9)]]
    ylathe(p, WHITE, prof, seg, t=(0, 55.5, -70.0), soft=30)                       # the dome
    ylathe(p, WHITE, [(0, 0), (4.0, 0), (4.0, 9.0), (0, 13.0)], seg // 2, t=(0, 85.0, -70.0))   # the lantern
    ylathe(p, STEEL, [(0, 0), (1.0, 0), (0.6, 8.0), (0, 8.0)], 6, t=(0, 98.0, -70.0))           # the statue
    box(p, STONE, (0, 2.0, 190.0), (14.0, 2.0, 14.0))                              # the obelisk's base
    ylathe(p, WHITE, [(0, 0), (4.2, 0), (2.6, 58.0), (0, 63.0)], 4, t=(0, 3.0, 190.0))
    for x, z in ((-110.0, -20.0), (110.0, -20.0), (-110.0, 160.0), (110.0, 160.0)):   # floodlight masts
        mast(p, x, z, 1.0, 46.0, 1.2, lod, top=False)
        box(p, FLOOD, (x, 47.5, z), (5.0, 2.0, 1.2))
    for sx in (-1, 1):                                                             # flagpoles
        cyl(p, STEEL, (sx * 30.0, 1.0, -10.0), (sx * 30.0, 30.0, -10.0), 0.3, 4)
        if lod < 2:
            box(p, CONT[0] if sx < 0 else CONT[1], (sx * 30.0 + 3.0, 27.0, -10.0), (6.0, 3.6, 0.1))


def american_landmark_hits():
    n = "hubx_american_landmark"
    return [hbox(n, (0, 16.0, -70.0), (80.0, 32.0, 60.0)), hbox(n, (-77.0, 13.0, -70.0), (74.0, 26.0, 44.0)),
            hbox(n, (77.0, 13.0, -70.0), (74.0, 26.0, 44.0)), hcyl(n, (0, 32.0, -70.0), (0, 106.0, -70.0), 26.0),
            hbox(n, (0, 0.5, 40.0), (240.0, 1.0, 330.0)), hcyl(n, (0, 1.0, 190.0), (0, 64.0, 190.0), 4.2)]


# ================================================================ registration
KIT = {
    "hubx_practical_tower": (practical_tower, practical_tower_hits, "Бетонная жилая башня с бронепоясами (практичный стиль)"),
    "hubx_practical_block": (practical_block, practical_block_hits, "Бункерные корпуса и контейнерное общежитие (практичный стиль)"),
    "hubx_practical_office": (practical_office, practical_office_hits, "Ступенчатый штабной корпус с мачтами связи (практичный стиль)"),
    "hubx_practical_downtown": (practical_downtown, practical_downtown_hits, "Башня в решётчатом экзоскелете (практичный стиль)"),
    "hubx_practical_landmark": (practical_landmark, practical_landmark_hits, "Бастион с радарной вышкой (практичный стиль)"),
    "hubx_belt_clans_tower": (belt_tower, belt_tower_hits, "Свалочная башня из цистерн на ферме (кланы пояса)"),
    "hubx_belt_clans_block": (belt_block, belt_block_hits, "Штабели жилых цистерн с лесами (кланы пояса)"),
    "hubx_belt_clans_office": (belt_office, belt_office_hits, "Клановый рынок под каменным куполом (кланы пояса)"),
    "hubx_belt_clans_downtown": (belt_downtown, belt_downtown_hits, "Корпус корабля, поставленный на корму (кланы пояса)"),
    "hubx_belt_clans_landmark": (belt_landmark, belt_landmark_hits, "Клановая скала с обломком корабля (кланы пояса)"),
    "hubx_american_houses": (None, american_houses_hits, "Квартал коттеджей 250x250 (американский стиль)"),
    "hubx_american_apartments": (american_apartments, american_apartments_hits, "Кирпичный дом с баками на крыше (американский стиль)"),
    "hubx_american_office": (american_office, american_office_hits, "Офисный парк и стрип-молл (американский стиль)"),
    "hubx_american_downtown": (american_downtown, american_downtown_hits, "Небоскрёб ар-деко (американский стиль)"),
    "hubx_american_tower": (american_tower, american_tower_hits, "Стеклянная жилая башня (американский стиль)"),
    "hubx_american_landmark": (american_landmark, american_landmark_hits, "Капитолий с аллеей и обелиском (американский стиль)"),
}
ASSETS = {k: (_one(k, v[0]) if v[0] else american_houses) for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Модуль хаба в национальном / политическом стиле (tools/models/units/hub_style_west.py).")
             for k, v in KIT.items()}

# the modules a hub of the style packs into its zones (keys and weights as hub_plane.ZONE_MODULES; default modules mixed
# in; the landmark once in a while in the downtown)
STYLE_ZONE_MODULES = {
    "practical": {
        "downtown": [("hubx_practical_downtown", 3), ("hubx_practical_tower", 3), ("hubx_practical_office", 2),
                     ("hubx_practical_landmark", 1), ("hubk_hab_b", 1)],
        "residential": [("hubx_practical_block", 3), ("hubx_practical_tower", 2), ("hubk_hab_c", 1), ("hubb_hab_slab", 1)],
        "hotel": [("hubx_practical_block", 1), ("hubx_practical_office", 1), ("hubb_hab_slab", 1)],
    },
    "belt_clans": {
        "downtown": [("hubx_belt_clans_downtown", 3), ("hubx_belt_clans_tower", 3), ("hubx_belt_clans_office", 1),
                     ("hubx_belt_clans_landmark", 1)],
        "residential": [("hubx_belt_clans_block", 4), ("hubx_belt_clans_tower", 2), ("hubk_hab_c", 1)],
        "hotel": [("hubx_belt_clans_block", 2), ("hubx_belt_clans_office", 1), ("hubv_ring_wheel", 1)],
    },
    "american": {
        "downtown": [("hubx_american_downtown", 3), ("hubx_american_tower", 3), ("hubx_american_office", 1),
                     ("hubx_american_landmark", 1), ("hubb_hab_tower", 1)],
        "residential": [("hubx_american_houses", 6), ("hubx_american_apartments", 2), ("hubx_american_office", 1)],
        "hotel": [("hubx_american_apartments", 2), ("hubx_american_office", 1), ("hubx_american_tower", 1)],
    },
}
STYLE_LANDMARK = {"practical": "hubx_practical_landmark", "belt_clans": "hubx_belt_clans_landmark",
                  "american": "hubx_american_landmark"}

# which polities build in which style. Sources: src/assets/data/economy/economy_rules.json blocs_members (citadel_compact:
# proxima_outpost, wolf359_citadel; free_belts: eps_eri_strife, gj581_freeclans), tools/colonization_lab/canon/world.json
# founding missions + missions.json cultures (wolf359_citadel <- IRON-SHIELD-1, nordic_pmc 0.35, a Nordic PMC "Skjold";
# cyg61_kern "Проспекторат Керн" <- RESOURCER-1, NeoMining Corp - a corporate mining prospectorate; eps_eri_strife
# "Варпстанис" <- FREE-VENTURE-1, crypto_libertarian 0.8 + american_frontier 0.2; gj581_freeclans <- FREE-WORLD-1,
# crypto_libertarian 0.6; hd20794_hellas2 "Хомстед" <- HOMESTEAD-1, the North American consortium: american_frontier
# 0.6, canadian 0.2, mexican 0.2). proxima_outpost (Меру, MERU-1: russian / indian / central_asian) is in the Citadel
# compact, so a practical hub by default; tau_ceti_cognium (MAYFLOWER-2 american 0.85 + GENESIS-1 chinese diaspora)
# is left to the ultra-modern style.
STYLE_POLITIES = {
    "practical": ["wolf359_citadel", "proxima_outpost", "cyg61_kern"],
    "belt_clans": ["eps_eri_strife", "gj581_freeclans"],
    "american": ["hd20794_hellas2"],
}
