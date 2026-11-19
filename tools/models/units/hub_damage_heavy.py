"""Heavily damaged hub modules beyond the residential ones (author 2026-11-19: «большие сквозные повреждения здания (нужно
внутри изобразить этажи и интерьеры, если это промышленное здание - цеха с большими печами, станками, если салон
кораблей - какие-то запчасти для кораблей и поврежденные корпуса, погнутые и развороченные фермы»).

  hubb_works_dmg_breach     the 300 m works hall blown through across its width: the roof and both long walls open
                            over ~80 m, the workshop inside - furnaces under their stacks (one wrecked, its stack lying
                            in the hall), presses, lathes, CNC centres, mills in rows, overhead cranes (one fallen), the
                            roof trusses snapped and hanging, pipes torn, rubble and roof panels on the floor
  hubb_works_dmg_torn       one end of the hall collapsed (a third of its length): the roof and walls torn down along a
                            jagged diagonal, the machines there crushed under roof slabs, a crane down
  hubk_factory_dmg_breach   the district factory blown through front to back: furnace, machine tools, a gantry crane
                            inside, roof tanks toppled, a vent stack fallen
  hubb_ship_salon           (intact) a ship dealer's showroom: glass front, ships on turntables, a ship on a pylon
  hubb_ship_salon_dmg_breach the glass front smashed, a hole through the roof and the back wall; the display ships
                            damaged (one knocked off its turntable, one with its hull torn open to the ribs, one burnt),
                            engine nozzles on racks, hull panels, landing gear, crates, a spare engine, the mezzanine
                            broken; the ship on the pylon fallen
  hubv_mast_dmg_bent        the 450 m lattice mast buckled: bent over at two kinks, braces snapped and dangling
  hubb_girder_dmg_bent / _snapped, hubk_truss_dmg_bent / _snapped
                            the city frame girders and the district truss: buckled and twisted (sagging in the middle,
                            the chords bulging, the section twisted) / snapped (both halves hang from their ends, one or
                            two chords still bridging, the cut members ragged and curled, loose members dangling)
A damaged building has no power: no lit windows, no beacons, no floodlights. Halls: the shell is rebuilt as a panel
grid (outer and inner faces, the burnt ring, the torn rim's edges, petals of torn sheet bent out); the machines and
props are LOD0 only (part group <module>_furniture), the big structure (furnaces, cranes, trusses, presses, the
ships) LOD0/1, LOD2+ the shell, furnaces and ships as plain volumes. Hit volumes: the intact module's.
"""
import math
import random
import zlib

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import (BEACON, COOL, DARK, FLOOD, HAZ, HULL, LIGHT, STEEL, WARM, beacon, factory_hits,
                                factory_parts, truss_hits, FACTORY_VENTS)
from units.hub_belt import GRATE, RAD, WK_H, WK_L, WK_W, GIRDER_H, SEG_L, girder_hits, works_hits, works_parts
from units.hub_vista import RED, mast_hits
from units.hub_damage import (BURNT, REBAR, SLAB, SLAB_U, _add, _clip_gone, _facade, _hash01, _mul, _noise, _q)
from units.hub_damage_names import FRAME_KINDS, HEAVY_KINDS

register_materials({
    "eq_hub_torn_metal": ((0.46, 0.45, 0.43), (0.55, 0.55, 0.55), 140),    # bright torn sheet at a rim
    "eq_ind_inner": ((0.31, 0.32, 0.33), (0.08, 0.08, 0.08), 15),          # a hall's inner cladding
    "eq_ind_floor": ((0.27, 0.27, 0.26), (0.05, 0.05, 0.05), 10),
    "eq_ind_furnace": ((0.26, 0.16, 0.11), (0.05, 0.05, 0.05), 8),         # refractory shell, rust
    "eq_ind_machine": ((0.25, 0.33, 0.29), (0.25, 0.25, 0.25), 50),        # machine-tool green
    "eq_ind_machine_dark": ((0.13, 0.14, 0.15), (0.30, 0.30, 0.30), 60),
    "eq_salon_floor": ((0.56, 0.57, 0.59), (0.45, 0.45, 0.45), 90),
    "eq_salon_wall": ((0.62, 0.63, 0.66), (0.10, 0.10, 0.10), 20),
    "eq_ship_hull": ((0.72, 0.73, 0.75), (0.40, 0.40, 0.40), 80),
    "eq_ship_accent": ((0.62, 0.18, 0.12), (0.30, 0.30, 0.30), 60),
    "eq_ship_accent_b": ((0.15, 0.30, 0.55), (0.30, 0.30, 0.30), 60),
    "eq_ship_metal": ((0.40, 0.41, 0.43), (0.60, 0.60, 0.60), 120),
    "eq_crate": ((0.42, 0.40, 0.30), (0.05, 0.05, 0.05), 10),
})
TORN, INNER, FLOOR, FURNACE = "eq_hub_torn_metal", "eq_ind_inner", "eq_ind_floor", "eq_ind_furnace"
MACH, MACH_D, CRATE = "eq_ind_machine", "eq_ind_machine_dark", "eq_crate"
SHIP, SHIP_A, SHIP_B, SHIP_M = "eq_ship_hull", "eq_ship_accent", "eq_ship_accent_b", "eq_ship_metal"
FURN_BURNT = "eq_furn_burnt"
DEAD = {WARM, COOL, BEACON, FLOOD, "eq_hub_window_warm", "eq_hub_window_cool"}     # no power in a ruin
SHELL_T = 0.5


# ---------------------------------------------------------------- small vector helpers
def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _len(a):
    return math.sqrt(_dot(a, a))


def _unit(a):
    L = _len(a)
    return _mul(a, 1.0 / L) if L > 1e-9 else (0.0, 1.0, 0.0)


def _lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def _rot(axis, ang, v):
    """Rodrigues: v rotated by ang about the unit axis"""
    c, s = math.cos(ang), math.sin(ang)
    k = axis
    kv = _cross(k, v)
    kd = _dot(k, v)
    return tuple(v[i] * c + kv[i] * s + k[i] * kd * (1 - c) for i in range(3))


def _smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def _bar(part, mat, p0, p1, w, h=None):
    """a square / rectangular bar from p0 to p1"""
    d = _sub(p1, p0)
    L = _len(d)
    if L < 0.05:
        return
    box(part, mat, _lerp(p0, p1, 0.5), (w, h or w, L), axis_to(_unit(d)))


def _member(part, mat, p0, p1, r, shape, cs):
    if shape == "bar":
        _bar(part, mat, p0, p1, r)
    elif _len(_sub(p1, p0)) > 0.05:
        cyl(part, mat, p0, p1, r, cs)


def _merge(dst, src):
    base = len(dst.v)
    dst.v += src.v
    dst.n += src.n
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + base for i in f) for f in fl)


def _drop_aabb(part, lo, hi, mats=None):
    """remove every face (of mats, or of any material) lying wholly inside the box lo..hi"""
    for m in list(part.faces):
        if mats is not None and m not in mats:
            continue
        part.faces[m] = [f for f in part.faces[m] if not all(lo[a] - 0.01 <= part.v[i][a] <= hi[a] + 0.01
                                                             for i in f for a in range(3))]


def _dead(P):
    for part in P.values():
        for m in list(part.faces):
            if m in DEAD:
                del part.faces[m]


def _first_gone(gone, f, y0, y1, step=1.0):
    """the lowest y in y0..y1 where gone(f(y)) (y1 if none)"""
    y = y0
    while y <= y1:
        if gone(f(y)):
            return y
        y += step
    return y1


# ---------------------------------------------------------------- a hall's shell as a panel grid with its wound
class _Plane:
    """a flat rectangle as a cell grid with _Face's interface (hub_damage._facade): u -w/2..w/2, v 0..h, d inward"""

    def __init__(self, origin, U, V, n, width, height, cell):
        self.origin, self.U, self.V, self.n = origin, U, V, n
        self.width, self.height = width, height
        self.nu = max(1, int(round(width / cell)))
        self.nv = max(1, int(round(height / cell)))
        self.cw, self.ch = width / self.nu, height / self.nv

    def cell_u(self, i):
        return -self.width / 2 + i * self.cw

    def cell_v(self, j):
        return j * self.ch

    def p(self, u, v, d=0.0):
        return tuple(self.origin[a] + self.U[a] * u + self.V[a] * v - self.n[a] * d for a in range(3))

    def inner(self, t):
        q = _Plane(self.p(0.0, 0.0, t), self.U, self.V, _mul(self.n, -1.0), self.width, self.height, self.cw)
        q.nu, q.nv, q.cw, q.ch = self.nu, self.nv, self.cw, self.ch            # the same grid as the outer face
        return q


def _shell_planes(c, s, cell, sides=(0, 1, 2, 3), roof=True):
    hx, hy, hz = s[0] / 2, s[1] / 2, s[2] / 2
    y0 = c[1] - hy
    defs = {0: ((c[0] + hx, y0, c[2]), (0.0, 0.0, -1.0), (0.0, 1.0, 0.0), (1.0, 0.0, 0.0), s[2], s[1]),
            1: ((c[0] - hx, y0, c[2]), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (-1.0, 0.0, 0.0), s[2], s[1]),
            2: ((c[0], y0, c[2] + hz), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), s[0], s[1]),
            3: ((c[0], y0, c[2] - hz), (-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, -1.0), s[0], s[1])}
    out = [(fi, _Plane(*defs[fi], cell)) for fi in sides]
    if roof:
        out.append(("roof", _Plane((c[0], c[1] + hy, c[2] - hz), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0),
                                   s[0], s[2], cell)))
    return out


