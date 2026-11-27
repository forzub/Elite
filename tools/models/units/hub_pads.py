"""Hub city landing pads (author 2026-11-21: «каждому модулю города посадочную площадку - на свободных участках, на
крышах, на ножках, как сейчас делают вертолётные площадки на платформах, можно - как гараж на 1-2 места»; «сгенерируй
площадки, что бы у нас был полный фарш»; «добавь площадки в модули в мешэдиторе и также их строящийся вид и
повреждённый»).

Frame of every module: metres, +Y up, the origin is the TOUCHDOWN point's footprint centre on the surface the module
stands on (a roof for hubl_pad_roof, the city plane for the others). The game places them at the HubPlan pads
(src/game/navigation/HubPlanCatalog.cpp: Roof - on a tall wide building's roof; Stilts - a pad on legs 25 m up on a free
plot; Garage - an apron at the plane with a 1 / 2 place hangar behind it, toward -Z).

  hubl_pad_roof        a 36 m roof pad: deck on a low frame, H, touchdown ring, edge lights, safety nets, hatch
  hubl_pad_stilts      the same deck 25 m up on four lattice legs, a stair tower, a beacon mast
  hubl_garage1         a 1-place small-craft hangar (door +Z) with its marked apron
  hubl_garage2         a 2-place one
Construction stages (the kit "pad" / "padstilts" / "garage" of hub_expansion, hubc_* like tools/models/units/
hub_construction.py): s1 foundation and starter frame, s2 bare frame, s3 decked / clad with screens and scaffolds.
Damaged variants <module>_dmg_missile / _dmg_laser / _dmg_gutted (src/game/simulation/CityDamage.h): a blast hole and
a burnt ring, a long burn cut, a wreck (the deck collapsed, a leg gone, the hangar roof caved in).
"""
import math
import random as _random
import zlib

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_belt import CONT, GRATE, MARK  # noqa: F401
from units.hub_construction import CONC, NET, PLY, PRIM, REBAR, TARP, YEL  # noqa: F401 (registers the materials)
import units.hub_damage  # noqa: F401 (registers eq_hub_burnt / eq_hub_soot / eq_hub_rebar)

BURNT, SOOT = "eq_hub_burnt", "eq_hub_soot"

PAD = 36.0            # the deck side (a Cobrett-class ship: 26 x 5 m with room round it)
DECK_T = 1.2          # the deck plate
ROOF_FRAME = 1.0      # the roof pad's low frame under the deck
STILT_H = 25.0        # the stilts pad's deck top over the plane
# a landing PLACE is a square of PLACE_SIDE (author 2026-11-27: «максимальный размер кораблей того же класса, что и кобра,
# + 30 %»): the Cobra's class - Cobrett-I and the Cobra Mk1 - is 26 m at its widest -> 33.8, rounded to 34 m
# (HubPlanPads.h HubBerthPlaceSideMeters). A garage is 1 / 2 places wide, its apron and its hangar one place deep.
PLACE_SIDE = 34.0
G1_W, G2_W, G_D, G_H = PLACE_SIDE, 2.0 * PLACE_SIDE, PLACE_SIDE, 11.0   # the hangars: width (1 / 2 places), depth, height
APRON = PLACE_SIDE    # the garage apron depth (its centre is the module origin)


# ================================================================ shared pieces
def deck(p, y, lod, holes=None, burnt=None):
    """the pad deck top at y; holes: a predicate (x, z) -> True for a missing 4 m tile; burnt: (x, z) -> True"""
    h = PAD / 2
    if holes is None and burnt is None:
        box(p, HULL, (0, y - DECK_T / 2, 0), (PAD, DECK_T, PAD))
    else:
        n = 9
        t = PAD / n
        for i in range(n):
            for j in range(n):
                x, z = -h + (i + 0.5) * t, -h + (j + 0.5) * t
                if holes and holes(x, z):
                    continue
                box(p, BURNT if burnt and burnt(x, z) else HULL, (x, y - DECK_T / 2, z), (t, DECK_T, t))
    if lod >= 3:
        return

    def ok(x, z):
        return not (holes and holes(x, z))

    # the yellow edge band, the touchdown ring and the H
    for s in (-1, 1):
        for k in range(6):
            a = -h + (k + 0.5) * PAD / 6
            if ok(a, s * (h - 0.6)):
                box(p, HAZ, (a, y + 0.05, s * (h - 0.6)), (PAD / 6 - 0.4, 0.1, 1.0))
            if ok(s * (h - 0.6), a):
                box(p, HAZ, (s * (h - 0.6), y + 0.05, a), (1.0, 0.1, PAD / 6 - 0.4))
    ring = 24 if lod < 2 else 12
    for k in range(ring):
        a = 2 * math.pi * k / ring
        x, z = 12.0 * math.cos(a), 12.0 * math.sin(a)
        if ok(x, z):
            box(p, MARK, (x, y + 0.05, z), (3.0, 0.1, 0.8), rot_y(-a))
    for x, z, sx, sz in ((-3.5, 0, 1.0, 9.0), (3.5, 0, 1.0, 9.0), (0, 0, 6.0, 1.0)):
        if ok(x, z):
            box(p, LIGHT, (x, y + 0.06, z), (sx, 0.12, sz))
    # edge lights (green) and the corner beacons
    if lod < 2:
        for s in (-1, 1):
            for k in range(5):
                a = -h + 3.0 + k * (PAD - 6.0) / 4
                for x, z in ((a, s * (h - 0.3)), (s * (h - 0.3), a)):
                    if ok(x, z):
                        box(p, FLOOD, (x, y + 0.25, z), (0.5, 0.5, 0.5))
    for sx in (-1, 1):
        for sz in (-1, 1):
            if ok(sx * (h - 0.8), sz * (h - 0.8)):
                beacon(p, (sx * (h - 0.8), y + 0.8, sz * (h - 0.8)), 0.8)


