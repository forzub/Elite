"""Hub construction kit (author 2026-11-19: hubs under construction are rebuilt from construction-stage modules -
foundation -> bare frame -> partly clad -> nearly done - with cranes, material stacks, temporary loading platforms and
worker barracks «абсолютно барачного внешнего вида»).

New hubc_* hub modules (no existing kit is touched). Frame of every module: metres, +Y up, the origin is the centre of
the footprint on the deck plane y = 0 (foundations go a few metres below). The ids, origins and sizes are a contract
with the city assembly / traffic code - see docs/HUB_CONSTRUCTION_MODELS.md.

Plausible construction logic: raft + pile caps + starter bars first, then the concrete core (self-climbing formwork) runs
two floors ahead of the steel frame, decks follow the frame, cladding follows the decks from the bottom; the working
floors carry climbing screens, catch nets and edge rails; hoists and scaffolds on one face; red aviation beacons on
everything tall, work floodlights on the working levels. Clad faces use the shared facade material (eq_hub_facade*: the
shader draws the window grid), so a bare frame has no windows.

Cranes (hubc_crane_*, hubc_gantry) are for planet / moon surfaces with gravity only; free-space hubs use the tug,
the clawship, worker pods and jigs (author correction 2026-11-19).
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon  # noqa: F401
from units.hub_belt import CONT, GRATE, TANK_W  # noqa: F401
from units.hub_mega import FACADE, FACADE_L

register_materials({
    "eq_hubc_concrete": ((0.56, 0.56, 0.54), (0.05, 0.05, 0.05), 8),       # fresh concrete (light grey)
    "eq_hubc_rebar": ((0.36, 0.19, 0.11), (0.12, 0.10, 0.08), 20),         # rusty rebar
    "eq_hubc_yellow": ((0.86, 0.66, 0.06), (0.25, 0.22, 0.10), 40),        # safety yellow (cranes, screens, hoists)
    "eq_hubc_net": ((0.92, 0.42, 0.08), (0.08, 0.08, 0.08), 10),           # orange safety nets / straps
    "eq_hubc_tarp_blue": ((0.14, 0.27, 0.50), (0.06, 0.06, 0.08), 12),     # blue tarp / scaffold sheeting
    "eq_hubc_tarp_grey": ((0.38, 0.40, 0.40), (0.05, 0.05, 0.05), 10),     # grey tarp
    "eq_hubc_primer": ((0.42, 0.15, 0.09), (0.12, 0.10, 0.09), 25),        # red-oxide primed steel
    "eq_hubc_barrack": ((0.40, 0.43, 0.38), (0.15, 0.15, 0.14), 30),       # corrugated barrack metal (faded green-grey)
    "eq_hubc_barrack_b": ((0.55, 0.52, 0.44), (0.15, 0.15, 0.14), 30),     # replacement unit (faded beige)
    "eq_hubc_plywood": ((0.62, 0.48, 0.30), (0.04, 0.04, 0.04), 8),        # plywood, formwork, timber dunnage
    "eq_hubc_cont_grey": ((0.35, 0.36, 0.37), (0.10, 0.10, 0.10), 20),     # dull containers
    "eq_hubc_cont_green": ((0.20, 0.32, 0.24), (0.10, 0.10, 0.10), 20),
    "eq_hubc_foam": ((0.10, 0.10, 0.11), (0.03, 0.03, 0.03), 6),           # rubber / foam bumpers, fenders
    "eq_hubc_nav_green": ((0.20, 1.00, 0.35), (0.10, 0.10, 0.10), 20),     # starboard nav light (emissive)
})
CONC, REBAR, YEL, NET, TARP, TARPG, PRIM, BARK, BARK2, PLY, CGRY, CGRN, FOAM, NAVG = (
    "eq_hubc_concrete", "eq_hubc_rebar", "eq_hubc_yellow", "eq_hubc_net", "eq_hubc_tarp_blue", "eq_hubc_tarp_grey",
    "eq_hubc_primer", "eq_hubc_barrack", "eq_hubc_barrack_b", "eq_hubc_plywood", "eq_hubc_cont_grey",
    "eq_hubc_cont_green", "eq_hubc_foam", "eq_hubc_nav_green")
CONTS = tuple(CONT) + (CGRY, CGRN)
TAU = 2 * math.pi
FH = 4.0                       # floor height (the facade shader's window grid is 4 m too)


# ================================================================ local kernel
def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _mul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _len(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def _lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def _hash(*a):
    """a deterministic pseudo-random number in [0, 1)"""
    h = 2166136261
    for v in a:
        for ch in repr(round(v, 3) if isinstance(v, float) else v):
            h = ((h ^ ord(ch)) * 16777619) & 0xFFFFFFFF
    return (h % 100000) / 100000.0


def merge(dst, src):
    off = len(dst.v)
    dst.v += src.v
    dst.n += src.n
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + off for i in f) for f in fl)


def put(dst, src, x, y, z, ang=0.0):
    """merge a copy of src turned by ang (rad, about Y) and moved to (x, y, z)"""
    merge(dst, placed(src, rot_y(ang), (x, y, z)))


def ab(p, mat, x0, y0, z0, x1, y1, z1):
    """an axis-aligned box between two corners"""
    box(p, mat, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)))


def bar(p, mat, a, b, w, h=None):
    """a square bar (box) from a to b"""
    d = _sub(b, a)
    L = _len(d)
    if L < 1e-6:
        return
    box(p, mat, _lerp(a, b, 0.5), (w, h or w, L), axis_to(norm(d)))


def oslab(p, mat, a, b, side, w, t):
    """a box from a to b (its length), w wide along `side`, t thick across"""
    d = _sub(b, a)
    L = _len(d)
    dn = norm(d)
    k = side[0] * dn[0] + side[1] * dn[1] + side[2] * dn[2]
    s = norm(_sub(side, _mul(dn, k)))
    u = _cross(dn, s)
    R = [[s[0], u[0], dn[0]], [s[1], u[1], dn[1]], [s[2], u[2], dn[2]]]
    box(p, mat, _lerp(a, b, 0.5), (w, t, L), R)


def quad4(p, mat, pts, hint):
    p.face(mat, [p.vert(q, hint) for q in pts], hint)


def tri(p, mat, a, b, c, hint):
    p.face(mat, [p.vert(a, hint), p.vert(b, hint), p.vert(c, hint)], hint)


def flood(p, pos, s=1.0):
    box(p, FLOOD, pos, (1.2 * s, 0.6 * s, 0.8 * s))


def lat(p, mat, bx, bz, y0, y1, h0, h1, lod, bays=None, chord=None):
    """a vertical 4-leg lattice (half widths h0 at y0, h1 at y1); lod 2: a tapered solid"""
    bays = bays or max(2, int(round((y1 - y0) / max(3.0, h0 + h1))))
    c = chord or max(0.18, (h0 + h1) * 0.07)
    corners = ((-1, -1), (1, -1), (1, 1), (-1, 1))
    if lod >= 2:
        p0 = [(bx + sx * h0, y0, bz + sz * h0) for sx, sz in corners]
        p1 = [(bx + sx * h1, y1, bz + sz * h1) for sx, sz in corners]
        for k in range(4):
            a0, a1, b1, b0 = p0[k], p0[(k + 1) % 4], p1[(k + 1) % 4], p1[k]
            m = _lerp(_lerp(a0, a1, 0.5), _lerp(b0, b1, 0.5), 0.5)
            quad4(p, mat, [a0, a1, b1, b0], norm((m[0] - bx, 0.0, m[2] - bz)))
        quad4(p, mat, p1, (0, 1, 0))
        return
    if lod == 1:
        bays = max(2, bays // 2)
    legs = [((bx + sx * h0, y0, bz + sz * h0), (bx + sx * h1, y1, bz + sz * h1)) for sx, sz in corners]
    for a, b in legs:
        bar(p, mat, a, b, c)
    for i in range(bays + 1):
        ring = [_lerp(a, b, i / bays) for a, b in legs]
        for k in range(4):
            bar(p, mat, ring[k], ring[(k + 1) % 4], c * 0.75)
        if i < bays:
            ring1 = [_lerp(a, b, (i + 1) / bays) for a, b in legs]
            for k in range(4):
                if i % 2 == 0 or lod == 0:
                    bar(p, mat, ring[k], ring1[(k + 1) % 4], c * 0.55)
                if i % 2 == 1 or lod == 0:
                    bar(p, mat, ring[(k + 1) % 4], ring1[k], c * 0.55)


def btruss(p, mat, a, b, w, h, lod, up=(0.0, 1.0, 0.0), chord=None, bay=None):
    """a box lattice girder from a to b (w wide, h deep): 4 chords, rings, zigzag diagonals; lod 2: a solid box"""
    d = _sub(b, a)
    L = _len(d)
    if L < 1e-6:
        return
    dn = norm(d)
    side = _cross(dn, up)
    if _len(side) < 1e-6:
        side = _cross(dn, (1.0, 0.0, 0.0))
    side = norm(side)
    upv = norm(_cross(side, dn))
    if lod >= 2:
        R = [[side[0], upv[0], dn[0]], [side[1], upv[1], dn[1]], [side[2], upv[2], dn[2]]]
        box(p, mat, _lerp(a, b, 0.5), (w, h, L), R)
        return
    c = chord or max(0.2, min(w, h) * 0.09)
    n = max(1, int(round(L / (bay or max(w, h)))))
    if lod == 1:
        n = max(1, n // 2)
    ks = ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5))
    off = [_add(_mul(side, k[0] * w), _mul(upv, k[1] * h)) for k in ks]
    for o in off:
        bar(p, mat, _add(a, o), _add(b, o), c)
    for i in range(n + 1):
        q = _lerp(a, b, i / n)
        for j in range(4):
            bar(p, mat, _add(q, off[j]), _add(q, off[(j + 1) % 4]), c * 0.75)
        if i < n:
            q1 = _lerp(a, b, (i + 1) / n)
            for j in range(4):
                k0, k1 = (j, (j + 1) % 4) if i % 2 == 0 else ((j + 1) % 4, j)
                bar(p, mat, _add(q, off[k0]), _add(q1, off[k1]), c * 0.6)


def ptruss(p, mat, a0, a1, b0, b1, n, lod, c):
    """a planar truss: top chord a0-a1, bottom chord b0-b1, n panels (verticals + alternating diagonals)"""
    bar(p, mat, a0, a1, c)
    bar(p, mat, b0, b1, c)
    if lod >= 2:
        return
    if lod == 1:
        n = max(2, n // 2)
    for i in range(n + 1):
        t = i / n
        ta, tb = _lerp(a0, a1, t), _lerp(b0, b1, t)
        bar(p, mat, ta, tb, c * 0.6)
        if i < n:
            t1 = (i + 1) / n
            if i % 2 == 0:
                bar(p, mat, ta, _lerp(b0, b1, t1), c * 0.55)
            else:
                bar(p, mat, tb, _lerp(a0, a1, t1), c * 0.55)


def light_mast(p, lod, x, z, h, y=0.0, n=4):
    """a floodlight mast: pole, head bar with lamps, a red beacon on the tall ones"""
    box(p, STEEL, (x, y + h / 2, z), (0.5, h, 0.5))
    if lod < 2:
        ab(p, STEEL, x - 1.6, y + h - 0.3, z - 0.3, x + 1.6, y + h + 0.1, z + 0.3)
        for i in range(n):
            box(p, FLOOD, (x - 1.2 + 2.4 * i / max(1, n - 1), y + h + 0.35, z), (0.7, 0.5, 0.7))
        if h >= 12.0:
            beacon(p, (x, y + h + 1.0, z), 0.5)
    else:
        box(p, FLOOD, (x, y + h + 0.3, z), (2.4, 0.6, 0.7))


def site_marks(p, lod, FW, FD, inset=1.2):
    """the site boundary: hazard dashes on the deck along the footprint edge"""
    if lod >= 2:
        return
    step = 4.0 if lod == 0 else 8.0
    hw, hd = FW / 2 - inset, FD / 2 - inset
    for (x0, z0, x1, z1) in ((-hw, -hd, hw, -hd), (-hw, hd, hw, hd), (-hw, -hd, -hw, hd), (hw, -hd, hw, hd)):
        L = math.hypot(x1 - x0, z1 - z0)
        n = max(1, int(L / (2 * step)))
        for i in range(n):
            t = (i + 0.25) / n
            c = (x0 + (x1 - x0) * t, 0.04, z0 + (z1 - z0) * t)
            box(p, HAZ, c, (step if z0 == z1 else 0.5, 0.08, 0.5 if z0 == z1 else step))


def rebar_mesh(p, lod, x0, z0, x1, z1, y):
    """a rebar mat (rusty grid) over a not yet poured area"""
    if lod >= 2:
        return
    step = 1.0 if close() else (3.0 if lod == 0 else 6.0)
    w = 0.12 if close() else 0.25
    nx, nz = max(1, int((x1 - x0) / step)), max(1, int((z1 - z0) / step))
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx
        ab(p, REBAR, x - w / 2, y, z0, x + w / 2, y + w, z1)
    for j in range(nz + 1):
        z = z0 + (z1 - z0) * j / nz
        ab(p, REBAR, x0, y + w, z - w / 2, x1, y + 2 * w, z + w / 2)


def starters(p, pts, y, h):
    """starter bars sticking out of fresh concrete (close level only)"""
    if not close():
        return
    for (x, z) in pts:
        ab(p, REBAR, x - 0.06, y, z - 0.06, x + 0.06, y + h, z + 0.06)


# ================================================================ materials on site: stacks, containers, cabins
CONT_H, CONT_W = 2.59, 2.44


def container(p, lod, c, L, mat, along_x=True):
    """an ISO container (L = 6.06 / 12.19 m) standing on c"""
    sx, sz = (L, CONT_W) if along_x else (CONT_W, L)
    box(p, mat, (c[0], c[1] + CONT_H / 2, c[2]), (sx, CONT_H, sz))
    if lod == 0:
        e = (L / 2 + 0.03)
        if along_x:
            box(p, STEEL, (c[0] + e, c[1] + CONT_H / 2, c[2]), (0.06, CONT_H * 0.9, 0.1))
        else:
            box(p, STEEL, (c[0], c[1] + CONT_H / 2, c[2] + e), (0.1, CONT_H * 0.9, 0.06))
        if close():
            n = int(L / 0.9)
            for i in range(n):
                t = -L / 2 + (i + 0.5) * L / n
                for s in (-1, 1):
                    if along_x:
                        box(p, mat, (c[0] + t, c[1] + CONT_H / 2, c[2] + s * (CONT_W / 2 + 0.03)), (0.3, CONT_H * 0.9, 0.06))
                    else:
                        box(p, mat, (c[0] + s * (CONT_W / 2 + 0.03), c[1] + CONT_H / 2, c[2] + t), (0.06, CONT_H * 0.9, 0.3))


def containers_part(lod, cols=3, rows=5, maxh=3, seed=1):
    """a container block: cols x rows stacks of 40-ft boxes (some 20-ft pairs), 1..maxh high, along X"""
    q = Part("t")
    pitchx, pitchz = 13.2, 2.85
    for i in range(cols):
        for j in range(rows):
            x = (i - (cols - 1) / 2) * pitchx
            z = (j - (rows - 1) / 2) * pitchz
            hgt = 1 + int(_hash(seed, i, j) * maxh)
            if lod >= 2:
                box(q, CONTS[int(_hash(seed, i, j, 9) * len(CONTS))], (x, hgt * CONT_H / 2, z), (12.19, hgt * CONT_H, CONT_W))
                continue
            for k in range(hgt):
                mat = CONTS[int(_hash(seed, i, j, k) * len(CONTS))]
                if _hash(seed, i, j, k, 5) < 0.25:
                    for s in (-1, 1):
                        container(q, lod, (x + s * 3.06, k * CONT_H, z), 6.06, CONTS[int(_hash(seed, i, j, k, s) * len(CONTS))])
                else:
                    container(q, lod, (x, k * CONT_H, z), 12.19, mat)
    return q


def cabin(p, lod, x, y, z, ang_x=True, lit=True):
    """a site office / welfare cabin (20-ft container body, window, door)"""
    container(p, lod, (x, y, z), 6.06, BARK, ang_x)
    if lod < 2:
        if ang_x:
            box(p, WARM if lit else DARK, (x - 1.2, y + 1.5, z + CONT_W / 2 + 0.05), (1.4, 0.9, 0.08))
            box(p, DARK, (x + 1.8, y + 1.1, z + CONT_W / 2 + 0.05), (0.9, 2.0, 0.08))
        else:
            box(p, WARM if lit else DARK, (x + CONT_W / 2 + 0.05, y + 1.5, z - 1.2), (0.08, 0.9, 1.4))


def ibeam(p, lod, c, L, h=0.9, w=0.5, mat=PRIM):
    """an I-beam along X lying on c"""
    x, y, z = c
    if lod >= 1:
        ab(p, mat, x - L / 2, y, z - w / 2, x + L / 2, y + h, z + w / 2)
        return
    ab(p, mat, x - L / 2, y, z - w / 2, x + L / 2, y + 0.08, z + w / 2)
    ab(p, mat, x - L / 2, y + h - 0.08, z - w / 2, x + L / 2, y + h, z + w / 2)
    ab(p, mat, x - L / 2, y + 0.08, z - 0.05, x + L / 2, y + h - 0.08, z + 0.05)


def stack_beams_part(lod, L=28.0, piles=2, n=5, layers=6):
    """steel profile stacks on timber dunnage: piles side by side (along Z), beams along X"""
    q = Part("t")
    pw = n * 1.0
    for pile in range(piles):
        zc = (pile - (piles - 1) / 2) * (pw + 1.5)
        if lod >= 2:
            ab(q, PRIM, -L / 2, 0, zc - pw / 2, L / 2, layers * 1.1, zc + pw / 2)
            continue
        y = 0.0
        for layer in range(layers):
            for x in (-L * 0.4, 0.0, L * 0.4):
                ab(q, PLY, x - 0.15, y, zc - pw / 2 - 0.2, x + 0.15, y + 0.2, zc + pw / 2 + 0.2)
            y += 0.2
            for i in range(n):
                z = zc - pw / 2 + (i + 0.5) * pw / n
                ibeam(q, lod, (0.0, y, z), L - (0.0 if (layer + i) % 3 else 1.5), mat=PRIM if (pile + layer) % 4 else STEEL)
            y += 0.9
        if close():
            for x in (-L * 0.3, L * 0.3):                                              # lashing straps
                ab(q, NET, x - 0.05, 0, zc - pw / 2 - 0.25, x + 0.05, y + 0.05, zc - pw / 2 - 0.2)
                ab(q, NET, x - 0.05, y, zc - pw / 2 - 0.2, x + 0.05, y + 0.05, zc + pw / 2 + 0.2)
                ab(q, NET, x - 0.05, 0, zc + pw / 2 + 0.2, x + 0.05, y + 0.05, zc + pw / 2 + 0.25)
    return q


def stack_panels_part(lod):
    """facade panel racks (A-frames with panels leaning both sides) and pallets of sheets: ~25 x 10 x 8"""
    q = Part("t")
    for r, x in enumerate((-8.4, 0.0, 8.4)):
        ab(q, STEEL, x - 3.6, 0, -1.6, x + 3.6, 0.3, 1.6)                                    # rack base
        if lod < 2:
            for xx in (x - 3.2, x + 3.2):
                bar(q, STEEL, (xx, 0.3, -1.4), (xx, 7.6, 0), 0.25)
                bar(q, STEEL, (xx, 0.3, 1.4), (xx, 7.6, 0), 0.25)
            bar(q, STEEL, (x - 3.4, 7.6, 0), (x + 3.4, 7.6, 0), 0.25)
            for s in (-1, 1):
                for k in range(4 if lod == 0 else 2):
                    mat = (FACADE_L, DARK, HULL)[(r + k) % 3]
                    box(q, mat, (x, 3.9, s * (0.9 + 0.32 * k)), (6.6, 7.0, 0.2), rot_x(-s * 0.12))
        else:
            ab(q, FACADE_L, x - 3.3, 0.3, -1.3, x + 3.3, 7.4, 1.3)
    for i, x in enumerate((-9.0, -3.0, 3.0, 9.0)):                                            # pallets of sheets
        hgt = 1.0 + 1.4 * _hash(i, 3)
        ab(q, PLY, x - 2.4, 0, 3.0, x + 2.4, 0.2, 4.6)
        ab(q, (PLY, TARPG, LIGHT, PLY)[i], x - 2.3, 0.2, 3.05, x + 2.3, 0.2 + hgt, 4.55)
    return q


def pallets_part(lod, nx=3, nz=2, seed=2):
    """a laydown of pallets on a deck: sheet bundles, bagged goods under tarps, rebar bundles (~12 x 8)"""
    q = Part("t")
    for i in range(nx):
        for j in range(nz):
            x, z = (i - (nx - 1) / 2) * 4.0, (j - (nz - 1) / 2) * 3.6
            h = 0.8 + 1.2 * _hash(seed, i, j)
            k = int(_hash(seed, j, i) * 4)
            if k == 3 and lod < 2:                                                         # rebar bundle
                for m in range(4):
                    ab(q, REBAR, x - 1.8, 0.15 + 0.3 * m, z - 0.9 + 0.45 * (m % 2), x + 1.8, 0.4 + 0.3 * m, z - 0.5 + 0.45 * (m % 2))
                ab(q, PLY, x - 1.6, 0, z - 1.2, x + 1.6, 0.15, z + 1.2)
                continue
            ab(q, PLY, x - 1.6, 0, z - 1.4, x + 1.6, 0.15, z + 1.4)
            ab(q, (TARP, TARPG, PLY)[k % 3], x - 1.5, 0.15, z - 1.3, x + 1.5, 0.15 + h, z + 1.3)
    return q


def stack(p, lod, kind, x, y, z, ang=0.0):
    src = {"beams": lambda: stack_beams_part(lod),
           "beams_s": lambda: stack_beams_part(lod, L=12.0, piles=1, n=4, layers=3),
           "panels": lambda: stack_panels_part(lod),
           "pallets": lambda: pallets_part(lod),
           "cont": lambda: containers_part(lod, 3, 5, 3, int(x * 7 + z)),
           "cont_s": lambda: containers_part(lod, 1, 3, 2, int(x * 3 + z)),
           "girders": lambda: girders_part(lod)}[kind]()
    put(p, src, x, y, z, ang)


def girders_part(lod):
    """three plate-girder segments on timber blocks (25 m along X)"""
    q = Part("t")
    for k, z in enumerate((-4.0, 0.0, 4.0)):
        for x in (-9.0, 9.0):
            ab(q, PLY, x - 0.6, 0, z - 1.6, x + 0.6, 0.5, z + 1.6)
        ab(q, PRIM, -12.5, 0.5, z - 1.5, 12.5, 6.0, z + 1.5)
        if lod == 0:
            for x in range(-10, 11, 5):
                ab(q, PRIM, x - 0.1, 0.5, z - 1.65, x + 0.1, 6.0, z + 1.65)               # stiffeners
    return q


def cabins_row(p, lod, x, z, n=3):
    for i in range(n):
        cabin(p, lod, x + (i - (n - 1) / 2) * 6.6, 0.0, z, True, _hash(x, i) > 0.3)


# ================================================================ generic building stages (tower / hab / slab)
def _ring_boxes(p, mat, X0, X1, Z0, Z1, y0, y1, hole):
    """the rectangle X0..X1 x Z0..Z1 minus the hole rectangle, as up to four boxes"""
    hx0, hz0, hx1, hz1 = hole
    hx0, hx1, hz0, hz1 = max(hx0, X0), min(hx1, X1), max(hz0, Z0), min(hz1, Z1)
    if hx0 > X0:
        ab(p, mat, X0, y0, Z0, hx0, y1, Z1)
    if hx1 < X1:
        ab(p, mat, hx1, y0, Z0, X1, y1, Z1)
    if hz0 > Z0:
        ab(p, mat, hx0, y0, Z0, hx1, y1, hz0)
    if hz1 < Z1:
        ab(p, mat, hx0, y0, hz1, hx1, y1, Z1)


def scaffold(p, lod, x0, x1, y0, y1, zf, sg, tarp_to=None):
    """a tube-and-fitting scaffold in front of a Z face (zf = facade plane, sg = outward sign)"""
    zi, zo = zf + sg * 0.5, zf + sg * 1.9
    if lod >= 2:
        ab(p, GRATE, x0, y0, min(zi, zo), x1, y1, max(zi, zo))
        return
    sx = 2.5 if close() else 5.0
    sy = 2.0 if close() else 4.0
    n = max(1, int((x1 - x0) / sx))
    for i in range(n + 1):
        x = x0 + (x1 - x0) * i / n
        for z in (zi, zo):
            ab(p, STEEL, x - 0.07, y0, z - 0.07, x + 0.07, y1, z + 0.07)
    m = max(1, int((y1 - y0) / sy))
    for j in range(1, m + 1):
        y = y0 + (y1 - y0) * j / m
        for z in (zi, zo):
            ab(p, STEEL, x0, y - 0.07, z - 0.07, x1, y + 0.07, z + 0.07)
        ab(p, GRATE, x0, y - 0.25, min(zi, zo), x1, y - 0.1, max(zi, zo))
        if lod == 0:
            ab(p, YEL, x0, y + 0.9, zo - 0.04, x1, y + 1.0, zo + 0.04)                        # guard rail
    for i in range(0, n, 4):                                                                  # face bracing
        xa, xb = x0 + (x1 - x0) * i / n, x0 + (x1 - x0) * min(n, i + 2) / n
        bar(p, STEEL, (xa, y0, zo), (xb, y1, zo), 0.12)
    if tarp_to:
        ab(p, TARP, x0, y0, zo + sg * 0.1 - 0.05, x1, tarp_to, zo + sg * 0.1 + 0.05)


def hoist(p, lod, x, z, y1, cages):
    """a rack-and-pinion construction hoist: mast, cages, wall ties (on the +X face)"""
    lat(p, STEEL, x, z, 0.6, y1, 0.8, 0.8, max(1, lod), bays=max(2, int(y1 / 6)))
    ab(p, CONC, x - 1.6, 0.0, z - 1.6, x + 1.6, 0.6, z + 4.4)
    for y in cages:                                                                           # cage beside the mast
        ab(p, YEL, x - 1.2, y, z + 0.9, x + 1.2, y + 3.0, z + 4.3)
        if lod < 2:
            box(p, WARM, (x + 1.24, y + 2.0, z + 2.6), (0.06, 0.8, 1.4))
    if lod < 2:
        y = 12.0
        while y < y1 - 2:
            bar(p, STEEL, (x, y, z - 0.8), (x - 2.6, y, z - 1.2), 0.2)                       # wall ties
            y += 24.0
    beacon(p, (x, y1 + 0.6, z), 0.6)


def bldg_geom(p, lod, name, C, st):
    cl = close()
    W, D = C["W"], C["D"]
    hw, hd = W / 2, D / 2
    FW, FD = C["F"]
    nf = int(round(C["Hs"].get(st, 0.0) / FH))
    Hf = nf * FH
    nx, nz = max(1, int(round(W / C["sp"]))), max(1, int(round(D / C["sp"])))
    xs = [-hw + W * i / nx for i in range(nx + 1)]
    zs = [-hd + D * j / nz for j in range(nz + 1)]
    perim = [(x, -hd) for x in xs] + [(x, hd) for x in xs] + [(-hw, z) for z in zs[1:-1]] + [(hw, z) for z in zs[1:-1]]
    inner = [(x, z) for z in C.get("rows", ()) for x in xs[1:-1:2]]
    cw, bw, RM = C.get("col", 1.2), C.get("beam", 0.8), C.get("raft", 3.0)
    pit = C["pit"] if st == 1 else None
    corners = ((-hw, -hd), (hw, -hd), (hw, hd), (-hw, hd))

    # ---- raft
    if pit:
        _ring_boxes(p, CONC, -hw - RM, hw + RM, -hd - RM, hd + RM, -2.5, 0.6, pit)
        ab(p, CONC, pit[0], -2.5, pit[1], pit[2], -1.4, pit[3])                         # blinding, pit bottom
        rebar_mesh(p, lod, pit[0] + 0.5, pit[1] + 0.5, pit[2] - 0.5, pit[3] - 0.5, -0.6)
        if lod < 2:
            for (x0, z0, x1, z1) in ((pit[0], pit[1], pit[2], pit[1]), (pit[0], pit[3], pit[2], pit[3]),
                                     (pit[0], pit[1], pit[0], pit[3]), (pit[2], pit[1], pit[2], pit[3])):
                ab(p, PLY, x0 - 0.15, -1.4, z0 - 0.15, x1 + 0.15, 0.9, z1 + 0.15)               # edge formwork
        for (x, z) in perim + inner:
            if pit[0] < x < pit[2] and pit[1] < z < pit[3]:
                ab(p, CONC, x - 1.6, -1.4, z - 1.6, x + 1.6, -0.6, z + 1.6)                     # pile caps in the pit
    else:
        ab(p, CONC, -hw - RM, -2.5, -hd - RM, hw + RM, 0.6, hd + RM)

    # ---- cladding plan (decides what is hidden)
    nd = {1: 0, 2: int(round(0.55 * nf)), 3: int(round(0.8 * nf)), 4: nf}[st]
    panels = []                                    # (face, a0, a1, y0, y1)
    faces = {"-z": xs, "+z": xs, "-x": zs, "+x": zs}
    hb = nz // 2                                   # the hoist bays on +X
    if st == 3:
        ch = int(round(0.5 * nf)) * FH
        for f, g in faces.items():
            panels.append((f, g[0], g[-1], 0.6, ch))
    elif st == 4:
        sc = C.get("scaff")
        for f, g in faces.items():
            for i in range(len(g) - 1):
                top = Hf - FH * (1 + int(_hash(name, f, i) * 4))
                if f == "+x" and i in (hb - 1, hb):
                    top = int(round(0.4 * nf)) * FH
                if sc and f == sc[0] and g[i] >= sc[1] - 1e-6 and g[i + 1] <= sc[2] + 1e-6:
                    top = int(round(0.55 * nf)) * FH
                if f == "+z" and i == len(g) // 3:                                           # a missing run
                    mid = int(round(0.5 * nf)) * FH
                    panels.append((f, g[i], g[i + 1], 0.6, mid))
                    panels.append((f, g[i], g[i + 1], mid + 3 * FH, top))
                    continue
                panels.append((f, g[i], g[i + 1], 0.6, top))
    clad_min = min((pn[4] for pn in panels), default=0.6)
    yskip = clad_min - FH if panels else -1.0
    for (f, a0, a1, y0, y1) in panels:
        e0 = -0.9 if abs(a0 - (-hw if f[1] == "z" else -hd)) < 1e-6 else 0.0
        e1 = 0.9 if abs(a1 - (hw if f[1] == "z" else hd)) < 1e-6 else 0.0
        if f[1] == "z":
            s = 1 if f[0] == "+" else -1
            ab(p, C["fac"], a0 + e0, y0, s * (hd + 0.3), a1 + e1, y1, s * (hd + 0.9))
        else:
            s = 1 if f[0] == "+" else -1
            ab(p, C["fac"], s * (hw + 0.3), y0, a0 + e0, s * (hw + 0.9), y1, a1 + e1)
        if lod < 2 and y1 > 8.0:
            if f[1] == "z":                                                                    # parapet / top edge
                ab(p, LIGHT, a0 + e0, y1 - 0.3, s * (hd + 0.2), a1 + e1, y1, s * (hd + 1.0))
            else:
                ab(p, LIGHT, s * (hw + 0.2), y1 - 0.3, a0 + e0, s * (hw + 1.0), y1, a1 + e1)

    # ---- cores
    for (cx, cz, cwid, cdep) in C["cores"]:
        x0, x1, z0, z1 = cx - cwid / 2, cx + cwid / 2, cz - cdep / 2, cz + cdep / 2
        if st == 1:
            t = 0.8
            for (a, b, c, d) in ((x0, z0, x1, z0 + t), (x0, z1 - t, x1, z1), (x0, z0, x0 + t, z1), (x1 - t, z0, x1, z1)):
                ab(p, CONC, a, 0.6, b, c, 6.0, d)
            for (a, b, c, d) in ((x0 - 0.4, z0 - 0.4, x1 + 0.4, z0 + t + 0.4), (x0 - 0.4, z1 - t - 0.4, x1 + 0.4, z1 + 0.4),
                                 (x0 - 0.4, z0, x0 + t + 0.4, z1), (x1 - t - 0.4, z0, x1 + 0.4, z1)):
                ab(p, PLY, a, 6.0, b, c, 9.0, d)                                              # wall formwork
            starters(p, [(x0 + 0.4 + k, z0 + 0.4) for k in range(0, int(cwid), 1)] +
                     [(x0 + 0.4 + k, z1 - 0.4) for k in range(0, int(cwid), 1)], 9.0, 2.0)
            if lod < 2:
                for (x, z) in ((x0, z0), (x1, z1)):
                    flood(p, (x, 9.4, z), 0.8)
        else:
            ctop = Hf + (2 if st < 4 else 1) * FH
            ab(p, CONC, x0, max(0.6, yskip), z0, x1, ctop - 4.0, z1)
            ab(p, YEL, x0 - 1.2, ctop - 6.0, z0 - 1.2, x1 + 1.2, ctop, z1 + 1.2)              # self-climbing formwork
            if lod < 2:
                for k, (x, z) in enumerate(((x0 - 1.2, z0 - 1.2), (x1 + 1.2, z0 - 1.2), (x1 + 1.2, z1 + 1.2), (x0 - 1.2, z1 + 1.2))):
                    flood(p, (x, ctop + 0.4, z), 1.0)
                bar(p, YEL, (cx, ctop, cz), (cx, ctop + 5.0, cz), 0.8)                         # placing boom
                bar(p, YEL, (cx, ctop + 5.0, cz), (cx + cwid * 0.7, ctop + 7.0, cz + cdep * 0.4), 0.6)
                if lod == 0:
                    for k in range(1, int((ctop - 6.0) / FH)):                                # door openings
                        if k * FH < yskip:
                            continue
                        box(p, DARK, (cx, k * FH - 1.8, z0 - 0.05), (2.4, 2.8, 0.1))
                        box(p, DARK, (cx, k * FH - 1.8, z1 + 0.05), (2.4, 2.8, 0.1))
            beacon(p, (cx, ctop + 0.7, cz), 0.8)

    # ---- columns
    if st == 1:
        if lod < 2:
            for (x, z) in perim + inner:
                if pit and pit[0] < x < pit[2] and pit[1] < z < pit[3]:
                    continue
                ab(p, STEEL, x - cw, 0.6, z - cw, x + cw, 0.9, z + cw)
                ab(p, PRIM, x - cw / 2, 0.9, z - cw / 2, x + cw / 2, 3.6, z + cw / 2)
    else:
        y0c = max(0.6, yskip)
        for (x, z) in perim + inner:
            if lod == 2 and (x, z) in inner:
                continue
            ab(p, PRIM, x - cw / 2, y0c, z - cw / 2, x + cw / 2, Hf, z + cw / 2)

    # ---- decks (concrete on metal decking)
    for k in range(1, nd + 1):
        y = k * FH
        if y < yskip or (lod == 2 and k % 2 and k != nd):
            continue
        ab(p, CONC, -hw - 0.3, y - 0.35, -hd - 0.3, hw + 0.3, y, hd + 0.3)
    if st in (2, 3) and lod < 2 and nd < nf:                                                   # decking being laid
        y = (nd + 1) * FH
        ab(p, GRATE, -hw, y - 0.5, -hd, 0.0, y - 0.38, hd)

    # ---- bare floors: beams
    for k in range(nd + 1, nf + 1):
        if lod == 2 and k % 2 and k != nf:
            continue
        y = k * FH - bw * 0.6
        for z in (-hd, hd):
            ab(p, PRIM, -hw, y - bw / 2, z - bw / 2, hw, y + bw / 2, z + bw / 2)
        for x in (-hw, hw):
            ab(p, PRIM, x - bw / 2, y - bw / 2, -hd, x + bw / 2, y + bw / 2, hd)
        if lod < 2:
            for z in (zs[1:-1] if lod == 0 else zs[1:-1][::2]):
                ab(p, PRIM, -hw, y - bw * 0.4, z - bw * 0.3, hw, y + bw * 0.4, z + bw * 0.3)
            for x in (xs[1:-1] if lod == 0 else xs[1:-1][::2]):
                ab(p, PRIM, x - bw * 0.3, y - bw * 0.4, -hd, x + bw * 0.3, y + bw * 0.4, hd)

    # ---- temporary erection bracing in the corner bays of the bare floors
    if lod < 2 and nf > nd:
        y = nd * FH
        while y + 2 * FH <= Hf + 1e-6:
            for (a, b) in ((xs[0], xs[1]), (xs[-2], xs[-1])):
                for z in (-hd, hd):
                    bar(p, STEEL, (a, y, z), (b, y + 2 * FH, z), 0.3)
                    bar(p, STEEL, (b, y, z), (a, y + 2 * FH, z), 0.3)
            for (a, b) in ((zs[0], zs[1]), (zs[-2], zs[-1])):
                for x in (-hw, hw):
                    bar(p, STEEL, (x, y, a), (x, y + 2 * FH, b), 0.3)
                    bar(p, STEEL, (x, y, b), (x, y + 2 * FH, a), 0.3)
            y += 2 * FH

    # ---- belt trusses (outriggers every C["belt"] floors)
    if C.get("belt") and lod < 2:
        kb = C["belt"]
        while kb * FH + 2 * FH <= Hf:
            yb = kb * FH
            if yb >= yskip:
                for (g, fixed, along_x) in ((xs, -hd, True), (xs, hd, True), (zs, -hw, False), (zs, hw, False)):
                    for i in range(len(g) - 1):
                        a, b, m = g[i], g[i + 1], (g[i] + g[i + 1]) / 2
                        pa = (a, yb, fixed) if along_x else (fixed, yb, a)
                        pb = (b, yb, fixed) if along_x else (fixed, yb, b)
                        pm = (m, yb + 2 * FH, fixed) if along_x else (fixed, yb + 2 * FH, m)
                        bar(p, PRIM, pa, pm, bw * 0.8)
                        bar(p, PRIM, pb, pm, bw * 0.8)
            kb += C["belt"]

    # ---- climbing screens round the top decked floors, catch fans, edge rails
    if nd >= 3:
        ys0, ys1 = (nd - 2) * FH - 1.0, nd * FH + 2.0
        o = 1.8
        if lod == 0:
            for (g, fixed, along_x) in ((xs, -hd - o, True), (xs, hd + o, True), (zs, -hw - o, False), (zs, hw + o, False)):
                for i in range(len(g) - 1):
                    a, b = g[i] + 0.2, g[i + 1] - 0.2
                    if along_x:
                        ab(p, YEL, a, ys0, fixed - 0.15, b, ys1, fixed + 0.15)
                    else:
                        ab(p, YEL, fixed - 0.15, ys0, a, fixed + 0.15, ys1, b)
        else:
            ab(p, YEL, -hw - o, ys0, -hd - o - 0.15, hw + o, ys1, -hd - o + 0.15)
            ab(p, YEL, -hw - o, ys0, hd + o - 0.15, hw + o, ys1, hd + o + 0.15)
            ab(p, YEL, -hw - o - 0.15, ys0, -hd - o, -hw - o + 0.15, ys1, hd + o)
            ab(p, YEL, hw + o - 0.15, ys0, -hd - o, hw + o + 0.15, ys1, hd + o)
        if lod < 2:
            for k in range(4):                                                                 # screen lamps
                x, z = corners[k]
                flood(p, (x + math.copysign(2.0, x), ys1 + 0.4, z + math.copysign(2.0, z)), 1.0)
    if st in (2, 3) and lod < 2 and nd >= 5:
        yf = (nd - 3) * FH
        for (nxv, nzv, e0, e1) in ((0, -1, (-hw, yf, -hd), (hw, yf, -hd)), (0, 1, (-hw, yf, hd), (hw, yf, hd)),
                                   (-1, 0, (-hw, yf, -hd), (-hw, yf, hd)), (1, 0, (hw, yf, -hd), (hw, yf, hd))):
            s = (nxv * 0.966, 0.259, nzv * 0.966)
            fw = C.get("fan", 4.0)
            sh = _add(_mul((nxv, 0, nzv), 0.4), _mul(s, fw / 2))
            oslab(p, NET, _add(e0, sh), _add(e1, sh), s, fw, 0.08)                            # catch fan
        k0 = int(round(0.5 * nf)) + 1 if st == 3 else 1
        for k in range(k0, nd - 2):
            y = k * FH
            for h in ((1.0,) if lod == 1 else (0.5, 1.1)):
                ab(p, YEL, -hw - 0.4, y + h - 0.06, -hd - 0.4, hw + 0.4, y + h + 0.06, -hd - 0.3)
                ab(p, YEL, -hw - 0.4, y + h - 0.06, hd + 0.3, hw + 0.4, y + h + 0.06, hd + 0.4)
                ab(p, YEL, -hw - 0.4, y + h - 0.06, -hd - 0.4, -hw - 0.3, y + h + 0.06, hd + 0.4)
                ab(p, YEL, hw + 0.3, y + h - 0.06, -hd - 0.4, hw + 0.4, y + h + 0.06, hd + 0.4)

    # ---- hoist (+X face) and scaffold
    if st >= 3:
        cages = [int(round(0.5 * nf)) * FH] + ([nd * FH - 8.0] if st == 4 else [8.0])
        hoist(p, lod, hw + 2.6, (zs[hb] + zs[hb - 1]) / 2 if nz > 1 else 0.0, nd * FH + 2 * FH, cages)
    if st == 4 and C.get("scaff"):
        f, a0, a1 = C["scaff"]
        sg = 1 if f == "+z" else -1
        scaffold(p, lod, a0, a1, 0.6, Hf + 2.0, sg * (hd + 0.9), sg, tarp_to=0.35 * Hf)

    # ---- lights: corner beacons (top + obstruction levels), top floods
    if Hf > 0:
        for k, (x, z) in enumerate(corners):
            if lod == 2 and k % 2:
                continue
            beacon(p, (x * 1.0, Hf + 0.8, z * 1.0), 1.0)
            if lod < 2:
                y = 45.0
                while y < Hf - 10:
                    beacon(p, (x * (1 + 1.2 / hw), y, z * (1 + 1.2 / hd)), 0.7)
                    y += 45.0
        if lod < 2:
            for x in xs[1:-1:2]:
                flood(p, (x, Hf + 0.5, -hd), 1.0)
                flood(p, (x, Hf + 0.5, hd), 1.0)

    # ---- site
    site_marks(p, lod, FW, FD)
    if lod < 2 and C.get("lamp"):
        for sx in (-1, 1):
            for sz in (-1, 1):
                light_mast(p, lod, sx * (FW / 2 - 2.0), sz * (FD / 2 - 2.0), C["lamp"])
    for (kind, x, y, z, ang) in C.get("stacks", {}).get(st, ()):
        if lod == 2 and kind in ("pallets", "beams_s"):
            continue
        yy = nd * FH if y == "top" else y
        if kind == "cabins":
            cabins_row(p, lod, x, z)
        else:
            stack(p, lod, kind, x, yy, z, ang)


def bldg_parts(name, C, st):
    def fn(lod):
        P = {name: Part(name)}
        bldg_geom(P[name], lod, name, C, st)
        return P
    return fn


def bldg_hits(name, C, st):
    def fn():
        W, D, RM = C["W"], C["D"], C.get("raft", 3.0)
        nf = int(round(C["Hs"].get(st, 0.0) / FH))
        Hf = nf * FH
        H = [hbox(name, (0, -0.95, 0), (W + 2 * RM, 3.1, D + 2 * RM))]
        for (cx, cz, cw, cd) in C["cores"]:
            top = 9.0 if st == 1 else Hf + (2 if st < 4 else 1) * FH
            H.append(hbox(name, (cx, (0.6 + top) / 2, cz), (cw + 2.4, top - 0.6, cd + 2.4)))
        if st >= 2:
            H.append(hbox(name, (0, (0.6 + Hf) / 2, 0), (W + 2.0, Hf - 0.6, D + 2.0)))
        if st >= 3:
            nd = {3: int(round(0.8 * nf)), 4: nf}[st]
            y1 = (nd + 2) * FH
            nz = max(1, int(round(D / C["sp"])))
            zs = [-D / 2 + D * j / nz for j in range(nz + 1)]
            zm = (zs[nz // 2] + zs[nz // 2 - 1]) / 2 if nz > 1 else 0.0
            H.append(hbox(name, (W / 2 + 2.6, y1 / 2, zm + 1.4), (3.4, y1, 6.0)))
        if st == 4 and C.get("scaff"):
            f, a0, a1 = C["scaff"]
            sg = 1 if f == "+z" else -1
            H.append(hbox(name, ((a0 + a1) / 2, (Hf + 2.6) / 2, sg * (D / 2 + 2.1)), (a1 - a0, Hf + 1.4, 1.8)))
        return H
    return fn


TOWER = dict(W=80.0, D=80.0, F=(100.0, 100.0), Hs={2: 110.0, 3: 210.0, 4: 290.0}, sp=10.0, cores=[(0.0, 0.0, 28.0, 28.0)],
             col=1.4, beam=1.0, belt=25, pit=(16.0, 16.0, 43.0, 43.0), fac=FACADE, lamp=16.0,
             stacks={1: [("beams", -20.0, 0.6, -24.0, 0.0), ("panels", -20.0, 0.6, 24.0, 0.0), ("cont", 22.0, 0.6, -25.0, 0.0),
                         ("cabins", 0.0, 0.0, -46.5, 0.0)],
                     2: [("beams", -24.0, 0.6, -25.0, 0.0), ("pallets", -26.0, "top", 26.0, 0.0), ("cabins", 0.0, 0.0, -46.5, 0.0)],
                     3: [("panels", -25.0, "top", 25.0, 0.0), ("beams_s", 25.0, "top", -26.0, 0.0), ("cabins", 0.0, 0.0, -46.5, 0.0)],
                     4: [("pallets", -26.0, "top", 26.0, 0.0), ("cabins", 0.0, 0.0, -46.5, 0.0)]})
HAB = dict(W=32.0, D=24.0, fan=2.5, F=(40.0, 30.0), Hs={2: 30.0, 3: 60.0, 4: 84.0}, sp=6.0, cores=[(0.0, 0.0, 8.0, 7.0)],
           col=0.8, beam=0.6, raft=1.5, pit=(5.0, 4.0, 17.5, 13.5), fac=FACADE_L, scaff=("+z", -16.0, 16.0), lamp=0.0,
           stacks={1: [("beams_s", -9.0, 0.6, -8.0, 0.0), ("pallets", -10.0, 0.6, 7.5, 0.0)],
                   2: [("pallets", -10.0, "top", 7.0, 0.0)], 3: [("pallets", -10.0, "top", 7.0, 0.0)], 4: []})
SLAB = dict(W=204.0, D=56.0, F=(220.0, 70.0), Hs={2: 40.0, 3: 72.0, 4: 100.0}, sp=10.0,
            cores=[(-60.0, 0.0, 12.0, 10.0), (60.0, 0.0, 12.0, 10.0)], rows=(-14.0, 14.0), col=1.0, beam=0.8, raft=2.0,
            pit=(-30.0, -30.0, 30.0, 30.0), fac=FACADE, scaff=("-z", 20.0, 102.0), lamp=14.0,
            stacks={1: [("beams", -85.0, 0.6, -13.0, 0.0), ("panels", -85.0, 0.6, 14.0, 0.0), ("cont", 81.0, 0.6, 0.0, 0.0),
                        ("cabins", 0.0, 0.0, -33.0, 0.0)],
                    2: [("beams", -90.0, 0.6, 12.0, 0.0), ("pallets", 30.0, "top", 10.0, 0.0), ("cabins", 0.0, 0.0, -33.0, 0.0)],
                    3: [("panels", -30.0, "top", 12.0, 0.0), ("beams_s", 30.0, "top", -12.0, 0.0), ("cabins", 0.0, 0.0, -33.0, 0.0)],
                    4: [("pallets", 0.0, "top", 12.0, 0.0), ("cabins", 0.0, 0.0, -33.0, 0.0)]})


# ================================================================ industrial hall (240 x 240, ~90 m)
HX = [-110.0, -55.0, 0.0, 55.0, 110.0]
HZ = [-110.0 + 20.0 * i for i in range(12)]
H_COL, H_RIDGE, H_BOT = 80.0, 88.0, 72.0


def _runs(bays):
    """contiguous runs of bay indices -> [(first, last)]"""
    out = []
    for b in sorted(bays):
        if out and out[-1][1] == b - 1:
            out[-1][1] = b
        else:
            out.append([b, b])
    return out


def hall_parts(st):
    name = f"hubc_hall_s{st}"

    def fn(lod):
        P = {name: Part(name)}
        p = P[name]
        cl = close()
        ncol = {1: 0, 2: 9, 3: 12, 4: 12}[st]
        ntr = {1: 0, 2: 7, 3: 12, 4: 12}[st]
        purl = {1: set(), 2: {0, 1, 2}, 3: set(range(11)), 4: set(range(11))}[st]
        roof = {1: set(), 2: set(), 3: set(range(5)), 4: set(range(10))}[st]
        # floor slab and footings
        if st == 1:
            ab(p, CONC, -112.0, 0.0, -112.0, 112.0, 0.6, -10.0)
            for x in HX:
                for z in HZ:
                    if z > -10.0:
                        ab(p, CONC, x - 2.5, -1.4, z - 2.5, x + 2.5, 0.6, z + 2.5)
            if lod < 2:
                for x in (-110.0, 110.0):
                    ab(p, CONC, x - 0.5, -0.4, -10.0, x + 0.5, 0.5, 110.0)                       # ground beams
                ab(p, CONC, -110.0, -0.4, 109.5, 110.0, 0.5, 110.5)
                rebar_mesh(p, lod, -108.0, -8.0, -60.0, 20.0, 0.0)
                starters(p, [(x, -10.2) for x in range(-110, 111, 2)], 0.6, 1.2)
        else:
            ab(p, CONC, -112.0, -0.4, -112.0, 112.0, 0.6, 112.0)
        for i, z in enumerate(HZ):
            for x in HX:
                if i < ncol:
                    ab(p, PRIM, x - 0.8, 0.6, z - 0.8, x + 0.8, H_COL, z + 0.8)
                elif lod < 2:
                    ab(p, STEEL, x - 1.2, 0.6, z - 1.2, x + 1.2, 0.9, z + 1.2)
                    ab(p, PRIM, x - 0.7, 0.9, z - 0.7, x + 0.7, 3.5, z + 0.7)
            if i < ntr:
                for s in range(4):
                    x0, x1 = HX[s], HX[s + 1]
                    xm = (x0 + x1) / 2
                    if lod >= 2:
                        ab(p, PRIM, x0, H_BOT, z - 0.3, x1, H_COL + 3.0, z + 0.3)
                        continue
                    ptruss(p, PRIM, (x0, H_COL, z), (xm, H_RIDGE, z), (x0, H_BOT, z), (xm, H_BOT, z), 5, lod, 0.6)
                    ptruss(p, PRIM, (x1, H_COL, z), (xm, H_RIDGE, z), (x1, H_BOT, z), (xm, H_BOT, z), 5, lod, 0.6)
                if lod < 2 and (i == 0 or i == ntr - 1):
                    for s in range(4):
                        beacon(p, ((HX[s] + HX[s + 1]) / 2, H_RIDGE + 1.0, z), 0.9)
        # bracing in the first bay (roof X + side walls)
        if st >= 2 and lod < 2:
            z0, z1 = HZ[0], HZ[1]
            for s in range(4):
                x0, x1 = HX[s], HX[s + 1]
                xm = (x0 + x1) / 2
                bar(p, STEEL, (x0, H_COL, z0), (xm, H_RIDGE, z1), 0.35)
                bar(p, STEEL, (xm, H_RIDGE, z0), (x0, H_COL, z1), 0.35)
                bar(p, STEEL, (x1, H_COL, z0), (xm, H_RIDGE, z1), 0.35)
                bar(p, STEEL, (xm, H_RIDGE, z0), (x1, H_COL, z1), 0.35)
            for x in (-110.0, 110.0):
                bar(p, STEEL, (x, 0.6, z0), (x, H_COL, z1), 0.4)
                bar(p, STEEL, (x, 0.6, z1), (x, H_COL, z0), 0.4)
        # purlins on the bare roof bays, sheeting on the covered ones
        for (b0, b1) in _runs(purl - roof):
            za, zb = HZ[b0], HZ[b1 + 1]
            if lod >= 2:
                continue
            for s in range(4):
                x0, x1 = HX[s], HX[s + 1]
                xm = (x0 + x1) / 2
                for t in ((0.0, 0.25, 0.5, 0.75, 1.0) if lod == 0 else (0.0, 0.5, 1.0)):
                    for xe in (x0, x1):
                        x = xe + (xm - xe) * t
                        y = H_COL + (H_RIDGE - H_COL) * t + 0.5
                        ab(p, STEEL, x - 0.2, y - 0.25, za, x + 0.2, y + 0.25, zb)
        for (b0, b1) in _runs(roof):
            za, zb = HZ[b0], HZ[b1 + 1]
            zc = (za + zb) / 2
            for s in range(4):
                x0, x1 = HX[s], HX[s + 1]
                xm = (x0 + x1) / 2
                for xe in (x0, x1):
                    oslab(p, LIGHT, (xe, H_COL + 0.8, zc), (xm, H_RIDGE + 0.8, zc), (0.0, 0.0, 1.0), zb - za + 0.4, 0.3)
        # walls
        side_bays = {1: [], 2: [], 3: list(range(6)), 4: list(range(11))}[st]
        for sx in (-1, 1):
            x = sx * 111.2
            for j in side_bays:
                top = H_COL
                if st == 4 and sx > 0 and j == 10:
                    top = 40.0
                ab(p, LIGHT, x - 0.25, 0.6, HZ[j], x + 0.25, top, HZ[j + 1])
                if lod < 2 and _hash(sx, j) > 0.35 and top > 64.0:
                    ab(p, WARM, x + sx * 0.3 - 0.05, 62.0, HZ[j] + 3.0, x + sx * 0.3 + 0.05, 64.0, HZ[j + 1] - 3.0)
        for sz, spans in ((-1, {3: [0, 1, 2, 3], 4: [0, 1, 2, 3]}.get(st, [])), (1, {4: [0, 1, 2]}.get(st, []))):
            z = sz * 111.2
            for s in spans:
                x0, x1 = HX[s], HX[s + 1]
                xm = (x0 + x1) / 2
                y0 = 0.6
                if sz > 0 and s in (1, 2):
                    y0 = 36.0                                                                  # door openings left open
                ab(p, LIGHT, x0, y0, z - 0.25, x1, H_COL, z + 0.25)
                tri(p, LIGHT, (x0, H_COL, z + sz * 0.25), (x1, H_COL, z + sz * 0.25), (xm, H_RIDGE, z + sz * 0.25), (0, 0, sz))
                if sz < 0 and lod < 2:
                    ab(p, DARK, xm - 12.0, 0.6, z - 0.35, xm + 12.0, 30.0, z - 0.3)                # big doors
                    ab(p, HAZ, xm - 12.5, 30.0, z - 0.4, xm + 12.5, 31.0, z - 0.3)
                    flood(p, (xm, 32.0, z - 0.8), 1.4)
        # crane runway girders
        if st >= 3:
            for x in (-108.8, -55.0, 0.0, 55.0, 108.8):
                ab(p, STEEL, x - 0.6, 54.0, -110.0, x + 0.6, 57.0, 110.0)
        # work lights on the outer columns
        if lod < 2 and ncol:
            for i in range(0, ncol, 2):
                for sx in (-1, 1):
                    flood(p, (sx * 112.4, 60.0, HZ[i]), 1.2)
        # site
        site_marks(p, lod, 240.0, 240.0)
        if lod < 2:
            for sx in (-1, 1):
                for sz in (-1, 1):
                    light_mast(p, lod, sx * 117.0, sz * 117.0, 14.0)
        cabins_row(p, lod, -20.0, -116.5)
        for (kind, x, y, z, ang) in {1: [("beams", -82.5, 0.0, 40.0, 0.0), ("cont", -27.5, 0.0, 100.0, 0.0),
                                         ("panels", 82.5, 0.0, 60.0, 0.0), ("beams", 82.5, 0.0, 20.0, 0.0)],
                                     2: [("beams", -82.5, 0.6, 85.0, 0.0), ("cont", -27.5, 0.6, 80.0, 0.0)],
                                     3: [("panels", -82.5, 0.6, 60.0, 0.0), ("panels", 27.5, 0.6, 60.0, 0.0)],
                                     4: [("cont", 82.5, 0.6, 100.0, 0.0)]}[st]:
            stack(p, lod, kind, x, y, z, ang)
        if st in (1, 2) and lod < 2:                                                         # a truss lying flat
            y = 0.6 if st == 2 else 0.0
            ptruss(p, PRIM, (5.0, y + 0.5, 100.0 if st == 2 else 80.0), (49.0, y + 0.5, 100.0 if st == 2 else 80.0),
                   (5.0, y + 0.5, 92.0 if st == 2 else 72.0), (49.0, y + 0.5, 92.0 if st == 2 else 72.0), 6, lod, 0.6)
        return P
    return fn


def hall_hits(st):
    name = f"hubc_hall_s{st}"

    def fn():
        H = [hbox(name, (0, 0.1, 0), (224.0, 1.0, 224.0))]
        if st >= 2:
            zmax = HZ[8] if st == 2 else 110.0
            H.append(hbox(name, (0, (0.6 + H_RIDGE) / 2, (-110.0 + zmax) / 2), (224.0, H_RIDGE - 0.6, zmax + 110.0 + 2.0)))
        return H
    return fn


# ================================================================ dock pier (130 x 300, deck at 42 m, gantry to ~62)
DK_BENTS = [-115.0, -70.0, -25.0, 20.0, 65.0, 110.0]
DK_GX = [-27.0, -9.0, 9.0, 27.0]
DK_Y = 42.0


def dock_parts(st):
    name = f"hubc_dock_s{st}"

    def fn(lod):
        P = {name: Part(name)}
        p = P[name]
        seg = {0: 16, 1: 10, 2: 6}[lod]
        # abutment (the root at the hub, -Z)
        ab(p, CONC, -35.0, -3.0, -150.0, 35.0, 30.0 if st == 1 else DK_Y, -140.0)
        if st == 1:
            starters(p, [(x, -145.0) for x in range(-34, 35, 2)], 30.0, 2.5)
            if lod < 2:
                ab(p, PLY, -35.4, 30.0, -150.4, 35.4, 33.0, -149.6)
                ab(p, PLY, -35.4, 30.0, -140.4, 35.4, 33.0, -139.6)
        # bents
        for bi, z in enumerate(DK_BENTS):
            full = st >= 2 or bi == 0
            for x in (-20.0, 20.0):
                ab(p, CONC, x - 4.0, -1.5, z - 4.0, x + 4.0, 1.0, z + 4.0)
                if full:
                    cyl(p, CONC, (x, 1.0, z), (x, 31.0, z), 2.2, seg)
                elif bi in (1, 2):
                    cyl(p, CONC, (x, 1.0, z), (x, 7.0, z), 2.2, seg)
                    if lod < 2:
                        cyl(p, PLY, (x, 6.0, z), (x, 10.0, z), 2.6, seg)
                else:
                    starters(p, [(x + 1.5 * math.cos(a * TAU / 8), z + 1.5 * math.sin(a * TAU / 8)) for a in range(8)], 1.0, 3.0)
            if full:
                ab(p, CONC, -33.0, 31.0, z - 3.0, 33.0, 36.0, z + 3.0)
                if lod < 2 and st < 4:
                    for x in (-33.0, 33.0):
                        beacon(p, (x, 36.6, z), 0.7)
        # girders
        zend = {1: -105.0, 2: 45.0, 3: 150.0, 4: 150.0}[st]
        for x in DK_GX:
            ab(p, PRIM, x - 1.5, 36.0, -140.0, x + 1.5, 41.6, zend)
        if lod < 2:
            step = 15.0 if lod == 0 else 30.0
            z = -135.0
            while z < zend - 2.0:
                ab(p, STEEL, -27.0, 38.0, z - 0.3, 27.0, 38.6, z + 0.3)                           # cross frames
                z += step
        if st == 2:                                                                          # launching nose
            for x in (-18.0, 18.0):
                btruss(p, YEL, (x, 39.0, 45.0), (x, 39.5, 72.0), 3.0, 4.0, lod)
            if lod < 2:
                bar(p, YEL, (-18.0, 39.0, 70.0), (18.0, 39.0, 70.0), 0.5)
                beacon(p, (0.0, 41.8, 72.0), 0.9)
            for x in DK_GX:
                bar(p, STEEL, (x, 41.6, 40.0), (0.6 * x, 39.0, 45.0), 0.4)
        # deck plating
        if st == 3:
            ab(p, HULL, -30.0, 41.6, -140.0, 30.0, DK_Y, 0.0)
        elif st == 4:
            ab(p, HULL, -30.0, 41.6, -140.0, 30.0, DK_Y, 120.0)
            ab(p, HULL, -30.0, 41.6, 120.0, 0.0, DK_Y, 150.0)
        # gantry (portal crane on the deck)
        if st >= 3:
            zg = -40.0
            for sx in (-1, 1):
                for sz in (-1, 1):
                    btruss(p, YEL, (sx * 26.0, DK_Y + 2.0, zg + sz * 8.0), (sx * 26.0, 58.0, zg + sz * 5.0), 2.2, 2.2, lod,
                           up=(0.0, 0.0, 1.0))
                ab(p, YEL, sx * 26.0 - 1.2, DK_Y, zg - 10.0, sx * 26.0 + 1.2, DK_Y + 2.2, zg + 10.0)    # sill on bogies
            for sz in (-1, 1):
                ab(p, YEL, -62.0, 58.0, zg + sz * 5.0 - 1.4, 62.0, 62.0, zg + sz * 5.0 + 1.4)
            if lod < 2:
                for sx in (-1, 1):
                    beacon(p, (sx * 62.0, 62.8, zg), 0.9)
                for x in (-45.0, -15.0, 15.0, 45.0):
                    flood(p, (x, 57.6, zg), 1.3)
            if st == 4:
                ab(p, YEL, 34.0, 62.0, zg - 7.0, 46.0, 65.0, zg + 7.0)                              # trolley
                if lod < 2:
                    box(p, WARM, (40.0, 63.5, zg - 7.05), (6.0, 1.2, 0.1))
                for dx in (-1.0, 1.0):
                    for dz in (-1.0, 1.0):
                        ab(p, STEEL, 40.0 + dx - 0.06, 48.0, zg + dz - 0.06, 40.0 + dx + 0.06, 58.0, zg + dz + 0.06)
                ab(p, YEL, 38.5, 46.0, zg - 1.5, 41.5, 48.0, zg + 1.5)
        # outfitting (nearly done): rails, bollards, fenders, lamps
        if st == 4 and lod < 2:
            for sx in (-1, 1):
                x = sx * 29.6
                ab(p, YEL, x - 0.08, DK_Y + 1.0, -140.0, x + 0.08, DK_Y + 1.15, 120.0)
                if lod == 0:
                    ab(p, YEL, x - 0.08, DK_Y + 0.5, -140.0, x + 0.08, DK_Y + 0.6, 120.0)
                    for z in range(-140, 121, 3):
                        ab(p, STEEL, x - 0.06, DK_Y, z - 0.06, x + 0.06, DK_Y + 1.15, z + 0.06)
                for z in range(-130, 101, 15):
                    cyl(p, STEEL, (sx * 27.5, DK_Y, float(z)), (sx * 27.5, DK_Y + 1.2, float(z)), 0.45, 8)
                    if z < 60:
                        ab(p, FOAM, sx * 30.0, 34.0, z - 2.0, sx * 31.2, 41.0, z + 2.0)
                for z in range(-120, 121, 30):
                    light_mast(p, lod, sx * 28.5, float(z), 10.0, DK_Y, 2)
        if st >= 3:
            for sx in (-1, 1):
                beacon(p, (sx * 29.0, DK_Y + 1.0, 149.0), 1.0)
        # site and material
        site_marks(p, lod, 130.0, 300.0)
        cabins_row(p, lod, -50.0, -135.0 if st < 4 else -120.0, 2)
        for (kind, x, y, z, ang) in {1: [("girders", 50.0, 0.0, -50.0, math.pi / 2), ("beams", 48.0, 0.0, 60.0, math.pi / 2),
                                         ("cont", -48.0, 0.0, -40.0, math.pi / 2)],
                                     2: [("girders", 50.0, 0.0, 40.0, math.pi / 2), ("beams", -48.0, 0.0, 0.0, math.pi / 2)],
                                     3: [("pallets", -15.0, DK_Y, -20.0, 0.0), ("pallets", 15.0, DK_Y, -110.0, 0.0)],
                                     4: [("cont_s", 15.0, DK_Y, -100.0, 0.0), ("pallets", -12.0, DK_Y, 100.0, 0.0)]}[st]:
            if lod == 2 and kind == "pallets":
                continue
            stack(p, lod, kind, x, y, z, ang)
        return P
    return fn


def dock_hits(st):
    name = f"hubc_dock_s{st}"

    def fn():
        H = [hbox(name, (0, 19.5, -145.0), (70.0, 45.0, 10.0))]
        for bi, z in enumerate(DK_BENTS):
            if st >= 2 or bi == 0:
                for x in (-20.0, 20.0):
                    H.append(hcyl(name, (x, 0.0, z), (x, 31.0, z), 2.2))
                H.append(hbox(name, (0, 33.5, z), (66.0, 5.0, 6.0)))
        zend = {1: -105.0, 2: 45.0, 3: 150.0, 4: 150.0}[st]
        H.append(hbox(name, (0, 38.8, (zend - 140.0) / 2), (57.0, 6.4, zend + 140.0)))
        if st >= 3:
            H.append(hbox(name, (0, 60.0, -40.0), (124.0, 4.0, 13.0)))
            for sx in (-1, 1):
                H.append(hbox(name, (sx * 26.0, 50.0, -40.0), (3.0, 16.0, 20.0)))
        return H
    return fn


# ================================================================ yard / pad (240 x 190, plating at 2 m, light masts 25 m)
YD_X = [-110.0 + 10.0 * i for i in range(23)]
YD_Z = [-85.0 + 10.0 * i for i in range(18)]
YD_MASTS = [(x, z) for x in (-80.0, 0.0, 80.0) for z in (-80.0, 80.0)]
YD_TOP = 2.0


def yard_parts(st):
    name = f"hubc_yard_s{st}"

    def fn(lod):
        P = {name: Part(name)}
        p = P[name]
        plates = {1: [], 2: [(-115.0, -90.0, -40.0, -20.0), (-30.0, -90.0, 20.0, -50.0)],
                  3: [(-115.0, -90.0, 55.0, 90.0)],
                  4: [(-115.0, -90.0, 95.0, 90.0), (95.0, -20.0, 115.0, 90.0)]}[st]
        holes = {3: [(10.0, 20.0, 40.0, 50.0)]}.get(st, [])

        def plated(x, z):
            if any(h[0] < x < h[2] and h[1] < z < h[3] for h in holes):
                return False
            return any(r[0] < x < r[2] and r[1] < z < r[3] for r in plates)
        ped_x = {1: 40.0, 2: 200.0, 3: 200.0, 4: 200.0}[st]
        beam_x = {1: -40.0, 2: 200.0, 3: 200.0, 4: 200.0}[st]
        step = 1 if lod == 0 else 2
        for x in YD_X[::step]:
            if x > ped_x:
                continue
            for z in YD_Z[::step]:
                if plated(x, z) or lod == 2:
                    continue
                ab(p, CONC, x - 0.5, 0.0, z - 0.5, x + 0.5, 1.4, z + 0.5)
        if lod == 2 and st == 1:
            ab(p, CONC, -115.0, 0.0, -90.0, ped_x, 1.0, 90.0)
        # main beams along X on every pedestal line, secondaries along Z (every 2nd line)
        for z in YD_Z:
            if lod == 2 and z % 20:
                continue
            ab(p, PRIM, -115.0, 1.4, z - 0.3, min(115.0, beam_x), 1.8, z + 0.3)
        if st >= 2:
            for x in YD_X[::2 if lod < 2 else 4]:
                ab(p, PRIM, x - 0.2, 1.5, -90.0, x + 0.2, 1.8, 90.0)
        for r in plates:
            if holes and r[0] < holes[0][0] and r[2] > holes[0][2]:
                hx0, hz0, hx1, hz1 = holes[0]
                _ring_boxes(p, HULL, r[0], r[2], r[1], r[3], 1.8, YD_TOP, (hx0, hz0, hx1, hz1))
            else:
                ab(p, HULL, r[0], 1.8, r[1], r[2], YD_TOP, r[3])
            if lod == 0:                                                                      # plate seams
                x = r[0] + 10.0
                while x < r[2] - 1.0:
                    ab(p, STEEL, x - 0.1, YD_TOP, r[1], x + 0.1, YD_TOP + 0.04, r[3])
                    x += 10.0
        # edge barriers
        if st >= 3:
            runs = [(-115.0, -90.0, 115.0, -90.0), (-115.0, -90.0, -115.0, 90.0)]
            if st == 4:
                runs += [(-115.0, 90.0, 60.0, 90.0), (115.0, -90.0, 115.0, 30.0)]
            for (x0, z0, x1, z1) in runs:
                L = math.hypot(x1 - x0, z1 - z0)
                n = int(L / 6.5) if lod == 0 else 1
                for i in range(n):
                    if lod == 0 and _hash(x0, z0, i) < 0.08:
                        continue
                    a, b = _lerp((x0, 0, z0), (x1, 0, z1), i / n), _lerp((x0, 0, z0), (x1, 0, z1), (i + 0.92) / n)
                    if z0 == z1:
                        ab(p, CONC, a[0], YD_TOP, z0 - 0.4, b[0], YD_TOP + 1.0, z0 + 0.4)
                    else:
                        ab(p, CONC, x0 - 0.4, YD_TOP, a[2], x0 + 0.4, YD_TOP + 1.0, b[2])
                if lod < 2:
                    if z0 == z1:
                        ab(p, HAZ, x0, YD_TOP + 0.6, z0 - 0.42, x1, YD_TOP + 0.8, z0 + 0.42)
                    else:
                        ab(p, HAZ, x0 - 0.42, YD_TOP + 0.6, z0, x0 + 0.42, YD_TOP + 0.8, z1)
        # light masts
        if st >= 3:
            for k, (x, z) in enumerate(YD_MASTS):
                ab(p, CONC, x - 1.5, YD_TOP, z - 1.5, x + 1.5, YD_TOP + 1.0, z + 1.5)
                if st == 4 or k == 0:
                    lat(p, STEEL, x, z, YD_TOP + 1.0, 25.0, 0.8, 0.35, lod, bays=6)
                    if lod < 2:
                        ab(p, STEEL, x - 2.5, 25.0, z - 0.4, x + 2.5, 25.4, z + 0.4)
                        for i in range(5):
                            box(p, FLOOD, (x - 2.0 + i, 25.7, z), (0.8, 0.6, 0.8))
                    else:
                        box(p, FLOOD, (x, 25.4, z), (4.0, 0.8, 0.8))
                    beacon(p, (x, 26.6, z), 0.6)
            if st == 3 and lod < 2:
                for i, x in enumerate((-60.0, -40.0)):                                         # masts lying ready
                    lat_lying = Part("t")
                    btruss(lat_lying, STEEL, (0.0, 0.8, 0.0), (0.0, 0.8, 22.0), 1.1, 1.1, lod)
                    put(p, lat_lying, x, YD_TOP, -75.0)
        # site, materials, survey stakes
        site_marks(p, lod, 240.0, 190.0)
        if st == 1 and lod < 2:
            for x in YD_X[::4]:
                for z in (-88.0, 88.0):
                    ab(p, HAZ, x - 0.1, 0.0, z - 0.1, x + 0.1, 1.5, z + 0.1)
        cabins_row(p, lod, 70.0, -86.0, 3)
        light_mast(p, lod, 112.0, -86.0, 12.0)
        for (kind, x, y, z, ang) in {1: [("beams", 80.0, 0.0, 0.0, math.pi / 2), ("beams", 80.0, 0.0, 45.0, math.pi / 2),
                                         ("cont", 70.0, 0.0, 65.0, 0.0)],
                                     2: [("pallets", 60.0, YD_TOP, -70.0, 0.0), ("beams", 80.0, YD_TOP, 30.0, math.pi / 2)],
                                     3: [("pallets", 80.0, YD_TOP, 0.0, 0.0), ("cont_s", 30.0, YD_TOP, -60.0, 0.0)],
                                     4: [("cont", -60.0, YD_TOP, 30.0, 0.0), ("cont_s", 30.0, YD_TOP, 40.0, 0.0)]}[st]:
            if lod == 2 and kind == "pallets":
                continue
            stack(p, lod, kind, x, y, z, ang)
        if st >= 2 and lod < 2:                                                                # plate stacks
            for i in range(3):
                ab(p, HULL, 100.0 - 8.0 * i - 3.0, YD_TOP if st >= 4 else 0.0, 60.0, 100.0 - 8.0 * i + 3.0,
                   (YD_TOP if st >= 4 else 0.0) + 0.8 + 0.5 * i, 70.0)
        return P
    return fn


def yard_hits(st):
    name = f"hubc_yard_s{st}"

    def fn():
        H = [hbox(name, (0, 1.0, 0), (230.0, 2.0, 180.0))]
        if st >= 3:
            for k, (x, z) in enumerate(YD_MASTS):
                if st == 4 or k == 0:
                    H.append(hbox(name, (x, 14.0, z), (2.0, 24.0, 2.0)))
        return H
    return fn


# ================================================================ relay mast (30 x 30, 120 m when finished)
MS_ANCH = [(13.0 * math.cos(math.radians(a)), 13.0 * math.sin(math.radians(a))) for a in (90.0, 210.0, 330.0)]


def mast_parts(st):
    name = f"hubc_mast_s{st}"

    def fn(lod):
        P = {name: Part(name)}
        p = P[name]
        Hm = {1: 20.0, 2: 60.0, 3: 120.0}[st]
        ab(p, CONC, -4.0, -1.5, -4.0, 4.0, 1.0, 4.0)
        for (x, z) in MS_ANCH:
            ab(p, CONC, x - 1.5, -1.5, z - 1.5, x + 1.5, 1.2, z + 1.5)
            if lod < 2:
                ab(p, YEL, x - 0.4, 1.2, z - 0.4, x + 0.4, 1.8, z + 0.4)
        lat(p, STEEL, 0.0, 0.0, 1.0, Hm, 1.5, 1.5, lod, bays=int(Hm / 3.0))
        guys = {1: [(18.0, True)], 2: [(40.0, False), (58.0, True)], 3: [(40.0, False), (80.0, False), (115.0, False)]}[st]
        if lod < 2:
            for (y, temp) in guys:
                for (x, z) in MS_ANCH:
                    d = norm((x, 0.0, z))
                    bar(p, NET if temp else STEEL, (d[0] * 1.5, y, d[2] * 1.5), (x, 1.8, z), 0.1 if temp else 0.14)
        # gin pole with a section on the hook
        if st < 3 or lod < 2:
            gy = Hm - 8.0 if st < 3 else 46.0
            lat(p, YEL, -2.2, -2.2, gy, gy + 20.0, 0.35, 0.35, lod, bays=7)
            if lod < 2:
                bar(p, STEEL, (-2.2, gy + 20.0, -2.2), (-6.0, 0.6, 9.0), 0.12)                    # load line to the winch
                ab(p, YEL, -7.5, 0.0, 8.0, -4.5, 1.8, 10.5)
                if st < 3:
                    bar(p, STEEL, (-2.2, gy + 20.0, -2.2), (-4.5, Hm + 7.5, -4.5), 0.1)
                    lat(p, STEEL, -4.5, -4.5, Hm + 0.8, Hm + 6.8, 1.5, 1.5, lod, bays=2)
        if st < 3 and lod < 2:                                                                 # sections waiting
            for z in (3.0, 7.0):
                sec = Part("t")
                btruss(sec, STEEL, (0.0, 1.5, 0.0), (6.0, 1.5, 0.0), 3.0, 3.0, lod)
                put(p, sec, -12.0, 0.0, z)
        if st == 3:
            ab(p, GRATE, -2.6, Hm, -2.6, 2.6, Hm + 0.3, 2.6)
            if lod < 2:
                for y in (40.0, 80.0):
                    beacon(p, (1.7, y, 1.7), 0.6)
                    beacon(p, (-1.7, y, -1.7), 0.6)
        beacon(p, (0.0, Hm + 0.8, 0.0), 0.9)
        if lod < 2:
            flood(p, (4.5, 1.6, 0.0), 1.0)
            cabin(p, lod, 6.0, 0.0, -9.0)
        site_marks(p, lod, 30.0, 30.0, 0.6)
        return P
    return fn


def mast_hits(st):
    name = f"hubc_mast_s{st}"

    def fn():
        Hm = {1: 20.0, 2: 60.0, 3: 120.0}[st]
        H = [hbox(name, (0, -0.25, 0), (8.0, 2.5, 8.0)), hbox(name, (0, (1.0 + Hm) / 2, 0), (3.4, Hm - 1.0, 3.4))]
        for (x, z) in MS_ANCH:
            H.append(hbox(name, (x, -0.15, z), (3.0, 2.7, 3.0)))
        return H
    return fn


# ================================================================ minehead (60 x 60: shaft collar, headframe, winder house)
MH_Z = -12.0           # shaft centre
MH_H = 52.0            # sheave deck


def _mh_half(y):
    return 7.0 - 3.8 * (y - 2.0) / (MH_H - 2.0)


def minehead_parts(st):
    name = f"hubc_minehead_s{st}"

    def fn(lod):
        P = {name: Part(name)}
        p = P[name]
        seg = {0: 32, 1: 20, 2: 10}[lod]
        ylathe(p, CONC, [(4.0, -1.0), (6.5, -1.0), (6.5, 2.0), (4.0, 2.0)], seg, t=(0.0, 0.0, MH_Z))      # collar
        ylathe(p, DARK, [(4.0, 0.3), (0.0, 0.3)], seg, t=(0.0, 0.0, MH_Z))
        for sx in (-1, 1):
            for sz in (-1, 1):
                ab(p, CONC, sx * 7.0 - 1.6, -1.0, MH_Z + sz * 7.0 - 1.6, sx * 7.0 + 1.6, 2.0, MH_Z + sz * 7.0 + 1.6)
            ab(p, CONC, sx * 6.0 - 1.8, -1.0, 12.2, sx * 6.0 + 1.8, 2.0, 15.8)                    # back-leg footings
        ab(p, CONC, -13.0, -0.5, 16.0, 13.0, 0.6, 29.0)                                           # winder house slab
        for x in (-4.0, 4.0):
            ab(p, CONC, x - 1.5, 0.6, 20.0, x + 1.5, 3.0, 25.0)                                    # drum plinths
        if st == 1:
            starters(p, [(sx * 7.0 + dx, MH_Z + sz * 7.0 + dz) for sx in (-1, 1) for sz in (-1, 1)
                         for dx in (-0.6, 0.6) for dz in (-0.6, 0.6)], 2.0, 1.2)
            # temporary sinking headframe over the shaft, kibble on its rope, sinking winch
            lat(p, YEL, 0.0, MH_Z, 2.0, 20.0, 4.0, 1.5, lod, bays=4)
            lathe(p, STEEL, crect(1.0, 1.4, -0.2, 0.2), 16, rot_y(math.pi / 2), (0.0, 21.0, MH_Z), smooth=False)
            ab(p, YEL, -1.0, 20.0, MH_Z - 1.4, 1.0, 20.6, MH_Z + 1.4)
            cyl(p, STEEL, (0.0, 21.0, MH_Z - 1.3), (0.0, 9.0, MH_Z - 1.3), 0.06 if lod == 0 else 0.15, 4)
            ylathe(p, STEEL, [(0.0, 6.0), (1.1, 6.0), (1.3, 9.0), (0.0, 9.0)], 12 if lod < 2 else 6, t=(0.0, 0.0, MH_Z - 1.3))
            ab(p, YEL, -1.5, 0.6, 3.0, 1.5, 3.0, 7.0)
            if lod < 2:
                bar(p, STEEL, (0.0, 22.2, MH_Z), (0.0, 3.0, 4.0), 0.12)
            beacon(p, (0.0, 21.8, MH_Z), 0.7)
        else:
            top = 30.0 if st == 2 else MH_H
            lat(p, PRIM, 0.0, MH_Z, 2.0, top, 7.0, _mh_half(top), lod, bays=int(top / 6.0))
            fy = 0.5 if st == 2 else 1.0
            for sx in (-1, 1):
                a = (sx * 6.0, 2.0, 14.0)
                b = (sx * 3.2, MH_H, MH_Z + 3.2)
                btruss(p, PRIM, a, _lerp(a, b, fy), 1.6, 1.6, lod, up=(1.0, 0.0, 0.0))
                if st == 2 and lod < 2:
                    m = _lerp(a, b, fy)
                    bar(p, STEEL, (m[0], 0.6, m[2]), m, 0.5)                                       # temporary prop
            if st == 2 and lod < 2:
                hh = _mh_half(top)
                for sx in (-1, 1):
                    flood(p, (sx * hh, top + 0.5, MH_Z), 1.0)
            beacon(p, (0.0, top + 0.7, MH_Z), 0.9)
        if st == 3:
            ab(p, YEL, -4.6, MH_H, MH_Z - 4.6, 4.6, MH_H + 1.2, MH_Z + 4.6)                         # sheave deck
            ab(p, PRIM, -5.0, MH_H + 7.0, MH_Z - 5.0, 5.0, MH_H + 8.0, MH_Z + 5.0)                  # crane beam
            for sx in (-1, 1):
                for sz in (-1, 1):
                    bar(p, PRIM, (sx * 4.2, MH_H + 1.2, MH_Z + sz * 4.2), (sx * 4.2, MH_H + 7.0, MH_Z + sz * 4.2), 0.5)
            for x in (-1.6, 1.6):
                lathe(p, STEEL, crect(2.8, 3.5, -0.35, 0.35), seg, rot_y(math.pi / 2), (x, MH_H + 4.4, MH_Z), smooth=False)
                cyl(p, DARK, (x - 0.5, MH_H + 4.4, MH_Z), (x + 0.5, MH_H + 4.4, MH_Z), 0.6, 8)
            beacon(p, (0.0, MH_H + 8.8, MH_Z), 1.0)
            # winder house frame, unclad
            for x in (-12.0, 12.0):
                for z in (16.5, 20.5, 24.5, 28.5):
                    ab(p, PRIM, x - 0.4, 0.6, z - 0.4, x + 0.4, 12.0, z + 0.4)
            for z in (16.5, 20.5, 24.5, 28.5):
                ptruss(p, PRIM, (-12.0, 14.0, z), (12.0, 14.0, z), (-12.0, 12.0, z), (12.0, 12.0, z), 8, lod, 0.35)
            if lod < 2:
                for x in (-12.0, -6.0, 0.0, 6.0, 12.0):
                    ab(p, STEEL, x - 0.15, 14.0, 16.5, x + 0.15, 14.4, 28.5)
            cyl(p, STEEL, (-6.0, 6.0, 22.5), (6.0, 6.0, 22.5), 3.0, seg)                            # winding drum
            if lod < 2:
                flood(p, (0.0, 14.8, 22.5), 1.2)
        # site
        site_marks(p, lod, 60.0, 60.0)
        if lod < 2:
            light_mast(p, lod, -27.0, -27.0, 12.0)
            light_mast(p, lod, 27.0, -27.0, 12.0)
        cabin(p, lod, 18.0, 0.0, -25.0)
        for (kind, x, y, z, ang) in {1: [("beams", -21.0, 0.0, -10.0, math.pi / 2),("cont_s", 18.0, 0.0, 4.0, math.pi / 2)],
                                     2: [("beams_s", -20.0, 0.0, 4.0, math.pi / 2), ("pallets", 20.0, 0.0, 6.0, 0.0)],
                                     3: [("pallets", 20.0, 0.0, 6.0, 0.0)]}[st]:
            if lod == 2 and kind == "pallets":
                continue
            stack(p, lod, kind, x, y, z, ang)
        return P
    return fn


def minehead_hits(st):
    name = f"hubc_minehead_s{st}"

    def fn():
        H = [hcyl(name, (0.0, -1.0, MH_Z), (0.0, 2.0, MH_Z), 6.5), hbox(name, (0, 0.05, 22.5), (26.0, 1.1, 13.0))]
        if st == 1:
            H.append(hbox(name, (0, 11.0, MH_Z), (8.0, 18.0, 8.0)))
        else:
            top = 30.0 if st == 2 else MH_H + 8.0
            H.append(hbox(name, (0, (2.0 + top) / 2, MH_Z), (14.0, top - 2.0, 14.0)))
            H.append(hbox(name, (0, (2.0 + MH_H) / 2 * (0.6 if st == 2 else 1.0), 2.0), (14.0, (MH_H - 2.0) * (0.5 if st == 2 else 1.0), 26.0)))
        if st == 3:
            H.append(hbox(name, (0, 7.3, 22.5), (25.0, 13.4, 13.0)))
        return H
    return fn


# ================================================================ tower crane (mast, jib), gantry
def crane_mast_parts(Hm, name):
    def fn(lod):
        P = {name: Part(name)}
        p = P[name]
        seg = {0: 32, 1: 20, 2: 10}[lod]
        # cross base with ballast
        ab(p, CONC, -4.0, -1.0, -4.0, 4.0, 1.0, 4.0)
        for R in (I3, rot_y(math.pi / 2)):
            box(p, YEL, (0.0, 0.6, 0.0), (16.0, 1.2, 1.4), R)
        for (x, z) in ((7.0, 0.0), (-7.0, 0.0), (0.0, 7.0), (0.0, -7.0)):
            for k in range(2):
                ab(p, CONC, x - 1.6, 1.2 + 1.3 * k, z - 1.6, x + 1.6, 2.4 + 1.3 * k, z + 1.6)
        lat(p, YEL, 0.0, 0.0, 1.2, Hm - 2.5, 3.0, 3.0, lod, bays=int((Hm - 3.7) / 6.0), chord=0.45)
        ylathe(p, YEL, [(0.0, Hm - 2.5), (3.6, Hm - 2.5), (4.2, Hm - 1.6), (4.2, Hm), (0.0, Hm)], seg)      # slewing ring
        if lod < 2:
            ylathe(p, STEEL, crect(4.2, 4.35, Hm - 1.4, Hm - 0.4), seg)
            y = 30.0
            while y < Hm - 10.0:
                ab(p, GRATE, -2.8, y, -2.8, 2.8, y + 0.2, 2.8)                                     # rest platforms
                y += 30.0
            y = 45.0
            while y < Hm - 5.0:
                for (x, z) in ((3.3, 3.3), (-3.3, -3.3)):
                    beacon(p, (x, y, z), 0.6)
                y += 45.0
            if close():
                for x in (-0.3, 0.3):
                    ab(p, STEEL, x - 0.04, 1.2, 2.2, x + 0.04, Hm - 2.5, 2.28)                   # ladder rails
            flood(p, (4.6, Hm - 3.2, 0.0), 1.0)
        beacon(p, (3.4, Hm - 3.0, 3.4), 0.8)
        return P
    return fn


def crane_mast_hits(Hm, name):
    return lambda: [hbox(name, (0, 0.6, 0), (16.0, 3.6, 16.0)), hbox(name, (0, Hm / 2, 0), (6.6, Hm, 6.6)),
                    hcyl(name, (0, Hm - 2.5, 0), (0, Hm, 0), 4.2)]


def crane_jib_parts(lod):
    """the slewing upper works, origin at the slewing pivot (top of the mast's slewing ring); jib along +Z"""
    name = "hubc_crane_jib"
    P = {name: Part(name)}
    p = P[name]
    ab(p, YEL, -3.5, 0.0, -3.5, 3.5, 3.0, 3.5)                                                     # turntable
    ab(p, LIGHT, 3.5, 2.0, 1.5, 6.3, 5.4, 5.0)                                                     # cab
    if lod < 2:
        box(p, COOL, (4.9, 4.2, 5.05), (2.4, 1.6, 0.08))
        box(p, COOL, (6.35, 4.2, 3.3), (0.08, 1.6, 3.0))
    # tower head (A-frame)
    apex = (0.0, 28.0, 0.0)
    for sx in (-1, 1):
        for sz in (-1, 1):
            bar(p, YEL, (sx * 2.5, 3.0, sz * 2.5), apex, 0.5)
    if lod < 2:
        for y in (10.0, 18.0):
            t = (y - 3.0) / 25.0
            h = 2.5 * (1 - t)
            for (a, b) in (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h))):
                bar(p, YEL, (a[0], y, a[1]), (b[0], y, b[1]), 0.3)
    # jib: triangular lattice, bottom chords x +-1.6 at y 3.5, top chord tapering 7.5 -> 5.5
    z0, z1 = 3.5, 80.0
    if lod >= 2:
        oslab(p, YEL, (0.0, 5.0, z0), (0.0, 4.5, z1), (1.0, 0.0, 0.0), 3.2, 3.0)
    else:
        n = int((z1 - z0) / (4.0 if lod == 0 else 8.0))
        tc = lambda t: (0.0, 7.5 - 2.0 * t, z0 + (z1 - z0) * t)
        bl = lambda t: (-1.6, 3.5, z0 + (z1 - z0) * t)
        br = lambda t: (1.6, 3.5, z0 + (z1 - z0) * t)
        for f in (tc, bl, br):
            bar(p, YEL, f(0.0), f(1.0), 0.35)
        for i in range(n + 1):
            t = i / n
            bar(p, YEL, bl(t), br(t), 0.2)
            if i < n:
                t1 = (i + 1) / n
                bar(p, YEL, bl(t), tc(t1), 0.2)
                bar(p, YEL, br(t), tc(t1), 0.2)
                if lod == 0:
                    bar(p, YEL, bl(t), br(t1), 0.15)
    # counter-jib with walkway, winch and counterweights
    for x in (-1.8, 1.8):
        ab(p, YEL, x - 0.3, 3.0, -25.0, x + 0.3, 4.4, -3.5)
    if lod < 2:
        ab(p, GRATE, -1.5, 3.4, -24.0, 1.5, 3.6, -3.5)
        if lod == 0:
            for x in (-2.2, 2.2):
                ab(p, YEL, x - 0.05, 4.4, -24.0, x + 0.05, 5.4, -3.5)
    ab(p, LIGHT, -1.4, 4.4, -13.0, 1.4, 6.6, -8.0)                                                 # hoist winch house
    for k in range(5):
        ab(p, CONC, -2.6, -1.6, -19.2 - 1.15 * k, 2.6, 4.2, -18.2 - 1.15 * k)                      # counterweight slabs
    # pendants
    for x in (-0.4, 0.4):
        for zt in (35.0, 70.0):
            t = (zt - z0) / (z1 - z0)
            bar(p, STEEL, (x, 28.0, 0.0), (x, 7.5 - 2.0 * t, zt), 0.14)
        bar(p, STEEL, (x, 28.0, 0.0), (x, 4.4, -25.0), 0.14)
    # trolley at 50 m, hook block on four falls
    ab(p, YEL, -1.9, 2.4, 48.5, 1.9, 3.5, 51.5)
    for dx in (-0.5, 0.5):
        for dz in (-0.5, 0.5):
            ab(p, STEEL, dx - 0.04, -30.0, 50.0 + dz - 0.04, dx + 0.04, 2.4, 50.0 + dz + 0.04)
    ab(p, YEL, -0.9, -32.0, 49.3, 0.9, -30.0, 50.7)
    ab(p, STEEL, -0.25, -33.6, 49.75, 0.25, -32.0, 50.25)
    if lod < 2:
        flood(p, (0.0, 3.0, 20.0), 1.0)
        flood(p, (0.0, 3.0, 62.0), 1.0)
    beacon(p, (0.0, 7.9, 80.0), 0.8)
    beacon(p, (0.0, 5.0, -25.4), 0.8)
    beacon(p, (0.0, 28.6, 0.0), 0.9)
    return P