def _shell(part, key, c, s, gone, heat, lod, outer, inner, cell, sides=(0, 1, 2, 3), roof=True, extra_open=None,
           petal_mat=TORN):
    """the box c/s as a shell: panel cells open where gone, the burnt ring, the inner cladding, the rim's edges, torn
    petals bent outward (LOD0/1). extra_open(fi, i, j, p) opens more cells (shattered glass)."""
    for fi, pl in _shell_planes(c, s, cell, sides, roof):
        nu, nv = pl.nu, pl.nv
        opened = [[False] * nv for _ in range(nu)]
        burnt = [[False] * nv for _ in range(nu)]
        for i in range(nu):
            for j in range(nv):
                pt = pl.p(pl.cell_u(i) + pl.cw / 2, pl.cell_v(j) + pl.ch / 2, 0.3)
                if fi != "roof" and j == 0:
                    opened[i][j] = False                                     # the wall's foot stands
                else:
                    opened[i][j] = gone(pt) or bool(extra_open and extra_open(fi, i, j, pt))
                h = heat(pt)
                burnt[i][j] = h > 0.3 or (h > 0.05 and _hash01(key, fi, i, j, "soot") < 0.5)
        _facade(part, pl, outer, opened, burnt)
        _facade(part, pl.inner(SHELL_T), inner, opened, burnt)
        rng = random.Random(zlib.crc32(f"{key}:{fi}:rim".encode()))
        for i in range(nu):
            for j in range(nv):
                if opened[i][j]:
                    continue
                for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    a, b = i + di, j + dj
                    if not (0 <= a < nu and 0 <= b < nv) or not opened[a][b]:
                        continue
                    if di:
                        u = pl.cell_u(i + 1) if di > 0 else pl.cell_u(i)
                        e0, e1 = (u, pl.cell_v(j)), (u, pl.cell_v(j + 1))
                        dirv = _mul(pl.U, float(di))
                    else:
                        v = pl.cell_v(j + 1) if dj > 0 else pl.cell_v(j)
                        e0, e1 = (pl.cell_u(i), v), (pl.cell_u(i + 1), v)
                        dirv = _mul(pl.V, float(dj))
                    _q(part, BURNT, pl.p(*e0, 0.0), pl.p(*e1, 0.0), pl.p(*e1, SHELL_T), pl.p(*e0, SHELL_T), dirv)
                    if lod <= 1 and rng.random() < 0.3:
                        # a petal of torn sheet hinged on the rim, bent out of the wall plane
                        f0, f1 = rng.uniform(0.0, 0.3), rng.uniform(0.6, 1.0)
                        h0 = pl.p(e0[0] + (e1[0] - e0[0]) * f0, e0[1] + (e1[1] - e0[1]) * f0, 0.0)
                        h1 = pl.p(e0[0] + (e1[0] - e0[0]) * f1, e0[1] + (e1[1] - e0[1]) * f1, 0.0)
                        ang = math.radians(rng.uniform(25.0, 80.0)) * rng.choice((1.0, -1.0))
                        L = min(pl.cw, pl.ch) * rng.uniform(0.35, 0.8)
                        dv = _add(_mul(dirv, math.cos(ang) * L), _mul(pl.n, math.sin(ang) * L))
                        tip = _mul(_sub(h1, h0), rng.uniform(-0.25, 0.25))
                        p2, p3 = _add(_add(h1, dv), tip), _add(h0, dv)
                        nn = _unit(_cross(_sub(h1, h0), dv))
                        _q(part, petal_mat, h0, h1, p2, p3, nn)
                        _q(part, BURNT, h0, h1, p2, p3, _mul(nn, -1.0))


def _cut_members(part, members, gone, rng, lod, dangle=0.5, step=1.5):
    """members (mat, p0, p1, r, shape, cs): drawn where not gone; at a cut end a bent remnant hangs down (LOD0/1)"""
    for mat, p0, p1, r, shape, cs in members:
        L = _len(_sub(p1, p0))
        n = max(1, int(math.ceil(L / step)))
        ok = [not gone(_lerp(p0, p1, (k + 0.5) / n)) for k in range(n)]
        k = 0
        while k < n:
            if not ok[k]:
                k += 1
                continue
            k1 = k
            while k1 < n and ok[k1]:
                k1 += 1
            a, b = _lerp(p0, p1, k / n), _lerp(p0, p1, k1 / n)
            _member(part, mat, a, b, r, shape, cs)
            for end, cut, other in ((b, k1 < n, a), (a, k > 0, b)):
                if cut and lod <= 1 and rng.random() < dangle:
                    d = _unit(_sub(end, other))
                    dn = _unit(_add(_mul(d, rng.uniform(0.2, 0.6)), (rng.uniform(-0.3, 0.3), -1.0, rng.uniform(-0.3, 0.3))))
                    _member(part, BURNT, end, _add(end, _mul(dn, min(L, rng.uniform(1.5, 6.0)))), r * 0.9, shape, max(4, cs - 2))
            k = k1


def _warren(y0, y1, a0, a1, at, panels, along_x=True, chord=0.8, web=0.45):
    """a flat Warren truss in a vertical plane: chords y0 / y1 from a0 to a1, at = the other horizontal coordinate"""
    P = (lambda a, y: (a, y, at)) if along_x else (lambda a, y: (at, y, a))
    out = [(STEEL, P(a0, y0), P(a1, y0), chord, "bar", 4), (STEEL, P(a0, y1), P(a1, y1), chord, "bar", 4)]
    for k in range(panels + 1):
        a = a0 + (a1 - a0) * k / panels
        out.append((STEEL, P(a, y0), P(a, y1), web, "bar", 4))
        if k < panels:
            b = a0 + (a1 - a0) * (k + 1) / panels
            out.append((STEEL, P(a, y0 if k % 2 == 0 else y1), P(b, y1 if k % 2 == 0 else y0), web, "bar", 4))
    return out


def _rubble(part, rng, x0, x1, z0, z1, y, n, mats=(HULL, BURNT, SLAB, "eq_ind_inner")):
    for _ in range(n):
        kind = rng.random()
        c = (rng.uniform(x0, x1), y, rng.uniform(z0, z1))
        R = mat_mul(rot_y(rng.uniform(0, math.pi)), rot_x(rng.uniform(-0.6, 0.6)))
        if kind < 0.5:            # a roof / wall panel lying tilted
            w, d = rng.uniform(3.0, 7.0), rng.uniform(2.5, 5.0)
            box(part, rng.choice(mats), (c[0], y + rng.uniform(0.4, 1.6), c[2]), (w, 0.35, d), R)
        elif kind < 0.8:          # a heap of chunks
            for _k in range(3):
                s = rng.uniform(0.8, 2.4)
                box(part, rng.choice(mats), (c[0] + rng.uniform(-2, 2), y + s * 0.35, c[2] + rng.uniform(-2, 2)),
                    (s, s * 0.7, s * 1.2), mat_mul(rot_y(rng.uniform(0, 3)), rot_z(rng.uniform(-0.5, 0.5))))
        else:                     # a bent beam
            a = rng.uniform(0, 2 * math.pi)
            p0 = (c[0], y + 0.4, c[2])
            p1 = (c[0] + 5 * math.cos(a), y + rng.uniform(0.4, 3.0), c[2] + 5 * math.sin(a))
            p2 = (p1[0] + 4 * math.cos(a + 0.7), y + 0.4, p1[2] + 4 * math.sin(a + 0.7))
            _bar(part, STEEL, p0, p1, 0.6)
            _bar(part, BURNT, p1, p2, 0.6)


