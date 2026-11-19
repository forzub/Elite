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
import os
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_mega import FACADE, FACADE_L  # noqa: F401
from units.hub_belt import (CONT, DRUM_L, GIRDER_H, GRATE, HX_L, MARK, ROAD, SEG_L, SPIN_CAP, WORKS_VENTS, _add, _frame,
                            _mul, lamp, road_strip)
from units.hub_lanes import _plane_connectors, _plane_dock_approaches, _plane_lanes


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
              "repair": "Ремонтные доки", "rig": "Платформы", "canopy": "Диск", "dock": "Док",
              "power": "Энергоблок", "services": "Службы: полиция и МЧС", "square": "Площадь", "void": "Просвет"}
WHEELED = {"storage", "industry", "port", "logistics", "refuel", "repair", "rig", "power"}
PASSENGER = {"downtown", "residential", "hotel", "hab_rot", "canopy", "square"}


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


# ---------------------------------------------------------------- a city after an attack (author 2026-11-18: «можешь ли ты
# собрать поврежденный город из поврежденных мешей?»): RUIN_OF hubs are the layout of another hub with the damage of a
# strike applied - 3..5 epicentres (seeded by the ruin's own id); a residential / office building (hub_damage.BUILDINGS)
# near one is swapped for a damaged variant (missile / laser + railgun / gutted facade) with a probability falling off
# with the distance, and the other modules close to an epicentre are blown away (their girders stay).
RUIN_OF = {"plane_ruin": "plane_demo"}
RUIN_REACH = 1800.0           # an epicentre's reach (m): the damage probability falls to 0 there
RUIN_BLOWN = 380.0            # within this of an epicentre a module that cannot be damaged is gone


def _ruin(L, hub_id):
    from units.hub_damage_names import DAMAGED_FRAME, DAMAGED_OF
    rng = random.Random(int(hashlib.sha1(("ruin:" + hub_id).encode("utf-8")).hexdigest()[:12], 16))
    centres = [d["centre"] for d in L["districts"]]
    homes = [d["centre"] for d in L["districts"] if d["kind"] in ("downtown", "residential", "hotel", "hab_rot")] or centres
    strikes = [rng.choice(homes if rng.random() < 0.75 else centres) for _ in range(rng.randint(3, 5))]
    strikes = [(c[0] + rng.uniform(-250, 250), c[2] + rng.uniform(-250, 250)) for c in strikes]
    nodes, damaged, blown = [], 0, 0
    for node in L["nodes"]:
        nid, folder, pos, rot, role, parent = node
        d = min(math.hypot(pos[0] - x, pos[2] - z) for x, z in strikes)
        p = max(0.0, 1.0 - d / RUIN_REACH)
        if folder in DAMAGED_FRAME and d < RUIN_BLOWN:      # the frame under a blast: bent / snapped girders (own seed)
            fr = random.Random(zlib_crc("ruin_frame:" + hub_id + ":" + nid))
            if fr.random() < 0.6:
                nodes.append((nid, fr.choice(DAMAGED_FRAME[folder]), pos, rot, role, parent))
                continue
        if folder in DAMAGED_OF and rng.random() < 0.15 + 0.85 * p:
            nodes.append((nid, rng.choice(DAMAGED_OF[folder]), pos, rot, role, parent))
            damaged += 1
            continue
        if role == "module" and folder not in DAMAGED_OF and d < RUIN_BLOWN and rng.random() < 0.7:
            blown += 1
            L["module_of"].pop(nid, None)
            continue
        nodes.append(node)
    L["nodes"] = nodes
    L["ruin"] = {"strikes": strikes, "damaged": damaged, "blown": blown}
    return L


