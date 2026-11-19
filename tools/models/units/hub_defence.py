"""Hub defence installations (docs/HUB_SECURITY.md; author 2026-11-19: «оборонительные установки - военный охранный
периметр + уничтожение непрошенных астероидов в мирное время»). Placed by the game client from the hub plan
(src/game/traffic/HubSecurity.cpp buildDefenceLayout: the outer corners of the rim blocks, the police block, a picket
ring at the perimeter) and aimed every frame (src/scene/HubDefencePresentation): each turret is a fixed BASE and a
slewing HEAD drawn on the base's pivot.

Frames: metres, +Y up (outward from the deck), mounting face y = 0. A HEAD: its pivot at the origin, the weapon along
+Z (the client yaws it about +Y and pitches it about +X). Pivot heights over the deck (the client's table
HubDefencePresentation.cpp kPivotY): PD laser 6.5 m, coilgun 5.0 m, picket 8.0 m, radar 61.0 m.

  hubd_pdl_base / hubd_pdl_head    point-defence laser turret (2 MW class beam director, 1.2 m aperture): stray bodies
                                    < 1.5 m - modelled on the ship-scale BT-4 (units/bastion.py), scaled up x3
  hubd_coil_base / hubd_coil_head  coilgun battery (3 km/s slugs, 22 m twin-rail barrel, capacitor banks): bigger bodies
  hubd_vls                         16-cell missile launcher block (military perimeter; closed hatches in peacetime)
  hubd_radar_mast / hubd_radar_dish  S-band search / track radar: a 60 m lattice mast, the rotating phased array on top
  hubd_picket                      a perimeter sentry platform (station-kept, sensor mast, radiators, a PD turret on top)
  hubd_rock                        a stray body (1 m nominal, scaled by its size)
  hubd_fx_beam / hubd_fx_flash     the laser beam (unit length along +Z) and an impact flash (unit size) - emissive
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM  # noqa: F401

register_materials({
    "eq_hubd_olive": ((0.27, 0.30, 0.25), (0.20, 0.20, 0.20), 40),     # military grey-green paint
    "eq_hubd_armour": ((0.17, 0.18, 0.19), (0.25, 0.25, 0.25), 50),    # armour plate, dark
    "eq_hubd_lens": ((0.10, 0.16, 0.22), (0.85, 0.85, 0.90), 140),     # beam director / sensor glass
    "eq_hubd_rock": ((0.30, 0.27, 0.24), (0.05, 0.05, 0.05), 8),       # a stray rock
})
OLIVE, ARMOUR, LENS, ROCKM = "eq_hubd_olive", "eq_hubd_armour", "eq_hubd_lens", "eq_hubd_rock"


def _deck(p, lod, side, h=1.0):
    """the mounting deck: a plate with a hazard border"""
    box(p, ARMOUR, (0, h / 2, 0), (side, h, side))
    if lod < 2:
        for s in (-1, 1):
            box(p, HAZ, (s * (side / 2 - 0.3), h + 0.02, 0), (0.6, 0.04, side - 1.2))
            box(p, HAZ, (0, h + 0.02, s * (side / 2 - 0.3)), (side - 1.2, 0.04, 0.6))


def _rail(p, y, half, lod):
    """a guard rail round a square of half size `half` at height y"""
    if lod >= 2:
        return
    for s in (-1, 1):
        box(p, STEEL, (s * half, y + 1.0, 0), (0.08, 0.08, half * 2))
        box(p, STEEL, (0, y + 1.0, s * half), (half * 2, 0.08, 0.08))
        for k in range(-2, 3):
            box(p, STEEL, (s * half, y + 0.5, k * half / 2), (0.08, 1.0, 0.08))
            box(p, STEEL, (k * half / 2, y + 0.5, s * half), (0.08, 1.0, 0.08))


def pdl_base_parts(lod):
    name = "hubd_pdl_base"
    P = {name: Part(name)}
    p = P[name]
    _deck(p, lod, 14.0)
    cyl(p, OLIVE, (0, 1.0, 0), (0, 5.6, 0), 2.6, seg_for(lod))            # the pedestal
    cyl(p, ARMOUR, (0, 5.6, 0), (0, 6.5, 0), 3.0, seg_for(lod))           # the slewing ring
    # the power / cooling cabinets and the radiator pair
    box(p, OLIVE, (4.6, 2.2, -3.0), (3.0, 2.4, 4.0))
    box(p, OLIVE, (-4.6, 2.0, -3.6), (2.6, 2.0, 3.0))
    for s in (-1, 1):
        box(p, LIGHT, (s * 4.6, 3.6, 3.2), (0.25, 4.6, 3.6))              # radiator fins (edge-on to the beam)
    if lod < 2:
        box(p, DARK, (4.6, 2.4, -0.98), (2.2, 1.2, 0.04))                 # cabinet door
        cyl(p, STEEL, (2.6, 3.0, 0), (4.6, 3.0, -1.0), 0.18, small_for(lod))   # conduits
        cyl(p, STEEL, (-2.6, 2.6, 0), (-4.6, 2.6, -2.0), 0.18, small_for(lod))
        box(p, BEACON, (6.4, 1.6, 6.4), (0.4, 0.4, 0.4))                   # the deck beacon
        _rail(p, 1.0, 6.8, lod)
    if close():
        for k in range(12):
            a = 2 * math.pi * k / 12
            hex_bolt(p, STEEL, (2.8 * math.cos(a), 6.5, 2.8 * math.sin(a)), (0, 1, 0), 0.12, 0.08)
    return P


def pdl_head_parts(lod):
    """a ball turret: the armoured ball, the beam director (1.2 m aperture) along +Z, a tracking sensor pod"""
    name = "hubd_pdl_head"
    P = {name: Part(name)}
    p = P[name]
    seg = seg_for(lod)
    prof = [(0.0, -2.3), (1.6, -2.1), (2.3, -1.0), (2.4, 0.0), (2.3, 1.0), (1.6, 2.1), (0.0, 2.3)]
    lathe(p, OLIVE, prof, seg, R=rot_x(-math.pi / 2), t=(0, 2.4, 0), soft=40)   # the ball, axis vertical
    cyl(p, ARMOUR, (0, 2.4, 1.8), (0, 2.4, 6.2), 0.95, seg)                   # the beam director tube
    cyl(p, ARMOUR, (0, 2.4, 6.2), (0, 2.4, 6.6), 1.15, seg)                   # the aperture ring
    cyl(p, LENS, (0, 2.4, 6.6), (0, 2.4, 6.64), 0.9, seg)                      # the output window
    box(p, ARMOUR, (1.9, 3.6, 1.6), (1.0, 1.0, 2.4))                          # the tracker pod
    if lod < 2:
        cyl(p, LENS, (1.9, 3.6, 2.8), (1.9, 3.6, 2.84), 0.35, small_for(lod))
        box(p, HAZ, (0, 2.4, 5.0), (2.0, 0.12, 0.5))                         # the barrel band
        for s in (-1, 1):
            box(p, ARMOUR, (s * 2.35, 2.4, 0), (0.3, 1.6, 1.6))              # trunnion caps
    return P


def coil_base_parts(lod):
    name = "hubd_coil_base"
    P = {name: Part(name)}
    p = P[name]
    _deck(p, lod, 22.0, 1.2)
    cyl(p, ARMOUR, (0, 1.2, 0), (0, 4.2, 0), 4.6, seg_for(lod))              # the armoured ring
    cyl(p, OLIVE, (0, 4.2, 0), (0, 5.0, 0), 4.0, seg_for(lod))
    # capacitor banks: two rows of drums each side
    for s in (-1, 1):
        for k in range(4 if lod < 2 else 1):
            z = -6.0 + k * 4.0 if lod < 2 else 0.0
            L = 3.0 if lod < 2 else 15.0
            if lod < 2:
                cyl(p, LIGHT, (s * 8.0, 1.2, z), (s * 8.0, 4.4, z), 1.3, small_for(lod))
            else:
                box(p, LIGHT, (s * 8.0, 2.8, z), (2.6, 3.2, L))
        box(p, OLIVE, (s * 8.0, 4.6, 0), (3.2, 0.4, 16.0))                    # the bank roof
    if lod < 2:
        for s in (-1, 1):
            cyl(p, STEEL, (s * 4.6, 2.6, 0), (s * 6.8, 2.6, 0), 0.4, small_for(lod))   # bus bars into the mount
        box(p, BEACON, (10.6, 1.8, 10.6), (0.5, 0.5, 0.5))
        box(p, BEACON, (-10.6, 1.8, -10.6), (0.5, 0.5, 0.5))
        _rail(p, 1.2, 10.8, lod)
    return P


def coil_head_parts(lod):
    """the mantlet with the twin-rail coil barrel (22 m) along +Z, coil rings, the breech / counterweight aft"""
    name = "hubd_coil_head"
    P = {name: Part(name)}
    p = P[name]
    box(p, OLIVE, (0, 2.6, 0), (7.0, 5.2, 7.0), chamfer=0.4)                 # the mantlet
    box(p, ARMOUR, (0, 2.6, -5.2), (5.0, 4.0, 3.4))                          # breech / counterweight
    for s in (-1, 1):
        box(p, ARMOUR, (s * 0.7, 2.6, 13.5), (0.6, 1.2, 20.0))               # the two rails
    rings = 9 if lod < 2 else 3
    for k in range(rings):
        z = 5.0 + 17.0 * (k + 0.5) / rings
        cyl(p, STEEL if k % 2 else LIGHT, (0, 2.6, z - 0.4), (0, 2.6, z + 0.4), 1.4, small_for(lod))   # coil rings
    cyl(p, ARMOUR, (0, 2.6, 23.2), (0, 2.6, 24.0), 1.5, small_for(lod))      # the muzzle brace
    if lod < 2:
        box(p, HAZ, (0, 5.25, 0), (6.0, 0.06, 0.8))
        box(p, ARMOUR, (2.8, 5.8, -1.0), (1.2, 1.0, 2.0))                    # fire-control sensor
        cyl(p, LENS, (2.8, 5.8, 0.0), (2.8, 5.8, 0.04), 0.4, small_for(lod))
    return P


def vls_parts(lod):
    """16 cells (4 x 4) under armoured hatches, an exhaust plenum vent each side"""
    name = "hubd_vls"
    P = {name: Part(name)}
    p = P[name]
    _deck(p, lod, 14.0, 0.8)
    box(p, OLIVE, (0, 2.8, 0), (9.0, 4.0, 9.0), chamfer=0.25)
    if lod < 2:
        for i in range(4):
            for j in range(4):
                box(p, ARMOUR, (-3.3 + 2.2 * i, 4.86, -3.3 + 2.2 * j), (1.8, 0.12, 1.8))   # the hatches
        for s in (-1, 1):
            box(p, DARK, (s * 4.52, 3.0, 0), (0.06, 1.6, 6.0))                 # plenum vents
        box(p, HAZ, (0, 4.82, 4.3), (8.6, 0.04, 0.3))
        box(p, BEACON, (4.6, 5.2, 4.6), (0.4, 0.4, 0.4))
    else:
        box(p, ARMOUR, (0, 4.86, 0), (8.6, 0.12, 8.6))
    return P


def radar_mast_parts(lod):
    """a 60 m four-leg lattice mast, the equipment house at its foot, the platform at the top"""
    name = "hubd_radar_mast"
    P = {name: Part(name)}
    p = P[name]
    _deck(p, lod, 16.0)
    box(p, OLIVE, (5.0, 3.0, 0), (5.0, 4.0, 8.0))                            # the equipment house
    H = 60.0
    b0, b1 = 4.0, 1.8   # leg half spacing at the foot / top
    for sx in (-1, 1):
        for sz in (-1, 1):
            cyl(p, STEEL, (sx * b0, 1.0, sz * b0), (sx * b1, H, sz * b1), 0.25, 6)
    levels = 10 if lod < 2 else 4
    for k in range(levels + 1):
        y = 1.0 + (H - 1.0) * k / levels
        w = b0 + (b1 - b0) * k / levels
        for s in (-1, 1):
            box(p, STEEL, (s * w, y, 0), (0.15, 0.15, 2 * w))
            box(p, STEEL, (0, y, s * w), (2 * w, 0.15, 0.15))
        if lod < 2 and k < levels:
            y2 = 1.0 + (H - 1.0) * (k + 1) / levels
            w2 = b0 + (b1 - b0) * (k + 1) / levels
            for s in (-1, 1):
                cyl(p, STEEL, (s * w, y, -w), (s * w2, y2, w2), 0.08, 4)       # diagonal braces
                cyl(p, STEEL, (-w, y, s * w), (w2, y2, s * w2), 0.08, 4)
    box(p, ARMOUR, (0, H + 0.4, 0), (6.0, 0.8, 6.0))                         # the top platform
    cyl(p, OLIVE, (0, H + 0.8, 0), (0, H + 1.0, 0), 1.6, small_for(lod))    # the turntable
    if lod < 2:
        box(p, BEACON, (0, H + 1.6, 2.6), (0.5, 0.5, 0.5))
        box(p, BEACON, (b0, 30.0, b0), (0.4, 0.4, 0.4))
    return P


def radar_dish_parts(lod):
    """the rotating S-band phased array (14 x 6 m face, tilted 15 deg up) with its IFF bar; pivot at the origin"""
    name = "hubd_radar_dish"
    P = {name: Part(name)}
    p = P[name]
    tilt = rot_x(-math.radians(15))
    box(p, OLIVE, (0, 1.0, 0), (2.0, 2.0, 2.0))                              # the pedestal
    box(p, LIGHT, (0, 4.2, 0.6), (14.0, 6.0, 0.6), R=tilt)                   # the array face
    box(p, ARMOUR, (0, 4.2, 0.0), (14.4, 6.4, 0.5), R=tilt)                  # its back frame
    box(p, ARMOUR, (0, 7.9, -0.4), (12.0, 0.6, 0.6), R=tilt)                 # the IFF bar
    if lod < 2:
        for k in range(-3, 4):
            box(p, DARK, (k * 1.9, 4.2, 0.92), (0.05, 5.6, 0.04), R=tilt)    # the array's tile seams
    return P


def picket_parts(lod):
    """a perimeter sentry platform: an octagonal hull 20 m across, 4 radiator / solar wings, a sensor mast, RCS quads,
    the PD turret's ring on the top deck (y 8)"""
    name = "hubd_picket"
    P = {name: Part(name)}
    p = P[name]
    oct_ = [(10.0 * math.cos(math.pi / 8 + k * math.pi / 4), 10.0 * math.sin(math.pi / 8 + k * math.pi / 4)) for k in range(8)]
    prism(p, OLIVE, oct_, -6.0, 6.0)
    prism(p, ARMOUR, [(x * 0.7, z * 0.7) for x, z in oct_], 6.0, 7.2)
    prism(p, ARMOUR, [(x * 0.6, z * 0.6) for x, z in oct_], -8.0, -6.0)
    cyl(p, ARMOUR, (0, 7.2, 0), (0, 8.0, 0), 3.0, seg_for(lod))              # the turret ring
    for k in range(4):
        a = k * math.pi / 2
        c, s = math.cos(a), math.sin(a)
        R = rot_y(-a)
        box(p, LIGHT, (c * 19.0, 0.0, s * 19.0), (18.0, 0.3, 7.0), R=R)       # wings (radiator / solar)
        box(p, STEEL, (c * 10.8, 0.0, s * 10.8), (1.8, 0.8, 0.8), R=R)       # wing roots
        if lod < 2:
            box(p, DARK, (c * 9.6, 4.0, s * 9.6), (0.6, 0.6, 0.6), R=R)      # RCS quads
            box(p, BEACON, (c * 27.8, 0.0, s * 27.8), (0.5, 0.5, 0.5))         # wingtip lights
    cyl(p, STEEL, (6.0, 6.0, 0), (6.0, 18.0, 0), 0.3, 6)                     # the sensor mast
    box(p, ARMOUR, (6.0, 18.6, 0), (1.6, 1.2, 1.6))
    if lod < 2:
        cyl(p, LENS, (6.0, 18.6, 0.8), (6.0, 18.6, 0.84), 0.4, small_for(lod))
        box(p, HAZ, (0, 0.0, 9.3), (6.0, 1.2, 0.06))
        box(p, WARM, (0, 3.0, -9.25), (4.0, 1.0, 0.06))                       # the crew cabin window
    return P


