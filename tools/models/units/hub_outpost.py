"""Outpost kit: the small modules of a settlement for about ten people (author 2026-11-21: «хаб строится из готовых
модулей или переоборудованных брошенных кораблей ... деревня на десяток ... генератор, станция очистки воды, небольшие
ангары для кораблей, заправочная станция, жилые модули, ремонтные модули»; «обязательно реактор, обеспечение
жизнедеятельности - вода, очистка, гидропоника, площадки, позднее - ангары, минимальная заправка, ремонтные боксы»;
a pirate beacon works on a schedule - its dish has a shutter). docs/HUB_FOUNDING.md.

Free-fall modules like hub_vista's: each stands on its own mounting plate at its origin, "out" = +Y, so a layout hangs
them on an asteroid, a truss or a derelict hull. Metres; sized after real kit (ISS modules ~4.2 m across, a 20-person
station's power ~1-2 MW, a small helipad 20-30 m, a light-aircraft hangar 20 x 15 m).

  hubo_core      the node: a pressurised hub with four docking collars and an airlock
  hubo_reactor   a small reactor (~2 MW e): the shielded core on a boom away from the crew, radiator wings
  hubo_water     water recovery: tanks, distillation columns, pipework
  hubo_hydro     hydroponics: a lit greenhouse cylinder (food and air)
  hubo_hab       a habitat for 4-6: a cylinder with windows, a cupola
  hubo_hangar    a one-ship hangar (Cobrett-class) with a door and its lights
  hubo_fuel      a minimal refuel: charge tanks, a hose boom, a small pad
  hubo_repair    a repair box: an open frame bay with two manipulator arms and floodlights
  hubo_pad       a small landing pad on a truss (24 m)
  hubo_beacon    a beacon mast with a dish; hubo_beacon_hidden - the dish under a shutter (a pirate's)
  hubo_truss     a 20 m connecting truss with a cable tray (the outpost's skeleton)
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_belt import GRATE, MARK, RAD, bullet, sphere  # noqa: F401

register_materials({
    "eq_hubo_glass": ((0.22, 0.72, 0.38), (0.45, 0.45, 0.45), 80),      # the greenhouse panes (lit green inside)
    "eq_hubo_grow": ((0.95, 0.42, 0.85), (0.10, 0.10, 0.10), 20),       # grow lights (magenta, emissive)
    "eq_hubo_shield": ((0.30, 0.31, 0.30), (0.10, 0.10, 0.10), 10),     # the reactor's shadow shield
})
GLASS, GROW, SHIELD = "eq_hubo_glass", "eq_hubo_grow", "eq_hubo_shield"


def _seg(lod, hi=20):
    return {0: hi, 1: max(10, hi // 2), 2: 8, 3: 6}.get(lod, 6)


def plate(p, r, lod, h=0.6):
    """the mounting plate at the origin (+Y out)"""
    ylathe(p, STEEL, [(0.0, 0.0), (r, 0.0), (r, h), (0.0, h)], _seg(lod, 12))


def can(p, mat, y0, y1, r, lod, axis="y", c=(0.0, 0.0, 0.0)):
    """a cylinder along an axis between two coordinates (c: the other two)"""
    if axis == "y":
        cyl(p, mat, (c[0], y0, c[2]), (c[0], y1, c[2]), r, _seg(lod))
    elif axis == "x":
        cyl(p, mat, (y0, c[1], c[2]), (y1, c[1], c[2]), r, _seg(lod))
    else:
        cyl(p, mat, (c[0], c[1], y0), (c[0], c[1], y1), r, _seg(lod))


def windows_ring(p, y, r, n, lod, mat=WARM):
    if lod >= 2:
        return
    for k in range(n):
        a = 2 * math.pi * k / n
        box(p, mat, ((r + 0.02) * math.cos(a), y, (r + 0.02) * math.sin(a)), (0.05, 0.6, 0.9), rot_y(-a))


# ---------------------------------------------------------------- the node
def core_parts(lod):
    P = {"hubo_core": Part("hubo_core")}
    p = P["hubo_core"]
    plate(p, 3.0, lod)
    can(p, HULL, 0.6, 9.0, 2.2, lod)
    sphere(p, HULL, (0, 9.0, 0), 3.0, _seg(lod))
    for a in range(4):                                                   # the docking collars
        ang = math.pi / 2 * a
        d = (math.cos(ang), math.sin(ang))
        cyl(p, LIGHT, (d[0] * 2.6, 9.0, d[1] * 2.6), (d[0] * 4.6, 9.0, d[1] * 4.6), 1.1, _seg(lod, 12))
        if lod < 2:
            beacon(p, (d[0] * 4.7, 10.4, d[1] * 4.7), 0.3)
    box(p, DARK, (0, 5.0, 2.25), (1.2, 1.9, 0.1))                        # the airlock hatch
    windows_ring(p, 7.0, 2.2, 6, lod)
    beacon(p, (0, 12.2, 0), 0.4)
    return P


def core_hits():
    return [hcyl("hubo_core", (0, 0, 0), (0, 12.0, 0), 3.0), hbox("hubo_core", (0, 9.0, 0), (9.6, 2.4, 9.6))]


# ---------------------------------------------------------------- the reactor
def reactor_parts(lod):
    P = {"hubo_reactor": Part("hubo_reactor")}
    p = P["hubo_reactor"]
    plate(p, 2.5, lod)
    lattice = 3 if lod < 2 else 1
    for k in range(4):                                                   # the boom: the core 22 m out from the crew
        a = math.pi / 2 * k + math.pi / 4
        box(p, STEEL, (1.0 * math.cos(a), 11.0, 1.0 * math.sin(a)), (0.3, 21.0, 0.3))
    if lod < 3:
        for j in range(lattice * 3):
            y = 2.0 + j * 18.0 / (lattice * 3)
            box(p, STEEL, (0, y, 0), (2.0, 0.2, 2.0))
    cyl(p, SHIELD, (0, 21.0, 0), (0, 22.5, 0), 2.4, _seg(lod))          # the shadow shield toward the crew
    bullet(p, HULL, 0.0, 0.0, 22.5, 1.6, 2.5, _seg(lod))                 # the reactor vessel
    for s in (-1, 1):                                                    # the radiator wings
        box(p, RAD, (s * 7.5, 18.0, 0), (11.0, 8.0, 0.15))
        box(p, STEEL, (s * 2.3, 18.0, 0), (1.6, 0.4, 0.4))
        if lod < 2:
            for k in range(5):
                box(p, STEEL, (s * (3.0 + k * 2.2), 18.0, 0.0), (0.12, 8.0, 0.3))
    beacon(p, (0, 28.5, 0), 0.4)
    box(p, HAZ, (0, 21.75, 2.41), (2.0, 0.6, 0.05))
    return P


def reactor_hits():
    return [hcyl("hubo_reactor", (0, 0, 0), (0, 21.0, 0), 1.6), hcyl("hubo_reactor", (0, 21.0, 0), (0, 28.0, 0), 2.4),
            hbox("hubo_reactor", (0, 18.0, 0), (26.0, 8.0, 0.6))]


# ---------------------------------------------------------------- water recovery
def water_parts(lod):
    P = {"hubo_water": Part("hubo_water")}
    p = P["hubo_water"]
    plate(p, 5.5, lod)
    box(p, GRATE, (0, 0.8, 0), (10.0, 0.3, 10.0))
    for x, z in ((-2.6, -2.6), (2.6, -2.6), (-2.6, 2.6)):               # the tanks (raw, grey, clean)
        bullet(p, HULL if x < 0 else LIGHT, x, z, 1.0, 1.7, 3.0, _seg(lod, 14))
    for k in range(3):                                                   # the distillation / filter columns
        cyl(p, STEEL, (2.0 + k * 0.9, 1.0, 2.0), (2.0 + k * 0.9, 8.0 + k, 2.0), 0.35, _seg(lod, 10))
    if lod < 2:
        for y in (3.0, 5.0, 7.0):                                        # the pipework
            cyl(p, STEEL, (-2.6, y, -0.9), (2.6, y, -0.9), 0.15, 6)
            cyl(p, STEEL, (-0.9, y, -2.6), (-0.9, y, 2.6), 0.15, 6)
    box(p, COOL, (3.2, 2.0, -3.2), (2.4, 2.0, 2.4))                      # the control cabin
    beacon(p, (3.8, 10.5, 2.0), 0.3)
    return P


def water_hits():
    return [hbox("hubo_water", (0, 5.0, 0), (11.0, 10.0, 11.0))]


# ---------------------------------------------------------------- hydroponics
def hydro_parts(lod):
    P = {"hubo_hydro": Part("hubo_hydro")}
    p = P["hubo_hydro"]
    plate(p, 3.0, lod)
    box(p, STEEL, (0, 2.0, 0), (1.2, 3.0, 1.2))
    can(p, HULL, -7.0, -6.0, 2.6, lod, axis="x", c=(0, 6.0, 0))         # the end caps
    can(p, HULL, 6.0, 7.0, 2.6, lod, axis="x", c=(0, 6.0, 0))
    can(p, GLASS, -6.0, 6.0, 2.4, lod, axis="x", c=(0, 6.0, 0))         # the greenhouse
    if lod < 3:
        for k in range(6):                                               # the ribs
            x = -5.0 + k * 2.0
            cyl(p, STEEL, (x - 0.15, 6.0, 0), (x + 0.15, 6.0, 0), 2.62, _seg(lod, 16))   # a hoop round the glass
        for x in (-4.0, 0.0, 4.0):                                       # the grow light bars inside
            box(p, GROW, (x, 6.0, 0), (3.0, 0.2, 3.6))
    beacon(p, (7.0, 8.8, 0), 0.3)
    return P


def hydro_hits():
    return [hcyl("hubo_hydro", (-7.0, 6.0, 0), (7.0, 6.0, 0), 2.6), hbox("hubo_hydro", (0, 2.0, 0), (1.4, 4.0, 1.4))]


# ---------------------------------------------------------------- habitat
def hab_parts(lod):
    P = {"hubo_hab": Part("hubo_hab")}
    p = P["hubo_hab"]
    plate(p, 2.8, lod)
    can(p, HULL, 0.6, 10.0, 2.1, lod)
    sphere(p, HULL, (0, 10.0, 0), 2.1, _seg(lod))
    windows_ring(p, 4.0, 2.1, 8, lod)
    windows_ring(p, 7.0, 2.1, 8, lod)
    sphere(p, GLASS, (0, 12.0, 0), 0.9, _seg(lod, 10))                   # the cupola
    if lod < 2:
        for k in range(4):                                               # handrails
            a = math.pi / 2 * k
            cyl(p, HAZ, (2.2 * math.cos(a), 1.5, 2.2 * math.sin(a)), (2.2 * math.cos(a), 9.5, 2.2 * math.sin(a)), 0.05, 4)
    box(p, DARK, (0, 2.0, 2.15), (1.0, 1.8, 0.1))                        # the hatch
    return P


def hab_hits():
    return [hcyl("hubo_hab", (0, 0, 0), (0, 13.0, 0), 2.3)]


# ---------------------------------------------------------------- one-ship hangar
HG_W, HG_D, HG_H = 24.0, 32.0, 10.0


def hangar_parts(lod):
    P = {"hubo_hangar": Part("hubo_hangar")}
    p = P["hubo_hangar"]
    box(p, STEEL, (0, 0.3, 0), (HG_W + 2.0, 0.6, HG_D + 2.0))
    for s in (-1, 1):
        box(p, HULL, (s * (HG_W / 2 - 0.4), HG_H / 2 + 0.6, 0), (0.8, HG_H, HG_D))
    box(p, HULL, (0, HG_H + 0.6 - 0.4, 0), (HG_W, 0.8, HG_D))
    box(p, HULL, (0, HG_H / 2 + 0.6, -HG_D / 2 + 0.4), (HG_W, HG_H, 0.8))
    if lod < 3:
        for k in range(int(HG_D / 4.0)):                                 # roof ribs
            box(p, STEEL, (0, HG_H + 0.9, -HG_D / 2 + 2.0 + k * 4.0), (HG_W, 0.4, 0.4))
    box(p, LIGHT, (0, HG_H - 1.6, HG_D / 2 - 0.1), (HG_W - 2.0, 2.4, 0.2))  # the door, rolled up
    if lod < 2:
        box(p, FLOOD, (0, HG_H - 0.4, HG_D / 2 + 1.0), (8.0, 0.3, 0.6))
        box(p, DARK, (0, 0.65, 0), (10.0, 0.1, HG_D - 4.0))
        for s in (-1, 1):
            box(p, HAZ, (s * 5.0, 0.65, 0), (0.4, 0.1, HG_D - 4.0))
    beacon(p, (HG_W / 2 - 0.5, HG_H + 1.6, HG_D / 2), 0.4)
    beacon(p, (-HG_W / 2 + 0.5, HG_H + 1.6, HG_D / 2), 0.4)
    return P


def hangar_hits():
    return [hbox("hubo_hangar", (0, HG_H / 2 + 0.6, 0), (HG_W + 2.0, HG_H + 1.2, HG_D + 2.0))]


# ---------------------------------------------------------------- minimal refuel
def fuel_parts(lod):
    P = {"hubo_fuel": Part("hubo_fuel")}
    p = P["hubo_fuel"]
    box(p, STEEL, (0, 0.3, 0), (30.0, 0.6, 22.0))
    box(p, HULL, (-7.0, 0.7, 0), (18.0, 0.2, 18.0))                      # the pad
    if lod < 3:
        for s in (-1, 1):
            box(p, HAZ, (-7.0, 0.85, s * 8.6), (18.0, 0.1, 0.6))
            box(p, HAZ, (-7.0 + s * 8.6, 0.85, 0), (0.6, 0.1, 18.0))
    for k in range(3):                                                   # the charge tanks (hazard class D)
        bullet(p, LIGHT, 9.0, -6.0 + k * 6.0, 0.6, 2.0, 3.0, _seg(lod, 14))
        if lod < 2:
            box(p, HAZ, (9.0, 4.6, -6.0 + k * 6.0), (4.1, 0.5, 4.1))
    cyl(p, STEEL, (4.0, 0.6, 0), (4.0, 8.0, 0), 0.4, 8)                  # the hose boom
    cyl(p, STEEL, (4.0, 8.0, 0), (-3.0, 7.0, 0), 0.3, 8)
    if lod < 2:
        cyl(p, DARK, (-3.0, 7.0, 0), (-3.0, 3.0, 0), 0.15, 6)            # the hose
        box(p, FLOOD, (-3.0, 7.5, 0), (1.0, 0.3, 1.0))
    beacon(p, (4.0, 8.8, 0), 0.4)
    return P


def fuel_hits():
    return [hbox("hubo_fuel", (0, 0.4, 0), (30.0, 0.8, 22.0)), hbox("hubo_fuel", (9.0, 3.5, 0), (4.2, 7.0, 16.2))]


# ---------------------------------------------------------------- repair box
RB_W, RB_D, RB_H = 26.0, 34.0, 12.0


def repair_parts(lod):
    P = {"hubo_repair": Part("hubo_repair")}
    p = P["hubo_repair"]
    box(p, STEEL, (0, 0.3, 0), (RB_W, 0.6, RB_D))
    for sx in (-1, 1):                                                   # the open frame
        for sz in (-1, 0, 1):
            box(p, STEEL, (sx * (RB_W / 2 - 0.5), RB_H / 2 + 0.6, sz * (RB_D / 2 - 0.5)), (0.8, RB_H, 0.8))
        box(p, STEEL, (sx * (RB_W / 2 - 0.5), RB_H + 0.6, 0), (0.8, 0.8, RB_D))
    for sz in (-1, 0, 1):
        box(p, STEEL, (0, RB_H + 0.6, sz * (RB_D / 2 - 0.5)), (RB_W, 0.8, 0.8))
    for s in (-1, 1):                                                    # two manipulator arms off the top rail
        x0 = s * 6.0
        cyl(p, HAZ, (x0, RB_H + 0.2, -6.0), (x0, RB_H - 4.0, -2.0), 0.35, 8)
        cyl(p, HAZ, (x0, RB_H - 4.0, -2.0), (x0 * 0.6, RB_H - 6.5, 2.0), 0.3, 8)
        box(p, DARK, (x0 * 0.6, RB_H - 7.0, 2.0), (0.8, 0.8, 0.8))
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(p, FLOOD, (sx * (RB_W / 2 - 1.5), RB_H, sz * (RB_D / 2 - 1.5)), (1.2, 0.3, 1.2))
        box(p, HULL, (RB_W / 2 + 3.0, 2.1, -RB_D / 2 + 3.0), (4.0, 3.0, 5.0))   # the parts store
    beacon(p, (RB_W / 2 - 0.5, RB_H + 1.4, RB_D / 2 - 0.5), 0.4)
    return P


def repair_hits():
    return [hbox("hubo_repair", (0, 0.3, 0), (RB_W, 0.6, RB_D)), hbox("hubo_repair", (0, RB_H + 0.6, 0), (RB_W, 1.0, RB_D))]


# ---------------------------------------------------------------- small pad
def pad_parts(lod):
    P = {"hubo_pad": Part("hubo_pad")}
    p = P["hubo_pad"]
    plate(p, 2.0, lod)
    box(p, STEEL, (0, 4.0, 0), (1.6, 7.0, 1.6))                          # the post
    for s in (-1, 1):
        cyl(p, STEEL, (0, 1.0, 0), (s * 8.0, 7.2, 0), 0.25, 6)
        cyl(p, STEEL, (0, 1.0, 0), (0, 7.2, s * 8.0), 0.25, 6)
    box(p, HULL, (0, 7.6, 0), (24.0, 0.6, 24.0))
    if lod < 3:
        for s in (-1, 1):
            box(p, HAZ, (0, 7.95, s * 11.6), (24.0, 0.1, 0.6))
            box(p, HAZ, (s * 11.6, 7.95, 0), (0.6, 0.1, 24.0))
        for x, z, sx, sz in ((-2.5, 0, 0.8, 6.5), (2.5, 0, 0.8, 6.5), (0, 0, 4.2, 0.8)):
            box(p, LIGHT, (x, 7.97, z), (sx, 0.1, sz))
    for sx in (-1, 1):
        for sz in (-1, 1):
            beacon(p, (sx * 11.5, 8.2, sz * 11.5), 0.35)
    return P


def pad_hits():
    return [hbox("hubo_pad", (0, 7.6, 0), (24.0, 0.8, 24.0)), hbox("hubo_pad", (0, 4.0, 0), (2.0, 8.0, 2.0))]


# ---------------------------------------------------------------- the beacon (open / shuttered)
def beacon_builder(name, hidden):
    def parts(lod):
        P = {name: Part(name)}
        p = P[name]
        plate(p, 2.0, lod)
        for k in range(3):                                               # the triangular lattice mast
            a = 2 * math.pi * k / 3
            cyl(p, STEEL, (1.0 * math.cos(a), 0.6, 1.0 * math.sin(a)), (0.4 * math.cos(a), 18.0, 0.4 * math.sin(a)), 0.12, 6)
        if lod < 3:
            for j in range(8):
                y = 1.0 + j * 2.1
                ylathe(p, STEEL, [(0.95 - j * 0.07, y), (1.0 - j * 0.07, y), (1.0 - j * 0.07, y + 0.15), (0.95 - j * 0.07, y + 0.15)], 6)
        if hidden:                                                       # the dish folded under a shutter box
            box(p, DARK, (0, 18.6, 0), (2.4, 1.6, 2.4))
        else:
            ylathe(p, LIGHT, [(0.0, 18.0), (1.6, 18.8), (1.7, 18.9), (0.0, 18.2)], _seg(lod, 16), t=(0, 0, 0))
            beacon(p, (0, 19.6, 0), 0.4)
        return P
    return parts


def beacon_hits_for(name):
    def hits():
        return [hcyl(name, (0, 0, 0), (0, 19.5, 0), 1.7)]
    return hits


# ---------------------------------------------------------------- the truss
def truss_parts(lod):
    P = {"hubo_truss": Part("hubo_truss")}
    p = P["hubo_truss"]
    L, h = 20.0, 1.6
    for sx in (-1, 1):
        for sy in (0, 1):
            box(p, STEEL, (sx * h / 2, sy * h + 0.3, 0), (0.15, 0.15, L))
    if lod < 3:
        for k in range(11):
            z = -L / 2 + k * L / 10
            box(p, STEEL, (0, 0.3, z), (h, 0.1, 0.1))
            box(p, STEEL, (0, h + 0.3, z), (h, 0.1, 0.1))
            for sx in (-1, 1):
                box(p, STEEL, (sx * h / 2, h / 2 + 0.3, z), (0.1, h, 0.1))
    box(p, DARK, (0, 0.6, 0), (0.6, 0.2, L))                             # the cable tray
    return P


def truss_hits():
    return [hbox("hubo_truss", (0, 1.1, 0), (1.8, 1.8, 20.0))]


KIT = {
    "hubo_core": (core_parts, core_hits, "Форпост: узел-ядро со стыковочными узлами"),
    "hubo_reactor": (reactor_parts, reactor_hits, "Форпост: малый реактор ~2 МВт с радиаторами"),
    "hubo_water": (water_parts, water_hits, "Форпост: очистка и регенерация воды"),
    "hubo_hydro": (hydro_parts, hydro_hits, "Форпост: гидропоника (оранжерея)"),
    "hubo_hab": (hab_parts, hab_hits, "Форпост: жилой модуль на 4-6 человек"),
    "hubo_hangar": (hangar_parts, hangar_hits, "Форпост: ангар на один корабль"),
    "hubo_fuel": (fuel_parts, fuel_hits, "Форпост: малая заправка с площадкой"),
    "hubo_repair": (repair_parts, repair_hits, "Форпост: ремонтный бокс с манипуляторами"),
    "hubo_pad": (pad_parts, pad_hits, "Форпост: малая площадка 24 м на опоре"),
    "hubo_beacon": (beacon_builder("hubo_beacon", False), beacon_hits_for("hubo_beacon"), "Форпост: маяк-мачта с антенной"),
    "hubo_beacon_hidden": (beacon_builder("hubo_beacon_hidden", True), beacon_hits_for("hubo_beacon_hidden"),
                           "Форпост: маяк под заслонкой (пиратский, работает по расписанию)"),
    "hubo_truss": (truss_parts, truss_hits, "Форпост: соединительная ферма 20 м"),
}

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Малые модули форпоста (tools/models/units/hub_outpost.py, docs/HUB_FOUNDING.md).")
             for k, v in KIT.items()}