# ================================================================ the hub MAP (author 2026-11-17: hub editor - «слои / этажи,
# карточки модулей на клетках как в SimCity, любой 2D контур; генератор строит хаб по карте; сгенерированную стартовую
# раскладку можно править»). The planar city is built in two steps: hub id -> MAP (the random generator, _generate_map)
# and MAP -> city (layout_from_map, no randomness except per-street seeds). A map saved by the mesh editor's hub mode
# (tools/models/hub_maps/<hub>.json, schema elite.hub_map.v1) replaces the generated one.
#   grid      cell / street sizes; anchor: the centre of one cell (positions stay put when the outline grows)
#   avenues   {"x": [street line i, between columns i and i+1], "z": [...]}: avenue class (500 m gap, tunnels, monorail)
#   cells     [{i, j, zone, top: [card], under: [card], spin: [deg/s], piers: {"ox,oz": module}, dock_side: [ox, oz]}]
#             card = {module, x, z, yaw}: the module origin in block metres, yaw 0 / 90 / 180 / 270 (0: its +Z = +Z)
#   lanes_off the ids of generated lanes switched off (one-way streets, closed corridors)
#   red_zones extra red zones drawn on the map: {id, x, z, w, d, h}
MAP_SCHEMA = "elite.hub_map.v1"
MAP_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "hub_maps")
LAYERS = {"top": 0.0, "under": UNDER_Y}
YAWS = {0: (0.0, 0.0, 1.0), 90: (1.0, 0.0, 0.0), 180: (0.0, 0.0, -1.0), 270: (-1.0, 0.0, 0.0)}
SPECIAL_ZONES = ("canopy", "hab_rot", "hotel", "dock")


def _card_rot(yaw, down):
    return _yaw(YAWS[int(yaw) % 360], down)


def _rot_yaw(rot, down):
    for y in YAWS:
        if all(abs(a - b) < 1.0e-6 for a, b in zip(_card_rot(y, down), rot)):
            return y
    raise ValueError(f"not a card yaw: {rot}")


def map_path(hub_id):
    return os.path.join(MAP_DIR, f"{hub_id}.json")


def load_map(hub_id):
    p = map_path(hub_id)
    if not os.path.exists(p):
        return None
    import json
    m = json.loads(open(p, encoding="utf-8").read())
    assert m.get("schema") == MAP_SCHEMA, p
    return m


def hub_map(hub_id):
    """the hub's map: the saved one (the editor's) or the generated start layout"""
    return load_map(hub_id) or _generate_map(hub_id)


PROFILES_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "hub_profiles.json")


def hub_profile(hub_id):
    """the hub's generation profile (tools/models/hub_profiles.json; author 2026-11-18: «какие-то более крупные - делаем
    больше, какие-то меньше, какой-то вытянутый, какой-то имеет примерно форму диска, какие-то состоят из пары слоев»):
    size [N, M] blocks, shape blob | disc | long, under (the city under the plane) true / false, style (a national
    style kit, tools/models/units/hub_styles.py), port (0..1: how much of the rim is the port front)"""
    import json
    if not os.path.exists(PROFILES_PATH):
        return None
    return json.load(open(PROFILES_PATH, encoding="utf-8")).get("hubs", {}).get(hub_id)


def _zone_modules(style):
    """the zone -> modules table of a style (the default kit where the style has no entry)"""
    if not style:
        return ZONE_MODULES, UNDER_MODULES
    from units.hub_styles import style_zone_modules
    zm = dict(ZONE_MODULES)
    zm.update(style_zone_modules(style))
    um = dict(UNDER_MODULES)
    if "downtown" in zm and style_zone_modules(style).get("downtown"):
        um["towers"] = style_zone_modules(style)["downtown"]
    if style_zone_modules(style).get("residential"):
        um["res"] = style_zone_modules(style)["residential"]
    return zm, um


