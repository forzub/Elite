"""Belt settlement kit (author 2026-11-17: «давай дорисуем еще модули - огромные резервуары для газа и горючего, как на
газовых хранилищах - вертикально стоящие большие баллоны соединённые трубопроводами. можем начать пробовать делать
поселение в астероидном поясе»).

Modules of a belt settlement hub: storage first (gas bullet batteries, fuel spheres). Sizes in metres; every module's
attachment point is its origin, its "out" axis +Y (it stands on its mounting frame), +Z along its long side. Modules go
to <assets_source>/models/hub/hubb_* (the editor's hub modules) like the hub_district / hub_mega kits.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, FLOOD, HAZ, HULL, LIGHT, STEEL, beacon  # noqa: F401

register_materials({
    "eq_hub_tank_white": ((0.70, 0.71, 0.70), (0.25, 0.25, 0.25), 40),     # gas bullets: pale paint (sun-reflective)
    "eq_hub_tank_fuel": ((0.55, 0.47, 0.30), (0.20, 0.20, 0.18), 30),      # fuel: ochre
    "eq_hub_grating": ((0.20, 0.21, 0.22), (0.30, 0.30, 0.30), 50),        # galleries, stairs, frames
})
TANK_W, TANK_F, GRATE = "eq_hub_tank_white", "eq_hub_tank_fuel", "eq_hub_grating"


def _seg(lod, hi=28):
    return {0: hi, 1: max(12, hi // 2), 2: 10, 3: 8}.get(lod, 8)


def bullet(p, mat, x, z, y0, r, h, seg):
    """a vertical bullet tank: hemispherical ends, cylinder height h between them, bottom at y0"""
    n = 6
    prof = [(0.0, y0)]
    prof += [(r * math.sin(0.5 * math.pi * k / n), y0 + r - r * math.cos(0.5 * math.pi * k / n)) for k in range(1, n + 1)]
    prof += [(r * math.cos(0.5 * math.pi * k / n), y0 + r + h + r * math.sin(0.5 * math.pi * k / n)) for k in range(0, n)]
    prof += [(0.0, y0 + 2 * r + h)]
    ylathe(p, mat, prof, seg, t=(x, 0, z), soft=40)


def sphere(p, mat, c, r, seg):
    n = max(6, seg // 2)
    prof = [(r * math.sin(math.pi * k / n), c[1] - r * math.cos(math.pi * k / n)) for k in range(0, n + 1)]
    prof[0], prof[-1] = (0.0, c[1] - r), (0.0, c[1] + r)
    ylathe(p, mat, prof, seg, t=(c[0], 0, c[2]), soft=40)


def gallery(p, x, z, y, r, seg):
    """a ring walkway around a tank (grating annulus + rail)"""
    ylathe(p, GRATE, [(r + 0.4, y - 0.6), (r + 3.0, y - 0.6), (r + 3.0, y), (r + 0.4, y), (r + 0.4, y - 0.6)], seg,
           t=(x, 0, z))
    ylathe(p, GRATE, [(r + 2.8, y + 1.1), (r + 3.0, y + 1.1), (r + 3.0, y + 1.3), (r + 2.8, y + 1.3), (r + 2.8, y + 1.1)],
           seg, t=(x, 0, z))


# ---------------------------------------------------------------- gas battery: 2 x 3 vertical bullets on one frame
GAS_R, GAS_H, GAS_DX, GAS_DZ, GAS_Y0 = 18.0, 120.0, 46.0, 46.0, 10.0
GAS_TOP = GAS_Y0 + 2 * GAS_R + GAS_H                  # 166 m


def gas_battery_parts(lod):
    P = {"hubb_gas_battery": Part("hubb_gas_battery")}
    g = P["hubb_gas_battery"]
    seg = _seg(lod)
    xs, zs = (-GAS_DX / 2, GAS_DX / 2), (-GAS_DZ, 0.0, GAS_DZ)
    # the mounting frame: a deck plate with edge girders
    box(g, STEEL, (0, 3.0, 0), (GAS_DX + 50.0, 6.0, 2 * GAS_DZ + 50.0))
    if lod < 3:
        for sx in (-1, 1):
            box(g, HULL, (sx * (GAS_DX / 2 + 24.0), 7.5, 0), (3.0, 3.0, 2 * GAS_DZ + 50.0))
    for x in xs:
        for z in zs:
            bullet(g, TANK_W, x, z, GAS_Y0, GAS_R, GAS_H, seg)
            cyl(g, STEEL, (x, 6.0, z), (x, GAS_Y0 + 4.0, z), GAS_R * 0.55, max(8, seg // 2))     # skirt
            if lod < 2:
                for y in (GAS_Y0 + GAS_R + 30.0, GAS_Y0 + GAS_R + 70.0, GAS_Y0 + GAS_R + GAS_H - 6.0):
                    gallery(g, x, z, y, GAS_R, seg)
                box(g, HAZ, (x, GAS_Y0 + GAS_R + 4.0, z + GAS_R + 0.05), (8.0, 4.0, 0.3))         # hazard plate
                # a riser up the tank side to the top manifold
                cyl(g, STEEL, (x + GAS_R + 2.0, 8.0, z), (x + GAS_R + 2.0, GAS_TOP - 8.0, z), 1.0, 8)
            if lod < 3:
                beacon(g, (x, GAS_TOP + 1.0, z), 1.6)
    # manifolds: along each row at the top and at the bottom, cross headers between the rows
    for x in xs:
        cyl(g, STEEL, (x + GAS_R + 2.0, GAS_TOP - 8.0, -GAS_DZ - 6.0), (x + GAS_R + 2.0, GAS_TOP - 8.0, GAS_DZ + 6.0),
            1.8, 10 if lod < 2 else 6)
        cyl(g, STEEL, (x, 8.5, -GAS_DZ - 20.0), (x, 8.5, GAS_DZ + 20.0), 2.6, 10 if lod < 2 else 6)
    cyl(g, STEEL, (-GAS_DX / 2, 8.5, GAS_DZ + 20.0), (GAS_DX / 2, 8.5, GAS_DZ + 20.0), 2.6, 10 if lod < 2 else 6)
    cyl(g, STEEL, (0, 8.5, GAS_DZ + 20.0), (0, 8.5, GAS_DZ + 30.0), 3.2, 10 if lod < 2 else 6)      # the outlet
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(g, FLOOD, (sx * (GAS_DX / 2 + 22.0), 10.0, sz * (GAS_DZ + 22.0)), (2.0, 1.5, 2.0))
    return P


def gas_battery_hits():
    return [hbox("hubb_gas_battery", (0, 3.0, 0), (GAS_DX + 50.0, 6.0, 2 * GAS_DZ + 50.0))] + [
        hcyl("hubb_gas_battery", (x, GAS_Y0, z), (x, GAS_TOP, z), GAS_R + 3.0)
        for x in (-GAS_DX / 2, GAS_DX / 2) for z in (-GAS_DZ, 0.0, GAS_DZ)]


# ---------------------------------------------------------------- fuel spheres: 4 Horton spheres in leg cages
FS_R, FS_D, FS_C = 32.0, 80.0, 48.0                  # radius, spacing, centre height


def fuel_spheres_parts(lod):
    P = {"hubb_fuel_spheres": Part("hubb_fuel_spheres")}
    f = P["hubb_fuel_spheres"]
    seg = _seg(lod, 32)
    box(f, STEEL, (0, 3.0, 0), (2 * FS_D + 20.0, 6.0, 2 * FS_D + 20.0))
    cs = [(sx * FS_D / 2, sz * FS_D / 2) for sx in (-1, 1) for sz in (-1, 1)]
    for x, z in cs:
        sphere(f, TANK_F, (x, FS_C, z), FS_R, seg)
        legs = 8 if lod < 2 else 4
        for k in range(legs):
            a = 2 * math.pi * k / legs
            px, pz = x + (FS_R + 1.5) * math.cos(a), z + (FS_R + 1.5) * math.sin(a)
            cyl(f, STEEL, (px, 6.0, pz), (px, FS_C + 2.0, pz), 1.4, 8 if lod < 2 else 6)
        if lod < 2:
            gallery(f, x, z, FS_C, FS_R, seg)
            ylathe(f, GRATE, [(0.0, FS_C + FS_R - 0.2), (6.0, FS_C + FS_R - 0.2), (6.0, FS_C + FS_R + 0.6),
                              (0.0, FS_C + FS_R + 0.6)], 12, t=(x, 0, z))                         # top platform
        if lod < 3:
            beacon(f, (x, FS_C + FS_R + 2.0, z), 1.5)
        # bottom outlet to the ring main
        cyl(f, STEEL, (x, FS_C - FS_R, z), (x, 10.0, z), 1.6, 8)
    # the ring main around the four spheres + a header out along +Z
    ring = [(-FS_D / 2, -FS_D / 2), (FS_D / 2, -FS_D / 2), (FS_D / 2, FS_D / 2), (-FS_D / 2, FS_D / 2), (-FS_D / 2, -FS_D / 2)]
    tube(f, STEEL, [(x, 10.0, z) for x, z in ring], 2.2, 10 if lod < 2 else 6, caps=False)
    cyl(f, STEEL, (0, 10.0, FS_D / 2), (0, 10.0, FS_D + 10.0), 3.0, 10 if lod < 2 else 6)
    if lod < 2:
        box(f, HULL, (FS_D / 2 + 14.0, 14.0, 0), (16.0, 16.0, 30.0))                              # pump house
        box(f, HAZ, (FS_D / 2 + 22.05, 14.0, 0), (0.3, 4.0, 20.0))
    return P


def fuel_spheres_hits():
    return [hbox("hubb_fuel_spheres", (0, 3.0, 0), (2 * FS_D + 20.0, 6.0, 2 * FS_D + 20.0))] + [
        hcyl("hubb_fuel_spheres", (x, FS_C - FS_R, z), (x, FS_C + FS_R, z), FS_R + 3.0)
        for x in (-FS_D / 2, FS_D / 2) for z in (-FS_D / 2, FS_D / 2)]



# ================================================================ the rest of the settlement kit
# Ship classes the docks are sized for (src/assets/data/ships/ship_catalog_3126.json envelope_m L x W x H):
# T-5000 220x50x40 (the largest), He-T12 jump train 160x40x40, GT-3000 110x30x30, HE-120 98x32x28, HT-20K "Мул" ore tug
# 60x25x25 (+ its raft), OCS-4 "Ласточка" 56x18x14. Berths: L 260 x 64 m clear (T-5000 + 20 m / 7 m margins), M 140 x 44.
from units.hub_mega import FACADE, FACADE_L  # noqa: E402  (the window facade materials)
from units.hub_district import COOL, DARK, WARM  # noqa: E402,F401

register_materials({
    "eq_hub_road": ((0.09, 0.09, 0.10), (0.05, 0.05, 0.05), 10),           # roadway
    "eq_hub_marking": ((0.85, 0.80, 0.55), (0.10, 0.10, 0.10), 10),        # lane / pad markings
    "eq_hub_container_a": ((0.55, 0.18, 0.12), (0.10, 0.10, 0.10), 20),
    "eq_hub_container_b": ((0.16, 0.30, 0.45), (0.10, 0.10, 0.10), 20),
    "eq_hub_container_c": ((0.45, 0.42, 0.16), (0.10, 0.10, 0.10), 20),
    "eq_hub_radiator": ((0.30, 0.20, 0.16), (0.40, 0.40, 0.40), 60),       # heat radiator fins
})
ROAD, MARK, RAD = "eq_hub_road", "eq_hub_marking", "eq_hub_radiator"
CONT = ("eq_hub_container_a", "eq_hub_container_b", "eq_hub_container_c")


def lamp(p, x, y, z, h=8.0):
    """a lamp post standing on y, light head at the top"""
    box(p, STEEL, (x, y + h / 2, z), (0.5, h, 0.5))
    box(p, FLOOD, (x, y + h + 0.4, z), (1.6, 0.6, 1.6))


def road_strip(p, x0, x1, y, z0, z1, lod, dash=12.0):
    """a roadway on a deck top y: dark strip, edge lines and a dashed centre line (along Z)"""
    box(p, ROAD, ((x0 + x1) / 2, y + 0.05, (z0 + z1) / 2), (x1 - x0, 0.1, z1 - z0))
    if lod < 2:
        for x in (x0 + 0.6, x1 - 0.6):
            box(p, MARK, (x, y + 0.12, (z0 + z1) / 2), (0.3, 0.05, z1 - z0))
        z = z0 + dash / 2
        while z < z1 - dash / 2:
            box(p, MARK, ((x0 + x1) / 2, y + 0.12, z), (0.3, 0.05, dash * 0.5))
            z += dash


# ---------------------------------------------------------------- docks as railway platforms
def pier_parts(lod, name, L, berth_w):
    """a platform pier: deck 40 m wide along +Z from the root (z 0) to L, deck top y 0; berths on both sides (+-X), the
    ship lies alongside; a two-lane road down the middle (the wheeled access), walkways on the edges, portal cranes
    spanning out over each berth, berth guide pylons at the far edge of each berth, clamp arms."""
    P = {name: Part(name)}
    d = P[name]
    W, T = 40.0, 10.0
    box(d, HULL, (0, -T / 2, L / 2), (W, T, L))
    box(d, STEEL, (0, -T - 3.0, L / 2), (W * 0.5, 6.0, L))                          # keel girder
    road_strip(d, -7.0, 7.0, 0.0, 0.0, L, lod)
    for sx in (-1, 1):
        box(d, GRATE, (sx * (W / 2 - 3.0), 0.3, L / 2), (5.0, 0.6, L))                 # platform walkway
        box(d, HAZ, (sx * (W / 2 - 0.4), 0.2, L / 2), (0.8, 0.4, L))                  # edge stripe
        # the berth: guide pylons along its far edge, a bumper rail at the platform edge
        xb = sx * (W / 2 + berth_w)
        n = int(L // 60) + 1
        for k in range(n):
            z = 10.0 + k * (L - 20.0) / max(1, n - 1)
            box(d, STEEL, (xb, 0.0, z), (3.0, 16.0, 3.0))
            box(d, STEEL, (sx * (W / 2 + berth_w / 2), -12.0, z), (berth_w, 2.0, 2.0))      # the boom out to the pylon
            if lod < 3:
                beacon(d, (xb, 8.8, z), 1.2)
            if lod < 2:
                cyl(d, STEEL, (sx * W / 2, -2.0, z), (sx * (W / 2 + 6.0), -2.0, z), 0.8, 6)  # clamp arm
                box(d, HAZ, (sx * (W / 2 + 6.4), -2.0, z), (1.0, 3.0, 3.0))
        if lod < 3:
            for k in range(max(1, int(L // 120))):                                   # portal cranes over the berth
                z = 60.0 + k * 120.0
                if z > L - 30.0:
                    break
                box(d, STEEL, (sx * (W / 2 - 2.0), 22.0, z), (2.5, 44.0, 2.5))
                box(d, STEEL, (sx * (W / 2 + berth_w * 0.5 - 4.0), 44.5, z), (berth_w + 8.0, 3.0, 3.0))
                if lod < 2:
                    box(d, LIGHT, (sx * (W / 2 + berth_w * 0.6), 41.0, z), (6.0, 4.0, 5.0))     # trolley
                    cyl(d, STEEL, (sx * (W / 2 + berth_w * 0.6), 39.0, z), (sx * (W / 2 + berth_w * 0.6), 26.0, z), 0.3, 4)
    if lod < 2:
        for z in range(15, int(L), 30):
            for sx in (-1, 1):
                lamp(d, sx * 8.5, 0.0, float(z))
    beacon(d, (0, 2.0, L + 0.6), 2.0)
    return P


def pier_hits(name, L):
    return [hbox(name, (0, -8.0, L / 2), (40.0, 16.0, L))]


def fuel_dock_parts(lod):
    """a refuelling pier: an M platform (140 m) with fuel arms over both berths and two feed bullets at the root"""
    P = pier_parts(lod, "hubb_fuel_dock", 160.0, 44.0)
    d = P["hubb_fuel_dock"]
    seg = _seg(lod, 16)
    for sx in (-1, 1):
        bullet(d, TANK_F, sx * 12.0, -18.0, 0.0, 7.0, 24.0, seg)
        for z in (50.0, 100.0):
            cyl(d, STEEL, (sx * 16.0, 0.0, z), (sx * 16.0, 14.0, z), 1.2, 8)
            cyl(d, STEEL, (sx * 16.0, 14.0, z), (sx * 34.0, 8.0, z), 0.9, 8)              # the hose boom
            box(d, HAZ, (sx * 34.0, 7.0, z), (2.0, 2.0, 2.0))
    cyl(d, STEEL, (0, -4.0, -18.0), (0, -4.0, 160.0), 1.4, 8)                            # the fuel main under the deck
    return P


# ---------------------------------------------------------------- repair dock: an open cage with gantry cranes
RD_L, RD_W, RD_H = 260.0, 90.0, 80.0


def repair_dock_parts(lod):
    P = {"hubb_repair_dock": Part("hubb_repair_dock")}
    r = P["hubb_repair_dock"]
    box(r, HULL, (0, -4.0, 0), (RD_W + 20.0, 8.0, RD_L))                                # the floor slab (+ shops)
    for sx in (-1, 1):
        for z in (-RD_L / 2 + 4.0, -RD_L / 4, 0.0, RD_L / 4, RD_L / 2 - 4.0):
            box(r, STEEL, (sx * RD_W / 2, RD_H / 2, z), (6.0, RD_H, 6.0))               # cage columns
        box(r, STEEL, (sx * RD_W / 2, RD_H + 2.0, 0), (6.0, 4.0, RD_L))                 # crane rails
        box(r, HULL, (sx * (RD_W / 2 + 6.0), 12.0, 0), (6.0, 24.0, RD_L * 0.8))         # side workshops
        if lod < 2:
            for z in range(-110, 120, 40):
                box(r, GRATE, (sx * (RD_W / 2 - 6.0), 30.0, float(z)), (10.0, 1.0, 30.0))   # work galleries
                box(r, GRATE, (sx * (RD_W / 2 - 6.0), 55.0, float(z)), (10.0, 1.0, 30.0))
                box(r, FLOOD, (sx * (RD_W / 2 - 2.0), RD_H - 6.0, float(z)), (2.0, 2.0, 2.0))
            for z in range(-100, 110, 25):
                box(r, WARM, (sx * (RD_W / 2 + 9.05), 14.0, float(z)), (0.2, 3.0, 12.0))  # shop windows
    for z in (-RD_L / 2 + 4.0, RD_L / 2 - 4.0):
        box(r, STEEL, (0, RD_H + 2.0, z), (RD_W + 6.0, 4.0, 6.0))
    for z in (-60.0, 50.0):                                                             # two gantry cranes
        box(r, LIGHT, (0, RD_H + 6.0, z), (RD_W + 4.0, 5.0, 8.0))
        if lod < 3:
            box(r, HAZ, (0, RD_H + 2.0, z), (8.0, 4.0, 8.0))
            cyl(r, STEEL, (0, RD_H, z), (0, RD_H - 30.0, z), 0.5, 4)
    if lod < 3:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(r, (sx * RD_W / 2, RD_H + 5.0, sz * (RD_L / 2 - 4.0)), 1.5)
    road_strip(r, -6.0, 6.0, 0.0, -RD_L / 2, RD_L / 2, lod)
    return P


def repair_dock_hits():
    return ([hbox("hubb_repair_dock", (0, -4.0, 0), (RD_W + 20.0, 8.0, RD_L))] +
            [hbox("hubb_repair_dock", (sx * (RD_W / 2 + 3.0), RD_H / 2, 0), (12.0, RD_H + 8.0, RD_L)) for sx in (-1, 1)])


# ---------------------------------------------------------------- parking: a plate of 8 pads 90 x 60
PK_X, PK_Z = 2, 4


def parking_parts(lod):
    P = {"hubb_parking": Part("hubb_parking")}
    k = P["hubb_parking"]
    W, D = PK_X * 100.0 + 40.0, PK_Z * 70.0 + 20.0
    box(k, HULL, (0, -5.0, 0), (W, 10.0, D))
    road_strip(k, -8.0, 8.0, 0.0, -D / 2, D / 2, lod)
    for ix in range(PK_X):
        for iz in range(PK_Z):
            x = (ix - (PK_X - 1) / 2) * 100.0 + (20.0 if ix else -20.0)
            z = (iz - (PK_Z - 1) / 2) * 70.0
            box(k, LIGHT, (x, 0.1, z), (88.0, 0.2, 58.0))
            if lod < 2:
                for sx in (-1, 1):
                    box(k, MARK, (x + sx * 42.0, 0.25, z), (1.0, 0.1, 54.0))
                for sz in (-1, 1):
                    box(k, MARK, (x, 0.25, z + sz * 27.0), (84.0, 0.1, 1.0))
                box(k, MARK, (x, 0.25, z), (20.0, 0.1, 1.5))
            if lod < 3:
                for sx in (-1, 1):
                    for sz in (-1, 1):
                        box(k, FLOOD if lod < 2 else BEACON, (x + sx * 44.0, 0.6, z + sz * 29.0), (1.0, 1.0, 1.0))
    if lod < 2:
        box(k, HULL, (W / 2 - 15.0, 12.0, -D / 2 + 20.0), (20.0, 24.0, 20.0))            # the control post
        box(k, WARM, (W / 2 - 15.0, 20.0, -D / 2 + 9.9), (16.0, 4.0, 0.2))
    return P


def parking_hits():
    return [hbox("hubb_parking", (0, -5.0, 0), (PK_X * 100.0 + 40.0, 10.0, PK_Z * 70.0 + 20.0))]


# ---------------------------------------------------------------- warehouse: a big hall, bay doors, container yard
WH_L, WH_W, WH_H = 240.0, 120.0, 50.0


def warehouse_parts(lod):
    P = {"hubb_warehouse": Part("hubb_warehouse")}
    w = P["hubb_warehouse"]
    box(w, HULL, (0, -3.0, 0), (WH_W + 60.0, 6.0, WH_L + 20.0))                         # the apron plate
    box(w, LIGHT, (-15.0, WH_H / 2, 0), (WH_W - 30.0, WH_H, WH_L))
    if lod < 3:
        for z in range(-110, 120, 20):                                                  # roof ribs
            box(w, STEEL, (-15.0, WH_H + 1.0, float(z)), (WH_W - 28.0, 2.0, 2.0))
    for z in (-90.0, -30.0, 30.0, 90.0):                                                # bay doors on +X
        box(w, DARK, (WH_W / 2 - 29.9, 16.0, z), (0.4, 28.0, 40.0))
        if lod < 2:
            box(w, HAZ, (WH_W / 2 - 29.7, 31.0, z), (0.4, 2.0, 40.0))
            box(w, FLOOD, (WH_W / 2 - 29.0, 33.0, z), (1.5, 1.0, 3.0))
    road_strip(w, WH_W / 2 - 25.0, WH_W / 2 - 5.0, 0.0, -WH_L / 2, WH_L / 2, lod)
    # container racks in the yard (+X of the road): 12 x 2.6 x 2.6 boxes, 3 high
    if lod < 3:
        rows = 4 if lod < 2 else 2
        for i in range(rows):
            x = WH_W / 2 + 2.0 + i * 6.0
            for j in range(16):
                z = -WH_L / 2 + 10.0 + j * 14.0
                h = 1 + (i * 7 + j * 3) % 3
                for k in range(h if lod < 2 else 1):
                    box(w, CONT[(i + j + k) % 3], (x, 1.4 + k * 2.7, z), (2.5, 2.6, 12.0))
    if lod < 3:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(w, (-15.0 + sx * (WH_W / 2 - 15.0), WH_H + 2.0, sz * WH_L / 2), 1.2)
    return P


def warehouse_hits():
    return [hbox("hubb_warehouse", (0, -3.0, 0), (WH_W + 60.0, 6.0, WH_L + 20.0)),
            hbox("hubb_warehouse", (-15.0, WH_H / 2, 0), (WH_W - 30.0, WH_H, WH_L))]


# ---------------------------------------------------------------- works: an industrial hall, furnaces, radiators
WK_L, WK_W, WK_H = 300.0, 140.0, 70.0
WORKS_VENTS = [(-40.0, WK_H + 60.0, -100.0), (-40.0, WK_H + 60.0, 0.0), (-40.0, WK_H + 60.0, 100.0)]


def works_parts(lod):
    P = {"hubb_works": Part("hubb_works")}
    w = P["hubb_works"]
    seg = _seg(lod, 20)
    box(w, STEEL, (0, -4.0, 0), (WK_W + 80.0, 8.0, WK_L + 40.0))
    box(w, HULL, (0, WK_H / 2, 0), (WK_W, WK_H, WK_L))
    box(w, LIGHT, (WK_W / 4, WK_H + 10.0, 0), (WK_W / 2, 20.0, WK_L * 0.9))             # the upper hall
    for z in (-100.0, 0.0, 100.0):                                                      # furnaces with flues
        cyl(w, STEEL, (-40.0, WK_H, z), (-40.0, WK_H + 50.0, z), 14.0, seg)
        cyl(w, HULL, (-40.0, WK_H + 50.0, z), (-40.0, WK_H + 60.0, z), 8.0, seg)
        if lod < 3:
            beacon(w, (-40.0, WK_H + 61.0, z), 1.5)
    # radiator wings standing out of both long sides (a factory in vacuum sheds heat by radiation)
    for sx in (-1, 1):
        for k in range(5 if lod < 2 else 3):
            z = -120.0 + k * (240.0 / (4 if lod < 2 else 2))
            box(w, RAD, (sx * (WK_W / 2 + 40.0), WK_H * 0.6, z), (80.0, 0.8, 36.0))
            box(w, STEEL, (sx * (WK_W / 2 + 4.0), WK_H * 0.6, z), (8.0, 3.0, 3.0))
    if lod < 2:                                                                         # pipe racks on the roof
        for x in (-10.0, -4.0, 2.0):
            cyl(w, STEEL, (x, WK_H + 3.0, -WK_L / 2), (x, WK_H + 3.0, WK_L / 2), 1.5, 8)
        for z in range(-140, 150, 20):
            box(w, WARM if (z // 20) % 3 else COOL, (WK_W / 2 + 0.05, 8.0, float(z)), (0.2, 3.0, 10.0))
    road_strip(w, -WK_W / 2 - 30.0, -WK_W / 2 - 10.0, 0.0, -WK_L / 2, WK_L / 2, lod)
    return P


def works_hits():
    return [hbox("hubb_works", (0, -4.0, 0), (WK_W + 80.0, 8.0, WK_L + 40.0)),
            hbox("hubb_works", (0, (WK_H + 20.0) / 2, 0), (WK_W, WK_H + 20.0, WK_L))] + [
        hcyl("hubb_works", (-40.0, WK_H, z), (-40.0, WK_H + 60.0, z), 14.0) for z in (-100.0, 0.0, 100.0)]


# ---------------------------------------------------------------- habitats: static tower / slab, rotating drum
def hab_tower_parts(lod):
    P = {"hubb_hab_tower": Part("hubb_hab_tower")}
    h = P["hubb_hab_tower"]
    box(h, STEEL, (0, -4.0, 0), (110.0, 8.0, 110.0))
    box(h, FACADE, (0, 90.0, 0), (80.0, 180.0, 80.0))
    box(h, FACADE_L, (0, 210.0, 0), (60.0, 60.0, 60.0))
    box(h, HULL, (0, 242.0, 0), (66.0, 4.0, 66.0))
    for y in (60.0, 120.0, 180.0):
        box(h, HULL, (0, y, 0), (86.0, 3.0, 86.0))
    cyl(h, STEEL, (0, 244.0, 0), (0, 300.0, 0), 1.5, 6)
    beacon(h, (0, 301.0, 0), 2.0)
    return P


def hab_tower_hits():
    return [hbox("hubb_hab_tower", (0, 90.0, 0), (86.0, 180.0, 86.0)), hbox("hubb_hab_tower", (0, 211.0, 0), (66.0, 62.0, 66.0))]


def hab_slab_parts(lod):
    P = {"hubb_hab_slab": Part("hubb_hab_slab")}
    h = P["hubb_hab_slab"]
    box(h, STEEL, (0, -3.0, 0), (230.0, 6.0, 70.0))
    box(h, FACADE, (0, 36.0, 0), (220.0, 72.0, 50.0))
    box(h, FACADE_L, (-40.0, 84.0, 0), (100.0, 24.0, 40.0))
    if lod < 2:
        for x in (-80.0, 0.0, 80.0):
            box(h, LIGHT, (x, 74.0, 0), (20.0, 4.0, 20.0))
    if lod < 3:
        beacon(h, (-90.0, 97.0, 0), 1.5)
    return P


def hab_slab_hits():
    return [hbox("hubb_hab_slab", (0, 40.0, 0), (220.0, 80.0, 50.0))]


DRUM_R, DRUM_L = 150.0, 500.0


def hab_drum_parts(lod):
    """a rotating habitat drum: the cylinder spins about its axis (+Z), people live on its inner wall (spin gravity
    0.35 g at 150 m and 1.5 rpm); windows on the hull, rib rings, the non-rotating axle docks at both ends"""
    P = {"hubb_hab_drum": Part("hubb_hab_drum")}
    d = P["hubb_hab_drum"]
    seg = _seg(lod, 40)
    lathe(d, FACADE, [(0.0, -DRUM_L / 2), (DRUM_R * 0.92, -DRUM_L / 2), (DRUM_R, -DRUM_L / 2 + 15.0),
                      (DRUM_R, DRUM_L / 2 - 15.0), (DRUM_R * 0.92, DRUM_L / 2), (0.0, DRUM_L / 2)], seg)
    for z in (-DRUM_L / 2 + 60.0, -DRUM_L / 6, DRUM_L / 6, DRUM_L / 2 - 60.0):          # rib rings
        lathe(d, LIGHT, [(DRUM_R - 1.0, z - 4.0), (DRUM_R + 5.0, z - 4.0), (DRUM_R + 5.0, z + 4.0), (DRUM_R - 1.0, z + 4.0),
                         (DRUM_R - 1.0, z - 4.0)], seg)
    for sz in (-1, 1):                                                                  # axle ports
        cyl(d, HULL, (0, 0, sz * DRUM_L / 2), (0, 0, sz * (DRUM_L / 2 + 40.0)), 24.0, max(8, seg // 3))
        box(d, DARK, (0, 0, sz * (DRUM_L / 2 + 40.2)), (20.0, 20.0, 0.3))
        if lod < 3:
            for k in range(4):
                a = math.pi / 2 * k
                beacon(d, (30.0 * math.cos(a), 30.0 * math.sin(a), sz * (DRUM_L / 2 + 2.0)), 2.0)
    return P


def hab_drum_hits():
    return [hcyl("hubb_hab_drum", (0, 0, -DRUM_L / 2), (0, 0, DRUM_L / 2), DRUM_R + 5.0),
            hcyl("hubb_hab_drum", (0, 0, -DRUM_L / 2 - 40.0), (0, 0, DRUM_L / 2 + 40.0), 24.0)]


# ---------------------------------------------------------------- hotel: a rotating hexagonal barrel
HX_A, HX_L = 90.0, 260.0                                                                # apothem, length


def hotel_hex_parts(lod):
    """a hotel dock: a rotating hexagonal barrel (like hub_hex_dock_a, smaller) - windowed sides, a dock mouth in the
    front face (+Z), a lit ring around it; spins about +Z"""
    P = {"hubb_hotel_hex": Part("hubb_hotel_hex")}
    h = P["hubb_hotel_hex"]
    R = HX_A / math.cos(math.pi / 6)
    pts = [(R * math.cos(math.pi / 3 * k), R * math.sin(math.pi / 3 * k)) for k in range(6)]
    for k in range(6):                                                                  # six windowed side walls
        a = math.pi / 3 * k + math.pi / 6
        c = (HX_A * math.cos(a), HX_A * math.sin(a), 0.0)
        box(h, FACADE, (c[0] - 3.0 * math.cos(a), c[1] - 3.0 * math.sin(a), 0.0), (6.0, R, HX_L), rot_z(a))
    for sz in (-1, 1):                                                                  # end plates
        for k in range(6):
            a = math.pi / 3 * k + math.pi / 6
            box(h, HULL, ((HX_A - 20.0) * math.cos(a), (HX_A - 20.0) * math.sin(a), sz * (HX_L / 2 - 3.0)),
                (40.0, R, 6.0), rot_z(a))
    box(h, DARK, (0, 0, HX_L / 2 + 0.2), (80.0, 40.0, 0.4))                              # the dock mouth
    if lod < 2:
        for k in range(12):
            a = 2 * math.pi * k / 12
            box(h, FLOOD, (52.0 * math.cos(a), 30.0 * math.sin(a), HX_L / 2 + 0.6), (3.0, 3.0, 0.6))
    cyl(h, HULL, (0, 0, -HX_L / 2), (0, 0, -HX_L / 2 - 40.0), 18.0, 12)                 # axle to the fixed base
    if lod < 3:
        for x, y in pts:
            beacon(h, (x, y, HX_L / 2), 2.0)
    return P


def hotel_hex_hits():
    return [hhex("hubb_hotel_hex", (0, 0, 0), (HX_A / math.cos(math.pi / 6), HX_A, HX_L / 2), HX_A / math.cos(math.pi / 6) / 2,
                 (0.0, 0.0, 0.0))]


# ---------------------------------------------------------------- the central rotating ring
RING_R, RING_W, RING_T, RING_N = 900.0, 140.0, 70.0, 32                                 # radius, width (Z), depth, segments


def ring_core_parts(lod):
    """the settlement's centre: a ring 1.8 km across spinning about +Z (0.3 g at 900 m and 1 rpm), its rim a chain of
    windowed habitat blocks with the "floor" on the inside; four spokes to a fixed-looking hub with the central
    dock on the axis (the hub spins with it in this model)"""
    P = {"hubb_ring_core": Part("hubb_ring_core")}
    r = P["hubb_ring_core"]
    n = RING_N if lod < 2 else RING_N // 2
    for k in range(n):
        a = 2 * math.pi * k / n
        ca, sa = math.cos(a), math.sin(a)
        L = 2 * RING_R * math.tan(math.pi / n) + 2.0
        box(r, FACADE, (RING_R * ca, RING_R * sa, 0.0), (RING_T, L, RING_W), rot_z(a))
        if lod < 3:
            box(r, LIGHT, ((RING_R + RING_T / 2 + 3.0) * ca, (RING_R + RING_T / 2 + 3.0) * sa, 0.0), (6.0, L * 0.3, RING_W + 10.0),
                rot_z(a))                                                               # outer rib
        if lod < 2 and k % 2 == 0:
            for sz in (-1, 1):
                beacon(r, ((RING_R + RING_T / 2 + 6.0) * ca, (RING_R + RING_T / 2 + 6.0) * sa, sz * (RING_W / 2 + 5.0)), 3.0)
    for k in range(4):                                                                  # spokes (lift shafts)
        a = math.pi / 2 * k + math.pi / 4
        p0 = (100.0 * math.cos(a), 100.0 * math.sin(a), 0.0)
        p1 = ((RING_R - RING_T / 2) * math.cos(a), (RING_R - RING_T / 2) * math.sin(a), 0.0)
        cyl(r, HULL, p0, p1, 14.0, 12 if lod < 2 else 8)
        if lod < 2:
            for t in (0.3, 0.6):
                c = tuple(p0[i] + (p1[i] - p0[i]) * t for i in range(3))
                cyl(r, LIGHT, tuple(c[i] - (p1[i] - p0[i]) * 0.01 for i in range(3)),
                    tuple(c[i] + (p1[i] - p0[i]) * 0.01 for i in range(3)), 18.0, 12)
    lathe(r, HULL, [(0.0, -160.0), (100.0, -160.0), (110.0, -120.0), (110.0, 120.0), (100.0, 160.0), (0.0, 160.0)],
          _seg(lod, 32))                                                                # the hub
    box(r, DARK, (0, 0, 160.2), (90.0, 40.0, 0.4))                                      # the central dock mouth
    if lod < 2:
        for k in range(8):
            a = 2 * math.pi * k / 8
            box(r, FLOOD, (70.0 * math.cos(a), 70.0 * math.sin(a), 160.6), (4.0, 4.0, 0.6))
    return P


def ring_core_hits():
    out = []
    for k in range(RING_N):
        a = 2 * math.pi * k / RING_N
        L = 2 * RING_R * math.tan(math.pi / RING_N) + 2.0
        out.append(hbox("hubb_ring_core", (RING_R * math.cos(a), RING_R * math.sin(a), 0.0), (RING_T, L, RING_W),
                        (0.0, 0.0, math.degrees(a))))
    out.append(hcyl("hubb_ring_core", (0, 0, -160.0), (0, 0, 160.0), 110.0))
    return out


# ---------------------------------------------------------------- the heavy girder: 30 x 30 m lattice, 100 m along +Z
GIRDER_H, SEG_L = 15.0, 100.0


def girder_parts(lod):
    P = {"hubb_girder": Part("hubb_girder")}
    g = P["hubb_girder"]
    h = GIRDER_H
    cs = {0: 8, 1: 6}.get(lod, 4)
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(g, STEEL, (sx * h, sy * h, SEG_L / 2), (2.4, 2.4, SEG_L))                  # chords
    bays = 4 if lod < 2 else 2
    for k in range(bays + 1):
        z = SEG_L * k / bays
        for a, b in (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h))):
            cyl(g, STEEL, (a[0], a[1], z), (b[0], b[1], z), 0.9, cs)
        if k < bays and lod < 3:
            z1 = SEG_L * (k + 1) / bays
            for a, b in (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h))):
                cyl(g, STEEL, (a[0], a[1], z), (b[0], b[1], z1) if k % 2 == 0 else (a[0], a[1], z1), 0.6, cs)
    if lod < 2:
        box(g, GRATE, (0, -h + 1.5, SEG_L / 2), (8.0, 0.6, SEG_L))                        # inner walkway
        for z in (2.0, SEG_L - 2.0):
            beacon(g, (h + 1.5, h + 1.5, z), 1.2)
    return P


def girder_hits():
    return [hbox("hubb_girder", (0, 0, SEG_L / 2), (2 * GIRDER_H + 2.4, 2 * GIRDER_H + 2.4, SEG_L))]


# ---------------------------------------------------------------- connectors: tube tunnel, junction, monorail, road
TUBE_R = 11.0


def tube_parts(lod):
    """a pressurised tunnel segment along +Z, 100 m: pedestrian / service tube, rib rings every 25 m, a lit window
    strip; a road deck and a monorail beam are separate modules laid beside it"""
    P = {"hubb_tube": Part("hubb_tube")}
    t = P["hubb_tube"]
    seg = _seg(lod, 16)
    cyl(t, HULL, (0, 0, 0), (0, 0, SEG_L), TUBE_R, seg, caps=False)
    for z in (0.0, 25.0, 50.0, 75.0):
        lathe(t, STEEL, [(TUBE_R - 0.2, z), (TUBE_R + 1.5, z), (TUBE_R + 1.5, z + 2.0), (TUBE_R - 0.2, z + 2.0),
                         (TUBE_R - 0.2, z)], seg)
    if lod < 3:
        for sx in (-1, 1):
            box(t, WARM if sx > 0 else COOL, (sx * (TUBE_R + 0.05), 2.0, SEG_L / 2), (0.2, 1.2, SEG_L - 10.0))
    return P


def tube_hits():
    return [hcyl("hubb_tube", (0, 0, 0), (0, 0, SEG_L), TUBE_R + 1.5)]


def junction_parts(lod):
    """a tube junction: a sphere with six collars (+-X, +-Y, +-Z), a beacon crown"""
    P = {"hubb_junction": Part("hubb_junction")}
    j = P["hubb_junction"]
    seg = _seg(lod, 20)
    sphere(j, HULL, (0, 0, 0), 24.0, seg)
    for d in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)):
        cyl(j, STEEL, tuple(20.0 * c for c in d), tuple(30.0 * c for c in d), TUBE_R + 2.0, max(8, seg // 2))
    if lod < 3:
        for d in ((0, 1, 0), (0, -1, 0)):
            beacon(j, tuple(25.0 * c for c in d), 1.5)
    return P


def junction_hits():
    return [hbox("hubb_junction", (0, 0, 0), (48.0, 48.0, 48.0))]


def monorail_parts(lod):
    """a monorail beam segment along +Z (100 m): a box girder with the running rail on top, a hanger at mid-span"""
    P = {"hubb_monorail": Part("hubb_monorail")}
    m = P["hubb_monorail"]
    box(m, LIGHT, (0, 0, SEG_L / 2), (3.0, 4.0, SEG_L))
    box(m, STEEL, (0, 2.4, SEG_L / 2), (1.2, 0.8, SEG_L))
    if lod < 2:
        for z in (0.0, 50.0):
            box(m, STEEL, (0, -6.0, z + 1.0), (1.0, 8.0, 1.0))                            # hanger (to the tube rib)
            box(m, FLOOD, (1.6, 0.0, z + 25.0), (0.3, 0.6, 0.6))
    return P


def monorail_hits():
    return [hbox("hubb_monorail", (0, 0.4, SEG_L / 2), (3.0, 4.8, SEG_L))]


def road_parts(lod):
    """an open road deck along +Z (100 m): two lanes each way, guard rails, lamp posts - the wheeled transport between
    platforms and halls"""
    P = {"hubb_road": Part("hubb_road")}
    r = P["hubb_road"]
    box(r, HULL, (0, -1.5, SEG_L / 2), (26.0, 3.0, SEG_L))
    road_strip(r, -12.0, 0.0, 0.0, 0.0, SEG_L, lod)
    road_strip(r, 0.0, 12.0, 0.0, 0.0, SEG_L, lod)
    for sx in (-1, 1):
        box(r, STEEL, (sx * 12.7, 0.6, SEG_L / 2), (0.4, 1.2, SEG_L))
        if lod < 2:
            lamp(r, sx * 12.7, 0.0, 25.0 if sx > 0 else 75.0, 10.0)
    return P


def road_hits():
    return [hbox("hubb_road", (0, -1.5, SEG_L / 2), (26.0, 3.0, SEG_L))]


KIT = {"hubb_gas_battery": (gas_battery_parts, gas_battery_hits,
                            "Газовая батарея: 6 вертикальных баллонов 36x156 м на раме, коллекторы"),
       "hubb_fuel_spheres": (fuel_spheres_parts, fuel_spheres_hits,
                             "Склад горючего: 4 сферы 64 м на опорах, кольцевой трубопровод"),
       "hubb_pier_l": (lambda lod: pier_parts(lod, "hubb_pier_l", 300.0, 64.0), lambda: pier_hits("hubb_pier_l", 300.0),
                       "Перрон L 300 м: причалы 260x64 по обе стороны (до T-5000), дорога, краны"),
       "hubb_pier_m": (lambda lod: pier_parts(lod, "hubb_pier_m", 160.0, 44.0), lambda: pier_hits("hubb_pier_m", 160.0),
                       "Перрон M 160 м: причалы 140x44 (до HE-120 / GT-3000)"),
       "hubb_fuel_dock": (fuel_dock_parts, lambda: pier_hits("hubb_fuel_dock", 160.0),
                          "Заправочный перрон 160 м: топливные стрелы над причалами"),
       "hubb_repair_dock": (repair_dock_parts, repair_dock_hits, "Ремонтный док: каркас 260x90x80, мостовые краны"),
       "hubb_parking": (parking_parts, parking_hits, "Стоянка: плита с 8 площадками 90x60"),
       "hubb_warehouse": (warehouse_parts, warehouse_hits, "Склад: зал 240x120x50, ворота, контейнерный двор"),
       "hubb_works": (works_parts, works_hits, "Промышленный цех 300x140, печи, радиаторы"),
       "hubb_hab_tower": (hab_tower_parts, hab_tower_hits, "Жилая башня 80x240 (неподвижная)"),
       "hubb_hab_slab": (hab_slab_parts, hab_slab_hits, "Жилой корпус 220x72x50 (неподвижный)"),
       "hubb_hab_drum": (hab_drum_parts, hab_drum_hits, "Жилой барабан 300x500 (вращается вокруг оси)"),
       "hubb_hotel_hex": (hotel_hex_parts, hotel_hex_hits, "Гостиничный док: вращающаяся шестигранная бочка 180x260"),
       "hubb_ring_core": (ring_core_parts, ring_core_hits, "Центральное кольцо 1,8 км (вращается)"),
       "hubb_tube": (tube_parts, tube_hits, "Тоннель-труба 100 м"),
       "hubb_junction": (junction_parts, junction_hits, "Узел тоннелей (6 выходов)"),
       "hubb_monorail": (monorail_parts, monorail_hits, "Монорельс, балка 100 м"),
       "hubb_road": (road_parts, road_hits, "Дорожная эстакада 100 м, 4 полосы"),
       "hubb_girder": (girder_parts, girder_hits, "Ферма-хребет 30x30, 100 м")}

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Модуль поселения в поясе астероидов (tools/models/units/hub_belt.py).")
             for k, v in KIT.items()}


# ================================================================ the settlement generator
# Author 2026-11-17: «можешь сделать генератор для такого типа структур? основное - каждый хаб уникален, повторяемость -
# один и тот же хаб имеет одинаковый вид каждый раз как к нему приезжаешь».
# belt_layout(hub_id) is a pure function of the hub id (the seed is a hash of it): the same hub is always the same.
# The static settlement is ONE assembly (its nodes reference the hubb_* / hubk_* modules); the spinning parts (the ring,
# habitat drums, hotel barrels) are separate hub modules with an angular velocity (one object each - the game spins
# whole objects: InitialWorldState dock_modules angular_velocity_deg_s).
import hashlib  # noqa: E402
import random  # noqa: E402

DISTRICT_KINDS = ["docks_l", "docks_m", "refuel", "repair", "parking", "hotel", "storage", "fuel", "industry",
                  "hab_static", "hab_rot", "landmark"]
EXTRA_KINDS = ["storage", "industry", "hab_static", "hab_rot", "docks_m", "fuel", "hab_static", "industry", "parking",
               "refinery", "rig", "landmark", "refinery", "rig"]
WHEELED = {"docks_l", "docks_m", "refuel", "repair", "parking", "storage", "fuel", "industry", "refinery", "rig"}
PASSENGER = {"hotel", "hab_static", "hab_rot", "core", "landmark"}
RING_AXLE = 420.0          # the fixed axle junctions in front of / behind the ring hub


def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _len(a): return math.sqrt(_dot(a, a))
def _unit(a): return _mul(a, 1.0 / max(_len(a), 1e-9))


def _frame(z, y_hint):
    """rotation_deg of a frame whose local +Z = z and local +Y is as close to y_hint as possible"""
    import pads
    z = _unit(z)
    x = _cross(y_hint, z)
    if _len(x) < 1e-6:
        x = _cross((0.0, 1.0, 0.0) if abs(z[1]) < 0.9 else (1.0, 0.0, 0.0), z)
    x = _unit(x)
    y = _cross(z, x)
    R = [[x[0], y[0], z[0]], [x[1], y[1], z[1]], [x[2], y[2], z[2]]]
    return tuple(pads.to_euler(R))


def _rand_dir(rng):
    while True:
        v = (rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))
        if 0.1 < _len(v) <= 1.0:
            return _unit(v)


# what a district hangs on its spine: (module, pitch along the spine m, mount, spins deg/s or 0)
# mount "face": the module's +Y out of the spine side, +Z along the spine; "comb": a pier sticking out of the side
# (+Z out, berths along the spine); "axial": a spinning body on an axle out of the side (its +Z out)
DISTRICT_MODULES = {
    "docks_l": [("hubb_pier_l", 200.0, "comb", 0.0)],
    "docks_m": [("hubb_pier_m", 140.0, "comb", 0.0)],
    "refuel": [("hubb_fuel_dock", 140.0, "comb", 0.0), ("hubb_fuel_spheres", 200.0, "face", 0.0)],
    "repair": [("hubb_repair_dock", 300.0, "face", 0.0), ("hubb_parking", 320.0, "face", 0.0)],
    "parking": [("hubb_parking", 320.0, "face", 0.0), ("hubv_pad_star", 180.0, "face", 0.0)],
    "hotel": [("hubb_hotel_hex", 260.0, "axial", 4.0), ("hubb_hab_slab", 260.0, "face", 0.0), ("hubv_ring_wheel", 170.0, "face", 0.0)],
    "storage": [("hubb_warehouse", 280.0, "face", 0.0), ("hubv_cantilever", 100.0, "comb", 0.0)],
    "fuel": [("hubb_gas_battery", 170.0, "face", 0.0), ("hubb_fuel_spheres", 200.0, "face", 0.0)],
    "industry": [("hubb_works", 360.0, "face", 0.0), ("hubb_gas_battery", 170.0, "face", 0.0), ("hubb_warehouse", 280.0, "face", 0.0),
                 ("hubv_cantilever", 100.0, "comb", 0.0)],
    "hab_static": [("hubb_hab_tower", 140.0, "face", 0.0), ("hubb_hab_slab", 260.0, "face", 0.0),
                   ("hubv_gallery_tower", 140.0, "face", 0.0), ("hubv_saucer_tower", 300.0, "face", 0.0)],
    "hab_rot": [("hubb_hab_drum", 360.0, "axial", 9.0)],
    # after the reference video (hub_vista.py)
    "landmark": [("hubv_crown_spire", 170.0, "face", 0.0), ("hubv_crown_spire", 170.0, "face", 0.0), ("hubv_mast", 80.0, "face", 0.0),
                 ("hubv_water_tower", 120.0, "face", 0.0)],
    "refinery": [("hubv_refinery", 180.0, "face", 0.0), ("hubb_gas_battery", 170.0, "face", 0.0), ("hubv_mast", 80.0, "face", 0.0)],
    "rig": [("hubv_rig", 200.0, "face", 0.0), ("hubv_pad_star", 180.0, "face", 0.0), ("hubv_cage_gantry", 240.0, "face", 0.0)],
}
# the out-of-spine offset of an axial body's centre (its axle starts at the spine side)
AXIAL_OFFSET = {"hubb_hotel_hex": HX_L / 2 + 40.0 + GIRDER_H + 4.0, "hubb_hab_drum": DRUM_L / 2 + 40.0 + GIRDER_H + 4.0}
DISTRICT_TITLE = {"docks_l": "Грузовые перроны L", "docks_m": "Грузовые перроны M", "refuel": "Заправочные доки",
                  "repair": "Ремонтные доки", "parking": "Стоянки", "hotel": "Гостиничные доки", "storage": "Склады",
                  "fuel": "Склады горючего", "industry": "Промзона", "hab_static": "Жилой квартал",
                  "hab_rot": "Жилые барабаны", "landmark": "Шпили", "refinery": "НПЗ", "rig": "Платформы"}


def belt_layout(hub_id):
    """-> dict(nodes, spinning, haze, module_of, districts): the settlement of one hub, a pure function of hub_id"""
    seed = int(hashlib.sha1(hub_id.encode("utf-8")).hexdigest()[:12], 16)
    rng = random.Random(seed)
    nodes, spinning, haze, module_of, districts = [], [], [], {}, []
    add = lambda nid, folder, pos, rot, role, group: (nodes.append((nid, folder, tuple(round(c, 3) + 0.0 for c in pos),
                                                                    tuple(round(c, 4) + 0.0 for c in rot), role,
                                                                    None if not nodes else "core_f")),
                                                      module_of.__setitem__(nid, group))
    # the centre: the ring spins (a separate object); fixed axle junctions in front of and behind its hub
    add("core_f", "hubb_junction", (0.0, 0.0, RING_AXLE), (0.0, 0.0, 0.0), "junction", "core")
    add("core_b", "hubb_junction", (0.0, 0.0, -RING_AXLE), (0.0, 0.0, 0.0), "junction", "core")
    for k in range(2):
        z0 = 160.0 + 2.0 if k == 0 else -RING_AXLE + 30.0
        add(f"core_axle_{k}", "hubb_tube", (0.0, 0.0, z0), (0.0, 0.0, 0.0), "tube", "core")
    spinning.append({"id": "ring", "module": "hubb_ring_core", "position": (0.0, 0.0, 0.0), "rotation_deg": (0.0, 0.0, 0.0),
                     "spin_axis_local": (0.0, 0.0, 1.0), "rate_deg_s": 6.0, "title": "Центральное кольцо"})

    # districts in a shell around the ring; docks further out (approach from outside)
    n = rng.randint(14, 19)
    kinds = DISTRICT_KINDS + [rng.choice(EXTRA_KINDS) for _ in range(n - len(DISTRICT_KINDS))]
    rng.shuffle(kinds)
    centres = []
    for kind in kinds:
        lo, hi = (5200.0, 8200.0) if kind.startswith("docks") or kind in ("refuel", "repair", "parking") else (2600.0, 6800.0)
        for _ in range(400):
            c = _mul(_rand_dir(rng), rng.uniform(lo, hi))
            if abs(c[2]) < 900.0 and _len((c[0], c[1], 0.0)) < RING_R + 600.0:
                continue                                                        # not through the ring
            if all(_len(_sub(c, o)) > 2100.0 for o in centres):
                break
        centres.append(c)
        spine = _rand_dir(rng)
        radial = _unit(c)
        up = _sub(radial, _mul(spine, _dot(radial, spine)))                     # out of the settlement, square to the spine
        if _len(up) < 0.2:
            up = _cross(spine, (0.0, 1.0, 0.0))
        up = _unit(up)
        side = _unit(_cross(up, spine))
        di = len(districts)
        dname = f"d{di}_{kind}"
        mods = DISTRICT_MODULES[kind]
        slots = rng.randint(3, 6)
        plan = [mods[rng.randrange(len(mods))] if len(mods) > 1 else mods[0] for _ in range(slots)]
        L = sum(m[1] for m in plan) + 80.0
        a = _sub(c, _mul(spine, L / 2))
        segs = int(math.ceil(L / SEG_L))
        rot_spine = _frame(spine, up)
        for k in range(segs):
            add(f"{dname}_sp{k}", "hubb_girder", _add(a, _mul(spine, k * SEG_L)), rot_spine, "spine", dname)
        ends = [_sub(a, _mul(spine, 30.0)), _add(a, _mul(spine, segs * SEG_L + 30.0))]
        for e, pe in enumerate(ends):
            add(f"{dname}_j{e}", "hubb_junction", pe, rot_spine, "junction", dname)
        t = 40.0
        faces = [up, _mul(up, -1.0), side, _mul(side, -1.0)]
        for si, (folder, pitch, mount, rate) in enumerate(plan):
            mid = _add(a, _mul(spine, t + pitch / 2))
            t += pitch
            use = faces if mount == "face" else [side, _mul(side, -1.0)] if mount == "comb" else [up, side, _mul(side, -1.0)]
            for fi, f in enumerate(use):
                if mount == "face" and fi > 1 and rng.random() < 0.35:
                    continue                                                    # some side faces stay bare
                nid = f"{dname}_m{si}_{fi}"
                base = _add(mid, _mul(f, GIRDER_H + 2.0))
                if mount == "face":
                    add(nid, folder, base, _frame(spine, f), "module", dname)
                    if folder in ("hubb_works", "hubv_refinery"):
                        import pads
                        from units.hub_vista import REFINERY_FLARE
                        R = pads.euler(_frame(spine, f))
                        haze += [{"position": [round(x, 1) for x in _add(apply(R, v), base)], "radius_m": 300.0, "density": 1.0}
                                 for v in (WORKS_VENTS if folder == "hubb_works" else [REFINERY_FLARE])]
                elif mount == "comb":
                    add(nid, folder, base, _frame(f, _cross(f, spine)), "dock", dname)
                else:
                    centre = _add(mid, _mul(f, AXIAL_OFFSET[folder]))
                    spinning.append({"id": nid, "module": folder, "position": centre, "rotation_deg": _frame(f, spine),
                                     "spin_axis_local": (0.0, 0.0, 1.0), "rate_deg_s": rate * (1 if rng.random() < 0.5 else -1),
                                     "title": DISTRICT_TITLE[kind]})
                    # its fixed mount: a short girder stub out of the spine to the axle
                    add(nid + "_mount", "hubb_girder", _add(mid, _mul(f, GIRDER_H)), _frame(f, spine), "mount", dname)
        districts.append({"id": dname, "kind": kind, "centre": c, "ends": ends, "spine": spine})

    # links: a minimum spanning tree over the district ends + the ring axle junctions, plus some short cross links
    pts = [("core", (0.0, 0.0, RING_AXLE)), ("core", (0.0, 0.0, -RING_AXLE))]
    for d in districts:
        pts += [(d["id"], d["ends"][0]), (d["id"], d["ends"][1])]
    kind_of = {d["id"]: d["kind"] for d in districts}
    kind_of["core"] = "core"
    groups = sorted({g for g, _ in pts})
    best = lambda ga, gb: min(((_len(_sub(pa, pb)), pa, pb) for g1, pa in pts if g1 == ga for g2, pb in pts if g2 == gb),
                              key=lambda x: x[0])
    inside, links = {"core"}, []
    while len(inside) < len(groups):
        cand = min((best(ga, gb) + (ga, gb) for ga in inside for gb in groups if gb not in inside), key=lambda x: x[0])
        links.append(cand)
        inside.add(cand[4])
    have = {(l[3], l[4]) for l in links} | {(l[4], l[3]) for l in links}
    for ga in groups:
        others = sorted((best(ga, gb) + (ga, gb) for gb in groups if gb != ga and (ga, gb) not in have), key=lambda x: x[0])
        if others and rng.random() < 0.4 and others[0][0] < 5000.0:
            links.append(others[0])
            have |= {(others[0][3], others[0][4]), (others[0][4], others[0][3])}
    for li, (dist, pa, pb, ga, gb) in enumerate(links):
        d = _unit(_sub(pb, pa))
        up = _cross(d, _cross((0.0, 1.0, 0.0), d))
        up = _unit(up) if _len(up) > 0.1 else _unit(_cross(d, (1.0, 0.0, 0.0)))
        side = _unit(_cross(up, d))
        rot = _frame(d, up)
        wheeled = kind_of[ga] in WHEELED or kind_of[gb] in WHEELED
        passenger = kind_of[ga] in PASSENGER or kind_of[gb] in PASSENGER
        L = dist - 60.0
        s0 = _add(pa, _mul(d, 30.0))
        for k in range(int(math.ceil(L / SEG_L))):
            g = f"l{li}_{k // 12}"
            at = _add(s0, _mul(d, k * SEG_L))
            add(f"l{li}_t{k}", "hubb_tube", at, rot, "tube", g)
            if k % 3 != 1:
                add(f"l{li}_g{k}", "hubb_girder", _add(at, _mul(side, -46.0)), rot, "girder", g)
            if wheeled:
                add(f"l{li}_r{k}", "hubb_road", _add(at, _mul(side, 30.0)), rot, "road", g)
            if passenger:
                add(f"l{li}_m{k}", "hubb_monorail", _add(at, _mul(up, 20.0)), rot, "monorail", g)
    return {"nodes": nodes, "spinning": spinning, "haze": haze, "module_of": module_of, "districts": districts,
            "links": len(links)}


def _module_obb():
    """per module: its local bounds (from the coarsest LOD) and the proxy material"""
    import units.hub_district as hd
    import units.hub_vista as hv
    out = {}
    builders = dict(ASSETS)
    builders.update(hv.ASSETS)
    builders["hubk_truss"] = hd.truss_parts
    for k, fn in builders.items():
        vs = [v for part in fn(2).values() for v in part.v]
        lo = tuple(min(v[i] for v in vs) for i in range(3))
        hi = tuple(max(v[i] for v in vs) for i in range(3))
        out[k] = (lo, hi, FACADE if k.startswith("hubb_hab") or k in ("hubb_hotel_hex", "hubb_ring_core") else HULL)
    return out


def belt_proxies(layout):
    """one simplified mesh per node group (a district, a 1.2 km stretch of a link): every node as its oriented
    bounding box (a girder as a thin bar, a tube as a square tube)"""
    import pads
    obb = _module_obb()
    parts, rep = {}, {}
    for nid, folder, pos, rot, role, parent in layout["nodes"]:
        g = layout["module_of"][nid]
        if g == "core" or folder not in obb:
            continue
        lo, hi, mat = obb[folder]
        R = pads.euler(rot)
        c = tuple((a + b) / 2 for a, b in zip(lo, hi))
        size = tuple(b - a for a, b in zip(lo, hi))
        if folder == "hubb_girder":
            size = (size[0] * 0.6, size[1] * 0.6, size[2])
        p = parts.setdefault(g, Part(g))
        box(p, mat, _add(apply(R, c), pos), size, R)
        rep.setdefault(g, nid)
    return {g: (parts[g], rep[g]) for g in parts}


# the settlements built as assemblies (hub_<id>): add an id here, run build_equipment_models.py, package it in the mesh
# editor, then tools/models/hub_belt_world.py --hub <id> puts it into the world
BELT_HUBS = ["belt_demo"]
SETS = {}
for _hid in BELT_HUBS:
    _L = belt_layout(_hid)
    ROOTS[f"hub_{_hid}"] = "hub"
    SETS[f"hub_{_hid}"] = {
        "title": f"Поселение в поясе «{_hid}» (генератор)",
        "description": f"Сгенерировано из id «{_hid}»: {len(_L['districts'])} районов, {_L['links']} связей, "
                       f"{len(_L['nodes'])} неподвижных узлов + {len(_L['spinning'])} вращающихся тел (отдельные объекты).",
        "nodes": _L["nodes"],
        "view_mode": "boxes",
        "modules": _L["module_of"],
        "proxies": belt_proxies(_L),
        "proxy_error_m": 40.0,
        "view_lods": {0: 1, 1: 2, 2: 2}}
