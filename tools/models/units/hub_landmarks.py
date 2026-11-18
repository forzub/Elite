"""Landmarks of the hub cities (author 2026-11-18: «каждый хаб должен быть узнаваем по индивидуальным особенностям -
уникальные лендмарки: здания особой формы, статуи, арочный мост, простой мост, "юбка" вокруг какого-то здания, или
планировка кварталов - очень широкая площадь с какой-то штукой в углу»).

A kit of one-off silhouettes that read from 2-5 km, sized like the national style modules (units/hub_style_*.py): every
module is a hub module (ROOTS "hub"), base on y 0, +Y up, front +Z, centred on the origin, fitting one 600 x 600 m block
(usable ~580 m) - except the bridges, which span a street gap between two blocks. Statues are plain bronze / marble /
iron (no window grid); towers wear eq_hub_facade_* skins (the shader draws their windows). Every module carries visible
lattice trusses (floodlight masts, cage crowns, bridge ribs, skirt struts) - the shared engineering layer.

Modules (ids hubl_*; sizes in LANDMARKS[...]["size"], x * z * height in metres):
  statues   hubl_statue_pioneer (a spacer in a helmet, a beacon torch raised), hubl_statue_robed (a robed figure
            holding a gilt orb), hubl_statue_worker (a striding worker swinging a hammer), hubl_statue_rider (a rider on
            a rearing mount with a banner), hubl_statue_hand (a giant hand holding a globe), hubl_statue_head (a giant
            helmeted head), hubl_ring_sculpture (two interlocked chrome rings)
  towers    hubl_obelisk, hubl_twisted_tower, hubl_skirt_tower (a slender tower with a wide flared collar platform on
            trusses), hubl_dome_ring (a dome girdled by a Saturn ring), hubl_clock_tower (four clock faces, a lattice
            belfry, a beacon), hubl_garden_terrace (stepped hanging gardens), hubl_gate (a monumental arched gate)
  squares   hubl_square (a whole block paved, a column monument in its +x +z corner), hubl_square_open (the paving
            alone), hubl_plaza_deck (a paved deck over a LOCAL street gap: 300 x 580, joins two squares into one)
  bridges   hubl_bridge_arch (a through-arch over an AVENUE gap, 500 m clear, crown 205 m), hubl_bridge_girder (a flat
            girder bridge over a LOCAL street gap, 300 m clear, top 24 m)

THE PICKER: landmarks_for(hub_id, style=None) -> dict
  {"hub_id", "style", "seed",
   "signature": {"name", "title", "rule", "cards": [{"module", "place"}...]},
   "landmarks": [{"module", "title", "kind", "span", "place"}...]}             (2-4 landmarks, no repeats)
Pure in the hub id: the seed is the first 12 hex digits of sha1(hub_id) (as hub_plane._generate_map), never Python
hash(); style (a hub_styles style name, "" = the default kit; None = take it from tools/models/hub_profiles.json)
only tilts the statue flavour and the tower pool (STYLE_STATUES, STYLE_TOWERS).

HOW THE GENERATOR (hub_plane._generate_map) SHOULD CONSUME IT - not wired yet; hub_plane is left as it is:
  1. Call landmarks_for(hub_id, prof.get("style")) right after the grid, the zones and the dock cell are decided, with
     its own rng (the map rng's draws stay as they are, the existing cities do not reshuffle).
  2. Apply the signature to the grid first (it claims cells and their max height), then place the landmarks:
       wide_square     the downtown cell nearest the centre that has a downtown / residential neighbour across a LOCAL
                       street (gap not in avenues): that cell gets the card hubl_square alone (the monument's corner
                       pointing away from the centre: yaw so local +x +z faces outward), the neighbour hubl_square_open
                       alone, the gap between them hubl_plaza_deck (origin on the gap centre, local +x across the gap,
                       yaw 90 for a gap between i and i+1, 0 for j and j+1). Both cells: zone "square", no under-city.
       ring_boulevard  the 8 cells round the canopy (Chebyshev distance 1) are kept low (max_top 120: hub_k / slab
                       modules only) so the canopy stands in a low green ring; the landmarks go on two OPPOSITE cells
                       of that ring (on the avenue axes through the centre), each alone in its cell.
       diagonal_avenue the cells on one diagonal from the centre to the rim (|di| == |dj|, the direction from the seed:
                       signature["dir"]) are kept low (max_top 40: parking / plazas, hubl_square_open) - a visible cut
                       across the grid; hubl_gate stands on the rim end of the diagonal (yaw so +z faces the centre),
                       the first statue on the centre end.
       central_void    one ring-1 cell (signature["dir"] from the centre, never the dock's) is removed from the cell set
                       (open sky through the city: the girder frame skips it, its streets close round it); the
                       landmarks stand on the cells that face the hole, the tallest one opposite the canopy.
       canal           the avenue line next to the centre along signature["axis"] (always "x": a gap j|j+1 in
                       avenues["z"], a street running along X - see "street_x" below) becomes the canal: the cells along both banks are kept at max_top 150, so the avenue
                       reads as a long canyon; hubl_bridge_arch crosses it on the cell pair nearest the centre (that
                       pair's facing edges keep a 80 m strip free for the abutments).
  3. Landmarks by place (each one alone in its cell, centred, yaw 0 unless said; a cell never takes two):
       "plaza"     a downtown cell at Chebyshev distance 1-2 from the centre (the statues; with a wide_square
                   signature the statue takes the plaza cell nearest the square, the square keeps its own column)
       "downtown"  a downtown cell (the towers; max_top does not apply to a landmark)
       "residential" a residential cell (the garden terrace)
       "axis"      a cell on an avenue axis through the centre, distance 2-3 (the gate: +z facing the centre)
       "street_x"  a bridge along local +Z (yaw 0) with its origin on the centre of the gap between cells (i, j) and
                   (i, j+1) - a street running along X. Over the X streets the lanes fly at 320 / 430 m (hub_lanes
                   LANE_LEVELS), so the arch's 205 m crown and the under-lanes at -320 / -430 stay clear. The arch
                   needs an AVENUE gap (j in avenues["z"], 500 m), the girder bridge a LOCAL gap (300 m). Both
                   cells must exist and not be the dock / services / power; the 80 m (arch) / 60 m (girder) strip of
                   each cell next to the gap is left free of modules (the abutment lands there).
       "street_any" the girder bridge may also cross a Z street (yaw 90): its top is 24 m, under the lowest lane
                   (100 m, Assisted ships <= 100 m, half of that below the lane).
     A place that has no cell left: drop the landmark (never move a generic module to make room).
  4. Hit volumes and footprints: HITS here; hub_belt._module_obb reads ASSETS (hub_plane._footprint packs by it).
"""
import hashlib
import math
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, DARK, FLOOD, HULL, STEEL, WARM, beacon  # noqa: F401
from units.hub_belt import MARK, ROAD, sphere
from units.hub_style_west import _add, _len, _mul, _sub, mast, strut, truss