def _generate_map(hub_id):
    """the random generator: hub id -> map (pure in the id: the same id is always the same city)"""
    seed = int(hashlib.sha1(hub_id.encode("utf-8")).hexdigest()[:12], 16)
    rng = random.Random(seed)
    rng_u = random.Random(seed ^ 0x5A5A5A)                 # the underside (keeps the top side as it was)
    prof = hub_profile(hub_id) or {}
    zone_modules, under_modules = _zone_modules(prof.get("style"))
    # the grid: an irregular blob of blocks (an ellipse with a noisy rim, a few holes), the largest connected piece
    N, M = rng.randint(7, 9), rng.randint(6, 8)
    if prof.get("size"):
        N, M = int(prof["size"][0]), int(prof["size"][1])
    shape = prof.get("shape", "blob")
    ci, cj = (N - 1) / 2, (M - 1) / 2
    noise = {(i, j): rng.uniform(-0.28, 0.22) for i in range(N) for j in range(M)}
    if shape == "disc":                                    # a round outline: almost no noise on the rim
        noise = {k: v * 0.2 for k, v in noise.items()}
    elif shape == "spire":                                 # a spire cluster (units/hub_spire.py): a compact round core
        noise = {k: v * 0.35 for k, v in noise.items()}
    elif shape == "long":                                  # a long city: the ends rounded, the sides straight
        noise = {k: v * 0.5 for k, v in noise.items()}
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
    av_x = [i for i in range(N - 1) if i % 3 == 0 or abs(i + 0.5 - ci) < 1.0]
    av_z = [j for j in range(M - 1) if j % 3 == 0 or abs(j + 0.5 - cj) < 1.0]
    nbr4 = lambda k: [(k[0] + 1, k[1]), (k[0] - 1, k[1]), (k[0], k[1] + 1), (k[0], k[1] - 1)]
    rim = {k for k in cells if any(n not in cells for n in nbr4(k))}
    # zones: the canopy in the centre, downtown round it, residential (+ drums, hotels), industry / storage, the port rim
    # the port is ONE waterfront (the side the city faces its traffic): the rim there is port / logistics, the rest of
    # the rim is industry, storage, fuel, repair yards, rigs
    port_dir = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
    kind = {centre: "canopy"}
    for k in sorted(cells - {centre}, key=lambda k: (rr[k], k)):
        r = rr[k]
        front = ((k[0] - ci) / (N / 2)) * port_dir[0] + ((k[1] - cj) / (M / 2)) * port_dir[1] > 0.3 - 0.8 * (prof.get("port", 0.5) - 0.5)
        if k in rim and r > 0.5 and front:
            kind[k] = rng.choice(["port"] * 3 + ["logistics"])
        elif k in rim and r > 0.5:
            kind[k] = rng.choice(["industry"] * 3 + ["storage"] * 3 + ["refuel", "repair", "rig", "logistics"])
        elif r < (0.62 if shape == "spire" else 0.4):     # a spire cluster: a wider downtown (its towers)
            kind[k] = "downtown"
        elif r < 0.66:
            kind[k] = rng.choice(["residential"] * 6 + ["hotel", "hab_rot"])
        else:
            kind[k] = rng.choice(["industry", "industry", "storage", "storage", "storage", "logistics"])
    # the dock (author: «все доки должны быть на окраинах»): a rim block with an open side away from the port front;
    # the hex dock's body sticks out of that side, its slot facing out (approach from open space)
    dock_pool = sorted((k for k in kind if k in rim and kind[k] not in ("port", "logistics") and _open_sides(k, cells)),
                       key=lambda k: (rr[k], k))
    dock_cell = dock_pool[0] if dock_pool else None
    for need in ("hotel", "hab_rot"):
        if need not in kind.values():
            res = sorted(k for k in kind if kind[k] in ("residential", "downtown") and abs(k[0] - centre[0]) + abs(k[1] - centre[1]) > 1)
            res = [k for k in res if k != dock_cell]
            if res:
                kind[rng.choice(res)] = need
    h = CELL / 2
    out_cells = []
    n_spin = 0                                             # the spinning bodies so far (SPIN_CAP: the rng draws stop)
    for k in sorted(cells):
        kd = kind[k]
        cell = {"i": k[0], "j": k[1], "zone": kd, "top": [], "under": []}
        placed, under = [], []
        if kd == "canopy":
            placed = [("hubv_canopy", ROT_Z, (0.0, 0.0, 0.0))]
        elif kd == "hab_rot":
            cell["spin"] = [9.0 * rng.choice((-1, 1))]
            n_spin += 1
        elif kd == "hotel":
            cell["spin"] = []
            for _ in range(3):
                if n_spin < SPIN_CAP:
                    cell["spin"].append(4.0 * rng.choice((-1, 1)))
                    n_spin += 1
            placed = _pack(rng, (-h + 10.0, -h + 10.0, -60.0, h - 10.0), zone_modules["hotel"])
        else:
            near = max(abs(k[0] - centre[0]), abs(k[1] - centre[1])) == 1
            placed = _pack(rng, (-h + 10.0, -h + 10.0, h - 10.0, h - 10.0), zone_modules[kd], max_top=400.0 if near else None)
        if kd == "canopy":
            under = [("hubv_canopy", _yaw((0.0, 0.0, 1.0), True), (0.0, UNDER_Y, 0.0))]
        elif kd in UNDER_OF and prof.get("under", True):
            near = max(abs(k[0] - centre[0]), abs(k[1] - centre[1])) == 1
            under = [(f, r, (q[0], UNDER_Y, q[2])) for f, r, q in
                     _pack(rng_u, (-h + 10.0, -h + 10.0, h - 10.0, h - 10.0), under_modules[UNDER_OF[kd]],
                           max_top=400.0 if near else None, down=True)]
        if k == dock_cell:
            # the block's own modules are drawn (the rng stays in step) but not placed
            placed, under = [], []
            cell["zone"] = "dock"
            cell["dock_side"] = list(_open_sides(k, cells)[0])
        cell["top"] = [{"module": f, "x": round(p[0], 3), "z": round(p[2], 3), "yaw": _rot_yaw(r, False)} for f, r, p in placed]
        cell["under"] = [{"module": f, "x": round(p[0], 3), "z": round(p[2], 3), "yaw": _rot_yaw(r, True)} for f, r, p in under]
        if cell["zone"] in ZONE_PIERS:
            cell["piers"] = {}
            for o in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if o in _open_sides(k, cells):
                    cell["piers"][f"{o[0]},{o[1]}"] = rng.choice(ZONE_PIERS[kd])[0]
        out_cells.append(cell)
    mp = add_services({"schema": MAP_SCHEMA, "hub_id": hub_id, "title": hub_id, "generated_from": hub_id,
            "style": prof.get("style", ""),
            "grid": {"cell_m": CELL, "street_local_m": STREET_LOCAL, "street_avenue_m": STREET_AVENUE,
                     "anchor": {"i": 0, "j": 0, "x": None, "z": None}, "extent": [0, 0, N - 1, M - 1]},
            "avenues": {"x": av_x, "z": av_z}, "cells": out_cells, "lanes_off": [], "red_zones": []})
    # the hub's landmarks + its block-layout signature (author 2026-11-18: «каждый хаб узнаваем по индивидуальным
    # чертам»): only where the profile asks for it - the cities frozen before keep their maps
    if prof.get("landmarks"):
        from units.hub_landmark_place import add_landmarks
        mp = add_landmarks(mp, hub_id, prof.get("style", ""))
    if shape == "spire":
        # the spire cluster pass (units/hub_spire.py): spires on the centre / downtown blocks, causeways, the berth
        from units.hub_spire import add_spires
        mp = add_spires(mp, hub_id)
    return mp