def nets(p, y, lod, skip=None):
    """safety nets on outriggers round the deck (grating sloping out and down)"""
    if lod >= 2:
        return
    h = PAD / 2
    for s in (-1, 1):
        for ax in (0, 1):
            for k in range(3):
                a = -h + (k + 0.5) * PAD / 3
                c = (a, y - 1.2, s * (h + 1.4)) if ax == 0 else (s * (h + 1.4), y - 1.2, a)
                if skip and skip(c[0], c[2]):
                    continue
                R = rot_x(s * 0.35) if ax == 0 else rot_z(-s * 0.35)
                box(p, GRATE, c, (PAD / 3 - 0.6, 0.2, 2.6) if ax == 0 else (2.6, 0.2, PAD / 3 - 0.6), R)


def roof_frame(p, lod):
    """the roof pad's low frame: beams on the roof under the deck, the access hatch and its rail"""
    h = PAD / 2
    for k in range(5):
        a = -h + 2.0 + k * (PAD - 4.0) / 4
        box(p, STEEL, (a, ROOF_FRAME / 2, 0), (0.8, ROOF_FRAME, PAD - 2.0))
    box(p, HULL, (h + 3.0, 1.2, -h + 4.0), (4.0, 2.4, 4.0))                     # the access hatch kiosk
    box(p, DARK, (h + 1.0 - 0.02, 1.2, -h + 4.0), (0.05, 1.8, 1.2))
    if lod < 2:
        box(p, STEEL, (h + 1.0, ROOF_FRAME + DECK_T + 0.5, -h + 4.0), (0.2, 1.0, 4.0))


def lattice_leg(p, mat, x, z, y0, y1, lod, w=1.6):
    box(p, mat, (x, (y0 + y1) / 2, z), (w, y1 - y0, w))


def stilts_legs(p, top, lod, missing=(), bent=None, frame=True):
    """four legs and their bracing under a deck whose top is at `top`; missing: leg indices gone"""
    h = PAD / 2 - 3.0
    legs = [(-h, -h), (h, -h), (h, h), (-h, h)]
    for i, (x, z) in enumerate(legs):
        if i in missing:
            box(p, BURNT, (x, 2.0, z), (2.2, 4.0, 2.2))                  # a torn stump
            continue
        if bent is not None and i == bent:
            cyl(p, BURNT, (x, 0.0, z), (x * 0.8, top - DECK_T - 6.0, z * 1.3), 0.9, 6)
            continue
        lattice_leg(p, STEEL, x, z, 0.0, top - DECK_T, lod)
        box(p, CONC, (x, 0.6, z), (3.4, 1.2, 3.4))                          # the footing
    if lod >= 3:
        return
    for k in range(4):                                                      # the X bracing between neighbours
        if k in missing or (k + 1) % 4 in missing or k == bent or (k + 1) % 4 == bent:
            continue
        (x0, z0), (x1, z1) = legs[k], legs[(k + 1) % 4]
        for y0, y1 in ((1.5, 12.0), (12.0, top - DECK_T - 1.0)):
            cyl(p, STEEL, (x0, y0, z0), (x1, y1, z1), 0.35, 6)
            cyl(p, STEEL, (x1, y0, z1), (x0, y1, z0), 0.35, 6)
    if frame:   # the deck's under-frame (gone with the deck in a wreck)
        box(p, STEEL, (0, top - DECK_T - 0.6, 0), (PAD - 2.0, 1.2, 1.0))
        box(p, STEEL, (0, top - DECK_T - 0.6, 0), (1.0, 1.2, PAD - 2.0))


def stair_tower(p, top, lod, burnt=False):
    """a stair tower beside the deck (+X side) with a short bridge onto it"""
    m = BURNT if burnt else STEEL
    x0 = PAD / 2 + 4.0
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(p, m, (x0 + sx * 2.0, top / 2, sz * 2.0), (0.5, top, 0.5))
    if lod < 3:
        n = int(top / 3.0)
        for k in range(1, n + 1):
            box(p, GRATE, (x0, k * 3.0, 0.0), (4.6, 0.2, 4.6))
    box(p, GRATE, (PAD / 2 + 1.0, top - 0.1, 0.0), (2.2, 0.2, 2.0))
    if lod < 2:
        beacon(p, (x0, top + 6.0, 0.0), 0.9)
        box(p, m, (x0, top + 2.8, 0.0), (0.3, 5.6, 0.3))