def rock_parts(lod):
    """a stray rock ~1 m: three rotated blocks (a lumpy, faceted body)"""
    name = "hubd_rock"
    P = {name: Part(name)}
    p = P[name]
    box(p, ROCKM, (0, 0, 0), (0.9, 0.7, 1.0), R=mat_mul(rot_y(0.5), rot_x(0.3)))
    box(p, ROCKM, (0.1, 0.05, -0.1), (0.7, 0.85, 0.75), R=mat_mul(rot_z(0.7), rot_y(-0.4)))
    if lod < 2:
        box(p, ROCKM, (-0.12, -0.1, 0.12), (0.75, 0.6, 0.8), R=mat_mul(rot_x(0.9), rot_z(-0.3)))
    return P


def fx_beam_parts(lod):
    """the laser beam: a unit-length square rod from z 0 to z 1 (the client scales it: width x width x length)"""
    name = "hubd_fx_beam"
    P = {name: Part(name)}
    box(P[name], FLOOD, (0, 0, 0.5), (1.0, 1.0, 1.0))
    return P


def fx_flash_parts(lod):
    """an impact flash: a burst of crossed blocks, unit size"""
    name = "hubd_fx_flash"
    P = {name: Part(name)}
    p = P[name]
    box(p, WARM, (0, 0, 0), (1.0, 0.25, 0.25))
    box(p, WARM, (0, 0, 0), (0.25, 1.0, 0.25))
    box(p, WARM, (0, 0, 0), (0.25, 0.25, 1.0))
    box(p, FLOOD, (0, 0, 0), (0.5, 0.5, 0.5), R=mat_mul(rot_y(0.785), rot_x(0.615)))
    return P