register_materials({
    # statues and monuments (plain: no window grid)
    "eq_hubl_bronze": ((0.46, 0.31, 0.17), (0.55, 0.45, 0.30), 70),
    "eq_hubl_patina": ((0.29, 0.50, 0.42), (0.20, 0.25, 0.22), 30),
    "eq_hubl_marble": ((0.85, 0.83, 0.78), (0.25, 0.25, 0.24), 40),
    "eq_hubl_granite": ((0.27, 0.26, 0.27), (0.20, 0.20, 0.20), 40),
    "eq_hubl_iron": ((0.20, 0.21, 0.23), (0.35, 0.35, 0.36), 50),
    "eq_hubl_gilt": ((0.80, 0.62, 0.24), (0.85, 0.75, 0.45), 140),
    "eq_hubl_chrome": ((0.62, 0.65, 0.70), (0.90, 0.90, 0.92), 160),
    "eq_hubl_paving": ((0.50, 0.49, 0.46), (0.06, 0.06, 0.06), 10),
    "eq_hubl_paving_b": ((0.36, 0.35, 0.33), (0.06, 0.06, 0.06), 10),
    "eq_hubl_lawn": ((0.21, 0.36, 0.15), (0.02, 0.02, 0.02), 5),
    "eq_hubl_tree": ((0.12, 0.27, 0.11), (0.02, 0.02, 0.02), 5),
    "eq_hubl_clockface": ((0.92, 0.89, 0.76), (0.10, 0.10, 0.10), 10),
    "eq_hubl_bridge_red": ((0.62, 0.20, 0.12), (0.30, 0.25, 0.22), 40),       # painted bridge steel (reads far off)
    "eq_hubl_water": ((0.10, 0.20, 0.28), (0.70, 0.75, 0.80), 150),
    # tower skins (eq_hub_facade prefix: lit windows)
    "eq_hub_facade_lm_white": ((0.82, 0.81, 0.78), (0.20, 0.20, 0.20), 40),
    "eq_hub_facade_lm_glass": ((0.13, 0.22, 0.30), (0.65, 0.66, 0.70), 130),
    "eq_hub_facade_lm_copper": ((0.52, 0.33, 0.22), (0.45, 0.38, 0.30), 70),
    "eq_hub_facade_lm_stone": ((0.62, 0.56, 0.47), (0.10, 0.10, 0.10), 20),
})
BRONZE, PATINA, MARBLE, GRANITE, IRON = "eq_hubl_bronze", "eq_hubl_patina", "eq_hubl_marble", "eq_hubl_granite", "eq_hubl_iron"
GILT, CHROME, PAVE, PAVE_B = "eq_hubl_gilt", "eq_hubl_chrome", "eq_hubl_paving", "eq_hubl_paving_b"
RED = "eq_hubl_bridge_red"
LAWN, TREE, CLOCK, WATER = "eq_hubl_lawn", "eq_hubl_tree", "eq_hubl_clockface", "eq_hubl_water"
F_WHITE, F_GLASS, F_COPPER, F_STONE = ("eq_hub_facade_lm_white", "eq_hub_facade_lm_glass", "eq_hub_facade_lm_copper",
                                       "eq_hub_facade_lm_stone")


def _seg(lod, n):
    return max(4, n if lod == 0 else n * 2 // 3 if lod == 1 else n // 2)


# ---------------------------------------------------------------- shape helpers
def frustum(p, mat, a, b, r0, r1, seg):
    """a capped cone frustum from a (radius r0) to b (radius r1)"""
    d = _sub(b, a)
    L = _len(d)
    if L < 1e-6:
        return
    lathe(p, mat, [(0, 0), (r0, 0), (r1, L), (0, L)], seg, axis_to(_mul(d, 1.0 / L)), a)


def ball(p, mat, c, r, seg, sy=1.0):
    """an ellipsoid (radius r, squashed by sy along Y)"""
    n = max(4, seg // 2)
    prof = [(r * math.sin(math.pi * k / n), c[1] - sy * r * math.cos(math.pi * k / n)) for k in range(n + 1)]
    prof[0], prof[-1] = (0.0, c[1] - sy * r), (0.0, c[1] + sy * r)
    ylathe(p, mat, prof, seg, t=(c[0], 0, c[2]), soft=50)


def _put(dst, src):
    base = len(dst.v)
    dst.v += src.v
    dst.n += src.n
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + base for i in fc) for fc in fl)


def torus(p, mat, c, R, r, seg, segt, Rm=None):
    """a ring (major radius R, tube r) round the Y axis, turned by Rm and centred on c"""
    tmp = Part("_t")
    prof = [(R + r * math.cos(2 * math.pi * k / segt), r * math.sin(2 * math.pi * k / segt)) for k in range(segt + 1)]
    ylathe(tmp, mat, prof, seg, soft=60)
    _put(p, placed(tmp, Rm or I3, c))


def tree(p, x, z, y, s, lod):
    """a park tree: trunk + crown (s = height)"""
    if lod >= 2:
        box(p, TREE, (x, y + s * 0.55, z), (s * 0.5, s * 0.7, s * 0.5))
        return
    box(p, IRON, (x, y + s * 0.2, z), (s * 0.08, s * 0.4, s * 0.08))
    ball(p, TREE, (x, y + s * 0.62, z), s * 0.32, 6, 1.2)


def flood_masts(p, pts, y0, h, lod, w=1.4):
    """lattice floodlight masts round a monument (the trusses every structure carries)"""
    for x, z in pts:
        truss(p, (x, y0, z), (x, y0 + h, z), w, lod, bay=4.0 * w)
        box(p, FLOOD, (x, y0 + h + 1.2, z), (6.0, 2.4, 2.0))


def plinth(p, lod, w, d, h, mat=GRANITE, band=BRONZE):
    """a stepped monument plinth w x d, h tall (two steps, a cornice, a bronze plaque band)"""
    box(p, mat, (0, 3.0, 0), (w + 24.0, 6.0, d + 24.0))
    box(p, mat, (0, 9.0, 0), (w + 12.0, 6.0, d + 12.0))
    box(p, mat, (0, 12.0 + (h - 16.0) / 2, 0), (w, h - 16.0, d))
    box(p, mat, (0, h - 2.0, 0), (w + 6.0, 4.0, d + 6.0))
    if lod < 2:
        box(p, band, (0, h * 0.55, d / 2 + 0.3), (w * 0.5, h * 0.18, 0.6))


# ---------------------------------------------------------------- figures (H = figure height; facing +Z)
def _limb(p, mat, pts, r0, r1, seg, joints=True):
    """a two-piece limb through pts (shoulder / hip, elbow / knee, hand / ankle), tapering r0 -> r1"""
    rm = (r0 + r1) / 2
    frustum(p, mat, pts[0], pts[1], r0, rm, seg)
    frustum(p, mat, pts[1], pts[2], rm, r1, seg)
    if joints:
        ball(p, mat, pts[1], rm * 1.02, seg)


def figure(p, lod, mat, y0, H, legs, arms, head="bare", torso=None):
    """a standing figure H tall on y0 (unit h = H / 8): legs / arms = [(hip|shoulder, knee|elbow, ankle|hand)] in h
    units relative to (0, y0, 0); head bare | helmet | hood; torso None = the default trunk"""
    h = H / 8.0
    s = _seg(lod, 10)
    P = lambda q: (q[0] * h, y0 + q[1] * h, q[2] * h)
    for L in legs:
        _limb(p, mat, [P(q) for q in L], 0.52 * h, 0.30 * h, s)
        a = P(L[2])
        box(p, mat, (a[0], a[1] - 0.15 * h, a[2] + 0.3 * h), (0.6 * h, 0.4 * h, 1.15 * h))
    if torso is None:
        box(p, mat, (0, y0 + 4.2 * h, 0), (1.75 * h, 1.0 * h, 1.0 * h))
        frustum(p, mat, P((0, 4.3, 0)), P((0, 6.75, 0)), 0.78 * h, 1.02 * h, s)
    else:
        torso(p, s, h, P)
    for A in arms:
        ball(p, mat, P(A[0]), 0.36 * h, s)
        _limb(p, mat, [P(q) for q in A], 0.32 * h, 0.21 * h, s)
        ball(p, mat, P(A[2]), 0.26 * h, s)
    frustum(p, mat, P((0, 6.7, 0)), P((0, 7.2, 0)), 0.3 * h, 0.28 * h, s)
    if head == "helmet":
        ball(p, mat, P((0, 7.55, 0)), 0.62 * h, s)
        ball(p, DARK, P((0, 7.6, 0.18)), 0.5 * h, s, 0.75)                     # the visor
        frustum(p, mat, P((0, 6.85, 0)), P((0, 7.05, 0)), 0.75 * h, 0.7 * h, s)  # the collar ring
    elif head == "hood":
        ball(p, mat, P((0, 7.55, -0.1)), 0.56 * h, s, 1.15)
        ball(p, DARK, P((0, 7.5, 0.22)), 0.36 * h, s, 1.1)
    else:
        ball(p, mat, P((0, 7.55, 0)), 0.46 * h, s, 1.15)