def crane_jib_hits():
    n = "hubc_crane_jib"
    return [hbox(n, (0, 1.5, 0), (7.0, 3.0, 7.0)), hbox(n, (0, 15.5, 0), (5.0, 25.0, 5.0)),
            hbox(n, (0, 5.5, 41.75), (3.6, 4.4, 76.5)), hbox(n, (0, 1.4, -14.25), (5.2, 6.0, 21.5)),
            hbox(n, (0, -31.8, 50.0), (1.8, 3.6, 1.4))]


GT_Y = 112.0


def gantry_parts(lod):
    """a space gantry (goliath) crane: span 260 m along X, legs on rail bogies at x = +-130, girders at 112..120 m"""
    name = "hubc_gantry"
    P = {name: Part(name)}
    p = P[name]
    for sx in (-1, 1):
        x = sx * 130.0
        for sz in (-1, 1):
            btruss(p, YEL, (x, 7.0, sz * 22.0), (x, GT_Y, sz * 8.0), 5.0, 5.0, lod, up=(0.0, 0.0, 1.0), chord=0.6)
        ab(p, YEL, x - 2.5, 4.0, -27.0, x + 2.5, 7.0, 27.0)                                        # sill beam
        for z in (-21.0, -9.0, 9.0, 21.0):
            ab(p, STEEL, x - 2.0, 0.9, z - 4.5, x + 2.0, 4.0, z + 4.5)                             # bogies
            if lod < 2:
                for dz in (-2.6, 2.6):
                    cyl(p, DARK, (x - 2.2, 0.9, z + dz), (x + 2.2, 0.9, z + dz), 0.9, 10 if lod == 0 else 6)
        if lod < 2:
            btruss(p, YEL, (x, 40.0, -16.5), (x, 40.0, 16.5), 2.0, 2.0, lod)
    for sz in (-1, 1):
        ab(p, YEL, -140.0, GT_Y, sz * 7.0 - 1.6, 140.0, GT_Y + 8.0, sz * 7.0 + 1.6)                 # box girders
        if lod == 0:
            for x in range(-130, 131, 10):
                ab(p, YEL, x - 0.15, GT_Y, sz * 8.65 - 0.1, x + 0.15, GT_Y + 8.0, sz * 8.65 + 0.1)
            ab(p, GRATE, -140.0, GT_Y + 8.0, sz * 7.0 - 1.0, 140.0, GT_Y + 8.2, sz * 7.0 + 1.0)
    for x in (-140.0, 140.0):
        ab(p, YEL, x - 1.5, GT_Y, -8.6, x + 1.5, GT_Y + 8.0, 8.6)
    # trolley, hoist falls, hook block, cab
    ab(p, YEL, 22.0, GT_Y + 8.0, -9.0, 38.0, GT_Y + 12.0, 9.0)
    ab(p, LIGHT, 26.0, GT_Y + 12.0, -4.0, 34.0, GT_Y + 16.0, 4.0)
    for dx in (-1.5, 1.5):
        for dz in (-1.5, 1.5):
            ab(p, STEEL, 30.0 + dx - 0.08, 47.0, dz - 0.08, 30.0 + dx + 0.08, GT_Y, dz + 0.08)
    ab(p, YEL, 27.5, 44.0, -2.5, 32.5, 47.0, 2.5)
    ab(p, STEEL, 29.4, 41.0, -0.6, 30.6, 44.0, 0.6)
    ab(p, LIGHT, -120.0, GT_Y - 6.0, -3.0, -112.0, GT_Y, 3.0)
    if lod < 2:
        box(p, WARM, (-116.0, GT_Y - 3.0, -3.05), (6.0, 2.0, 0.08))
        for x in range(-120, 121, 40):
            flood(p, (float(x), GT_Y - 0.4, 0.0), 1.4)
    for sx in (-1, 1):
        for sz in (-1, 1):
            beacon(p, (sx * 139.0, GT_Y + 8.8, sz * 7.0), 1.0)
    beacon(p, (30.0, GT_Y + 16.8, 0.0), 0.8)
    return P


