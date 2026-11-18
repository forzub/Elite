"""City services of the planar hub (author 2026-11-18: «добавить генераторы. можно на квартал или несколько кварталов,
службы охраны - доки с полицаями и доки с мчс-никами» + «фермы везде обязательно, хотя бы в качестве мачт или опорных
столбов»).

Modules for the planar floating city (hub_plane.py; blocks 600 x 600 m, street plane y = 0, +Y up). Every module
stands on its apron slab with the street level at y = 0 (the slab hangs below it), so it can also be hung upside down
under the plane. Lattice trusses (truss() below - chords, ring frames, zig-zag diagonals) carry the masts, radiator
headers, deck columns and crane jibs of every module.

  hubs_power_plant  a block's power station 520 x 520 m: two fusion reactor domes (tokamak belts), the control
                    building, three radiator rows on lattice masts (the city's waste heat), coolant loops, a
                    transformer yard under a lattice portal gantry, beacons
  hubs_power_unit   a quarter's substation 160 x 120 m: one reactor dome, a radiator row, transformers, switchgear
  hubs_police_dock  a security station: the station block with a tower and a comms mast, a landing deck on lattice
                    columns with 3 pads for PC-30 patrol cutters, a hangar, blue / white lights
  hubs_rescue_dock  a rescue (МЧС) station: a big hangar with an open bay, 2 pads for rescue tugs, a lattice jib
                    crane, fuel and foam tanks, orange / white livery, red beacons
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_mega import FACADE, FACADE_L  # noqa: F401
from units.hub_belt import GRATE, MARK, RAD, _seg, bullet, lamp, road_strip  # noqa: F401
from units.hub_vista import FLARE

register_materials({
    "eq_hub_rad_panel": ((0.74, 0.76, 0.76), (0.45, 0.45, 0.45), 70),      # radiator panels: pale, high emissivity
    "eq_hub_reactor": ((0.58, 0.60, 0.62), (0.30, 0.30, 0.32), 50),        # reactor domes
    "eq_hub_police_blue": ((0.08, 0.18, 0.48), (0.30, 0.30, 0.35), 60),
    "eq_hub_police_white": ((0.82, 0.84, 0.86), (0.25, 0.25, 0.25), 40),
    "eq_hub_rescue_orange": ((0.85, 0.36, 0.06), (0.25, 0.22, 0.20), 40),
    "eq_hub_rescue_white": ((0.86, 0.85, 0.82), (0.25, 0.25, 0.25), 40),
    "eq_hub_insulator": ((0.48, 0.30, 0.20), (0.40, 0.40, 0.40), 80),      # ceramic insulators
})
PANEL, REACTOR, INSUL = "eq_hub_rad_panel", "eq_hub_reactor", "eq_hub_insulator"
PBLUE, PWHITE = "eq_hub_police_blue", "eq_hub_police_white"
RORANGE, RWHITE = "eq_hub_rescue_orange", "eq_hub_rescue_white"


# ---------------------------------------------------------------- small vector helpers
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _len(a): return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])
def _unit(a): return _mul(a, 1.0 / max(_len(a), 1e-9))


# ---------------------------------------------------------------- the lattice truss
def strut(p, mat, a, b, r):
    """a square-section member from a to b (one box, 12 triangles)"""
    d = _sub(b, a)
    L = _len(d)
    box(p, mat, _mul(_add(a, b), 0.5), (2 * r, 2 * r, L), R=axis_to(_unit(d)))


def truss(p, p0, p1, w, lod, mat=STEEL, bay=2.0):
    """a square lattice truss of side w from p0 to p1 (a Warren truss): 4 chords, zig-zag diagonals on the 4 faces, ring
    frames every other bay (LOD0); LOD1 doubles the bay length; the far LOD a single slender box"""
    d = _sub(p1, p0)
    L = _len(d)
    u = _unit(d)
    if lod >= 2:
        box(p, mat, _mul(_add(p0, p1), 0.5), (w * 0.8, w * 0.8, L), R=axis_to(u))
        return
    a = _unit(_cross(u, (0.0, 1.0, 0.0) if abs(u[1]) < 0.9 else (1.0, 0.0, 0.0)))
    b = _cross(u, a)
    h = w / 2
    cs = [_add(_mul(a, sx * h), _mul(b, sy * h)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    rc = max(0.2, w * 0.05)
    rb = rc * 0.6
    for c in cs:
        strut(p, mat, _add(p0, c), _add(p1, c), rc)
    n = max(1, int(round(L / (w * bay * (1 if lod == 0 else 2)))))
    pts = [_add(p0, _mul(d, k / n)) for k in range(n + 1)]
    for k, q in enumerate(pts):
        if k % 2 == 0 or k == n:
            for i in range(4):
                strut(p, mat, _add(q, cs[i]), _add(q, cs[(i + 1) % 4]), rb)
        if k < n:
            q1 = pts[k + 1]
            for i in range(4):
                c0, c1 = (cs[i], cs[(i + 1) % 4]) if k % 2 == 0 else (cs[(i + 1) % 4], cs[i])
                strut(p, mat, _add(q, c0), _add(q1, c1), rb)


def mast(p, x, z, y0, y1, w, lod, light=BEACON):
    """a vertical lattice mast with a base plate and a light on top"""
    truss(p, (x, y0, z), (x, y1, z), w, lod)
    box(p, STEEL, (x, y0 + 0.5, z), (w * 1.6, 1.0, w * 1.6))
    if lod < 3:
        box(p, STEEL, (x, y1 + 0.4, z), (w * 1.2, 0.8, w * 1.2))
        box(p, light, (x, y1 + 1.6, z), (max(1.2, w * 0.3),) * 3)


def ring(p, mat, x, z, r0, r1, y0, y1, seg):
    """a flat annulus / belt about a vertical axis (closed rectangular section)"""
    ylathe(p, mat, [(r0, y0), (r1, y0), (r1, y1), (r0, y1), (r0, y0)], seg, t=(x, 0, z))


def dome(p, mat, x, z, r, h_drum, h_cap, seg):
    """a reactor dome: a drum wall r up to h_drum, a spherical-ish cap h_cap above it (base open on the slab)"""
    n = max(4, seg // 4)
    prof = [(r, 0.0), (r, h_drum)]
    prof += [(r * math.cos(0.5 * math.pi * k / n), h_drum + h_cap * math.sin(0.5 * math.pi * k / n)) for k in range(1, n)]
    prof += [(0.0, h_drum + h_cap)]
    ylathe(p, mat, prof, seg, t=(x, 0, z), soft=40)


def reactor(p, x, z, r, h_drum, h_cap, lod, mast_h=0.0):
    """a fusion reactor hall: the dome, the tokamak magnet belt round it, the plasma-glow slit, ribs, a lattice mast"""
    seg = _seg(lod, 32)
    dome(p, REACTOR, x, z, r, h_drum, h_cap, seg)
    ring(p, STEEL, x, z, r - 0.5, r + r * 0.13, h_drum * 0.35, h_drum * 0.8, seg)          # the magnet belt
    if lod < 2:
        ring(p, COOL, x, z, r - 0.2, r + 0.4, h_drum * 0.88, h_drum * 0.95, seg)             # the plasma-glow slit
    if lod < 2:
        nr = 16 if lod == 0 else 8
        for k in range(nr):                                                                 # buttress ribs
            th = 2 * math.pi * k / nr
            c, s = math.cos(th), math.sin(th)
            box(p, STEEL, (x + c * (r + r * 0.16), h_drum * 0.4, z + s * (r + r * 0.16)), (3.0, h_drum * 0.8, 3.0),
                R=rot_y(-th))
        ring(p, STEEL, x, z, r * 0.25, r * 0.32, h_drum + h_cap - 2.0, h_drum + h_cap + 1.5, seg // 2)  # crown ring
    if mast_h > 0:
        mast(p, x, z, h_drum + h_cap - 1.0, h_drum + h_cap + mast_h, max(3.0, r * 0.06), lod)


def radiator_row(p, x, z0, z1, top, panels, lod, w=9.0, header_truss=True, mid_mast=True):
    """a row of vertical radiator panels (thin in X) hanging from a lattice header truss along Z, carried by lattice
    masts at the ends and in the middle; a coolant manifold along the header"""
    if header_truss:
        truss(p, (x, top, z0), (x, top, z1), w, lod)
    else:
        box(p, STEEL, (x, top, (z0 + z1) / 2), (w * 0.8, w * 0.6, z1 - z0 + w))                 # the box-girder header
    for z in ((z0, (z0 + z1) / 2, z1) if mid_mast else (z0, z1)):
        mast(p, x, z, 0.0, top - w / 2, w, lod)
    pw = (z1 - z0 - w * 2) / panels
    for k in range(panels):
        zc = z0 + w + pw * (k + 0.5)
        y0, y1 = 16.0, top - w / 2 - 2.0
        box(p, PANEL if k % 2 == 0 or lod >= 2 else RAD, (x, (y0 + y1) / 2, zc), (0.8, y1 - y0, pw - 4.0))
        if lod < 2:
            box(p, STEEL, (x, y1 + 0.6, zc), (2.0, 1.2, pw - 3.0))                          # header manifold
            box(p, STEEL, (x, y0 - 0.6, zc), (2.0, 1.2, pw - 3.0))                          # return manifold
            if lod == 0:
                for j in (-1, 1):                                                           # panel ribs
                    box(p, STEEL, (x, (y0 + y1) / 2, zc + j * (pw / 2 - 2.5)), (1.4, y1 - y0, 0.8))
                box(p, STEEL, (x, (y0 + y1) / 2, zc), (1.2, y1 - y0, 0.6))
    if lod < 2:
        cyl(p, STEEL, (x + w / 2 + 2.0, top - w / 2 - 1.0, z0), (x + w / 2 + 2.0, top - w / 2 - 1.0, z1), 1.6, 8)


def transformer(p, x, z, lod, s=1.0):
    """a power transformer: the tank, cooler fins, three bushings with insulators"""
    box(p, HULL, (x, 5.0 * s, z), (10.0 * s, 10.0 * s, 14.0 * s), chamfer=0.4 if lod == 0 else 0.0)
    if lod < 3:
        box(p, HAZ, (x, 10.3 * s, z), (10.2 * s, 0.6 * s, 14.2 * s))
    if lod < 2:
        for k in (-1, 0, 1):
            cyl(p, INSUL, (x, 10.0 * s, z + k * 4.0 * s), (x, 16.0 * s, z + k * 4.0 * s), 0.8 * s, 8)
            box(p, STEEL, (x, 16.5 * s, z + k * 4.0 * s), (1.2 * s, 1.0 * s, 1.2 * s))
        for sx in (-1, 1):
            box(p, STEEL, (x + sx * 6.2 * s, 5.0 * s, z), (2.4 * s, 8.0 * s, 12.0 * s))      # cooler banks


def portal(p, x, z0, z1, h, lod):
    """a lattice portal gantry along Z (two lattice legs, a lattice crossbeam) with the busbars hanging from it"""
    if lod >= 2:
        return
    w = 3.0
    for z in (z0, z1):
        truss(p, (x, 0.0, z), (x, h, z), w, lod)
    truss(p, (x, h + w / 2, z0 - w / 2), (x, h + w / 2, z1 + w / 2), w, lod)
    if lod < 2:
        for k in (-1, 0, 1):
            cyl(p, INSUL, (x + k * 3.0, h, (z0 + z1) / 2), (x + k * 3.0, h - 4.0, (z0 + z1) / 2), 0.5, 6)


def pipe_loop(p, pts, r, lod, mat=HULL):
    """a coolant pipe along a polyline with support trestles"""
    seg = 10 if lod == 0 else 6
    for a, b in zip(pts, pts[1:]):
        cyl(p, mat, a, b, r, seg, caps=False)
        if lod < 2:
            L = _len(_sub(b, a))
            for k in range(1, int(L // 30) + 1):
                q = _add(a, _mul(_sub(b, a), k * 30.0 / L))
                if q[1] > r + 1.0:
                    box(p, STEEL, (q[0], (q[1] - r) / 2, q[2]), (1.2, q[1] - r, r * 2.4))
    if lod < 3:
        for q in pts[1:-1]:
            box(p, STEEL, q, (r * 2.6, r * 2.6, r * 2.6))                                       # elbow casings


# ================================================================ hubs_power_plant: the block's power station
PP = 520.0                                  # slab side
PP_R, PP_DRUM, PP_CAP = 58.0, 34.0, 62.0    # reactor dome radius / drum / cap heights
PP_RX = (-125.0, -125.0)
PP_RZ = (-125.0, 125.0)
PP_RAD_X = (70.0, 150.0, 230.0)
PP_RAD_TOP = 180.0


def power_plant_parts(lod):
    name = "hubs_power_plant"
    P = {name: Part(name)}
    p = P[name]
    H = PP / 2
    box(p, HULL, (0, -3.0, 0), (PP, 6.0, PP))                                               # the apron slab
    road_strip(p, -H + 4.0, -H + 24.0, 0.0, -H, H, lod)                                      # the access road on -X
    # two fusion reactor halls with their lattice masts
    for x, z in zip(PP_RX, PP_RZ):
        reactor(p, x, z, PP_R, PP_DRUM, PP_CAP, lod, mast_h=50.0)
    # the converter / control building between them: facade block, the control room band, a lattice antenna mast
    box(p, FACADE_L, (-125.0, 28.0, 0), (64.0, 56.0, 80.0))
    box(p, LIGHT, (-125.0, 57.0, 0), (68.0, 2.0, 84.0))
    box(p, FACADE, (-125.0, 70.0, -14.0), (36.0, 24.0, 30.0))                                # control room storey
    if lod < 2:
        for sx in (-1, 1):
            box(p, WARM, (-125.0 + sx * 18.05, 72.0, -14.0), (0.2, 6.0, 26.0))              # lit control room windows
        box(p, WARM, (-125.0, 72.0, -29.05), (32.0, 6.0, 0.2))
        for k in range(6):                                                                   # roof vents
            box(p, STEEL, (-140.0 + (k % 3) * 15.0, 60.5, 18.0 + (k // 3) * 14.0), (8.0, 5.0, 8.0))
    mast(p, -125.0, -14.0, 82.0, 150.0, 4.0, lod)
    if lod < 2:
        for y in (110.0, 135.0):
            box(p, STEEL, (-125.0, y, -14.0), (18.0, 0.8, 0.8))                              # antenna cross-arms
    # the converter halls next to each reactor (towards the radiators)
    for z in PP_RZ:
        box(p, LIGHT, (-35.0, 18.0, z), (50.0, 36.0, 70.0))
        if lod < 2:
            for k in range(5):
                box(p, WARM if k % 2 else COOL, (-9.9, 12.0, z - 28.0 + k * 14.0), (0.2, 3.0, 9.0))
    # coolant loops: reactor -> converter hall -> header along Z -> radiator rows
    if lod < 2:
        for z in PP_RZ:
            for dy, r in ((10.0, 3.0), (20.0, 3.0)):
                pipe_loop(p, [(-125.0 + PP_R + 6.0, dy, z + (8.0 if dy > 15 else -8.0)), (-60.0, dy, z + (8.0 if dy > 15 else -8.0))], r, lod)
                pipe_loop(p, [(-10.0, dy, z + (8.0 if dy > 15 else -8.0)), (30.0, dy, z + (8.0 if dy > 15 else -8.0)),
                              (30.0, dy, 0.0)], r, lod)
        pipe_loop(p, [(30.0, 10.0, -H + 30.0), (30.0, 10.0, H - 30.0)], 4.0, lod)            # the supply header
        pipe_loop(p, [(36.0, 20.0, -H + 30.0), (36.0, 20.0, H - 30.0)], 4.0, lod)            # the return header
        for x in PP_RAD_X:
            for zz, dy in ((-H + 30.0, 10.0), (H - 30.0, 20.0)):
                pipe_loop(p, [(30.0 if dy < 15 else 36.0, dy, zz), (x, dy, zz), (x, 14.0, zz)], 2.5, lod)
    # three radiator rows (the city's waste heat goes out here)
    for x in PP_RAD_X:
        radiator_row(p, x, -H + 22.0, H - 22.0, PP_RAD_TOP, 8 if lod < 2 else 4, lod, header_truss=False, mid_mast=False)
    # the transformer yard on the far -Z / +Z edges west of the domes, lattice portal gantries over the bays
    for sz in (-1, 1):
        zc = sz * (H - 26.0)
        for k in range(4):
            transformer(p, -210.0 + k * 28.0, zc, lod)
        if lod < 3:
            portal(p, -140.0, zc - 16.0, zc + 16.0, 26.0, lod)
        if lod < 2:
            box(p, GRATE, (-150.0, 0.15, zc), (130.0, 0.3, 40.0))                           # the yard's gravel grating
            for k in range(4):
                lamp(p, -215.0 + k * 40.0, 0.0, zc + sz * 22.0, 10.0)
    # the switchgear hall on the -X road side
    box(p, LIGHT, (-215.0, 12.0, 0), (30.0, 24.0, 110.0))
    if lod < 2:
        for k in range(6):
            box(p, DARK, (-230.05, 8.0, -45.0 + k * 18.0), (0.2, 12.0, 10.0))
    # beacons on the slab corners and the radiator row ends
    if lod < 3:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(p, (sx * (H - 2.0), 1.5, sz * (H - 2.0)), 2.5)
    return P


def power_plant_hits():
    n = "hubs_power_plant"
    H = PP / 2
    v = [hbox(n, (0, -3.0, 0), (PP, 6.0, PP)),
         hbox(n, (-125.0, 41.0, 0), (64.0, 82.0, 80.0)),
         hbox(n, (-215.0, 12.0, 0), (30.0, 24.0, 110.0))]
    for x, z in zip(PP_RX, PP_RZ):
        v.append(hcyl(n, (x, 0.0, z), (x, PP_DRUM, z), PP_R + PP_R * 0.13))
        v.append(hcyl(n, (x, PP_DRUM, z), (x, PP_DRUM + PP_CAP, z), PP_R, PP_R * 0.3))
    for z in PP_RZ:
        v.append(hbox(n, (-35.0, 18.0, z), (50.0, 36.0, 70.0)))
    for x in PP_RAD_X:
        v.append(hbox(n, (x, PP_RAD_TOP / 2 + 3.0, 0), (12.0, PP_RAD_TOP + 6.0, PP - 36.0)))
    return v


# ================================================================ hubs_power_unit: a quarter's substation
PU_W, PU_D = 160.0, 120.0
PU_R, PU_DRUM, PU_CAP = 28.0, 18.0, 30.0
PU_RAD_X, PU_RAD_TOP = 52.0, 82.0


def power_unit_parts(lod):
    name = "hubs_power_unit"
    P = {name: Part(name)}
    p = P[name]
    box(p, HULL, (0, -2.0, 0), (PU_W, 4.0, PU_D))
    reactor(p, -40.0, -8.0, PU_R, PU_DRUM, PU_CAP, lod, mast_h=20.0)
    # radiator row on +X: one header truss along Z, 4 panels
    radiator_row(p, PU_RAD_X, -PU_D / 2 + 6.0, PU_D / 2 - 6.0, PU_RAD_TOP, 4 if lod < 2 else 2, lod, w=5.0)
    # transformers between the dome and the radiators, under a lattice portal
    for k in range(3):
        transformer(p, 12.0, -34.0 + k * 22.0, lod, 0.8)
    if lod < 2:
        portal(p, 26.0, -40.0, 20.0, 20.0, lod)
        pipe_loop(p, [(-40.0 + PU_R + 4.0, 8.0, -8.0), (PU_RAD_X - 8.0, 8.0, -8.0), (PU_RAD_X - 8.0, 8.0, 48.0),
                      (PU_RAD_X, 8.0, 48.0)], 2.0, lod)
    # switchgear hall and the control kiosk with lit windows
    box(p, LIGHT, (-35.0, 7.0, 46.0), (70.0, 14.0, 20.0))
    box(p, FACADE_L, (-68.0, 9.0, -44.0), (18.0, 18.0, 24.0))
    if lod < 2:
        box(p, WARM, (-58.95, 12.0, -44.0), (0.2, 4.0, 18.0))
        for k in range(5):
            box(p, DARK, (-62.0 + k * 13.0, 6.0, 35.95), (8.0, 8.0, 0.2))                   # switchgear doors
        box(p, GRATE, (12.0, 0.15, -12.0), (24.0, 0.3, 70.0))
        lamp(p, -10.0, 0.0, -54.0)
        lamp(p, 30.0, 0.0, 54.0)
    if lod < 3:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(p, (sx * (PU_W / 2 - 1.5), 1.2, sz * (PU_D / 2 - 1.5)), 1.5)
    return P


def power_unit_hits():
    n = "hubs_power_unit"
    return [hbox(n, (0, -2.0, 0), (PU_W, 4.0, PU_D)),
            hcyl(n, (-40.0, 0.0, -8.0), (-40.0, PU_DRUM, -8.0), PU_R * 1.13),
            hcyl(n, (-40.0, PU_DRUM, -8.0), (-40.0, PU_DRUM + PU_CAP, -8.0), PU_R, PU_R * 0.3),
            hbox(n, (-35.0, 7.0, 46.0), (70.0, 14.0, 20.0)),
            hbox(n, (-68.0, 9.0, -44.0), (18.0, 18.0, 24.0)),
            hbox(n, (12.0, 8.0, -12.0), (14.0, 16.0, 60.0)),
            hbox(n, (PU_RAD_X, PU_RAD_TOP / 2, 0), (7.0, PU_RAD_TOP + 3.0, PU_D - 6.0))]


# ================================================================ small craft parked on the pads
def cutter(p, x, y, z, lod, yaw=0.0):
    """a PC-30 patrol cutter (34 m, nose to -Z), police livery"""
    R = rot_y(yaw)

    def b(mat, c, s):
        box(p, mat, _add((x, y, z), apply(R, c)), s, R=R)
    b(PWHITE, (0, 3.2, 1.0), (8.0, 4.4, 26.0))
    b(PBLUE, (0, 2.6, -14.5), (6.0, 3.4, 5.0))                                               # the nose
    b(PBLUE, (0, 3.2, 4.0), (22.0, 0.8, 8.0))                                                # stub wings
    if lod < 2:
        b(PBLUE, (0, 3.6, 1.0), (8.1, 0.9, 26.1))                                            # the livery band
        b(COOL, (0, 4.6, -12.5), (4.0, 1.4, 3.0))                                            # the canopy
        b(STEEL, (0, 0.6, 1.0), (5.0, 1.2, 18.0))                                            # skids
        for sx in (-1, 1):
            b(STEEL, (sx * 2.6, 3.0, 15.5), (2.2, 2.2, 3.0))                                 # engines
            b(COOL if sx < 0 else FLOOD, (sx * 2.0, 5.6, -2.0), (1.2, 0.6, 1.2))              # the light bar
    return p


def tug(p, x, y, z, lod, yaw=0.0):
    """a rescue tug (30 m, nose to -Z): a boxy hull, a cabin, a front grapple frame, rescue livery"""
    R = rot_y(yaw)

    def b(mat, c, s):
        box(p, mat, _add((x, y, z), apply(R, c)), s, R=R)
    b(RORANGE, (0, 4.0, 2.0), (12.0, 7.0, 22.0))
    b(RWHITE, (0, 8.6, -5.0), (8.0, 3.0, 8.0))                                               # the cabin
    b(STEEL, (0, 4.0, -11.0), (14.0, 2.0, 2.0))                                              # the grapple frame
    if lod < 2:
        b(RWHITE, (0, 4.0, 2.0), (12.1, 1.6, 22.1))                                          # the livery band
        b(COOL, (0, 9.0, -9.05), (6.0, 1.6, 0.2))
        b(STEEL, (0, 0.6, 2.0), (8.0, 1.2, 18.0))
        for sx in (-1, 1):
            b(STEEL, (sx * 6.0, 4.0, -15.0), (1.4, 1.4, 8.0))                                 # grapple arms
            b(HAZ, (sx * 6.0, 4.0, -19.2), (2.2, 2.2, 1.0))
            b(STEEL, (sx * 4.0, 4.0, 14.5), (3.0, 3.0, 3.0))                                  # engines
            b(BEACON if sx < 0 else FLARE, (sx * 2.5, 10.4, -5.0), (1.0, 0.6, 1.0))
    return p


def pad(p, x, y, z, s, ring_mat, lod, lights=(FLOOD, FLOOD)):
    """a square landing pad of side s on a deck top y: plate, a marking ring and cross, edge lights"""
    box(p, LIGHT, (x, y + 0.1, z), (s, 0.2, s))
    if lod < 3:
        seg = 32 if lod == 0 else 16
        ring(p, ring_mat, x, z, s * 0.36, s * 0.42, y + 0.2, y + 0.3, seg)
    if lod < 2:
        box(p, MARK, (x, y + 0.25, z), (s * 0.5, 0.1, 2.0))
        box(p, MARK, (x, y + 0.25, z), (2.0, 0.1, s * 0.5))
        for k in range(8):
            th = 2 * math.pi * k / 8 + math.pi / 8
            r = s * 0.48
            box(p, lights[k % 2], (x + r * math.cos(th), y + 0.5, z + r * math.sin(th)), (0.8, 0.6, 0.8))


def tank(p, mat, x, z, r, h, seg, lod):
    """a vertical bullet tank on the slab (a plain cylinder on the far LOD)"""
    if lod >= 2:
        cyl(p, mat, (x, 0.0, z), (x, h + 2 * r, z), r, 8)
    else:
        bullet(p, mat, x, z, 0.0, r, h, seg)


# ================================================================ hubs_police_dock: the security station
PD_W, PD_D = 240.0, 200.0
PD_DECK_Y = 16.0
PD_DECK_X0, PD_DECK_X1 = 22.0, 118.0
PD_PADS_Z = (-62.0, 0.0, 62.0)


def police_dock_parts(lod):
    name = "hubs_police_dock"
    P = {name: Part(name)}
    p = P[name]
    box(p, HULL, (0, -3.0, 0), (PD_W, 6.0, PD_D))
    road_strip(p, -118.0, -104.0, 0.0, -98.0, 98.0, lod)                                     # the street access on -X
    # the station block (120 x 80, x -100..20? -> -100..10 x z -98..-18) - white facade, blue bands
    bx, bz, bw, bd, bh = -45.0, -58.0, 110.0, 80.0, 40.0
    box(p, FACADE_L, (bx, bh / 2, bz), (bw, bh, bd))
    box(p, PWHITE, (bx, bh + 1.0, bz), (bw + 2.0, 2.0, bd + 2.0))
    box(p, PBLUE, (bx, 6.0, bz), (bw + 0.6, 3.0, bd + 0.6))                                  # the blue ground band
    if lod < 3:
        box(p, PBLUE, (bx, bh - 2.0, bz), (bw + 0.6, 2.0, bd + 0.6))                         # the blue cornice band
    if lod < 2:
        box(p, DARK, (bx - 20.0, 3.5, bz + bd / 2 + 0.2), (24.0, 7.0, 0.4))                  # the entrance
        box(p, COOL, (bx - 20.0, 8.0, bz + bd / 2 + 1.5), (26.0, 0.6, 3.0))                  # lit canopy
        for k in range(5):                                                                    # roof plant
            box(p, STEEL, (bx - 40.0 + k * 18.0, bh + 4.0, bz + 20.0), (10.0, 6.0, 10.0))
    # the command tower on the block's -X corner: blue glass band, a lattice comms mast, a dish
    tx, tz = -84.0, -82.0
    box(p, PWHITE, (tx, 30.0, tz), (24.0, 60.0, 24.0))
    box(p, COOL, (tx, 54.0, tz), (24.6, 5.0, 24.6))                                          # the lit ops room band
    box(p, PBLUE, (tx, 61.0, tz), (26.0, 2.0, 26.0))
    mast(p, tx, tz, 62.0, 105.0, 4.0, lod, light=COOL)
    if lod < 2:
        for y, L in ((80.0, 16.0), (95.0, 10.0)):
            box(p, STEEL, (tx, y, tz), (L, 0.8, 0.8))
            box(p, STEEL, (tx, y, tz), (0.8, 0.8, L))
        ylathe(p, LIGHT, [(0.0, 0.0), (4.0, 1.0), (6.0, 3.0), (5.6, 3.2), (0.0, 0.4)], 16, t=(tx + 9.0, 64.0, tz + 9.0))
        cyl(p, STEEL, (tx + 9.0, 62.0, tz + 9.0), (tx + 9.0, 64.5, tz + 9.0), 0.6, 6)
    # the hangar on +Z of the block: a hall with the door opening onto the deck
    hx, hz, hw, hd, hh = -45.0, 48.0, 110.0, 96.0, 34.0
    box(p, LIGHT, (hx, hh / 2, hz), (hw, hh, hd))
    box(p, PBLUE, (hx, hh + 0.6, hz), (hw + 1.0, 1.2, hd + 1.0))
    box(p, DARK, (hx + hw / 2 + 0.2, PD_DECK_Y + 7.0, hz), (0.4, 14.0, 70.0))               # the hangar door
    if lod < 3:
        box(p, HAZ, (hx + hw / 2 + 0.3, PD_DECK_Y + 15.0, hz), (0.4, 1.2, 72.0))
    if lod < 2:
        for k in range(6):
            box(p, STEEL, (hx - hw / 2 + 10.0 + k * 18.0, hh + 2.0, hz), (2.0, 2.0, hd))      # roof ribs
        for k in range(4):
            box(p, FLOOD, (hx + hw / 2 + 1.0, PD_DECK_Y + 16.5, hz - 30.0 + k * 20.0), (1.0, 1.0, 2.0))
    # the landing deck on lattice columns, 3 pads, the cutters
    dx = (PD_DECK_X0 + PD_DECK_X1) / 2
    dw, dd = PD_DECK_X1 - PD_DECK_X0, PD_D - 8.0
    box(p, HULL, (dx, PD_DECK_Y - 1.5, 0), (dw, 3.0, dd))
    if lod < 3:
        box(p, PBLUE, (dx, PD_DECK_Y - 1.5, 0), (dw + 0.4, 1.0, dd + 0.4))                    # deck edge stripe
    for cx in (PD_DECK_X0 + 6.0, PD_DECK_X1 - 6.0):
        for cz in (-dd / 2 + 6.0, -dd / 6, dd / 6, dd / 2 - 6.0):
            truss(p, (cx, 0.0, cz), (cx, PD_DECK_Y - 3.0, cz), 4.0, lod, bay=1.5)
    if lod < 3:
        truss(p, (PD_DECK_X0 + 6.0, PD_DECK_Y - 4.5, -dd / 2 + 6.0), (PD_DECK_X0 + 6.0, PD_DECK_Y - 4.5, dd / 2 - 6.0),
              5.0, lod)                                                                      # deck girders
        truss(p, (PD_DECK_X1 - 6.0, PD_DECK_Y - 4.5, -dd / 2 + 6.0), (PD_DECK_X1 - 6.0, PD_DECK_Y - 4.5, dd / 2 - 6.0),
              5.0, lod)
    for k, z in enumerate(PD_PADS_Z):
        pad(p, dx + 4.0, PD_DECK_Y, z, 54.0, PBLUE, lod, lights=(COOL, FLOOD))
        if lod < 3 and k != 1:
            cutter(p, dx + 4.0, PD_DECK_Y + 0.2, z, lod, yaw=0.0)
    if lod < 2:
        for sz in (-1, 1):                                                                   # railings on the long edges
            box(p, STEEL, (dx, PD_DECK_Y + 1.0, sz * (dd / 2 - 0.5)), (dw, 0.3, 0.3))
        box(p, GRATE, (PD_DECK_X0 - 4.0, PD_DECK_Y - 0.5, 48.0), (8.0, 1.0, 70.0))          # the apron to the hangar
    # floodlight masts on the deck corners (lattice), blue / white beacons
    for sz in (-1, 1):
        mast(p, PD_DECK_X1 - 3.0, sz * (dd / 2 - 3.0), PD_DECK_Y, PD_DECK_Y + 24.0, 2.0, lod,
             light=COOL if sz < 0 else FLOOD)
    if lod < 3:
        for k, (x, z) in enumerate(((bx - bw / 2, bz - bd / 2), (bx + bw / 2, bz - bd / 2), (bx + bw / 2, bz + bd / 2),
                                    (hx - hw / 2, hz + hd / 2), (hx + hw / 2, hz + hd / 2))):
            box(p, COOL if k % 2 == 0 else FLOOD, (x, (bh if k < 3 else hh) + 2.6, z), (2.0, 2.0, 2.0))
    if lod < 2:
        for z in (-90.0, -45.0, 0.0, 45.0, 90.0):
            lamp(p, -100.0, 0.0, z)
    return P


def police_dock_hits():
    n = "hubs_police_dock"
    dx = (PD_DECK_X0 + PD_DECK_X1) / 2
    return [hbox(n, (0, -3.0, 0), (PD_W, 6.0, PD_D)),
            hbox(n, (-45.0, 20.0, -58.0), (110.0, 42.0, 80.0)),
            hbox(n, (-84.0, 31.0, -82.0), (26.0, 62.0, 26.0)),
            hbox(n, (-45.0, 17.5, 48.0), (110.0, 35.0, 96.0)),
            hbox(n, (dx, PD_DECK_Y - 1.5, 0), (PD_DECK_X1 - PD_DECK_X0, 3.0, PD_D - 8.0)),
            hbox(n, (dx, (PD_DECK_Y - 3.0) / 2, 0), (PD_DECK_X1 - PD_DECK_X0, PD_DECK_Y - 3.0, PD_D - 8.0))]


# ================================================================ hubs_rescue_dock: the rescue (МЧС) station
RD_W, RD_D = 240.0, 200.0
RH_X0, RH_X1, RH_Z0, RH_Z1, RH_H = -112.0, 0.0, -96.0, 14.0, 46.0       # the hangar box (open on +X)
RD_PADS = ((62.0, -50.0), (62.0, 46.0))


def rescue_dock_parts(lod):
    name = "hubs_rescue_dock"
    P = {name: Part(name)}
    p = P[name]
    box(p, HULL, (0, -3.0, 0), (RD_W, 6.0, RD_D))
    # the big hangar: back wall, side walls, roof - the +X face an open bay
    t = 3.0
    hcx, hcz = (RH_X0 + RH_X1) / 2, (RH_Z0 + RH_Z1) / 2
    hw, hd = RH_X1 - RH_X0, RH_Z1 - RH_Z0
    box(p, RWHITE, (RH_X0 + t / 2, RH_H / 2, hcz), (t, RH_H, hd))                            # back wall
    for z in (RH_Z0 + t / 2, RH_Z1 - t / 2):
        box(p, RWHITE, (hcx, RH_H / 2, z), (hw, RH_H, t))                                    # side walls
        box(p, RORANGE, (hcx, RH_H * 0.6, z + (0.3 if z > hcz else -0.3) * 2), (hw + 0.2, 6.0, t))  # orange bands
    box(p, RWHITE, (hcx, RH_H + 2.0, hcz), (hw + 2.0, 4.0, hd + 2.0))                         # the roof
    box(p, RORANGE, (RH_X1 + 0.5, RH_H - 1.0, hcz), (1.6, 6.0, hd + 2.0))                     # the bay lintel
    box(p, GRATE, (hcx, 0.15, hcz), (hw - 2 * t, 0.3, hd - 2 * t))                            # the hangar floor
    if lod < 3:
        for k in range(3):                                                                   # roof trusses (lattice)
            x = RH_X0 + 12.0 + k * (hw - 24.0) / 2
            truss(p, (x, RH_H - 3.0, RH_Z0 + t), (x, RH_H - 3.0, RH_Z1 - t), 5.0, lod)
        box(p, HAZ, (RH_X1 + 0.5, RH_H - 4.6, hcz), (1.8, 1.2, hd + 2.2))
    if lod < 3:
        tug(p, hcx + 8.0, 0.3, hcz, lod, yaw=-math.pi / 2)                                   # a tug inside, nose out
    if lod < 2:
        for k in range(4):
            box(p, FLOOD, (hcx - 30.0 + k * 22.0, RH_H - 8.0, hcz), (3.0, 0.6, 3.0))           # hall lights
        for sz in (-1, 1):                                                                   # the door leaf tracks
            box(p, STEEL, (RH_X1 + 2.0, RH_H / 2, hcz + sz * (hd / 2 - 1.0)), (3.0, RH_H, 2.0))
        for k in range(4):                                                                   # roof vents
            box(p, STEEL, (hcx - 30.0 + k * 20.0, RH_H + 5.5, hcz + 30.0), (6.0, 3.0, 6.0))
        box(p, HAZ, (RH_X1 + 6.0, 0.12, hcz), (12.0, 0.05, hd - 10.0))                       # the bay apron hatching
    # the station building on +Z of the hangar: white facade, orange band, a tower with red beacons
    sx0, sx1, sz0, sz1, sh = -112.0, -12.0, 26.0, 96.0, 30.0
    scx, scz = (sx0 + sx1) / 2, (sz0 + sz1) / 2
    box(p, FACADE_L, (scx, sh / 2, scz), (sx1 - sx0, sh, sz1 - sz0))
    box(p, RORANGE, (scx, sh - 3.0, scz), (sx1 - sx0 + 0.6, 4.0, sz1 - sz0 + 0.6))
    box(p, RWHITE, (scx, sh + 1.0, scz), (sx1 - sx0 + 2.0, 2.0, sz1 - sz0 + 2.0))
    tx, tz = -24.0, 84.0
    box(p, RWHITE, (tx, 27.0, tz), (20.0, 54.0, 20.0))
    box(p, RORANGE, (tx, 48.0, tz), (20.6, 5.0, 20.6))
    box(p, WARM, (tx, 42.0, tz), (20.4, 3.0, 20.4))                                          # the lit dispatch room
    mast(p, tx, tz, 54.0, 92.0, 3.5, lod)
    if lod < 2:
        box(p, DARK, (sx1 + 0.2, 4.0, scz - 10.0), (0.4, 8.0, 20.0))                          # the crew entrance
        box(p, RORANGE, (sx1 + 1.5, 9.0, scz - 10.0), (3.0, 0.6, 22.0))
        for y in (70.0, 84.0):
            box(p, STEEL, (tx, y, tz), (12.0, 0.8, 0.8))
        for k in range(4):
            box(p, STEEL, (scx - 36.0 + k * 22.0, sh + 4.0, scz), (8.0, 4.0, 8.0))
    # 2 pads at grade for the rescue tugs, orange markings
    for k, (x, z) in enumerate(RD_PADS):
        pad(p, x, 0.0, z, 56.0, RORANGE, lod, lights=(BEACON, FLOOD))
        if lod < 3 and k == 0:
            tug(p, x, 0.3, z, lod, yaw=math.pi / 2)
    # the lattice jib crane between the pads (a lattice tower, a lattice jib reaching over the pads, the hook)
    cx, cz = 104.0, -2.0
    mast(p, cx, cz, 0.0, 58.0, 5.0, lod)
    truss(p, (cx + 6.0, 60.0, cz), (cx - 64.0, 60.0, cz), 4.0, lod)                           # the jib (over the pads)
    if lod < 3:
        box(p, RORANGE, (cx + 3.0, 63.5, cz), (10.0, 5.0, 7.0))                               # the slewing cab
        box(p, STEEL, (cx + 12.0, 59.0, cz), (8.0, 6.0, 8.0))                                 # counterweight
    if lod < 2:
        cyl(p, STEEL, (cx - 48.0, 58.0, cz), (cx - 48.0, 30.0, cz), 0.3, 4)                   # the hoist cable
        box(p, HAZ, (cx - 48.0, 29.0, cz), (2.0, 2.0, 2.0))                                   # the hook block
        cyl(p, STEEL, (cx + 2.0, 72.0, cz), (cx - 60.0, 61.0, cz), 0.25, 4)                   # jib stays
        truss(p, (cx, 58.0, cz), (cx, 74.0, cz), 2.0, lod)                                    # the A-frame top
    # a second boom: a lattice derrick at the hangar bay for lifting wrecks out
    truss(p, (RH_X1 + 8.0, 0.0, RH_Z0 + 4.0), (RH_X1 + 8.0, 52.0, RH_Z0 + 4.0), 3.0, lod)
    if lod < 3:
        truss(p, (RH_X1 + 8.0, 50.0, RH_Z0 + 4.0), (RH_X1 + 40.0, 40.0, RH_Z0 + 24.0), 2.4, lod)
    # fuel tanks (orange) and foam tanks (white, red bands) on the +X edge
    seg = _seg(lod, 16)
    for k in range(3):
        tank(p, RORANGE, 112.0 - k * 16.0, -88.0, 6.0, 14.0, seg, lod)
        tank(p, RWHITE, 112.0 - k * 16.0, 88.0, 6.0, 18.0, seg, lod)
        if lod < 2:
            ring(p, RORANGE, 112.0 - k * 16.0, 88.0, 6.0, 6.3, 20.0, 23.0, seg)
            cyl(p, STEEL, (112.0 - k * 16.0, 3.0, -88.0 + 6.0), (112.0 - k * 16.0, 3.0, -76.0), 0.8, 6)
    if lod < 2:
        pipe_loop(p, [(112.0, 3.0, -76.0), (80.0, 3.0, -76.0)], 0.9, lod, mat=STEEL)
        pipe_loop(p, [(112.0, 3.0, 76.0), (80.0, 3.0, 76.0)], 0.9, lod, mat=STEEL)
        for x, z in ((80.0, -76.0), (80.0, 76.0)):
            box(p, RORANGE, (x, 2.5, z), (4.0, 5.0, 3.0))                                     # hose stations
    # beacons: red on the hangar / building corners, orange flares on the tank rows
    if lod < 3:
        for x, z, y in ((RH_X0, RH_Z0, RH_H + 4.6), (RH_X1, RH_Z0, RH_H + 4.6), (RH_X1, RH_Z1, RH_H + 4.6),
                        (sx0, sz1, sh + 2.6), (sx1, sz1, sh + 2.6)):
            beacon(p, (x, y, z), 2.0)
        for z in (-98.0, 98.0):
            box(p, FLARE, (118.0, 1.5, z), (2.0, 2.0, 2.0))
    if lod < 2:
        for z in (-60.0, -20.0, 20.0, 60.0):
            lamp(p, 22.0, 0.0, z)
    return P


def rescue_dock_hits():
    n = "hubs_rescue_dock"
    hcx, hcz = (RH_X0 + RH_X1) / 2, (RH_Z0 + RH_Z1) / 2
    hw, hd = RH_X1 - RH_X0, RH_Z1 - RH_Z0
    return [hbox(n, (0, -3.0, 0), (RD_W, 6.0, RD_D)),
            hbox(n, (RH_X0 + 1.5, RH_H / 2, hcz), (3.0, RH_H, hd)),
            hbox(n, (hcx, RH_H / 2, RH_Z0 + 1.5), (hw, RH_H, 3.0)),
            hbox(n, (hcx, RH_H / 2, RH_Z1 - 1.5), (hw, RH_H, 3.0)),
            hbox(n, (hcx, RH_H + 2.0, hcz), (hw + 2.0, 4.0, hd + 2.0)),
            hbox(n, (-62.0, 16.0, 61.0), (100.0, 32.0, 70.0)),
            hbox(n, (-24.0, 27.0, 84.0), (20.0, 54.0, 20.0)),
            hbox(n, (104.0, 30.0, -2.0), (6.0, 60.0, 6.0)),
            hbox(n, (80.0, 15.0, -88.0), (44.0, 30.0, 14.0)),
            hbox(n, (80.0, 17.0, 88.0), (44.0, 34.0, 14.0))]


KIT = {
    "hubs_power_plant": (power_plant_parts, power_plant_hits, "Электростанция квартала",
                         "Электростанция на целый квартал 520x520 м: два термоядерных реактора под куполами, пульт "
                         "управления, три ряда радиаторов на решётчатых мачтах (сброс тепла города), контуры "
                         "охлаждения, трансформаторный двор (tools/models/units/hub_services.py)."),
    "hubs_power_unit": (power_unit_parts, power_unit_hits, "Подстанция / реакторный блок",
                        "Подстанция на четверть квартала 160x120 м: реакторный купол, ряд радиаторов, трансформаторы, "
                        "распредустройство (tools/models/units/hub_services.py)."),
    "hubs_police_dock": (police_dock_parts, police_dock_hits, "Полицейский участок с доком",
                         "Участок службы охраны 240x200 м: здание с башней и мачтой связи, ангар, посадочная палуба на "
                         "решётчатых опорах с 3 площадками для патрульных катеров PC-30, сине-белые огни "
                         "(tools/models/units/hub_services.py)."),
    "hubs_rescue_dock": (rescue_dock_parts, rescue_dock_hits, "Станция МЧС с доком",
                         "Спасательная станция 240x200 м: большой ангар с открытым проёмом, 2 площадки для "
                         "спасательных буксиров, решётчатый кран, баки горючего и пены, оранжево-белая окраска, "
                         "красные маяки (tools/models/units/hub_services.py)."),
}

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", v[3]) for k, v in KIT.items()}