def statue_pioneer(p, lod):
    """a spacer in a pressure suit and helmet, a beacon torch raised in the right hand, a helmet-shaped life-support
    pack on the back; bronze, 210 m, on a 52 m granite plinth ringed by lattice floodlight masts"""
    y0, H = 52.0, 210.0
    h = H / 8
    plinth(p, lod, 90.0, 90.0, y0)
    legs = [[(-0.5, 4.0, 0), (-0.62, 2.1, 0.15), (-0.7, 0.4, 0)], [(0.5, 4.0, 0), (0.75, 2.1, -0.1), (0.9, 0.4, -0.35)]]
    arms = [[(-1.1, 6.5, 0), (-1.45, 8.0, 0.15), (-1.5, 9.55, 0.3)], [(1.1, 6.5, 0), (1.35, 5.0, 0.15), (1.3, 3.7, 0.4)]]
    figure(p, lod, BRONZE, y0, H, legs, arms, head="helmet")
    box(p, BRONZE, (0, y0 + 5.6 * h, -0.95 * h), (1.5 * h, 2.2 * h, 0.7 * h))     # the life-support pack
    hx, hz = -1.5 * h, 0.3 * h
    frustum(p, BRONZE, (hx, y0 + 9.0 * h, hz), (hx, y0 + 10.4 * h, hz), 0.18 * h, 0.4 * h, _seg(lod, 8))
    ball(p, BEACON, (hx, y0 + 10.75 * h, hz), 0.45 * h, _seg(lod, 10), 1.3)
    flood_masts(p, [(-60.0, 60.0), (60.0, 60.0), (-60.0, -60.0), (60.0, -60.0)], 0.0, 46.0, lod)


def statue_robed(p, lod):
    """a robed, hooded figure holding a gilt orb before it in both hands; marble, 200 m, on a round tiered drum"""
    y0, H = 46.0, 200.0
    h = H / 8
    seg = _seg(lod, 20)
    ylathe(p, GRANITE, [(0, 0), (80.0, 0), (80.0, 8.0), (64.0, 8.0), (64.0, 16.0), (52.0, 16.0), (52.0, 42.0),
                        (56.0, 42.0), (56.0, 46.0), (0, 46.0)], seg)

    def robe(p_, s, h_, P):
        ylathe(p_, MARBLE, [(0, y0), (1.45 * h_, y0), (1.25 * h_, y0 + 1.6 * h_), (0.95 * h_, y0 + 4.2 * h_),
                            (1.05 * h_, y0 + 6.2 * h_), (0.62 * h_, y0 + 6.95 * h_), (0, y0 + 6.95 * h_)], s, soft=35)
    arms = [[(-1.0, 6.4, 0), (-1.25, 5.2, 0.7), (-0.4, 5.75, 1.55)], [(1.0, 6.4, 0), (1.25, 5.2, 0.7), (0.4, 5.75, 1.55)]]
    figure(p, lod, MARBLE, y0, H, [], arms, head="hood", torso=robe)
    for a in arms:                                                              # wide sleeves
        q0 = (a[1][0] * h, y0 + a[1][1] * h, a[1][2] * h)
        q1 = (a[2][0] * h * 1.6, y0 + a[2][1] * h, a[2][2] * h * 0.85)
        frustum(p, MARBLE, q0, q1, 0.3 * h, 0.5 * h, _seg(lod, 8))
    ball(p, GILT, (0, y0 + 6.15 * h, 1.95 * h), 0.6 * h, _seg(lod, 14))
    flood_masts(p, [(-70.0, 70.0), (70.0, 70.0), (0.0, -88.0)], 0.0, 40.0, lod)


def statue_worker(p, lod):
    """a striding worker swinging a sledgehammer over the shoulder; dark iron, 200 m, on a constructivist pedestal
    (a stepped block with a slanted prow) braced by a lattice frame"""
    y0, H = 56.0, 200.0
    h = H / 8
    plinth(p, lod, 110.0, 80.0, y0, GRANITE, IRON)
    box(p, GRANITE, (0, y0 * 0.55, 52.0), (60.0, y0 * 0.9, 20.0))                    # the prow
    legs = [[(-0.5, 4.0, 0), (-0.75, 2.2, 0.7), (-0.9, 0.4, 1.35)], [(0.5, 4.0, 0), (0.7, 2.0, -0.6), (0.85, 0.4, -1.4)]]
    arms = [[(-1.1, 6.5, 0), (-1.2, 7.7, 0.55), (-0.5, 8.8, 0.35)], [(1.1, 6.5, 0), (0.9, 7.6, 0.6), (-0.1, 8.5, 0.5)]]
    figure(p, lod, IRON, y0, H, legs, arms)
    a, b = (0.3 * h, y0 + 8.3 * h, 0.45 * h), (-2.4 * h, y0 + 10.6 * h, 0.4 * h)   # the hammer
    frustum(p, IRON, a, b, 0.14 * h, 0.14 * h, _seg(lod, 6))
    box(p, IRON, (b[0] - 0.2 * h, b[1] + 0.25 * h, b[2]), (1.1 * h, 0.8 * h, 0.8 * h), axis_to(norm((-1.0, -1.15, 0.0))))
    for x in (-62.0, 62.0):                                                          # the bracing frame
        truss(p, (x, 0.0, -48.0), (x, 70.0, -48.0), 2.0, lod, bay=8.0)
    truss(p, (-62.0, 70.0, -48.0), (62.0, 70.0, -48.0), 2.0, lod, bay=8.0)
    box(p, FLOOD, (0, 73.0, -48.0), (40.0, 2.0, 2.0))


def statue_rider(p, lod):
    """a rider on a rearing mount raising a banner; bronze, 205 m to the banner tip, on a long plinth"""
    y0, u = 44.0, 17.0
    plinth(p, lod, 150.0, 70.0, y0)
    s = _seg(lod, 10)
    P = lambda q: (q[0] * u, y0 + q[1] * u, q[2] * u)
    m = BRONZE
    frustum(p, m, P((0, 3.3, -2.0)), P((0, 4.3, 1.9)), 1.0 * u, 0.95 * u, s)          # the mount's barrel (rearing)
    ball(p, m, P((0, 3.3, -2.0)), 1.0 * u, s)
    ball(p, m, P((0, 4.3, 1.9)), 0.95 * u, s)
    _limb(p, m, [P((-0.5, 3.0, -2.1)), P((-0.6, 1.6, -2.6)), P((-0.6, 0.2, -2.2))], 0.42 * u, 0.24 * u, s)  # hind legs
    _limb(p, m, [P((0.5, 3.0, -2.1)), P((0.6, 1.6, -2.4)), P((0.65, 0.2, -1.6))], 0.42 * u, 0.24 * u, s)
    _limb(p, m, [P((-0.5, 4.0, 2.1)), P((-0.55, 3.2, 3.2)), P((-0.5, 3.6, 4.0))], 0.36 * u, 0.22 * u, s)    # forelegs up
    _limb(p, m, [P((0.5, 4.0, 2.1)), P((0.55, 2.9, 2.9)), P((0.5, 2.6, 3.8))], 0.36 * u, 0.22 * u, s)
    frustum(p, m, P((0, 4.6, 2.2)), P((0, 6.6, 3.0)), 0.6 * u, 0.4 * u, s)            # neck
    frustum(p, m, P((0, 6.7, 2.9)), P((0, 6.0, 4.1)), 0.42 * u, 0.26 * u, s)          # head
    frustum(p, m, P((0, 3.5, -2.9)), P((0, 1.7, -3.6)), 0.35 * u, 0.1 * u, s)         # tail
    for sx in (-1, 1):                                                               # rider's legs astride
        _limb(p, m, [P((sx * 0.5, 4.9, 0.0)), P((sx * 1.15, 4.4, 0.6)), P((sx * 1.1, 3.2, 0.3))], 0.38 * u, 0.24 * u, s)
    frustum(p, m, P((0, 4.8, 0.0)), P((0, 7.0, 0.2)), 0.6 * u, 0.72 * u, s)           # rider's trunk
    frustum(p, m, P((0, 7.0, 0.2)), P((0, 7.4, 0.2)), 0.25 * u, 0.25 * u, s)
    ball(p, m, P((0, 7.75, 0.25)), 0.38 * u, s, 1.15)
    _limb(p, m, [P((-0.75, 6.8, 0.2)), P((-1.3, 7.7, 0.4)), P((-1.45, 8.7, 0.7))], 0.26 * u, 0.18 * u, s)  # banner arm
    _limb(p, m, [P((0.75, 6.8, 0.2)), P((1.0, 5.9, 0.9)), P((0.4, 5.6, 1.6))], 0.26 * u, 0.18 * u, s)       # rein arm
    frustum(p, m, P((-1.45, 4.5, 0.7)), P((-1.45, 12.0, 0.7)), 0.1 * u, 0.1 * u, 6)       # the banner pole
    box(p, PATINA, (-1.45 * u, y0 + 10.9 * u, -0.9 * u), (0.12 * u, 1.9 * u, 3.0 * u))
    flood_masts(p, [(-100.0, 55.0), (100.0, 55.0), (-100.0, -55.0), (100.0, -55.0)], 0.0, 42.0, lod)


