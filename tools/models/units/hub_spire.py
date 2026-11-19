"""Spire cluster hub shape (author's reference 2026-11-19: D:/tmp/refs/yt_lqpHpz6Tv2s/maxresdefault.jpg - a night city of
tall dark tower stacks, landing platforms cantilevered out of the towers at several heights, long lit causeways
linking the towers at different levels, a capital ship moored alongside a tower).

A hub whose profile shape is "spire" (tools/models/hub_colonies.py picks it from the hub id) is the usual planar city
(hub_plane._generate_map: blocks, streets, lanes, dock, services, landmarks) with a wider downtown, and then
add_spires(map, hub_id) turns it into a spire cluster:
  - the centre block (the canopy's) gets the main spire hubsp_spire_main (1500 m) instead of the canopy disc, every
    downtown block that holds no landmark one of the spire stacks hubsp_spire_a / _b / _c (1250 / 1080 / 940 m), turned
    by the hub's rng; under the plane each spire block hangs a shorter stack upside down (the city goes both ways);
  - causeways hubsp_causeway_l (a LOCAL street gap: towers 900 m apart) / hubsp_causeway_a (an AVENUE gap: 1100 m)
    link neighbouring spire blocks at one of the three causeway levels CAUSEWAY_LEVELS - every spire wears a collar at
    each level, so a causeway always lands on one; the levels sit above every street lane (hub_lanes LANE_LEVELS: the
    highest 430 m) and the plaza ring (265 m): the causeways never cross ship traffic;
  - one rim block with an open side, far from the hex dock, becomes the capital-ship berth hubsp_capital_berth: a berth
    tower at the block's outer edge, two gantry arms out over open space and a 520 m capital ship moored along them,
    parallel to the rim and within the block's own span (the street lanes' overrun ends stay clear); a red zone
    covers the ship (hub_plane.layout_from_map).
The platforms are part of the spire modules (3-4 per spire, each 70-90 m wide, reaching at most 250 m from the axis:
inside the block, clear of the street lanes and the plaza ring). Pure in the hub id: its own rng (the map's draws stay).

Every module is a hub module (ROOTS "hub"), base on y 0, +Y up; dark facades wear eq_hub_facade_sp_* (the shader
draws lit windows), the causeways' light strips are the emissive eq_hub_window_cool.
"""
import hashlib
import math
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HULL, STEEL, WARM, beacon  # noqa: F401
from units.hub_belt import MARK
from units.hub_style_west import strut, truss

register_materials({
    "eq_hub_facade_sp_dark": ((0.075, 0.085, 0.10), (0.30, 0.32, 0.36), 60),     # the spire stacks: near black
    "eq_hub_facade_sp_slate": ((0.13, 0.15, 0.18), (0.30, 0.32, 0.36), 60),
    "eq_hubsp_deck": ((0.16, 0.17, 0.19), (0.20, 0.20, 0.22), 30),
    "eq_hubsp_ship": ((0.12, 0.13, 0.15), (0.40, 0.42, 0.46), 90),
})
F_DARK, F_SLATE, DECK, SHIP = "eq_hub_facade_sp_dark", "eq_hub_facade_sp_slate", "eq_hubsp_deck", "eq_hubsp_ship"

CELL, STREET_LOCAL, STREET_AVENUE = 600.0, 300.0, 500.0
CAUSEWAY_LEVELS = (560.0, 720.0, 880.0)
COLLAR_R = 78.0
SPAN = {"hubsp_causeway_l": CELL + STREET_LOCAL, "hubsp_causeway_a": CELL + STREET_AVENUE}
CAUSEWAY_END = 52.0          # a causeway ends this far from each tower's axis (inside the collar)
# the berth: tower / ship placement in the berth module's frame (+Z = out of the block's open side)
BERTH_TOWER_Z, SHIP_Z, SHIP_Y, SHIP_L, SHIP_B, SHIP_H = 205.0, 440.0, 320.0, 520.0, 92.0, 74.0

