"""Planar floating city (author 2026-11-17: «элементы - шикарные, нужно ощущение города. ... нужно, что бы как в порту
-были ряды контейнеров, вокзал для грузовиков. просто это все нужно собрать. давай для начала сделаем все в одной
плоскости. пусть будет парящая мегаструктура но в одной плоскости. сгенерируй что-то вроде того города как ты делал, но
без центральной плоскости»).

plane_layout(hub_id): a city in ONE plane (street level y = 0), no base plate under it. A grid of blocks 600 x 600 m
(each block its own girder lattice under the modules' aprons, the void visible between them), streets 226 m wide gaps
with a road bridge down the middle on a girder (crossings at the nodes, short access bridges into every block, avenues
with tunnels and a monorail). Zoning makes it read as a city: a downtown of towers round the disc canopy in the centre,
a residential ring (+ habitat drums and hotel barrels, spinning, lying in the plane), an industrial / storage ring, and
the port on the rim - container terminals, truck stations, warehouses, platform piers sticking out of the rim in the
plane. A pure function of the hub id: the same hub is always the same city.

New kit here: hubp_container_yard (rows of container stacks, gantry cranes), hubp_truck_station (a truck terminal:
drive-through bays under a roof, platforms, trucks), hubp_crossing (a street crossing plate).
"""
import hashlib
import math
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_mega import FACADE, FACADE_L  # noqa: F401
from units.hub_belt import (CONT, DRUM_L, GIRDER_H, GRATE, HX_L, MARK, ROAD, SEG_L, SPIN_CAP, WORKS_VENTS, _add, _frame,
                            _mul, _sub, lamp, road_strip)


# ---------------------------------------------------------------- container terminal: blocks of stacks, RTG cranes
CY_W, CY_D = 250.0, 190.0
C_L, C_W, C_H = 12.2, 2.44, 2.59                  # a 40 ft container


def container_yard_parts(lod):
    """7 container blocks along Z (7 stacks across x 12 long, 0..5 high) with a truck lane beside each, rubber-tyred
    gantry cranes spanning block + lane, high-mast floodlights, a gate house; deck top y 0"""
    name = "hubp_container_yard"
    P = {name: Part(name)}
    y = P[name]
    rng = random.Random(5)
    box(y, HULL, (0, -3.0, 0), (CY_W, 6.0, CY_D))
    road_strip(y, -CY_W / 2 + 2.0, -CY_W / 2 + 18.0, 0.0, -CY_D / 2, CY_D / 2, lod)                # the yard road
    road_strip(y, -CY_W / 2 + 18.0, CY_W / 2, 0.0, CY_D / 2 - 14.0, CY_D / 2 - 2.0, lod)
    x0 = -CY_W / 2 + 24.0
    zs = -CY_D / 2 + 14.0
    for b in range(7):
        bx = x0 + b * 32.0
        # the stack heights (the same for every LOD: drawn from the rng before any LOD branch)
        H = [[max(0, min(5, int(rng.gauss(3.2, 1.3)))) for _ in range(12)] for _ in range(7)]
        col = [[[rng.randrange(3) for _ in range(5)] for _ in range(12)] for _ in range(7)]
        for i in range(7):
            cx = bx + i * 2.6 + 1.3
            if lod >= 2:                                                  # one bar per stack row (x)
                h = sum(H[i]) / 12.0
                box(y, CONT[col[i][0][0]], (cx, h * C_H / 2 + 0.05, zs + 6.4 * 12), (C_W, max(h, 0.5) * C_H, 12.8 * 12 - 0.6))
                continue
            for j in range(12):
                cz = zs + j * 12.8 + 6.4
                if lod == 1:
                    if H[i][j]:
                        box(y, CONT[col[i][j][H[i][j] - 1]], (cx, H[i][j] * C_H / 2, cz), (C_W, H[i][j] * C_H, C_L))
                    continue
                for k in range(H[i][j]):
                    box(y, CONT[col[i][j][k]], (cx, C_H / 2 + k * (C_H + 0.04), cz), (C_W, C_H, C_L))
        if lod < 3:                                                       # 2 RTG cranes over block + lane
            for zc in (zs + 12.8 * rng.uniform(1.5, 5.0), zs + 12.8 * rng.uniform(7.0, 10.5)):
                for sx in (bx - 2.5, bx + 29.0):
                    for dz in (-5.0, 5.0):
                        box(y, STEEL, (sx, 11.0, zc + dz), (1.2, 22.0, 1.2))
                    if lod < 2:
                        box(y, HAZ, (sx, 1.0, zc), (2.0, 2.0, 12.0))      # the wheel bogie
                for dz in (-5.0, 5.0):
                    box(y, LIGHT, (bx + 13.25, 22.5, zc + dz), (33.0, 2.0, 1.6))
                box(y, LIGHT, (bx + 8.0 + 10.0 * rng.random(), 21.0, zc), (5.0, 3.0, 10.0))   # the trolley
                if lod < 2:
                    box(y, WARM, (bx + 27.5, 18.0, zc), (2.6, 2.0, 2.6))                     # the cab
    if lod < 2:                                                           # high-mast lights between the blocks
        for b in range(0, 8, 2):
            for z in (-CY_D / 2 + 6.0, CY_D / 2 - 20.0):
                x = x0 - 6.0 + b * 32.0
                box(y, STEEL, (x, 17.5, z), (0.8, 35.0, 0.8))
                box(y, FLOOD, (x, 35.5, z), (4.0, 1.0, 4.0))
        box(y, LIGHT, (-CY_W / 2 + 10.0, 5.0, -CY_D / 2 + 8.0), (14.0, 10.0, 10.0))           # the gate house
        box(y, WARM, (-CY_W / 2 + 17.1, 6.0, -CY_D / 2 + 8.0), (0.2, 2.5, 8.0))
    if lod < 3:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(y, (sx * (CY_W / 2 - 1.0), 1.0, sz * (CY_D / 2 - 1.0)), 1.2)
    return P


def container_yard_hits():
    return [hbox("hubp_container_yard", (0, -3.0, 0), (CY_W, 6.0, CY_D)), hbox("hubp_container_yard", (0, 8.0, 0), (CY_W - 20.0, 16.0, CY_D - 30.0))]


# ---------------------------------------------------------------- truck station: a terminal for road trucks
TS_W, TS_D, TS_ROOF = 220.0, 160.0, 24.0


def truck(p, x, z, s, rng, lod):
    """a road truck along Z (s = +-1 its heading): cab, a container trailer, chassis, headlights"""
    box(p, rng.choice((HAZ, LIGHT, HULL)), (x, 2.4, z + s * 8.6), (3.0, 3.8, 3.8))
    box(p, CONT[rng.randrange(3)], (x, 2.9, z - s * 1.4), (2.6, 3.0, 14.4))
    if lod == 0:
        box(p, DARK, (x, 0.7, z), (2.4, 1.0, 19.6))
        box(p, FLOOD, (x, 1.6, z + s * 10.55), (2.4, 0.5, 0.1))
        box(p, WARM, (x, 3.6, z + s * 10.55), (2.2, 1.2, 0.08))           # the cab window, lit