def statue_hand(p, lod):
    """a giant hand rising out of a plinth, palm up, cradling a gilt globe with a ring; bronze, 300 m"""
    s = _seg(lod, 12)
    plinth(p, lod, 90.0, 90.0, 30.0)
    frustum(p, BRONZE, (0, 20.0, 0), (0, 200.0, 0), 36.0, 26.0, s)              # the forearm, a cuff at its root
    frustum(p, PATINA, (0, 24.0, 0), (0, 44.0, 0), 40.0, 39.0, s)
    box(p, BRONZE, (0, 214.0, 4.0), (66.0, 24.0, 70.0))                         # the palm (up)
    for k, x in enumerate((-24.0, -8.0, 8.0, 24.0)):                             # fingers curling up at the far edge
        L = (1.0, 1.12, 1.06, 0.88)[k]
        a = (x, 220.0, 38.0)
        b = (x, 220.0 + 34.0 * L, 52.0)
        c = (x, 220.0 + 58.0 * L, 42.0)
        _limb(p, BRONZE, [a, b, c], 7.5, 6.0, s)
        ball(p, BRONZE, c, 6.0, s)
    _limb(p, BRONZE, [(36.0, 212.0, 0.0), (52.0, 236.0, 4.0), (48.0, 258.0, 12.0)], 9.0, 6.5, s)   # the thumb
    ball(p, GILT, (0, 262.0, 2.0), 36.0, _seg(lod, 20))                          # the globe
    torus(p, CHROME, (0, 262.0, 2.0), 52.0, 2.2, _seg(lod, 24), 6, rot_x(0.35))
    flood_masts(p, [(-64.0, 64.0), (64.0, 64.0), (-64.0, -64.0), (64.0, -64.0)], 0.0, 50.0, lod)