def hangar(p, w, lod, caved=False, door_burnt=False, burnt_side=False):
    """the hangar behind the apron (-Z): walls, a ribbed roof, the door frame with its lights; w = its width"""
    z0, z1 = -APRON / 2, -APRON / 2 - G_D
    zc = (z0 + z1) / 2
    wall = BURNT if burnt_side else HULL
    box(p, HULL, (0, G_H / 2, z1 + 0.4), (w, G_H, 0.8))                     # the back wall
    for s in (-1, 1):
        box(p, wall if s > 0 else HULL, (s * (w / 2 - 0.4), G_H / 2, zc), (0.8, G_H, G_D))
    if caved:
        box(p, BURNT, (0, 2.0, zc), (w - 2.0, 4.0, G_D - 2.0))                # the roof fallen in
        box(p, BURNT, (-w / 4, G_H - 2.0, zc - 3.0), (w / 2, 0.8, G_D / 2), rot_z(0.5))
    else:
        box(p, HULL, (0, G_H - 0.4, zc), (w, 0.8, G_D))
        if lod < 3:
            for k in range(int(G_D / 3.0)):                                 # roof ribs
                box(p, STEEL, (0, G_H + 0.2, z1 + 1.5 + k * 3.0), (w, 0.4, 0.4))
    # the door: frame, a half-open roll-up shutter, the bay lights
    box(p, STEEL, (0, G_H - 1.0, z0), (w, 2.0, 0.6))
    for s in (-1, 1):
        box(p, STEEL, (s * (w / 2 - 0.6), G_H / 2, z0), (1.2, G_H, 0.6))
    box(p, BURNT if door_burnt else LIGHT, (0, G_H - 3.2, z0 + 0.1), (w - 2.4, 2.4, 0.2))
    if lod < 2:
        places = 2 if w > 30.0 else 1
        for k in range(places):
            x = 0.0 if places == 1 else (-w / 4 if k == 0 else w / 4)
            box(p, FLOOD, (x, G_H - 1.6, z0 - 2.0), (6.0, 0.3, 0.6))           # the bay floodlight
            box(p, DARK, (x, 0.05, zc), (8.0, 0.1, G_D - 4.0))                 # the bay floor (dark)
        beacon(p, (w / 2 - 0.6, G_H + 0.8, z0), 0.8)
        beacon(p, (-w / 2 + 0.6, G_H + 0.8, z0), 0.8)


def apron(p, w, lod, holes=None):
    """the marked apron in front of the door (the module origin at its centre)"""
    box(p, HULL, (0, -0.15, 0), (w, 0.3, APRON))
    if lod >= 3:
        return
    places = 2 if w > 30.0 else 1
    for k in range(places):
        x = 0.0 if places == 1 else (-w / 4 if k == 0 else w / 4)
        if holes and holes(x, 0.0):
            continue
        for zz in (-APRON / 2 + 1.0, APRON / 2 - 1.0):
            box(p, HAZ, (x, 0.05, zz), (8.0, 0.1, 0.6))
        box(p, MARK, (x, 0.05, 0.0), (0.6, 0.1, APRON - 2.0))                # the lead-in line
    for s in (-1, 1):
        box(p, HAZ, (s * (w / 2 - 0.4), 0.05, 0.0), (0.6, 0.1, APRON))
    if lod < 2:
        for s in (-1, 1):
            for k in range(4):
                box(p, FLOOD, (s * (w / 2 - 0.4), 0.3, -APRON / 2 + 2.5 + k * 5.0), (0.5, 0.5, 0.5))




def cargo_lift(p, top, lod, ground):
    """the cargo lift at the deck's -X edge (author 2026-11-21: «на самом краю площадки - грузовой лифт: кобра таскает всё
    контейнерами, приземлилась - что-то должно приехать и забрать / отдать контейнеры»): a lift shaft frame down to
    `ground`, its platform level with the deck carrying a container, a hazard rim, the lift's lamp"""
    x0 = -PAD / 2 - 4.0
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(p, STEEL, (x0 + sx * 3.4, (ground + top + 3.0) / 2, sz * 3.4), (0.5, top + 3.0 - ground, 0.5))
    # the SHAFT the platform runs in (author 2026-11-22: «грузовые лифты на площадках не имеют собственно шахт - как лифт
    # будет подниматься / опускаться?»): three walls down to `ground` (open toward the deck - the container rolls off
    # there), the two guide rails the car runs on, the landing at the bottom with its door frame; on a roof pad the
    # shaft goes on down into the building (ground below the roof)
    h = top - 0.6 - ground
    if h > 0.5:
        ymid = ground + h / 2
        box(p, HULL, (x0 - 3.55, ymid, 0), (0.25, h, 7.1))                          # the back wall
        for sz in (-1, 1):
            box(p, HULL, (x0, ymid, sz * 3.55), (7.1, h, 0.25))                     # the side walls
            box(p, STEEL, (x0 - 2.8, ymid, sz * 2.9), (0.3, h, 0.3))                # a guide rail
        if lod < 2:
            # bands round the shaft every ~6 m (its frame), the landing slab and the door frame at the bottom
            y = ground + 6.0
            while y < top - 1.5:
                box(p, STEEL, (x0, y, 0), (7.4, 0.35, 7.4))
                y += 6.0
            box(p, GRATE, (x0, ground + 0.15, 0), (6.8, 0.3, 6.8))
            box(p, HAZ, (x0 + 3.45, ground + 3.2, 0), (0.3, 0.3, 6.2))             # the door head (open side)
    box(p, STEEL, (x0, top + 3.0, 0), (7.4, 0.5, 7.4))                     # the head frame (the hoist)
    box(p, HULL, (x0, top - 0.3, 0), (6.4, 0.6, 6.4))                       # the platform at the deck's level
    box(p, HAZ, (x0, top + 0.02, 0), (6.6, 0.06, 0.4))
    box(p, CONT[0], (x0, top + 1.3, 0), (2.4, 2.6, 6.0))                     # a container on it
    box(p, GRATE, (-PAD / 2 - 0.5, top - 0.1, 0), (1.6, 0.2, 6.0))           # the bridge onto the deck
    if lod < 2:
        box(p, FLOOD, (x0, top + 2.6, 0), (1.0, 0.3, 1.0))
        beacon(p, (x0 + 3.4, top + 3.6, 3.4), 0.5)