ASSETS = {
    "hubd_pdl_base": pdl_base_parts,
    "hubd_pdl_head": pdl_head_parts,
    "hubd_coil_base": coil_base_parts,
    "hubd_coil_head": coil_head_parts,
    "hubd_vls": vls_parts,
    "hubd_radar_mast": radar_mast_parts,
    "hubd_radar_dish": radar_dish_parts,
    "hubd_picket": picket_parts,
    "hubd_rock": rock_parts,
    "hubd_fx_beam": fx_beam_parts,
    "hubd_fx_flash": fx_flash_parts,
}
HITS = {
    "hubd_pdl_base": lambda: [hbox("hubd_pdl_base", (0, 0.5, 0), (14.0, 1.0, 14.0)), hcyl("hubd_pdl_base", (0, 1.0, 0), (0, 6.5, 0), 3.0)],
    "hubd_pdl_head": lambda: [hcyl("hubd_pdl_head", (0, 0.1, 0), (0, 4.7, 0), 2.4), hcyl("hubd_pdl_head", (0, 2.4, 1.8), (0, 2.4, 6.6), 1.15)],
    "hubd_coil_base": lambda: [hbox("hubd_coil_base", (0, 2.5, 0), (22.0, 5.0, 22.0))],
    "hubd_coil_head": lambda: [hbox("hubd_coil_head", (0, 2.6, 0), (7.0, 5.2, 7.0)), hbox("hubd_coil_head", (0, 2.6, 14.0), (3.0, 3.0, 20.0))],
    "hubd_vls": lambda: [hbox("hubd_vls", (0, 2.5, 0), (14.0, 5.0, 14.0))],
    "hubd_radar_mast": lambda: [hbox("hubd_radar_mast", (0, 31.0, 0), (9.0, 62.0, 9.0))],
    "hubd_radar_dish": lambda: [hbox("hubd_radar_dish", (0, 4.2, 0.3), (14.4, 8.0, 2.0))],
    "hubd_picket": lambda: [hcyl("hubd_picket", (0, -8.0, 0), (0, 8.0, 0), 10.0), hbox("hubd_picket", (0, 0, 0), (56.0, 0.6, 7.0)),
                            hbox("hubd_picket", (0, 0, 0), (7.0, 0.6, 56.0))],
    "hubd_rock": lambda: [hbox("hubd_rock", (0, 0, 0), (1.0, 1.0, 1.0))],
    "hubd_fx_beam": lambda: [],
    "hubd_fx_flash": lambda: [],
}
ROOTS = {k: "hub" for k in ASSETS}
_SRC = "Оборона хаба (tools/models/units/hub_defence.py, docs/HUB_SECURITY.md)."
PASSPORTS = {
    "hubd_pdl_base": ("Турель ПРО-лазера: основание", "hub_module", _SRC),
    "hubd_pdl_head": ("Турель ПРО-лазера: башня с лучевым директором", "hub_module", _SRC),
    "hubd_coil_base": ("Батарея койлгана: основание и конденсаторы", "hub_module", _SRC),
    "hubd_coil_head": ("Батарея койлгана: башня со стволом 22 м", "hub_module", _SRC),
    "hubd_vls": ("Пусковой блок 16 ячеек", "hub_module", _SRC),
    "hubd_radar_mast": ("Мачта РЛС 60 м", "hub_module", _SRC),
    "hubd_radar_dish": ("Антенна РЛС (ФАР 14 x 6 м)", "hub_module", _SRC),
    "hubd_picket": ("Сторожевая платформа периметра", "hub_module", _SRC),
    "hubd_rock": ("Блуждающее тело (камень 1 м)", "hub_module", _SRC),
    "hubd_fx_beam": ("Эффект: луч лазера", "hub_module", _SRC),
    "hubd_fx_flash": ("Эффект: вспышка попадания", "hub_module", _SRC),
}