def gantry_hits():
    n = "hubc_gantry"
    H = [hbox(n, (0, GT_Y + 4.0, 0), (280.0, 8.0, 17.2))]
    for sx in (-1, 1):
        H.append(hbox(n, (sx * 130.0, GT_Y / 2, 0), (6.0, GT_Y, 50.0)))
    H.append(hbox(n, (30.0, 44.0, 0), (5.0, 6.0, 5.0)))
    return H


# ================================================================ standalone stacks
def single(name, build):
    def fn(lod):
        P = {name: Part(name)}
        merge(P[name], build(lod))
        return P
    return fn


# ================================================================ temporary loading platform (deck top exactly y = 15)
PF_Y = 15.0


def platform_parts(lod):
    name = "hubc_platform"
    P = {name: Part(name)}
    p = P[name]
    ab(p, GRATE, -30.0, PF_Y - 0.4, -20.0, 30.0, PF_Y, 20.0)                                      # grating deck
    for z in (-18.0, -6.0, 6.0, 18.0):                                                           # deck trusses
        if lod >= 2:
            ab(p, PRIM, -30.0, PF_Y - 3.4, z - 0.3, 30.0, PF_Y - 0.4, z + 0.3)
        else:
            ptruss(p, PRIM, (-30.0, PF_Y - 0.6, z), (30.0, PF_Y - 0.6, z), (-30.0, PF_Y - 3.4, z), (30.0, PF_Y - 3.4, z), 12, lod, 0.4)
    if lod < 2:
        for x in range(-30, 31, 10):
            ab(p, PRIM, x - 0.25, PF_Y - 0.9, -20.0, x + 0.25, PF_Y - 0.4, 20.0)
    for x in (-24.0, 0.0, 24.0):
        for z in (-16.0, 16.0):
            lat(p, PRIM, x, z, 0.6, PF_Y - 3.4, 1.5, 1.5, lod, bays=4, chord=0.3)
            ab(p, CONC, x - 2.2, 0.0, z - 2.2, x + 2.2, 0.6, z + 2.2)
    if lod < 2:
        for z in (-16.0, 16.0):                                                                   # bracing between legs
            for (xa, xb) in ((-24.0, 0.0), (0.0, 24.0)):
                bar(p, STEEL, (xa, 1.0, z), (xb, PF_Y - 3.6, z), 0.25)
                bar(p, STEEL, (xb, 1.0, z), (xa, PF_Y - 3.6, z), 0.25)
    # ramp to the deck plane along -Z side (top at x = +26), trestles
    oslab(p, GRATE, (-30.0, 0.2, -24.0), (26.0, PF_Y - 0.2, -24.0), (0.0, 0.0, 1.0), 7.0, 0.4)
    ab(p, GRATE, 22.0, PF_Y - 0.4, -27.5, 30.0, PF_Y, -20.0)                                      # top landing
    for x in (-16.0, -2.0, 12.0):
        y = (x + 30.0) / 56.0 * (PF_Y - 0.4)
        for z in (-27.0, -21.0):
            ab(p, PRIM, x - 0.3, 0.0, z - 0.3, x + 0.3, y, z + 0.3)
    if lod < 2:
        for z in (-27.4, -20.6):
            bar(p, YEL, (-30.0, 1.4, z), (26.0, PF_Y + 0.9, z), 0.12)
    # railings (-X and +Z edges, -Z edge except the ramp landing), bollards on +X
    if lod < 2:
        for (x0, z0, x1, z1) in ((-30.0, -20.0, -30.0, 20.0), (-30.0, 20.0, 30.0, 20.0), (-30.0, -20.0, 22.0, -20.0)):
            for h in ((1.1,) if lod == 1 else (0.55, 1.1)):
                ab(p, YEL, min(x0, x1) - 0.05, PF_Y + h - 0.05, min(z0, z1) - 0.05, max(x0, x1) + 0.05, PF_Y + h + 0.05,
                   max(z0, z1) + 0.05)
            if lod == 0:
                L = math.hypot(x1 - x0, z1 - z0)
                for i in range(int(L / 2.5) + 1):
                    q = _lerp((x0, 0, z0), (x1, 0, z1), i * 2.5 / L)
                    ab(p, YEL, q[0] - 0.05, PF_Y, q[2] - 0.05, q[0] + 0.05, PF_Y + 1.1, q[2] + 0.05)
        for z in (-15.0, -5.0, 5.0, 15.0):
            cyl(p, STEEL, (28.8, PF_Y, z), (28.8, PF_Y + 1.0, z), 0.4, 8)
            cyl(p, HAZ, (28.8, PF_Y + 1.0, z), (28.8, PF_Y + 1.2, z), 0.55, 8)
    # landing box + H in hazard yellow
    cx, cz, s = 6.0, 0.0, 12.0
    for (a0, b0, a1, b1) in ((cx - s, cz - s, cx + s, cz - s + 0.8), (cx - s, cz + s - 0.8, cx + s, cz + s),
                             (cx - s, cz - s, cx - s + 0.8, cz + s), (cx + s - 0.8, cz - s, cx + s, cz + s)):
        ab(p, HAZ, a0, PF_Y, b0, a1, PF_Y + 0.06, b1)
    ab(p, HAZ, cx - 5.0, PF_Y, cz - 6.0, cx - 3.5, PF_Y + 0.06, cz + 6.0)
    ab(p, HAZ, cx + 3.5, PF_Y, cz - 6.0, cx + 5.0, PF_Y + 0.06, cz + 6.0)
    ab(p, HAZ, cx - 3.5, PF_Y, cz - 0.75, cx + 3.5, PF_Y + 0.06, cz + 0.75)
    for (x, z) in ((-29.0, -19.0), (-29.0, 19.0), (29.0, -19.0), (29.0, 19.0)):
        light_mast(p, lod, x, z, 10.0, PF_Y, 3)
        beacon(p, (x, PF_Y + 11.2, z), 0.6)
    for i, z in enumerate((-12.0, -9.0)):                                                         # containers parked
        container(p, lod, (-21.0, PF_Y, z), 12.19, CONTS[i])
    container(p, lod, (-21.0, PF_Y + CONT_H, -10.5), 12.19, CONTS[3])
    container(p, lod, (-24.0, PF_Y, 12.0), 6.06, CONTS[4])
    return P