def truck_station_parts(lod):
    """10 drive-through bays along Z under a roof on columns, loading platforms between them, trucks at the bays,
    the concourse building on -X, signal gantries over both ends; deck top y 0"""
    name = "hubp_truck_station"
    P = {name: Part(name)}
    t = P[name]
    rng = random.Random(17)
    box(t, HULL, (0, -3.0, 0), (TS_W + 40.0, 6.0, TS_D + 60.0))
    x0 = -TS_W / 2 + 10.0
    for k in range(10):
        xl = x0 + k * 20.0 + 5.5
        xp = xl + 10.0
        road_strip(t, xl - 5.5, xl + 5.5, 0.0, -TS_D / 2 - 28.0, TS_D / 2 + 28.0, lod)
        box(t, LIGHT, (xp, 1.0, 0), (9.0, 2.0, TS_D - 10.0))                                     # loading platform
        if lod < 2:
            for sx in (-1, 1):
                box(t, HAZ, (xp + sx * 4.3, 2.05, 0), (0.4, 0.1, TS_D - 10.0))
        if lod < 3:
            for z in (-TS_D / 2 + 10.0, -TS_D / 6, TS_D / 6, TS_D / 2 - 10.0):                   # roof columns
                box(t, STEEL, (xp, TS_ROOF / 2, z), (1.2, TS_ROOF, 1.2))
        # trucks at the bay (the rng runs the same on every LOD)
        n = rng.choice((0, 1, 1, 2, 2, 3))
        heads = rng.choice((-1, 1))
        zs = [-TS_D / 2 + 20.0 + 42.0 * i + rng.uniform(-4.0, 4.0) for i in range(n)]
        seeds = [rng.random() for _ in range(n)]
        if lod < 3:
            for z, sd in zip(zs, seeds):
                truck(t, xl, z, heads, random.Random(sd), lod)
    # the roof: a slab per bay pair with a lit strip under it, ribs on top
    box(t, LIGHT, (x0 + 100.0, TS_ROOF + 1.0, 0), (TS_W - 10.0, 2.0, TS_D))
    if lod < 3:
        for k in range(10):
            box(t, WARM if k % 3 else COOL, (x0 + k * 20.0 + 5.5, TS_ROOF - 0.2, 0), (1.6, 0.3, TS_D - 16.0))
        for z in range(-int(TS_D / 2) + 10, int(TS_D / 2), 20):
            box(t, STEEL, (x0 + 100.0, TS_ROOF + 3.0, float(z)), (TS_W - 10.0, 2.0, 1.6))
    # the concourse on -X: a facade block with the windows, a sign tower
    box(t, FACADE, (-TS_W / 2 - 12.0, 22.0, 0), (24.0, 44.0, TS_D))
    box(t, LIGHT, (-TS_W / 2 - 12.0, 45.0, 0), (26.0, 2.0, TS_D + 2.0))
    box(t, STEEL, (-TS_W / 2 - 12.0, 70.0, TS_D / 2 - 10.0), (6.0, 50.0, 6.0))
    box(t, WARM, (-TS_W / 2 - 12.0, 88.0, TS_D / 2 - 10.0), (2.0, 10.0, 34.0))                  # the sign
    if lod < 3:
        beacon(t, (-TS_W / 2 - 12.0, 96.0, TS_D / 2 - 10.0), 1.6)
        for sz in (-1, 1):                                                                        # signal gantries
            z = sz * (TS_D / 2 + 14.0)
            box(t, STEEL, (x0 + 100.0, 12.0, z), (TS_W - 6.0, 1.6, 1.6))
            for sx in (-1, 1):
                box(t, STEEL, (x0 + 100.0 + sx * (TS_W / 2 - 3.0), 6.0, z), (1.2, 12.0, 1.2))
            if lod < 2:
                for k in range(10):
                    box(t, COOL if (k + sz) % 4 else HAZ, (x0 + k * 20.0 + 5.5, 11.0, z), (1.2, 1.0, 0.4))
    return P


def truck_station_hits():
    return [hbox("hubp_truck_station", (0, -3.0, 0), (TS_W + 40.0, 6.0, TS_D + 60.0)),
            hbox("hubp_truck_station", (0, TS_ROOF / 2, 0), (TS_W, TS_ROOF + 4.0, TS_D))]


# ---------------------------------------------------------------- street crossing: a 40 x 40 plate where streets meet
def crossing_parts(lod):
    name = "hubp_crossing"
    P = {name: Part(name)}
    c = P[name]
    box(c, HULL, (0, -1.5, 0), (40.0, 3.0, 40.0))
    box(c, ROAD, (0, 0.05, 0), (40.0, 0.1, 40.0))
    if lod < 2:
        for a in (0.0, math.pi / 2, math.pi, 1.5 * math.pi):                                     # stop lines
            box(c, MARK, (17.0 * math.cos(a), 0.12, 17.0 * math.sin(a)), (1.2, 0.05, 24.0), rot_y(-a))
        for sx in (-1, 1):
            for sz in (-1, 1):
                lamp(c, sx * 19.0, 0.0, sz * 19.0, 12.0)
    if lod < 3:
        beacon(c, (0, 0.6, 0), 1.0)
    return P


def crossing_hits():
    return [hbox("hubp_crossing", (0, -1.5, 0), (40.0, 3.0, 40.0))]


KIT = {"hubp_container_yard": (container_yard_parts, container_yard_hits,
                               "Контейнерный терминал 250x190: 7 штабелей до 5 ярусов, козловые краны, мачты света"),
       "hubp_truck_station": (truck_station_parts, truck_station_hits,
                              "Вокзал грузовиков: 10 сквозных путей под крышей, платформы, грузовики, вокзал"),
       "hubp_crossing": (crossing_parts, crossing_hits, "Перекрёсток 40x40")}
ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Модуль плоского города (tools/models/units/hub_plane.py).") for k, v in KIT.items()}


# ================================================================ the planar city generator
CELL = 600.0                           # a block
STREET_LOCAL, STREET_AVENUE = 300.0, 500.0   # the gaps between blocks by traffic class (see plane_layout)
FRAME_Y = -GIRDER_H - 12.0             # the girder axes under the blocks and streets: the girder tops at y -12
ROT_Z = (0.0, 0.0, 0.0)                # along +Z
ROT_X = None                           # along +X (set below: _frame needs pads)

# what fills a block, by zone: (module, weight)
ZONE_MODULES = {
    "downtown": [("hubb_hab_tower", 3), ("hubv_gallery_tower", 1), ("hubv_crown_spire", 1), ("hubv_crown_spire_b", 1),
                 ("hubv_crown_spire_c", 1), ("hubv_saucer_tower", 1), ("hubk_hab_b", 3), ("hubb_hab_slab", 1)],
    "residential": [("hubb_hab_slab", 3), ("hubb_hab_tower", 2), ("hubk_hab_a", 3), ("hubk_hab_c", 3), ("hubk_hab_b", 2),
                    ("hubv_saucer_tower", 1), ("hubv_ring_wheel", 1), ("hubv_water_tower", 1)],
    "hotel": [("hubb_hab_slab", 2), ("hubv_ring_wheel", 1), ("hubv_pad_star", 1), ("hubk_hab_a", 2)],
    "storage": [("hubb_warehouse", 3), ("hubp_container_yard", 3), ("hubp_truck_station", 1), ("hubb_gas_battery", 1)],
    "industry": [("hubb_works", 3), ("hubb_gas_battery", 2), ("hubb_fuel_spheres", 2), ("hubv_refinery", 1), ("hubb_warehouse", 1),
                 ("hubv_mast", 1)],
    "port": [("hubp_container_yard", 4), ("hubb_warehouse", 2), ("hubp_truck_station", 2)],
    "logistics": [("hubp_truck_station", 2), ("hubp_container_yard", 2), ("hubb_warehouse", 2), ("hubb_parking", 1)],
    "refuel": [("hubb_fuel_spheres", 2), ("hubb_gas_battery", 2), ("hubp_truck_station", 1)],
    "repair": [("hubb_repair_dock", 2), ("hubb_parking", 2), ("hubb_warehouse", 1)],
    "rig": [("hubv_rig", 1), ("hubv_pad_star", 2), ("hubv_cage_gantry", 1), ("hubv_mast", 1)],
}
# the city continues UNDER the plane (author 2026-11-17: «надо сделать продолжение города - с другой стороны от
# уровня "земли"»): every block also hangs modules upside down below its girder lattice - towers hanging into the dark
# under the centre, tanks and works under the industry, stores under the port; a mirrored disc under the canopy
UNDER_Y = FRAME_Y - GIRDER_H - 2.0     # the underside "ground": just below the girder bottoms
UNDER_MODULES = {
    "towers": [("hubb_hab_tower", 3), ("hubk_hab_b", 3), ("hubv_saucer_tower", 1), ("hubv_gallery_tower", 1), ("hubv_crown_spire_c", 1),
               ("hubb_hab_slab", 1)],
    "res": [("hubb_hab_slab", 3), ("hubk_hab_a", 3), ("hubk_hab_c", 3), ("hubb_hab_tower", 1), ("hubv_ring_wheel", 1)],
    "ind": [("hubb_works", 2), ("hubb_gas_battery", 3), ("hubb_fuel_spheres", 2), ("hubv_refinery", 1)],
    "store": [("hubb_warehouse", 3), ("hubp_container_yard", 3), ("hubp_truck_station", 1), ("hubb_fuel_spheres", 1)],
}
UNDER_OF = {"downtown": "towers", "residential": "res", "industry": "ind", "refuel": "ind", "rig": "ind", "storage": "store",
            "port": "store", "logistics": "store", "repair": "store"}