# ---------------------------------------------------------------- machines (LOD0 props; presses big enough for LOD1)
def _machine(furn, body, kind, x, z, yaw, burnt, tip, lod, rng):
    R = rot_y(yaw)
    if tip:
        R = mat_mul(R, rot_z(tip))

    def at(lx, ly, lz):
        q = apply(R, (lx, ly, lz))
        return (x + q[0], q[1], z + q[2])

    m = FURN_BURNT if burnt else MACH
    md = FURN_BURNT if burnt else MACH_D
    lift = 1.5 if tip else 0.0
    if kind == "press":                       # a big frame press, 20 m: in the body (seen from afar)
        if lod > 1:
            return
        tgt = body
        for sx in (-1, 1):
            box(tgt, m, at(sx * 4.5, 9.0, 0), (2.5, 18.0, 3.5), R)
        box(tgt, m, at(0, 19.5, 0), (12.0, 4.0, 4.5), R)                    # crown
        box(tgt, md, at(0, 1.5 + lift, 0), (11.0, 3.0, 6.0), R)              # bed
        if lod == 0:
            box(tgt, md, at(0, 12.0, 0), (7.0, 3.0, 4.0), R)                # ram
            box(tgt, STEEL, at(0, 3.4, 0), (6.0, 0.6, 4.0), R)              # die
        return
    if lod != 0:
        return
    if kind == "lathe":
        box(furn, m, at(0, 1.2, 0), (16.0, 1.4, 2.6), R)                    # bed
        for sx in (-6.5, 0.0, 6.5):
            box(furn, md, at(sx, 0.25, 0), (1.2, 0.5, 2.8), R)              # feet
        box(furn, m, at(-6.5, 3.6, 0), (3.2, 3.4, 3.2), R)                 # headstock
        box(furn, m, at(6.5, 2.9, 0), (2.0, 2.0, 2.2), R)                  # tailstock
        box(furn, md, at(1.0, 2.4, 0.4), (2.0, 1.0, 3.2), R)               # carriage
        q = apply(R, (1.0, 0.0, 0.0))
        c0 = at(-4.9, 3.6, 0)
        cyl(furn, STEEL, c0, _add(c0, _mul(q, 0.8)), 1.4, 10)               # chuck
        cyl(furn, md, _add(c0, _mul(q, 0.8)), _add(c0, _mul(q, 10.5)), 0.5, 8)   # the workpiece
    elif kind == "cnc":
        box(furn, md, at(0, 0.5, 0), (8.0, 1.0, 5.0), R)
        box(furn, m, at(0, 3.5, 0), (8.0, 5.0, 5.0), R)
        box(furn, DARK, at(0, 3.8, 2.53), (4.5, 2.4, 0.05), R)             # the dead window
        box(furn, md, at(4.6, 3.0, 2.0), (0.9, 1.6, 0.4), R)               # pendant
    elif kind == "mill":
        box(furn, md, at(0, 0.6, 0), (3.0, 1.2, 3.0), R)
        box(furn, m, at(0, 4.5, -0.8), (2.4, 7.0, 2.0), R)
        box(furn, m, at(0, 7.4, 1.0), (2.0, 2.0, 3.6), R)
        box(furn, md, at(0, 2.2, 1.2), (6.5, 0.8, 2.2), R)
        cyl(furn, STEEL, at(0, 6.4, 2.2), at(0, 4.6, 2.2), 0.35, 8)
    else:                                     # a stack of coils / billets on a pallet
        box(furn, md, at(0, 0.15, 0), (4.0, 0.3, 4.0), R)
        for k in range(3):
            c0 = at(-1.3 + 1.3 * k, 1.2, -1.5)
            cyl(furn, STEEL if not burnt else FURN_BURNT, c0, _add(c0, apply(R, (0, 0, 3.0))), 0.9, 10)


# ---------------------------------------------------------------- the works hall (hub_belt.works_parts)
def _works_plan(kind):
    rng = random.Random(zlib.crc32(f"hubb_works:{kind}:heavy".encode()))
    if kind == "breach":
        zc = rng.uniform(-60.0, 60.0)
        yc = WK_H * 0.72
        r0 = rng.uniform(38.0, 46.0)
        nz, nz2 = _noise(rng, 5), _noise(rng, 3)
        sgn = rng.choice((-1.0, 1.0))

        def rr(p):
            a = math.atan2(p[1] - yc, p[2] - zc)
            return r0 * (1.0 + 0.6 * nz(a)) * (1.0 + 0.12 * sgn * p[0] / (WK_W / 2) + 0.35 * nz2(p[0] * 0.05 + a))

        def d(p):
            return math.hypot(p[2] - zc, p[1] - yc)
        return (lambda p: p[1] > 7.0 and d(p) < rr(p)), (lambda p: max(0.0, 1.0 - (d(p) - rr(p)) / 14.0)), zc, rng
    # torn: one end collapsed along a jagged diagonal
    end = rng.choice((-1.0, 1.0))
    ph = [rng.uniform(0, 6.3) for _ in range(3)]

    def cut(x, z):
        f = (WK_L / 2 - end * z) / WK_L                          # 0 at the collapsed end .. 1 at the far one
        return WK_H * (0.10 + 2.6 * f + 0.25 * x / WK_W) + 6.0 * math.sin(z * 0.09 + ph[0]) + 4.0 * math.sin(x * 0.13 + ph[1])
    return ((lambda p: p[1] > 6.0 and p[1] > cut(p[0], p[2])),
            (lambda p: 0.7 * max(0.0, 1.0 - (cut(p[0], p[2]) - p[1]) / 16.0)), end * WK_L * 0.38, rng)


def _works_builder(kind):
    def fn(lod):
        gone, heat, zc, _ = _works_plan(kind)
        rng = random.Random(zlib.crc32(f"hubb_works:{kind}:inside:{lod}".encode()))
        P = works_parts(lod)
        _dead(P)
        w = P["hubb_works"]
        _drop_aabb(w, (-WK_W / 2, 0.0, -WK_L / 2), (WK_W / 2, WK_H, WK_L / 2), {HULL})          # the hall box
        _drop_aabb(w, (0.0, WK_H, -WK_L * 0.45), (WK_W / 2, WK_H + 20.0, WK_L * 0.45), {LIGHT})  # the upper hall
        _drop_aabb(w, (-20.0, WK_H, -WK_L / 2), (10.0, WK_H + 6.0, WK_L / 2), {STEEL})         # roof pipe rack
        furnaces = []
        for z in (-100.0, 0.0, 100.0):
            wrecked = gone((-40.0, WK_H - 8.0, z))
            furnaces.append((z, wrecked))
            if wrecked:
                _drop_aabb(w, (-55.0, WK_H - 1.0, z - 15.0), (-25.0, WK_H + 62.0, z + 15.0))
        # radiator wings hit by the blast hang off their roots
        for sx in (-1, 1):
            for k in range(5 if lod < 2 else 3):
                z = -120.0 + k * (240.0 / (4 if lod < 2 else 2))
                root = (sx * (WK_W / 2 + 4.0), WK_H * 0.6, z)
                if gone(_add(root, (-sx * 6.0, 0.0, 0.0))) or heat(root) > 0.8:
                    _drop_aabb(w, (min(sx * 70, sx * 154) - 1, WK_H * 0.6 - 3, z - 19), (max(sx * 70, sx * 154) + 1, WK_H * 0.6 + 3, z + 19))
                    a = math.radians(rng.uniform(25.0, 60.0))
                    L = rng.uniform(35.0, 70.0)
                    dvec = (sx * math.cos(a), -math.sin(a), 0.0)
                    hinge = (sx * (WK_W / 2 + 0.5), WK_H * 0.6, z)
                    box(w, BURNT if rng.random() < 0.5 else RAD, _add(hinge, _mul(dvec, L / 2)), (L, 0.8, 36.0 * rng.uniform(0.6, 0.95)),
                        mat_mul(rot_z(-sx * a), rot_x(rng.uniform(-0.15, 0.15))))
        _clip_gone(w, gone)
        furn = Part("hubb_works_furniture") if lod == 0 else None
        _shell(w, f"works:{kind}", (0.0, WK_H / 2, 0.0), (WK_W, WK_H, WK_L), gone, heat, lod, HULL, INNER, 10.0)
        _shell(w, f"works:{kind}:up", (WK_W / 4, WK_H + 10.0, 0.0), (WK_W / 2, 20.0, WK_L * 0.9), gone, heat, lod,
               LIGHT, INNER, 10.0)
        # the roof pipe rack, torn where the roof is
        if lod < 2:
            _cut_members(w, [(STEEL, (x, WK_H + 3.0, -WK_L / 2), (x, WK_H + 3.0, WK_L / 2), 1.5, "cyl", 8)
                             for x in (-10.0, -4.0, 2.0)], gone, rng, lod, step=3.0)
        _works_inside(w, furn, lod, gone, heat, zc, furnaces, kind, rng)
        if furn is not None and furn.faces:
            P[furn.name] = furn
        return P
    return fn


