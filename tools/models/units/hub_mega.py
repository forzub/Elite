"""Hub megastructure prototype (author 2026-11-16: «да, начинай с мегаструктуры. самое плохое в этом - нет ощущения
расстояния. интересно - получилось бы сделать мегаструктуру на десятки км? может сделать ее более тесной, что бы
чувствовался размер?»).

A dense city on a big belt asteroid: a deck of 600 m tiles on the rock, a grid of shafts (160 x 160 m, stacked 1 km
segments, up to 8 km tall - the skyline peaks in the middle) 300 m apart - 140 m canyons between walls kilometres high,
bridges across the canyons, annexes stepping out of the walls, pipe bundles along the canyon floors, the small kit of
hub_district.py (tank farms, factories, lattice towers) as the ruler; an industrial quarter of factories on the deck at
one end (vent haze sources). The walls are a facade material (eq_hub_facade*): the shader draws their windows, most
buildings dim, one in ten lit up. The hub origin sits in a canyon crossing 1.5 km above the deck (the dev start puts the
ship there). Modules: <assets_source>/models/hub/hubm_*, assembly hub_proto_mega (viewed as node boxes in the editor).
"""
import math
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, FLOOD, HULL, LIGHT, STEEL, beacon, rock_builder, FACTORY_VENTS  # noqa: F401

register_materials({
    "eq_hub_facade": ((0.22, 0.23, 0.25), (0.20, 0.20, 0.22), 40),
    "eq_hub_facade_light": ((0.40, 0.41, 0.42), (0.20, 0.20, 0.22), 40),
})
FACADE, FACADE_L = "eq_hub_facade", "eq_hub_facade_light"

SPACING, SHAFT_W, SEG_H = 300.0, 160.0, 1000.0
DECK_Y = -1500.0                   # the deck's top surface (hub frame); the origin is a canyon crossing above it


def shaft_parts(lod):
    P = {"hubm_shaft": Part("hubm_shaft")}
    s = P["hubm_shaft"]
    w, h = SHAFT_W, SEG_H
    rng = random.Random(41)
    box(s, FACADE, (0, h / 2, 0), (w, h, w))
    for sx in (-1, 1):
        for sz in (-1, 1):                                                        # corner pilasters
            box(s, LIGHT, (sx * (w / 2 - 3.0), h / 2, sz * (w / 2 - 3.0)), (14.0, h, 14.0))
            if lod < 2:
                for y in (250.0, 500.0, 750.0, 995.0):
                    beacon(s, (sx * (w / 2 + 4.6), y, sz * (w / 2 + 4.6)), 3.0)
    for y in (250.0, 500.0, 750.0):                                               # setback ledges
        box(s, HULL, (0, y, 0), (w + 10.0, 5.0, w + 10.0))
    if lod < 2:
        for face in range(4):                                                     # vent boxes / service hatches
            nx, nz = ((1, 0), (-1, 0), (0, 1), (0, -1))[face]
            for k in range(6 if lod == 0 else 3):
                y = rng.uniform(40.0, h - 40.0)
                t = rng.uniform(-w / 3, w / 3)
                c = (nx * (w / 2 + 2.5) + (t if nz else 0.0), y, nz * (w / 2 + 2.5) + (t if nx else 0.0))
                box(s, HULL, c, (5.0 if nx else 18.0, 10.0, 5.0 if nz else 18.0))
    return P


def shaft_hits():
    return [hbox("hubm_shaft", (0, SEG_H / 2, 0), (SHAFT_W + 10.0, SEG_H, SHAFT_W + 10.0))]


def cap_parts(lod):
    P = {"hubm_cap": Part("hubm_cap")}
    c = P["hubm_cap"]
    box(c, FACADE_L, (0, 30.0, 0), (130.0, 60.0, 130.0))
    box(c, HULL, (0, 64.0, 0), (90.0, 8.0, 90.0))
    cyl(c, STEEL, (0, 68.0, 0), (0, 220.0, 0), 2.5, 8 if lod < 2 else 6)
    beacon(c, (0, 222.0, 0), 5.0)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(c, (sx * 66.0, 61.0, sz * 66.0), 3.0)
                box(c, FLOOD, (sx * 45.0, 69.0, sz * 45.0), (6.0, 2.0, 6.0))
    return P


def cap_hits():
    return [hbox("hubm_cap", (0, 34.0, 0), (130.0, 68.0, 130.0)), hcyl("hubm_cap", (0, 68.0, 0), (0, 222.0, 0), 3.0)]


def annex_parts(lod):
    """a block stepping out of a shaft wall: origin on the wall, out along +Z, Y up"""
    P = {"hubm_annex": Part("hubm_annex")}
    a = P["hubm_annex"]
    box(a, FACADE, (0, 65.0, 27.0), (110.0, 130.0, 54.0))
    box(a, HULL, (0, 131.5, 27.0), (114.0, 3.0, 58.0))
    if lod < 2:
        box(a, LIGHT, (30.0, 140.0, 20.0), (20.0, 15.0, 16.0))                   # roof plant
        beacon(a, (-50.0, 134.0, 52.0), 2.0)
    return P