# the spire stacks: segments (y0, y1, half x, half z), platforms (y, bearing deg, reach from the axis, width), crown top
SPIRES = {
    "hubsp_spire_main": {"segs": [(40, 300, 112, 104), (300, 600, 96, 90), (600, 900, 84, 78), (900, 1180, 70, 66),
                                  (1180, 1380, 52, 50)], "plats": [(250, 30, 245, 90), (470, 200, 240, 84),
                                                                   (650, 110, 235, 80), (1010, 290, 220, 70)],
                         "top": 1500.0, "title": "Главный шпиль 1500 м с консольными площадками"},
    "hubsp_spire_a": {"segs": [(40, 280, 96, 84), (280, 540, 82, 74), (540, 800, 70, 64), (800, 1060, 58, 54),
                               (1060, 1160, 44, 40)], "plats": [(210, 0, 230, 84), (430, 135, 225, 80), (980, 250, 205, 70)],
                      "top": 1250.0, "title": "Шпиль-стопка 1250 м"},
    "hubsp_spire_b": {"segs": [(40, 360, 100, 70), (360, 640, 86, 62), (640, 900, 72, 58), (900, 1000, 52, 46)],
                      "plats": [(300, 90, 230, 80), (480, 270, 225, 76), (640, 180, 215, 74), (960, 45, 190, 64)],
                      "top": 1080.0, "twin": True, "title": "Сдвоенный шпиль 1080 м"},
    "hubsp_spire_c": {"segs": [(40, 320, 88, 88), (320, 600, 76, 76), (600, 860, 64, 64), (860, 900, 50, 50)],
                      "plats": [(180, 315, 220, 78), (400, 60, 215, 74), (800, 200, 200, 70)],
                      "top": 940.0, "title": "Шпиль-стопка 940 м"},
}
UNDER_SPIRE = "hubsp_spire_c"   # the stack hung under the plane
MIN_TOWERS = 4                   # a cluster: at least this many spires (the main one included)
# blocks that never become a spire: the dock, the service / power blocks, the spinning drums / hotels, the squares,
# the port waterfront (its piers and the berth)
TOWER_NEVER = ("dock", "services", "power", "hab_rot", "hotel", "square", "canopy", "port", "void")