def statue_head(p, lod):
    """a giant head in a spacer helmet looking out over the city - a neck collar, the helmet shell turned about the
    front axis so its visor is a clean dark cap (50 degrees round) in a gilt rim, ear pods, a lattice antenna mast;
    bronze / patina, 250 m"""
    s = _seg(lod, 18)
    plinth(p, lod, 130.0, 130.0, 24.0)
    ylathe(p, PATINA, [(0, 22.0), (62.0, 22.0), (58.0, 60.0), (52.0, 92.0), (0, 92.0)], s)     # the shoulders / collar
    ylathe(p, BRONZE, [(0, 90.0), (58.0, 90.0), (60.0, 100.0), (0, 100.0)], s)                  # the neck ring
    R, c, cut = 74.0, (0, 168.0, 0), math.radians(130.0)
    n = max(6, s // 2 + 2)
    th = [cut * k / n for k in range(n + 1)]
    lathe(p, BRONZE, [(R * math.sin(t), -R * math.cos(t)) for t in th], s, I3, c, smooth=True, soft=50, ends=None)
    th = [cut + (math.pi - cut) * k / 4 for k in range(5)]
    lathe(p, DARK, [(R * 1.01 * math.sin(t), -R * 1.01 * math.cos(t)) for t in th], s, I3, c, smooth=True, soft=50,
          ends=None)                                                                           # the visor
    torus(p, GILT, (0, 168.0, -R * math.cos(cut)), R * math.sin(cut), 3.0, s, 6, rot_x(math.pi / 2))
    for sx in (-1, 1):
        box(p, BRONZE, (sx * 72.0, 168.0, -6.0), (14.0, 36.0, 30.0))                              # ear pods
    mast(p, -40.0, -30.0, 220.0, 262.0, 2.0, lod)
    flood_masts(p, [(-82.0, 82.0), (82.0, 82.0)], 0.0, 44.0, lod)


def ring_sculpture(p, lod):
    """two interlocked chrome rings standing on edge on a needle pedestal; abstract, 270 m"""
    s = _seg(lod, 32)
    ylathe(p, GRANITE, [(0, 0), (70.0, 0), (70.0, 6.0), (40.0, 14.0), (0, 14.0)], _seg(lod, 16))
    ylathe(p, WATER, [(0, 0), (66.0, 0), (66.0, 7.0), (0, 7.0)], _seg(lod, 16), t=(0, 0.2, 0))   # the pool rim
    R = 85.0
    frustum(p, CHROME, (-20.0, 14.0, 0), (-20.0, 46.0, 0), 10.0, 4.0, _seg(lod, 10))
    # a Hopf link, both rings on edge: A in the XY plane, B in the YZ plane 0.5 R aside and 0.9 R higher - B's disk is
    # pierced by A exactly once
    torus(p, CHROME, (-20.0, 52.0 + R, 0), R, 8.0, s, 8, rot_x(math.pi / 2))
    torus(p, GILT, (22.5, 52.0 + 1.9 * R, 0), R, 8.0, s, 8, rot_z(math.pi / 2))
    flood_masts(p, [(-90.0, 50.0), (90.0, 50.0), (0.0, -90.0)], 0.0, 36.0, lod)


# ---------------------------------------------------------------- towers
def obelisk(p, lod):
    """a 420 m four-sided obelisk: stepped granite base, a white shaft tapering 40 -> 24 m, a gilt pyramidion, beacon
    belts; four lattice floodlight masts"""
    box(p, GRANITE, (0, 4.0, 0), (120.0, 8.0, 120.0))
    box(p, GRANITE, (0, 14.0, 0), (80.0, 12.0, 80.0))
    lathe(p, MARBLE, [(0, 0), (28.3, 0), (17.0, 372.0), (0, 372.0)], 4, R_Y, (0, 20.0, 0), math.pi / 4, math.pi / 4 + 2 * math.pi,
          smooth=False)
    lathe(p, GILT, [(0, 0), (17.0, 0), (0, 30.0)], 4, R_Y, (0, 392.0, 0), math.pi / 4, math.pi / 4 + 2 * math.pi, smooth=False)
    if lod < 2:
        for y in (120.0, 240.0, 360.0):
            w = 2 * (28.3 - (28.3 - 17.0) * (y - 20.0) / 372.0) / math.sqrt(2) + 1.0
            box(p, BEACON, (0, y, 0), (w, 1.5, w))
    beacon(p, (0, 423.0, 0), 3.0)
    flood_masts(p, [(-55.0, 55.0), (55.0, 55.0), (-55.0, -55.0), (55.0, -55.0)], 8.0, 60.0, lod, 1.6)


def twisted_tower(p, lod):
    """a 360 m tower of 36 floor plates 58 x 58 m, each turned 2.5 degrees on the one below (a quarter turn in all),
    white slab edges, an external lattice spine on two corners, a lattice crown and a mast"""
    n, dy, hw = 36, 10.0, 29.0
    box(p, F_STONE, (0, 6.0, 0), (120.0, 12.0, 120.0))
    for k in range(n):
        a = math.radians(2.5 * k)
        sq = [(hw * math.sqrt(2) * math.cos(a + math.pi / 4 + i * math.pi / 2),
               hw * math.sqrt(2) * math.sin(a + math.pi / 4 + i * math.pi / 2)) for i in range(4)]
        y = 12.0 + k * dy
        prism(p, F_GLASS, sq, y, y + dy - (1.2 if lod < 2 else 0.0))
        if lod < 2:
            prism(p, "eq_hubl_marble", [(x * 1.04, z * 1.04) for x, z in sq], y + dy - 1.2, y + dy)
    top = 12.0 + n * dy
    for i in (0, 2):                                                    # the corner spines, stepping round the twist
        pts = []
        for k in range(0, n + 1, 6):
            a = math.radians(2.5 * k) + math.pi / 4 + i * math.pi / 2
            pts.append((hw * 1.5 * math.cos(a), 12.0 + k * dy, hw * 1.5 * math.sin(a)))
        for a_, b_ in zip(pts, pts[1:]):
            truss(p, a_, b_, 2.0, lod, bay=8.0)
    c = 20.0                                                            # the crown cage
    for sx in (-1, 1):
        for sz in (-1, 1):
            truss(p, (sx * c, top, sz * c), (sx * 6.0, top + 40.0, sz * 6.0), 1.2, lod, bay=5.0)
    mast(p, 0.0, 0.0, top, top + 70.0, 1.5, lod)


def skirt_tower(p, lod):
    """a slender 380 m round tower in a wide 'skirt': a flared collar platform (r 150 m) at 150-170 m, carried by
    diagonal lattice struts from the shaft, lights round its rim; a smaller collar at 300 m, a lattice spire"""
    s = _seg(lod, 24)
    ylathe(p, F_STONE, [(0, 0), (60.0, 0), (60.0, 14.0), (0, 14.0)], s)
    ylathe(p, F_WHITE, [(0, 14.0), (34.0, 14.0), (26.0, 380.0), (0, 380.0)], s)
    ylathe(p, F_COPPER, [(0, 150.0), (150.0, 140.0), (152.0, 146.0), (40.0, 172.0), (0, 172.0)], s)   # the skirt
    ylathe(p, F_COPPER, [(0, 296.0), (70.0, 292.0), (71.0, 296.0), (30.0, 312.0), (0, 312.0)], s)
    nst = 12 if lod == 0 else 8 if lod == 1 else 6
    for k in range(nst):
        a = 2 * math.pi * (k + 0.5) / nst
        truss(p, (31.0 * math.cos(a), 70.0, 31.0 * math.sin(a)), (135.0 * math.cos(a), 141.0, 135.0 * math.sin(a)),
              2.2, lod, bay=10.0, diag=(0, 2))
        if lod < 2:
            beacon(p, (151.0 * math.cos(a), 146.0, 151.0 * math.sin(a)), 2.5)
    lattice = 4.0
    for sx in (-1, 1):
        for sz in (-1, 1):
            truss(p, (sx * 12.0, 380.0, sz * 12.0), (sx * 2.0, 440.0, sz * 2.0), 1.0, lod, bay=5.0)
    beacon(p, (0, 442.0, 0), lattice)


def dome_ring(p, lod):
    """a 120 m ribbed dome on a drum, girdled by a Saturn ring (r 190 m) held off it on eight lattice arms; a lantern"""
    s = _seg(lod, 32)
    ylathe(p, F_STONE, [(0, 0), (150.0, 0), (150.0, 10.0), (0, 10.0)], s)
    ylathe(p, F_STONE, [(0, 10.0), (122.0, 10.0), (122.0, 50.0), (0, 50.0)], s)
    prof = [(0, 50.0)] + [(122.0 * math.cos(t), 50.0 + 118.0 * math.sin(t)) for t in [k * math.pi / 2 / 10 for k in range(11)]]
    prof[-1] = (0, 168.0)
    ylathe(p, F_COPPER, prof, s, soft=30)
    if lod < 2:
        for k in range(16 if lod == 0 else 8):                            # ribs
            a = 2 * math.pi * k / (16 if lod == 0 else 8)
            pts = [(123.0 * math.cos(t) * math.cos(a), 50.0 + 119.5 * math.sin(t), 123.0 * math.cos(t) * math.sin(a))
                   for t in [j * math.pi / 2 / 6 for j in range(6)]]
            for a_, b_ in zip(pts, pts[1:]):
                strut(p, MARBLE, a_, b_, 1.4)
    ylathe(p, MARBLE, [(0, 0), (12.0, 0), (12.0, 18.0), (0, 26.0)], _seg(lod, 12), t=(0, 166.0, 0))
    ylathe(p, F_GLASS, [(165.0, 92.0), (205.0, 96.0), (205.0, 100.0), (165.0, 96.0), (165.0, 92.0)], s)     # the ring
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        r_in = 122.0 * math.cos(math.asin((95.0 - 50.0) / 118.0))
        truss(p, ((r_in - 2.0) * math.cos(a), 95.0, (r_in - 2.0) * math.sin(a)), (168.0 * math.cos(a), 95.0, 168.0 * math.sin(a)),
              2.5, lod, bay=10.0)
        if lod < 2:
            beacon(p, (206.0 * math.cos(a), 98.0, 206.0 * math.sin(a)), 2.5)
    beacon(p, (0, 194.0, 0), 3.0)


def clock_tower(p, lod):
    """a 260 m stone tower, a clock stage with four lit faces (r 26 m), an open lattice belfry and a beacon lantern on a
    lattice spire to 420 m"""
    box(p, F_STONE, (0, 6.0, 0), (90.0, 12.0, 90.0))
    box(p, F_STONE, (0, 136.0, 0), (54.0, 248.0, 54.0))
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(p, F_STONE, (sx * 28.0, 136.0, sz * 28.0), (8.0, 248.0, 8.0))           # corner buttresses
    box(p, F_COPPER, (0, 295.0, 0), (72.0, 70.0, 72.0))                                  # the clock stage
    box(p, MARBLE, (0, 331.0, 0), (78.0, 2.0, 78.0))
    seg = _seg(lod, 24)
    for d in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        c = (d[0] * 36.0, 295.0, d[1] * 36.0)
        e = (d[0] * 37.5, 295.0, d[1] * 37.5)
        cyl(p, CLOCK, c, e, 26.0, seg)
        if lod < 2:                                                                       # the hands: ten past two
            t = (d[1], 0, -d[0])                                                          # along the face
            q = (e[0] + d[0] * 0.5, 295.0, e[2] + d[1] * 0.5)
            box(p, DARK, (q[0] + t[0] * 8.0, 295.0, q[2] + t[2] * 8.0), (1.0 + abs(t[0]) * 16.0, 2.4, 1.0 + abs(t[2]) * 16.0))
            box(p, DARK, (q[0], 295.0 + 11.0, q[2]), (1.0 + abs(t[0]) * 1.6, 22.0, 1.0 + abs(t[2]) * 1.6))
    for sx in (-1, 1):                                                                    # the lattice belfry
        for sz in (-1, 1):
            truss(p, (sx * 30.0, 332.0, sz * 30.0), (sx * 8.0, 400.0, sz * 8.0), 2.0, lod, bay=8.0)
    for y, c in ((354.0, 23.0), (378.0, 15.5)):
        for (ax, az), (bx, bz) in (((-c, -c), (c, -c)), ((c, -c), (c, c)), ((c, c), (-c, c)), ((-c, c), (-c, -c))):
            truss(p, (ax, y, az), (bx, y, bz), 1.2, min(lod + 1, 2), bay=6.0)
    ball(p, GILT, (0, 360.0, 0), 10.0, _seg(lod, 12))                                     # the bell
    mast(p, 0.0, 0.0, 400.0, 420.0, 1.5, lod, top=False)
    ball(p, BEACON, (0, 424.0, 0), 5.0, _seg(lod, 10), 1.3)


def garden_terrace(p, lod):
    """hanging gardens: six stepped terraces (270 -> 120 m, 36 m each), lawns and tree rows on every step, vines hanging
    down the walls, lattice pergolas cantilevered off the corners, a waterfall slab"""
    rng = random.Random(7)
    y = 0.0
    tiers = [(270.0, 36.0), (240.0, 36.0), (210.0, 36.0), (180.0, 36.0), (150.0, 36.0), (120.0, 30.0)]
    for k, (w, hgt) in enumerate(tiers):
        box(p, F_STONE, (0, y + hgt / 2, 0), (w, hgt, w))
        nxt = tiers[k + 1][0] if k + 1 < len(tiers) else 0.0
        box(p, LAWN, (0, y + hgt + 0.3, 0), (w - 2.0, 0.6, w - 2.0))
        if lod < 2:
            m = (w + nxt) / 4
            step = 36.0 if lod == 0 else 54.0
            for side in range(4):
                t = -w / 2 + 12.0
                while t < w / 2 - 10.0:
                    x, z = [(t, m), (t, -m), (m, t), (-m, t)][side]
                    if nxt == 0.0 or max(abs(x), abs(z)) > nxt / 2 + 4.0:
                        tree(p, x, z, y + hgt + 0.6, rng.uniform(10.0, 15.0), lod)
                    t += step
            for side in range(4):                                         # vines down the walls
                for j in range(6 if lod == 0 else 3):
                    t = -w / 2 + (j + 0.5) * w / (6 if lod == 0 else 3)
                    L = rng.uniform(12.0, 26.0)
                    x, z = [(t, w / 2 + 0.6), (t, -w / 2 - 0.6), (w / 2 + 0.6, t), (-w / 2 - 0.6, t)][side]
                    box(p, TREE, (x, y + hgt - L / 2, z), (8.0 if side < 2 else 1.0, L, 1.0 if side < 2 else 8.0))
        y += hgt
    box(p, WATER, (0, 104.0, 136.0), (30.0, 200.0, 2.0))                    # the waterfall down the +z face
    for sx in (-1, 1):                                                     # lattice pergolas off the top corners
        for sz in (-1, 1):
            truss(p, (sx * 60.0, y, sz * 60.0), (sx * 60.0, y + 30.0, sz * 60.0), 1.5, lod, bay=6.0)
            truss(p, (sx * 60.0, y + 30.0, sz * 60.0), (sx * 110.0, y - 20.0, sz * 110.0), 1.5, lod, bay=6.0)
    for (ax, az), (bx, bz) in (((-60, -60), (60, -60)), ((60, -60), (60, 60)), ((60, 60), (-60, 60)), ((-60, 60), (-60, -60))):
        truss(p, (ax, y + 30.0, az), (bx, y + 30.0, bz), 1.5, min(lod + 1, 2), bay=8.0)
    mast(p, 0.0, 0.0, y, y + 60.0, 1.5, lod)


def gate(p, lod):
    """a monumental gate 380 m wide, 300 m high: two stone pylons, a round arch between them (opening 180 m wide),
    a deep attic with a gilt quadriga-like group, flag masts and lattice light pylons; the passage runs along Z"""
    seg = _seg(lod, 24)
    for sx in (-1, 1):
        box(p, F_STONE, (sx * 140.0, 132.5, 0), (100.0, 265.0, 90.0))
        box(p, MARBLE, (sx * 140.0, 4.0, 0), (112.0, 8.0, 102.0))
        if lod < 2:
            for x in (-36.0, -12.0, 12.0, 36.0):                           # pilasters
                box(p, MARBLE, (sx * 140.0 + x, 115.0, 46.0), (8.0, 220.0, 3.0))
                box(p, MARBLE, (sx * 140.0 + x, 115.0, -46.0), (8.0, 220.0, 3.0))
    lathe(p, F_STONE, [(90.0, -45.0), (130.0, -45.0), (130.0, 45.0), (90.0, 45.0), (90.0, -45.0)], seg, I3, (0, 140.0, 0),
          0.0, math.pi)                                                    # the arch (springs at 140 m)
    box(p, F_STONE, (0, 248.5, 0), (190.0, 33.0, 88.0))                    # the spandrel wall over the arch
    box(p, MARBLE, (0, 268.0, 0), (390.0, 6.0, 100.0))                       # the cornice
    box(p, F_STONE, (0, 286.0, 0), (360.0, 30.0, 86.0))                      # the attic
    box(p, GILT, (0, 302.0, 0), (60.0, 2.0, 30.0))
    for k in range(4):                                                     # the group on top: four rearing blocks + figure
        x = -21.0 + k * 14.0
        frustum(p, GILT, (x, 302.0, -6.0), (x, 322.0, 6.0), 4.0, 3.5, _seg(lod, 6))
    frustum(p, GILT, (0, 302.0, -12.0), (0, 336.0, -12.0), 4.5, 3.0, _seg(lod, 6))
    ball(p, GILT, (0, 339.0, -12.0), 4.0, _seg(lod, 8))
    for sx in (-1, 1):
        mast(p, sx * 170.0, 0.0, 301.0, 345.0, 1.2, lod)
        for sz in (-1, 1):
            truss(p, (sx * 230.0, 0.0, sz * 60.0), (sx * 230.0, 70.0, sz * 60.0), 1.6, lod, bay=6.0)
            box(p, FLOOD, (sx * 230.0, 72.0, sz * 60.0), (2.0, 2.0, 8.0))
    box(p, PAVE, (0, 0.3, 0), (180.0, 0.6, 400.0))                           # the processional way through


# ---------------------------------------------------------------- squares
def _paving(p, lod, half=290.0, grid=58.0):
    box(p, PAVE, (0, 0.5, 0), (2 * half, 1.0, 2 * half))
    if lod < 2:
        k = -half + grid
        while k < half - 1.0:
            box(p, PAVE_B, (k, 1.05, 0), (2.5, 0.1, 2 * half))
            box(p, PAVE_B, (0, 1.05, k), (2 * half, 0.1, 2.5))
            k += grid


def _edge_lamps(p, lod, half=290.0, step=116.0, h=28.0):
    k = -half + 10.0
    while k <= half - 9.0:
        for x, z in ((k, half - 8.0), (k, -half + 8.0), (half - 8.0, k), (-half + 8.0, k)):
            truss(p, (x, 1.0, z), (x, 1.0 + h, z), 0.9, min(lod + 1, 2), bay=4.0)
            if lod < 2:
                box(p, FLOOD, (x, h + 2.0, z), (3.0, 1.0, 3.0))
        k += step


def square(p, lod):
    """the very wide square: the whole block paved in a 58 m grid, rows of trees along two edges, lattice lamp masts
    round it - and in its +x +z corner a 190 m column on a stepped base with a gilt figure, four fountains round it"""
    _paving(p, lod)
    _edge_lamps(p, lod)
    rng = random.Random(3)
    for k in range(12 if lod == 0 else 6):
        t = -250.0 + k * (500.0 / (11 if lod == 0 else 5))
        tree(p, t, -262.0, 1.0, rng.uniform(14.0, 18.0), lod)
        tree(p, -262.0, t, 1.0, rng.uniform(14.0, 18.0), lod)
    cx, cz = 200.0, 200.0
    s = _seg(lod, 16)
    box(p, GRANITE, (cx, 4.0, cz), (70.0, 6.0, 70.0))
    box(p, GRANITE, (cx, 11.0, cz), (46.0, 8.0, 46.0))
    box(p, GRANITE, (cx, 26.0, cz), (30.0, 22.0, 30.0))
    ylathe(p, MARBLE, [(0, 0), (11.0, 0), (8.5, 150.0), (0, 150.0)], s, t=(cx, 37.0, cz))
    box(p, BRONZE, (cx, 190.0, cz), (24.0, 6.0, 24.0))
    fig = Part("_f")                                       # the gilt figure on the column (built on the origin, moved)
    figure(fig, lod, GILT, 193.0, 32.0, [[(-0.5, 4.0, 0), (-0.6, 2.1, 0.1), (-0.65, 0.4, 0)],
                                       [(0.5, 4.0, 0), (0.6, 2.1, 0.1), (0.65, 0.4, 0)]],
           [[(-1.1, 6.5, 0), (-1.6, 7.6, 0.6), (-2.0, 8.8, 1.0)], [(1.1, 6.5, 0), (1.3, 5.0, 0.1), (1.3, 3.7, 0.3)]])
    _put(p, placed(fig, rot_y(math.pi * 1.25), (cx, 0.0, cz)))   # facing the square's centre
    for dx, dz in ((-60.0, 0.0), (0.0, -60.0), (60.0, 0.0), (0.0, 60.0)):
        if abs(cx + dx) < 280.0 and abs(cz + dz) < 280.0:
            ylathe(p, GRANITE, [(0, 0), (14.0, 0), (14.0, 3.0), (0, 3.0)], s, t=(cx + dx, 1.0, cz + dz))
            ylathe(p, WATER, [(0, 0), (12.0, 0), (12.0, 0.4), (0, 0.4)], s, t=(cx + dx, 4.0, cz + dz))
    flood_masts(p, [(cx - 44.0, cz - 44.0), (cx + 44.0, cz - 44.0), (cx - 44.0, cz + 44.0)], 1.0, 40.0, lod)


def square_open(p, lod):
    """the square's second half: the paving and the lamp masts alone (a wide_square pairs it with hubl_square)"""
    _paving(p, lod)
    _edge_lamps(p, lod)
    box(p, WATER, (0, 1.1, 0), (80.0, 0.2, 80.0))
    box(p, GRANITE, (0, 1.5, 0), (84.0, 1.0, 2.0))
    box(p, GRANITE, (0, 1.5, 0), (2.0, 1.0, 84.0))


def plaza_deck(p, lod):
    """a paved deck over a LOCAL street gap (300 x 580 m, local +X across the gap): joins two square blocks into one
    plaza; carried by six Warren trusses spanning the gap, lamp masts along its edges"""
    box(p, PAVE, (0, 0.5, 0), (300.0, 1.0, 580.0))
    box(p, HULL, (0, -1.5, 0), (300.0, 3.0, 580.0))
    if lod < 2:
        for z in range(-232, 233, 58):
            box(p, PAVE_B, (0, 1.05, float(z)), (300.0, 0.1, 2.5))
    for z in (-250.0, -150.0, -50.0, 50.0, 150.0, 250.0):
        truss(p, (-160.0, -10.0, z), (160.0, -10.0, z), 6.0, lod, bay=20.0, diag=(0, 2))
    for x in (-110.0, 0.0, 110.0):
        for z in (-282.0, 282.0):
            truss(p, (x, 1.0, z), (x, 29.0, z), 0.9, min(lod + 1, 2), bay=4.0)
            box(p, FLOOD, (x, 30.0, z), (3.0, 1.0, 3.0))


# ---------------------------------------------------------------- bridges (along local +Z, the origin on the gap centre)
ARCH_GAP, GIRDER_GAP = 500.0, 300.0


def bridge_arch(p, lod):
    """a through-arch bridge over an AVENUE gap (500 m clear): a 40 m deck at y 14 over the gap and 20 m onto each
    block, then a 60 m ramp down to the block's street level; two lattice arch ribs (painted, crown 205 m) leaning in,
    hangers every 25 m, wind bracing over the deck, granite abutments with pier pylons and lamp masts"""
    half = ARCH_GAP / 2
    D = half + 20.0                                                        # the level deck's half-length
    box(p, HULL, (0, 10.0, 0), (40.0, 8.0, 2 * D))
    box(p, ROAD, (0, 14.1, 0), (28.0, 0.2, 2 * D))
    if lod < 2:
        z = -D + 6.0
        while z < D - 6.0:
            box(p, MARK, (0, 14.25, z), (0.4, 0.1, 6.0))
            z += 12.0
        for sx in (-1, 1):
            box(p, STEEL, (sx * 19.5, 15.5, 0), (0.6, 3.0, 2 * D))
    for sz in (-1, 1):                                                     # the abutments (on the blocks' edges)
        box(p, GRANITE, (0, 7.0, sz * (half + 10.0)), (60.0, 14.0, 20.0))
        d = norm((0.0, -14.0, sz * 60.0))                                  # the ramp down to the block
        box(p, ROAD, (0, 7.2, sz * (D + 30.0)), (28.0, 0.6, math.hypot(14.0, 60.0)), axis_to(d))
        box(p, GRANITE, (0, 3.5, sz * (D + 15.0)), (32.0, 7.0, 30.0))
        for sx in (-1, 1):
            box(p, GRANITE, (sx * 30.0, 26.0, sz * (half + 8.0)), (14.0, 24.0, 16.0))     # pier pylons by the road
            truss(p, (sx * 30.0, 38.0, sz * (half + 8.0)), (sx * 30.0, 70.0, sz * (half + 8.0)), 1.2, lod, bay=5.0)
            box(p, FLOOD, (sx * 30.0, 72.0, sz * (half + 8.0)), (3.0, 2.0, 3.0))
    yc, ys = 205.0, 14.0
    n = 12 if lod == 0 else 8 if lod == 1 else 6
    lean = 8.0

    def rib(sx, t):                                                        # t in -1..1 along the span
        z = t * half
        y = ys + (yc - ys) * (1.0 - t * t)
        return (sx * (22.0 - lean * (1.0 - t * t)), y, z)
    for sx in (-1, 1):
        pts = [rib(sx, -1.0 + 2.0 * k / n) for k in range(n + 1)]
        for a, b in zip(pts, pts[1:]):
            truss(p, a, b, 4.5, lod, bay=14.0, diag=(0, 2), mat=RED)
            strut(p, RED, _add(a, (0, 5.5, 0)), _add(b, (0, 5.5, 0)), 2.6)        # solid chords: the arch reads far off
            strut(p, RED, _add(a, (0, -5.5, 0)), _add(b, (0, -5.5, 0)), 2.0)
    nh = 19 if lod < 2 else 9
    for k in range(1, nh):                                                 # hangers
        t = -1.0 + 2.0 * k / nh
        for sx in (-1, 1):
            a = rib(sx, t)
            if a[1] - 18.0 > 6.0:
                strut(p, STEEL, (a[0], a[1] - 3.5, a[2]), (sx * 19.0, 14.0, a[2]), 0.5)
    for k in range(-3, 4):                                                 # wind bracing between the ribs
        t = k / 5.0
        a, b = rib(-1, t), rib(1, t)
        if a[1] > 110.0:
            truss(p, (a[0] + 3.5, a[1], a[2]), (b[0] - 3.5, b[1], b[2]), 1.5, min(lod + 1, 2), bay=6.0)
    if lod < 2:
        beacon(p, (0, yc + 6.0, 0), 3.0)
        for sx in (-1, 1):
            for t in (-0.6, 0.0, 0.6):
                a = rib(sx, t)
                beacon(p, (a[0], a[1] + 4.0, a[2]), 1.5)


def bridge_girder(p, lod):
    """a flat girder bridge over a LOCAL street gap (300 m clear, 60 m into each block): a 36 m deck at y 8 on two deep
    box girders, low Warren truss parapets on both sides, lamp posts; the top stays under 24 m (below every lane)"""
    half = GIRDER_GAP / 2
    L = half + 60.0
    box(p, HULL, (0, 5.0, 0), (36.0, 6.0, 2 * L))
    box(p, ROAD, (0, 8.1, 0), (26.0, 0.2, 2 * L))
    for sx in (-1, 1):
        box(p, STEEL, (sx * 12.0, -8.0, 0), (6.0, 20.0, 2 * half))               # the box girders, within the gap
        box(p, STEEL, (sx * 12.0, -22.0, 0), (6.0, 8.0, 2 * half - 140.0))
        truss(p, (sx * 17.0, 14.0, -L + 10.0), (sx * 17.0, 14.0, L - 10.0), 5.5, lod, bay=16.0, diag=(1, 3))
    if lod < 2:
        z = -L + 6.0
        while z < L - 6.0:
            box(p, MARK, (0, 8.25, z), (0.4, 0.1, 6.0))
            z += 12.0
        for z in (-120.0, -40.0, 40.0, 120.0):
            for sx in (-1, 1):
                box(p, FLOOD, (sx * 11.0, 22.0, z), (2.0, 1.0, 2.0))
    for sz in (-1, 1):
        box(p, GRANITE, (0, 4.0, sz * (half + 30.0)), (50.0, 8.0, 60.0))


# ================================================================ registration
KIT = {
    # id: (builder, hits, title, kind, place, span)
    "hubl_statue_pioneer": (statue_pioneer, "statue", "plaza", 1, "Статуя первопроходца с факелом-маяком"),
    "hubl_statue_robed": (statue_robed, "statue", "plaza", 1, "Статуя фигуры в мантии с золотой сферой"),
    "hubl_statue_worker": (statue_worker, "statue", "plaza", 1, "Статуя рабочего с молотом"),
    "hubl_statue_rider": (statue_rider, "statue", "plaza", 1, "Конная статуя со знаменем"),
    "hubl_statue_hand": (statue_hand, "statue", "plaza", 1, "Гигантская рука, держащая глобус"),
    "hubl_statue_head": (statue_head, "statue", "plaza", 1, "Гигантская голова в шлеме"),
    "hubl_ring_sculpture": (ring_sculpture, "statue", "plaza", 1, "Скульптура из двух сцепленных колец"),
    "hubl_obelisk": (obelisk, "tower", "downtown", 1, "Обелиск 420 м"),
    "hubl_twisted_tower": (twisted_tower, "tower", "downtown", 1, "Закрученная башня"),
    "hubl_skirt_tower": (skirt_tower, "tower", "downtown", 1, "Башня с «юбкой» - кольцевой платформой"),
    "hubl_dome_ring": (dome_ring, "tower", "downtown", 1, "Купол с кольцом"),
    "hubl_clock_tower": (clock_tower, "tower", "downtown", 1, "Часовая башня с маяком"),
    "hubl_garden_terrace": (garden_terrace, "tower", "residential", 1, "Висячие сады"),
    "hubl_gate": (gate, "gate", "axis", 1, "Монументальные ворота"),
    "hubl_square": (square, "square", "square", 1, "Широкая площадь с колонной в углу"),
    "hubl_square_open": (square_open, "square", "square", 1, "Широкая площадь (вторая половина)"),
    "hubl_plaza_deck": (plaza_deck, "square", "street_local", 2, "Настил площади над улицей"),
    "hubl_bridge_arch": (bridge_arch, "bridge", "street_x", 2, "Арочный мост над проспектом"),
    "hubl_bridge_girder": (bridge_girder, "bridge", "street_any", 2, "Балочный мост над улицей"),
}


def _one(name, fn):
    def build(lod):
        P = {name: Part(name)}
        fn(P[name], lod)
        return P
    return build


def _bounds(name):
    vs = [v for part in ASSETS[name](2).values() for v in part.v]
    lo = [min(v[i] for v in vs) for i in range(3)]
    hi = [max(v[i] for v in vs) for i in range(3)]
    return lo, hi


def _hits_of(name):
    """coarse hit volumes: the module's bounds split into a base slab and a body (bridges: deck + the arch envelope)"""
    def hits():
        if name == "hubl_bridge_arch":
            L = ARCH_GAP / 2 + 80.0
            H = [hbox(name, (0, 10.0, 0), (40.0, 10.0, 2 * L))]
            for k in range(-2, 3):                                 # the ribs in five pieces
                t0, t1 = (k - 0.5) / 2.5, (k + 0.5) / 2.5
                y0 = 14.0 + 191.0 * (1 - t0 * t0)
                y1 = 14.0 + 191.0 * (1 - t1 * t1)
                z0, z1 = t0 * ARCH_GAP / 2, t1 * ARCH_GAP / 2
                H.append(hbox(name, (0, (y0 + y1) / 2, (z0 + z1) / 2), (56.0, abs(y1 - y0) + 12.0, abs(z1 - z0))))
            return H
        if name == "hubl_bridge_girder":
            L = GIRDER_GAP / 2 + 60.0
            return [hbox(name, (0, 0.0, 0), (40.0, 52.0, 2 * L))]
        lo, hi = _bounds(name)
        base = min(hi[1], 14.0)
        H = [hbox(name, ((lo[0] + hi[0]) / 2, (lo[1] + base) / 2, (lo[2] + hi[2]) / 2), (hi[0] - lo[0], base - lo[1], hi[2] - lo[2]))]
        if hi[1] > base + 1.0:
            w = min(hi[0] - lo[0], hi[2] - lo[2]) * 0.6 if KIT[name][1] in ("statue", "tower") else hi[0] - lo[0]
            d = w if KIT[name][1] in ("statue", "tower") else hi[2] - lo[2]
            H.append(hbox(name, (0, (base + hi[1]) / 2, 0), (w, hi[1] - base, d)))
        return H
    return hits


ASSETS = {k: _one(k, v[0]) for k, v in KIT.items()}
HITS = {k: _hits_of(k) for k in KIT}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[4], "hub_module", "Лендмарк хаба (tools/models/units/hub_landmarks.py).") for k, v in KIT.items()}
# what the generator needs to place one (see the docstring): kind, place, span in cells, title
LANDMARKS = {k: {"kind": v[1], "place": v[2], "span": v[3], "title": v[4]} for k, v in KIT.items()}