SERVICE_CARDS = {
    # a services block (author 2026-11-18: «службы охраны - доки с полицаями и доки с мчс-никами»): the police station
    # with its deck of 3 pads, the rescue station with 2 pads and its hangar, a power unit (tools/models/units/hub_services.py)
    "services": [{"module": "hubs_police_dock", "x": -140.0, "z": -150.0, "yaw": 0},
                 {"module": "hubs_rescue_dock", "x": -140.0, "z": 150.0, "yaw": 0},
                 {"module": "hubs_power_unit", "x": 170.0, "z": 0.0, "yaw": 90}],
    # a power block («добавить генераторы - на квартал или несколько кварталов»): the district power station
    "power": [{"module": "hubs_power_plant", "x": 0.0, "z": 0.0, "yaw": 0}],
}
POWER_BLOCKS_PER = 10          # one power station block per this many blocks (at least one)
SERVICE_BLOCKS_PER = 16        # one police + rescue block per this many blocks (at least one)


def add_services(mp):
    """turn some blocks of a map into power stations and police / rescue service blocks (idempotent: a map that has
    them keeps them): power from industry / storage blocks (inner ones first: the plants stay off the waterfront),
    services on rim blocks with an open side (their decks reachable from open space), away from the port and the dock"""
    cells = mp["cells"]
    keys = {(c["i"], c["j"]) for c in cells}
    n = len(cells)
    have = lambda z: sum(1 for c in cells if c["zone"] == z)
    centre = (sum(c["i"] for c in cells) / n, sum(c["j"] for c in cells) / n)
    dist = lambda c: math.hypot(c["i"] - centre[0], c["j"] - centre[1])
    need_power = max(1, round(n / POWER_BLOCKS_PER)) - have("power")
    pool = sorted((c for c in cells if c["zone"] in ("industry", "storage")), key=lambda c: (dist(c), c["i"], c["j"]))
    for c in pool[:max(0, need_power)]:
        c["zone"], c["top"] = "power", [dict(x) for x in SERVICE_CARDS["power"]]
        c.pop("piers", None)
    need_services = max(1, round(n / SERVICE_BLOCKS_PER)) - have("services")
    pool = sorted((c for c in cells if c["zone"] in ("industry", "storage", "logistics", "residential", "repair", "rig")
                   and _open_sides((c["i"], c["j"]), keys)), key=lambda c: (-dist(c), c["i"], c["j"]))
    chosen = []
    for c in pool:
        if len(chosen) >= max(0, need_services):
            break
        if any(abs(c["i"] - o["i"]) + abs(c["j"] - o["j"]) < 3 for o in chosen):
            continue                                    # spread over the rim
        chosen.append(c)
    for c in chosen:
        c["zone"], c["top"] = "services", [dict(x) for x in SERVICE_CARDS["services"]]
        c.pop("piers", None)
    return mp