def _seg(lod, n):
    return max(4, n if lod == 0 else n * 2 // 3 if lod == 1 else n // 2)


def _ring(p, mat, y, r_in, r_out, h, seg):
    ylathe(p, mat, [(r_in, y), (r_out, y), (r_out, y + h), (r_in, y + h), (r_in, y)], seg)


def platform(p, lod, y, bearing, reach, width, face):
    """a landing platform cantilevered out of the tower face at height y along a bearing (deg, 0 = +Z): an 8 m deck from
    the face to `reach`, two diagonal braces from the face 70 m lower to the deck's outer third, a control hut at the
    root, lights on the outer corners, a pad mark"""
    a = math.radians(bearing)
    d = (math.sin(a), 0.0, math.cos(a))
    s = (d[2], 0.0, -d[0])
    L = reach - face + 10.0
    mid = (face - 10.0 + L / 2)
    R = axis_to(d)
    box(p, DECK, (d[0] * mid, y - 4.0, d[2] * mid), (width, 8.0, L), R)
    box(p, HULL, (d[0] * mid, y - 11.0, d[2] * mid), (width * 0.55, 6.0, L * 0.92), R)
    for sg in (-1, 1):
        base = (d[0] * (face - 6.0) + s[0] * sg * width * 0.3, y - 78.0, d[2] * (face - 6.0) + s[2] * sg * width * 0.3)
        tip = (d[0] * (face + L * 0.62) + s[0] * sg * width * 0.3, y - 12.0, d[2] * (face + L * 0.62) + s[2] * sg * width * 0.3)
        if lod < 2:
            truss(p, base, tip, 2.4, lod, bay=12.0, diag=(0, 2))
        else:
            strut(p, STEEL, base, tip, 2.0)
    hut = face + 14.0
    box(p, F_SLATE, (d[0] * hut, y + 9.0, d[2] * hut), (30.0, 18.0, 20.0), R)
    tip = face + L - 10.0
    for sg in (-1, 1):
        q = (d[0] * tip + s[0] * sg * (width / 2 - 3.0), y + 1.5, d[2] * tip + s[2] * sg * (width / 2 - 3.0))
        beacon(p, q, 2.6)
        if lod < 2:
            q2 = (d[0] * (face + L * 0.45) + s[0] * sg * (width / 2 - 2.0), y + 0.8,
                  d[2] * (face + L * 0.45) + s[2] * sg * (width / 2 - 2.0))
            box(p, COOL, q2, (1.6, 1.2, 1.6))
    if lod < 2:
        c = face + L * 0.55
        box(p, MARK, (d[0] * c, y + 0.15, d[2] * c), (width * 0.55, 0.2, 3.0), R)
        box(p, MARK, (d[0] * c, y + 0.15, d[2] * c), (3.0, 0.2, width * 0.55), R)


def spire(name):
    spec = SPIRES[name]

    def build(p, lod):
        seg8 = 8
        segs = spec["segs"]
        twin = spec.get("twin", False)
        # the plinth on the block deck, corner buttresses
        box(p, HULL, (0, 20.0, 0), (300.0, 40.0, 280.0))
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(p, F_SLATE, (sx * 120.0, 70.0, sz * 110.0), (40.0, 60.0, 40.0))
        for k, (y0, y1, hx, hz) in enumerate(segs):
            if twin:
                for sx in (-1, 1):                                   # two slender stacks joined by bridges
                    box(p, F_DARK, (sx * hx * 0.55, (y0 + y1) / 2, 0), (hx * 0.9, y1 - y0, hz * 2))
                box(p, F_SLATE, (0, (y0 + y1) / 2, 0), (hx * 0.6, y1 - y0, hz * 1.2))
            else:
                box(p, F_DARK if k % 2 == 0 else F_SLATE, (0, (y0 + y1) / 2, 0), (hx * 2, y1 - y0, hz * 2))
            box(p, HULL, (0, y1 + 3.0, 0), (hx * 2 + 16.0, 6.0, hz * 2 + 16.0))                # the setback ledge
            if lod < 2:
                for sx in (-1, 1):                                   # vertical light strips on the corners
                    for sz in (-1, 1):
                        box(p, COOL, (sx * (hx + 0.6), (y0 + y1) / 2, sz * (hz + 0.6)), (1.4, (y1 - y0) * 0.86, 1.4))
                if lod == 0:                                         # side fins
                    for sz in (-1, 1):
                        box(p, STEEL, (0, (y0 + y1) / 2, sz * (hz + 5.0)), (6.0, (y1 - y0) * 0.9, 10.0))
        # the causeway collars (every level: a causeway can land on any of them)
        for y in CAUSEWAY_LEVELS:
            if y + 30.0 < spec["top"]:
                _ring(p, STEEL, y - 18.0, 30.0, COLLAR_R, 36.0, _seg(lod, 16))
                if lod < 2:
                    _ring(p, COOL, y - 1.0, COLLAR_R - 1.0, COLLAR_R + 1.0, 2.0, _seg(lod, 16))
        # the cantilevered landing platforms
        for y, bearing, reach, width in spec["plats"]:
            hx, hz = next(((sx, sz) for y0, y1, sx, sz in segs if y0 <= y <= y1), segs[-1][2:])
            a = math.radians(bearing)
            sa, ca = abs(math.sin(a)), abs(math.cos(a))
            face = min(hx / sa if sa > 1e-6 else 1e9, hz / ca if ca > 1e-6 else 1e9)   # the face along the bearing
            platform(p, lod, y, bearing, reach, width, face - 4.0)
        # the crown: a tapered cap, a lattice mast, a beacon
        y_top = segs[-1][1]
        hx, hz = segs[-1][2], segs[-1][3]
        frustum_h = (spec["top"] - y_top) * 0.45
        ylathe(p, F_SLATE, [(0, y_top), (min(hx, hz) * 0.9, y_top), (min(hx, hz) * 0.3, y_top + frustum_h),
                            (0, y_top + frustum_h)], seg8)
        truss(p, (0, y_top + frustum_h - 4.0, 0), (0, spec["top"], 0), 4.0, lod, bay=16.0)
        beacon(p, (0, spec["top"] + 3.0, 0), 5.0)
        if lod < 2:
            for k in range(4):
                a = 2 * math.pi * k / 4 + math.pi / 4
                truss(p, (hx * 0.6 * math.cos(a), y_top, hz * 0.6 * math.sin(a)),
                      (hx * 0.6 * math.cos(a), y_top + frustum_h * 1.3, hz * 0.6 * math.sin(a)), 1.4, lod, bay=8.0)
                beacon(p, (hx * 0.6 * math.cos(a), y_top + frustum_h * 1.3 + 2.0, hz * 0.6 * math.sin(a)), 2.0)
    return build


def causeway(name):
    span = SPAN[name]

    def build(p, lod):
        """a lit causeway along local +Z between two tower axes `span` apart (origin = the midpoint, y 0 = the level):
        a 40 m deck, a glazed gallery on it, light strips on both edges, a lattice keel truss, lights under it"""
        L = span - 2 * CAUSEWAY_END
        box(p, HULL, (0, -5.0, 0), (40.0, 10.0, L))
        box(p, DARK, (0, 9.0, 0), (24.0, 18.0, L - 20.0))                     # the glazed gallery
        box(p, DECK, (0, 19.0, 0), (28.0, 2.0, L - 16.0))
        for sx in (-1, 1):
            box(p, COOL, (sx * 19.6, 0.6, 0), (1.2, 1.2, L))                   # the edge light strips
            box(p, COOL, (sx * 12.4, 17.0, 0), (0.8, 0.8, L - 24.0))
        truss(p, (0, -22.0, -L / 2 + 20.0), (0, -22.0, L / 2 - 20.0), 7.0, lod, bay=20.0, diag=(1, 3))
        for k in range(3):                                                     # stays: hanger struts deck -> keel
            z = (k - 1) * L / 3
            strut(p, STEEL, (-18.0, -10.0, z), (0, -29.0, z), 1.2)
            strut(p, STEEL, (18.0, -10.0, z), (0, -29.0, z), 1.2)
        if lod < 2:
            n = int(L // 60)
            for k in range(n + 1):
                z = -L / 2 + 10.0 + k * (L - 20.0) / max(1, n)
                box(p, FLOOD, (0, -31.0, z), (3.0, 1.0, 3.0))
                if lod == 0:
                    for sx in (-1, 1):
                        box(p, WARM, (sx * 12.1, 9.0, z), (0.4, 8.0, 14.0))
        for sz in (-1, 1):                                                     # the end portals (inside the collars)
            box(p, STEEL, (0, 6.0, sz * (L / 2 - 6.0)), (48.0, 34.0, 12.0))
    return build


def capital_berth(p, lod):
    """a capital-ship berth on a rim block (local +Z = out of the block's open side): a 640 m berth tower at the outer
    edge, two gantry arms out over open space and a 520 m capital ship moored along them, parallel to the rim"""
    tz = BERTH_TOWER_Z
    box(p, HULL, (0, 20.0, tz - 60.0), (260.0, 40.0, 200.0))
    for y0, y1, hx, hz in ((40, 280, 70, 50), (280, 520, 58, 44), (520, 600, 44, 36)):
        box(p, F_DARK, (0, (y0 + y1) / 2, tz), (hx * 2, y1 - y0, hz * 2))
        box(p, HULL, (0, y1 + 3.0, tz), (hx * 2 + 14.0, 6.0, hz * 2 + 14.0))
        if lod < 2:
            for sx in (-1, 1):
                box(p, COOL, (sx * (hx + 0.6), (y0 + y1) / 2, tz + hz + 0.6), (1.4, (y1 - y0) * 0.85, 1.4))
    truss(p, (0, 606.0, tz), (0, 680.0, tz), 3.0, lod, bay=12.0)
    beacon(p, (0, 683.0, tz), 4.0)
    face = tz + 50.0
    hull_in = SHIP_Z - SHIP_B / 2
    for y in (SHIP_Y - 20.0, SHIP_Y + 28.0):                                   # the gantry arms over open space
        for x in (-150.0, 150.0) if y < SHIP_Y else (0.0,):
            box(p, STEEL, (x, y, (face + hull_in) / 2), (22.0, 12.0, hull_in - face + 6.0))
            if lod < 2:
                box(p, COOL, (x, y + 6.6, (face + hull_in) / 2), (1.2, 1.2, hull_in - face))
                truss(p, (x, y - 60.0, face), (x, y - 8.0, (face + hull_in) / 2 + 20.0), 2.2, lod, bay=10.0, diag=(0, 2))
    # the capital ship: its axis along X, the bow to +X
    zc, yc, L, B, H = SHIP_Z, SHIP_Y, SHIP_L, SHIP_B, SHIP_H
    box(p, SHIP, (-20.0, yc, zc), (L - 120.0, H * 0.55, B), chamfer=6.0)               # the main hull
    box(p, SHIP, (-60.0, yc + H * 0.35, zc), (L - 220.0, H * 0.25, B * 0.75))          # the upper deck
    box(p, F_DARK, (-120.0, yc + H * 0.62, zc), (150.0, H * 0.3, B * 0.45))            # the superstructure
    box(p, SHIP, (-150.0, yc - H * 0.33, zc), (L - 260.0, H * 0.18, B * 0.6))          # the keel
    lathe(p, SHIP, [(0.0, 0.0), (B * 0.5, 0.0), (B * 0.18, 110.0), (0.0, 116.0)], 8, axis_to((1.0, 0.0, 0.0)),
          ((L - 120.0) / 2 - 20.0, yc, zc))                                             # the bow taper
    for k, (dy, dz) in enumerate(((0.0, 0.0), (18.0, 24.0), (18.0, -24.0), (-16.0, 20.0), (-16.0, -20.0))):
        x0 = -(L - 120.0) / 2 - 20.0
        lathe(p, STEEL, [(0.0, 0.0), (14.0, 0.0), (16.0, 26.0), (0.0, 26.0)], 8, axis_to((-1.0, 0.0, 0.0)), (x0, yc + dy, zc + dz))
        if lod < 3:
            box(p, COOL, (x0 - 26.5, yc + dy, zc + dz), (1.0, 22.0, 22.0))             # the engine glow
    if lod < 2:
        for sz in (-1, 1):                                                            # window rows along both sides
            for row, (dy, mat) in enumerate(((H * 0.12, WARM), (-H * 0.05, COOL), (H * 0.42, WARM))):
                box(p, mat, (-30.0, yc + dy, zc + sz * (B / 2 + 0.4 - (row == 2) * B * 0.125)), (L - 200.0, 1.6, 0.8))
        for x in (-200.0, -60.0, 80.0, 200.0):
            beacon(p, (x, yc + H * 0.5 + 2.0, zc + B * 0.38), 2.2)
    beacon(p, ((L - 120.0) / 2 + 96.0, yc, zc), 3.0)


# ================================================================ registration
def _one(name, fn):
    def build(lod):
        P = {name: Part(name)}
        fn(P[name], lod)
        return P
    return build


KIT = {**{k: (spire(k), v["title"]) for k, v in SPIRES.items()},
       "hubsp_causeway_l": (causeway("hubsp_causeway_l"), "Освещённая эстакада между шпилями (улица, 900 м)"),
       "hubsp_causeway_a": (causeway("hubsp_causeway_a"), "Освещённая эстакада между шпилями (проспект, 1100 м)"),
       "hubsp_capital_berth": (capital_berth, "Причал капитального корабля у башни")}


def _spire_hits(name):
    spec = SPIRES[name]

    def hits():
        H = [hbox(name, (0, 20.0, 0), (300.0, 40.0, 280.0))]
        for y0, y1, hx, hz in spec["segs"]:
            H.append(hbox(name, (0, (y0 + y1) / 2, 0), (hx * 2 + (40.0 if spec.get("twin") else 0.0), y1 - y0, hz * 2)))
        for y in CAUSEWAY_LEVELS:
            if y + 30.0 < spec["top"]:
                H.append(hbox(name, (0, y, 0), (2 * COLLAR_R, 36.0, 2 * COLLAR_R)))
        for y, bearing, reach, width in spec["plats"]:
            a = math.radians(bearing)
            mid = reach / 2 + 30.0
            H.append(hbox(name, (math.sin(a) * mid, y - 6.0, math.cos(a) * mid), (width, 14.0, reach - 50.0),
                          (0.0, bearing, 0.0)))
        y_top = spec["segs"][-1][1]
        H.append(hbox(name, (0, (y_top + spec["top"]) / 2, 0), (40.0, spec["top"] - y_top, 40.0)))
        return H
    return hits


def _causeway_hits(name):
    L = SPAN[name] - 2 * CAUSEWAY_END
    return lambda: [hbox(name, (0, -2.0, 0), (48.0, 44.0, L)), hbox(name, (0, -22.0, 0), (16.0, 16.0, L - 40.0))]


def _berth_hits():
    n = "hubsp_capital_berth"
    face = BERTH_TOWER_Z + 50.0
    hull_in = SHIP_Z - SHIP_B / 2
    return [hbox(n, (0, 20.0, BERTH_TOWER_Z - 60.0), (260.0, 40.0, 200.0)),
            hbox(n, (0, 320.0, BERTH_TOWER_Z), (140.0, 560.0, 100.0)),
            hbox(n, (0, SHIP_Y, (face + hull_in) / 2), (330.0, 70.0, hull_in - face)),
            hbox(n, (0, SHIP_Y + 6.0, SHIP_Z), (SHIP_L, SHIP_H * 1.1, SHIP_B))]


ASSETS = {k: _one(k, v[0]) for k, v in KIT.items()}
HITS = {**{k: _spire_hits(k) for k in SPIRES}, "hubsp_causeway_l": _causeway_hits("hubsp_causeway_l"),
        "hubsp_causeway_a": _causeway_hits("hubsp_causeway_a"), "hubsp_capital_berth": _berth_hits}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[1], "hub_module", "Шпилевой кластер хаба (tools/models/units/hub_spire.py).") for k, v in KIT.items()}