def _works_inside(w, furn, lod, gone, heat, zc, furnaces, kind, rng):
    hw, hl = WK_W / 2 - SHELL_T, WK_L / 2 - SHELL_T
    _q(w, FLOOR, (-hw, 0.05, -hl), (hw, 0.05, -hl), (hw, 0.05, hl), (-hw, 0.05, hl), (0.0, 1.0, 0.0))
    # wall columns and the roof trusses on them (span across x), snapped and hanging in the wound
    zs = [-WK_L / 2 + 12.5 + 25.0 * k for k in range(12)]
    for z in zs:
        for sx in (-1, 1):
            x = sx * (WK_W / 2 - 1.5)
            yt = _first_gone(gone, lambda y: (x, y, z), 0.0, WK_H, 2.0)
            box(w, STEEL, (x, yt / 2, z), (1.6, yt, 1.6))
            if yt < WK_H - 1.0 and lod <= 1:
                a = rng.uniform(0.4, 1.1)
                tip = (x - sx * math.sin(a) * 7.0, yt + math.cos(a) * 7.0 * rng.uniform(-0.5, 1.0), z + rng.uniform(-3, 3))
                _bar(w, BURNT, (x, yt, z), tip, 1.4)
        if lod <= 1:
            _cut_members(w, _warren(WK_H - 7.0, WK_H - 1.0, -WK_W / 2 + 1.5, WK_W / 2 - 1.5, z, 10 if lod == 0 else 5),
                         gone, rng, lod, dangle=0.6)
    # crane rails along the long walls, the bridges
    if lod <= 1:
        _cut_members(w, [(STEEL, (sx * (WK_W / 2 - 4.0), 54.0, -hl), (sx * (WK_W / 2 - 4.0), 54.0, hl), 1.8, "bar", 4)
                         for sx in (-1, 1)], gone, rng, lod, dangle=0.7, step=4.0)
    bridges = [-110.0, -20.0, 90.0]
    k = min(range(3), key=lambda i: abs(bridges[i] - zc))
    bridges[k] = zc + rng.uniform(-8.0, 8.0)
    for zb in bridges:
        if gone((0.0, 56.0, zb)) or gone((WK_W / 2 - 4.0, 54.0, zb)):
            side = rng.choice((-1.0, 1.0))                    # fallen: one end still on its rail, the other on the floor
            a, b = (side * (WK_W / 2 - 4.0), 54.0, zb), (-side * 20.0, 2.0, zb + rng.uniform(-12.0, 12.0))
            box(w, HAZ if rng.random() < 0.5 else BURNT, _lerp(a, b, 0.5), (3.0, 4.0, _len(_sub(b, a))), axis_to(_unit(_sub(b, a))))
            box(w, BURNT, _lerp(a, b, 0.6), (5.0, 4.0, 6.0), axis_to(_unit(_sub(b, a))))
        else:
            box(w, HAZ, (0.0, 57.5, zb), (WK_W - 6.0, 4.0, 3.0))
            tx = rng.uniform(-40.0, 40.0)
            box(w, HAZ, (tx, 60.5, zb), (6.0, 2.0, 5.0))
            if lod == 0:
                hd = rng.uniform(8.0, 30.0)
                cyl(w, STEEL, (tx, 55.5, zb), (tx, 55.5 - hd, zb), 0.15, 4)
                box(w, STEEL, (tx, 55.5 - hd - 0.8, zb), (1.2, 1.6, 0.8))
    # furnaces under the stacks (dead, no glow); a wrecked one burst, its stack lying in the hall
    seg = {0: 24, 1: 14, 2: 10}[lod]
    for z, wrecked in furnaces:
        x = -40.0
        if not wrecked:
            ylathe(w, FURNACE, [(0, 0.0), (15.0, 0.0), (15.0, 3.0), (13.0, 3.0), (13.0, 30.0), (9.0, 42.0), (8.0, 42.0),
                                (8.0, WK_H - 0.5), (0, WK_H - 0.5)], seg, t=(x, 0.0, z))
            if lod <= 1:
                for y in (8.0, 16.0, 24.0):
                    ylathe(w, STEEL, [(13.0, y - 0.4), (13.4, y - 0.4), (13.4, y + 0.4), (13.0, y + 0.4)], seg, t=(x, 0, z))
                for sx, sz in ((1, 0), (-1, 0), (0, 1), (0, -1)):              # the charging platform round it
                    box(w, GRATE, (x + sx * 16.5, 18.0, z + sz * 16.5), (34.0 if sz else 4.0, 0.6, 4.0 if sz else 34.0))
            if lod == 0:
                box(furn, FURNACE, (x + 16.0, 3.0, z), (4.0, 1.2, 2.0), rot_z(-0.2))           # tapping spout
                cyl(furn, FURNACE, (x + 22.0, 1.0, z), (x + 22.0, 5.5, z), 3.4, 14)             # a ladle on its car
                box(furn, MACH_D, (x + 22.0, 0.5, z), (7.0, 1.0, 7.0))
        else:
            hb = rng.uniform(10.0, 18.0)
            cyl(w, BURNT, (x, 0.0, z), (x + 2.0, hb, z + 1.5), 13.0, seg)                     # the burst shell
            for k in range(5 if lod <= 1 else 0):
                a = rng.uniform(0, 2 * math.pi)
                box(w, rng.choice((BURNT, FURNACE)), (x + math.cos(a) * 15.0, 1.5, z + math.sin(a) * 15.0),
                    (rng.uniform(3, 7), rng.uniform(1.5, 3.5), rng.uniform(3, 6)),
                    mat_mul(rot_y(a), rot_x(rng.uniform(-0.6, 0.6))))
            a = rng.uniform(-0.6, 0.6)
            p0 = (x + 10.0, 13.0, z)
            p1 = (x + 10.0 + 48.0 * math.cos(a), 13.0, z + 48.0 * math.sin(a))
            cyl(w, BURNT, p0, p1, 13.0, seg)                                                   # the fallen stack
            if lod <= 1:
                cyl(w, STEEL, p1, _add(p1, (6.0 * math.cos(a), 0.0, 6.0 * math.sin(a))), 8.0, seg)
    # machine-tool rows (x 5..60), presses among them
    kinds = ("lathe", "press", "cnc", "mill", "lathe", "coils", "cnc")
    for xi, x in enumerate((8.0, 30.0, 52.0)):
        for zi in range(13):
            z = -138.0 + 23.0 * zi
            kd = kinds[(xi * 5 + zi * 3) % len(kinds)]
            h = heat((x, 10.0, z))
            crushed = gone((x, 14.0, z)) if kind == "torn" else False
            if crushed:
                if lod <= 1:
                    box(w, BURNT, (x, 1.5, z), (rng.uniform(6, 12), 3.0, rng.uniform(4, 8)), rot_y(rng.uniform(0, 3)))
                continue
            tip = rng.uniform(0.3, 1.2) * rng.choice((-1, 1)) if (h > 0.6 and rng.random() < 0.35) else 0.0
            _machine(furn, w, kd, x, z, (xi + zi) % 2 * math.pi / 2, h > 0.45, tip, lod, rng)
    # pipes along the -x wall, torn in the wound
    if lod <= 1:
        _cut_members(w, [(STEEL, (-WK_W / 2 + 4.0, y, -hl), (-WK_W / 2 + 4.0, y, hl), 0.8, "cyl", 8 if lod == 0 else 6)
                         for y in (36.0, 39.0, 42.0)], gone, rng, lod, dangle=0.8, step=4.0)
    # rubble under the open roof
    if lod <= 1:
        for x in range(-63, 64, 14):
            for z in range(-147, 148, 14):
                if gone((float(x), WK_H - 1.0, float(z))) and _hash01("works", kind, x, z) < (0.55 if lod == 0 else 0.3):
                    _rubble(w, rng, x - 5, x + 5, z - 5, z + 5, 0.0, 1 if lod else 2)
    if kind == "torn" and lod <= 1:                       # big roof sections slid down into the collapsed end
        for k in range(4):
            z = zc + rng.uniform(-35.0, 35.0)
            sx = rng.choice((-1.0, 1.0))
            a, b = (sx * 60.0, rng.uniform(12.0, 25.0), z), (sx * rng.uniform(-10.0, 25.0), 1.0, z + rng.uniform(-8.0, 8.0))
            d = _unit(_sub(b, a))
            side = _unit(_cross(d, (0.0, 1.0, 0.0)))
            wdt = rng.uniform(12.0, 22.0)
            c0, c1 = _add(a, _mul(side, wdt / 2)), _add(a, _mul(side, -wdt / 2))
            nrm = _unit(_cross(side, d))
            if nrm[1] < 0:
                nrm = _mul(nrm, -1.0)
            _q(w, HULL, c0, c1, _add(c1, _sub(b, a)), _add(c0, _sub(b, a)), nrm)
            _q(w, BURNT, c0, c1, _add(c1, _sub(b, a)), _add(c0, _sub(b, a)), _mul(nrm, -1.0))


# ---------------------------------------------------------------- the district factory (hub_district.factory_parts)
F_C, F_S = (0.0, 14.0, 0.0), (70.0, 28.0, 45.0)


def _factory_plan():
    rng = random.Random(zlib.crc32(b"hubk_factory:breach:heavy"))
    xc, yc, r0 = rng.uniform(-12.0, 4.0), 21.0, rng.uniform(15.0, 17.0)
    nz = _noise(rng, 5)

    def rr(p):
        return r0 * (1.0 + 0.6 * nz(math.atan2(p[1] - yc, p[0] - xc)))

    def d(p):
        return math.hypot(p[0] - xc, p[1] - yc)
    return (lambda p: p[1] > 4.0 and d(p) < rr(p)), (lambda p: max(0.0, 1.0 - (d(p) - rr(p)) / 8.0)), xc


