"""Free-standing space stations (author 2026-11-18: «создать модели маяка - думаю достаточно крупный объект с антеннами,
вращающимся жилым модулем и доком. имеет пару истребителей охраны и персонал. есть свой генератор. нуждается в ротациях
и поставках, далее - несколько моделей исследовательских лабораторий - достаточно крупное - думаю - торообразные модули
с генератором, доком. лаборатории должны иметь несколько моделей внешне различающихся.»; later: labs may sit on moons /
small asteroids or float free, they need supply runs; «фермы везде обязательно, хотя бы в качестве мачт или опорных
столбов» - every model carries visible lattice trusses).

Frames (metres, +Y up):
  stn_beacon_core       - the static beacon, spine along +Z through the origin; the habitat ring's bearing (axle r 18 m,
                          |z| < 48) sits at the origin. Antennas at +Z, the dock at the -Z tip (mouth at z = -400, facing
                          -Z), the reactor on a 210 m lattice boom off to -X (away from the crew).
  stn_beacon_hab_ring   - the beacon's spinning habitat: origin = spin centre, spins about its own +Z; place it at the
                          core's origin with the same orientation (0.25 g at 150 m and ~1.2 rpm).
  stn_lab_torus_a       - one big torus (separate spinning asset stn_lab_torus_a_ring, attached at the core's origin,
                          spin about +Z), a central sphere and a docking spine (dock mouth z = +330, facing +Z), reactor
                          at the -Z tip.
  stn_lab_torus_b       - twin static (microgravity) tori stacked on a long truss along Z, reactor + big radiators at -Z,
                          dock mouth at z = +420 facing +Z, a sideways experiment boom.
  stn_lab_torus_c       - a flattened static torus lying in XZ (axis +Y) with radial lab pods, a huge open-truss
                          telescope up +Y, reactor and dock below (dock mouth y = -340, facing -Y).
  stn_lab_surface       - a surface lab for a moon / small asteroid: the ground is y = 0, everything above it on lattice
                          columns; landing pad (top y = 14) + cargo apron at -X, reactor 300 m away at +X.
"""
import math
import random

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_mega import FACADE, FACADE_L
from units.hub_belt import CONT

register_materials({
    "eq_stn_radiator": ((0.80, 0.80, 0.82), (0.30, 0.30, 0.30), 40),
    "eq_stn_array": ((0.10, 0.14, 0.26), (0.55, 0.55, 0.65), 110),
    "eq_stn_mirror": ((0.72, 0.74, 0.78), (0.90, 0.90, 0.90), 200),
    "eq_stn_dish": ((0.86, 0.86, 0.84), (0.30, 0.30, 0.30), 40),
    "eq_stn_fighter": ((0.36, 0.40, 0.46), (0.45, 0.45, 0.50), 90),
})
RAD, ARRAY, MIRROR, DISH, FIGHTER = "eq_stn_radiator", "eq_stn_array", "eq_stn_mirror", "eq_stn_dish", "eq_stn_fighter"
UP_Y = rot_x(-math.pi / 2)          # build frame +Z -> model +Y (C and the surface lab are built along Z, then stood up)


# ---------------------------------------------------------------- small vector helpers / part merging
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def _len(a): return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def merge(dst, src):
    base = len(dst.v)
    dst.v += src.v
    dst.n += src.n
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + base for i in f) for f in fl)


def put(dst, src, R=I3, t=(0, 0, 0)):
    merge(dst, placed(src, R, t))


def segs(lod):
    """(major, minor) segments of the big tori"""
    if close():
        return 112, 20
    return {0: 80, 1: 48, 2: 24}[lod], {0: 16, 1: 10, 2: 6}[lod]


# ---------------------------------------------------------------- lattice trusses (mandatory everywhere)
def rod(p, mat, a, b, t):
    d = _sub(b, a)
    L = _len(d)
    if L < 1e-6:
        return
    R = axis_to(norm(d))
    h = t / 2
    for nx, ny in ((1, 0), (0, 1), (-1, 0), (0, -1)):        # the four sides only: the rod ends sit in the joints
        ux, uy = -ny, nx
        n = apply(R, (nx, ny, 0.0))
        ids = [p.vert(_add(a, apply(R, (nx * h + su * ux * h, ny * h + su * uy * h, z))), n)
               for su, z in ((-1, 0.0), (1, 0.0), (1, L), (-1, L))]
        p.face(mat, ids, n)


def lattice(p, a, b, w, lod, bay=None, t=None, mat=STEEL):
    """a square lattice truss from a to b: 4 chords, a ring of struts every bay, zig-zag diagonals on the four faces"""
    d = _sub(b, a)
    L = _len(d)
    R = axis_to(norm(d))
    h = w / 2
    t = t or max(0.5, w * 0.07)
    bay = max(bay or w * 2.0, 6.0) * (1, 2, 4)[lod]
    n = 1 if lod == 2 and w < 12.0 else max(1, int(round(L / bay)))
    P = lambda x, y, z: _add(a, apply(R, (x, y, z)))
    cs = ((-h, -h), (h, -h), (h, h), (-h, h))
    for cx, cy in cs:
        rod(p, mat, P(cx, cy, 0), P(cx, cy, L), t * 1.4)
    for k in range(n + 1):
        z = L * k / n
        for i in range(4):
            (ax, ay), (bx, by) = cs[i], cs[(i + 1) % 4]
            rod(p, mat, P(ax, ay, z), P(bx, by, z), t)
            if k < n and lod < 2:
                z1 = L * (k + 1) / n
                if (k + i) % 2:
                    rod(p, mat, P(ax, ay, z), P(bx, by, z1), t * 0.8)
                else:
                    rod(p, mat, P(bx, by, z), P(ax, ay, z1), t * 0.8)


def ring_lattice(p, z0, z1, r, n_long, lod, bay=40.0, t=1.6, mat=STEEL):
    """an open polygonal lattice tube along Z (the telescope barrel): longerons, hoops, diagonals"""
    pts = lambda z: [(r * math.cos(2 * math.pi * i / n_long), r * math.sin(2 * math.pi * i / n_long), z) for i in range(n_long)]
    n = max(1, int(round((z1 - z0) / (bay * (1, 2, 4)[lod]))))
    for i in range(n_long):
        rod(p, mat, pts(z0)[i], pts(z1)[i], t * 1.4)
    for k in range(n + 1):
        z = z0 + (z1 - z0) * k / n
        ring = pts(z)
        for i in range(n_long):
            rod(p, mat, ring[i], ring[(i + 1) % n_long], t)
            if k < n and lod < 2:
                ring1 = pts(z0 + (z1 - z0) * (k + 1) / n)
                rod(p, mat, ring[i], ring1[(i + 1) % n_long] if (k + i) % 2 else ring1[i - 1], t * 0.8)


