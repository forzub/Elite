"""The city's traffic vehicles (author 2026-11-18: «надо сделать трафик - сгенерировать каких-то типа машин, грузовичков,
пассажирских поездов»). Small models driven by the game client along the hub plan's streets (src/scene/HubCityTraffic):
cars and trucks on the road bridges (both sides of the plane), passenger trains on the monorail beams.

Frame of every vehicle: +Z forward, +Y up (the wheels / the bogies on y 0), centred on X. Sizes: a car 4.6 x 1.9 m, a
truck 16.5 x 2.6 m (a cab + a 40 ft box), a monorail train 3 cars x 18 m (straddling the 1.2 m rail: its floor on
the rail top). Lit windows / lamps make them read at night distances.
"""
from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM  # noqa: F401
from units.hub_belt import CONT

register_materials({
    "eq_car_red": ((0.55, 0.10, 0.08), (0.45, 0.45, 0.45), 80),
    "eq_car_blue": ((0.12, 0.22, 0.48), (0.45, 0.45, 0.45), 80),
    "eq_car_white": ((0.80, 0.81, 0.82), (0.45, 0.45, 0.45), 80),
    "eq_car_glass": ((0.06, 0.08, 0.10), (0.70, 0.70, 0.75), 120),
    "eq_car_tail": ((0.90, 0.10, 0.06), (0.10, 0.10, 0.10), 10),
})


def car_parts(lod, name, paint):
    P = {name: Part(name)}
    c = P[name]
    box(c, paint, (0, 0.65, 0), (1.9, 0.8, 4.6))                     # body
    box(c, "eq_car_glass" if lod < 2 else paint, (0, 1.3, -0.2), (1.7, 0.6, 2.4))   # cabin
    if lod < 2:
        box(c, DARK, (0, 0.25, 0), (1.95, 0.4, 4.0))                  # wheels / skirt
        for sx in (-0.65, 0.65):
            box(c, FLOOD, (sx, 0.75, 2.31), (0.35, 0.18, 0.04))       # headlights
            box(c, "eq_car_tail", (sx, 0.8, -2.31), (0.35, 0.15, 0.04))
    return P


def truck_parts(lod):
    name = "hubt_truck"
    P = {name: Part(name)}
    t = P[name]
    box(t, HAZ, (0, 1.9, 6.5), (2.5, 2.6, 3.0))                       # cab
    box(t, CONT[1], (0, 2.3, -1.7), (2.5, 2.9, 12.2))                 # the 40 ft box
    if lod < 3:
        box(t, DARK, (0, 0.55, 0.6), (2.4, 1.1, 15.8))                # chassis + wheels
    if lod < 2:
        box(t, WARM, (0, 2.6, 8.02), (2.2, 1.0, 0.04))                # the cab window, lit
        for sx in (-0.9, 0.9):
            box(t, FLOOD, (sx, 1.0, 8.02), (0.4, 0.25, 0.04))
            box(t, "eq_car_tail", (sx, 1.0, -7.82), (0.4, 0.2, 0.04))
    return P


def train_parts(lod):
    """a 3-car monorail train straddling the beam's rail (rail top = y 0 here): cars 18 m, 3 m wide, 3.4 m high"""
    name = "hubt_train"
    P = {name: Part(name)}
    t = P[name]
    for k, z in enumerate((-19.0, 0.0, 19.0)):
        box(t, LIGHT, (0, 2.0, z), (3.0, 3.4, 18.0))                  # the car body
        box(t, STEEL, (0, -0.6, z), (2.0, 1.6, 14.0))                 # the straddle bogie skirt over the rail
        if lod < 2:
            for sx in (-1.51, 1.51):
                box(t, COOL, (sx, 2.4, z), (0.04, 1.0, 15.0))         # the lit window band
        if lod < 3 and k < 2:
            box(t, DARK, (0, 2.0, z + 9.5), (2.0, 2.6, 1.0))           # the gangway
    if lod < 2:
        for zf, mat in ((28.02, FLOOD), (-28.02, "eq_car_tail")):
            box(t, mat, (0, 1.4, zf), (1.6, 0.5, 0.04))
    return P


def shuttle_parts(lod):
    """a passenger shuttle of the city's air traffic (author 2026-11-18: «воздушный трафик из фронтиров и др.»): a
    windowed fuselage 30 m, stub wings, two engines glowing aft; NOSE TO -Z like the ship packages (the client flies
    every air vehicle nose-first along -Z)"""
    name = "hubt_shuttle"
    P = {name: Part(name)}
    s = P[name]
    box(s, LIGHT, (0, 0, 0), (6.0, 5.0, 26.0))                         # fuselage
    box(s, HULL, (0, -0.4, -14.5), (5.0, 3.6, 3.0))                    # the nose
    box(s, HULL, (0, -0.5, 3.0), (20.0, 0.8, 7.0))                     # the wing
    box(s, HULL, (0, 3.2, 11.0), (0.8, 3.0, 4.0))                      # the fin
    for sx in (-4.0, 4.0):
        box(s, STEEL, (sx, -0.5, 10.5), (2.2, 2.2, 5.0))               # the engines
        if lod < 2:
            box(s, FLOOD, (sx, -0.5, 13.02), (1.6, 1.6, 0.04))         # their glow
    if lod < 2:
        for sx in (-3.01, 3.01):
            box(s, WARM, (sx, 1.0, -2.0), (0.04, 1.0, 18.0))           # the cabin window band
        box(s, COOL, (0, 0.6, -15.99), (3.2, 1.2, 0.04))               # the cockpit
    return P


ASSETS = {
    "hubt_car_a": lambda lod: car_parts(lod, "hubt_car_a", "eq_car_red"),
    "hubt_car_b": lambda lod: car_parts(lod, "hubt_car_b", "eq_car_blue"),
    "hubt_car_c": lambda lod: car_parts(lod, "hubt_car_c", "eq_car_white"),
    "hubt_truck": truck_parts,
    "hubt_train": train_parts,
    "hubt_shuttle": shuttle_parts,
}
HITS = {k: (lambda k=k: []) for k in ASSETS}
ROOTS = {k: "hub" for k in ASSETS}
PASSPORTS = {
    "hubt_car_a": ("Легковой автомобиль (красный)", "hub_module", "Городской трафик (tools/models/units/hub_traffic.py)."),
    "hubt_car_b": ("Легковой автомобиль (синий)", "hub_module", "Городской трафик (tools/models/units/hub_traffic.py)."),
    "hubt_car_c": ("Легковой автомобиль (белый)", "hub_module", "Городской трафик (tools/models/units/hub_traffic.py)."),
    "hubt_truck": ("Грузовик с 40-футовым кузовом", "hub_module", "Городской трафик (tools/models/units/hub_traffic.py)."),
    "hubt_train": ("Поезд монорельса, 3 вагона", "hub_module", "Городской трафик (tools/models/units/hub_traffic.py)."),
    "hubt_shuttle": ("Пассажирский челнок 30 м", "hub_module", "Воздушный трафик города (tools/models/units/hub_traffic.py)."),
}