# ================================================================ the picker
SIGNATURES = {
    "wide_square": "Очень широкая площадь с колонной в углу",
    "ring_boulevard": "Низкое зелёное кольцо вокруг диска",
    "diagonal_avenue": "Диагональный проспект от центра к краю",
    "central_void": "Пустота в квартале у центра",
    "canal": "Проспект-канал под арочным мостом",
}
STATUES = ["hubl_statue_pioneer", "hubl_statue_robed", "hubl_statue_worker", "hubl_statue_rider", "hubl_statue_hand",
           "hubl_statue_head", "hubl_ring_sculpture"]
TOWERS = ["hubl_obelisk", "hubl_twisted_tower", "hubl_skirt_tower", "hubl_dome_ring", "hubl_clock_tower",
          "hubl_garden_terrace"]
# the statue flavour of a national style (hub_styles): the first entries weigh more
STYLE_STATUES = {
    "": ["hubl_statue_pioneer", "hubl_statue_hand", "hubl_ring_sculpture", "hubl_statue_head", "hubl_statue_rider"],
    "ultramodern": ["hubl_ring_sculpture", "hubl_statue_hand", "hubl_statue_head"],
    "ancient": ["hubl_statue_robed", "hubl_statue_rider", "hubl_statue_head"],
    "indian": ["hubl_statue_robed", "hubl_statue_hand", "hubl_statue_rider"],
    "practical": ["hubl_statue_worker", "hubl_statue_head", "hubl_statue_pioneer"],
    "belt_clans": ["hubl_statue_worker", "hubl_statue_pioneer", "hubl_statue_hand"],
    "american": ["hubl_statue_pioneer", "hubl_statue_rider", "hubl_statue_hand"],
}
STYLE_TOWERS = {
    "ultramodern": ["hubl_twisted_tower", "hubl_skirt_tower", "hubl_dome_ring"],
    "ancient": ["hubl_obelisk", "hubl_dome_ring", "hubl_garden_terrace", "hubl_clock_tower"],
    "indian": ["hubl_dome_ring", "hubl_garden_terrace", "hubl_obelisk"],
    "practical": ["hubl_clock_tower", "hubl_skirt_tower", "hubl_obelisk"],
    "belt_clans": ["hubl_skirt_tower", "hubl_clock_tower", "hubl_twisted_tower"],
    "american": ["hubl_clock_tower", "hubl_obelisk", "hubl_skirt_tower", "hubl_dome_ring"],
}