def platform_hits():
    n = "hubc_platform"
    H = [hbox(n, (0, PF_Y - 1.9, 0), (60.0, 3.8, 40.0)), hbox(n, (-2.0, 7.6, -24.0), (58.0, 1.0, 7.0), (0.0, 0.0, 15.0))]
    for x in (-24.0, 0.0, 24.0):
        for z in (-16.0, 16.0):
            H.append(hbox(n, (x, (PF_Y - 3.4) / 2, z), (3.2, PF_Y - 3.4, 3.2)))
    H.append(hbox(n, (-21.0, PF_Y + CONT_H, -10.5), (12.2, 2 * CONT_H, 5.4)))
    return H


# ================================================================ barracks: 2 x 12 container huts, gallery, stairs
HUT_P, HUT_D, HUT_H = 6.1, 5.2, 2.9
BK_Z0, BK_Z1 = -4.0, 1.2           # hut depth (front at +Z)
BK_L = 12 * HUT_P


def barracks_part(lod, fine):
    q = Part("hubc_barracks")
    x0 = -BK_L / 2
    levels = (0.3, 0.3 + HUT_H + 0.1)
    if lod >= 2:
        for y in levels:
            ab(q, BARK, x0, y, BK_Z0, -x0, y + HUT_H, BK_Z1)
        ab(q, GRATE, x0, levels[1] - 0.15, BK_Z1, -x0, levels[1], BK_Z1 + 1.6)
        for i in range(0, 12, 3):
            box(q, WARM, (x0 + (i + 0.5) * HUT_P, levels[1] + 1.6, BK_Z1 + 0.05), (1.0, 0.8, 0.08))
        return q
    ab(q, STEEL, x0, 0.0, BK_Z0 + 0.4, -x0, 0.3, BK_Z0 + 0.9)
    ab(q, STEEL, x0, 0.0, BK_Z1 - 0.9, -x0, 0.3, BK_Z1 - 0.4)
    for lv, y in enumerate(levels):
        for i in range(12):
            xc = x0 + (i + 0.5) * HUT_P
            h = _hash(i, lv)
            mat = BARK2 if h < 0.2 else (CGRY if h > 0.93 else BARK)
            ab(q, mat, xc - HUT_P / 2 + 0.05, y, BK_Z0, xc + HUT_P / 2 - 0.05, y + HUT_H, BK_Z1)
            box(q, PLY if _hash(i, lv, 1) < 0.3 else DARK, (xc + 1.9, y + 1.05, BK_Z1 + 0.04), (0.9, 2.05, 0.08))   # door
            lit = _hash(i, lv, 2) > 0.55
            box(q, WARM if lit else DARK, (xc - 1.0, y + 1.6, BK_Z1 + 0.04), (1.0, 0.8, 0.08))            # window
            box(q, WARM if _hash(i, lv, 3) > 0.7 else DARK, (xc, y + 1.6, BK_Z0 - 0.04), (0.8, 0.6, 0.08))
            ab(q, LIGHT, xc + 0.6, y + 1.9, BK_Z0 - 0.5, xc + 1.4, y + 2.5, BK_Z0)                      # wall AC unit
            if lv == 1:
                ab(q, LIGHT, xc - 1.6, y + HUT_H, BK_Z0 + 1.0, xc - 0.4, y + HUT_H + 0.8, BK_Z0 + 2.0)  # roof AC box
            if fine:
                for k in range(10):                                                                     # corrugation
                    xr = xc - HUT_P / 2 + 0.35 + k * 0.6
                    if abs(xr - (xc + 1.9)) < 0.6 or abs(xr - (xc - 1.0)) < 0.65:
                        continue
                    ab(q, mat, xr - 0.1, y + 0.05, BK_Z1, xr + 0.1, y + HUT_H - 0.05, BK_Z1 + 0.05)
                    ab(q, mat, xr - 0.1, y + 0.05, BK_Z0 - 0.05, xr + 0.1, y + HUT_H - 0.05, BK_Z0)
                ab(q, STEEL, xc - 0.1, y + HUT_H + 0.02, BK_Z0 + 0.3, xc + 0.1, y + HUT_H + 0.25, BK_Z0 + 0.6)  # vent
    # gallery (upper level walkway) on posts, rails
    yg = levels[1]
    ab(q, GRATE, x0, yg - 0.15, BK_Z1, -x0, yg, BK_Z1 + 1.6)
    zr = BK_Z1 + 1.55
    for i in range(13):
        x = x0 + i * HUT_P
        ab(q, STEEL, x - 0.07, 0.0, zr - 0.07, x + 0.07, yg + 1.1, zr + 0.07)
    ab(q, STEEL, x0, yg + 1.0, zr - 0.04, -x0, yg + 1.1, zr + 0.04)
    if lod == 0:
        ab(q, STEEL, x0, yg + 0.5, zr - 0.03, -x0, yg + 0.56, zr + 0.03)
    # stairs at both ends (in front of the gallery)
    for sx in (-1, 1):
        xa, xb = sx * (BK_L / 2), sx * (BK_L / 2 - 5.2)
        a, b = (xa, 0.0, BK_Z1 + 2.2), (xb, yg, BK_Z1 + 2.2)
        oslab(q, STEEL, a, b, (0.0, 0.0, 1.0), 1.1, 0.25)
        if lod == 0:
            for k in range(16):
                c = _lerp(a, b, (k + 0.5) / 16)
                ab(q, GRATE, c[0] - 0.15, c[1] + 0.1, BK_Z1 + 1.65, c[0] + 0.15, c[1] + 0.16, BK_Z1 + 2.75)
        bar(q, STEEL, (xa, 1.0, BK_Z1 + 2.8), (xb, yg + 1.0, BK_Z1 + 2.8), 0.06 if lod == 0 else 0.12)
        ab(q, GRATE, xb, yg - 0.15, BK_Z1 + 1.6, xb - sx * 0.8, yg, BK_Z1 + 2.8)                   # top landing
    # pipes along the back, a cable sagging between huts, lamps
    cyl(q, STEEL, (x0, 1.0, BK_Z0 - 0.8), (-x0, 1.0, BK_Z0 - 0.8), 0.15, 6)
    cyl(q, TANK_W, (x0, 4.2, BK_Z0 - 0.75), (-x0, 4.2, BK_Z0 - 0.75), 0.2, 6)
    if lod == 0:
        for i in range(12):                                                                      # pipe brackets
            x = x0 + (i + 0.5) * HUT_P
            ab(q, STEEL, x - 0.05, 0.8, BK_Z0 - 1.0, x + 0.05, 4.5, BK_Z0)
        for i in range(0, 12, 2):                                                                # sagging cable
            pts = sag((x0 + i * HUT_P, 6.6, BK_Z0 + 0.3), (x0 + (i + 2) * HUT_P, 6.6, BK_Z0 + 0.3), 0.5, 6)
            tube(q, DARK, pts, 0.04, 4)
    for i in range(0, 13, 4):
        box(q, FLOOD, (x0 + i * HUT_P, yg + 2.6, BK_Z1 + 0.2), (0.4, 0.2, 0.3))
    # laundry lines on the gallery
    if fine:
        cols = (TARP, NET, TANK_W, YEL, CONTS[0], LIGHT, CONTS[1])
        for li in range(4):
            xa = x0 + (1 + 3 * li) * HUT_P
            ab(q, STEEL, xa - 0.6, yg + 2.2, BK_Z1 + 1.0, xa + 2 * HUT_P, yg + 2.23, BK_Z1 + 1.03)
            for k in range(6):
                if _hash(li, k) < 0.25:
                    continue
                x = xa + 0.4 + k * 1.9
                hh = 0.5 + 0.4 * _hash(li, k, 1)
                ab(q, cols[int(_hash(li, k, 2) * len(cols))], x - 0.35, yg + 2.2 - hh, BK_Z1 + 1.0, x + 0.35, yg + 2.2,
                   BK_Z1 + 1.03)
    return q