# ---------------------------------------------------------------- volumes
def torus(p, mat, Rc, r, lod, R=I3, t=(0, 0, 0), sr=1.0, sz=1.0):
    """a torus about local Z (major radius Rc, tube radius r; sr / sz flatten the section radially / axially)"""
    M, m = segs(lod)
    prof = [(Rc + sr * r * math.cos(2 * math.pi * i / m), sz * r * math.sin(2 * math.pi * i / m)) for i in range(m)]
    lathe(p, mat, prof + [prof[0]], M, R, t, soft=70)


def sphere(p, mat, c, r, lod, R=I3):
    m = {0: 12, 1: 8, 2: 6}[lod]
    prof = [(r * math.cos(math.pi * (i / m - 0.5)), r * math.sin(math.pi * (i / m - 0.5))) for i in range(m + 1)]
    prof[0], prof[-1] = (0.0, -r), (0.0, r)
    lathe(p, mat, prof, segs(lod)[0] // 2, R, c, soft=70)


def zcyl(p, mat, z0, z1, r, lod, x=0.0, y=0.0):
    cyl(p, mat, (x, y, z0), (x, y, z1), r, max(8, segs(lod)[0] // 3))


def band(p, mat, z0, z1, r0, r1, lod, R=I3, t=(0, 0, 0)):
    """a collar / annulus about local Z (closed rectangle section)"""
    lathe(p, mat, crect(r0, r1, z0, z1), max(8, segs(lod)[0] // 3), R, t)


def dish(p, c, direction, r, depth, lod, mat=DISH):
    """a parabolic dish facing `direction`, its vertex at c, with a feed on three struts and a back mount"""
    R = axis_to(norm(direction))
    th = max(0.6, r * 0.04)
    n = 5 if lod < 2 else 3
    back = [(r * i / n, depth * (i / n) ** 2 - th) for i in range(n + 1)]
    front = [(r * i / n, depth * (i / n) ** 2) for i in range(n, -1, -1)]
    lathe(p, mat, back + front, max(10, segs(lod)[0] // 3), R, c)
    W = lambda x, y, z: _add(c, apply(R, (x, y, z)))
    focus = W(0, 0, r * r / (4 * depth) if depth > 0 else r)
    if lod < 2:
        for k in range(3):
            a = 2 * math.pi * k / 3
            rod(p, STEEL, W(r * 0.9 * math.cos(a), r * 0.9 * math.sin(a), depth * 0.81), focus, max(0.3, r * 0.025))
    box(p, HULL, focus, (r * 0.12, r * 0.12, r * 0.12), R)
    cyl(p, HULL, W(0, 0, -th), W(0, 0, -th - r * 0.3), r * 0.12, 8)


def radiator_wing(p, a, b, side, width, lod, n_panels=4):
    """a lattice boom a -> b carrying flat radiator panels offset along `side` (a unit vector, the panel plane =
    boom x side)"""
    lattice(p, a, b, max(3.0, width * 0.12), lod)
    d = _sub(b, a)
    L = _len(d)
    u = norm(d)
    nrm = norm((u[1] * side[2] - u[2] * side[1], u[2] * side[0] - u[0] * side[2], u[0] * side[1] - u[1] * side[0]))
    # frame: local X = side, local Y = panel normal, local Z = boom
    R = [[side[0], nrm[0], u[0]], [side[1], nrm[1], u[1]], [side[2], nrm[2], u[2]]]
    pl = L / n_panels
    for k in range(n_panels):
        c = _add(a, _mul(u, pl * (k + 0.5)))
        for s in (-1, 1):
            cc = _add(c, _mul(side, s * (width * 0.5 + 2.0)))
            box(p, RAD, cc, (width, 0.6, pl - 3.0), R)
            if lod < 2:
                box(p, STEEL, cc, (width + 1.0, 1.2, 1.2), R)        # the panel's coolant header
                box(p, STEEL, _add(c, _mul(side, s * 1.5)), (3.0, 1.6, pl - 3.0), R)


def reactor(p, z0, z1, r, lod, wings=4, span=120.0, x=0.0, y=0.0):
    """a reactor module along Z between z0 (crew side, the shadow shield) and z1: a banded core, a shield disc, four
    (or two) radiator wings along its length"""
    s = 1 if z1 > z0 else -1
    zcyl(p, HULL, min(z0, z1), max(z0, z1), r, lod, x, y)
    band(p, HAZ, -2.0, 2.0, r, r + 1.5, lod, t=(x, y, z0 + s * 6.0))
    band(p, HAZ, -2.0, 2.0, r, r + 1.5, lod, t=(x, y, z1 - s * 6.0))
    lathe(p, DARK, crect(0.0, r * 1.8, -2.5, 2.5), max(8, segs(lod)[0] // 3), I3, (x, y, z0))     # shadow shield
    if lod < 2:
        for k in range(1, 4):
            band(p, STEEL, -1.0, 1.0, r, r + 0.8, lod, t=(x, y, z0 + (z1 - z0) * k / 4))
        for k in range(4):
            a = math.pi / 2 * k + math.pi / 4
            beacon(p, (x + (r + 1.0) * math.cos(a), y + (r + 1.0) * math.sin(a), z1 - s * 2.0), 1.5)
    for k in range(wings if lod < 2 else 2):
        a = (math.pi / 2 if wings == 4 else math.pi) * k
        ca, sa = math.cos(a), math.sin(a)
        zm = z0 + (z1 - z0) * 0.5
        radiator_wing(p, (x + r * ca, y + r * sa, zm), (x + (r + span) * ca, y + (r + span) * sa, zm),
                      (0.0, 0.0, 1.0), abs(z1 - z0) * 0.42, lod)


def dock_hex(p, zc, zm, A, lod):
    """a hexagonal dock module along Z: windowed side walls, a closed end at zc, an open lit bay mouth at zm"""
    s = 1 if zm > zc else -1
    L = abs(zm - zc)
    mid = (zc + zm) / 2
    Rv = A / math.cos(math.pi / 6)
    for k in range(6):
        a = math.pi / 3 * k + math.pi / 6
        ca, sa = math.cos(a), math.sin(a)
        box(p, FACADE, ((A - 3.0) * ca, (A - 3.0) * sa, mid), (6.0, Rv, L), rot_z(a))
        box(p, HULL, (A / 2 * ca, A / 2 * sa, zc + s * 3.0), (A, Rv, 6.0), rot_z(a))                  # closed end
        box(p, HULL, ((A - A * 0.2) * ca, (A - A * 0.2) * sa, zm - s * 3.0), (A * 0.4, Rv, 6.0), rot_z(a))  # mouth frame
        if lod < 2:
            box(p, LIGHT, ((A + 1.0) * ca, (A + 1.0) * sa, mid), (2.0, Rv * 0.3, L - 10.0), rot_z(a))  # hull rib
            for z in (zc + s * L * 0.3, zc + s * L * 0.65):                                         # bay lamps
                box(p, FLOOD, ((A - 6.6) * ca, (A - 6.6) * sa, z), (1.0, Rv * 0.5, 2.0), rot_z(a))
    box(p, DARK, (0, 0, zc + s * 6.6), (1.3 * A, 1.3 * A, 1.0))                                      # bay back wall
    nl = 16 if lod < 2 else 8
    for k in range(nl):                                                                             # lit mouth ring
        a = 2 * math.pi * k / nl
        box(p, FLOOD, (A * 0.7 * math.cos(a), A * 0.7 * math.sin(a), zm + s * 0.4), (2.5, 2.5, 0.8))
    if lod < 2:
        for k in range(6):
            a = math.pi / 3 * k
            beacon(p, (Rv * math.cos(a), Rv * math.sin(a), zm), 2.0)
            beacon(p, (Rv * math.cos(a), Rv * math.sin(a), zc), 1.5)


def cargo_apron(lod, w=70.0, d=90.0, seed=1):
    """a supply apron in its own frame: a deck (top y = 0, normal +Y) with container stacks, a lattice gantry crane,
    lamps and hazard edges; placed with put()"""
    rng = random.Random(seed)
    a = Part("tmp")
    box(a, HULL, (0, -1.5, 0), (w, 3.0, d))
    if lod < 2:
        for sx in (-1, 1):
            box(a, HAZ, (sx * (w / 2 - 1.0), 0.2, 0), (2.0, 0.4, d))
    for i in range(3):
        for j in range(4 if lod < 2 else 2):
            n = rng.randint(1, 3)
            for k in range(n):
                box(a, CONT[rng.randrange(3)], (-w / 2 + 12.0 + i * 14.0, 1.3 + k * 2.6, -d / 2 + 10.0 + j * 14.0 * (2 if lod >= 2 else 1)),
                    (2.44, 2.59, 12.2))
    gx = w / 2 - 10.0                                                     # gantry crane over the stacks
    if lod >= 2:
        box(a, STEEL, ((gx - w / 2 + 5.0) / 2, 23.5, 0), (gx + w / 2 - 5.0, 3.0, 3.0))
        return a
    for z in (-d / 2 + 5.0, d / 2 - 5.0):
        lattice(a, (gx, 0, z), (gx, 22.0, z), 3.0, lod)
        lattice(a, (-w / 2 + 5.0, 0, z), (-w / 2 + 5.0, 22.0, z), 3.0, lod)
        lattice(a, (-w / 2 + 5.0, 23.5, z), (gx, 23.5, z), 3.0, lod)
    lattice(a, (gx, 23.5, -d / 2 + 5.0), (gx, 23.5, d / 2 - 5.0), 3.0, lod)
    box(a, HAZ, (0, 21.0, 0), (6.0, 3.0, 4.0))                             # the trolley
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(a, (sx * w / 2, 1.0, sz * d / 2), 1.2)
                box(a, FLOOD, (sx * (w / 2 - 3.0), 0.4, sz * (d / 2 - 3.0)), (2.0, 0.6, 2.0))
    return a


def fighter(lod):
    """a guard interceptor ~18 m (B-4 class look), nose +Z, wheels/skids on y = 0"""
    f = Part("tmp")
    box(f, FIGHTER, (0, 1.6, -0.5), (2.6, 2.2, 12.0))                                 # fuselage
    lathe(f, FIGHTER, [(0.0, 0.0), (1.4, 0.0), (0.0, 6.0)], 8, I3, (0, 1.6, 5.5), smooth=False)   # nose cone
    box(f, FIGHTER, (0, 1.2, -2.0), (12.0, 0.35, 4.5))                                # wing
    box(f, HULL, (0, 1.2, -1.0), (5.0, 0.45, 7.0))                                    # wing root
    for sx in (-1, 1):
        box(f, FIGHTER, (sx * 1.3, 3.4, -5.0), (0.3, 2.6, 3.0), rot_z(-sx * 0.35))  # canted fins
        cyl(f, DARK, (sx * 0.75, 1.6, -6.4), (sx * 0.75, 1.6, -7.0), 0.75, 8)       # nozzles
        if lod < 2:
            box(f, BEACON if sx < 0 else COOL, (sx * 6.0, 1.2, -2.0), (0.3, 0.4, 0.6))  # nav lights
            box(f, STEEL, (sx * 6.0, 1.2, -1.0), (0.4, 0.4, 3.0))                   # tip rails
    box(f, COOL, (0, 2.95, 3.0), (1.2, 0.6, 3.2))                                     # canopy
    if lod < 2:
        for z in (-3.0, 3.0):
            box(f, DARK, (0, 0.25, z), (1.8, 0.5, 0.6))                               # skids
    return f


def hab_ring_parts(name, Rc, half_w, half_h, hub_r0, hub_r1, hub_hz, spokes, lod, torus_r=None):
    """a spinning ring about +Z centred on its origin: a windowed rim (rounded-box section or a round torus), rib rings,
    lattice spokes with pressurised tubes, a hub bearing ring (bore hub_r0)"""
    P = {name: Part(name)}
    p = P[name]
    M, m = segs(lod)
    if torus_r:
        torus(p, FACADE, Rc, torus_r, lod)
        rim_in = Rc - torus_r
    else:
        lathe(p, FACADE, rrect(Rc - half_w, Rc + half_w, -half_h, half_h, min(half_w, half_h) * 0.35, 3 if lod < 2 else 1), M)
        rim_in = Rc - half_w
    if lod < 2:                                                                       # rib frames around the rim
        for k in range(12):
            a = 2 * math.pi * k / 12
            c = (Rc * math.cos(a), Rc * math.sin(a), 0.0)
            if torus_r:
                lathe(p, LIGHT, crect(torus_r * 0.98, torus_r * 1.1, -1.5, 1.5), max(8, m), axis_to((-math.sin(a), math.cos(a), 0.0)), c)
            else:
                box(p, LIGHT, c, (2 * half_w + 3.0, 4.0, 2 * half_h + 3.0), rot_z(a))
    band(p, HULL, -hub_hz, hub_hz, hub_r0, hub_r1, lod)                               # hub bearing ring
    band(p, STEEL, -hub_hz - 2.0, -hub_hz, hub_r0 - 1.5, hub_r0 + 4.0, lod)           # race lips
    band(p, STEEL, hub_hz, hub_hz + 2.0, hub_r0 - 1.5, hub_r0 + 4.0, lod)
    for k in range(spokes):
        a = 2 * math.pi * k / spokes
        ca, sa = math.cos(a), math.sin(a)
        p0, p1 = (hub_r1 * ca, hub_r1 * sa, 0.0), ((rim_in + 1.0) * ca, (rim_in + 1.0) * sa, 0.0)
        lattice(p, p0, p1, max(8.0, half_h * 0.7 if not torus_r else torus_r * 0.45), lod)
        cyl(p, HULL, p0, p1, 3.5 if lod < 2 else 4.5, 8)                              # the lift tube inside
        if lod < 2:
            beacon(p, ((rim_in + 3.0) * ca, (rim_in + 3.0) * sa, (half_h if not torus_r else torus_r) + 1.0), 2.0)
    if lod < 2:
        nb = 24                                                                       # lit window bands on the rim
        for k in range(nb):
            a = 2 * math.pi * (k + 0.5) / nb
            ro = (Rc + half_w if not torus_r else Rc + torus_r) + 0.3
            box(p, WARM if k % 3 else COOL, (ro * math.cos(a), ro * math.sin(a), 0.0),
                (0.6, 2 * math.pi * ro / nb * 0.5, 2.0), rot_z(a))
    return P


def ring_hits(part, Rc, radial, axial, n, z=0.0):
    """boxes around a ring about Z"""
    out = []
    for k in range(n):
        a = 2 * math.pi * k / n
        out.append(hbox(part, (Rc * math.cos(a), Rc * math.sin(a), z), (radial, 2 * Rc * math.tan(math.pi / n) + 2.0, axial),
                        (0.0, 0.0, math.degrees(a))))
    return out


def ring_hits_y(part, Rc, radial, height, n, y=0.0):
    """boxes around a ring about Y (local X radial, local Z tangent)"""
    out = []
    for k in range(n):
        a = 2 * math.pi * k / n
        out.append(hbox(part, (Rc * math.cos(a), y, Rc * math.sin(a)), (radial, height, 2 * Rc * math.tan(math.pi / n) + 2.0),
                        (0.0, -math.degrees(a), 0.0)))
    return out


# ================================================================ the beacon
AXLE_R, AXLE_HZ = 18.0, 48.0
BCN_RC, BCN_HW, BCN_HH = 150.0, 18.0, 22.0       # ring radius, rim half-width (radial), half-height (axial)


def beacon_core_parts(lod):
    name = "stn_beacon_core"
    P = {name: Part(name)}
    p = P[name]
    # --- the axle the habitat ring turns on (origin), bearing collars clear of the ring hub (|z| <= 28)
    zcyl(p, HULL, -AXLE_HZ, AXLE_HZ, AXLE_R, lod)
    for sz in (-1, 1):
        band(p, STEEL, sz * 32.0 - 3.0, sz * 32.0 + 3.0, AXLE_R, 32.0, lod)
    # --- the spine: a pressurised core tube inside a 26 m lattice
    zcyl(p, HULL, -300.0, 420.0, 6.0, lod)
    lattice(p, (0, 0, 40.0), (0, 0, 420.0), 26.0, lod, bay=40.0)
    lattice(p, (0, 0, -40.0), (0, 0, -285.0), 26.0, lod, bay=40.0)
    # --- the crew / operations module next to the ring (windowed), its docking collar to the axle
    zcyl(p, FACADE, 58.0, 112.0, 30.0, lod)
    band(p, LIGHT, 56.0, 60.0, 6.0, 33.0, lod)
    band(p, LIGHT, 110.0, 114.0, 6.0, 33.0, lod)
    if lod < 2:
        for k in range(12):
            a = 2 * math.pi * k / 12
            box(p, WARM, (30.3 * math.cos(a), 30.3 * math.sin(a), 85.0), (0.6, 5.0, 3.0), rot_z(a))
    # --- antenna complex (+Z): phased-array panels, radial mast arrays with dipoles, side dishes, the main dish, top mast
    for k in range(4):
        a = math.pi / 2 * k + math.pi / 4
        ca, sa = math.cos(a), math.sin(a)
        box(p, ARRAY, (42.0 * ca, 42.0 * sa, 165.0), (1.5, 44.0, 80.0), rot_z(a))
        box(p, STEEL, (41.0 * ca, 41.0 * sa, 165.0), (1.5, 46.0, 2.0), rot_z(a))
        for z in (135.0, 195.0):
            rod(p, STEEL, (13.0 * ca, 13.0 * sa, z), (41.0 * ca, 41.0 * sa, z), 1.6)
    for k in range(4):
        a = math.pi / 2 * k
        ca, sa = math.cos(a), math.sin(a)
        r0, r1 = 13.0, 180.0
        lattice(p, (r0 * ca, r0 * sa, 260.0), (r1 * ca, r1 * sa, 260.0), 7.0, lod)
        if lod < 2:
            for j in range(1, 9):                                                    # dipole elements across the mast
                rj = r0 + (r1 - r0) * j / 9
                rod(p, STEEL, (rj * ca - 12.0 * sa, rj * sa + 12.0 * ca, 260.0), (rj * ca + 12.0 * sa, rj * sa - 12.0 * ca, 260.0), 0.6)
        dish(p, (r1 * ca, r1 * sa, 264.0), (0.0, 0.0, 1.0), 12.0, 3.0, lod)
        beacon(p, (r1 * ca, r1 * sa, 255.0), 2.0)
    for sx in (-1, 1):                                                               # side dishes on short booms
        lattice(p, (sx * 13.0, 0, 340.0), (sx * 70.0, 0, 340.0), 6.0, lod)
        dish(p, (sx * 72.0, 0, 340.0), (sx * 1.0, 0.0, 0.3), 24.0, 6.0, lod)
    band(p, HULL, 405.0, 422.0, 0.0, 16.0, lod)
    dish(p, (0, 0, 424.0), (0.0, 0.0, 1.0), 62.0, 16.0, lod)                         # the main beacon dish
    lattice(p, (0, 0, 470.0), (0, 0, 560.0), 6.0, lod)                               # the top mast (over the feed)
    beacon(p, (0, 0, 561.0), 3.0)
    # --- reactor on a 210 m lattice boom off to -X (away from the habitat), radiators along the boom
    lattice(p, (-13.0, 0, -200.0), (-232.0, 0, -200.0), 14.0, lod)
    cyl(p, HULL, (-13.0, 0, -200.0), (-232.0, 0, -200.0), 2.5, 8)                     # power / coolant trunk
    lathe(p, HULL, [(0.0, 0.0), (22.0, 0.0), (22.0, 70.0), (0.0, 70.0)], max(8, segs(lod)[0] // 3), axis_to((-1, 0, 0)), (-232.0, 0, -200.0))
    lathe(p, DARK, crect(0.0, 36.0, -2.5, 2.5), max(8, segs(lod)[0] // 3), axis_to((-1, 0, 0)), (-234.0, 0, -200.0))
    for x in (-250.0, -285.0):
        lathe(p, HAZ, crect(22.0, 23.5, -2.0, 2.0), max(8, segs(lod)[0] // 3), axis_to((-1, 0, 0)), (x, 0, -200.0))
    beacon(p, (-303.0, 0, -200.0), 2.5)
    radiator_wing(p, (-40.0, 0, -200.0), (-220.0, 0, -200.0), (0.0, 0.0, 1.0), 45.0, lod)
    # --- dock module at the -Z tip, mouth at z = -400 facing -Z
    dock_hex(p, -282.0, -400.0, 34.0, lod)
    # --- guard apron + hangar on +X with two parked interceptors; supply apron on -X
    ap = Part("tmp")
    box(ap, HULL, (0, -1.5, 0), (70.0, 3.0, 100.0))
    if lod < 2:
        for sz in (-1, 1):
            box(ap, HAZ, (0, 0.2, sz * 49.0), (70.0, 0.4, 2.0))
    box(ap, LIGHT, (24.0, 9.0, 0), (22.0, 18.0, 60.0))                               # the hangar shed
    box(ap, DARK, (12.9, 7.5, 0), (0.4, 14.0, 50.0))                                  # its open front
    box(ap, WARM, (35.1, 14.0, 0), (0.4, 2.0, 40.0))                                  # ready-room windows
    for z in (-20.0, 20.0):
        put(ap, fighter(lod), rot_y(-math.pi / 2), (-8.0, 0.0, z))                    # nose -X (out of the apron)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                beacon(ap, (sx * 35.0, 1.0, sz * 50.0), 1.2)
    # apron frame: deck normal -> +X? keep the deck horizontal below the dock: top at y = -36, +X of the hull
    put(p, ap, I3, (70.0, -36.0, -340.0))
    lattice(p, (25.0, -26.0, -310.0), (40.0, -36.0, -310.0), 5.0, lod)
    lattice(p, (25.0, -26.0, -370.0), (40.0, -36.0, -370.0), 5.0, lod)
    put(p, cargo_apron(lod, 60.0, 90.0, seed=7), rot_z(math.pi / 2), (-40.0, 0.0, -340.0))   # deck on the -X wall, facing -X
    for k in range(4 if lod < 2 else 2):                                              # spine nav lights
        z = -260.0 + 160.0 * k
        beacon(p, (14.0, 14.0, z), 2.0)
        beacon(p, (-14.0, -14.0, z), 2.0)
    return P


def beacon_core_hits():
    n = "stn_beacon_core"
    return [hcyl(n, (0, 0, -AXLE_HZ), (0, 0, AXLE_HZ), AXLE_R),
            hbox(n, (0, 0, 190.0), (28.0, 28.0, 300.0)),
            hbox(n, (0, 0, -160.0), (28.0, 28.0, 250.0)),
            hcyl(n, (0, 0, 56.0), (0, 0, 114.0), 33.0),
            hbox(n, (0, 0, 165.0), (90.0, 90.0, 80.0)),
            hbox(n, (0, 0, 260.0), (370.0, 10.0, 10.0)),
            hbox(n, (0, 0, 260.0), (10.0, 370.0, 10.0)),
            hcyl(n, (0, 0, 405.0), (0, 0, 442.0), 62.0),
            hbox(n, (0, 0, 515.0), (8.0, 8.0, 90.0)),
            hbox(n, (0, 0, 340.0), (200.0, 50.0, 30.0)),
            hbox(n, (-160.0, 0, -200.0), (300.0, 16.0, 16.0)),
            hbox(n, (-130.0, 0, -200.0), (180.0, 4.0, 100.0)),
            hcyl(n, (-232.0, 0, -200.0), (-302.0, 0, -200.0), 23.0),
            hhex(n, (0, 0, -341.0), (34.0 / math.cos(math.pi / 6), 34.0, 59.0), 34.0 / math.cos(math.pi / 6) / 2),
            hbox(n, (70.0, -30.0, -340.0), (70.0, 22.0, 100.0)),
            hbox(n, (-50.0, 0.0, -340.0), (28.0, 60.0, 90.0))]


def beacon_ring_parts(lod):
    return hab_ring_parts("stn_beacon_hab_ring", BCN_RC, BCN_HW, BCN_HH, 22.0, 44.0, 26.0, 6, lod)


def beacon_ring_hits():
    n = "stn_beacon_hab_ring"
    return ring_hits(n, BCN_RC, 2 * BCN_HW, 2 * BCN_HH, 16) + [hcyl(n, (0, 0, -26.0), (0, 0, 26.0), 44.0)]


# ================================================================ lab A: one big spinning torus, central sphere, spine
LA_RC, LA_R = 300.0, 34.0


def lab_a_parts(lod):
    name = "stn_lab_torus_a"
    P = {name: Part(name)}
    p = P[name]
    zcyl(p, HULL, -50.0, 50.0, 20.0, lod)                                           # the ring's axle
    for sz in (-1, 1):
        band(p, STEEL, sz * 34.0 - 3.0, sz * 34.0 + 3.0, 20.0, 34.0, lod)
    zcyl(p, HULL, -230.0, 240.0, 7.0, lod)
    lattice(p, (0, 0, 42.0), (0, 0, 240.0), 24.0, lod, bay=36.0)
    lattice(p, (0, 0, -42.0), (0, 0, -225.0), 24.0, lod, bay=36.0)
    sphere(p, FACADE, (0, 0, 130.0), 62.0, lod)                                     # the central laboratory sphere
    band(p, LIGHT, 126.0, 134.0, 60.0, 66.0, lod)
    if lod < 2:
        for k in range(16):
            a = 2 * math.pi * k / 16
            box(p, COOL, (63.0 * math.cos(a), 63.0 * math.sin(a), 150.0), (0.6, 6.0, 2.5), rot_z(a))
    dock_hex(p, 240.0, 330.0, 30.0, lod)                                             # dock, mouth +Z at z = 330
    # sample bays: two lattice outriggers carrying exposure racks
    for sy in (-1, 1):
        lattice(p, (0, sy * 12.0, -110.0), (0, sy * 120.0, -110.0), 8.0, lod)
        for j in range(3):
            box(p, ARRAY if j % 2 else LIGHT, (0, sy * (50.0 + 28.0 * j), -110.0), (22.0, 16.0, 22.0))
    reactor(p, -240.0, -330.0, 22.0, lod, wings=4, span=130.0)
    put(p, cargo_apron(lod, 56.0, 80.0, seed=3), rot_z(math.pi), (0.0, -31.0, 285.0))    # under the dock, deck facing -Y
    for z in (-200.0, 60.0, 220.0):
        beacon(p, (12.0, 12.0, z), 2.0)
    return P


def lab_a_hits():
    n = "stn_lab_torus_a"
    return [hcyl(n, (0, 0, -50.0), (0, 0, 50.0), 20.0),
            hbox(n, (0, 0, 140.0), (26.0, 26.0, 200.0)),
            hbox(n, (0, 0, -135.0), (26.0, 26.0, 190.0)),
            hcyl(n, (0, 0, 68.0), (0, 0, 192.0), 62.0),
            hhex(n, (0, 0, 285.0), (30.0 / math.cos(math.pi / 6), 30.0, 45.0), 30.0 / math.cos(math.pi / 6) / 2),
            hbox(n, (0, 0, -110.0), (24.0, 250.0, 24.0)),
            hcyl(n, (0, 0, -236.0), (0, 0, -330.0), 40.0),
            hbox(n, (0, 0, -285.0), (320.0, 6.0, 40.0)),
            hbox(n, (0, 0, -285.0), (6.0, 320.0, 40.0)),
            hbox(n, (0, -45.0, 285.0), (56.0, 30.0, 80.0))]


def lab_a_ring_parts(lod):
    return hab_ring_parts("stn_lab_torus_a_ring", LA_RC, 0, 0, 24.0, 46.0, 28.0, 4, lod, torus_r=LA_R)


def lab_a_ring_hits():
    n = "stn_lab_torus_a_ring"
    return ring_hits(n, LA_RC, 2 * LA_R, 2 * LA_R, 24) + [hcyl(n, (0, 0, -28.0), (0, 0, 28.0), 46.0)]


# ================================================================ lab B: twin static tori stacked on a long truss
LB_RC, LB_R, LB_Z = 165.0, 28.0, (60.0, 230.0)


def lab_b_parts(lod):
    name = "stn_lab_torus_b"
    P = {name: Part(name)}
    p = P[name]
    zcyl(p, HULL, -320.0, 330.0, 8.0, lod)
    lattice(p, (0, 0, -330.0), (0, 0, 330.0), 30.0, lod, bay=44.0)
    for zt in LB_Z:
        torus(p, FACADE, LB_RC, LB_R, lod, t=(0, 0, zt))
        band(p, HULL, zt - 18.0, zt + 18.0, 16.0, 34.0, lod)                       # the torus' hub collar
        for k in range(6):
            a = math.pi / 3 * k + math.pi / 6
            ca, sa = math.cos(a), math.sin(a)
            lattice(p, (34.0 * ca, 34.0 * sa, zt), ((LB_RC - LB_R + 1.0) * ca, (LB_RC - LB_R + 1.0) * sa, zt), 10.0, lod)
            cyl(p, HULL, (34.0 * ca, 34.0 * sa, zt), ((LB_RC - LB_R + 1.0) * ca, (LB_RC - LB_R + 1.0) * sa, zt), 4.0, 8)
        if lod < 2:
            for k in range(12):
                a = 2 * math.pi * k / 12
                box(p, WARM if k % 2 else COOL, ((LB_RC + LB_R + 0.3) * math.cos(a), (LB_RC + LB_R + 0.3) * math.sin(a), zt),
                    (0.6, 18.0, 4.0), rot_z(a))
                beacon(p, ((LB_RC + LB_R) * math.cos(a), (LB_RC + LB_R) * math.sin(a), zt + LB_R * 0.6), 1.5)
    # an inter-torus lab drum and connecting struts between the tori
    zcyl(p, FACADE_L, 110.0, 180.0, 26.0, lod)
    for k in range(4):
        a = math.pi / 2 * k
        ca, sa = math.cos(a), math.sin(a)
        lattice(p, (LB_RC * ca, LB_RC * sa, LB_Z[0] + LB_R), (LB_RC * ca, LB_RC * sa, LB_Z[1] - LB_R), 8.0, lod)
    # the experiment boom off to +X with sample bays and a sensor sphere
    lattice(p, (15.0, 0, -90.0), (330.0, 0, -90.0), 12.0, lod)
    for x in (100.0, 170.0, 240.0):
        box(p, LIGHT, (x, 0, -90.0), (36.0, 20.0, 20.0))
        if lod < 2:
            box(p, COOL, (x, 10.2, -90.0), (28.0, 0.4, 4.0))
    sphere(p, MIRROR, (345.0, 0, -90.0), 16.0, lod)
    beacon(p, (362.0, 0, -90.0), 2.0)
    # the generator at -Z: shield, reactor, two very long radiator wings (along +-Y)
    reactor(p, -330.0, -440.0, 26.0, lod, wings=4, span=170.0)
    # the dock at +Z, mouth z = 420 facing +Z; supply apron on +Y of the dock
    dock_hex(p, 330.0, 420.0, 28.0, lod)
    put(p, cargo_apron(lod, 50.0, 80.0, seed=11), I3, (0.0, 29.0, 375.0))
    for z in (-300.0, 0.0, 300.0):
        beacon(p, (16.0, 16.0, z), 2.0)
    return P


def lab_b_hits():
    n = "stn_lab_torus_b"
    out = [hbox(n, (0, 0, 0.0), (32.0, 32.0, 660.0)),
           hcyl(n, (0, 0, 110.0), (0, 0, 180.0), 26.0),
           hbox(n, (175.0, 0, -90.0), (330.0, 22.0, 22.0)),
           hcyl(n, (0, 0, -326.0), (0, 0, -440.0), 46.0),
           hbox(n, (0, 0, -385.0), (400.0, 6.0, 50.0)),
           hbox(n, (0, 0, -385.0), (6.0, 400.0, 50.0)),
           hhex(n, (0, 0, 375.0), (28.0 / math.cos(math.pi / 6), 28.0, 45.0), 28.0 / math.cos(math.pi / 6) / 2),
           hbox(n, (0, 40.0, 375.0), (50.0, 26.0, 80.0))]
    for zt in LB_Z:
        out += ring_hits(n, LB_RC, 2 * LB_R, 2 * LB_R, 16, zt)
    return out


# ================================================================ lab C: flattened static torus, radial pods, telescope
LC_RC, LC_R = 215.0, 30.0


def lab_c_build(lod):
    """built along +Z (the telescope up +Z), stood up afterwards"""
    p = Part("tmp")
    torus(p, FACADE, LC_RC, LC_R, lod, sr=1.6, sz=0.55)
    ro = LC_RC + LC_R * 1.6
    for k in range(8):                                                              # radial lab pods
        a = 2 * math.pi * k / 8 + math.pi / 8
        ca, sa = math.cos(a), math.sin(a)
        cyl(p, FACADE_L, ((ro - 6.0) * ca, (ro - 6.0) * sa, 0), ((ro + 44.0) * ca, (ro + 44.0) * sa, 0), 13.0,
            max(8, segs(lod)[0] // 4))
        if lod < 2:
            sphere(p, LIGHT, ((ro + 44.0) * ca, (ro + 44.0) * sa, 0), 13.0, 2 if lod > 0 else 1)
        if lod < 2:
            lattice(p, ((ro + 52.0) * ca, (ro + 52.0) * sa, 0), ((ro + 90.0) * ca, (ro + 90.0) * sa, 0), 4.0, lod)  # sensor masts
            beacon(p, ((ro + 91.0) * ca, (ro + 91.0) * sa, 0), 1.6)
    ri = LC_RC - LC_R * 1.6
    for k in range(4):                                                              # spokes
        a = math.pi / 2 * k
        ca, sa = math.cos(a), math.sin(a)
        lattice(p, (32.0 * ca, 32.0 * sa, 0), ((ri + 1.0) * ca, (ri + 1.0) * sa, 0), 12.0, lod)
        cyl(p, HULL, (32.0 * ca, 32.0 * sa, 0), ((ri + 1.0) * ca, (ri + 1.0) * sa, 0), 4.0, 8)
    zcyl(p, HULL, -24.0, 24.0, 32.0, lod)                                           # the hub
    # the telescope: an open 8-longeron lattice barrel, the primary mirror at its foot, the secondary on a spider
    ring_lattice(p, 30.0, 420.0, 58.0, 8 if lod < 2 else 6, lod, bay=39.0, t=1.8)
    band(p, HULL, 24.0, 34.0, 0.0, 62.0, lod)
    dish(p, (0, 0, 36.0), (0.0, 0.0, 1.0), 54.0, 6.0, lod, mat=MIRROR)
    box(p, DARK, (0, 0, 405.0), (16.0, 16.0, 10.0))                                  # the secondary mirror cell
    for k in range(4):
        a = math.pi / 2 * k + math.pi / 4
        rod(p, STEEL, (0, 0, 405.0), (58.0 * math.cos(a), 58.0 * math.sin(a), 405.0), 1.2)
    band(p, LIGHT, 412.0, 420.0, 58.0, 62.0, lod)                                    # the barrel's top baffle ring
    if lod < 2:
        for k in range(4):
            a = math.pi / 2 * k
            beacon(p, (61.0 * math.cos(a), 61.0 * math.sin(a), 421.0), 2.0)
    # below: spine, the generator, the dock (mouth -Z at z = -340), a supply apron
    zcyl(p, HULL, -250.0, -24.0, 6.0, lod)
    lattice(p, (0, 0, -24.0), (0, 0, -250.0), 22.0, lod, bay=34.0)
    reactor(p, -120.0, -200.0, 18.0, lod, wings=2, span=150.0)
    dock_hex(p, -250.0, -340.0, 28.0, lod)
    put(p, cargo_apron(lod, 50.0, 80.0, seed=5), rot_z(-math.pi / 2), (33.0, 0.0, -295.0))
    return p


def lab_c_parts(lod):
    name = "stn_lab_torus_c"
    P = {name: Part(name)}
    merge(P[name], placed(lab_c_build(lod), UP_Y, (0, 0, 0)))
    return P


def lab_c_hits():
    n = "stn_lab_torus_c"
    ro = LC_RC + LC_R * 1.6
    out = ring_hits_y(n, LC_RC, 2 * LC_R * 1.6, 2 * LC_R * 0.55, 20)
    for k in range(8):
        a = 2 * math.pi * k / 8 + math.pi / 8
        # build-frame (x, y, 0) -> model (x, 0, -y)
        x, z = (ro + 19.0) * math.cos(a), -(ro + 19.0) * math.sin(a)
        out.append(hbox(n, (x, 0.0, z), (50.0, 26.0, 26.0), (0.0, math.degrees(a), 0.0)))
    out += [hcyl(n, (0, -24.0, 0), (0, 24.0, 0), 32.0),
            hcyl(n, (0, 24.0, 0), (0, 420.0, 0), 60.0),
            hbox(n, (0, -137.0, 0), (24.0, 226.0, 24.0)),
            hcyl(n, (0, -118.0, 0), (0, -200.0, 0), 32.0),
            hbox(n, (0, -160.0, 0), (380.0, 40.0, 6.0)),
            hhex(n, (0, -295.0, 0), (28.0 / math.cos(math.pi / 6), 28.0, 45.0), 28.0 / math.cos(math.pi / 6) / 2, (90.0, 0.0, 0.0)),
            hbox(n, (43.0, -295.0, 0), (26.0, 80.0, 50.0))]
    return out


# ================================================================ surface lab (moon / small asteroid), ground y = 0
LS_RC, LS_R, LS_Z = 150.0, 24.0, 70.0           # torus radius, tube radius, its centre height


def lab_surface_build(lod):
    """built with the ground at z = 0 and up = +Z, stood up afterwards (ground y = 0)"""
    p = Part("tmp")
    torus(p, FACADE, LS_RC, LS_R, lod, t=(0, 0, LS_Z), sr=1.3, sz=0.8)
    ro = LS_RC + LS_R * 1.3
    nleg = 12 if lod < 2 else 6
    for k in range(nleg):                                                           # lattice support columns
        a = 2 * math.pi * k / nleg
        ca, sa = math.cos(a), math.sin(a)
        lattice(p, (LS_RC * ca, LS_RC * sa, 0.0), (LS_RC * ca, LS_RC * sa, LS_Z - LS_R * 0.8 + 1.0), 7.0, lod)
        box(p, LIGHT, (LS_RC * ca, LS_RC * sa, 1.5), (16.0, 16.0, 3.0), rot_z(a))      # footing pads
        if lod < 2:
            box(p, WARM if k % 2 else COOL, ((ro + 0.3) * ca, (ro + 0.3) * sa, LS_Z), (0.6, 30.0, 4.0), rot_z(a))
    # the central lab core and its spokes; an antenna mast on top
    zcyl(p, FACADE_L, 0.0, 120.0, 40.0, lod)
    band(p, LIGHT, 0.0, 6.0, 40.0, 48.0, lod)
    sphere(p, LIGHT, (0, 0, 120.0), 40.0, 2 if lod > 0 else 1)
    ri = LS_RC - LS_R * 1.3
    for k in range(4):
        a = math.pi / 2 * k + math.pi / 4
        ca, sa = math.cos(a), math.sin(a)
        lattice(p, (40.0 * ca, 40.0 * sa, LS_Z), ((ri + 1.0) * ca, (ri + 1.0) * sa, LS_Z), 10.0, lod)
        cyl(p, HULL, (40.0 * ca, 40.0 * sa, LS_Z), ((ri + 1.0) * ca, (ri + 1.0) * sa, LS_Z), 4.0, 8)
    lattice(p, (0, 0, 155.0), (0, 0, 290.0), 9.0, lod)
    dish(p, (12.0, 0, 230.0), (0.6, 0.0, 0.8), 22.0, 6.0, lod)
    beacon(p, (0, 0, 292.0), 3.0)
    # the reactor 320 m away at +X, on a lattice platform, coupled by a lattice power bridge
    lattice(p, (48.0, 0, 30.0), (295.0, 0, 30.0), 10.0, lod)
    cyl(p, HULL, (48.0, 0, 30.0), (295.0, 0, 30.0), 2.5, 8)
    for sx in (-1, 1):
        for sy in (-1, 1):
            lattice(p, (320.0 + sx * 22.0, sy * 22.0, 0.0), (320.0 + sx * 22.0, sy * 22.0, 20.0), 5.0, lod)
    box(p, HULL, (320.0, 0, 21.5), (56.0, 56.0, 3.0))
    zcyl(p, HULL, 23.0, 100.0, 20.0, lod, x=320.0)
    for z in (40.0, 85.0):
        band(p, HAZ, z - 2.0, z + 2.0, 20.0, 21.5, lod, t=(320.0, 0, 0))
    beacon(p, (320.0, 0, 101.0), 2.5)
    for sy in (-1, 1):                                                              # vertical radiator walls
        radiator_wing(p, (320.0, sy * 22.0, 60.0), (320.0, sy * 160.0, 60.0), (0.0, 0.0, 1.0), 40.0, lod, n_panels=4)
        for y in (60.0, 120.0):
            lattice(p, (320.0, sy * y, 0.0), (320.0, sy * y, 60.0), 5.0, lod)
    # the landing pad (the "dock") at -X on lattice legs: top at z = 14, lit edge + approach lights
    xp = -300.0
    for k in range(6):
        a = math.pi / 3 * k
        lattice(p, (xp + 50.0 * math.cos(a), 50.0 * math.sin(a), 0.0), (xp + 50.0 * math.cos(a), 50.0 * math.sin(a), 11.0), 5.0, lod)
    lathe(p, HULL, crect(0.0, 62.0, 11.0, 14.0), max(12, segs(lod)[0] // 2), I3, (xp, 0, 0))
    if lod < 2:
        lathe(p, HAZ, crect(20.0, 23.0, 14.0, 14.3), max(12, segs(lod)[0] // 2), I3, (xp, 0, 0))
        for k in range(16):
            a = 2 * math.pi * k / 16
            box(p, FLOOD, (xp + 60.0 * math.cos(a), 60.0 * math.sin(a), 14.6), (2.0, 2.0, 1.2))
    lattice(p, (-48.0, 0, 8.0), (xp + 62.0, 0, 8.0), 8.0, lod)                         # the walkway truss to the core
    cyl(p, HULL, (-48.0, 0, 8.0), (xp + 62.0, 0, 8.0), 3.5, 8)
    # supply apron + a cargo shed next to the pad
    put(p, cargo_apron(lod, 60.0, 90.0, seed=13), rot_x(math.pi / 2), (xp, 105.0, 3.0))
    box(p, LIGHT, (xp - 20.0, -110.0, 15.0), (40.0, 30.0, 30.0))
    box(p, DARK, (xp - 20.0, -94.8, 13.0), (30.0, 0.4, 24.0))
    beacon(p, (LS_RC, 0, LS_Z + LS_R), 2.0)
    return p


def lab_surface_parts(lod):
    name = "stn_lab_surface"
    P = {name: Part(name)}
    merge(P[name], placed(lab_surface_build(lod), UP_Y, (0, 0, 0)))
    return P


def lab_surface_hits():
    n = "stn_lab_surface"
    out = ring_hits_y(n, LS_RC, 2 * LS_R * 1.3, 2 * LS_R * 0.8, 16, LS_Z)
    out += [hcyl(n, (0, 0.0, 0), (0, 160.0, 0), 40.0),
            hbox(n, (0, 222.0, 0), (10.0, 136.0, 10.0)),
            hbox(n, (171.0, 30.0, 0), (250.0, 12.0, 12.0)),
            hbox(n, (320.0, 11.0, 0), (56.0, 23.0, 56.0)),
            hcyl(n, (320.0, 23.0, 0), (320.0, 100.0, 0), 21.0),
            hbox(n, (320.0, 60.0, 0), (6.0, 120.0, 330.0)),
            hcyl(n, (-300.0, 0.0, 0), (-300.0, 14.0, 0), 62.0),
            hbox(n, (-174.0, 8.0, 0), (252.0, 9.0, 9.0)),
            hbox(n, (-300.0, 12.0, -105.0), (60.0, 24.0, 90.0)),
            hbox(n, (-320.0, 15.0, 110.0), (40.0, 30.0, 30.0))]
    return out


ASSETS = {
    "stn_beacon_core": beacon_core_parts,
    "stn_beacon_hab_ring": beacon_ring_parts,
    "stn_lab_torus_a": lab_a_parts,
    "stn_lab_torus_a_ring": lab_a_ring_parts,
    "stn_lab_torus_b": lab_b_parts,
    "stn_lab_torus_c": lab_c_parts,
    "stn_lab_surface": lab_surface_parts,
}
HITS = {
    "stn_beacon_core": beacon_core_hits,
    "stn_beacon_hab_ring": beacon_ring_hits,
    "stn_lab_torus_a": lab_a_hits,
    "stn_lab_torus_a_ring": lab_a_ring_hits,
    "stn_lab_torus_b": lab_b_hits,
    "stn_lab_torus_c": lab_c_hits,
    "stn_lab_surface": lab_surface_hits,
}
ROOTS = {k: "stations" for k in ASSETS}
_SRC = " (tools/models/units/space_stations.py)."
PASSPORTS = {
    "stn_beacon_core": ("Маяк: основной корпус", "station",
                        "Навигационный маяк-ретранслятор ~960 м: ферменный хребет, антенный комплекс, реактор на выносной "
                        "ферме, док (устье z = -400, к -Z), стоянка двух истребителей охраны, грузовая площадка. Ось "
                        "вращения жилого кольца - +Z через начало координат" + _SRC),
    "stn_beacon_hab_ring": ("Маяк: вращающийся жилой модуль", "station",
                            "Жилое кольцо маяка Ø336 м, спицы-фермы, ступица; вращается вокруг своей +Z, ставится в "
                            "начало координат основного корпуса" + _SRC),
    "stn_lab_torus_a": ("Исследовательская лаборатория А (тор)", "station",
                        "Неподвижная часть: ось с подшипником тора, центральная сфера-лаборатория, док (+Z), реактор с "
                        "радиаторами (-Z), стойки с образцами, грузовая площадка" + _SRC),
    "stn_lab_torus_a_ring": ("Лаборатория А: вращающийся тор", "station",
                             "Тор Ø668 м на четырёх спицах-фермах; вращается вокруг своей +Z, ставится в начало "
                             "координат лаборатории А" + _SRC),
    "stn_lab_torus_b": ("Исследовательская лаборатория Б (двойной тор)", "station",
                        "Два неподвижных тора (микрогравитация) на длинной ферме, реактор с большими радиаторами, "
                        "экспериментальная штанга, док (+Z), грузовая площадка" + _SRC),
    "stn_lab_torus_c": ("Исследовательская лаборатория В (телескоп)", "station",
                        "Плоский тор с радиальными модулями, огромный ферменный телескоп вверх (+Y), реактор и док "
                        "снизу (устье y = -340)" + _SRC),
    "stn_lab_surface": ("Исследовательская лаборатория Г (наземная)", "station",
                        "Для спутника или малого астероида: грунт y = 0, тор на ферменных опорах, ядро с мачтой, "
                        "реактор в 320 м, посадочная площадка и склад снабжения" + _SRC),
}