def _factory_builder(kind):
    def fn(lod):
        gone, heat, xc = _factory_plan()
        rng = random.Random(zlib.crc32(f"hubk_factory:{kind}:inside:{lod}".encode()))
        P = factory_parts(lod)
        _dead(P)
        f = P["hubk_factory"]
        _drop_aabb(f, (-35.0, 0.0, -22.5), (35.0, 28.0, 22.5), {HULL})
        seg = {0: 20, 1: 12, 2: 8}[lod]
        for x in (14.0, 24.0, 34.0):                       # roof process tanks: toppled where the roof went
            tx = x - 30.0
            if gone((tx, 26.0, 8.0)) or gone((tx, 30.0, 8.0)):
                _drop_aabb(f, (tx - 6.5, 27.0, 1.5), (tx + 6.5, 58.0, 14.5))
                a = rng.uniform(0.6, 1.2) * rng.choice((-1, 1))
                p0 = (tx, 3.0 if gone((tx, 24.0, 8.0)) else 28.5, 8.0 + rng.uniform(-4, 4))
                p1 = (tx + math.sin(a) * 26.0, max(6.0, p0[1] - 8.0), p0[2] + math.cos(a) * 12.0)
                cyl(f, BURNT, p0, p1, 6.0, seg)
        for (x, y, z) in FACTORY_VENTS:                     # vent stacks: fallen where their foot went
            if gone((x, 26.0, z)):
                _drop_aabb(f, (x - 2.6, 27.0, z - 2.6), (x + 2.6, y + 2.0, z + 2.6))
                a = rng.uniform(0, 2 * math.pi)
                cyl(f, BURNT, (x, 1.0, z), (x + math.cos(a) * 30.0, 2.5, z + math.sin(a) * 20.0), 2.2, seg)
        _clip_gone(f, gone)
        _shell(f, "factory", F_C, F_S, gone, heat, lod, HULL, INNER, 4.0)
        furn = Part("hubk_factory_furniture") if lod == 0 else None
        hw, hd = 35.0 - SHELL_T, 22.5 - SHELL_T
        _q(f, FLOOR, (-hw, 0.05, -hd), (hw, 0.05, -hd), (hw, 0.05, hd), (-hw, 0.05, hd), (0.0, 1.0, 0.0))
        for x in (-30.0, -20.0, -10.0, 0.0, 10.0, 20.0, 30.0):     # columns + roof trusses spanning z
            for sz in (-1, 1):
                z = sz * 21.0
                yt = _first_gone(gone, lambda y: (x, y, z), 0.0, 28.0, 1.0)
                box(f, STEEL, (x, yt / 2, z), (0.8, yt, 0.8))
            if lod <= 1:
                _cut_members(f, _warren(23.5, 27.3, -21.5, 21.5, x, 8 if lod == 0 else 4, along_x=False, chord=0.5, web=0.3),
                             gone, rng, lod, dangle=0.6, step=1.0)
        # a melting furnace under the stacks
        if lod <= 1:
            ylathe(f, FURNACE, [(0, 0.0), (5.5, 0.0), (5.5, 1.2), (4.5, 1.2), (4.5, 11.0), (2.5, 15.0), (2.2, 15.0),
                                (2.2, 27.5), (0, 27.5)], seg, t=(-24.0, 0.0, -10.0))
            box(f, GRATE, (-24.0, 8.0, -3.5), (12.0, 0.4, 3.0))
        # gantry crane rails along x, the bridge across
        if lod <= 1:
            _cut_members(f, [(STEEL, (-34.0, 20.0, sz * 19.5), (34.0, 20.0, sz * 19.5), 0.9, "bar", 4) for sz in (-1, 1)],
                         gone, rng, lod, dangle=0.7, step=2.0)
            xb = xc + rng.uniform(-4.0, 4.0)
            a, b = (xb, 20.0, -19.5), (xb + rng.uniform(-6, 6), 1.0, 12.0)
            box(f, HAZ, _lerp(a, b, 0.5), (1.6, 2.0, _len(_sub(b, a))), axis_to(_unit(_sub(b, a))))       # fallen
            box(f, HAZ, (22.0, 21.0, 0.0), (1.6, 2.0, 39.0))                                            # standing
        # machine tools
        spots = [(-8.0, -12.0), (-8.0, 2.0), (6.0, -12.0), (6.0, 2.0), (20.0, -12.0), (20.0, 2.0), (28.0, 14.0), (-28.0, 12.0)]
        kinds = ("lathe", "cnc", "press", "mill", "lathe", "cnc", "coils", "mill")
        for (x, z), kd in zip(spots, kinds):
            h = heat((x, 8.0, z))
            tip = rng.uniform(0.3, 1.0) * rng.choice((-1, 1)) if (h > 0.6 and rng.random() < 0.4) else 0.0
            if kd == "press":
                if lod <= 1:                                   # a smaller press (12 m) for the low hall
                    m = FURN_BURNT if h > 0.45 else MACH
                    for sx in (-1, 1):
                        box(f, m, (x + sx * 3.0, 6.0, z), (1.6, 12.0, 2.4))
                    box(f, m, (x, 12.5, z), (8.0, 2.5, 3.0))
                    box(f, MACH_D, (x, 1.0, z), (7.0, 2.0, 4.0))
                continue
            _machine(furn, f, kd, x, z, 0.0 if (x + z) % 2 else math.pi / 2, h > 0.45, tip, lod, rng)
        if lod <= 1:
            _cut_members(f, [(STEEL, (-34.0, y, -21.5), (34.0, y, -21.5), 0.5, "cyl", 8) for y in (16.0, 17.5)],
                         gone, rng, lod, dangle=0.8, step=1.5)
            for x in range(-30, 31, 8):
                if gone((float(x), 27.0, 0.0)):
                    _rubble(f, rng, x - 3, x + 3, -15, 15, 0.0, 3 if lod == 0 else 1)
        if furn is not None and furn.faces:
            P[furn.name] = furn
        return P
    return fn


# ---------------------------------------------------------------- ships (the salon's display pieces)
def _ship_local(lod, state, rng, hull=SHIP, accent=SHIP_A):
    """a ~26 m light ship in its own frame: nose +Z, up +Y. state: intact | burnt | torn (the rear hull torn off to
    its ribs, a wing and an engine pod gone) | wingless"""
    p = Part("ship")
    burnt = state == "burnt"
    hm = BURNT if burnt else hull
    am = BURNT if burnt else accent
    seg = {0: 16, 1: 10, 2: 6}[lod]
    if lod >= 2:
        box(p, hm, (0, 0, 13.0), (5.6, 5.0, 26.0))
        box(p, am, (0, -0.5, 9.0), (20.0, 0.5, 8.0))
        return p
    prof = [(0, 0.0), (2.4, 0.0), (3.0, 6.0), (3.1, 16.0), (2.8, 21.0), (1.6, 24.2), (0, 26.0)]
    if state == "torn":
        prof = [(0, 11.0), (3.05, 11.0), (3.1, 16.0), (2.8, 21.0), (1.6, 24.2), (0, 26.0)]
        lathe(p, BURNT, [(2.6, 11.0), (3.15, 11.0), (3.15, 11.8), (2.6, 11.8), (2.6, 11.0)], seg)   # the torn rim
        for z in (2.0, 5.0, 8.0):                                                                  # bare ribs
            lathe(p, SHIP_M, [(2.6, z - 0.2), (3.0, z - 0.2), (3.0, z + 0.2), (2.6, z + 0.2), (2.6, z - 0.2)], seg)
        for a in range(6):                                                                          # stringers
            ang = a * math.pi / 3 + 0.3
            x, y = 2.85 * math.cos(ang), 2.85 * math.sin(ang)
            if rng.random() < 0.8:
                _bar(p, SHIP_M, (x, y, rng.uniform(0.5, 3.0)), (x, y, 11.0), 0.18)
        box(p, SHIP_M, (0, -1.0, 6.0), (2.4, 2.0, 9.0))                                            # exposed machinery
        cyl(p, BURNT, (0.8, 0.5, 2.0), (0.8, 0.5, 10.0), 0.9, 8)
    lathe(p, hm, prof, seg)
    box(p, DARK, (0, 2.5, 19.5), (2.2, 1.3, 4.5), rot_x(0.12))                                    # canopy
    for sx in (-1, 1):
        if state in ("torn", "wingless") and sx == 1:
            continue
        box(p, am, (sx * 6.8, -0.6, 9.0), (9.0, 0.5, 7.0), rot_y(sx * 0.35))                     # swept wing
        if state == "torn":
            continue
        cyl(p, hm, (sx * 3.7, -0.6, -1.0), (sx * 3.7, -0.6, 8.0), 1.3, seg)                      # engine pod
        lathe(p, SHIP_M, [(1.0, 0.0), (1.25, 0.6), (1.7, 2.2)], seg, axis_to((0.0, 0.0, -1.0)), (sx * 3.7, -0.6, -1.0))
    box(p, am, (0, 4.2, 3.5), (0.4, 4.0, 5.5), rot_x(-0.35))                                      # fin
    if lod == 0 and state != "torn":
        for sz in (4.0, 20.0):                                                                    # landing gear down
            cyl(p, SHIP_M, (0, -2.6, sz), (0, -4.6, sz), 0.25, 6)
            cyl(p, SHIP_M, (0, -4.6, sz - 0.5), (0, -4.6, sz + 0.5), 0.45, 8)
    return p


def _place(dst, src, pos, yaw=0.0, pitch=0.0, roll=0.0):
    _merge(dst, placed(src, mat_mul(rot_y(yaw), mat_mul(rot_x(pitch), rot_z(roll))), pos))


def _nozzle(part, mat, base, direction, r0, r1, L, seg):
    lathe(part, mat, [(r0, 0.0), (r0 * 1.15, L * 0.15), (r1, L)], seg, axis_to(_unit(direction)), base)


# ---------------------------------------------------------------- the ship salon (intact + damaged)
SAL_W, SAL_H, SAL_D = 150.0, 34.0, 90.0
SAL_C, SAL_S = (0.0, SAL_H / 2, 0.0), (SAL_W, SAL_H, SAL_D)
PYLON = (45.0, 0.0, 66.0)


def ship_salon_parts(lod):
    P = {"hubb_ship_salon": Part("hubb_ship_salon")}
    s = P["hubb_ship_salon"]
    box(s, STEEL, (0, -2.0, 15.0), (SAL_W + 20.0, 4.0, SAL_D + 50.0))                       # the plinth with a forecourt
    box(s, "eq_salon_wall", SAL_C, SAL_S)                                                    # the hall
    box(s, COOL, (0, 16.0, SAL_D / 2 + 0.2), (SAL_W - 4.0, 30.0, 0.4))                      # the lit glass front
    box(s, HULL, (0, SAL_H + 1.0, 0), (SAL_W + 4.0, 2.0, SAL_D + 4.0))                       # roof cornice
    if lod < 2:
        for k in range(13):
            box(s, STEEL, (-SAL_W / 2 + 3.0 + k * (SAL_W - 6.0) / 12, 16.0, SAL_D / 2 + 0.5), (0.6, 30.0, 0.6))   # mullions
        for y in (11.0, 21.0):
            box(s, STEEL, (0, y, SAL_D / 2 + 0.5), (SAL_W - 4.0, 0.5, 0.6))
    box(s, STEEL, (0, SAL_H + 5.0, SAL_D / 2 - 2.0), (64.0, 6.0, 1.0))                      # the sign frame
    box(s, WARM, (0, SAL_H + 5.0, SAL_D / 2 - 1.4), (60.0, 5.0, 0.4))                       # the sign
    cyl(s, STEEL, PYLON, _add(PYLON, (0, 16.0, 0)), 1.5, {0: 16, 1: 10, 2: 6}[lod])         # the display pylon
    box(s, STEEL, _add(PYLON, (0, 16.5, 0)), (4.0, 1.0, 4.0))                              # the cradle
    yaw = 0.5
    _place(s, _ship_local(lod, "intact", random.Random(3)),
           _add(PYLON, (-13.0 * math.sin(yaw), 21.6, -13.0 * math.cos(yaw))), yaw=yaw)       # centred on the pylon
    if lod < 3:
        beacon(s, (SAL_W / 2, SAL_H + 2.0, SAL_D / 2), 1.2)
    return P