def annex_hits():
    return [hbox("hubm_annex", (0, 66.5, 27.0), (114.0, 133.0, 58.0))]


def bridge_parts(lod):
    """an enclosed bridge across a canyon: from the origin along +Z, 140 m"""
    P = {"hubm_bridge": Part("hubm_bridge")}
    b = P["hubm_bridge"]
    L = SPACING - SHAFT_W
    box(b, FACADE_L, (0, 0, L / 2), (18.0, 12.0, L))
    if lod < 2:
        for x in (-7.0, 7.0):
            cyl(b, STEEL, (x, -9.0, 0), (x, -9.0, L), 1.0, 6)
        for k in range(0, int(L) + 1, 35):
            cyl(b, STEEL, (-7.0, -9.0, k), (7.0, -9.0, k), 0.6, 4)
        for z in (2.0, L - 2.0):
            beacon(b, (9.5, 6.5, z), 1.2)
    return P


def bridge_hits():
    return [hbox("hubm_bridge", (0, -2.0, (SPACING - SHAFT_W) / 2), (18.0, 16.0, SPACING - SHAFT_W))]


def deck_parts(lod):
    """a deck tile 600 x 600 m, top surface at the origin"""
    P = {"hubm_deck": Part("hubm_deck")}
    d = P["hubm_deck"]
    box(d, HULL, (0, -20.0, 0), (600.0, 40.0, 600.0))
    if lod < 2:
        for k in (-200.0, 0.0, 200.0):                                            # ribs underneath
            box(d, STEEL, (k, -52.0, 0), (12.0, 24.0, 600.0))
            box(d, STEEL, (0, -52.0, k), (600.0, 24.0, 12.0))
    if lod == 0:
        for x in range(-250, 251, 100):
            for z in range(-250, 251, 100):
                box(d, FLOOD, (x, 0.3, z), (2.0, 0.6, 2.0))                           # deck lamps
    return P


def deck_hits():
    return [hbox("hubm_deck", (0, -32.0, 0), (600.0, 64.0, 600.0))]


def pipe_parts(lod):
    """a bundle of three pipes 300 m along +Z on brackets"""
    P = {"hubm_pipe": Part("hubm_pipe")}
    p = P["hubm_pipe"]
    seg = {0: 12, 1: 8, 2: 6}[lod]
    for x, y in ((-12.0, 6.0), (0.0, 6.0), (12.0, 6.0)):
        cyl(p, STEEL if x else LIGHT, (x, y, 0), (x, y, 300.0), 5.0, seg)
    if lod < 2:
        for z in range(0, 301, 50):
            box(p, HULL, (0, 1.0, z), (34.0, 2.0, 3.0))
    return P


def pipe_hits():
    return [hbox("hubm_pipe", (0, 6.0, 150.0), (34.0, 12.0, 300.0))]


KIT = {"hubm_shaft": (shaft_parts, shaft_hits, "Ствол-башня 160x160x1000 (сегмент)"),
       "hubm_cap": (cap_parts, cap_hits, "Венец ствола с мачтой"),
       "hubm_annex": (annex_parts, annex_hits, "Пристройка на стене ствола"),
       "hubm_bridge": (bridge_parts, bridge_hits, "Мост через ущелье 140 м"),
       "hubm_deck": (deck_parts, deck_hits, "Настил 600x600"),
       "hubm_pipe": (pipe_parts, pipe_hits, "Трубный пучок 300 м"),
       "hubm_rock": (rock_builder(4000.0, 23, "hubm_rock"), lambda: [hcyl("hubm_rock", (0, -3800.0, 0), (0, 3800.0, 0), 3800.0)],
                     "Астероид-основание 8 км")}


# ---------------------------------------------------------------- the layout
def _yaw(dx, dz):
    """rotation_deg taking +Z to (dx, 0, dz)"""
    return (0.0, round(math.degrees(math.atan2(dx, dz)), 4) + 0.0, 0.0)


