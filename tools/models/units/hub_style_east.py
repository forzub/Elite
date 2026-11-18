"""National / political city styles for the planar hubs, the "east" share (author 2026-11-18: «сгенерируй модули для хабов
для других политических систем согласно их духу. сейчас постройки довольно изящные, где-то сделать тупо практичные, где-то
с намеком на древнюю архитектуру, Китайских потомков можно селить в ультрасовременные здания ... Может какие-то здания
будут в национальном стиле. главное, что бы по прибытию все эти хабы визуально различались, но что бы имели одинаковый
слой механики» + «фермы везде обязательно, хотя бы в качестве мачт или опорных столбов»).

Three styles, five modules each, covering the functional roles of the default city kit (hub_plane.py ZONE_MODULES):
tower (tall residential), slab (mid-rise residential), office, downtown (mixed tower), landmark.

  ultramodern  the Chinese-descended polities: twisted / tapered glass megatowers, twin towers on a truss sky bridge,
               LED crowns on truss cages, a "pearl" TV tower on a lattice tripod
  ancient      the Hellas-type polities: a Pharos-like tiered tower, terraced arcaded housing, a stoa office, an agora
               quad with a campanile, a temple on an acropolis podium; marble-white and warm stone
  indian       the Indian-descended polities: a cruciform shikhara tower, a haveli courtyard block with jharokhas and
               chhatris, a jali-screened office, a gopuram-like stepped downtown tower, a domed mausoleum-landmark with
               minarets and a charbagh forecourt

Frame of every module: base at y 0, +Y up, footprint centred near the origin (fits a 600 m block, usable ~520 m); the
same modules hang upside down under the city plane. Walls are eq_hub_facade_* materials (the shader draws their lit
windows); columns, domes, roofs are plain materials (no windows). Every module carries visible lattice trusses (masts,
crown cages, sky-bridge girders, courtyard roofs, lamp pylons) - the shared engineering layer under the national skin.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_mega import FACADE, FACADE_L  # noqa: F401
from units.hub_belt import sphere  # (hub_belt first: it imports hub_vista itself)
from units.hub_vista import FLARE, lattice_column, ring_y

register_materials({
    # facades (the shader keys on the eq_hub_facade prefix: lit windows)
    "eq_hub_facade_um_glass": ((0.12, 0.18, 0.24), (0.60, 0.62, 0.66), 120),
    "eq_hub_facade_um_silver": ((0.44, 0.48, 0.52), (0.50, 0.52, 0.55), 100),
    "eq_hub_facade_um_rose": ((0.52, 0.24, 0.34), (0.55, 0.50, 0.52), 110),
    "eq_hub_facade_an_marble": ((0.80, 0.77, 0.70), (0.16, 0.16, 0.15), 30),
    "eq_hub_facade_an_stone": ((0.64, 0.53, 0.40), (0.10, 0.10, 0.09), 20),
    "eq_hub_facade_in_sandstone": ((0.60, 0.33, 0.23), (0.10, 0.09, 0.08), 20),
    "eq_hub_facade_in_ivory": ((0.82, 0.75, 0.62), (0.14, 0.13, 0.12), 30),
    # plain materials (no windows): columns, roofs, domes, metal
    "eq_hubx_um_chrome": ((0.58, 0.61, 0.65), (0.80, 0.80, 0.85), 140),
    "eq_hubx_an_marble": ((0.88, 0.86, 0.80), (0.20, 0.20, 0.19), 40),
    "eq_hubx_an_terracotta": ((0.58, 0.29, 0.17), (0.08, 0.07, 0.06), 15),
    "eq_hubx_an_bronze": ((0.30, 0.46, 0.40), (0.35, 0.40, 0.38), 60),
    "eq_hubx_in_sandstone": ((0.66, 0.38, 0.26), (0.08, 0.07, 0.06), 15),
    "eq_hubx_in_marble": ((0.91, 0.90, 0.87), (0.25, 0.25, 0.24), 50),
    "eq_hubx_in_gold": ((0.80, 0.62, 0.22), (0.80, 0.70, 0.40), 120),
    "eq_hubx_in_shadow": ((0.20, 0.13, 0.10), (0.05, 0.05, 0.05), 10),
})
UM_GLASS, UM_SILVER, UM_ROSE, CHROME = ("eq_hub_facade_um_glass", "eq_hub_facade_um_silver", "eq_hub_facade_um_rose",
                                        "eq_hubx_um_chrome")
AN_MARBLE_F, AN_STONE_F, MARBLE, TERRA, BRONZE = ("eq_hub_facade_an_marble", "eq_hub_facade_an_stone", "eq_hubx_an_marble",
                                                  "eq_hubx_an_terracotta", "eq_hubx_an_bronze")
IN_SAND_F, IN_IVORY_F, SAND, IN_MARBLE, GOLD, SHADOW = ("eq_hub_facade_in_sandstone", "eq_hub_facade_in_ivory",
                                                        "eq_hubx_in_sandstone", "eq_hubx_in_marble", "eq_hubx_in_gold",
                                                        "eq_hubx_in_shadow")


# ---------------------------------------------------------------- small vector helpers
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _len(a): return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])
def _unit(a): return _mul(a, 1.0 / max(_len(a), 1e-9))


def _seg(lod, hi=24):
    return {0: hi, 1: max(10, hi * 2 // 3)}.get(lod, max(8, hi // 3))


# ---------------------------------------------------------------- shared builders: trusses, masts, gables, polygons
def girder(p, a, b, h, lod, mat=STEEL, bay=None):
    """a square lattice girder from a to b (half depth h): 4 chords, a ring + one zigzag diagonal per face every bay"""
    d = _sub(b, a)
    L = _len(d)
    dn = _unit(d)
    up = (0.0, 1.0, 0.0) if abs(dn[1]) < 0.9 else (1.0, 0.0, 0.0)
    u = _unit(_cross(dn, up))
    v = _cross(u, dn)
    cs = 6 if lod == 0 else 4
    rc = max(0.3, 0.13 * h)
    rb = rc * 0.6
    cn = [_add(_mul(u, sx * h), _mul(v, sy * h)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for c in cn:
        cyl(p, mat, _add(a, c), _add(b, c), rc, cs)
    if lod >= 2:
        return
    bays = max(1, min(24, int(round(L / ((bay or 2.0 * h) * (1 if lod == 0 else 2))))))
    for k in range(bays + 1):
        q = _add(a, _mul(d, k / bays))
        for i in range(4):
            cyl(p, mat, _add(q, cn[i]), _add(q, cn[(i + 1) % 4]), rb, 4)
        if k < bays:
            q1 = _add(a, _mul(d, (k + 1) / bays))
            for i in range(4):
                cyl(p, mat, _add(q, cn[i]), _add(q1, cn[(i + 1) % 4]), rb * 0.8, 4)


def mast(p, x, z, y0, y1, w, lod, taper=0.45, top=True):
    """a tapering square lattice mast (half width w at the foot) with a beacon on top"""
    H = y1 - y0
    bays = max(1, int(round(H / (2.2 * w)))) if lod == 0 else max(1, int(round(H / (4.4 * w))))
    if lod >= 2:
        for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            cyl(p, STEEL, (x + sx * w, y0, z + sz * w), (x + sx * w * taper, y1, z + sz * w * taper), max(0.3, 0.12 * w), 4)
    else:
        lattice_column(p, STEEL, y0, y1, w, w * taper, bays, chord=max(0.3, 0.12 * w), brace=max(0.2, 0.07 * w),
                       cs=6 if lod == 0 else 4, cx=x, cz=z)
    if top:
        beacon(p, (x, y1 + 0.8, z), max(0.8, 0.25 * w))


def gable(p, mat, c, w, l, h, along_x=True):
    """a triangular prism (a pitched roof / pediment): base w across, l long, the ridge h above c[1]"""
    cx, y0, cz = c

    def P(a, s, y):
        return (cx + a, y, cz + s) if along_x else (cx + s, y, cz + a)

    def N(a, s, y):
        return (a, y, s) if along_x else (s, y, a)
    hl, hw = l / 2, w / 2
    A0, B0, C0 = P(-hl, -hw, y0), P(-hl, hw, y0), P(-hl, 0, y0 + h)
    A1, B1, C1 = P(hl, -hw, y0), P(hl, hw, y0), P(hl, 0, y0 + h)
    for tri, n in (((A0, B0, C0), N(-1, 0, 0)), ((A1, B1, C1), N(1, 0, 0))):
        p.face(mat, [p.vert(q, n) for q in tri], n)
    for quad, n in (((A0, A1, C1, C0), norm(N(0, -h, hw))), ((B0, B1, C1, C0), norm(N(0, h, hw)))):
        p.face(mat, [p.vert(q, n) for q in quad], n)
    n = (0.0, -1.0, 0.0)
    p.face(mat, [p.vert(q, n) for q in (A0, A1, B1, B0)], n)


def octagon(cx, cz, half, ch):
    """a chamfered square (x, z) polygon: half width half, corner chamfer ch"""
    a, b = half, half - ch
    return [(cx + x, cz + z) for x, z in ((b, -a), (a, -b), (a, b), (b, a), (-b, a), (-a, b), (-a, -b), (-b, -a))]


def stadium(cx, cz, L, W, n):
    """a convex rounded-end (x, z) polygon L long (X) and W wide"""
    r = W / 2
    pts = []
    for k in range(n + 1):
        a = -math.pi / 2 + math.pi * k / n
        pts.append((cx + L / 2 - r + r * math.cos(a), cz + r * math.sin(a)))
    for k in range(n + 1):
        a = math.pi / 2 + math.pi * k / n
        pts.append((cx - L / 2 + r + r * math.cos(a), cz + r * math.sin(a)))
    return pts


def colonnade(p, mat, a, b, y0, h, r, pitch, lod, cap=True):
    """a row of columns from point a to b (x, z) standing on y0, h tall, radius r; base + capital blocks at lod 0"""
    L = math.hypot(b[0] - a[0], b[1] - a[1])
    n = max(1, int(round(L / (pitch * (1 if lod == 0 else 2)))))
    seg = {0: 10, 1: 6}.get(lod, 4)
    for k in range(n + 1):
        t = k / n
        x, z = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        cyl(p, mat, (x, y0, z), (x, y0 + h, z), r, seg)
        if cap and lod == 0:
            box(p, mat, (x, y0 + h - 0.4 * r, z), (2.6 * r, 0.8 * r, 2.6 * r))
            box(p, mat, (x, y0 + 0.3 * r, z), (2.4 * r, 0.6 * r, 2.4 * r))


def ring_cols(p, mat, cx, cz, R, n, y0, h, r, lod):
    seg = {0: 8, 1: 6}.get(lod, 4)
    n = n if lod < 2 else max(4, n // 2)
    for k in range(n):
        a = 2 * math.pi * k / n
        x, z = cx + R * math.cos(a), cz + R * math.sin(a)
        cyl(p, mat, (x, y0, z), (x, y0 + h, z), r, seg)


def dome(p, mat, c, r, seg, rise=1.0):
    """a hemispherical dome (rise: height / radius) on c"""
    n = max(4, seg // 3)
    prof = [(r * math.cos(0.5 * math.pi * k / n), c[1] + rise * r * math.sin(0.5 * math.pi * k / n)) for k in range(n + 1)]
    prof[-1] = (0.0, c[1] + rise * r)
    ylathe(p, mat, [(0.0, c[1])] + prof, seg, t=(c[0], 0, c[2]), soft=50)


def onion(p, mat, c, r, seg):
    """an onion dome (Mughal) on a base circle of radius r at c: bulges to 1.18 r, pinches to a point at 1.85 r"""
    f = [(1.0, 0.0), (1.10, 0.22), (1.18, 0.50), (1.12, 0.80), (0.92, 1.10), (0.62, 1.36), (0.30, 1.58), (0.10, 1.74),
         (0.0, 1.85)]
    if seg <= 10:
        f = [f[0], f[2], f[4], f[6], f[8]]
    ylathe(p, mat, [(0.0, c[1])] + [(r * a, c[1] + r * b) for a, b in f], seg, t=(c[0], 0, c[2]), soft=60)


def chhatri(p, x, y, z, s, lod, mat=SAND, dmat=None):
    """a chhatri: four pillars, a slab, a dome and a gold finial; s = its width"""
    seg = {0: 12, 1: 8}.get(lod, 6)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(p, mat, (x + sx * 0.4 * s, y + 0.45 * s, z + sz * 0.4 * s), (0.12 * s, 0.9 * s, 0.12 * s))
    else:
        box(p, mat, (x, y + 0.45 * s, z), (0.7 * s, 0.9 * s, 0.7 * s))
    box(p, mat, (x, y + 0.97 * s, z), (1.1 * s, 0.14 * s, 1.1 * s))
    onion(p, dmat or mat, (x, y + 1.04 * s, z), 0.38 * s, seg)
    if lod == 0:
        cyl(p, GOLD, (x, y + 1.04 * s + 0.68 * s, z), (x, y + 1.04 * s + 0.95 * s, z), 0.04 * s + 0.15, 6)


# ================================================================ ULTRAMODERN (Chinese-descended polities)
def um_tower_parts(lod):
    """a twisted tapering glass tower: 320 m shaft turning 72 degrees, chrome mechanical bands, an LED crown on a truss
    cage, a chrome spire (top 398 m)"""
    name = "hubx_ultramodern_tower"
    P = {name: Part(name)}
    t = P[name]
    box(t, UM_SILVER, (0, 9.0, 0), (100.0, 18.0, 100.0))                                  # podium
    box(t, CHROME, (0, 18.6, 0), (104.0, 1.2, 104.0))
    N = {0: 40, 1: 20}.get(lod, 10)
    y0, y1 = 18.0, 338.0
    sh = (y1 - y0) / N
    side = lambda u: 62.0 - 24.0 * u
    for k in range(N):
        u = (k + 0.5) / N
        R = rot_y(math.radians(72.0 * u))
        box(t, UM_GLASS, (0, y0 + sh * (k + 0.5), 0), (side(u), sh, side(u)), R)
        if lod < 2 and k % 4 == 3:
            box(t, CHROME, (0, y0 + sh * (k + 1), 0), (side(u) + 1.6, 1.2, side(u) + 1.6), R)
    Rt = rot_y(math.radians(72.0))
    box(t, CHROME, (0, y1 + 1.0, 0), (side(1.0) + 3.0, 2.0, side(1.0) + 3.0), Rt)        # crown deck
    # the crown: a truss cage tapering to the spire, LED rings around it
    if lod < 2:
        lattice_column(t, STEEL, y1 + 2.0, 372.0, 17.0, 5.0, 4 if lod == 0 else 2, chord=0.9, brace=0.5,
                       cs=6 if lod == 0 else 4)
        for y, r in ((346.0, 15.6), (356.0, 11.6), (366.0, 7.6)):
            ring_y(t, COOL, (0, y, 0), r, 0.5, 1.4, _seg(lod, 32))
    else:
        ylathe(t, STEEL, [(0, y1 + 2.0), (16.0, y1 + 2.0), (5.0, 372.0), (0, 372.0)], 4)
    cyl(t, CHROME, (0, 372.0, 0), (0, 396.0, 0), 1.6, 8 if lod < 2 else 4)
    beacon(t, (0, 397.0, 0), 2.0)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(t, FLOOD, (sx * 49.0, 19.5, sz * 49.0), (3.0, 1.0, 3.0))                  # podium uplights
    return P


def um_tower_hits():
    n = "hubx_ultramodern_tower"
    return [hbox(n, (0, 9.0, 0), (104.0, 19.0, 104.0)), hbox(n, (0, 178.0, 0), (66.0, 320.0, 66.0)),
            hcyl(n, (0, 338.0, 0), (0, 372.0, 0), 18.0, 5.0), hcyl(n, (0, 372.0, 0), (0, 398.0, 0), 2.0)]


def um_slab_parts(lod):
    """a sleek residential slab: a 200 x 40 m stadium plan, 96 m, chrome floor bands, a truss sky-garden pergola with an
    LED edge on the roof"""
    name = "hubx_ultramodern_slab"
    P = {name: Part(name)}
    s = P[name]
    n = {0: 8, 1: 5}.get(lod, 3)
    box(s, CHROME, (0, 2.0, 0), (210.0, 4.0, 50.0))
    prism(s, UM_GLASS, stadium(0, 0, 200.0, 40.0, n), 4.0, 96.0)
    for y in ((28.0, 52.0, 76.0) if lod < 2 else (52.0,)):
        prism(s, CHROME, stadium(0, 0, 202.4, 42.4, n), y - 0.8, y + 0.8)
    prism(s, UM_SILVER, stadium(0, 0, 202.0, 42.0, n), 96.0, 99.0)                          # roof edge
    # sky garden: two long girders on lattice posts, cross girders
    for z in (-13.0, 13.0):
        girder(s, (-88.0, 112.0, z), (88.0, 112.0, z), 2.0, lod, bay=8.0)
        for x in ((-80.0, 0.0, 80.0) if lod < 2 else (-80.0, 80.0)):
            mast(s, x, z, 99.0, 110.0, 1.6, lod, taper=1.0, top=False)
    if lod < 2:
        for x in (-80.0, -40.0, 0.0, 40.0, 80.0):
            girder(s, (x, 112.0, -13.0), (x, 112.0, 13.0), 1.6, lod, bay=6.0)
        prism(s, COOL, stadium(0, 0, 203.0, 43.0, n), 97.6, 98.4)                         # LED edge line
    beacon(s, (88.0, 115.0, 13.0), 1.4)
    beacon(s, (-88.0, 115.0, -13.0), 1.4)
    return P


def um_slab_hits():
    n = "hubx_ultramodern_slab"
    return [hbox(n, (0, 50.0, 0), (210.0, 100.0, 50.0)), hbox(n, (0, 106.0, 0), (182.0, 12.0, 32.0))]


def um_office_parts(lod):
    """twin office towers (chamfered octagon plans, two setbacks) on a glass podium, joined at 170 m by a truss sky
    bridge on V-struts; a lattice spire on each tower (top 300 m)"""
    name = "hubx_ultramodern_office"
    P = {name: Part(name)}
    o = P[name]
    box(o, UM_SILVER, (0, 12.0, 0), (170.0, 24.0, 70.0))
    box(o, CHROME, (0, 24.6, 0), (172.0, 1.2, 72.0))
    for sx in (-1, 1):
        x = sx * 48.0
        prism(o, UM_GLASS, octagon(x, 0, 24.0, 7.0), 24.0, 170.0)
        prism(o, UM_GLASS, octagon(x, 0, 20.0, 6.0), 170.0, 222.0)
        prism(o, UM_GLASS, octagon(x, 0, 15.0, 4.5), 222.0, 250.0)
        for y, h in ((170.0, 24.0), (222.0, 20.0), (250.0, 15.0)):
            prism(o, CHROME, octagon(x, 0, h + 0.8, 7.0 * h / 24.0), y - 0.6, y + 0.6)
        if lod < 2:
            ring_y(o, COOL, (x, 248.0, 0), 15.6, 0.4, 1.0, _seg(lod, 24))
        mast(o, x, 0, 250.0, 298.0, 4.0, lod, taper=0.2)
    # the sky bridge: a two-storey glass tube inside a truss, V struts down to the towers at 120 m
    box(o, UM_SILVER, (0, 170.0, 0), (48.0, 8.0, 10.0))
    girder(o, (-24.0, 170.0, 0), (24.0, 170.0, 0), 6.0, lod, bay=8.0)
    for sx in (-1, 1):
        girder(o, (sx * 24.0, 120.0, 0), (0, 164.0, 0), 2.0, lod, bay=8.0)
    if lod < 2:
        for sx in (-1, 1):
            box(o, FLOOD, (sx * 24.5, 26.0, 36.0), (6.0, 1.0, 1.0))
    return P


def um_office_hits():
    n = "hubx_ultramodern_office"
    H = [hbox(n, (0, 12.0, 0), (172.0, 25.0, 72.0)), hbox(n, (0, 170.0, 0), (48.0, 14.0, 14.0))]
    for sx in (-1, 1):
        H += [hbox(n, (sx * 48.0, 137.0, 0), (48.0, 226.0, 48.0)), hcyl(n, (sx * 48.0, 250.0, 0), (sx * 48.0, 299.0, 0), 4.0, 1.0)]
    return H


def um_downtown_parts(lod):
    """a megatall round tower (Shanghai-like): a tapering glass cylinder 46 -> 22 m radius with chrome sky-lobby rings
    every 60 m, a truss crown cage with LED rings, top 396 m"""
    name = "hubx_ultramodern_downtown"
    P = {name: Part(name)}
    d = P[name]
    seg = _seg(lod, 28)
    ylathe(d, UM_SILVER, [(0, 0), (58.0, 0), (58.0, 18.0), (0, 18.0)], seg)                    # podium
    r = lambda y: 46.0 - 24.0 * (y - 18.0) / (352.0 - 18.0)
    lob = [18.0, 78.0, 138.0, 198.0, 258.0, 318.0, 352.0]
    for y0, y1 in zip(lob, lob[1:]):
        ylathe(d, UM_GLASS, [(0, y0), (r(y0) - 1.5, y0), (r(y1), y1 - 3.0), (r(y1) - 1.5, y1), (0, y1)], seg)
        ylathe(d, CHROME, [(0, y1 - 3.0), (r(y1) + 1.2, y1 - 3.0), (r(y1) + 1.2, y1), (0, y1)], seg)
    if lod < 2:
        lattice_column(d, STEEL, 352.0, 384.0, 14.0, 4.0, 4 if lod == 0 else 2, chord=0.9, brace=0.5,
                       cs=6 if lod == 0 else 4)
        for y, rr in ((360.0, 12.0), (370.0, 8.6), (379.0, 5.6)):
            ring_y(d, COOL, (0, y, 0), rr, 0.5, 1.2, _seg(lod, 28))
        for k in range(8):                                                                 # podium uplights
            a = 2 * math.pi * k / 8
            box(d, FLOOD, (57.0 * math.cos(a), 19.0, 57.0 * math.sin(a)), (2.5, 1.0, 2.5))
    else:
        ylathe(d, STEEL, [(0, 352.0), (14.0, 352.0), (4.0, 384.0), (0, 384.0)], 4)
    cyl(d, CHROME, (0, 384.0, 0), (0, 396.0, 0), 1.4, 6)
    beacon(d, (0, 397.0, 0), 2.0)
    return P


def um_downtown_hits():
    n = "hubx_ultramodern_downtown"
    return [hcyl(n, (0, 0, 0), (0, 18.0, 0), 58.0), hcyl(n, (0, 18.0, 0), (0, 352.0, 0), 47.0, 23.0),
            hcyl(n, (0, 352.0, 0), (0, 397.0, 0), 14.0, 2.0)]


def um_landmark_parts(lod):
    """a "pearl" TV tower: three columns braced by truss rings on three inclined legs, a big rose glass sphere at 100 m,
    five small ones up the shaft, an upper sphere at 262 m, a lattice spire (top 398 m)"""
    name = "hubx_ultramodern_landmark"
    P = {name: Part(name)}
    m = P[name]
    seg = _seg(lod, 24)
    ylathe(m, UM_SILVER, [(0, 0), (62.0, 0), (62.0, 4.0), (40.0, 8.0), (0, 8.0)], seg)       # plaza disc
    cols = [(14.0 * math.cos(2 * math.pi * k / 3 + math.pi / 2), 14.0 * math.sin(2 * math.pi * k / 3 + math.pi / 2))
            for k in range(3)]
    cs = 12 if lod == 0 else 8 if lod == 1 else 6
    for x, z in cols:
        cyl(m, CHROME, (x, 8.0, z), (x, 300.0, z), 6.0, cs)
        cyl(m, CHROME, (x * 3.6, 6.0, z * 3.6), (x, 92.0, z), 4.0, cs)                    # the inclined legs
    for y in ((40.0, 140.0, 200.0, 290.0) if lod < 2 else (140.0, 290.0)):                # truss bracing rings
        for i in range(3):
            a, b = cols[i], cols[(i + 1) % 3]
            girder(m, (a[0], y, a[1]), (b[0], y, b[1]), 2.4, lod, bay=6.0)
    if lod < 2:
        for i in range(3):                                                                 # trusses leg -> column
            x, z = cols[i]
            girder(m, (x * 3.0, 26.0, z * 3.0), (x * 1.3, 26.0, z * 1.3), 2.0, lod, bay=6.0)
    sphere(m, UM_ROSE, (0, 100.0, 0), 30.0, seg)
    ring_y(m, COOL, (0, 100.0, 0), 30.0, 0.8, 1.6, seg)
    for y in ((150.0, 170.0, 190.0, 210.0, 230.0) if lod < 2 else (170.0, 210.0)):
        sphere(m, UM_ROSE, (0, y, 0), 9.0, max(8, seg // 2))
    sphere(m, UM_ROSE, (0, 262.0, 0), 18.0, seg)
    ring_y(m, COOL, (0, 262.0, 0), 18.0, 0.6, 1.2, seg)
    ylathe(m, CHROME, [(0, 300.0), (12.0, 300.0), (12.0, 304.0), (0, 304.0)], seg)
    mast(m, 0, 0, 304.0, 372.0, 5.0, lod, taper=0.25, top=False)
    cyl(m, CHROME, (0, 372.0, 0), (0, 396.0, 0), 1.2, 6)
    beacon(m, (0, 397.0, 0), 2.0)
    return P


def um_landmark_hits():
    n = "hubx_ultramodern_landmark"
    return [hcyl(n, (0, 0, 0), (0, 8.0, 0), 62.0), hcyl(n, (0, 8.0, 0), (0, 300.0, 0), 22.0),
            hcyl(n, (0, 70.0, 0), (0, 130.0, 0), 30.0), hcyl(n, (0, 244.0, 0), (0, 280.0, 0), 18.0),
            hcyl(n, (0, 300.0, 0), (0, 397.0, 0), 6.0, 1.5)]


# ================================================================ ANCIENT (Hellas-type polities)
def an_tower_parts(lod):
    """a Pharos-like tiered residential tower: a peristyle podium, a square tier to 160 m, an octagonal one to 240 m, a
    round one with a tholos and a bronze dome, a lantern of trusses with a fire on top (360 m)"""
    name = "hubx_ancient_tower"
    P = {name: Part(name)}
    t = P[name]
    seg = _seg(lod, 24)
    for k, w in enumerate((112.0, 106.0, 100.0)):                                          # stepped stylobate
        box(t, MARBLE, (0, 1.5 + 3.0 * k, 0), (w, 3.0, w))
    box(t, AN_MARBLE_F, (0, 84.5, 0), (70.0, 151.0, 70.0))                                 # square tier
    c4 = ((-46.0, -46.0), (46.0, -46.0), (46.0, 46.0), (-46.0, 46.0))                    # peristyle 9..39 m
    for i in range(4):
        a, b = c4[i], c4[(i + 1) % 4]
        colonnade(t, MARBLE, a, (a[0] + (b[0] - a[0]) * 0.9, a[1] + (b[1] - a[1]) * 0.9), 9.0, 28.0, 2.2, 9.2, lod)
    box(t, MARBLE, (0, 39.5, 0), (98.0, 5.0, 98.0))                                        # entablature / roof
    for y in ((80.0, 120.0) if lod < 2 else ()):
        box(t, MARBLE, (0, y, 0), (72.0, 1.6, 72.0))                                       # string courses
    box(t, MARBLE, (0, 162.0, 0), (78.0, 4.0, 78.0))                                       # cornice
    prism(t, AN_STONE_F, octagon(0, 0, 30.0, 8.8), 164.0, 240.0)                           # octagonal tier
    prism(t, MARBLE, octagon(0, 0, 33.0, 9.7), 240.0, 244.0)
    ylathe(t, AN_MARBLE_F, [(0, 244.0), (19.0, 244.0), (19.0, 284.0), (0, 284.0)], seg)       # round tier
    ylathe(t, MARBLE, [(0, 284.0), (24.0, 284.0), (24.0, 287.0), (0, 287.0)], seg)
    ylathe(t, AN_STONE_F, [(0, 287.0), (13.0, 287.0), (13.0, 305.0), (0, 305.0)], seg)       # tholos cella
    ring_cols(t, MARBLE, 0, 0, 20.0, 16, 287.0, 18.0, 1.1, lod)
    ylathe(t, MARBLE, [(0, 305.0), (23.0, 305.0), (23.0, 308.0), (0, 308.0)], seg)
    dome(t, BRONZE, (0, 308.0, 0), 20.0, seg, rise=0.6)
    # the lantern: a truss cage with the beacon fire
    mast(t, 0, 0, 318.0, 352.0, 7.0, lod, taper=0.35, top=False)
    box(t, FLARE, (0, 324.0, 0), (6.0, 8.0, 6.0))
    beacon(t, (0, 354.0, 0), 2.4)
    if lod < 2:
        for k in range(4):
            a = math.pi / 4 + math.pi / 2 * k
            box(t, FLOOD, (51.0 * math.cos(a) * 1.41, 9.6, 51.0 * math.sin(a) * 1.41), (3.0, 1.0, 3.0))
    return P


def an_tower_hits():
    n = "hubx_ancient_tower"
    return [hbox(n, (0, 21.0, 0), (112.0, 42.0, 112.0)), hbox(n, (0, 103.0, 0), (78.0, 122.0, 78.0)),
            hbox(n, (0, 204.0, 0), (66.0, 80.0, 66.0)), hcyl(n, (0, 244.0, 0), (0, 320.0, 0), 24.0),
            hcyl(n, (0, 318.0, 0), (0, 355.0, 0), 7.0, 2.5)]


def an_slab_parts(lod):
    """terraced housing: four tiers stepping back 12 m each, an arcade (colonnade + architrave) along each terrace,
    terracotta parapets, a pitched roof on top, lattice masts at the ends"""
    name = "hubx_ancient_slab"
    P = {name: Part(name)}
    s = P[name]
    W, D, H = 200.0, 64.0, 22.0
    box(s, MARBLE, (0, 1.5, 0), (W + 8.0, 3.0, D + 8.0))
    back = -D / 2
    for i in range(4):
        depth = D - 12.0 * i
        y0 = 3.0 + H * i
        box(s, AN_STONE_F, (0, y0 + H / 2, back + depth / 2), (W - 4.0 * i, H, depth))
        front = back + depth
        # the arcade on the terrace in front of this tier (tier 0: on the street)
        cz = front + 5.0
        colonnade(s, MARBLE, (-(W - 4.0 * i) / 2 + 3.0, cz), ((W - 4.0 * i) / 2 - 3.0, cz), y0, H - 4.0, 1.2, 8.0, lod)
        box(s, MARBLE, (0, y0 + H - 2.0, cz), (W - 4.0 * i, 4.0, 3.2))                    # architrave
        if lod < 2:
            box(s, MARBLE, (0, y0 + H - 2.0, front + 2.5), (W - 4.0 * i, 0.8, 5.0))       # arcade ceiling
            box(s, TERRA, (0, y0 + H + 0.8, cz + 1.4), (W - 4.0 * i, 1.6, 0.6))           # parapet on the next terrace
    topw, topd, ytop = W - 12.0, D - 36.0, 3.0 + 4 * H
    gable(s, TERRA, (0, ytop, back + topd / 2), topd + 3.0, topw + 3.0, 9.0, along_x=True)
    for sx in (-1, 1):
        mast(s, sx * (topw / 2 - 6.0), back + 6.0, ytop, ytop + 34.0, 1.8, lod)
    return P


def an_slab_hits():
    n = "hubx_ancient_slab"
    H = [hbox(n, (0, 1.5, 0), (208.0, 3.0, 72.0))]
    for i in range(4):
        depth = 64.0 - 12.0 * i
        H.append(hbox(n, (0, 3.0 + 22.0 * i + 11.0, -32.0 + depth / 2 + 3.0), (200.0 - 4.0 * i, 22.0, depth + 6.0)))
    return H


def an_office_parts(lod):
    """a stoa office: a 170 m hall in a giant two-sided colonnade, a stepped stylobate, an entablature, a terracotta
    pitched roof with marble pediments, a ridge truss and two lattice masts"""
    name = "hubx_ancient_office"
    P = {name: Part(name)}
    o = P[name]
    box(o, MARBLE, (0, 1.5, 0), (194.0, 3.0, 80.0))
    box(o, MARBLE, (0, 4.5, 0), (188.0, 3.0, 74.0))
    box(o, AN_MARBLE_F, (0, 6.0 + 19.0, 0), (166.0, 38.0, 40.0))                          # the hall
    for sz in (-1, 1):
        colonnade(o, MARBLE, (-88.0, sz * 32.0), (88.0, sz * 32.0), 6.0, 38.0, 2.4, 11.0, lod)
    for sx in (-1, 1):
        colonnade(o, MARBLE, (sx * 88.0, -21.0), (sx * 88.0, 21.0), 6.0, 38.0, 2.4, 10.5, lod)
    box(o, MARBLE, (0, 48.0, 0), (186.0, 8.0, 72.0))                                       # entablature
    if lod < 2:
        box(o, AN_STONE_F, (0, 52.6, 0), (186.0, 1.2, 72.0))                               # frieze band
    gable(o, MARBLE, (0, 52.0, 0), 72.0, 186.0, 14.0, along_x=True)                         # pediments
    gable(o, TERRA, (0, 52.2, 0), 74.0, 182.0, 14.4, along_x=True)                          # the roof skin
    girder(o, (-80.0, 70.0, 0), (80.0, 70.0, 0), 1.6, lod, bay=10.0)                         # ridge truss
    for sx in (-1, 1):
        mast(o, sx * 80.0, 0, 66.0, 100.0, 1.8, lod)
    if lod < 2:
        for sx in (-1, 1):
            box(o, FLOOD, (sx * 96.0, 3.5, 36.0), (2.0, 1.0, 2.0))
    return P


def an_office_hits():
    n = "hubx_ancient_office"
    return [hbox(n, (0, 26.0, 0), (194.0, 52.0, 80.0)), hbox(n, (0, 59.0, 0), (186.0, 14.0, 74.0)),
            hcyl(n, (-80.0, 66.0, 0), (-80.0, 100.0, 0), 2.0), hcyl(n, (80.0, 66.0, 0), (80.0, 100.0, 0), 2.0)]


def an_downtown_parts(lod):
    """an agora quad with a campanile: four stone wings round a colonnaded court roofed by a truss grid, an obelisk in
    the middle, a marble tower on one corner (a belfry loggia, a bronze dome, a lattice mast, 300 m)"""
    name = "hubx_ancient_downtown"
    P = {name: Part(name)}
    d = P[name]
    A, wing, H = 170.0, 26.0, 36.0
    box(d, MARBLE, (0, 1.0, 0), (A + 6.0, 2.0, A + 6.0))
    h = A / 2
    for sx in (-1, 1):                                                                     # the four wings
        box(d, AN_STONE_F, (sx * (h - wing / 2), 2.0 + H / 2, 0), (wing, H, A))
        box(d, AN_STONE_F, (0, 2.0 + H / 2, sx * (h - wing / 2)), (A - 2 * wing, H, wing))
        box(d, MARBLE, (sx * (h - wing / 2), 2.0 + H + 1.0, 0), (wing + 3.0, 2.0, A + 3.0))
        box(d, MARBLE, (0, 2.0 + H + 1.0, sx * (h - wing / 2)), (A - 2 * wing, 2.0, wing + 3.0))
    c = h - wing - 6.0                                                                     # court colonnade
    for a, b in (((-c, -c), (c, -c)), ((c, -c), (c, c)), ((c, c), (-c, c)), ((-c, c), (-c, -c))):
        colonnade(d, MARBLE, a, b, 2.0, 14.0, 1.1, 8.0, lod)
    if lod < 2:
        for sx in (-1, 1):
            box(d, MARBLE, (sx * (c + 3.0), 17.0, 0), (6.0, 2.0, 2 * c + 6.0))
            box(d, MARBLE, (0, 17.0, sx * (c + 3.0)), (2 * c, 2.0, 6.0))
    ci = h - wing                                                                          # truss grid over the court
    for k in ((-0.5, 0.0, 0.5) if lod < 2 else (0.0,)):
        girder(d, (-ci, 2.0 + H - 2.0, k * ci * 2 * 0.9), (ci, 2.0 + H - 2.0, k * ci * 2 * 0.9), 1.6, lod, bay=8.0)
        girder(d, (k * ci * 2 * 0.9, 2.0 + H - 5.4, -ci), (k * ci * 2 * 0.9, 2.0 + H - 5.4, ci), 1.6, lod, bay=8.0)
    box(d, MARBLE, (0, 4.0, 0), (10.0, 4.0, 10.0))                                         # obelisk
    ylathe(d, MARBLE, [(0, 6.0), (3.0, 6.0), (1.8, 38.0), (0, 41.0)], 4)
    # the campanile on the -X -Z corner
    tx, tz = -(h - 22.0), -(h - 22.0)
    box(d, AN_MARBLE_F, (tx, 2.0 + 89.0, tz), (44.0, 178.0, 44.0))
    for y in ((60.0, 120.0) if lod < 2 else ()):
        box(d, MARBLE, (tx, y, tz), (46.0, 1.6, 46.0))
    box(d, MARBLE, (tx, 182.0, tz), (48.0, 4.0, 48.0))
    for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):                                    # belfry corner piers
        box(d, MARBLE, (tx + sx * 18.0, 198.0, tz + sz * 18.0), (8.0, 28.0, 8.0))
    for a, b in (((-14.0, -20.0), (14.0, -20.0)), ((-14.0, 20.0), (14.0, 20.0)), ((-20.0, -14.0), (-20.0, 14.0)),
                 ((20.0, -14.0), (20.0, 14.0))):
        colonnade(d, MARBLE, (tx + a[0], tz + a[1]), (tx + b[0], tz + b[1]), 184.0, 28.0, 1.0, 7.0, lod, cap=False)
    box(d, AN_STONE_F, (tx, 198.0, tz), (26.0, 28.0, 26.0))                                # the belfry core
    box(d, MARBLE, (tx, 214.0, tz), (48.0, 4.0, 48.0))
    prism(d, AN_MARBLE_F, octagon(tx, tz, 16.0, 4.7), 216.0, 236.0)
    dome(d, BRONZE, (tx, 236.0, tz), 16.0, _seg(lod, 20), rise=0.9)
    mast(d, tx, tz, 250.0, 296.0, 3.0, lod, taper=0.3)
    return P


def an_downtown_hits():
    n = "hubx_ancient_downtown"
    tx = -(85.0 - 22.0)
    return [hbox(n, (0, 20.0, 0), (176.0, 40.0, 176.0)), hbox(n, (tx, 128.0, tx), (48.0, 216.0, 48.0)),
            hcyl(n, (tx, 216.0, tx), (tx, 251.0, tx), 16.0), hcyl(n, (tx, 250.0, tx), (tx, 297.0, tx), 3.0, 1.0)]


def an_landmark_parts(lod):
    """a temple on an acropolis: a stone podium with a grand stair, a three-step stylobate, a 46-column peristyle round
    the cella, an entablature, marble pediments under a terracotta roof, two truss fire pylons in front"""
    name = "hubx_ancient_landmark"
    P = {name: Part(name)}
    m = P[name]
    box(m, AN_STONE_F, (0, 12.0, -6.0), (176.0, 24.0, 112.0))                              # the acropolis podium
    box(m, MARBLE, (0, 24.6, -6.0), (178.0, 1.2, 114.0))
    ns = 8 if lod < 2 else 2                                                               # the grand stair
    for k in range(ns):
        hgt = 24.0 * (k + 1) / ns
        box(m, MARBLE, (0, hgt / 2, 50.0 + 16.0 * (ns - k - 0.5) / ns), (60.0, hgt, 16.0 / ns))
    y = 25.2
    for k, (w, dd) in enumerate(((152.0, 92.0), (146.0, 86.0), (140.0, 80.0))):           # stylobate
        box(m, MARBLE, (0, y + 1.5 + 3.0 * k, -6.0), (w, 3.0, dd))
    y += 9.0
    box(m, AN_MARBLE_F, (0, y + 20.0, -6.0), (104.0, 40.0, 38.0))                          # the cella
    xs, zs = 66.0, 36.0
    for a, b in (((-xs, -zs), (xs, -zs)), ((-xs, zs), (xs, zs))):
        colonnade(m, MARBLE, (a[0], a[1] - 6.0), (b[0], b[1] - 6.0), y, 40.0, 3.0, 8.25, lod)
    for sx in (-1, 1):
        colonnade(m, MARBLE, (sx * xs, -zs + 10.3 - 6.0), (sx * xs, zs - 10.3 - 6.0), y, 40.0, 3.0, 10.3, lod)
    box(m, MARBLE, (0, y + 44.0, -6.0), (142.0, 8.0, 82.0))                                # entablature
    if lod < 2:
        box(m, AN_STONE_F, (0, y + 46.0, -6.0), (142.6, 2.0, 82.6))                        # frieze
    gable(m, MARBLE, (0, y + 48.0, -6.0), 82.0, 142.0, 16.0, along_x=True)
    gable(m, TERRA, (0, y + 48.2, -6.0), 84.0, 138.0, 16.4, along_x=True)
    if lod < 2:
        for sx in (-1, 1):                                                                 # acroteria
            box(m, MARBLE, (sx * 71.0, y + 50.0, -6.0), (2.0, 4.0, 6.0))
        box(m, MARBLE, (71.0, y + 66.0, -6.0), (2.0, 5.0, 3.0))
        box(m, MARBLE, (-71.0, y + 66.0, -6.0), (2.0, 5.0, 3.0))
    for sx in (-1, 1):                                                                     # fire pylons at the stair
        mast(m, sx * 40.0, 52.0, 0.0, 56.0, 3.0, lod, taper=0.5, top=False)
        ylathe(m, BRONZE, [(0, 56.0), (2.0, 56.0), (5.0, 60.0), (5.0, 61.0), (0, 61.0)], _seg(lod, 12),
               t=(sx * 40.0, 0, 52.0))
        box(m, FLARE, (sx * 40.0, 63.0, 52.0), (6.0, 4.0, 6.0))
    for sx in (-1, 1):
        for sz in (-1, 1):
            beacon(m, (sx * 87.0, 26.0, -6.0 + sz * 55.0), 1.2)
    return P


def an_landmark_hits():
    n = "hubx_ancient_landmark"
    return [hbox(n, (0, 12.0, -6.0), (178.0, 25.0, 114.0)), hbox(n, (0, 12.0, 58.0), (60.0, 24.0, 16.0)),
            hbox(n, (0, 50.0, -6.0), (152.0, 52.0, 92.0)), hbox(n, (0, 82.0, -6.0), (142.0, 16.0, 84.0)),
            hcyl(n, (-40.0, 0, 52.0), (-40.0, 65.0, 52.0), 5.0), hcyl(n, (40.0, 0, 52.0), (40.0, 65.0, 52.0), 5.0)]


# ================================================================ INDIAN (Indian-descended polities)
def shikhara(p, mat, x, y0, z, W, H, lod, cap=GOLD):
    """a curvilinear cruciform (rathas) tower: shrinking levels of a square + a cross of projections, an amalaka disc
    and a gold kalasha on top"""
    n = {0: 14, 1: 9}.get(lod, 5)
    dh = H / n
    for k in range(n):
        t = (k + 0.5) / n
        w = W * (1.0 - 0.72 * t ** 1.6)
        yc = y0 + dh * (k + 0.5)
        box(p, mat, (x, yc, z), (w, dh, w))
        if lod < 2:
            box(p, mat, (x, yc, z), (w * 0.5, dh * 0.98, w * 1.08))
            box(p, mat, (x, yc, z), (w * 1.08, dh * 0.98, w * 0.5))
    wt = W * 0.28
    ylathe(p, mat, [(0, y0 + H), (wt * 0.55, y0 + H), (wt * 0.62, y0 + H + 0.06 * W), (wt * 0.55, y0 + H + 0.12 * W),
                    (0, y0 + H + 0.12 * W)], _seg(lod, 16), t=(x, 0, z), soft=50)           # amalaka
    yk = y0 + H + 0.12 * W
    ylathe(p, cap, [(0, yk), (0.08 * W, yk + 0.04 * W), (0.10 * W, yk + 0.10 * W), (0.04 * W, yk + 0.16 * W),
                    (0.02 * W, yk + 0.22 * W), (0, yk + 0.24 * W)], _seg(lod, 12), t=(x, 0, z), soft=50)   # kalasha
    return yk + 0.24 * W


def in_tower_parts(lod):
    """a cruciform residential tower (rathas projections, cornice bands) crowned by a shikhara with four corner
    sub-spires (urushringa), an amalaka and a kalasha, a lattice lightning mast (390 m)"""
    name = "hubx_indian_tower"
    P = {name: Part(name)}
    t = P[name]
    box(t, SAND, (0, 6.0, 0), (104.0, 12.0, 104.0))
    box(t, IN_SAND_F, (0, 12.0 + 109.0, 0), (60.0, 218.0, 60.0))
    box(t, IN_SAND_F, (0, 12.0 + 109.0, 0), (34.0, 218.0, 74.0))
    box(t, IN_SAND_F, (0, 12.0 + 109.0, 0), (74.0, 218.0, 34.0))
    for y in ((52.0, 92.0, 132.0, 172.0, 212.0) if lod < 2 else (132.0,)):                 # cornice bands
        box(t, IN_IVORY_F, (0, y, 0), (64.0, 3.0, 64.0))
        box(t, IN_IVORY_F, (0, y, 0), (38.0, 3.0, 78.0))
        box(t, IN_IVORY_F, (0, y, 0), (78.0, 3.0, 38.0))
    box(t, SAND, (0, 232.0, 0), (80.0, 4.0, 80.0))
    top = shikhara(t, SAND, 0, 234.0, 0, 66.0, 100.0, lod)
    for sx in (-1, 1):
        for sz in (-1, 1):
            shikhara(t, SAND, sx * 30.0, 234.0, sz * 30.0, 16.0, 30.0, 2 if lod > 0 else 1)
    mast(t, 0, 0, top, 388.0, 2.0, lod, taper=0.3)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                chhatri(t, sx * 44.0, 12.0, sz * 44.0, 10.0, lod)
    return P


def in_tower_hits():
    n = "hubx_indian_tower"
    return [hbox(n, (0, 6.0, 0), (104.0, 12.0, 104.0)), hbox(n, (0, 123.0, 0), (78.0, 222.0, 78.0)),
            hcyl(n, (0, 234.0, 0), (0, 350.0, 0), 40.0, 8.0), hcyl(n, (0, 350.0, 0), (0, 390.0, 0), 2.0, 0.6)]


def in_slab_parts(lod):
    """a haveli courtyard block: four ivory wings round a court with a fountain, rows of jharokha balconies with
    domed hoods on the long fronts, chhatris on the roof, a truss shade canopy across the court"""
    name = "hubx_indian_slab"
    P = {name: Part(name)}
    s = P[name]
    L, D, H, wing = 190.0, 80.0, 54.0, 20.0
    box(s, SAND, (0, 2.0, 0), (L + 6.0, 4.0, D + 6.0))
    for sz in (-1, 1):
        box(s, IN_IVORY_F, (0, 4.0 + H / 2, sz * (D / 2 - wing / 2)), (L, H, wing))
    for sx in (-1, 1):
        box(s, IN_IVORY_F, (sx * (L / 2 - wing / 2), 4.0 + H / 2, 0), (wing, H, D - 2 * wing))
    box(s, SAND, (0, 4.0 + H + 1.0, -(D / 2 - wing / 2)), (L + 2.0, 2.0, wing + 2.0))         # parapets
    box(s, SAND, (0, 4.0 + H + 1.0, (D / 2 - wing / 2)), (L + 2.0, 2.0, wing + 2.0))
    for sx in (-1, 1):
        box(s, SAND, (sx * (L / 2 - wing / 2), 4.0 + H + 1.0, 0), (wing + 2.0, 2.0, D - 2 * wing))
    # jharokhas: projecting balconies with a little dome hood
    levels = (16.0, 30.0, 44.0) if lod == 0 else (30.0,) if lod == 1 else ()
    for sz in (-1, 1):
        for y in levels:
            for k in range(7):
                x = -72.0 + 24.0 * k
                z = sz * (D / 2 + 1.6)
                box(s, SAND, (x, y, z), (8.0, 6.0, 3.2))
                box(s, SAND, (x, y + 3.4, z), (9.0, 0.8, 4.0))
                if lod == 0:
                    dome(s, SAND, (x, y + 3.8, z), 2.2, 8, rise=1.2)
    for sx in (-1, 1):                                                                     # roof chhatris
        for sz in (-1, 1):
            chhatri(s, sx * (L / 2 - 10.0), 4.0 + H + 2.0, sz * (D / 2 - 10.0), 12.0, lod)
    if lod < 2:
        for sz in (-1, 1):
            chhatri(s, 0, 4.0 + H + 2.0, sz * (D / 2 - 10.0), 9.0, lod)
    # the court: a truss shade canopy on two girders, a fountain
    for x in (-40.0, 40.0):
        girder(s, (x, 4.0 + H - 2.0, -(D / 2 - wing)), (x, 4.0 + H - 2.0, D / 2 - wing), 1.6, lod, bay=6.0)
    if lod < 2:
        girder(s, (-(L / 2 - wing), 4.0 + H + 1.4, 0), (L / 2 - wing, 4.0 + H + 1.4, 0), 1.6, lod, bay=10.0)
        box(s, IN_MARBLE, (0, 4.6, 0), (24.0, 1.2, 24.0))
        ylathe(s, IN_MARBLE, [(0, 5.2), (2.0, 5.2), (1.0, 9.0), (4.0, 10.0), (0, 10.4)], 10)
    mast(s, L / 2 - 10.0, 0.0, 4.0 + H + 2.0, 4.0 + H + 34.0, 1.6, lod)
    return P


def in_slab_hits():
    n = "hubx_indian_slab"
    return [hbox(n, (0, 31.0, 0), (196.0, 62.0, 86.0)), hcyl(n, (85.0, 60.0, 0), (85.0, 93.0, 0), 1.6)]


def _jali(p, mat, x0, x1, y0, y1, z, pitch, lod):
    """a diagonal jali screen in the plane z: two families of 45-degree bars clipped to the rectangle, a frame"""
    pitch = pitch * (1 if lod == 0 else 2)
    W, H = x1 - x0, y1 - y0
    for sgn in (1, -1):
        c = -H
        while c < W:
            # line: x - x0 = c + (y - y0) * sgn' ; parametrised by y offset s in [0, H]
            pts = []
            for s in (0.0, H):
                xx = c + (s if sgn > 0 else H - s)
                pts.append((xx, s))
            (ax, ay), (bx, by) = pts
            # clip to 0 <= x <= W
            def clip(t):
                return (ax + (bx - ax) * t, ay + (by - ay) * t)
            t0, t1 = 0.0, 1.0
            if bx != ax:
                for lim in (0.0, W):
                    tt = (lim - ax) / (bx - ax)
                    if (bx - ax) > 0:
                        if lim == 0.0:
                            t0 = max(t0, tt)
                        else:
                            t1 = min(t1, tt)
                    else:
                        if lim == 0.0:
                            t1 = min(t1, tt)
                        else:
                            t0 = max(t0, tt)
            if t1 - t0 > 1e-3:
                (qx, qy), (rx, ry) = clip(t0), clip(t1)
                Lb = math.hypot(rx - qx, ry - qy)
                ang = math.atan2(ry - qy, rx - qx)
                box(p, mat, (x0 + (qx + rx) / 2, y0 + (qy + ry) / 2, z), (Lb, 0.9, 0.6), rot_z(ang))
            c += pitch
    for y in (y0, y1):
        box(p, mat, ((x0 + x1) / 2, y, z), (W + 1.2, 1.2, 0.8))
    for x in (x0, x1):
        box(p, mat, (x, (y0 + y1) / 2, z), (1.2, H, 0.8))


def in_office_parts(lod):
    """a red sandstone office block 140 x 60 x 120 m with marble jali screens over the middle of the long fronts, a
    plinth arcade of pillars, chhatris on the corners, a domed central pavilion, a truss solar canopy on lattice posts"""
    name = "hubx_indian_office"
    P = {name: Part(name)}
    o = P[name]
    L, D, H = 140.0, 60.0, 120.0
    box(o, SAND, (0, 3.0, 0), (L + 10.0, 6.0, D + 10.0))
    box(o, IN_SAND_F, (0, 6.0 + H / 2, 0), (L, H, D))
    box(o, SAND, (0, 6.0 + H + 1.5, 0), (L + 4.0, 3.0, D + 4.0))                           # cornice
    for sz in (-1, 1):
        _jali(o, IN_MARBLE, -36.0, 36.0, 14.0, 6.0 + H - 6.0, sz * (D / 2 + 3.0), 9.0, lod)
        if lod < 2:
            for x in range(-63, 64, 14):                                                   # plinth pillars
                box(o, SAND, (x, 10.0, sz * (D / 2 + 3.0)), (2.4, 8.0, 2.4))
            box(o, SAND, (0, 14.5, sz * (D / 2 + 3.0)), (L, 1.2, 6.0))
    yr = 6.0 + H + 3.0
    for sx in (-1, 1):
        for sz in (-1, 1):
            chhatri(o, sx * (L / 2 - 8.0), yr, sz * (D / 2 - 8.0), 12.0, lod)
    box(o, SAND, (0, yr + 6.0, 0), (26.0, 12.0, 26.0))                                     # central pavilion
    onion(o, IN_MARBLE, (0, yr + 12.0, 0), 11.0, _seg(lod, 16))
    cyl(o, GOLD, (0, yr + 32.0, 0), (0, yr + 36.0, 0), 0.8, 6)
    # solar canopy: girders along X on lattice posts, both sides of the pavilion
    for sz in (-1, 1):
        z = sz * 18.0
        girder(o, (-52.0, yr + 12.0, z), (52.0, yr + 12.0, z), 1.6, lod, bay=8.0)
        for x in ((-48.0, -16.0, 16.0, 48.0) if lod < 2 else (-48.0, 48.0)):
            mast(o, x, z, yr, yr + 10.4, 1.2, lod, taper=1.0, top=False)
        if lod < 2:
            box(o, DARK, (0, yr + 14.0, z), (100.0, 0.4, 10.0))                            # the panels
    beacon(o, (52.0, yr + 14.0, 18.0), 1.2)
    beacon(o, (-52.0, yr + 14.0, -18.0), 1.2)
    return P


def in_office_hits():
    n = "hubx_indian_office"
    return [hbox(n, (0, 64.5, 0), (150.0, 129.0, 70.0)), hbox(n, (0, 141.0, 0), (130.0, 24.0, 50.0))]


def in_downtown_parts(lod):
    """a gopuram-like downtown tower: a jali-banded podium, a rectangular stepped tower tapering 84 x 54 -> 34 x 16 m with
    a cornice and little kudu shrines on every tier, a gold barrel-vault crown with kalashas, truss flag masts"""
    name = "hubx_indian_downtown"
    P = {name: Part(name)}
    d = P[name]
    box(d, IN_IVORY_F, (0, 15.0, 0), (130.0, 30.0, 96.0))
    box(d, SAND, (0, 31.0, 0), (134.0, 2.0, 100.0))
    if lod < 2:
        for sz in (-1, 1):
            _jali(d, IN_MARBLE, -60.0, 60.0, 4.0, 26.0, sz * 49.5, 8.0, lod)
    N = {0: 14, 1: 9}.get(lod, 5)
    y0, y1 = 32.0, 272.0
    dh = (y1 - y0) / N
    for k in range(N):
        t = k / max(1, N - 1)
        w, dd = 84.0 - 50.0 * t, 54.0 - 38.0 * t
        yc = y0 + dh * (k + 0.5)
        box(d, IN_SAND_F, (0, yc, 0), (w, dh - 1.6, dd))
        box(d, SAND, (0, y0 + dh * (k + 1) - 0.8, 0), (w + 3.0, 1.6, dd + 3.0))             # tier cornice
        if lod == 0 and k % 2 == 0:
            for sx in (-1, 1):
                chhatri(d, sx * (w / 2 - 3.0), y0 + dh * (k + 1), dd / 2 - 3.0, 4.0, 2)
                chhatri(d, sx * (w / 2 - 3.0), y0 + dh * (k + 1), -(dd / 2 - 3.0), 4.0, 2)
    # the crown: a gold barrel vault along X with a row of kalashas
    cyl(d, GOLD, (-17.0, y1 + 1.0, 0), (17.0, y1 + 1.0, 0), 7.5, _seg(lod, 16))
    for k in range(7 if lod < 2 else 3):
        x = -15.0 + 30.0 * k / (6 if lod < 2 else 2)
        ylathe(d, GOLD, [(0, y1 + 8.0), (1.6, y1 + 9.0), (1.8, y1 + 11.0), (0.6, y1 + 13.5), (0, y1 + 14.5)], 8, t=(x, 0, 0))
    for sx in (-1, 1):
        mast(d, sx * 22.0, 0, y1, y1 + 46.0, 2.0, lod, taper=0.3)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                chhatri(d, sx * 56.0, 32.0, sz * 40.0, 12.0, lod)
    return P


def in_downtown_hits():
    n = "hubx_indian_downtown"
    return [hbox(n, (0, 16.0, 0), (134.0, 32.0, 100.0)), hbox(n, (0, 152.0, 0), (87.0, 240.0, 57.0)),
            hcyl(n, (-17.0, 273.0, 0), (17.0, 273.0, 0), 7.5), hcyl(n, (-22.0, 272.0, 0), (-22.0, 319.0, 0), 2.0, 0.6),
            hcyl(n, (22.0, 272.0, 0), (22.0, 319.0, 0), 2.0, 0.6)]


def in_landmark_parts(lod):
    """a domed mausoleum-landmark: a marble plinth, a chamfered ivory hall with pishtaq arch frames, a drum, a big onion
    dome with a gold finial, four corner chhatris, four minarets; in front a charbagh garden with water channels and
    four truss lamp pylons"""
    name = "hubx_indian_landmark"
    P = {name: Part(name)}
    m = P[name]
    seg = _seg(lod, 28)
    zc = -35.0
    box(m, IN_MARBLE, (0, 6.0, zc), (150.0, 12.0, 150.0))
    prism(m, IN_IVORY_F, octagon(0, zc, 45.0, 13.0), 12.0, 72.0)                            # the hall
    for k in range(4):                                                                     # pishtaq arch frames
        a = math.pi / 2 * k
        R = rot_y(a)
        c = apply(R, (0.0, 0.0, 45.6))
        box(m, IN_MARBLE, (c[0], 40.0, zc + c[2]), (36.0, 52.0, 1.2), R)
        box(m, SHADOW, (apply(R, (0, 0, 46.6))[0], 36.0, zc + apply(R, (0, 0, 46.6))[2]), (24.0, 40.0, 0.4), R)
        if lod < 2:
            dome(m, IN_MARBLE, (apply(R, (0, 0, 46.0))[0], 56.0, zc + apply(R, (0, 0, 46.0))[2]), 6.0, 10, rise=1.3)
    prism(m, IN_MARBLE, octagon(0, zc, 47.0, 13.8), 72.0, 75.0)
    ylathe(m, IN_IVORY_F, [(0, 75.0), (27.0, 75.0), (27.0, 90.0), (0, 90.0)], seg, t=(0, 0, zc))   # drum
    onion(m, IN_MARBLE, (0, 90.0, zc), 30.0, seg)
    ylathe(m, GOLD, [(0, 145.0), (1.6, 145.0), (2.2, 150.0), (1.0, 154.0), (1.6, 157.0), (0, 162.0)], 8, t=(0, 0, zc))
    for sx in (-1, 1):
        for sz in (-1, 1):
            chhatri(m, sx * 30.0, 75.0, zc + sz * 30.0, 14.0, lod, mat=IN_MARBLE)
    # minarets on the plinth corners, a balcony ring every 36 m, a chhatri on top
    for sx in (-1, 1):
        for sz in (-1, 1):
            x, z = sx * 66.0, zc + sz * 66.0
            ylathe(m, IN_MARBLE, [(0, 12.0), (6.0, 12.0), (4.2, 118.0), (0, 118.0)], _seg(lod, 12), t=(x, 0, z))
            for y in ((48.0, 84.0, 118.0) if lod < 2 else (118.0,)):
                ylathe(m, IN_MARBLE, [(0, y - 1.5), (7.5, y - 1.0), (7.5, y + 0.5), (0, y + 0.5)], _seg(lod, 12), t=(x, 0, z))
            chhatri(m, x, 118.5, z, 9.0, lod, mat=IN_MARBLE)
    # the charbagh: four beds split by cross water channels, lamp pylons on the corners
    box(m, SAND, (0, 0.6, 75.0), (150.0, 1.2, 70.0))
    box(m, DARK, (0, 1.3, 75.0), (6.0, 0.2, 70.0))                                           # water channels
    box(m, DARK, (0, 1.3, 75.0), (150.0, 0.2, 6.0))
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(m, SHADOW, (sx * 38.0, 1.6, 75.0 + sz * 18.0), (64.0, 0.8, 28.0))   # the beds
    for sx in (-1, 1):
        for zz in (48.0, 106.0):
            mast(m, sx * 70.0, zz, 1.2, 40.0, 2.0, lod, taper=0.4, top=False)
            box(m, FLOOD, (sx * 70.0, 40.8, zz), (3.0, 1.6, 3.0))
    return P


def in_landmark_hits():
    n = "hubx_indian_landmark"
    zc = -35.0
    H = [hbox(n, (0, 6.0, zc), (150.0, 12.0, 150.0)), hbox(n, (0, 43.5, zc), (92.0, 63.0, 92.0)),
         hcyl(n, (0, 75.0, zc), (0, 90.0, zc), 27.0), hcyl(n, (0, 90.0, zc), (0, 145.0, zc), 35.0, 5.0),
         hbox(n, (0, 0.8, 75.0), (150.0, 1.6, 70.0))]
    for sx in (-1, 1):
        for sz in (-1, 1):
            H.append(hcyl(n, (sx * 66.0, 12.0, zc + sz * 66.0), (sx * 66.0, 130.0, zc + sz * 66.0), 6.0, 4.5))
            H.append(hcyl(n, (sx * 70.0, 0.0, 77.0 + sz * 29.0), (sx * 70.0, 42.0, 77.0 + sz * 29.0), 2.0))
    return H


# ================================================================ registry
KIT = {
    "hubx_ultramodern_tower": (um_tower_parts, um_tower_hits, "Ультрасовременный стиль: закрученная стеклянная башня 398 м"),
    "hubx_ultramodern_slab": (um_slab_parts, um_slab_hits, "Ультрасовременный стиль: обтекаемый жилой корпус 200x40 с садом-фермой на крыше"),
    "hubx_ultramodern_office": (um_office_parts, um_office_hits, "Ультрасовременный стиль: башни-близнецы с ферменным небесным мостом"),
    "hubx_ultramodern_downtown": (um_downtown_parts, um_downtown_hits, "Ультрасовременный стиль: сужающаяся круглая мегабашня 396 м"),
    "hubx_ultramodern_landmark": (um_landmark_parts, um_landmark_hits, "Ультрасовременный стиль: телебашня-«жемчужина» на решётчатой треноге"),
    "hubx_ancient_tower": (an_tower_parts, an_tower_hits, "Античный стиль: ярусная башня-маяк с колоннадой и бронзовым куполом"),
    "hubx_ancient_slab": (an_slab_parts, an_slab_hits, "Античный стиль: террасный жилой корпус с аркадами"),
    "hubx_ancient_office": (an_office_parts, an_office_hits, "Античный стиль: контора-стоя в колоннаде с фронтонами"),
    "hubx_ancient_downtown": (an_downtown_parts, an_downtown_hits, "Античный стиль: агора с колоннадным двором и кампанилой"),
    "hubx_ancient_landmark": (an_landmark_parts, an_landmark_hits, "Античный стиль: храм на акрополе"),
    "hubx_indian_tower": (in_tower_parts, in_tower_hits, "Индийский стиль: башня-шикхара"),
    "hubx_indian_slab": (in_slab_parts, in_slab_hits, "Индийский стиль: двор-хавели с джарокхами и чхатри"),
    "hubx_indian_office": (in_office_parts, in_office_hits, "Индийский стиль: контора из песчаника с решётками-джали"),
    "hubx_indian_downtown": (in_downtown_parts, in_downtown_hits, "Индийский стиль: ступенчатая башня-гопурам"),
    "hubx_indian_landmark": (in_landmark_parts, in_landmark_hits, "Индийский стиль: купольный мавзолей с минаретами и садом-чарбагом"),
}

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Модуль городского стиля хаба (tools/models/units/hub_style_east.py).")
             for k, v in KIT.items()}

# the zone pools of a styled hub (keys as hub_plane.ZONE_MODULES; the missing zones keep the default kit). The default
# warehouses / works stay - the mechanics layer is the same, only the skin differs.
STYLE_ZONE_MODULES = {
    "ultramodern": {
        "downtown": [("hubx_ultramodern_downtown", 2), ("hubx_ultramodern_tower", 3), ("hubx_ultramodern_office", 2),
                     ("hubx_ultramodern_landmark", 1), ("hubx_ultramodern_slab", 1)],
        "residential": [("hubx_ultramodern_slab", 3), ("hubx_ultramodern_tower", 2), ("hubx_ultramodern_office", 1),
                        ("hubb_hab_slab", 1)],
        "hotel": [("hubx_ultramodern_slab", 2), ("hubx_ultramodern_office", 1), ("hubv_pad_star", 1)],
    },
    "ancient": {
        "downtown": [("hubx_ancient_downtown", 2), ("hubx_ancient_tower", 2), ("hubx_ancient_office", 2),
                     ("hubx_ancient_landmark", 1), ("hubx_ancient_slab", 1)],
        "residential": [("hubx_ancient_slab", 4), ("hubx_ancient_tower", 1), ("hubx_ancient_office", 1), ("hubk_hab_c", 1)],
        "hotel": [("hubx_ancient_slab", 2), ("hubx_ancient_office", 1), ("hubv_pad_star", 1)],
    },
    "indian": {
        "downtown": [("hubx_indian_downtown", 2), ("hubx_indian_tower", 2), ("hubx_indian_office", 2),
                     ("hubx_indian_landmark", 1), ("hubx_indian_slab", 1)],
        "residential": [("hubx_indian_slab", 4), ("hubx_indian_tower", 1), ("hubx_indian_office", 1), ("hubk_hab_a", 1)],
        "hotel": [("hubx_indian_slab", 2), ("hubx_indian_office", 1), ("hubv_pad_star", 1)],
    },
}

# which polities (colony ids = the polity / bloc-member ids of src/assets/data/economy/economy_rules.json blocs_members)
# take which style, by their founding culture:
#   ultramodern: gj832_tianhe (TIANHE-1: chinese_state 1.0), eps_eri_harmony (CELESTIAL-DESTINY-1, HOMEWARD-1:
#                chinese_state 1.0), hd40307_firmament (CELESTIAL-DESTINY-2: chinese_state 1.0) - the heavenly_pact bloc;
#                tau_ceti_cognium (GENESIS-1: chinese_diaspora 0.6 beside MAYFLOWER-2 american_frontier 0.85 and
#                TRADERS-GUILD-1 greek / polynesian; a technocratic meritocracy) - mixed, ultramodern by its regime, the
#                integrator may give it to the western styles instead
#   ancient:     lalande_hellas (HERACLES-1: greek 0.4, italian 0.3, iberian / maghrebi), hd114783_hellas3 (ODYSSEY-1:
#                greek 0.35, italian 0.35, iberian 0.3) - the lalande_league; NOT hd20794_hellas2 (HOMESTEAD-1:
#                american_frontier 0.6 - the name only) and NOT gj581_europa (EUROPA-1: eu_bureaucratic 0.7)
#   indian:      delta_pav_karma (DHARMA-1: indian 1.0), delta_pav_freekarma (the DHARMA-1 exiles), hd20794_newindia
#                (DHARMA-2: indian 1.0); NOT proxima_outpost (MERU-1: indian 0.35 under russian 0.4) or cyg61_kern
#                (RESOURCER-1: indian 0.3)
# sources: tools/colonization_lab/canon/missions.json (missions[].cultures, missions[].colony), world.json (colonies[]
# .founders, .polity, .regime_3126), cultures.json (the culture keys)
STYLE_POLITIES = {
    "ultramodern": ["gj832_tianhe", "eps_eri_harmony", "hd40307_firmament", "tau_ceti_cognium"],
    "ancient": ["lalande_hellas", "hd114783_hellas3"],
    "indian": ["delta_pav_karma", "delta_pav_freekarma", "hd20794_newindia"],
}