def ship_salon_hits():
    return [hbox("hubb_ship_salon", (0, -2.0, 15.0), (SAL_W + 20.0, 4.0, SAL_D + 50.0)),
            hbox("hubb_ship_salon", SAL_C, SAL_S),
            hcyl("hubb_ship_salon", PYLON, _add(PYLON, (0, 16.0, 0)), 1.6),
            hbox("hubb_ship_salon", _add(PYLON, (0, 20.0, 0)), (26.0, 8.0, 26.0))]


def _salon_plan():
    rng = random.Random(zlib.crc32(b"hubb_ship_salon:breach:heavy"))
    xc, yc, r0 = rng.uniform(-20.0, 20.0), 23.0, rng.uniform(19.0, 22.0)
    nz = _noise(rng, 5)

    def rr(p):
        return r0 * (1.0 + 0.6 * nz(math.atan2(p[1] - yc, (p[0] - xc) / 2.2))) * (1.0 + 0.1 * p[2] / SAL_D)

    def d(p):
        return math.hypot((p[0] - xc) / 2.2, p[1] - yc)                 # a wide hole: most of the showroom open
    return (lambda p: p[1] > 3.0 and d(p) < rr(p)), (lambda p: max(0.0, 1.0 - (d(p) - rr(p)) / 12.0)), xc


def _salon_builder(kind):
    def fn(lod):
        gone, heat, xc = _salon_plan()
        rng = random.Random(zlib.crc32(f"hubb_ship_salon:{kind}:inside:{lod}".encode()))
        P = {"hubb_ship_salon": Part("hubb_ship_salon")}
        s = P["hubb_ship_salon"]
        seg = {0: 16, 1: 10, 2: 6}[lod]
        box(s, STEEL, (0, -2.0, 15.0), (SAL_W + 20.0, 4.0, SAL_D + 50.0))
        # the glass front: most panes shattered (dead glass), the hole through
        shatter = lambda fi, i, j, p: fi == 2 and _hash01("salon", i, j) < 0.25 + 0.7 * heat(p)
        _shell(s, "salon", SAL_C, SAL_S, gone, heat, lod, "eq_salon_wall", "eq_salon_wall", 6.0, sides=(0, 1, 3))
        _shell(s, "salon:glass", SAL_C, SAL_S, gone, heat, lod, DARK, DARK, 6.0, sides=(2,), roof=False,
               extra_open=shatter, petal_mat=DARK)
        if lod < 2:                                                           # the mullion frame, bent where hit
            mull = [(STEEL, (-SAL_W / 2 + 3.0 + k * (SAL_W - 6.0) / 12, 0.5, SAL_D / 2 + 0.5),
                     (-SAL_W / 2 + 3.0 + k * (SAL_W - 6.0) / 12, 31.0, SAL_D / 2 + 0.5), 0.6, "bar", 4) for k in range(13)]
            mull += [(STEEL, (-SAL_W / 2 + 2.0, y, SAL_D / 2 + 0.5), (SAL_W / 2 - 2.0, y, SAL_D / 2 + 0.5), 0.5, "bar", 4)
                     for y in (11.0, 21.0)]
            _cut_members(s, mull, gone, rng, lod, dangle=0.8, step=1.5)
        # the roof cornice where the roof stands, the dead sign hanging askew
        _cut_members(s, [(HULL, (-SAL_W / 2 - 2.0, SAL_H + 1.0, z), (SAL_W / 2 + 2.0, SAL_H + 1.0, z), 2.0, "bar", 4)
                         for z in (-SAL_D / 2 - 1.0, SAL_D / 2 + 1.0)], gone, rng, lod, dangle=0.0, step=3.0)
        box(s, DARK, (-10.0, 3.0, SAL_D / 2 + 6.0), (60.0, 5.0, 0.4), mat_mul(rot_y(0.15), rot_x(1.15)))   # the fallen sign
        # the pylon bent, its ship fallen onto the forecourt, burnt
        cyl(s, STEEL, PYLON, _add(PYLON, (0, 9.0, 0)), 1.5, seg)
        cyl(s, BURNT, _add(PYLON, (0, 9.0, 0)), _add(PYLON, (5.0, 13.0, 3.0)), 1.4, seg)
        _place(s, _ship_local(lod, "burnt", rng), (PYLON[0] - 14.0, 3.6, PYLON[2] - 2.0), yaw=2.2, pitch=0.08, roll=0.45)
        hw, hd = SAL_W / 2 - SHELL_T, SAL_D / 2 - SHELL_T
        _q(s, "eq_salon_floor", (-hw, 0.05, -hd), (hw, 0.05, -hd), (hw, 0.05, hd), (-hw, 0.05, hd), (0.0, 1.0, 0.0))
        for k in range(6 if lod <= 1 else 0):                                 # soot on the floor under the hole
            c = (xc + rng.uniform(-20, 20), 0.08, rng.uniform(-35, 40))
            r = rng.uniform(3.0, 8.0)
            _q(s, BURNT, (c[0] - r, 0.08 + k * 0.004, c[2] - r), (c[0] + r, 0.08 + k * 0.004, c[2] - r),
               (c[0] + r, 0.08 + k * 0.004, c[2] + r), (c[0] - r, 0.08 + k * 0.004, c[2] + r), (0.0, 1.0, 0.0))
        # the roof trusses (spanning z), snapped in the hole
        if lod <= 1:
            for x in range(-60, 61, 20):
                _cut_members(s, _warren(SAL_H - 4.0, SAL_H - 0.8, -hd, hd, float(x), 10 if lod == 0 else 5, along_x=False,
                                        chord=0.6, web=0.35), gone, rng, lod, dangle=0.6, step=1.5)
        # the turntables and their ships: the one nearest the hole torn open, one knocked off, one burnt
        tables = [(-48.0, -6.0), (0.0, -14.0), (48.0, -6.0)]
        order = sorted(range(3), key=lambda i: abs(tables[i][0] - xc))
        states = {order[0]: "torn", order[1]: "fallen", order[2]: "burnt"}
        for ti, (x, z) in enumerate(tables):
            ylathe(s, SHIP_M, [(0, 0.0), (11.0, 0.0), (11.0, 1.0), (0, 1.0)], seg * 2, t=(x, 0, z))
            st = states[ti]
            if st == "fallen":                       # knocked off: nose down on the floor, the right wing broken off
                _place(s, _ship_local(lod, "wingless", rng, accent=SHIP_B), (x + 9.0, 5.0, z + 6.0), yaw=2.0, pitch=0.35, roll=0.5)
                box(s, SHIP_B, (x - 4.0, 0.6, z + 12.0), (9.0, 0.5, 7.0), mat_mul(rot_y(1.1), rot_z(0.15)))
            elif st == "torn":                        # on its table, the rear torn off to the ribs, a pod lying by it
                _place(s, _ship_local(lod, "torn", rng), (x, 5.8, z + 2.0), yaw=0.4, roll=0.08)
                cyl(s, BURNT, (x + 10.0, 1.4, z + 10.0), (x + 15.0, 1.4, z + 17.0), 1.3, seg)
                for k in range(4 if lod <= 1 else 0):
                    box(s, rng.choice((SHIP, BURNT)), (x + rng.uniform(-14, 14), 0.4, z + rng.uniform(-14, 14)),
                        (rng.uniform(1.5, 4.0), 0.25, rng.uniform(1.0, 3.0)), rot_y(rng.uniform(0, 3)))
            else:
                _place(s, _ship_local(lod, "burnt", rng, accent=SHIP_A), (x, 5.8, z), yaw=-0.6)
        # the mezzanine along the back wall: deck segments, broken in the hole
        if lod <= 1:
            for k in range(15):
                x0 = -hw + k * (2 * hw / 15)
                x1 = x0 + 2 * hw / 15
                mid = ((x0 + x1) / 2, 10.0, -hd + 5.0)
                if gone(mid):
                    continue
                broken = gone((x0 - 2.0, 10.0, mid[2])) or gone((x1 + 2.0, 10.0, mid[2]))
                R = rot_z(rng.uniform(-0.35, 0.35)) if broken else I3
                box(s, GRATE, (mid[0], 10.0 - (1.5 if broken else 0.0), mid[2]), (x1 - x0 - 0.1, 0.5, 10.0), R)
                if not broken:
                    box(s, STEEL, (x0 + 0.5, 5.0, -hd + 9.5), (0.6, 10.0, 0.6))
                    if lod == 0:
                        _bar(s, STEEL, (x0, 11.1, -hd + 9.8), (x1, 11.1, -hd + 9.8), 0.12)
        if lod == 0:
            _salon_props(P, s, gone, heat, rng, seg)
        return P
    return fn