# ================================================================ finished modules
def pad_roof_parts(lod):
    P = {"hubl_pad_roof": Part("hubl_pad_roof")}
    p = P["hubl_pad_roof"]
    roof_frame(p, lod)
    deck(p, ROOF_FRAME + DECK_T, lod)
    nets(p, ROOF_FRAME + DECK_T, lod, skip=lambda x, z: x < 0 and abs(z) < 5.0)
    cargo_lift(p, ROOF_FRAME + DECK_T, lod, -8.0)
    return P


def pad_roof_hits(name="hubl_pad_roof"):
    top = ROOF_FRAME + DECK_T
    return [hbox(name, (0, top / 2, 0), (PAD, top, PAD)),
            hbox(name, (-PAD / 2 - 4.0, (top + 3.0 - 8.0) / 2, 0), (7.4, top + 3.0 + 8.0, 7.4))]   # the lift shaft


def pad_stilts_parts(lod):
    P = {"hubl_pad_stilts": Part("hubl_pad_stilts")}
    p = P["hubl_pad_stilts"]
    stilts_legs(p, STILT_H, lod)
    deck(p, STILT_H, lod)
    nets(p, STILT_H, lod, skip=lambda x, z: x < 0 and abs(z) < 5.0)
    stair_tower(p, STILT_H, lod)
    cargo_lift(p, STILT_H, lod, 0.0)
    return P


def pad_stilts_hits(name="hubl_pad_stilts"):
    return [hbox(name, (0, STILT_H - DECK_T / 2, 0), (PAD, DECK_T + 1.2, PAD)),
            hbox(name, (0, (STILT_H - DECK_T) / 2, 0), (PAD - 4.0, STILT_H - DECK_T, PAD - 4.0)),
            hbox(name, (PAD / 2 + 4.0, STILT_H / 2, 0), (5.0, STILT_H, 5.0)),
            hbox(name, (-PAD / 2 - 4.0, (STILT_H + 3.0) / 2, 0), (7.4, STILT_H + 3.0, 7.4))]   # the lift shaft


def garage_builder(name, w):
    def parts(lod):
        P = {name: Part(name)}
        p = P[name]
        apron(p, w, lod)
        hangar(p, w, lod)
        return P
    return parts


def garage_hits_for(name, w):
    def hits():
        return [hbox(name, (0, -0.15, 0), (w, 0.3, APRON)),
                hbox(name, (0, G_H / 2, -APRON / 2 - G_D / 2), (w, G_H + 1.0, G_D))]
    return hits