def barracks_parts(lod):
    P = {"hubc_barracks": barracks_part(lod, close())}
    return P


def barracks_hits():
    return [hbox("hubc_barracks", (0, (0.3 + 2 * HUT_H + 0.1) / 2 + 0.15, (BK_Z0 + BK_Z1) / 2), (BK_L, 2 * HUT_H + 0.4, BK_Z1 - BK_Z0)),
            hbox("hubc_barracks", (0, 3.4, BK_Z1 + 1.4), (BK_L, 1.0, 2.8))]


CAMP_ROWS = [-37.0, -22.0, -7.0, 8.0]
CAMP_RX = -18.0


def camp_parts(lod):
    name = "hubc_barracks_camp"
    P = {name: Part(name)}
    p = P[name]
    row = barracks_part(lod, False)
    for z in CAMP_ROWS:
        put(p, row, CAMP_RX, 0.0, z)
    # mess hut (5 container modules side by side, low roof)
    ab(p, BARK2, 26.0, 0.3, -36.0, 56.0, 4.5, -24.0)
    ab(p, STEEL, 25.6, 4.5, -36.4, 56.4, 4.8, -23.6)
    ab(p, STEEL, 26.0, 0.0, -35.0, 56.0, 0.3, -25.0)
    if lod < 2:
        for i in range(10):
            box(p, WARM if _hash(i, 77) > 0.25 else DARK, (27.5 + i * 3.0, 2.6, -23.95), (1.6, 1.0, 0.08))
        box(p, DARK, (41.0, 1.4, -23.95), (1.8, 2.2, 0.08))
        ab(p, STEEL, 39.0, 2.9, -23.9, 43.0, 3.1, -22.0)
        cyl(p, STEEL, (50.0, 4.8, -30.0), (50.0, 7.0, -30.0), 0.35, 8)                              # kitchen vent
        for x in (30.0, 36.0, 46.0):
            ab(p, LIGHT, x, 4.8, -33.0, x + 1.4, 5.6, -31.6)
    # washroom / toilet block with vent pipes
    ab(p, BARK, 30.0, 0.3, -16.0, 52.0, 3.4, -10.0)
    ab(p, STEEL, 30.0, 0.0, -15.5, 52.0, 0.3, -10.5)
    if lod < 2:
        for i in range(6):
            box(p, LIGHT, (32.0 + i * 3.5, 2.8, -9.95), (1.2, 0.4, 0.08))
            box(p, DARK if i % 3 else PLY, (33.5 + i * 3.5, 1.3, -9.95), (0.9, 2.0, 0.08))
        for x in (34.0, 41.0, 48.0):
            cyl(p, STEEL, (x, 3.4, -14.0), (x, 5.5, -14.0), 0.15, 6)
    # water tank on stilts
    for sx in (-1, 1):
        for sz in (-1, 1):
            bar(p, STEEL, (47.0 + sx * 3.2, 0.0, 10.0 + sz * 3.2), (47.0 + sx * 2.4, 10.0, 10.0 + sz * 2.4), 0.35)
    ab(p, GRATE, 43.5, 9.8, 6.5, 50.5, 10.1, 13.5)
    ylathe(p, TANK_W, [(0.0, 10.1), (3.4, 10.1), (3.4, 14.5), (2.6, 15.6), (0.0, 15.8)], {0: 20, 1: 12, 2: 8}[lod], t=(47.0, 0.0, 10.0))
    if lod < 2:
        bar(p, STEEL, (47.0, 10.5, 10.0), (38.0, 1.0, -9.5), 0.15)                                  # pipe to the washroom
        if lod == 0:
            for sx in (-0.3, 0.3):
                ab(p, STEEL, 43.6 + sx - 0.03, 0.0, 12.4, 43.6 + sx + 0.03, 10.0, 12.46)            # ladder
    # generator container with exhaust stack and fuel tank
    container(p, lod, (34.0, 0.0, 30.0), 12.19, CGRN)
    if lod < 2:
        ab(p, HAZ, 27.9, 2.3, 28.7, 40.1, 2.6, 31.3)
        cyl(p, STEEL, (38.0, CONT_H, 30.0), (38.0, CONT_H + 3.5, 30.0), 0.3, 8)
        cyl(p, DARK, (38.0, CONT_H + 3.5, 30.0), (38.0, CONT_H + 3.8, 30.0), 0.35, 8)
        cyl(p, TANK_W, (28.0, 1.4, 35.0), (40.0, 1.4, 35.0), 1.2, 12 if lod == 0 else 8)
        for z in CAMP_ROWS:                                                                      # cables to the rows
            bar(p, DARK, (30.0, 0.15, 30.0), (CAMP_RX + BK_L / 2, 0.15, z - 4.6), 0.12)
    # fence with a gate on -Z, lamp posts
    if lod < 2:
        step = 5.0 if lod == 0 else 10.0
        for (x0, z0, x1, z1) in ((-59.0, -44.0, 30.0, -44.0), (42.0, -44.0, 59.0, -44.0), (-59.0, 44.0, 59.0, 44.0),
                                 (-59.0, -44.0, -59.0, 44.0), (59.0, -44.0, 59.0, 44.0)):
            L = math.hypot(x1 - x0, z1 - z0)
            n = max(1, int(L / step))
            for i in range(n + 1):
                q = _lerp((x0, 0, z0), (x1, 0, z1), i / n)
                ab(p, STEEL, q[0] - 0.06, 0.0, q[2] - 0.06, q[0] + 0.06, 2.6, q[2] + 0.06)
            for y in ((0.2, 1.3, 2.4) if lod == 0 else (2.4,)):
                ab(p, STEEL, min(x0, x1) - 0.03, y - 0.03, min(z0, z1) - 0.03, max(x0, x1) + 0.03, y + 0.03, max(z0, z1) + 0.03)
        for (x, z) in ((-56.0, -41.0), (-56.0, 41.0), (20.0, -41.0), (20.0, 41.0), (56.0, 0.0), (36.0, -41.0), (-18.0, 41.0)):
            light_mast(p, lod, x, z, 8.0, 0.0, 1)
        ab(p, HAZ, 30.0, 2.5, -44.2, 42.0, 2.8, -43.8)                                             # gate beam
    return P