def mega_layout(seed=7, half=20):
    rng = random.Random(seed)
    N, haze = [("rock", "hubm_rock", (0.0, DECK_Y - 4000.0 + 260.0, 0.0), (0.0, 0.0, 0.0), "anchor", None)], []
    ext = half * SPACING
    # the deck: 600 m tiles over the footprint
    for i in range(-int(ext // 600), int(ext // 600)):
        for k in range(-int(ext // 600), int(ext // 600)):
            N.append((f"deck_{i}_{k}", "hubm_deck", (i * 600.0 + 300.0, DECK_Y, k * 600.0 + 300.0), (0.0, 0.0, 0.0), "deck", "rock"))
    heights = {}
    for i in range(-half, half):
        for k in range(-half, half):
            x, z = (i + 0.5) * SPACING, (k + 0.5) * SPACING
            r = math.hypot(x, z) / ext
            industrial = x > 0.45 * ext
            p = 0.92 - 0.55 * r if not industrial else 0.25
            if rng.random() > p:
                continue
            tall = max(1, int(round((8.0 * max(0.0, 1.0 - r) ** 1.6) * rng.uniform(0.55, 1.1))))
            if industrial:
                tall = rng.choice((1, 1, 2))
            if abs(x) < 400 and abs(z) < 400:
                tall = max(tall, 4)                                                # tall walls round the start
            heights[(i, k)] = tall
            for s in range(tall):
                N.append((f"sh_{i}_{k}_{s}", "hubm_shaft", (x, DECK_Y + s * SEG_H, z), (0.0, 0.0, 0.0), "shaft", "rock"))
            top = DECK_Y + tall * SEG_H
            N.append((f"cap_{i}_{k}", "hubm_cap", (x, top, z), (0.0, 0.0, 0.0), "cap", "rock"))
            # annexes and the small kit on the walls (the ruler)
            for f, (dx, dz) in enumerate(((1, 0), (-1, 0), (0, 1), (0, -1))):
                for m in range(rng.randint(0, 2 + tall)):
                    y = DECK_Y + rng.uniform(60.0, tall * SEG_H - 160.0)
                    t = rng.uniform(-35.0, 35.0)
                    wx = x + dx * SHAFT_W / 2 + (t if dz else 0.0)
                    wz = z + dz * SHAFT_W / 2 + (t if dx else 0.0)
                    u = rng.random()
                    if u < 0.55:
                        N.append((f"ax_{i}_{k}_{f}_{m}", "hubm_annex", (wx, y, wz), _yaw(dx, dz), "annex", "rock"))
                    elif u < 0.75:
                        # a tank farm clamped to the wall (its +Y out of the wall)
                        rot = {(1, 0): (0.0, 0.0, -90.0), (-1, 0): (0.0, 0.0, 90.0), (0, 1): (90.0, 0.0, 0.0), (0, -1): (-90.0, 0.0, 0.0)}[(dx, dz)]
                        N.append((f"tk_{i}_{k}_{f}_{m}", "hubk_tanks", (wx, y, wz), rot, "kit", "rock"))
                    elif u < 0.85:
                        rot = {(1, 0): (0.0, 0.0, -90.0), (-1, 0): (0.0, 0.0, 90.0), (0, 1): (90.0, 0.0, 0.0), (0, -1): (-90.0, 0.0, 0.0)}[(dx, dz)]
                        N.append((f"tw_{i}_{k}_{f}_{m}", "hubk_tower", (wx, y, wz), rot, "kit", "rock"))   # a mast sticking out
    # bridges across the canyons between neighbours
    for (i, k), tall in heights.items():
        for di, dk in ((1, 0), (0, 1)):
            o = heights.get((i + di, k + dk))
            if not o:
                continue
            for b in range(rng.randint(0, min(tall, o) + 1)):
                y = DECK_Y + rng.uniform(80.0, min(tall, o) * SEG_H - 80.0)
                x0 = (i + 0.5) * SPACING + di * SHAFT_W / 2
                z0 = (k + 0.5) * SPACING + dk * SHAFT_W / 2
                N.append((f"br_{i}_{k}_{di}{dk}_{b}", "hubm_bridge", (x0, y, z0), _yaw(di, dk), "bridge", "rock"))
    # pipe bundles along some canyon floors, the industrial quarter's factories on the deck (haze sources)
    for i in range(-half, half, 3):
        x = i * SPACING
        for k in range(-half, half - 1, 1):
            if rng.random() < 0.18:
                N.append((f"pp_{i}_{k}", "hubm_pipe", (x, DECK_Y, k * SPACING), (0.0, 0.0, 0.0), "pipe", "rock"))
    import pads
    for i in range(-half, half):
        for k in range(-half, half):
            x, z = (i + 0.5) * SPACING, (k + 0.5) * SPACING
            if x > 0.45 * ext and (i, k) not in heights and rng.random() < 0.7:
                yaw = rng.choice((0.0, 90.0, 180.0, 270.0))
                N.append((f"fc_{i}_{k}", "hubk_factory", (x, DECK_Y, z), (0.0, yaw, 0.0), "factory", "rock"))
                R = pads.euler((0.0, yaw, 0.0))
                haze += [{"position": [round(a + b, 1) for a, b in zip(apply(R, v), (x, DECK_Y, z))], "radius_m": 260.0,
                          "density": 1.0} for v in FACTORY_VENTS]
    return N, haze


MEGA_NODES, MEGA_HAZE = mega_layout()

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
ROOTS["hub_proto_mega"] = "hub"
PASSPORTS = {k: (v[2] + " (мегаструктура хаба)", "hub_module", "Модуль прототипа мегаструктуры хаба (tools/models/units/hub_mega.py).")
             for k, v in KIT.items()}
SETS = {"hub_proto_mega": {
    "title": "Прототип мегаструктуры хаба (сборка)",
    "description": f"Город на астероиде: {len(MEGA_NODES)} узлов по ссылкам, ущелья 140 м между стенами до 8 км; дымка у {len(MEGA_HAZE)} труб.",
    "nodes": MEGA_NODES,
    "view_mode": "boxes",
    "view_lods": {0: 1, 1: 2, 2: 2}}}