def _open_sides(k, cells):
    """the sides of block k that face open space: no block there and no other block next to that spot (a dock / pier
    sticking out of that side meets nothing)"""
    nbr4 = lambda c: [(c[0] + 1, c[1]), (c[0] - 1, c[1]), (c[0], c[1] + 1), (c[0], c[1] - 1)]
    return [o for o in ((1, 0), (-1, 0), (0, 1), (0, -1)) if (k[0] + o[0], k[1] + o[1]) not in cells and
            not any(n in cells and n != k for n in nbr4((k[0] + o[0], k[1] + o[1])))]


def _axis(cells_ix, avenues, anchor_i, anchor_c, extent):
    """-> (centre(i), line(i)): the block centre of column i and the street line between i and i+1 (any i)"""
    gw = lambda i: STREET_AVENUE if i in avenues else STREET_LOCAL
    lo, hi = extent
    cache = {anchor_i: anchor_c}

    def centre(i):
        if i not in cache:
            if i > anchor_i:
                cache[i] = centre(i - 1) + CELL + gw(i - 1)
            else:
                cache[i] = centre(i + 1) - CELL - gw(i)
        return cache[i]
    return centre, (lambda i: centre(i) + CELL / 2 + gw(i) / 2)


def layout_from_map(mp):
    """map -> dict(nodes, spinning, haze, module_of, districts, links, cells, docks, lanes, connectors, red_zones)"""
    import pads
    rot_x = _yaw((1.0, 0.0, 0.0))
    nodes, spinning, haze, module_of, districts, docks = [], [], [], {}, [], []
    hid = mp["hub_id"]

    def add(nid, folder, pos, rot, role, group):
        nodes.append((nid, folder, tuple(round(c, 3) + 0.0 for c in pos), tuple(round(c, 4) + 0.0 for c in rot), role,
                      None if not nodes else nodes[0][0]))
        module_of[nid] = group

    cmap = {(c["i"], c["j"]): c for c in mp["cells"]}
    cells = set(cmap)
    g = mp["grid"]
    i0, j0, i1, j1 = g["extent"]
    av_x, av_z = set(mp["avenues"]["x"]), set(mp["avenues"]["z"])
    # the generated anchor: the grid centred on the extent (as the generator always placed it)
    anc = g["anchor"]
    if anc["x"] is None:
        def centred(n, avs, a0):
            out = [0.0]
            for i in range(1, n):
                out.append(out[-1] + CELL + (STREET_AVENUE if (a0 + i - 1) in avs else STREET_LOCAL))
            return -(out[0] + out[-1]) / 2
        anc = {"i": i0, "j": j0, "x": centred(i1 - i0 + 1, av_x, i0), "z": centred(j1 - j0 + 1, av_z, j0)}
    cx_of, line_x = _axis(None, av_x, anc["i"], anc["x"], (i0, i1))
    cz_of, line_z = _axis(None, av_z, anc["j"], anc["z"], (j0, j1))
    pos_of = lambda k: (cx_of(k[0]), 0.0, cz_of(k[1]))
    zone = {k: cmap[k]["zone"] for k in cells}
    canopy = [k for k in sorted(cells) if zone[k] == "canopy"]
    centre = canopy[0] if canopy else min(cells, key=lambda k: math.hypot(*pos_of(k)[::2]))

    # the blocks
    for k in sorted(cells):
        c = pos_of(k)
        cell = cmap[k]
        kd = zone[k]
        g = f"c{k[0]}_{k[1]}_{kd}"
        h = CELL / 2
        # the girder lattice under the block (in the drum / hotel / dock blocks only the rim lines along X)
        rim_only = kd in ("hab_rot", "hotel", "dock")
        lines_z = [] if rim_only else [-h, 0.0, h]
        lines_x = [-h, h] if rim_only else [-h, 0.0, h]
        for li, x in enumerate(lines_z):
            for s in range(int(CELL // SEG_L)):
                add(f"{g}_fz{li}_{s}", "hubb_girder", (c[0] + x, FRAME_Y, c[2] - h + s * SEG_L), ROT_Z, "frame", g)
        for li, z in enumerate(lines_x):
            for s in range(int(CELL // SEG_L)):
                add(f"{g}_fx{li}_{s}", "hubb_girder", (c[0] - h + s * SEG_L, FRAME_Y, c[2] + z), rot_x, "frame", g)
        spin = cell.get("spin") or []
        if kd == "hab_rot":
            # a habitat drum lying in the plane, its axis along X, held at both axle ends by bearings on the rim girders
            spinning.append({"id": f"{g}_drum", "module": "hubb_hab_drum", "position": c, "rotation_deg": rot_x,
                             "spin_axis_local": (0.0, 0.0, 1.0), "rate_deg_s": float(spin[0]) if spin else 9.0, "title": ZONE_TITLE[kd]})
            for sx in (-1, 1):
                add(f"{g}_brg{sx}", "hubb_junction", (c[0] + sx * (DRUM_L / 2 + 64.0), 0.0, c[2]), ROT_Z, "bearing", g)
                for s in range(int(CELL // SEG_L)):
                    add(f"{g}_bz{sx}_{s}", "hubb_girder", (c[0] + sx * (h + 14.0), 0.0, c[2] - h + s * SEG_L), ROT_Z, "frame", g)
        elif kd == "hotel":
            # three hotel barrels in a row, mouths out to +X, axles back to a spine girder; static hotel blocks on -X
            for zi, z in enumerate((-180.0, 0.0, 180.0)):
                if zi < len(spin) and len(spinning) < SPIN_CAP:
                    spinning.append({"id": f"{g}_hex{zi}", "module": "hubb_hotel_hex", "position": (c[0] + 160.0, 0.0, c[2] + z),
                                     "rotation_deg": rot_x, "spin_axis_local": (0.0, 0.0, 1.0),
                                     "rate_deg_s": float(spin[zi]), "title": ZONE_TITLE[kd]})
            for s in range(int(CELL // SEG_L)):
                add(f"{g}_hs{s}", "hubb_girder", (c[0] - 25.0, 0.0, c[2] - h + s * SEG_L), ROT_Z, "frame", g)
        if kd == "dock" and cell.get("dock_side"):
            # the hex dock (src/game/docking/model/HexDockFacility.h: a hexagonal body 400 m along its Z, apothem 150, the
            # entry slot on its -Z face, a fixed service block 600 m behind it): upright, the slot facing out of the open
            # side, its back face on the street plane, the service block hanging under the plane
            o = (float(cell["dock_side"][0]), 0.0, float(cell["dock_side"][1]))
            rot_dock = _yaw(_mul(o, -1.0))                       # local -Z (the slot) = out
            n_dock = sum(1 for d in docks if d["type"] == "hub_hex_dock")
            sfx = "" if n_dock == 0 else f"_{n_dock + 1}"
            docks.append({"id": "dock" + sfx, "type": "hub_hex_dock", "position": _add(c, _mul(o, h + 210.0)), "rotation_deg": rot_dock,
                          "spin_deg_s": 2.0, "name": "Док" + (f" {n_dock + 1}" if sfx else "")})
            docks.append({"id": "dock_base" + sfx, "type": "dock_service_block", "position": _add(c, _mul(o, h + 210.0 - 510.0)),
                          "rotation_deg": rot_dock, "spin_deg_s": 0.0, "name": "Док: сервисный блок"})
        mi = 0
        for layer, down in (("top", False), ("under", True)):
            y = LAYERS[layer]
            for card in cell.get(layer, []):
                folder, rot = card["module"], _card_rot(card.get("yaw", 0), down)
                # a card's optional "y": its height over (under) the layer's plane - the spire causeways
                wp = _add(c, (card["x"], y + (-1.0 if down else 1.0) * card.get("y", 0.0), card["z"]))
                add(f"{g}_m{mi}", folder, wp, rot, "module", g)
                mi += 1
                if folder in ("hubb_works", "hubv_refinery"):
                    from units.hub_vista import REFINERY_FLARE
                    R = pads.euler(rot)
                    haze.extend({"position": [round(x, 1) for x in _add(apply(R, v), wp)], "radius_m": 300.0, "density": 1.0}
                                for v in (WORKS_VENTS if folder == "hubb_works" else [REFINERY_FLARE]))
        # piers out of the open sides of a rim block (in the plane: their berths lie along the rim)
        piers = cell.get("piers") or {}
        for o2 in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            folder = piers.get(f"{o2[0]},{o2[1]}")
            if not folder or o2 not in _open_sides(k, cells):
                continue                                    # only into open space (no other block's piers there)
            o = (float(o2[0]), 0.0, float(o2[1]))
            pitch = dict(sum(ZONE_PIERS.values(), []))[folder]
            along = (o[2], 0.0, -o[0])
            n = int(CELL // pitch)
            for pi in range(n):
                t = (pi - (n - 1) / 2) * pitch
                root = _add(c, _add(_mul(o, h), _mul(along, t)))
                add(f"{g}_p{o2[0]}{o2[1]}_{pi}", folder, root, _yaw(o), "dock", g)
        districts.append({"id": g, "kind": kd, "centre": c, "cell": k})

    # streets: a road bridge on a girder down the middle of every gap between two blocks, crossings at the nodes,
    # avenues with two tunnels and a monorail. The access bridges are seeded per street (an edit elsewhere leaves them)
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
        li = int(math.floor(line))
        srng = random.Random(zlib_crc(f"{hid}:{ax}:{li}:{at}"))
        kinds = {zone[(li, at)], zone[(li + 1, at)]} if ax == "z" else {zone[(at, li)], zone[(at, li + 1)]}
        av = (li in av_x) if ax == "z" else (li in av_z)
        gw = STREET_AVENUE if av else STREET_LOCAL
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
            for ai in range(srng.choice((1, 1, 2))):
                t = srng.uniform(-200.0, 200.0)
                for top in (True, False):
                    base = _add(_add(mid, _mul(d, t if top else -t)), _mul(side, sg * 13.0))
                    for pi_, (off, lift) in enumerate(pieces(0.0, reach)):
                        q = _add(base, _mul(ad, off))
                        dy = 0.04 if lift else 0.0
                        tag = "" if top else "u"
                        add(f"{g}_{tag}a{sg}_{ai}_{pi_}", "hubb_road", (q[0], dy if top else UNDER_Y - dy, q[2]),
                            _yaw(ad, not top), "access", g)
                        add(f"{g}_{tag}ag{sg}_{ai}_{pi_}", "hubb_girder", (q[0], FRAME_Y, q[2]), _yaw(ad), "girder", g)
    av_xf = lambda i: i in av_x
    av_zf = lambda j: j in av_z
    lanes, red_zones = _plane_lanes(runs, line_x, line_z, av_xf, av_zf), []
    off = set(mp.get("lanes_off") or [])
    lanes = [ln for ln in lanes if ln["id"] not in off]
    plaza, connectors = _plane_connectors(lanes, (cx_of(centre[0]), cz_of(centre[1])), [line_x(centre[0] - 1), line_x(centre[0])],
                                          [line_z(centre[1] - 1), line_z(centre[1])]) if canopy else ([], [])
    plaza = [p for p in plaza if p["id"] not in off]
    ids = {ln["id"] for ln in lanes + plaza}
    connectors = [cn for cn in connectors if cn["from_lane"] in ids and cn["to_lane"] in ids]
    lanes = lanes + plaza
    blue = [b for b in _plane_dock_approaches(lanes, docks) if b["id"] not in off]
    for b in blue:                                             # lane end -> its approach: a zero-length connector
        src = next(ln for ln in lanes if ln["id"] == b["from_lane"])
        connectors.append({"id": f"to.{b['id']}", "from_lane": src["id"], "from_station": len(src["stations"]) - 1,
                           "to_lane": b["id"], "to_station": 0, "points": [src["stations"][-1], b["stations"][0]],
                           "radius": b["radius"], "speed": b["speed"], "max_ship_m": b["max_ship_m"]})
    lanes = lanes + blue
    for nid, folder, pos, rot, role, parent in nodes:
        if role == "dock":                                      # the berths of a pier: a red zone (no through traffic)
            L, bw = {"hubb_pier_l": (300.0, 64.0), "hubb_pier_m": (160.0, 44.0), "hubb_fuel_dock": (160.0, 44.0)}[folder]
            R = pads.euler(rot)
            red_zones.append({"id": f"red.{nid}", "center": tuple(round(v, 1) for v in _add(pos, apply(R, (0.0, 0.0, L / 2)))),
                              "half_extents": (20.0 + bw + 10.0, 70.0, L / 2 + 20.0), "rotation_deg": rot})
        if folder == "hubsp_capital_berth":                     # the moored capital ship and its gantry arms
            from units.hub_spire import berth_red_zone
            cz_, he_ = berth_red_zone(pos, pads.euler(rot))
            red_zones.append({"id": f"red.{nid}", "center": tuple(round(v, 1) for v in cz_), "half_extents": he_,
                              "rotation_deg": rot})
    for rz in mp.get("red_zones") or []:                        # drawn on the map
        red_zones.append({"id": f"red.map.{rz['id']}", "center": (rz["x"], 0.0, rz["z"]),
                          "half_extents": (rz["w"] / 2, rz.get("h", 600.0) / 2, rz["d"] / 2), "rotation_deg": (0.0, 0.0, 0.0)})
    for ni, (x, z) in enumerate(sorted(node_pts)):
        add(f"x{ni}", "hubp_crossing", (x, 0.0, z), ROT_Z, "crossing", f"xs{ni // 16}")
        add(f"x{ni}_u", "hubp_crossing", (x, UNDER_Y, z), _yaw((0.0, 0.0, 1.0), True), "crossing", f"xs{ni // 16}")
    return {"nodes": nodes, "spinning": spinning, "haze": haze, "module_of": module_of, "districts": districts,
            "links": len(runs), "cells": {k: zone[k] for k in cells}, "grid": (i1 - i0 + 1, j1 - j0 + 1, None, None),
            "docks": docks, "lanes": lanes, "connectors": connectors, "red_zones": red_zones,
            "anchor": anc, "lines": {"x": {i: line_x(i) for i in range(i0 - 1, i1 + 1)}, "z": {j: line_z(j) for j in range(j0 - 1, j1 + 1)}},
            "centres": {"x": {i: cx_of(i) for i in range(i0, i1 + 1)}, "z": {j: cz_of(j) for j in range(j0, j1 + 1)}}}


def zlib_crc(s):
    import zlib
    return zlib.crc32(s.encode("utf-8"))


def plane_layout(hub_id):
    """-> dict(nodes, spinning, haze, module_of, districts, links, cells, ...): the planar city of one hub (its map)"""
    if hub_id in RUIN_OF:
        return _ruin(plane_layout(RUIN_OF[hub_id]), hub_id)
    return layout_from_map(hub_map(hub_id))


def hub_layout(hub_id):
    """the settlement of any generated hub id: a planar city for the PLANE_HUBS, the belt settlement otherwise"""
    if hub_id in PLANE_HUBS:
        return plane_layout(hub_id)
    from units.hub_belt import belt_layout
    return belt_layout(hub_id)


# the planar hubs: the generator's demos + every hub the editor saved a map of (tools/models/hub_maps/<hub>.json)
PLANE_HUBS = ["plane_demo", "plane_ruin"] + sorted(f[:-5] for f in (os.listdir(MAP_DIR) if os.path.isdir(MAP_DIR) else [])
                                                   if f.endswith(".json") and f[:-5] not in ("plane_demo", "plane_ruin"))
# + every hub with a generation profile (tools/models/hub_profiles.json: the jurisdiction's hubs built on this template)
if os.path.exists(PROFILES_PATH):
    import json as _json
    PLANE_HUBS += sorted(h for h in _json.load(open(PROFILES_PATH, encoding="utf-8")).get("hubs", {}) if h not in PLANE_HUBS)
class _LazyHubSet:
    """a planar hub's set spec, computed on first access: importing this unit must not lay out every city (hundreds
    of hubs since the colony batch - an eager loop made each import take minutes); build_equipment_models skips a lazy
    set under --only unless the set id itself matches"""
    lazy = True

    def __init__(self, hub_id):
        self.hub_id, self._spec = hub_id, None

    def _load(self):
        if self._spec is None:
            from units.hub_belt import belt_proxies as _bp
            _L = plane_layout(self.hub_id)
            hid = self.hub_id
            self._spec = {
                "title": f"Плоский парящий город «{hid}» (генератор)",
                "description": f"Сгенерировано из id «{hid}»: {len(_L['districts'])} кварталов, {_L['links']} улиц, "
                               f"{len(_L['nodes'])} неподвижных узлов + {len(_L['spinning'])} вращающихся тел (отдельные объекты).",
                "nodes": _L["nodes"],
                "view_mode": "boxes",
                "modules": _L["module_of"],
                "proxies": _bp(_L),
                "proxy_error_m": 12.0,
                "view_lods": {0: 1, 1: 2, 2: 2}}
        return self._spec

    def __getitem__(self, k):
        return self._load()[k]

    def get(self, k, default=None):
        return self._load().get(k, default)

    def __contains__(self, k):
        return k in self._load()


SETS = {}
for _hid in PLANE_HUBS:
    ROOTS[f"hub_{_hid}"] = "hub"
    SETS[f"hub_{_hid}"] = _LazyHubSet(_hid)