def camp_hits():
    n = "hubc_barracks_camp"
    H = [hbox(n, (CAMP_RX, 3.35, z + (BK_Z0 + BK_Z1) / 2), (BK_L, 6.7, 8.0)) for z in CAMP_ROWS]
    H += [hbox(n, (41.0, 2.4, -30.0), (30.0, 4.8, 12.0)), hbox(n, (41.0, 1.7, -13.0), (22.0, 3.4, 6.0)),
          hcyl(n, (47.0, 0.0, 10.0), (47.0, 15.8, 10.0), 3.4), hbox(n, (34.0, CONT_H / 2, 30.0), (12.2, CONT_H, 2.44))]
    return H


# ================================================================ flyers (drawn by the traffic code: lean)
def nav_lights(p, xl, xr, y, z):
    box(p, BEACON, (xl, y, z), (0.5, 0.5, 0.5))
    box(p, NAVG, (xr, y, z), (0.5, 0.5, 0.5))


def rcs_quad(p, lod, c, out):
    """an RCS quad block at c, nozzles pointing along out (+-X) and +-Y/+-Z"""
    box(p, STEEL, c, (0.8, 0.8, 0.8))
    if lod < 2:
        for d in ((out, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1)):
            e = _add(c, _mul(d, 0.5))
            cyl(p, DARK, e, _add(e, _mul(d, 0.35)), 0.18, 6)