# the piers a rim block of the zone puts out of its open sides: (module, its width across, its length)
ZONE_PIERS = {"port": [("hubb_pier_l", 200.0), ("hubb_pier_m", 150.0)], "refuel": [("hubb_fuel_dock", 150.0)],
              "logistics": [("hubb_pier_m", 150.0)]}
ZONE_TITLE = {"downtown": "Центр", "residential": "Жилой район", "hotel": "Гостиничные доки", "hab_rot": "Жилые барабаны",
              "storage": "Склады", "industry": "Промзона", "port": "Порт", "logistics": "Логистика", "refuel": "Заправка",
              "repair": "Ремонтные доки", "rig": "Платформы", "canopy": "Диск", "dock": "Док"}
WHEELED = {"storage", "industry", "port", "logistics", "refuel", "repair", "rig"}
PASSENGER = {"downtown", "residential", "hotel", "hab_rot", "canopy"}


def _yaw(d, down=False):
    return tuple(_frame(d, (0.0, -1.0 if down else 1.0, 0.0)))


_OBB = {}


def _obb():
    if not _OBB:
        from units.hub_belt import _module_obb
        _OBB.update(_module_obb())
    return _OBB


def _footprint(folder, rot):
    import pads
    lo, hi, _ = _obb()[folder]
    R = pads.euler(rot)
    cs = [apply(R, (x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
    return (min(c[0] for c in cs), max(c[0] for c in cs), min(c[2] for c in cs), max(c[2] for c in cs), max(abs(c[1]) for c in cs))


def _pack(rng, region, choices, max_top=None, gap=26.0, down=False):
    """shelf-pack modules into region (x0, z0, x1, z1): -> [(folder, rot, pos)]; shelves spread evenly; down: the
    modules hang upside down (their +Y to world -Y)"""
    x0, z0, x1, z1 = region
    pool = [f for f, w in choices for _ in range(w)]
    yaws = [_yaw(d, down) for d in ((0.0, 0.0, 1.0), (1.0, 0.0, 0.0), (0.0, 0.0, -1.0), (-1.0, 0.0, 0.0))]
    shelves, cur, cx, cz, sh, fails = [], [], x0, z0, 0.0, 0
    while fails < 14:
        folder = rng.choice(pool)
        opts = []
        for rot in rng.sample(yaws, 4):
            fp = _footprint(folder, rot)
            w, d = fp[1] - fp[0], fp[3] - fp[2]
            if (max_top is None or fp[4] <= max_top) and w <= x1 - x0 and d <= z1 - z0:
                opts.append((rot, fp, w, d))
        if not opts:
            fails += 1
            continue
        fit = [o for o in opts if cx + o[2] <= x1 and cz + o[3] <= z1]
        if not fit:
            if cur:
                shelves.append((cz, sh, cur))
                cz += sh + gap
                cur, cx, sh = [], x0, 0.0
            fit = [o for o in opts if cz + o[3] <= z1]
            if not fit:
                fails += 1
                continue
        rot, fp, w, d = min(fit, key=lambda o: o[3]) if rng.random() < 0.6 else fit[0]
        cur.append((folder, rot, fp, w, d))
        cx += w + gap
        sh = max(sh, d)
    if cur:
        shelves.append((cz, sh, cur))
    out = []
    if not shelves:
        return out
    used = shelves[-1][0] + shelves[-1][1] - z0
    oz = (z1 - z0 - used) / 2
    for cz, sh, items in shelves:
        free = (x1 - x0) - sum(i[3] for i in items)
        s = free / (len(items) + 1)
        x = x0 + s
        for folder, rot, fp, w, d in items:
            z = cz + oz + (sh - d) / 2
            out.append((folder, rot, (x - fp[0], 0.0, z - fp[2])))
            x += w + s
    return out


def plane_layout(hub_id):
    """-> dict(nodes, spinning, haze, module_of, districts, links, cells): the planar city of one hub (pure in hub_id)"""
    import pads
    seed = int(hashlib.sha1(hub_id.encode("utf-8")).hexdigest()[:12], 16)
    rng = random.Random(seed)
    rot_x = _yaw((1.0, 0.0, 0.0))
    nodes, spinning, haze, module_of, districts, docks = [], [], [], {}, [], []
    rng_u = random.Random(seed ^ 0x5A5A5A)                 # the underside (keeps the top side as it was)

    def add(nid, folder, pos, rot, role, group):
        nodes.append((nid, folder, tuple(round(c, 3) + 0.0 for c in pos), tuple(round(c, 4) + 0.0 for c in rot), role,
                      None if not nodes else nodes[0][0]))
        module_of[nid] = group

    # the grid: an irregular blob of blocks (an ellipse with a noisy rim, a few holes), the largest connected piece
    N, M = rng.randint(7, 9), rng.randint(6, 8)
    ci, cj = (N - 1) / 2, (M - 1) / 2
    noise = {(i, j): rng.uniform(-0.28, 0.22) for i in range(N) for j in range(M)}
    rr = {(i, j): math.hypot((i - ci) / (N / 2), (j - cj) / (M / 2)) for i in range(N) for j in range(M)}
    cells = {k for k in rr if rr[k] < 1.0 + noise[k] and (rr[k] < 0.3 or rng.random() > 0.06)}
    centre = min(cells, key=lambda k: rr[k])
    seen, todo = {centre}, [centre]
    while todo:
        i, j = todo.pop()
        for n in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
            if n in cells and n not in seen:
                seen.add(n)
                todo.append(n)
    cells = seen
    # street gaps by traffic class (author 2026-11-17: «надо исходить из того, какие корабли будут двигаться по "городу"»
    # -> 3 classes; «ньютоновскому кораблю в таком месте делать особо и нечего. поэтому все доки должны быть на
    # окраинах»): local streets carry Assisted ships up to ~50 m (gap 300 m), avenues (every third line + the lines
    # round the centre) Assisted up to ~100 m (gap 500 m); Newtonian / larger ships only dock on the rim.
    av_x = lambda i: i % 3 == 0 or abs(i + 0.5 - ci) < 1.0
    av_z = lambda j: j % 3 == 0 or abs(j + 0.5 - cj) < 1.0
    gw_x = [STREET_AVENUE if av_x(i) else STREET_LOCAL for i in range(N - 1)]
    gw_z = [STREET_AVENUE if av_z(j) else STREET_LOCAL for j in range(M - 1)]

    def axis_centres(n, gw):
        out = [0.0]
        for i in range(1, n):
            out.append(out[-1] + CELL + gw[i - 1])
        sh = (out[0] + out[-1]) / 2
        return [v - sh for v in out]
    X, Z = axis_centres(N, gw_x), axis_centres(M, gw_z)

    def line_at(cs, gw, i):
        """the centre of the street line i + 0.5 (between cells i and i + 1); -1 / n-1: just outside the grid"""
        if i < 0:
            return cs[0] - CELL / 2 - STREET_LOCAL / 2
        if i >= len(cs) - 1:
            return cs[-1] + CELL / 2 + STREET_LOCAL / 2
        return cs[i] + CELL / 2 + gw[i] / 2
    line_x = lambda i: line_at(X, gw_x, i)
    line_z = lambda j: line_at(Z, gw_z, j)
    pos_of = lambda k: (X[k[0]], 0.0, Z[k[1]])
    nbr4 = lambda k: [(k[0] + 1, k[1]), (k[0] - 1, k[1]), (k[0], k[1] + 1), (k[0], k[1] - 1)]
    rim = {k for k in cells if any(n not in cells for n in nbr4(k))}

    # zones: the canopy in the centre, downtown round it, residential (+ drums, hotels), industry / storage, the port rim
    # the port is ONE waterfront (the side the city faces its traffic): the rim there is port / logistics, the rest of
    # the rim is industry, storage, fuel, repair yards, rigs
    port_dir = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
    kind = {centre: "canopy"}
    for k in sorted(cells - {centre}, key=lambda k: (rr[k], k)):
        r = rr[k]
        front = ((k[0] - ci) / (N / 2)) * port_dir[0] + ((k[1] - cj) / (M / 2)) * port_dir[1] > 0.3
        if k in rim and r > 0.5 and front:
            kind[k] = rng.choice(["port"] * 3 + ["logistics"])
        elif k in rim and r > 0.5:
            kind[k] = rng.choice(["industry"] * 3 + ["storage"] * 3 + ["refuel", "repair", "rig", "logistics"])
        elif r < 0.4:
            kind[k] = "downtown"
        elif r < 0.66:
            kind[k] = rng.choice(["residential"] * 6 + ["hotel", "hab_rot"])
        else:
            kind[k] = rng.choice(["industry", "industry", "storage", "storage", "storage", "logistics"])
    # the dock (author 2026-11-17: «надо еще добавить где-нибудь док и попробовать как автопилот будет строить маршрут
    # через это все»): the residential block nearest the centre but two blocks away from it (clear of the canopy disc) -
    # a ship docking there comes down over the towers
    # MOVED to the outskirts (author: «все доки должны быть на окраинах»): a rim block with an open side away from the
    # port front; the hex dock's body sticks out of that side, its slot facing out (approach from open space), the
    # service block lying along the axis inside the block
    open_sides = lambda k: [o for o in ((1, 0), (-1, 0), (0, 1), (0, -1)) if (k[0] + o[0], k[1] + o[1]) not in cells and
                            not any(n in cells and n != k for n in nbr4((k[0] + o[0], k[1] + o[1])))]
    dock_pool = sorted((k for k in kind if k in rim and kind[k] not in ("port", "logistics") and open_sides(k)),
                       key=lambda k: (rr[k], k))
    dock_cell = dock_pool[0] if dock_pool else None
    for need in ("hotel", "hab_rot"):
        if need not in kind.values():
            res = sorted(k for k in kind if kind[k] in ("residential", "downtown") and abs(k[0] - centre[0]) + abs(k[1] - centre[1]) > 1)
            res = [k for k in res if k != dock_cell]
            if res:
                kind[rng.choice(res)] = need

    # the blocks
    for k in sorted(cells):
        c = pos_of(k)
        kd = kind[k]
        g = f"c{k[0]}_{k[1]}_{kd}"
        h = CELL / 2
        # the girder lattice under the block (in the drum / hotel blocks only the rim lines along X - the bodies hang there)
        rim_only = kd in ("hab_rot", "hotel") or k == dock_cell
        lines_z = [] if rim_only else [-h, 0.0, h]
        lines_x = [-h, h] if rim_only else [-h, 0.0, h]
        for li, x in enumerate(lines_z):
            for s in range(int(CELL // SEG_L)):
                add(f"{g}_fz{li}_{s}", "hubb_girder", (c[0] + x, FRAME_Y, c[2] - h + s * SEG_L), ROT_Z, "frame", g)
        for li, z in enumerate(lines_x):
            for s in range(int(CELL // SEG_L)):
                add(f"{g}_fx{li}_{s}", "hubb_girder", (c[0] - h + s * SEG_L, FRAME_Y, c[2] + z), rot_x, "frame", g)
        placed = []
        if kd == "canopy":
            placed = [("hubv_canopy", ROT_Z, (0.0, 0.0, 0.0))]
        elif kd == "hab_rot":
            # a habitat drum lying in the plane, its axis along X, held at both axle ends by bearings on the rim girders
            spinning.append({"id": f"{g}_drum", "module": "hubb_hab_drum", "position": c, "rotation_deg": rot_x,
                             "spin_axis_local": (0.0, 0.0, 1.0), "rate_deg_s": 9.0 * rng.choice((-1, 1)), "title": ZONE_TITLE[kd]})
            for sx in (-1, 1):
                add(f"{g}_brg{sx}", "hubb_junction", (c[0] + sx * (DRUM_L / 2 + 64.0), 0.0, c[2]), ROT_Z, "bearing", g)
                for s in range(int(CELL // SEG_L)):
                    add(f"{g}_bz{sx}_{s}", "hubb_girder", (c[0] + sx * (h + 14.0), 0.0, c[2] - h + s * SEG_L), ROT_Z, "frame", g)
        elif kd == "hotel":
            # three hotel barrels in a row, mouths out to +X, axles back to a spine girder; static hotel blocks on -X
            for zi, z in enumerate((-180.0, 0.0, 180.0)):
                if len(spinning) < SPIN_CAP:
                    spinning.append({"id": f"{g}_hex{zi}", "module": "hubb_hotel_hex", "position": (c[0] + 160.0, 0.0, c[2] + z),
                                     "rotation_deg": rot_x, "spin_axis_local": (0.0, 0.0, 1.0),
                                     "rate_deg_s": 4.0 * rng.choice((-1, 1)), "title": ZONE_TITLE[kd]})
            for s in range(int(CELL // SEG_L)):
                add(f"{g}_hs{s}", "hubb_girder", (c[0] - 25.0, 0.0, c[2] - h + s * SEG_L), ROT_Z, "frame", g)
            placed = _pack(rng, (-h + 10.0, -h + 10.0, -60.0, h - 10.0), ZONE_MODULES["hotel"])
        else:
            near = max(abs(k[0] - centre[0]), abs(k[1] - centre[1])) == 1
            placed = _pack(rng, (-h + 10.0, -h + 10.0, h - 10.0, h - 10.0), ZONE_MODULES[kd], max_top=400.0 if near else None)
        # the underside
        if kd == "canopy":
            placed = placed + [("hubv_canopy", _yaw((0.0, 0.0, 1.0), True), (0.0, UNDER_Y, 0.0))]
        elif kd in UNDER_OF:
            near = max(abs(k[0] - centre[0]), abs(k[1] - centre[1])) == 1
            placed = placed + [(f, r, (q[0], UNDER_Y, q[2])) for f, r, q in
                               _pack(rng_u, (-h + 10.0, -h + 10.0, h - 10.0, h - 10.0), UNDER_MODULES[UNDER_OF[kd]],
                                     max_top=400.0 if near else None, down=True)]
        if k == dock_cell:
            # the hex dock (src/game/docking/model/HexDockFacility.h: a hexagonal body 400 m along its Z, apothem 150, the
            # entry slot on its -Z face, a fixed service block 600 m behind it): upright, the slot facing +Y, its back
            # face on the street plane, the service block hanging under the plane. The block's own modules are drawn
            # (the rng stays in step) but not placed.
            placed = []
            o2 = open_sides(k)[0]
            o = (float(o2[0]), 0.0, float(o2[1]))
            rot_dock = _yaw(_mul(o, -1.0))                       # local -Z (the slot) = out
            docks.append({"id": "dock", "type": "hub_hex_dock", "position": _add(c, _mul(o, h + 210.0)), "rotation_deg": rot_dock,
                          "spin_deg_s": 2.0, "name": "Док"})
            docks.append({"id": "dock_base", "type": "dock_service_block", "position": _add(c, _mul(o, h + 210.0 - 510.0)),
                          "rotation_deg": rot_dock, "spin_deg_s": 0.0, "name": "Док: сервисный блок"})
            kd = "dock"
            g = f"c{k[0]}_{k[1]}_{kd}"
        for mi, (folder, rot, p) in enumerate(placed):
            wp = _add(c, p)
            add(f"{g}_m{mi}", folder, wp, rot, "module", g)
            if folder in ("hubb_works", "hubv_refinery"):
                from units.hub_vista import REFINERY_FLARE
                R = pads.euler(rot)
                haze.extend({"position": [round(x, 1) for x in _add(apply(R, v), wp)], "radius_m": 300.0, "density": 1.0}
                            for v in (WORKS_VENTS if folder == "hubb_works" else [REFINERY_FLARE]))
        # piers out of the open sides of a rim block (in the plane: their berths lie along the rim)
        if kd in ZONE_PIERS:
            for o in ((1.0, 0.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 0.0, -1.0)):
                out_c = (k[0] + int(o[0]), k[1] + int(o[2]))
                if out_c in cells or any(n in cells and n != k for n in nbr4(out_c)):
                    continue                                    # only into open space (no other block's piers there)
                folder, pitch = rng.choice(ZONE_PIERS[kd])
                along = (o[2], 0.0, -o[0])
                n = int(CELL // pitch)
                for pi in range(n):
                    t = (pi - (n - 1) / 2) * pitch
                    root = _add(c, _add(_mul(o, h), _mul(along, t)))
                    add(f"{g}_p{int(o[0])}{int(o[2])}_{pi}", folder, root, _yaw(o), "dock", g)
        districts.append({"id": g, "kind": kd, "centre": c, "cell": k})

    # streets: a road bridge on a girder down the middle of every gap between two blocks, crossings at the nodes,
    # avenues (every third line + the central ones) with two tunnels and a monorail
    runs = []
    for (i, j) in sorted(cells):
        if (i + 1, j) in cells:
            runs.append(("z", i + 0.5, j))           # a street along Z at column line i+0.5, beside row j
        if (i, j + 1) in cells:
            runs.append(("x", j + 0.5, i))
    node_pts = set()

    def pieces(a0, a1):
        """100 m pieces covering [a0, a1]: offsets from a0; the last one ends at a1 (overlapping, lifted 4 cm)"""
        n = max(1, int(math.ceil((a1 - a0) / SEG_L - 1.0e-6)))
        return [(min(k * SEG_L, a1 - a0 - SEG_L), k == n - 1 and k * SEG_L > a1 - a0 - SEG_L) for k in range(n)]

    for ri, (ax, line, at) in enumerate(runs):
        g = f"s{ri}"
        li = int(line - 0.5)
        kinds = {kind[(li, at)], kind[(li + 1, at)]} if ax == "z" else {kind[(at, li)], kind[(at, li + 1)]}
        av = av_x(li) if ax == "z" else av_z(li)
        gw = gw_x[li] if ax == "z" else gw_z[li]
        if ax == "z":
            x = line_x(li)
            a0, a1 = line_z(at - 1), line_z(at)
            d, rot, side = (0.0, 0.0, 1.0), ROT_Z, (1.0, 0.0, 0.0)
            start = (x, 0.0, a0 + 20.0)
            node_pts |= {(round(x, 1), round(a0, 1)), (round(x, 1), round(a1, 1))}
        else:
            z = line_z(li)
            a0, a1 = line_x(at - 1), line_x(at)
            d, rot, side = (1.0, 0.0, 0.0), rot_x, (0.0, 0.0, -1.0)
            start = (a0 + 20.0, 0.0, z)
            node_pts |= {(round(a0, 1), round(z, 1)), (round(a1, 1), round(z, 1))}
        rot_d = _yaw(d, True)
        run_len = (a1 - a0) - 40.0
        for s_i, (off, lift) in enumerate(pieces(0.0, run_len)):
            p = _add(start, _mul(d, off))
            dy = 0.04 if lift else 0.0
            add(f"{g}_r{s_i}", "hubb_road", (p[0], dy, p[2]), rot, "road", g)
            add(f"{g}_g{s_i}", "hubb_girder", (p[0], FRAME_Y, p[2]), rot, "girder", g)
            add(f"{g}_u{s_i}", "hubb_road", (p[0], UNDER_Y - dy, p[2]), rot_d, "road", g)
            if av:
                for sg in (-1, 1):
                    add(f"{g}_t{sg}_{s_i}", "hubb_tube", _add((p[0], FRAME_Y + 4.0, p[2]), _mul(side, sg * 40.0)), rot, "tube", g)
            if av and kinds & PASSENGER:
                add(f"{g}_mr{s_i}", "hubb_monorail", _add((p[0], 14.0, p[2]), _mul(side, 18.0)), rot, "monorail", g)
        # access bridges from the road edge to the block edges beside it (1..2 per side), girders under them
        mid = _add(start, _mul(d, run_len / 2))
        reach = gw / 2 - 13.0
        for sg in (-1, 1):
            ad = _mul(side, float(sg))
            for ai in range(rng.choice((1, 1, 2))):
                t = rng.uniform(-200.0, 200.0)
                for top in (True, False):
                    base = _add(_add(mid, _mul(d, t if top else -t)), _mul(side, sg * 13.0))
                    for pi_, (off, lift) in enumerate(pieces(0.0, reach)):
                        q = _add(base, _mul(ad, off))
                        dy = 0.04 if lift else 0.0
                        tag = "" if top else "u"
                        add(f"{g}_{tag}a{sg}_{ai}_{pi_}", "hubb_road", (q[0], dy if top else UNDER_Y - dy, q[2]),
                            _yaw(ad, not top), "access", g)
                        add(f"{g}_{tag}ag{sg}_{ai}_{pi_}", "hubb_girder", (q[0], FRAME_Y, q[2]), _yaw(ad), "girder", g)
    lanes, red_zones = _plane_lanes(runs, line_x, line_z, av_x, av_z), []
    plaza, connectors = _plane_connectors(lanes, (X[centre[0]], Z[centre[1]]), [line_x(centre[0] - 1), line_x(centre[0])],
                                          [line_z(centre[1] - 1), line_z(centre[1])])
    lanes = lanes + plaza
    blue = _plane_dock_approaches(lanes, docks)
    for b in blue:                                             # lane end -> its approach: a zero-length connector
        src = next(ln for ln in lanes if ln["id"] == b["from_lane"])
        connectors.append({"id": f"to.{b['id']}", "from_lane": src["id"], "from_station": len(src["stations"]) - 1,
                           "to_lane": b["id"], "to_station": 0, "points": [src["stations"][-1], b["stations"][0]],
                           "radius": b["radius"], "speed": b["speed"], "max_ship_m": b["max_ship_m"]})
    lanes = lanes + blue
    for nid, folder, pos, rot, role, parent in nodes:
        if role == "dock":                                      # the berths of a pier: a red zone (no through traffic)
            import pads
            L, bw = {"hubb_pier_l": (300.0, 64.0), "hubb_pier_m": (160.0, 44.0), "hubb_fuel_dock": (160.0, 44.0)}[folder]
            R = pads.euler(rot)
            red_zones.append({"id": f"red.{nid}", "center": tuple(round(v, 1) for v in _add(pos, apply(R, (0.0, 0.0, L / 2)))),
                              "half_extents": (20.0 + bw + 10.0, 70.0, L / 2 + 20.0), "rotation_deg": rot})
    for ni, (x, z) in enumerate(sorted(node_pts)):
        add(f"x{ni}", "hubp_crossing", (x, 0.0, z), ROT_Z, "crossing", f"xs{ni // 16}")
        add(f"x{ni}_u", "hubp_crossing", (x, UNDER_Y, z), _yaw((0.0, 0.0, 1.0), True), "crossing", f"xs{ni // 16}")
    return {"nodes": nodes, "spinning": spinning, "haze": haze, "module_of": module_of, "districts": districts,
            "links": len(runs), "cells": {k: ("dock" if k == dock_cell else kind[k]) for k in cells}, "grid": (N, M, ci, cj),
            "docks": docks, "lanes": lanes, "connectors": connectors, "red_zones": red_zones}


# ---------------------------------------------------------------- green traffic lanes (author 2026-11-17: «предлагаю
# использовать таки зеленые, синие и красные зоны в пределах хаба ... зеленые тунели - зоны через весь город - карта
# быстрых маршрутов сформированная заранее ... участки для докинга грузовиков - делаем красной зоной ... можно какую-то
# часть вести в центр - там сделать типа площади и из центра уже разлетаться»): every straight street line of 3+
# blocks carries air lanes down the middle of its gap - two one-way lanes side by side (right-hand traffic, 50 m off
# the gap centre, radius 45 m: a T-5000 is 50 x 40), the lanes along Z at 150 m above the street plane, those along X
# at 290 m (no lane crosses another at its level); the same set mirrored under the plane. The four lines round the
# central block make the square round the canopy - the "plaza" every crossing lane passes.
# REVISED the same day (3 traffic classes, free legs only in open sky): the two directions of a street are stacked in
# its centre (Z streets: +Z at 100 m, -Z at 210 m; X streets: +X at 320 m, -X at 430 m above the plane, mirrored
# under it) - no lane crosses another at its level, and the clearance to the blocks is the whole half-gap.
# Local streets: Assisted <= 55 m, r 35 m, 100 m/s; avenues: Assisted <= 100 m, r 55 m, 150 m/s.
LANE_LEVELS = {("z", 1): 100.0, ("z", -1): 210.0, ("x", 1): 320.0, ("x", -1): 430.0}
LANE_CLASS = {False: {"radius": 35.0, "speed": 100.0, "max_ship_m": 55.0},
              True: {"radius": 55.0, "speed": 150.0, "max_ship_m": 100.0}}
LANE_MIN_BLOCKS = 3
LANE_OVERRUN = 120.0           # past the end crossings


def _plane_lanes(runs, line_x, line_z, av_x, av_z):
    """-> [{id, a, b, stations (city frame, in travel order), radius, speed, max_ship_m, level}]"""
    by_line = {}
    for ax, line, at in runs:
        by_line.setdefault((ax, int(line - 0.5)), []).append(at)
    lanes = []
    for (ax, li), ats in sorted(by_line.items()):
        ats = sorted(ats)
        seg = [ats[0]]
        groups = []
        for v in ats[1:]:
            if v == seg[-1] + 1:
                seg.append(v)
            else:
                groups.append(seg)
                seg = [v]
        groups.append(seg)
        avenue = av_x(li) if ax == "z" else av_z(li)
        cls = LANE_CLASS[avenue]
        c_line = line_x(li) if ax == "z" else line_z(li)
        node_at = line_z if ax == "z" else line_x
        for gi, gr in enumerate(groups):
            if len(gr) < LANE_MIN_BLOCKS:
                continue
            along = [node_at(gr[0] - 1) - LANE_OVERRUN] + [node_at(j - 1) for j in gr] + [node_at(gr[-1]), node_at(gr[-1]) + LANE_OVERRUN]
            for level, ys in (("top", 1.0), ("under", -1.0)):
                for sgn in (1, -1):
                    y = ys * LANE_LEVELS[(ax, sgn)]
                    st = [(c_line, y, v) if ax == "z" else (v, y, c_line) for v in along]
                    if sgn < 0:
                        st = st[::-1]
                    lanes.append({"id": f"green.{ax}{li + 0.5:g}.{gi}.{level}.{'p' if sgn > 0 else 'n'}", "a": st[0], "b": st[-1],
                                  "stations": [tuple(round(c, 2) for c in q) for q in st], "radius": cls["radius"],
                                  "speed": cls["speed"], "max_ship_m": cls["max_ship_m"], "level": level, "avenue": avenue})
    return lanes


# ---------------------------------------------------------------- pre-written lane CONNECTORS (author 2026-11-17: «в
# зеленой и синей зонах маршрут должен быть уже прописан заранее ... можно какую-то часть вести в центр - там сделать
# типа площади и из центра уже разлетаться по нужным направлениям»). A ship flies only pre-written geometry inside the
# city; free legs exist only in open sky beyond the rim (the lanes' first / last stations are the open portals).
#  - TURNS at every crossing: from a lane to each perpendicular lane of the same side (top / under) - a straight climb
#    run along the first street, a quarter arc inside the crossing square (radius TURN_R: 100 m keeps 170 m off the
#    block corners), a run along the second street; the height changes smoothly over the whole connector (the levels
#    differ by up to 330 m: the ship crosses the other levels of the junction - a junction is where levels cross).
#  - the PLAZA: a one-way ring under the canopy disc (top side; mirrored under the plane) round the central block,
#    PLAZA_Y above the plane, between the canopy's trumpet and the four avenues round the centre. At its four axis
#    points the avenue lane running the ring's way merges in (an S-curve over PLAZA_MERGE m) and diverges out again.
#    The ring's stations run 0..630 deg (one and three quarter laps) so that from any merge every diverge is ahead.
# Each connector joins station from_station of from_lane to station to_station of to_lane (the stations are inserted
# into the lanes here); its points include both stations.
TURN_R, TURN_RUN = 100.0, 150.0
PLAZA_Y, PLAZA_MERGE, PLAZA_SPEED = 265.0, 450.0, 80.0


def _norm_h(v):
    n = math.hypot(v[0], v[2])
    return (v[0] / n, 0.0, v[2] / n)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def _lane_dir(ln):
    return _norm_h(tuple(b - a for a, b in zip(ln["stations"][0], ln["stations"][1])))


def _insert_station(ln, p):
    """put point p (on the lane's straight polyline) into its stations in travel order"""
    st = ln["stations"]
    p = tuple(round(c, 2) for c in p)
    for q in st:
        if _dist(q, p) < 0.5:
            return
    for k in range(len(st) - 1):
        a, b = st[k], st[k + 1]
        ab = tuple(y - x for x, y in zip(a, b))
        t = _dot(tuple(y - x for x, y in zip(a, p)), ab) / _dot(ab, ab)
        if 0.0 < t < 1.0 and _dist(tuple(x + c * t for x, c in zip(a, ab)), p) < 0.5:
            st.insert(k + 1, p)
            return
    raise ValueError(f"station {p} is not on lane {ln['id']}")


def _vprofile(horiz, y0, y1):
    """horizontal polyline -> 3D with the height eased from y0 to y1 over its length"""
    cum = [0.0]
    for a, b in zip(horiz, horiz[1:]):
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[2] - a[2]))
    return [(p[0], y0 + (y1 - y0) * _smooth(c / cum[-1]), p[2]) for p, c in zip(horiz, cum)]


def _seg_pts(a, b, step=25.0):
    n = max(1, int(math.ceil(_dist(a, b) / step)))
    return [tuple(x + (y - x) * i / n for x, y in zip(a, b)) for i in range(n + 1)]


def _hermite(p0, t0, p1, t1, n=24):
    out = []
    for i in range(n + 1):
        s = i / n
        h00, h10, h01, h11 = 2 * s ** 3 - 3 * s ** 2 + 1, s ** 3 - 2 * s ** 2 + s, -2 * s ** 3 + 3 * s ** 2, s ** 3 - s ** 2
        out.append(tuple(h00 * a + h10 * b + h01 * c + h11 * d for a, b, c, d in zip(p0, t0, p1, t1)))
    return out


def _plane_connectors(lanes, centre_xz, lines_x, lines_z):
    """-> (plaza lanes, connectors); inserts the connectors' stations into the lanes"""
    conns = []
    D = TURN_R + TURN_RUN
    # ---- turns at the crossings
    by_xz = {}
    for ln in lanes:
        for q in ln["stations"][1:-1]:                      # the crossing stations (not the overrun ends)
            by_xz.setdefault((round(q[0], 1), round(q[2], 1), ln["level"]), []).append(ln)
    for (x, z, level), here in sorted(by_xz.items(), key=lambda kv: kv[0]):
        for A in here:
            fA = _lane_dir(A)
            yA = A["stations"][0][1]
            for B in here:
                fB = _lane_dir(B)
                if B is A or abs(_dot(fA, fB)) > 0.1:
                    continue
                yB = B["stations"][0][1]
                c = (x, 0.0, z)
                # room for the run on both lanes (not into an overrun end)
                if _dot(_sub(c, A["stations"][0]), fA) < D + 20.0 or _dot(_sub(B["stations"][-1], c), fB) < D + 20.0:
                    continue
                off, a0 = _add(c, _mul(fA, -D)), _add(c, _mul(fA, -TURN_R))
                q = _add(a0, _mul(fB, TURN_R))
                arc = [_add(q, _add(_mul(fB, -TURN_R * math.cos(math.radians(f))), _mul(fA, TURN_R * math.sin(math.radians(f)))))
                       for f in range(0, 91, 10)]
                b0, on = _add(c, _mul(fB, TURN_R)), _add(c, _mul(fB, D))
                horiz = _seg_pts(off, a0)[:-1] + arc + _seg_pts(b0, on)[1:]
                pts = _vprofile(horiz, yA, yB)
                p_off, p_on = (off[0], yA, off[2]), (on[0], yB, on[2])
                _insert_station(A, p_off)
                _insert_station(B, p_on)
                conns.append({"id": f"turn.{A['id']}>{B['id']}", "from_lane": A["id"], "from_point": p_off,
                              "to_lane": B["id"], "to_point": p_on, "points": pts,
                              "radius": min(A["radius"], B["radius"]), "speed": min(A["speed"], B["speed"]),
                              "max_ship_m": min(A["max_ship_m"], B["max_ship_m"])})
    # ---- the plaza ring(s)
    cx, cz = centre_xz
    reach = min(abs(v - cx) for v in lines_x)
    reach = min([reach] + [abs(v - cz) for v in lines_z])
    rp = reach - 150.0
    plaza = []
    cls = LANE_CLASS[True]
    for level, ys in (("top", 1.0), ("under", -1.0)):
        y = ys * PLAZA_Y
        st = [(round(cx + rp * math.cos(math.radians(t)), 2), y, round(cz + rp * math.sin(math.radians(t)), 2))
              for t in range(0, 631, 10)]
        ring = {"id": f"green.plaza.{level}", "a": st[0], "b": st[-1], "stations": st, "radius": cls["radius"],
                "speed": PLAZA_SPEED, "max_ship_m": cls["max_ship_m"], "level": level, "avenue": True, "plaza": True}
        plaza.append(ring)
        for th in (0, 90, 180, 270):
            P = st[th // 10]
            t = (-math.sin(math.radians(th)), 0.0, math.cos(math.radians(th)))
            u = (math.cos(math.radians(th)), 0.0, math.sin(math.radians(th)))
            best = None
            for ln in lanes:
                if ln["level"] != level or _dot(_lane_dir(ln), t) < 0.99:
                    continue
                d = _dot(_sub(ln["stations"][0], P), u)
                if not 50.0 < d < 400.0:
                    continue
                s0, s1 = _dot(_sub(ln["stations"][0], P), t), _dot(_sub(ln["stations"][-1], P), t)
                if s0 > -PLAZA_MERGE - 20.0 or s1 < PLAZA_MERGE + 20.0:
                    continue
                if best is None or d < best[0]:
                    best = (d, ln)
            if not best:
                continue
            d, ln = best
            yl = ln["stations"][0][1]
            side = _add(P, _mul(u, d))
            p_in = (side[0] - t[0] * PLAZA_MERGE, yl, side[2] - t[2] * PLAZA_MERGE)
            p_out = (side[0] + t[0] * PLAZA_MERGE, yl, side[2] + t[2] * PLAZA_MERGE)
            tan = _mul(t, PLAZA_MERGE)
            _insert_station(ln, p_in)
            _insert_station(ln, p_out)
            conns.append({"id": f"plaza.in.{ln['id']}@{th}", "from_lane": ln["id"], "from_point": p_in, "to_lane": ring["id"],
                          "to_point": P, "points": _hermite(p_in, tan, P, tan), "radius": cls["radius"], "speed": PLAZA_SPEED,
                          "max_ship_m": ln["max_ship_m"]})
            for lap in range(th // 10, len(st), 36):
                if lap == 0:
                    continue                                  # the ring's very first station: nothing ridden yet
                Q = st[lap]
                conns.append({"id": f"plaza.out@{lap * 10}.{ln['id']}", "from_lane": ring["id"], "from_point": Q,
                              "to_lane": ln["id"], "to_point": p_out, "points": _hermite(Q, tan, p_out, tan),
                              "radius": cls["radius"], "speed": PLAZA_SPEED, "max_ship_m": ln["max_ship_m"]})
    # resolve the stations
    by_id = {ln["id"]: ln for ln in lanes + plaza}

    def index(lid, p):
        st = by_id[lid]["stations"]
        k = min(range(len(st)), key=lambda i: _dist(st[i], p))
        assert _dist(st[k], p) < 0.5, (lid, p)
        return k
    for cn in conns:
        cn["from_station"] = index(cn["from_lane"], cn["from_point"])
        cn["to_station"] = index(cn["to_lane"], cn["to_point"])
        cn["points"] = [tuple(round(v, 1) for v in q) for q in cn["points"]]
    for ln in lanes + plaza:
        ln["a"], ln["b"] = ln["stations"][0], ln["stations"][-1]
    return plaza, conns


# ---------------------------------------------------------------- BLUE dock approaches (author 2026-11-17: «в зеленой
# и синей зонах маршрут должен быть уже прописан заранее ... а после выхода из нее или в новую зону или уже в док»).
# A dock on the rim faces out; the lanes leave the rim outward too - a ship leaving a lane near the dock would have to
# turn round inside a kilometre (live run 8: the final leg infeasible). So from the end of every lane that ends within
# DOCK_APPROACH_REACH of the dock a pre-written BLUE approach runs on: the shortest path with BLUE_TURN_R turns (Dubins
# CSC in the plane, the height eased) onto the dock's axis, DOCK_AXIS_OUT in front of its slot, heading into it. The
# ship leaves the approach there and flies the last straight along the axis to the dock's own holds / axis join.
# DOCK_AXIS_OUT (live run 14): the final leg along the axis to the docking goal (~500 m off the slot) needs its 150 m
# start straight + ~1 km turn budget + 900 m goal straight; 1.7 km left 1.1 km and the squeezed curve looped back
BLUE_TURN_R, BLUE_SPEED, DOCK_AXIS_OUT, DOCK_APPROACH_REACH = 400.0, 80.0, 3300.0, 3000.0
BLUE_AXIS_RUN = 300.0


def _dubins(p0, a0, p1, a1, r):
    """shortest CSC path in the x-z plane from p0 heading a0 (rad) to p1 heading a1 -> list of (x, z) points"""
    def ctr(p, a, left):
        return (p[0] - r * math.sin(a), p[1] + r * math.cos(a)) if left else (p[0] + r * math.sin(a), p[1] - r * math.cos(a))

    def arc(c, frm, to, left):
        s0, s1 = math.atan2(frm[1] - c[1], frm[0] - c[0]), math.atan2(to[1] - c[1], to[0] - c[0])
        d = (s1 - s0) % (2 * math.pi) if left else (s0 - s1) % (2 * math.pi)
        n = max(1, int(math.ceil(d / math.radians(10.0))))
        sg = 1.0 if left else -1.0
        return [(c[0] + r * math.cos(s0 + sg * d * i / n), c[1] + r * math.sin(s0 + sg * d * i / n)) for i in range(n + 1)], d * r

    best = None
    for w0, w1 in ((True, True), (False, False), (True, False), (False, True)):
        c0, c1 = ctr(p0, a0, w0), ctr(p1, a1, w1)
        v = (c1[0] - c0[0], c1[1] - c0[1])
        D = math.hypot(*v)
        if D < 1.0e-6:
            continue
        if w0 == w1:
            n0 = (v[1] / D, -v[0] / D) if w0 else (-v[1] / D, v[0] / D)
            n1 = n0
        else:
            if D < 2 * r:
                continue
            phi, k = math.atan2(v[1], v[0]), math.acos(2 * r / D)
            n0 = None
            for ang in (phi + k, phi - k):
                cand = (math.cos(ang), math.sin(ang))
                u = (-cand[1], cand[0]) if w0 else (cand[1], -cand[0])
                if u[0] * v[0] + u[1] * v[1] > 0:
                    n0 = cand
            if n0 is None:
                continue
            n1 = (-n0[0], -n0[1])
        t0 = (c0[0] + r * n0[0], c0[1] + r * n0[1])
        t1 = (c1[0] + r * n1[0], c1[1] + r * n1[1])
        arc0, l0 = arc(c0, p0, t0, w0)
        arc1, l1 = arc(c1, t1, p1, w1)
        ls = math.hypot(t1[0] - t0[0], t1[1] - t0[1])
        mid = [(x, z) for x, _, z in _seg_pts((t0[0], 0.0, t0[1]), (t1[0], 0.0, t1[1]))]
        total = l0 + ls + l1
        if best is None or total < best[0]:
            best = (total, arc0 + mid[1:-1] + arc1)
    return best[1] if best else None


def _plane_dock_approaches(lanes, docks):
    """-> the blue approach lanes (each starting at a green lane's last station)"""
    out = []
    for dk in docks:
        if dk.get("type") != "hub_hex_dock":
            continue
        import pads
        R = pads.euler(dk["rotation_deg"])
        o = _norm_h(apply(R, (0.0, 0.0, -1.0)))                 # the slot's outward direction
        A = _add(dk["position"], _mul(o, DOCK_AXIS_OUT))
        A = (A[0], dk["position"][1], A[2])
        a1 = math.atan2(-o[2], -o[0])                              # arriving into the slot
        for ln in lanes:
            if ln.get("plaza") or ln.get("kind") == "blue":
                continue
            E = ln["stations"][-1]
            if math.hypot(E[0] - dk["position"][0], E[2] - dk["position"][2]) > DOCK_APPROACH_REACH:
                continue
            f = _lane_dir(ln)
            # the curve ends BLUE_AXIS_RUN before A; the last stretch is an exact straight on the axis (the thinned
            # arc stations alone left the final segment 5 deg off it)
            A0 = _add(A, _mul(o, BLUE_AXIS_RUN))
            flat = _dubins((E[0], E[2]), math.atan2(f[2], f[0]), (A0[0], A0[2]), a1, BLUE_TURN_R)
            if not flat:
                continue
            pts = _vprofile([(x, 0.0, z) for x, z in flat], E[1], A[1])
            pts[0], pts[-1] = E, (A0[0], A[1], A0[2])
            pts.append(A)
            # thin the stations to ~every 50 m (the arcs are sampled at 10 deg = 70 m, the straights at 25 m)
            st = [pts[0]]
            for q in pts[1:-2]:
                if _dist(q, st[-1]) >= 50.0:
                    st.append(q)
            st += pts[-2:]
            out.append({"id": f"blue.{dk['id']}.from.{ln['id']}", "kind": "blue", "a": st[0], "b": st[-1],
                        "stations": [tuple(round(c, 2) for c in q) for q in st], "radius": ln["radius"], "speed": BLUE_SPEED,
                        "max_ship_m": ln["max_ship_m"], "level": ln["level"], "avenue": ln["avenue"], "from_lane": ln["id"]})
    return out


def hub_layout(hub_id):
    """the settlement of any generated hub id: a planar city for the PLANE_HUBS, the belt settlement otherwise"""
    if hub_id in PLANE_HUBS:
        return plane_layout(hub_id)
    from units.hub_belt import belt_layout
    return belt_layout(hub_id)


PLANE_HUBS = ["plane_demo"]
SETS = {}
for _hid in PLANE_HUBS:
    from units.hub_belt import belt_proxies as _bp
    _L = plane_layout(_hid)
    ROOTS[f"hub_{_hid}"] = "hub"
    SETS[f"hub_{_hid}"] = {
        "title": f"Плоский парящий город «{_hid}» (генератор)",
        "description": f"Сгенерировано из id «{_hid}»: {len(_L['districts'])} кварталов, {_L['links']} улиц, "
                       f"{len(_L['nodes'])} неподвижных узлов + {len(_L['spinning'])} вращающихся тел (отдельные объекты).",
        "nodes": _L["nodes"],
        "view_mode": "boxes",
        "modules": _L["module_of"],
        "proxies": _bp(_L),
        "proxy_error_m": 12.0,
        "view_lods": {0: 1, 1: 2, 2: 2}}