# ================================================================ construction stages (hubc_*)
def pad_stage_builder(name, kind, stage):
    """kind: roof / stilts / garage1 / garage2; stage 1..3"""
    def parts(lod):
        P = {name: Part(name)}
        p = P[name]
        rng = _random.Random(zlib.crc32(f"{name}:{stage}".encode()))
        if kind == "roof":
            base = 0.0
            if stage >= 1:
                for k in range(5):                                          # the starter beams on the roof
                    a = -PAD / 2 + 2.0 + k * (PAD - 4.0) / 4
                    box(p, PRIM, (a, ROOF_FRAME / 2, 0), (0.8, ROOF_FRAME, PAD - 2.0 if stage > 1 else PAD / 2))
            top = ROOF_FRAME + DECK_T
            base = ROOF_FRAME
        elif kind == "stilts":
            top = STILT_H
            legs = [(-PAD / 2 + 3.0, -PAD / 2 + 3.0), (PAD / 2 - 3.0, -PAD / 2 + 3.0), (PAD / 2 - 3.0, PAD / 2 - 3.0),
                    (-PAD / 2 + 3.0, PAD / 2 - 3.0)]
            for x, z in legs:
                box(p, CONC, (x, 0.6, z), (3.4, 1.2, 3.4))
                h = (6.0 if stage == 1 else STILT_H - DECK_T)
                box(p, PRIM, (x, h / 2, z), (1.6, h, 1.6))
                if stage == 1:
                    for k in range(4):                                      # starter bars out of the footings
                        box(p, REBAR, (x + (k % 2 - 0.5) * 2.0, 7.5, z + (k // 2 - 0.5) * 2.0), (0.2, 3.0, 0.2))
            base = STILT_H - DECK_T - 1.2
            if stage >= 2:
                box(p, PRIM, (0, base + 0.6, 0), (PAD - 2.0, 1.2, 1.0))
                box(p, PRIM, (0, base + 0.6, 0), (1.0, 1.2, PAD - 2.0))
                # the scaffold tower where the stair will be
                x0 = PAD / 2 + 4.0
                for sx in (-1, 1):
                    for sz in (-1, 1):
                        box(p, YEL, (x0 + sx * 2.0, top / 2, sz * 2.0), (0.3, top, 0.3))
                if lod < 3:
                    for k in range(1, int(top / 4.0)):
                        box(p, PLY, (x0, k * 4.0, 0.0), (4.4, 0.15, 4.4))
        else:
            w = G1_W if kind == "garage1" else G2_W
            box(p, CONC, (0, -0.15, 0), (w, 0.3, APRON if stage > 1 else APRON * 0.6))
            z0, z1 = -APRON / 2, -APRON / 2 - G_D
            box(p, CONC, (0, 0.3, (z0 + z1) / 2), (w + 2.0, 0.6, G_D + 2.0))    # the slab
            if stage >= 2:
                for x in (-w / 2 + 0.5, 0.0, w / 2 - 0.5):                    # the portal frames
                    for z in (z0, (z0 + z1) / 2, z1):
                        box(p, PRIM, (x, G_H / 2, z), (0.6, G_H, 0.6))
                for z in (z0, (z0 + z1) / 2, z1):
                    box(p, PRIM, (0, G_H - 0.3, z), (w, 0.6, 0.6))
            if stage >= 3:
                box(p, HULL, (0, G_H / 2, z1 + 0.4), (w, G_H, 0.8))
                box(p, HULL, (-w / 2 + 0.4, G_H / 2, (z0 + z1) / 2), (0.8, G_H, G_D))
                box(p, TARP, (w / 2 - 0.4, G_H / 2, (z0 + z1) / 2), (0.4, G_H, G_D))   # the side still sheeted
                box(p, HULL, (0, G_H - 0.4, (z0 + z1) / 2 - G_D / 4), (w, 0.8, G_D / 2))
            if lod < 2:
                for k in range(3):                                          # material stacks
                    box(p, PLY, (w / 2 + 3.0, 0.6 + k * 1.2, z0 + 2.0 + rng.uniform(-1, 1)), (3.0, 1.0, 6.0))
            if lod < 2:
                beacon(p, (w / 2, G_H + 1.0 if stage >= 2 else 2.0, z0), 0.8)
            return P
        # roof / stilts: the deck by stage
        if stage >= 3:
            n = 9
            t = PAD / n
            for i in range(n):
                for j in range(n):
                    if (i + j) % 7 == 3 or j >= n - 2:                         # a few plates still missing
                        continue
                    box(p, HULL, (-PAD / 2 + (i + 0.5) * t, top - DECK_T / 2, -PAD / 2 + (j + 0.5) * t), (t, DECK_T, t))
            if lod < 2:
                for s in (-1, 1):                                           # the edge rails / climbing screens
                    box(p, NET, (s * (PAD / 2 + 0.6), top + 1.0, 0), (0.2, 2.0, PAD))
                    box(p, NET, (0, top + 1.0, s * (PAD / 2 + 0.6)), (PAD, 2.0, 0.2))
        elif stage >= 2 and kind == "roof":
            for k in range(5):
                a = -PAD / 2 + 2.0 + k * (PAD - 4.0) / 4
                box(p, PRIM, (0, base + 0.3, a), (PAD - 2.0, 0.6, 0.6))         # the cross beams of the deck grid
        if lod < 2:
            for k in range(2):
                box(p, PLY, (-PAD / 2 + 4.0 + k * 3.5, (0.6 if kind == "stilts" else base + 0.6), PAD / 2 + 6.0 if kind == "stilts" else PAD / 2 - 3.0),
                    (3.0, 1.2, 5.0))   # plates waiting (the stilts pad's: on the ground beside it)
            beacon(p, (PAD / 2 - 1.0, top + 1.0, PAD / 2 - 1.0), 0.8)
        return P
    return parts


def pad_stage_hits_for(name, kind, stage):
    def hits():
        if kind == "roof":
            return [hbox(name, (0, ROOF_FRAME / 2, 0), (PAD, ROOF_FRAME, PAD))]
        if kind == "stilts":
            h = 6.0 if stage == 1 else STILT_H
            return [hbox(name, (0, h / 2, 0), (PAD - 4.0, h, PAD - 4.0))]
        w = G1_W if kind == "garage1" else G2_W
        return [hbox(name, (0, G_H / 2 if stage >= 2 else 0.3, -APRON / 2 - G_D / 2),
                     (w + 2.0, G_H if stage >= 2 else 0.6, G_D + 2.0))]
    return hits


# ================================================================ damaged variants (<module>_dmg_<kind>)
def damage_masks(kind, seed):
    """(holes, burnt) predicates on the deck plane for a damage kind"""
    rng = _random.Random(seed)
    if kind == "missile":
        cx, cz = rng.uniform(-6, 6), rng.uniform(-6, 6)
        r = rng.uniform(6.0, 8.5)
        return (lambda x, z: (x - cx) ** 2 + (z - cz) ** 2 < r * r,
                lambda x, z: (x - cx) ** 2 + (z - cz) ** 2 < (r + 6.0) ** 2)
    if kind == "laser":
        a = rng.uniform(0, math.pi)
        dx, dz = math.cos(a), math.sin(a)
        off = rng.uniform(-5, 5)
        return (lambda x, z: abs(-dz * x + dx * z - off) < 1.6 and abs(dx * x + dz * z) < 14.0,
                lambda x, z: abs(-dz * x + dx * z - off) < 4.5)
    # gutted: most of the deck gone
    return (lambda x, z: not (x < -6 and z < 4), lambda x, z: True)


def dmg_roof_builder(name, kind):
    def parts(lod):
        P = {name: Part(name)}
        p = P[name]
        holes, burnt = damage_masks(kind, zlib.crc32(name.encode()))
        roof_frame(p, lod)
        deck(p, ROOF_FRAME + DECK_T, lod, holes, burnt)
        nets(p, ROOF_FRAME + DECK_T, lod, skip=holes if kind != "laser" else None)
        if kind != "laser":
            box(p, BURNT, (2.0, ROOF_FRAME + 0.8, 1.0), (6.0, 1.6, 3.0), rot_z(0.6))   # a plate thrown up on its edge
        return P
    return parts


def dmg_stilts_builder(name, kind):
    def parts(lod):
        P = {name: Part(name)}
        p = P[name]
        holes, burnt = damage_masks(kind, zlib.crc32(name.encode()))
        if kind == "gutted":
            stilts_legs(p, STILT_H, lod, missing=(1,), bent=2, frame=False)
            # the deck slid down onto the plane, tilted, half of it broken off
            R = rot_x(0.42)
            for i in range(5):
                for j in range(4):
                    box(p, BURNT if (i + j) % 3 == 0 else HULL, (-PAD / 2 + 4.0 + i * 7.0, 8.0 + j * 2.6, -PAD / 2 + 4.0 + j * 7.0),
                        (6.8, DECK_T, 6.8), R)
            stair_tower(p, STILT_H, lod, burnt=True)
        else:
            stilts_legs(p, STILT_H, lod, bent=0 if kind == "missile" else None)
            deck(p, STILT_H, lod, holes, burnt)
            nets(p, STILT_H, lod, skip=holes)
            stair_tower(p, STILT_H, lod, burnt=kind == "missile")
        return P
    return parts


def dmg_garage_builder(name, w, kind):
    def parts(lod):
        P = {name: Part(name)}
        p = P[name]
        holes, _ = damage_masks(kind, zlib.crc32(name.encode()))
        apron(p, w, lod, holes=holes if kind != "gutted" else None)
        hangar(p, w, lod, caved=kind == "gutted", door_burnt=kind != "laser", burnt_side=kind != "missile")
        if kind == "missile":
            box(p, BURNT, (w / 4, 0.1, 0.0), (10.0, 0.2, 10.0))                   # the scorch on the apron
        return P
    return parts


# ================================================================ the repair hangar (author 2026-11-22: «модель - кубик с доком,
# подставить под здание; широкий заезд, чтобы корабль мог влететь. Для ремонтной мастерской: большой ангар с несколькими
# ремонтными местами, имитация подъёмного оборудования, верстаки со шкафчиками, побитые запчасти вдоль стен»)
# A box RH_W x RH_D x RH_H, the door the whole front (+Z) RH_DOOR_W x RH_DOOR_H; RH_BAYS bays side by side, each with
# its marked floor, a scissor lift and a gantry crane under the roof; benches + lockers along the back wall, wrecked
# parts along the side walls. The hit volumes are the shell only (floor, roof, walls, the lintel): a ship flies in.
RH_W, RH_D, RH_H = 110.0, 60.0, 26.0
RH_DOOR_W, RH_DOOR_H = 100.0, 22.0
RH_BAYS = 2                     # the wide one (author 2026-11-22: «делай ангар пошире» - ships up to ~40 m)
RH_BAY_X = 26.5                 # the bays' centres left / right (HubPlanPads.h HubHangarBayX)
RH_T = 0.8                      # wall / roof thickness


def repair_hangar_parts(lod):
    """a SHIPYARD repair hall, not a tyre shop (author 2026-11-22: «выглядит как шиномонтаж»): per bay a cradle of hull
    supports with service towers (scaffolds, platforms, hoses) both sides, a full-span gantry crane, welding screens and
    floodlights; an engine stripped on its stand, tool carts, a glazed control booth over the door, wall ribs, floor
    grating strips, light bands"""
    name = "hubl_repair_hangar"
    P = {name: Part(name)}
    p = P[name]
    rng = _random.Random(20261123)
    zb = -RH_D / 2
    zf = RH_D / 2
    # ---- the shell
    box(p, CONC, (0, -0.25, 0), (RH_W + 2.0, 0.5, RH_D + 2.0))
    box(p, HULL, (0, RH_H - RH_T / 2, 0), (RH_W, RH_T, RH_D))
    box(p, HULL, (0, RH_H / 2, zb + RH_T / 2), (RH_W, RH_H, RH_T))
    jw = (RH_W - RH_DOOR_W) / 2
    for sx in (-1, 1):
        box(p, HULL, (sx * (RH_W / 2 - RH_T / 2), RH_H / 2, 0), (RH_T, RH_H, RH_D))
        box(p, STEEL, (sx * (RH_W / 2 - jw / 2), RH_H / 2, zf - RH_T / 2), (jw, RH_H, RH_T))
    box(p, STEEL, (0, (RH_DOOR_H + RH_H) / 2, zf - RH_T / 2), (RH_DOOR_W, RH_H - RH_DOOR_H, RH_T))
    box(p, HAZ, (0, RH_DOOR_H - 0.4, zf + 0.05), (RH_DOOR_W, 0.6, 0.1))
    if lod < 2:
        # the wall ribs (the frame), the roof trusses, the light bands
        for i in range(-6, 7):
            box(p, STEEL, (i * RH_W / 13, RH_H / 2, zb + RH_T + 0.3), (0.7, RH_H, 0.6))
            box(p, STEEL, (i * RH_W / 13, RH_H - RH_T - 0.8, 0), (0.7, 1.6, RH_D - 1.0))
        for sx in (-1, 1):
            for k in range(-3, 4):
                box(p, STEEL, (sx * (RH_W / 2 - RH_T - 0.3), RH_H / 2, k * RH_D / 7), (0.6, RH_H, 0.7))
        for k in (-1, 1):
            box(p, FLOOD, (k * RH_W / 4, RH_H - RH_T - 1.8, 0), (1.0, 0.25, RH_D - 6.0))
        # the glazed control booth over the door, inside
        box(p, HULL, (0, RH_DOOR_H + 1.6, zf - 4.0), (14.0, 3.2, 5.0))
        box(p, WARM, (0, RH_DOOR_H + 1.8, zf - 6.55), (12.0, 1.6, 0.1))
    # ---- the bays
    for b in range(RH_BAYS):
        cx = -RH_BAY_X if b == 0 else RH_BAY_X
        # the floor: hazard edges, a grating strip down the middle
        for sx in (-1, 1):
            box(p, HAZ, (cx + sx * 22.0, 0.03, 2.0), (0.5, 0.06, RH_D - 8.0))
        box(p, GRATE, (cx, 0.04, 2.0), (6.0, 0.08, RH_D - 12.0))
        # the cradle: four hull supports (padded arms) the ship sits on
        for dz in (-10.0, 10.0):
            for dx in (-6.0, 6.0):
                box(p, STEEL, (cx + dx, 0.9, 2.0 + dz), (1.2, 1.8, 1.2))
                box(p, YEL, (cx + dx, 1.9, 2.0 + dz), (2.0, 0.3, 2.0))
        # the service towers both sides: a scaffold with three platforms, a hose down to the bay
        for sx in (-1, 1):
            tx = cx + sx * 17.0
            for dz in (-12.0, 0.0, 12.0):
                box(p, STEEL, (tx, 8.0, 2.0 + dz), (0.4, 16.0, 0.4))
            for y in (5.0, 10.0, 15.0):
                box(p, GRATE, (tx - sx * 1.2, y, 2.0), (3.2, 0.3, 25.0))
                if lod < 2:
                    box(p, HAZ, (tx - sx * 2.8, y + 0.6, 2.0), (0.1, 0.9, 25.0))   # the railing
            if lod < 2:
                cyl(p, DARK, (tx - sx * 2.6, 15.0, 2.0 + rng.uniform(-8, 8)), (cx + sx * 4.0, 3.0, 2.0 + rng.uniform(-6, 6)),
                    0.22, 6)   # a hose to the hull
                box(p, FLOOD, (tx - sx * 2.0, 16.5, 2.0), (0.6, 0.6, 0.6))
        # welding screens (yellow-green curtains on frames) beside one end of the bay
        if lod < 2:
            for k in range(3):
                box(p, YEL, (cx + rng.uniform(-8, 8), 1.4, zb + 9.0 + k * 1.6), (3.0, 2.8, 0.08))
    # ---- the full-span gantry crane (two runways on the side walls, the bridge across the hall, the trolley, the hook)
    for sx in (-1, 1):
        box(p, STEEL, (sx * (RH_W / 2 - 2.0), RH_H - 4.0, 0), (1.0, 1.2, RH_D - 2.0))
    bz = 6.0
    box(p, YEL, (0, RH_H - 4.8, bz), (RH_W - 4.0, 1.6, 2.0))
    box(p, YEL, (RH_BAY_X * 0.6, RH_H - 6.2, bz), (3.0, 1.6, 2.6))
    if lod < 2:
        box(p, STEEL, (RH_BAY_X * 0.6, RH_H - 12.0, bz), (0.18, 10.0, 0.18))
        box(p, DARK, (RH_BAY_X * 0.6, RH_H - 17.5, bz), (1.4, 1.2, 1.4))
    # ---- the middle aisle: an engine stripped on its stand, tool carts
    cyl(p, BURNT, (0, 2.6, zb + 14.0), (0, 2.6, zb + 20.0), 2.2, 12)          # the engine casing on its side
    box(p, STEEL, (0, 0.6, zb + 17.0), (3.0, 1.2, 7.0))                        # its stand
    if lod < 2:
        cyl(p, SOOT, (0, 2.6, zb + 20.0), (0, 2.6, zb + 22.5), 1.4, 12)       # the nozzle, off
        for k in range(4):
            x = rng.uniform(-8.0, 8.0)
            z = rng.uniform(zb + 24.0, zf - 12.0)
            box(p, PRIM, (x, 0.55, z), (1.6, 1.1, 0.9))                       # a tool cart
    # ---- along the back wall: benches with their lockers; along the side walls: wrecked parts
    x = -RH_W / 2 + 3.0
    while x < RH_W / 2 - 9.5:
        box(p, STEEL, (x + 2.0, 0.9, zb + 1.6), (4.0, 0.15, 1.6))
        if lod < 2:
            for lx in (0.2, 3.8):
                box(p, STEEL, (x + lx, 0.45, zb + 1.6), (0.15, 0.9, 1.4))
            box(p, DARK, (x + 1.0, 1.2, zb + 1.6), (0.8, 0.5, 0.6))
        for k in range(3):
            box(p, PRIM if (k + int(x)) % 2 else HULL, (x + 4.6 + k * 0.7, 1.0, zb + 0.9), (0.62, 2.0, 0.6))
        x += 7.0
    for sx in (-1, 1):
        z = zb + 4.0
        while z < zf - 8.0:
            kind = rng.random()
            xw = sx * (RH_W / 2 - 3.0 - rng.uniform(0.0, 1.5))
            if kind < 0.35:
                r = rng.uniform(0.6, 1.4)
                cyl(p, BURNT, (xw, r, z), (xw + sx * 0.2, r, z + rng.uniform(1.5, 3.0)), r, 10)
            elif kind < 0.7:
                box(p, SOOT, (xw, 1.2, z), (0.2, rng.uniform(1.5, 2.6), rng.uniform(2.0, 4.0)), R=rot_z(sx * rng.uniform(0.15, 0.45)))
            else:
                cyl(p, HULL, (xw, 0.0, z), (xw, rng.uniform(1.6, 2.8), z), rng.uniform(0.6, 1.0), 10)
            z += rng.uniform(3.0, 5.5)
    return P


def repair_hangar_hits(name="hubl_repair_hangar"):
    zb = -RH_D / 2
    jw = (RH_W - RH_DOOR_W) / 2
    out = [hbox(name, (0, -0.25, 0), (RH_W + 2.0, 0.5, RH_D + 2.0)),
           hbox(name, (0, RH_H - RH_T / 2, 0), (RH_W, RH_T, RH_D)),
           hbox(name, (0, RH_H / 2, zb + RH_T / 2), (RH_W, RH_H, RH_T)),
           hbox(name, (0, (RH_DOOR_H + RH_H) / 2, RH_D / 2 - RH_T / 2), (RH_DOOR_W, RH_H - RH_DOOR_H, RH_T))]
    for sx in (-1, 1):
        out.append(hbox(name, (sx * (RH_W / 2 - RH_T / 2), RH_H / 2, 0), (RH_T, RH_H, RH_D)))
        out.append(hbox(name, (sx * (RH_W / 2 - jw / 2), RH_H / 2, RH_D / 2 - RH_T / 2), (jw, RH_H, RH_T)))
    return out


# ================================================================ the kit
KIT = {
    "hubl_repair_hangar": (repair_hangar_parts, repair_hangar_hits,
                           "Ремонтный ангар на 2 места (корабли до ~40 м): ложементы, сервисные башни, кран-балка, заезд 100 м"),
    "hubl_pad_roof": (pad_roof_parts, pad_roof_hits, "Посадочная площадка на крыше 36 м"),
    "hubl_pad_stilts": (pad_stilts_parts, pad_stilts_hits, "Посадочная площадка на опорах 36 м, 25 м над плоскостью"),
    "hubl_garage1": (garage_builder("hubl_garage1", G1_W), garage_hits_for("hubl_garage1", G1_W), "Гараж на 1 корабль с площадкой"),
    "hubl_garage2": (garage_builder("hubl_garage2", G2_W), garage_hits_for("hubl_garage2", G2_W), "Гараж на 2 корабля с площадкой"),
}
STAGES = {"roof": ("hubc_padroof", 3), "stilts": ("hubc_padstilts", 3), "garage1": ("hubc_garage1", 3),
          "garage2": ("hubc_garage2", 3)}
for _kind, (_prefix, _n) in STAGES.items():
    for _s in range(1, _n + 1):
        _name = f"{_prefix}_s{_s}"
        KIT[_name] = (pad_stage_builder(_name, _kind, _s), pad_stage_hits_for(_name, _kind, _s),
                      f"Площадка ({_kind}) в постройке, стадия {_s} из {_n}")
for _kind in ("missile", "laser", "gutted"):
    _ru = {"missile": "ракета", "laser": "лазер", "gutted": "разрушена"}[_kind]
    _n = f"hubl_pad_roof_dmg_{_kind}"
    KIT[_n] = (dmg_roof_builder(_n, _kind), (lambda n=_n: pad_roof_hits(n)), f"Площадка на крыше, повреждена ({_ru})")
    _n = f"hubl_pad_stilts_dmg_{_kind}"
    KIT[_n] = (dmg_stilts_builder(_n, _kind), (lambda n=_n: pad_stilts_hits(n)), f"Площадка на опорах, повреждена ({_ru})")
    for _g, _w in (("hubl_garage1", G1_W), ("hubl_garage2", G2_W)):
        _n = f"{_g}_dmg_{_kind}"
        KIT[_n] = (dmg_garage_builder(_n, _w, _kind), garage_hits_for(_n, _w), f"Гараж ({_g[-1]} мест.), повреждён ({_ru})")

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Посадочные площадки города хаба (tools/models/units/hub_pads.py).")
             for k, v in KIT.items()}