def hauler_parts(lod):
    """flying construction cargo transport, ~45 m, +Z forward: open truss cargo frame, containers + beam bundle,
    cab at the front, 4 thruster pods on outriggers, nav lights"""
    name = "hubc_hauler"
    P = {name: Part(name)}
    p = P[name]
    seg = {0: 14, 1: 10, 2: 6}[lod]
    # cargo frame: two side trusses + bottom deck frame (z -16..+14)
    for sx in (-1, 1):
        if lod >= 2:
            ab(p, YEL, sx * 4.2 - 0.3, -3.0, -16.0, sx * 4.2 + 0.3, 3.0, 14.0)
        else:
            ptruss(p, YEL, (sx * 4.2, 3.0, -16.0), (sx * 4.2, 3.0, 14.0), (sx * 4.2, -3.0, -16.0), (sx * 4.2, -3.0, 14.0),
                   8, lod, 0.45)
    for z in (-16.0, -6.0, 4.0, 14.0):
        ab(p, YEL, -4.2, -3.3, z - 0.3, 4.2, -2.7, z + 0.3)
        if lod < 2:
            ab(p, YEL, -4.2, 2.7, z - 0.3, 4.2, 3.3, z + 0.3)
    ab(p, STEEL, -4.0, -3.4, -16.0, 4.0, -3.1, 14.0)                                              # cargo floor
    for sx in (-1, 1):                                                                          # containers
        container(p, min(lod, 1), (sx * 1.4, -3.1, -6.0), 12.19, CONTS[1 + sx], False)
    for k in range(4 if lod < 2 else 1):                                                         # beam bundle
        if lod < 2:
            ab(p, PRIM, -2.6 + 1.3 * k - 0.5, -3.1, 2.0, -2.6 + 1.3 * k + 0.5, -2.1, 13.5)
        else:
            ab(p, PRIM, -2.6, -3.1, 2.0, 2.6, -2.1, 13.5)
    if lod < 2:
        for z in (4.0, 11.0):
            ab(p, NET, -2.8, -2.1, z - 0.15, 2.8, -1.9, z + 0.15)
    # cab
    box(p, HULL, (0.0, 0.0, 18.0), (7.0, 5.5, 8.0), chamfer=0.4 if lod == 0 else 0.0)
    box(p, HULL, (0.0, -0.8, 22.5), (5.6, 3.6, 1.6))
    box(p, COOL, (0.0, 1.0, 22.05), (5.0, 1.4, 0.1))
    if lod < 2:
        for sx in (-1, 1):
            box(p, WARM, (sx * 3.55, 1.0, 19.0), (0.08, 1.0, 3.0))
        box(p, HAZ, (0.0, -2.6, 22.0), (5.6, 0.4, 0.2))
    # thruster pods on outriggers
    for sx in (-1, 1):
        for sz in (-1, 1):
            c = (sx * 8.0, 0.0, sz * 15.0 - 3.0)
            bar(p, STEEL, (sx * 4.2, 0.0, c[2]), (sx * 6.6, 0.0, c[2]), 0.8)
            cyl(p, LIGHT, (c[0], 0.0, c[2] - 2.6), (c[0], 0.0, c[2] + 2.6), 1.4, seg)
            cyl(p, DARK, (c[0], 0.0, c[2] - 2.6), (c[0], 0.0, c[2] - 3.4), 1.1, seg)                 # main nozzle
            cyl(p, DARK, (c[0], -1.4, c[2]), (c[0], -2.0, c[2]), 0.8, seg)                          # hover nozzle
    nav_lights(p, -9.5, 9.5, 0.0, 12.0)
    box(p, FLOOD, (0.0, 2.9, -16.4), (0.5, 0.5, 0.5))
    if lod < 2:
        beacon(p, (0.0, 3.3, 18.0), 0.5)
        flood(p, (0.0, -2.2, 22.4), 0.8)
    return P


def hauler_hits():
    n = "hubc_hauler"
    return [hbox(n, (0, 0, -1.0), (9.0, 6.6, 30.0)), hbox(n, (0, 0, 18.5), (7.0, 5.5, 9.0)),
            hbox(n, (0, 0, 12.0), (19.6, 2.8, 5.2)), hbox(n, (0, 0, -18.0), (19.6, 2.8, 7.0))]


def tug_parts(lod):
    """compact construction tug ~12 x 8 x 14, +Z forward: boxy pusher, foam bumper frame at the front, RCS quads at
    the corners, tether reel, floods, hazard stripes, nav lights"""
    name = "hubc_tug"
    P = {name: Part(name)}
    p = P[name]
    box(p, YEL, (0.0, 0.0, -0.5), (9.0, 6.0, 10.0), chamfer=0.35 if lod == 0 else 0.0)
    box(p, HULL, (0.0, 2.6, 2.2), (5.0, 1.6, 3.4))                                              # cab
    box(p, COOL, (0.0, 2.7, 3.95), (4.2, 0.9, 0.1))
    # bumper frame + foam pads
    for sx in (-1, 1):
        bar(p, STEEL, (sx * 4.0, -2.5, 4.5), (sx * 5.0, -2.5, 6.4), 0.5)
        bar(p, STEEL, (sx * 4.0, 2.5, 4.5), (sx * 5.0, 2.5, 6.4), 0.5)
    ab(p, STEEL, -5.2, -3.0, 6.2, 5.2, 3.0, 6.6)
    for sx in (-1, 1):
        for sy in (-1, 1):
            ab(p, FOAM, sx * 2.7 - 2.2, sy * 1.5 - 1.3, 6.6, sx * 2.7 + 2.2, sy * 1.5 + 1.3, 7.4)
    if lod < 2:
        for k in range(5):                                                                     # hazard stripes
            box(p, "eq_metal_graphite",(-4.0 + 2.0 * k, -3.05, 4.45), (0.8, 0.12, 0.2), rot_z(0.6))
            ab(p, HAZ, -5.2 + 2.1 * k, 3.0, 6.2, -4.4 + 2.1 * k, 3.15, 6.6)
    # RCS quads at 8 corners
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                rcs_quad(p, lod, (sx * 4.9, sy * 3.1, -0.5 + sz * 4.6), sx)
    # main thrusters aft
    for sx in (-1, 1):
        cyl(p, STEEL, (sx * 2.2, -0.5, -5.5), (sx * 2.2, -0.5, -6.5), 1.2, 12 if lod < 2 else 6)
        cyl(p, DARK, (sx * 2.2, -0.5, -6.5), (sx * 2.2, -0.5, -6.9), 1.0, 12 if lod < 2 else 6)
    # tether reel on the roof and the cable stub
    cyl(p, STEEL, (-1.6, 3.9, -2.5), (1.6, 3.9, -2.5), 0.9, 12 if lod < 2 else 6)
    if lod < 2:
        cyl(p, NET, (-1.4, 3.9, -2.5), (1.4, 3.9, -2.5), 1.0, 12)
        for sx in (-1, 1):
            ab(p, STEEL, sx * 1.8 - 0.15, 3.0, -3.0, sx * 1.8 + 0.15, 4.6, -2.0)
        bar(p, NET, (0.0, 4.8, -2.5), (0.0, 5.5, -7.0), 0.12)
    # floods and nav lights
    for sx in (-1, 1):
        flood(p, (sx * 3.6, 3.3, 6.0), 0.8)
        flood(p, (sx * 3.6, -3.3, 6.0), 0.8)
    nav_lights(p, -5.6, 5.6, 1.5, 2.0)
    box(p, FLOOD, (0.0, 2.0, -5.8), (0.4, 0.4, 0.4))
    beacon(p, (0.0, 3.6, 3.8), 0.4)
    return P