def _salon_props(P, s, gone, heat, rng, seg):
    """LOD0: the spare parts - nozzle racks, hull panels, landing gear, crates, a spare engine"""
    furn = P.setdefault("hubb_ship_salon_furniture", Part("hubb_ship_salon_furniture"))
    hw, hd = SAL_W / 2 - SHELL_T, SAL_D / 2 - SHELL_T

    def m(base, p):
        return FURN_BURNT if heat(p) > 0.5 and rng.random() < 0.7 else base
    # engine nozzles on cradles along the -x wall
    for k in range(5):
        z = -30.0 + k * 11.0
        p = (-hw + 5.0, 2.6, z)
        box(furn, STEEL, (p[0], 0.6, z), (3.0, 1.2, 4.0))
        if rng.random() < 0.25:                                           # rolled off its cradle
            _nozzle(furn, m(SHIP_M, p), (p[0] + 5.0, 2.4, z), (rng.uniform(-1, 1), 0.0, 1.0), 1.2, 2.4, 4.5, seg)
        else:
            _nozzle(furn, m(SHIP_M, p), (p[0], 2.6, z - 2.0), (0.0, 0.0, 1.0), 1.2, 2.4, 4.5, seg)
    # hull panels leaning on the back wall (under the mezzanine), some fallen flat
    for k in range(9):
        x = -60.0 + k * 6.5
        p = (x, 1.8, -hd + 0.9)
        if rng.random() < 0.3:
            box(furn, m(SHIP, p), (x, 0.2, -hd + 4.0), (5.5, 0.25, 3.4), rot_y(rng.uniform(-0.4, 0.4)))
        else:
            box(furn, m(SHIP if k % 3 else SHIP_A, p), p, (5.5, 3.4, 0.25), rot_x(0.25))
    # landing gear sets standing (some knocked over)
    for k in range(6):
        x, z = 14.0 + (k % 3) * 6.0, 22.0 + (k // 3) * 7.0
        p = (x, 2.0, z)
        mm = m(SHIP_M, p)
        if rng.random() < 0.35:
            a = rng.uniform(0, 2 * math.pi)
            cyl(furn, mm, (x, 0.5, z), (x + 4.5 * math.cos(a), 0.5, z + 4.5 * math.sin(a)), 0.35, 8)
            continue
        cyl(furn, mm, (x, 0.3, z), (x, 4.8, z), 0.32, 8)
        cyl(furn, STEEL, (x, 0.6, z), (x, 2.2, z), 0.5, 8)
        cyl(furn, MACH_D, (x, 0.0, z), (x, 0.3, z), 1.1, 10)
        cyl(furn, mm, (x, 3.6, z), (x + 1.8, 4.6, z), 0.18, 6)
    # crate stacks by the +x wall, scattered near the hole
    for k in range(10):
        x, z = hw - 3.0 - (k % 2) * 3.0, -32.0 + (k // 2) * 4.0
        for h in range(1 + (k * 7) % 3):
            box(furn, m(CRATE, (x, h * 2.4, z)), (x, 1.2 + h * 2.4, z), (2.4, 2.4, 2.4), rot_y(rng.uniform(-0.1, 0.1)))
    for k in range(8):
        c = (rng.uniform(-50, 50), 1.2, rng.uniform(5, 40))
        if heat(c) > 0.3:
            box(furn, m(CRATE, c), c, (2.4, 2.4, 2.4), mat_mul(rot_y(rng.uniform(0, 3)), rot_x(rng.uniform(-0.4, 0.4))))
    # a spare engine on its stand
    e0 = (-30.0, 3.4, 26.0)
    for dx in (-2.5, 2.5):
        box(furn, STEEL, (e0[0] + dx, 1.0, e0[2]), (1.0, 2.0, 5.0))
    cyl(furn, m(SHIP_M, e0), (e0[0] - 5.0, e0[1], e0[2]), (e0[0] + 4.0, e0[1], e0[2]), 2.6, seg)
    _nozzle(furn, SHIP_M, (e0[0] + 4.0, e0[1], e0[2]), (1.0, 0.0, 0.0), 1.8, 2.9, 3.5, seg)


# ---------------------------------------------------------------- lattices: members, a deformation field, pieces
def _pieces(part, members, F, keep, rng, lod, piece, curl=None, dangle_zone=None):
    """each member (mat, p0, p1, r, shape, cs) split into pieces of <= piece m, every end moved by F; keep(p, member
    index) False drops a piece (a snapped gap); a piece next to a dropped one curls; in dangle_zone(p) some braces
    hang loose from one end"""
    for mi, (mat, p0, p1, r, shape, cs) in enumerate(members):
        L = _len(_sub(p1, p0))
        n = max(1, int(math.ceil(L / piece)))
        pts = [_lerp(p0, p1, k / n) for k in range(n + 1)]
        ok = [keep(_lerp(pts[k], pts[k + 1], 0.5), mi) for k in range(n)]
        if shape == "cyl" and dangle_zone and n <= 3 and all(ok) and dangle_zone(_lerp(p0, p1, 0.5)) and rng.random() < 0.25:
            a = F(p0) if rng.random() < 0.5 else F(p1)
            d = _unit((rng.uniform(-0.4, 0.4), -1.0, rng.uniform(-0.4, 0.4)))
            _member(part, BURNT, a, _add(a, _mul(d, L * rng.uniform(0.5, 1.0))), r, shape, cs)
            continue
        if shape == "cyl" and dangle_zone and dangle_zone(_lerp(p0, p1, 0.5)) and rng.random() < 0.15:
            continue                                                       # a brace blown away
        for k in range(n):
            if not ok[k]:
                continue
            a, b = F(pts[k]), F(pts[k + 1])
            if curl and k + 1 < n and not ok[k + 1]:
                b = _add(b, curl(rng))
            if curl and k > 0 and not ok[k - 1]:
                a = _add(a, curl(rng))
            _member(part, mat, a, b, r, shape, cs)


def _dplate(part, mat, c, s, F, n):
    """a plate (box c/s, long along z) cut in n slices, its corners moved by F"""
    for k in range(n):
        z0 = c[2] - s[2] / 2 + s[2] * k / n
        z1 = z0 + s[2] / n
        q = [F((c[0] + sx * s[0] / 2, c[1] + sy * s[1] / 2, z)) for z in (z0, z1) for sy in (-1, 1) for sx in (-1, 1)]
        # q index: z0: (-,-) 0, (+,-) 1, (-,+) 2, (+,+) 3; z1: 4..7
        mid = tuple(sum(v[a] for v in q) / 8.0 for a in range(3))
        for idx in ((2, 3, 7, 6), (0, 4, 5, 1), (0, 2, 6, 4), (1, 5, 7, 3)):
            pts = [q[i] for i in idx]
            nn = _unit(_cross(_sub(pts[1], pts[0]), _sub(pts[2], pts[0])))
            fc = tuple(sum(v[a] for v in pts) / 4.0 for a in range(3))
            if _dot(nn, _sub(fc, mid)) < 0:
                nn = _mul(nn, -1.0)                                   # outward
            _q(part, mat, *pts, nn)


def _girder_members(lod):
    h = GIRDER_H
    cs = {0: 8, 1: 6}.get(lod, 4)
    out = [(STEEL, (sx * h, sy * h, 0.0), (sx * h, sy * h, SEG_L), 2.4, "bar", 4) for sx in (-1, 1) for sy in (-1, 1)]
    bays = 4 if lod < 2 else 2
    sq = (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h)))
    for k in range(bays + 1):
        z = SEG_L * k / bays
        out += [(STEEL, (a[0], a[1], z), (b[0], b[1], z), 0.9, "cyl", cs) for a, b in sq]
        if k < bays:
            z1 = SEG_L * (k + 1) / bays
            out += [(STEEL, (a[0], a[1], z), (b[0], b[1], z1) if k % 2 == 0 else (a[0], a[1], z1), 0.6, "cyl", cs) for a, b in sq]
    return out


def _truss_members(lod):
    h, L = 4.0, 100.0
    bays = {0: 10, 1: 5, 2: 2}[lod]
    cs = {0: 8, 1: 6, 2: 4}[lod]
    sq = (((-h, -h), (h, -h)), ((h, -h), (h, h)), ((h, h), (-h, h)), ((-h, h), (-h, -h)))
    out = [(STEEL, (sx * h, sy * h, 0.0), (sx * h, sy * h, L), 0.45, "cyl", cs) for sx in (-1, 1) for sy in (-1, 1)]
    for k in range(bays + 1):
        z = L * k / bays
        out += [(STEEL, (a[0], a[1], z), (b[0], b[1], z), 0.3, "cyl", 6 if lod < 2 else 4) for a, b in sq]
        if k < bays and lod < 2:
            z1 = L * (k + 1) / bays
            out += [(STEEL, (a[0], a[1], z), (b[0], b[1], z1), 0.22, "cyl", 6 if lod == 0 else 4) for a, b in sq]
    return out