def _seed(hub_id):
    return int(hashlib.sha1(hub_id.encode("utf-8")).hexdigest()[:12], 16)


def _weighted(rng, pool, favoured):
    """one of pool: the style's favoured ones (in order) weigh 4, 3, 2..., the rest 1"""
    w = [max(1, 5 - favoured.index(m)) if m in favoured else 1 for m in pool]
    return rng.choices(pool, weights=w)[0]


def _style_of(hub_id):
    try:
        from units.hub_plane import hub_profile
        return (hub_profile(hub_id) or {}).get("style", "")
    except Exception:  # noqa: BLE001 - no profile table: the default kit
        return ""


def landmarks_for(hub_id, style=None):
    """the hub's landmarks and its block-layout signature (see the module docstring for how the generator places
    them); pure in hub_id (+ style)"""
    if style is None:
        style = _style_of(hub_id)
    seed = _seed(hub_id)
    rng = random.Random(seed ^ 0x1A4D3A2C)
    sig = rng.choice(sorted(SIGNATURES))
    signature = {"name": sig, "title": SIGNATURES[sig], "cards": []}
    picks = []
    if sig == "wide_square":
        signature["cards"] = [{"module": "hubl_square", "place": "square"}, {"module": "hubl_plaza_deck", "place": "street_local"},
                              {"module": "hubl_square_open", "place": "square"}]
    elif sig == "canal":
        signature["axis"] = "x"            # an X street: only there do the lanes fly above the arch (320 / 430 m)
        picks.append("hubl_bridge_arch")
    elif sig == "diagonal_avenue":
        signature["dir"] = list(rng.choice(((1, 1), (1, -1), (-1, 1), (-1, -1))))
        picks.append("hubl_gate")
    elif sig == "central_void":
        signature["dir"] = list(rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1))))
    n = 2 + rng.randrange(3)                                              # 2..4 landmarks
    picks.append(_weighted(rng, STATUES, STYLE_STATUES.get(style, STYLE_STATUES[""])))
    picks.append(_weighted(rng, [t for t in TOWERS if t not in picks], STYLE_TOWERS.get(style, [])))
    extras = [m for m in STATUES + TOWERS + ["hubl_gate", "hubl_bridge_girder", "hubl_bridge_arch"] if m not in picks]
    while len(picks) < n:
        m = _weighted(rng, extras, STYLE_TOWERS.get(style, []) + STYLE_STATUES.get(style, []))
        extras.remove(m)
        if LANDMARKS[m]["kind"] == "bridge" and any(LANDMARKS[q]["kind"] == "bridge" for q in picks):
            continue                                                       # one bridge a city
        if LANDMARKS[m]["kind"] == "statue" and sum(LANDMARKS[q]["kind"] == "statue" for q in picks) >= 2:
            continue
        picks.append(m)
    picks = picks[:max(n, 2)] if len(picks) > 4 else picks
    return {"hub_id": hub_id, "style": style, "seed": seed, "signature": signature,
            "landmarks": [{"module": m, "title": LANDMARKS[m]["title"], "kind": LANDMARKS[m]["kind"],
                           "span": LANDMARKS[m]["span"], "place": LANDMARKS[m]["place"]} for m in picks]}