def tug_hits():
    n = "hubc_tug"
    return [hbox(n, (0, 0.2, -0.3), (10.6, 7.4, 12.0)), hbox(n, (0, 0, 6.8), (10.4, 6.0, 1.2))]


CLAW_GRIP = (0.0, 0.0, 32.0)        # the held element's centre in the clawship frame


def clawship_parts(lod):
    """'assisted' construction ship: hull z -14..+12, a pair of articulated claws at x = +-8 reaching to z ~34; the
    jaws' pads clamp an element (hubc_element_*) whose centre sits at the grip point CLAW_GRIP"""
    name = "hubc_clawship"
    P = {name: Part(name)}
    p = P[name]
    seg = {0: 16, 1: 10, 2: 6}[lod]
    box(p, HULL, (0.0, 0.0, -1.0), (10.0, 7.0, 26.0), chamfer=0.5 if lod == 0 else 0.0)
    box(p, LIGHT, (0.0, 4.4, 8.0), (6.0, 2.4, 6.0))                                                 # cab
    box(p, COOL, (0.0, 4.6, 11.05), (5.0, 1.2, 0.1))
    if lod < 2:
        for sx in (-1, 1):
            box(p, WARM, (sx * 3.05, 4.6, 8.0), (0.08, 0.9, 3.5))
            box(p, HAZ, (sx * 5.05, -2.0, 4.0), (0.1, 0.6, 14.0))
        box(p, YEL, (0.0, 0.0, 12.1), (8.0, 4.0, 0.3))
    # engines aft
    for x in (-3.0, 0.0, 3.0):
        cyl(p, STEEL, (x, -0.5, -14.0), (x, -0.5, -15.5), 1.2, seg)
        cyl(p, DARK, (x, -0.5, -15.5), (x, -0.5, -16.0), 1.0, seg)
    # RCS clusters
    for sx in (-1, 1):
        for sz in (-1, 1):
            rcs_quad(p, lod, (sx * 5.4, 3.6, -1.0 + sz * 11.0), sx)
    # claws: shoulder -> elbow -> wrist above the grip point -> two fingers with pads (fore and aft of the element)
    gx, gy, gz = CLAW_GRIP
    for sx in (-1, 1):
        sh = (sx * 5.5, 1.0, 12.0)
        el = (sx * 11.0, 9.0, 21.0)
        wr = (sx * 8.0, gy + 8.5, gz)
        cyl(p, STEEL, (sx * 4.0, 1.0, 12.0), (sx * 7.0, 1.0, 12.0), 1.6, seg)
        bar(p, YEL, sh, el, 1.6)
        cyl(p, STEEL, (el[0] - 1.0, el[1], el[2]), (el[0] + 1.0, el[1], el[2]), 1.2, seg)
        bar(p, YEL, el, wr, 1.3)
        if lod < 2:
            bar(p, STEEL, _lerp(sh, el, 0.3), _lerp(el, wr, 0.4), 0.4)                               # hydraulic ram
        ab(p, STEEL, wr[0] - 0.9, wr[1] - 0.6, gz - 2.6, wr[0] + 0.9, wr[1] + 0.6, gz + 2.6)       # wrist / palm
        for fz in (-1, 1):
            z = gz + fz * 2.1
            ab(p, YEL, wr[0] - 0.6, gy - 2.5, z - 0.3, wr[0] + 0.6, wr[1] - 0.6, z + 0.3)        # finger
            ab(p, FOAM, wr[0] - 0.8, gy - 2.2, z - fz * 0.3 - 0.15, wr[0] + 0.8, gy + 2.2, z - fz * 0.3 + 0.15)  # pad
        flood(p, (sx * 9.0, el[1] + 1.2, el[2]), 0.8)
    for sx in (-1, 1):
        flood(p, (sx * 3.5, 1.5, 12.3), 1.0)
    nav_lights(p, -5.6, 5.6, 0.0, 8.0)
    box(p, FLOOD, (0.0, 2.0, -14.2), (0.4, 0.4, 0.4))
    beacon(p, (0.0, 5.9, 8.0), 0.5)
    return P


def clawship_hits():
    n = "hubc_clawship"
    H = [hbox(n, (0, 0.5, -1.5), (10.0, 8.0, 29.0))]
    for sx in (-1, 1):
        H.append(hbox(n, (sx * 9.0, 4.5, 21.0), (5.0, 10.0, 20.0)))
        H.append(hbox(n, (sx * 8.0, 2.6, 32.0), (1.8, 12.4, 5.8)))
    return H


def element_beam_parts(lod):
    """a structural truss segment 30 m along X, 3 m section, red-oxide primer, lifting lugs; origin at the centre"""
    name = "hubc_element_beam"
    P = {name: Part(name)}
    p = P[name]
    btruss(p, PRIM, (-15.0, 0.0, 0.0), (15.0, 0.0, 0.0), 3.0, 3.0, lod, chord=0.35)
    for x in (-15.0, 15.0):
        ab(p, STEEL, x - 0.2, -1.8, -1.8, x + 0.2, 1.8, 1.8)                                         # end plates
    if lod < 2:
        for x in (-8.0, 8.0):
            ab(p, YEL, x - 0.3, 1.5, -0.3, x + 0.3, 2.1, 0.3)
    return P


def element_panel_parts(lod):
    """a facade / hull panel 20 x 12 x 1 m (thin along Z), steel edge frame, lifting lugs; origin at the centre"""
    name = "hubc_element_panel"
    P = {name: Part(name)}
    p = P[name]
    ab(p, FACADE_L, -10.0, -6.0, -0.4, 10.0, 6.0, 0.4)
    for (x0, y0, x1, y1) in ((-10.0, -6.0, 10.0, -5.6), (-10.0, 5.6, 10.0, 6.0), (-10.0, -6.0, -9.6, 6.0), (9.6, -6.0, 10.0, 6.0)):
        ab(p, STEEL, x0, y0, -0.5, x1, y1, 0.5)
    if lod < 2:
        for x in (-6.0, 6.0):
            ab(p, YEL, x - 0.3, 6.0, -0.3, x + 0.3, 6.6, 0.3)
        ab(p, STEEL, -0.2, -5.6, -0.45, 0.2, 5.6, 0.45)
    return P


def pod_parts(lod):
    """a tethered worker pod ~3 m: sphere, lit window, manipulator arm, small RCS, tether stub; +Z forward"""
    name = "hubc_pod"
    P = {name: Part(name)}
    p = P[name]
    seg = {0: 20, 1: 12, 2: 8}[lod]
    n = seg // 2
    prof = [(1.4 * math.sin(math.pi * k / n), -1.4 * math.cos(math.pi * k / n)) for k in range(n + 1)]
    prof[0], prof[-1] = (0.0, -1.4), (0.0, 1.4)
    ylathe(p, YEL, prof, seg, soft=40)
    box(p, WARM, (0.0, 0.25, 1.32), (1.2, 0.8, 0.2))
    box(p, STEEL, (0.0, -1.05, 0.9), (0.5, 0.5, 0.6))
    bar(p, STEEL, (0.0, -1.05, 1.1), (0.4, -1.3, 2.4), 0.22)
    bar(p, STEEL, (0.4, -1.3, 2.4), (0.2, -0.8, 3.2), 0.18)
    if lod < 2:
        for fx in (-0.15, 0.15):
            bar(p, DARK, (0.2, -0.8, 3.2), (0.2 + fx * 2, -0.7, 3.6), 0.08)
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(p, STEEL, (sx * 1.15, sy * 0.85, -0.3), (0.35, 0.35, 0.35))
    cyl(p, STEEL, (0.0, 0.0, -1.35), (0.0, 0.0, -1.9), 0.18, 8)
    if lod < 2:
        bar(p, NET, (0.0, 0.0, -1.9), (0.0, 0.3, -4.0), 0.05)
        box(p, FLOOD, (0.6, 0.9, 1.0), (0.3, 0.2, 0.2))
    box(p, BEACON, (-1.45, 0.0, 0.0), (0.15, 0.15, 0.15))
    box(p, NAVG, (1.45, 0.0, 0.0), (0.15, 0.15, 0.15))
    return P


def jig_parts(lod):
    """a temporary truss jig / strongback fixture on the deck (60 x 60 x 80): corner lattice towers, perimeter trusses
    at 20 / 50 / 78 m, clamps holding a beam element and a panel element, work lights"""
    name = "hubc_jig"
    P = {name: Part(name)}
    p = P[name]
    c = 28.0
    for sx in (-1, 1):
        for sz in (-1, 1):
            lat(p, YEL, sx * c, sz * c, 0.6, 80.0, 1.2, 1.2, max(1, lod), bays=20, chord=0.3)
            ab(p, CONC, sx * c - 2.0, 0.0, sz * c - 2.0, sx * c + 2.0, 0.6, sz * c + 2.0)
            beacon(p, (sx * c, 80.7, sz * c), 0.7)
    for y in (20.0, 50.0, 78.0):
        for (a, b) in (((-c, -c), (c, -c)), ((-c, c), (c, c)), ((-c, -c), (-c, c)), ((c, -c), (c, c))):
            btruss(p, YEL, (a[0], y, a[1]), (b[0], y, b[1]), 1.6, 1.6, max(1, lod), chord=0.22, bay=3.2)
    # a beam held on the +Z truss at y 50 (two clamps), a panel hanging inside from the -Z truss at y 50
    put(p, element_beam_parts(lod)["hubc_element_beam"], 0.0, 54.0, c)
    for x in (-10.0, 10.0):
        ab(p, YEL, x - 0.8, 50.8, c - 2.0, x + 0.8, 55.8, c + 2.0)
    put(p, element_panel_parts(lod)["hubc_element_panel"], 0.0, 40.0, -c + 4.0)
    for x in (-8.0, 8.0):
        ab(p, YEL, x - 0.4, 46.0, -c - 0.5, x + 0.4, 50.0, -c + 0.5)
        bar(p, STEEL, (x, 46.0, -c), (x * 0.75, 46.6, -c + 4.0), 0.3)
        ab(p, YEL, x * 0.75 - 0.6, 45.6, -c + 3.2, x * 0.75 + 0.6, 46.8, -c + 4.8)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                for y in (21.0, 51.0, 79.0):
                    flood(p, (sx * (c - 1.6), y, sz * (c - 1.6)), 1.0)
        site_marks(p, lod, 64.0, 64.0, 0.8)
    return P


def jig_hits():
    n = "hubc_jig"
    H = []
    for sx in (-1, 1):
        for sz in (-1, 1):
            H.append(hbox(n, (sx * 28.0, 40.3, sz * 28.0), (2.6, 79.4, 2.6)))
    for y in (20.0, 50.0, 78.0):
        H += [hbox(n, (0, y, -28.0), (57.6, 1.8, 1.8)), hbox(n, (0, y, 28.0), (57.6, 1.8, 1.8)),
              hbox(n, (-28.0, y, 0), (1.8, 1.8, 57.6)), hbox(n, (28.0, y, 0), (1.8, 1.8, 57.6))]
    H += [hbox(n, (0, 54.0, 28.0), (30.4, 3.6, 3.6)), hbox(n, (0, 40.0, -24.0), (20.0, 12.0, 1.0))]
    return H


# ================================================================ the kit
STAGE_RU = {1: "фундамент", 2: "голый каркас", 3: "частично обшит", 4: "почти готов"}
KIT = {}
for _st in (1, 2, 3, 4):
    for _id, _C, _title in (("hubc_tower", TOWER, "Башня 100x100 (~300 м)"), ("hubc_hab", HAB, "Жилой блок 40x30 (~90 м)"),
                            ("hubc_slab", SLAB, "Жилая пластина 220x70 (~105 м)")):
        _n = f"{_id}_s{_st}"
        KIT[_n] = (bldg_parts(_n, _C, _st), bldg_hits(_n, _C, _st), f"Стройка: {_title}, стадия {_st} - {STAGE_RU[_st]}")
    KIT[f"hubc_hall_s{_st}"] = (hall_parts(_st), hall_hits(_st), f"Стройка: цех / склад 240x240, стадия {_st} - {STAGE_RU[_st]}")
    KIT[f"hubc_dock_s{_st}"] = (dock_parts(_st), dock_hits(_st), f"Стройка: пирс 130x300, стадия {_st} - {STAGE_RU[_st]}")
    KIT[f"hubc_yard_s{_st}"] = (yard_parts(_st), yard_hits(_st), f"Стройка: площадка 240x190, стадия {_st} - {STAGE_RU[_st]}")
for _st, _ru in ((1, "основание + 20 м решётки, монтажная мачта"), (2, "60 м решётки, оттяжки, монтажная мачта"),
                 (3, "120 м без антенн")):
    KIT[f"hubc_mast_s{_st}"] = (mast_parts(_st), mast_hits(_st), f"Стройка: мачта-ретранслятор, стадия {_st} - {_ru}")
for _st, _ru in ((1, "устье ствола и фундаменты, проходческий копёр"), (2, "ноги копра до 30 м, укосины"),
                 (3, "копёр со шкивами, каркас подъёмной машины")):
    KIT[f"hubc_minehead_s{_st}"] = (minehead_parts(_st), minehead_hits(_st), f"Стройка: копёр шахты, стадия {_st} - {_ru}")
KIT.update({
    "hubc_crane_mast": (crane_mast_parts(330.0, "hubc_crane_mast"), crane_mast_hits(330.0, "hubc_crane_mast"),
                        "Башенный кран: мачта 330 м (поворотный круг на y = 330)"),
    "hubc_crane_mast_s": (crane_mast_parts(120.0, "hubc_crane_mast_s"), crane_mast_hits(120.0, "hubc_crane_mast_s"),
                          "Башенный кран: мачта 120 м (поворотный круг на y = 120)"),
    "hubc_crane_jib": (crane_jib_parts, crane_jib_hits, "Башенный кран: поворотная часть (стрела 80 м, противовес)"),
    "hubc_gantry": (gantry_parts, gantry_hits, "Козловой кран: пролёт 260 м, балка на 112-120 м"),
    "hubc_stack_beams": (single("hubc_stack_beams", stack_beams_part),
                         lambda: [hbox("hubc_stack_beams", (0, 3.3, 0), (28.0, 6.6, 11.5))], "Штабель двутавров 30x12x6"),
    "hubc_stack_panels": (single("hubc_stack_panels", stack_panels_part),
                          lambda: [hbox("hubc_stack_panels", (0, 3.9, 0.5), (24.4, 7.8, 9.4))], "Стеллажи фасадных панелей 25x10x8"),
    "hubc_containers": (single("hubc_containers", lambda lod: containers_part(lod, 3, 5, 3, 11)),
                        lambda: [hbox("hubc_containers", (0, 3.9, 0), (39.6, 7.8, 13.8))], "Штабель контейнеров 40x15, до 3 ярусов"),
    "hubc_platform": (platform_parts, platform_hits, "Временная погрузочная платформа 60x40, палуба на 15 м"),
    "hubc_barracks": (barracks_parts, barracks_hits, "Барак: 2 яруса по 12 бытовок, галерея, лестницы"),
    "hubc_barracks_camp": (camp_parts, camp_hits, "Вахтовый посёлок 120x90: 4 барака, столовая, душевые, бак, генератор"),
    "hubc_hauler": (hauler_parts, hauler_hits, "Летающий строительный транспорт 45 м"),
    "hubc_tug": (tug_parts, tug_hits, "Строительный буксир 12x8x14"),
    "hubc_clawship": (clawship_parts, clawship_hits, "Монтажный корабль с двумя захватами (точка захвата 0, 0, +32)"),
    "hubc_element_beam": (element_beam_parts, lambda: [hbox("hubc_element_beam", (0, 0, 0), (30.4, 3.6, 3.6))],
                          "Монтажный элемент: ферма 30 м"),
    "hubc_element_panel": (element_panel_parts, lambda: [hbox("hubc_element_panel", (0, 0, 0), (20.0, 12.0, 1.0))],
                           "Монтажный элемент: панель 20x12x1"),
    "hubc_pod": (pod_parts, lambda: [hcyl("hubc_pod", (0, -1.4, 0), (0, 1.4, 0), 1.4)], "Монтажная капсула на тросе ~3 м"),
    "hubc_jig": (jig_parts, jig_hits, "Монтажный кондуктор 60x60x80 с элементами"),
})

ASSETS = {k: v[0] for k, v in KIT.items()}
HITS = {k: v[1] for k, v in KIT.items()}
ROOTS = {k: "hub" for k in KIT}
PASSPORTS = {k: (v[2], "hub_module", "Строительные модули хаба (tools/models/units/hub_construction.py, docs/HUB_CONSTRUCTION_MODELS.md).")
             for k, v in KIT.items()}