# ================================================================ the map pass
def berth_red_zone(pos, R):
    """the red zone of a placed berth (city frame): the ship and its arms (no through traffic)"""
    face = BERTH_TOWER_Z + 50.0
    loc = (0.0, SHIP_Y, (face + SHIP_Z + SHIP_B / 2) / 2)
    c = tuple(pos[i] + sum(R[i][k] * loc[k] for k in range(3)) for i in range(3))
    return c, (SHIP_L / 2 + 40.0, SHIP_H / 2 + 50.0, (SHIP_Z + SHIP_B / 2 - face) / 2 + 40.0)


def add_spires(mp, hub_id):
    """turn a generated planar map into a spire cluster (see the module docstring); idempotent"""
    if mp.get("spires"):
        return mp
    rng = random.Random(int(hashlib.sha1(("spire:" + hub_id).encode("utf-8")).hexdigest()[:12], 16))
    cells = {(c["i"], c["j"]): c for c in mp["cells"]}
    av_x, av_z = set(mp["avenues"]["x"]), set(mp["avenues"]["z"])
    spires = []
    canopy = next((k for k, c in cells.items() if c["zone"] == "canopy"), None)
    near = lambda k: canopy is not None and max(abs(k[0] - canopy[0]), abs(k[1] - canopy[1])) <= 1
    for k in sorted(cells):
        c = cells[k]
        # the ring round the centre is all towers (a small hub is a cluster too): its residential / industrial / storage
        # blocks become downtown (the spinning drums / hotels, the port, the dock and the service blocks stay)
        if near(k) and c["zone"] in ("residential", "industry", "storage", "logistics", "repair", "rig", "refuel") \
                and not c.get("landmark"):
            c["zone"] = "downtown"
            c.pop("piers", None)
        if c["zone"] == "canopy":
            c["top"] = [{"module": "hubsp_spire_main", "x": 0.0, "z": 0.0, "yaw": rng.choice((0, 90, 180, 270))}]
            c["under"] = [{"module": UNDER_SPIRE, "x": 0.0, "z": 0.0, "yaw": rng.choice((0, 90, 180, 270))}]
            spires.append(k)
        elif c["zone"] == "downtown" and not c.get("landmark"):
            c["top"] = [{"module": rng.choice(("hubsp_spire_a", "hubsp_spire_a", "hubsp_spire_b", "hubsp_spire_c")),
                         "x": 0.0, "z": 0.0, "yaw": rng.choice((0, 90, 180, 270))}]
            c["under"] = [{"module": UNDER_SPIRE, "x": 0.0, "z": 0.0, "yaw": rng.choice((0, 90, 180, 270))}]
            c["landmark"] = c["top"][0]["module"]
            spires.append(k)
    # a cluster has at least MIN_TOWERS spires: on a small hub the ring round the centre is mostly the dock, the
    # service / power blocks and the landmarks - take the free blocks nearest the centre (next to a spire first), then
    # landmark blocks (the spires are the hub's signature; the displaced landmark is dropped from the map's list)
    def take(k):
        c = cells[k]
        if c.get("landmark") and mp.get("landmarks"):
            mp["landmarks"]["modules"] = [m for m in mp["landmarks"]["modules"] if m != c["landmark"]]
        c["zone"] = "downtown"
        c.pop("piers", None)
        c["top"] = [{"module": rng.choice(("hubsp_spire_a", "hubsp_spire_c")), "x": 0.0, "z": 0.0,
                     "yaw": rng.choice((0, 90, 180, 270))}]
        c["under"] = [{"module": UNDER_SPIRE, "x": 0.0, "z": 0.0, "yaw": rng.choice((0, 90, 180, 270))}]
        c["landmark"] = c["top"][0]["module"]
        spires.append(k)
    if canopy is not None and len(spires) < MIN_TOWERS:
        adj = lambda k: any(abs(k[0] - s[0]) + abs(k[1] - s[1]) == 1 for s in spires)
        for allow_landmark in (False, True):
            pool = [k for k, c in cells.items() if k not in spires and c["zone"] not in TOWER_NEVER
                    and (allow_landmark or not c.get("landmark")) and not (c.get("landmark") or "").startswith("hubsp")]
            for k in sorted(pool, key=lambda k: (not adj(k), max(abs(k[0] - canopy[0]), abs(k[1] - canopy[1])), k)):
                if len(spires) >= MIN_TOWERS:
                    break
                take(k)
    ss = set(spires)
    causeways = []
    for k in spires:
        for o in ((1, 0), (0, 1)):
            n = (k[0] + o[0], k[1] + o[1])
            if n not in ss or (rng.random() > 0.8 and canopy not in (k, n)):   # the main spire: always linked
                continue
            av = (k[0] in av_x) if o[0] else (k[1] in av_z)
            mod = "hubsp_causeway_a" if av else "hubsp_causeway_l"
            half = SPAN[mod] / 2
            level = rng.choice(CAUSEWAY_LEVELS)
            cells[k]["top"].append({"module": mod, "x": round(o[0] * half, 3), "z": round(o[1] * half, 3), "y": level,
                                    "yaw": 90 if o[0] else 0})
            causeways.append([list(k), list(n), level])
    # the capital-ship berth: a rim block with an open side, not a busy / landmark block, as far from the dock as can be
    from units.hub_plane import _open_sides
    keys = set(cells)
    dock = next((k for k, c in cells.items() if c["zone"] == "dock"), None)
    pool = [k for k, c in cells.items() if c["zone"] in ("port", "logistics", "storage", "industry", "repair", "rig", "refuel",
                                                         "residential") and not c.get("landmark") and _open_sides(k, keys)]
    berth = None
    if pool:
        far = lambda k: -(abs(k[0] - dock[0]) + abs(k[1] - dock[1])) if dock else 0
        berth = sorted(pool, key=lambda k: (far(k), k))[0]
        o = _open_sides(berth, keys)[0]
        yaw = {(0, 1): 0, (1, 0): 90, (0, -1): 180, (-1, 0): 270}[tuple(o)]
        c = cells[berth]
        c["top"] = [{"module": "hubsp_capital_berth", "x": 0.0, "z": 0.0, "yaw": yaw}]
        c.pop("piers", None)
        c["landmark"] = "hubsp_capital_berth"
    mp["spires"] = {"towers": [list(k) for k in spires], "causeways": causeways, "berth": list(berth) if berth else None}
    return mp