def _frame_builder(module, kind):
    """a girder / truss along +Z (0..L), its ends where the neighbours join"""
    girder = module == "hubb_girder"
    h, L = (GIRDER_H, SEG_L) if girder else (4.0, 100.0)

    def fn(lod):
        rng = random.Random(zlib.crc32(f"{module}:{kind}:frame".encode()))
        members = _girder_members(lod) if girder else _truss_members(lod)
        piece = {0: 2.5, 1: 5.0, 2: 12.5}[lod] * (2.0 if girder else 1.0)
        sk = rng.uniform(0.38, 0.62) * L
        P = {module: Part(module)}
        p = P[module]
        if kind == "bent":
            # buckled: the middle sags (ends fixed), sideways too, the section twisted; the chords bulge near the kink
            D = rng.uniform(0.12, 0.2) * L
            lat = rng.uniform(-0.5, 0.5) * D
            tw = rng.uniform(0.25, 0.5) * rng.choice((-1, 1))
            w = 1.6 * h

            def f(z):
                return (z / sk) ** 1.6 if z < sk else ((L - z) / (L - sk)) ** 1.6

            def F(q):
                z = q[2]
                g = max(0.0, min(1.0, f(max(0.0, min(L, z)))))
                a = tw * g
                x, y = q[0] * math.cos(a) - q[1] * math.sin(a), q[0] * math.sin(a) + q[1] * math.cos(a)
                bump = max(0.0, 1.0 - abs(z - sk) / w)
                bulge = 0.3 * bump
                x, y = x * (1.0 + bulge), y * (1.0 + bulge * (1.6 if q[1] > 0 else 0.4))
                return (x + lat * g, y - D * g, z - 0.08 * D * bump * (1 if q[1] > 0 else -1))
            keep = lambda q, mi: True
            curl = None
            zone = lambda q: abs(q[2] - sk) < 2.2 * h
        else:
            # snapped: both halves hang from their ends; one or two chords still bridge the gap
            g = rng.uniform(1.0, 2.0) * (h * 0.6 if girder else 2.0)
            t1, t2 = math.radians(rng.uniform(10.0, 22.0)), math.radians(rng.uniform(14.0, 30.0))
            side = rng.uniform(-0.15, 0.15)
            bridging = set(rng.sample(range(4), rng.choice((1, 2))))
            cut_off = {mi: rng.uniform(-g * 0.8, g * 0.8) for mi in range(len(members))}
            hinge0, hinge1 = (0.0, h, 0.0), (0.0, h, L)

            def left(q):
                return _add(hinge0, _rot(_unit((1.0, 0.0, side)), t1, _sub(q, hinge0)))

            def right(q):
                return _add(hinge1, _rot(_unit((-1.0, 0.0, side)), t2, _sub(q, hinge1)))

            def F(q):
                if q[2] <= sk - g:
                    return left(q)
                if q[2] >= sk + g:
                    return right(q)
                t = (q[2] - (sk - g)) / (2 * g)
                a, b = left(q), right(q)
                m = _lerp(a, b, t)
                return (m[0], m[1] - math.sin(math.pi * t) * g * 1.5, m[2])

            def keep(q, mi):
                if mi in bridging:
                    return True
                return abs(q[2] - sk - cut_off[mi] * 0.5) > g

            def curl(r):
                return (r.uniform(-1.0, 1.0) * h * 0.25, r.uniform(-1.2, 0.4) * h * 0.25, r.uniform(-0.5, 0.5) * h * 0.2)
            zone = lambda q: abs(q[2] - sk) < 3.0 * h
        _pieces(p, members, F, keep, rng, lod, piece, curl, zone)
        if lod < 2:                                          # the walkway plate, bent with the frame (no lamps: dead)
            if girder:
                c, s = (0.0, -h + 1.5, L / 2), (8.0, 0.6, L)
            else:
                c, s = (0.0, -h + 0.4, L / 2), (2 * h - 1.0, 0.4, L - 2.0)
            n = 20 if lod == 0 else 10
            if kind == "snapped":
                for half, (z0, z1) in enumerate(((c[2] - s[2] / 2, sk - g), (sk + g, c[2] + s[2] / 2))):
                    _dplate(p, GRATE if girder else HULL, (c[0], c[1], (z0 + z1) / 2), (s[0], s[1], z1 - z0), F, max(2, n // 2))
            else:
                _dplate(p, GRATE if girder else HULL, c, s, F, n)
        return P
    return fn


# ---------------------------------------------------------------- the lattice mast (hub_vista.mast_parts), bent over
def _mast_members(lod):
    out = []
    corners = ((-1, -1), (1, -1), (1, 1), (-1, 1))
    for kb in range(8):
        y0, y1 = 6.0 + kb * 55.0, 6.0 + (kb + 1) * 55.0
        h0, h1 = 14.0 - 9.0 * kb / 8, 14.0 - 9.0 * (kb + 1) / 8
        mat = RED if kb % 2 == 0 else LIGHT
        bays = 3 if lod < 2 else 1
        cs = 6 if lod < 2 else 4

        def at(y, sx, sz):
            hh = h0 + (h1 - h0) * (y - y0) / (y1 - y0)
            return (sx * hh, y, sz * hh)
        for sx, sz in corners:
            out.append((mat, at(y0, sx, sz), at(y1, sx, sz), 0.9, "cyl", cs))
        for k in range(bays + 1):
            y = y0 + (y1 - y0) * k / bays
            for i in range(4):
                out.append((mat, at(y, *corners[i]), at(y, *corners[(i + 1) % 4]), 0.45, "cyl", 4))
            if k < bays:
                y2 = y0 + (y1 - y0) * (k + 1) / bays
                for i in range(4):
                    out.append((mat, at(y, *corners[i]), at(y2, *corners[(i + 1) % 4]), 0.36, "cyl", 4))
    return out


def _mast_builder(kind):
    def fn(lod):
        rng = random.Random(zlib.crc32(b"hubv_mast:bent"))
        P = {"hubv_mast": Part("hubv_mast")}
        m = P["hubv_mast"]
        box(m, STEEL, (0, 3.0, 0), (40.0, 6.0, 40.0))
        phi = rng.uniform(0, 2 * math.pi)
        dirh = (math.cos(phi), 0.0, math.sin(phi))
        axis = _unit(_cross((0.0, 1.0, 0.0), dirh))                 # bending towards dirh
        k1, k2 = rng.uniform(150.0, 210.0), rng.uniform(290.0, 340.0)
        a1, a2 = math.radians(rng.uniform(48.0, 62.0)), math.radians(rng.uniform(25.0, 40.0))
        w = 14.0

        def F(q):
            p = q
            for yk, ang in ((k2, a2), (k1, a1)):                     # the upper kink first, then carry it round the lower
                t = _smooth((q[1] - (yk - w)) / (2 * w))
                if t <= 0.0:
                    continue
                hinge = _add((0.0, yk, 0.0), _mul(dirh, 8.0))        # the compressed side's chord
                bump = max(0.0, 1.0 - abs(q[1] - yk) / w)
                lp = _sub(p, hinge)
                lp = _add(lp, (q[0] * 0.35 * bump, 0.0, q[2] * 0.35 * bump))   # crushed chords bulge
                p = _add(hinge, _rot(axis, ang * t, lp))
            return p
        zone = lambda q: abs(q[1] - k1) < 30.0 or abs(q[1] - k2) < 22.0
        _pieces(m, _mast_members(lod), F, lambda q, mi: True, rng, lod, {0: 6.0, 1: 11.0, 2: 27.5}[lod], None, zone)
        return P
    return fn


# ---------------------------------------------------------------- registration
KIT_INTACT = {"hubb_ship_salon": (ship_salon_parts, ship_salon_hits,
                                  "Салон кораблей: стеклянный фасад, корабли на поворотных столах, корабль на пилоне")}
BUILDERS = {"hubb_works": (_works_builder, works_hits), "hubk_factory": (_factory_builder, factory_hits),
            "hubb_ship_salon": (_salon_builder, ship_salon_hits), "hubv_mast": (lambda k: _mast_builder(k), mast_hits)}
FRAME_HITS = {"hubb_girder": girder_hits, "hubk_truss": truss_hits}

ASSETS = {k: v[0] for k, v in KIT_INTACT.items()}
HITS = {k: v[1] for k, v in KIT_INTACT.items()}
ROOTS = {k: "hub" for k in KIT_INTACT}
PASSPORTS = {k: (v[2], "hub_module", "Модуль хаба (tools/models/units/hub_damage_heavy.py).") for k, v in KIT_INTACT.items()}
assert set(BUILDERS) == set(HEAVY_KINDS), "hub_damage_names.HEAVY_KINDS and BUILDERS differ"
for _m, _ks in HEAVY_KINDS.items():
    for _k, _title in _ks:
        _id = f"{_m}_dmg_{_k}"
        ASSETS[_id] = BUILDERS[_m][0](_k)
        HITS[_id] = BUILDERS[_m][1]
        ROOTS[_id] = "hub"
        PASSPORTS[_id] = (f"{_m} - повреждён: {_title}", "hub_module",
                          "Тяжело повреждённый модуль хаба (tools/models/units/hub_damage_heavy.py): сквозной пролом, "
                          "внутри цех / салон; станки и запчасти на LOD0 отдельной группой.")
for _m, _ks in FRAME_KINDS.items():
    for _k, _title in _ks:
        _id = f"{_m}_dmg_{_k}"
        ASSETS[_id] = _frame_builder(_m, _k)
        HITS[_id] = FRAME_HITS[_m]
        ROOTS[_id] = "hub"
        PASSPORTS[_id] = (f"{_m} - повреждена: {_title}", "hub_module",
                          "Повреждённая ферма каркаса хаба (tools/models/units/hub_damage_heavy.py): погнутые и "
                          "оборванные пояса, скрученная секция, висящие раскосы.")
