"""Damaged hub buildings (author 2026-11-17: «сразу надо сделать поврежденные постройки. тут надо учесть, что жилые или
офисного типа постройки могут быть прожжены лазерами, ракетами, рельсотронами, может не быть фасада, но при этом если
фасада нет - должны быть видны секции, на секциях этажи и отдельные комнаты или квартиры. на ЛОД0 можно помещать туда
даже столики с компьютерами, кровати, холодильники и какую-то такую мелочевку. может предметы быта вынести в отдельную
группу»; 2026-11-18: «давай сначала поврежденные модули, потом редактор»).

The intact residential / office modules are facade boxes whose windows the shader draws (eq_hub_facade*). A damaged
module here is the SAME module with its facade boxes rebuilt as a grid of cells - floors of FLOOR_H, bays of BAY - from
which the wounds knock cells out:
  missile  a blast crater several floors high, jagged; slabs broken off and hanging out over the rim; a burnt ring;
  laser    a long narrow burn cut across the facade, possibly round a corner; molten (burnt) edges;
  rail     a line of small punched holes and their exits on the opposite face;
  gutted   the facade torn off over a large area - the building's sections open floor by floor.
Behind every knocked-out cell the building's inside is built: the floor slab (its edge recessed and broken where two
open floors meet), the ceiling, the partitions between flats / offices every ROOM_BAYS bays, the room's back wall at
the section depth; near the blast everything inside is burnt. LOD0 also furnishes the open rooms (flats: beds,
wardrobes, tables, fridges, sofas; offices: desks with screens, chairs, cabinets) in a separate part group
<module>_furniture; LOD1 drops the furniture and the partitions, LOD2+ keep only the hole, the slabs and the back walls.
Each damaged module is its own module folder <module>_dmg_<kind> (the city assembly references modules by folder and
instances them), with the intact module's hit volumes.
"""
import math
import random
import zlib

from eqlib import *  # noqa: F401,F403 - the unit kernel
from units.hub_district import STEEL  # noqa: F401
from units.hub_district import ASSETS as _DISTRICT_ASSETS, HITS as _DISTRICT_HITS
from units.hub_belt import ASSETS as _BELT_ASSETS, HITS as _BELT_HITS

register_materials({
    "eq_hub_burnt": ((0.045, 0.040, 0.036), (0.04, 0.04, 0.04), 8),          # soot, charred facade
    "eq_hub_slab": ((0.36, 0.35, 0.33), (0.06, 0.06, 0.06), 10),            # floor slab, concrete
    "eq_hub_slab_under": ((0.26, 0.26, 0.25), (0.05, 0.05, 0.05), 10),
    "eq_hub_wall_flat": ((0.46, 0.42, 0.36), (0.05, 0.05, 0.05), 12),       # flats: warm plaster
    "eq_hub_wall_office": ((0.38, 0.41, 0.44), (0.06, 0.06, 0.06), 14),     # offices: cool panels
    "eq_hub_rebar": ((0.20, 0.17, 0.15), (0.30, 0.28, 0.26), 40),
    "eq_furn_wood": ((0.36, 0.24, 0.15), (0.08, 0.08, 0.08), 20),
    "eq_furn_fabric": ((0.30, 0.33, 0.40), (0.03, 0.03, 0.03), 6),
    "eq_furn_white": ((0.78, 0.79, 0.80), (0.30, 0.30, 0.30), 60),
    "eq_furn_metal": ((0.32, 0.33, 0.35), (0.40, 0.40, 0.42), 60),
    "eq_furn_burnt": ((0.06, 0.05, 0.05), (0.03, 0.03, 0.03), 6),
    # the facade colours without the eq_hub_facade prefix: the shader draws no lit windows on a damaged building
    "eq_hub_dead_facade": ((0.22, 0.23, 0.25), (0.20, 0.20, 0.22), 40),
    "eq_hub_dead_facade_light": ((0.40, 0.41, 0.42), (0.20, 0.20, 0.22), 40),
})
DEAD_FACADE = {"eq_hub_facade": "eq_hub_dead_facade", "eq_hub_facade_light": "eq_hub_dead_facade_light"}
BURNT, SLAB, SLAB_U, REBAR = "eq_hub_burnt", "eq_hub_slab", "eq_hub_slab_under", "eq_hub_rebar"
SCREEN = "eq_hub_window_dark"          # a dead screen: the damaged building has no power, nothing glows

FLOOR_H = 4.0          # the facade shader's floor height (large_object_fill.frag: floors 4 m)
BAY = 6.0              # a bay: two of the shader's 3 m window pitches
ROOM_BAYS = 2          # a flat / an office spans two bays
SECTION_D = 9.0        # the depth of a room behind the facade (at most half the building's depth)
WALL_IN = 0.15         # an inner wall stands this far inside its own room: two rooms' walls never share a plane
CORNER_GAP = 0.3       # the x faces' rooms stop this far short of the corner squares the z faces own
SLAB_T = 0.35


# ---------------------------------------------------------------- the damageable modules: their facade boxes
# (material, centre, size) of every facade box, exactly as the intact builders place them; and the kind of rooms
BUILDINGS = {
    "hubb_hab_tower": {"boxes": [("eq_hub_facade", (0.0, 90.0, 0.0), (80.0, 180.0, 80.0)),
                                 ("eq_hub_facade_light", (0.0, 210.0, 0.0), (60.0, 60.0, 60.0))],
                       "use": "flat", "src": (_BELT_ASSETS, _BELT_HITS)},
    "hubb_hab_slab": {"boxes": [("eq_hub_facade", (0.0, 36.0, 0.0), (220.0, 72.0, 50.0)),
                                ("eq_hub_facade_light", (-40.0, 84.0, 0.0), (100.0, 24.0, 40.0))],
                      "use": "flat", "src": (_BELT_ASSETS, _BELT_HITS)},
    "hubk_hab_a": {"boxes": [("eq_hub_facade", (0.0, 45.0, 0.0), (40.0, 90.0, 30.0))], "use": "office",
                   "src": (_DISTRICT_ASSETS, _DISTRICT_HITS)},
    "hubk_hab_b": {"boxes": [("eq_hub_facade", (0.0, 65.0, 0.0), (24.0, 130.0, 24.0))], "use": "flat",
                   "src": (_DISTRICT_ASSETS, _DISTRICT_HITS)},
    "hubk_hab_c": {"boxes": [("eq_hub_facade", (0.0, 20.0, 0.0), (60.0, 40.0, 36.0))], "use": "office",
                   "src": (_DISTRICT_ASSETS, _DISTRICT_HITS)},
}
from units.hub_damage_names import DAMAGEABLE, KINDS  # noqa: E402 - one list for the builders and the ruined city

FACES = [((1.0, 0.0, 0.0), (0.0, 0.0, -1.0)), ((-1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
         ((0.0, 0.0, 1.0), (1.0, 0.0, 0.0)), ((0.0, 0.0, -1.0), (-1.0, 0.0, 0.0))]   # (outward normal, along-face u)


def _v(*a):
    return tuple(float(x) for x in a)


def _add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _mul(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _quad(part, mat, p0, p1, p2, p3, n):
    part.face(mat, [part.vert(p, n) for p in (p0, p1, p2, p3)], n)


class _Face:
    """one vertical face of a facade box: u along it (-w/2..w/2), v up from the box bottom, depth inward"""

    def __init__(self, centre, size, fi):
        n, t = FACES[fi]
        self.n, self.t = n, t
        hx, hz = size[0] / 2, size[2] / 2
        self.width = size[2] if abs(n[0]) > 0.5 else size[0]
        self.thick = size[0] if abs(n[0]) > 0.5 else size[2]          # the building's depth seen from this face
        self.height = size[1]
        self.origin = (centre[0] + n[0] * hx, centre[1] - size[1] / 2, centre[2] + n[2] * hz)
        self.nu = max(1, int(round(self.width / BAY)))
        self.nv = max(1, int(round(self.height / FLOOR_H)))
        self.cw, self.ch = self.width / self.nu, self.height / self.nv
        self.depth = min(SECTION_D, self.thick / 2 - 0.6)
        # the u span the face's rooms may use. The rooms of two adjacent faces would both cover the corner square
        # (depth x depth): coplanar floors / ceilings / walls z-fight (live 2026-11-18: «стенки с нестабильным
        # освещением»). The z faces own the corners, the x faces stop short of them.
        self.ulo, self.uhi = -self.width / 2, self.width / 2
        if abs(n[0]) > 0.5:
            side = min(SECTION_D, self.width / 2 - 0.6) + CORNER_GAP
            self.ulo, self.uhi = self.ulo + side, self.uhi - side

    def span(self, i):
        """the cell i's u span clipped to the face's room span (None: nothing of it is the face's)"""
        a, b = max(self.cell_u(i), self.ulo), min(self.cell_u(i + 1), self.uhi)
        return (a, b) if b - a >= 0.5 else None

    def p(self, u, v, d=0.0):
        """face coords -> module coords (d: metres inward)"""
        return (self.origin[0] + self.t[0] * u - self.n[0] * d, self.origin[1] + v, self.origin[2] + self.t[2] * u - self.n[2] * d)

    def cell_u(self, i):
        return -self.width / 2 + i * self.cw

    def cell_v(self, j):
        return j * self.ch


# ---------------------------------------------------------------- wounds -> the cell masks of every face
def _noise(rng, k=4):
    ph = [rng.uniform(0, 2 * math.pi) for _ in range(k)]
    am = [rng.uniform(0.05, 0.22) / (i + 1) ** 0.6 for i in range(k)]
    return lambda a: sum(am[i] * math.sin((i + 2) * a + ph[i]) for i in range(k))


def _plan(module, kind, faces):
    """-> {(box index, face index): [wound]}, wound = (type, params): the seeded damage of one module variant. Heavy
    (author: «может не быть фасада»): several hits, the big ones a third of the face across."""
    rng = random.Random(zlib.crc32(f"{module}:{kind}".encode()))
    main = faces[0]                                   # the main facade box's faces
    plan = {}

    def blast(bi, fi, r_frac, v_lo, v_hi):
        f = faces[bi][fi]
        r = max(6.0, min(30.0, r_frac * min(f.width, f.height)))
        plan.setdefault((bi, fi), []).append(("blast", (rng.uniform(-0.3, 0.3) * f.width, rng.uniform(v_lo, v_hi) * f.height,
                                                        r, _noise(rng))))

    fi = rng.randrange(4)
    f = main[fi]
    if kind == "missile":
        blast(0, fi, rng.uniform(0.30, 0.38), 0.3, 0.75)                 # the main hit
        blast(0, (fi + rng.choice((1, 3))) % 4, rng.uniform(0.18, 0.26), 0.15, 0.8)
        if rng.random() < 0.7:
            blast(0, (fi + 2) % 4, rng.uniform(0.12, 0.2), 0.2, 0.85)
        if len(faces) > 1 and rng.random() < 0.6:                       # the top block took one too
            blast(1, rng.randrange(4), rng.uniform(0.25, 0.35), 0.3, 0.7)
    elif kind == "laser":
        # two cuts sweeping across the facade (one runs on round the corner), and a railgun burst + its exits
        for k in range(2):
            g = (fi + k) % 4
            h = main[g]
            u0, v0 = -0.5 * h.width - 1.0, rng.uniform(0.2, 0.85) * h.height
            u1, v1 = 0.5 * h.width + 1.0, v0 + rng.uniform(-0.45, 0.45) * h.height
            plan.setdefault((0, g), []).append(("cut", (u0, v0, u1, v1, rng.uniform(3.0, 5.5), _noise(rng))))
        rf = (fi + 2) % 4
        rr = main[rf]
        base_u, base_v = rng.uniform(-0.35, 0.0) * rr.width, rng.uniform(0.2, 0.7) * rr.height
        holes = [(base_u + k * rng.uniform(5.0, 8.0), base_v + k * rng.uniform(-3.0, 3.0)) for k in range(rng.randint(4, 7))]
        plan.setdefault((0, rf), []).extend(("hole", (u, v, rng.uniform(2.4, 3.6))) for u, v in holes)
        # the exits on the opposite face (= fi): mirrored u (the faces' u axes run opposite), a little lower, wider
        plan.setdefault((0, fi), []).extend(("hole", (-u, v - rng.uniform(0.0, 4.0), rng.uniform(3.5, 5.5))) for u, v in holes)
    else:   # gutted: the facade torn off over most of one face and much of another, + a blast
        for k, (wf, hf) in enumerate(((rng.uniform(0.6, 0.9), rng.uniform(0.45, 0.8)), (rng.uniform(0.35, 0.6), rng.uniform(0.3, 0.55)))):
            g = (fi + k) % 4
            h = main[g]
            cu = rng.uniform(-0.5 + wf / 2, 0.5 - wf / 2) * h.width
            h0 = rng.uniform(0.08, max(0.09, 0.95 - hf)) * h.height
            h1 = min(h.height - FLOOR_H, h0 + hf * h.height)
            plan.setdefault((0, g), []).append(("strip", (cu - wf * h.width / 2, h0, cu + wf * h.width / 2, h1,
                                                          _noise(rng), _noise(rng))))
        blast(0, (fi + 2) % 4, rng.uniform(0.15, 0.25), 0.35, 0.7)
    return plan


def _mask(face, wounds):
    """-> (open[i][j], burnt[i][j], heat[i][j] 0..1 how close to the blast centre)"""
    nu, nv = face.nu, face.nv
    opened = [[False] * nv for _ in range(nu)]
    burnt = [[False] * nv for _ in range(nu)]
    heat = [[0.0] * nv for _ in range(nu)]
    for i in range(nu):
        for j in range(nv):
            u = face.cell_u(i) + face.cw / 2
            v = face.cell_v(j) + face.ch / 2
            for kind, prm in wounds:
                if kind == "blast":
                    cu, cv, r, nz = prm
                    a = math.atan2(v - cv, u - cu)
                    d = math.hypot(u - cu, v - cv)
                    rr = r * (1.0 + nz(a))
                    if d < rr:
                        opened[i][j] = True
                    if d < rr + 7.0:
                        burnt[i][j] = True
                    heat[i][j] = max(heat[i][j], max(0.0, 1.0 - d / (rr + 7.0)))
                elif kind == "cut":
                    u0, v0, u1, v1, w, nz = prm
                    du, dv = u1 - u0, v1 - v0
                    L2 = du * du + dv * dv
                    t = max(0.0, min(1.0, ((u - u0) * du + (v - v0) * dv) / L2)) if L2 > 1e-9 else 0.0
                    d = math.hypot(u - (u0 + du * t), v - (v0 + dv * t))
                    ww = w * (1.0 + 2.0 * nz(t * 9.0))
                    if d < ww + face.cw * 0.25:
                        opened[i][j] = True
                    if d < ww + 4.0:
                        burnt[i][j] = True
                    heat[i][j] = max(heat[i][j], 0.5 * max(0.0, 1.0 - d / (ww + 4.0)))
                elif kind == "hole":
                    cu, cv, r = prm
                    d = math.hypot(u - cu, v - cv)
                    if d < r + 0.5 * min(face.cw, face.ch):
                        opened[i][j] = True
                    if d < r + 3.0:
                        burnt[i][j] = True
                else:   # strip
                    u0, v0, u1, v1, nzu, nzv = prm
                    jag_u = 2.5 * BAY * nzu(v * 0.21)
                    jag_v = 2.5 * FLOOR_H * nzv(u * 0.17)
                    if u0 + jag_u < u < u1 + jag_u and v0 + jag_v < v < v1 + jag_v:
                        opened[i][j] = True
                    if u0 - 5.0 + jag_u < u < u1 + 5.0 + jag_u and v0 - 5.0 + jag_v < v < v1 + 5.0 + jag_v:
                        burnt[i][j] = True
                    cu, cv = (u0 + u1) / 2, (v0 + v1) / 2
                    heat[i][j] = max(heat[i][j], 0.35 * max(0.0, 1.0 - math.hypot(u - cu, v - cv) / max(u1 - u0, v1 - v0)))
    # the ground floor and the top floor keep their cells (the plinth and the roof edge hold the box)
    for i in range(nu):
        opened[i][0] = False
        opened[i][nv - 1] = False
    return opened, burnt, heat


# ---------------------------------------------------------------- geometry
def _merged_rects(nu, nv, take):
    """greedy merge of the cells where take(i, j) is true into rectangles (i0, j0, i1, j1) (exclusive ends)"""
    used = [[False] * nv for _ in range(nu)]
    out = []
    for j in range(nv):
        i = 0
        while i < nu:
            if used[i][j] or not take(i, j):
                i += 1
                continue
            i1 = i
            while i1 < nu and not used[i1][j] and take(i1, j):
                i1 += 1
            j1 = j + 1
            while j1 < nv and all(not used[k][j1] and take(k, j1) for k in range(i, i1)):
                j1 += 1
            for k in range(i, i1):
                for m in range(j, j1):
                    used[k][m] = True
            out.append((i, j, i1, j1))
            i = i1
    return out


def _facade(part, face, mat, opened, burnt):
    """the face's remaining cells: the facade material (windows by the shader) and the burnt ring (no windows).
    The merged rectangles share the face's grid vertices along every border (each is fanned from its own centre over
    all the grid points of its perimeter), so neighbouring pieces share their edges: no T-junction, no open edge -
    the edge pass draws the hole's rim, not the seams of the pieces (live 2026-11-18: separate quads striped every
    damaged facade with the edge lines)."""
    n = face.n
    grid = {}

    def gv(i, j):
        k = (i, j)
        if k not in grid:
            grid[k] = part.vert(face.p(face.cell_u(i), face.cell_v(j)), n)
        return grid[k]

    for take, m in ((lambda i, j: not opened[i][j] and not burnt[i][j], mat), (lambda i, j: not opened[i][j] and burnt[i][j], BURNT)):
        for i0, j0, i1, j1 in _merged_rects(face.nu, face.nv, take):
            ring = ([gv(i, j0) for i in range(i0, i1)] + [gv(i1, j) for j in range(j0, j1)] +
                    [gv(i, j1) for i in range(i1, i0, -1)] + [gv(i0, j) for j in range(j1, j0, -1)])
            c = part.vert(face.p((face.cell_u(i0) + face.cell_u(i1)) / 2, (face.cell_v(j0) + face.cell_v(j1)) / 2), n)
            for k in range(len(ring)):
                part.face(m, [c, ring[k], ring[(k + 1) % len(ring)]], n)


def _box_caps(part, mat, centre, size):
    """the facade box's roof and underside (the sides are the cell grid)"""
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    cx, cy, cz = centre
    for s in (1.0, -1.0):
        y = cy + s * hy
        _quad(part, mat, (cx - hx, y, cz - hz), (cx + hx, y, cz - hz), (cx + hx, y, cz + hz), (cx - hx, y, cz + hz), (0.0, s, 0.0))


def _interior(part, face, opened, heat, use, rng, lod, furn):
    """the rooms behind the knocked-out cells: slabs, ceilings, back walls, partitions (+ furniture at LOD0)"""
    nu, nv, cw, ch, D = face.nu, face.nv, face.cw, face.ch, face.depth
    n, t = face.n, face.t
    inward = (-n[0], 0.0, -n[2])
    wall = "eq_hub_wall_flat" if use == "flat" else "eq_hub_wall_office"
    # broken slab edges: where two open floors meet the slab front is recessed by a jagged amount
    recess = [[(rng.uniform(0.0, 1.6) if (0 < j < nv and opened[i][j] and opened[i][j - 1]) else 0.0) for j in range(nv + 1)]
              for i in range(nu)]
    collapsed = _collapsed(face, opened, heat)
    for i in range(nu):
        sp = face.span(i)
        if sp is None:
            continue
        u0, u1 = sp
        for j in range(nv):
            if not opened[i][j]:
                continue
            hot = heat[i][j] > 0.55
            v0, v1 = face.cell_v(j), face.cell_v(j + 1)
            vc = v1 - SLAB_T                                     # the ceiling (the next slab's underside)
            r0 = recess[i][j]
            r1 = recess[i][j + 1] if j + 1 < nv else 0.0
            fm = BURNT if hot else SLAB
            # floor (top of this floor's slab), ceiling (underside of the next one) - unless that slab fell
            top_gone = j + 1 < nv and collapsed[i][j + 1]
            if not collapsed[i][j]:
                _quad(part, fm, face.p(u0, v0, r0), face.p(u1, v0, r0), face.p(u1, v0, D), face.p(u0, v0, D), (0.0, 1.0, 0.0))
            else:
                # the fallen slab lies tilted on the floor below, its back still caught at the section wall
                vb = face.cell_v(j - 1)
                a0, a1 = u0 + 0.3, u1 - 0.3
                p0, p1 = face.p(a0, vb + 0.15, 0.8), face.p(a1, vb + 0.15, 0.8)
                p2, p3 = face.p(a1, v0 - 0.4, D - 0.4), face.p(a0, v0 - 0.4, D - 0.4)
                tilt = (n[0] * 0.5, 1.0, n[2] * 0.5)
                _quad(part, BURNT if hot else SLAB, p0, p1, p2, p3, tilt)
                under = _mul(tilt, -SLAB_T / math.sqrt(_dot(tilt, tilt)))       # a real thickness, not back-to-back
                _quad(part, SLAB_U, _add(p0, under), _add(p3, under), _add(p2, under), _add(p1, under), _mul(tilt, -1.0))
            if not top_gone:
                _quad(part, BURNT if hot else SLAB_U, face.p(u0, vc, r1), face.p(u1, vc, r1), face.p(u1, vc, D), face.p(u0, vc, D),
                      (0.0, -1.0, 0.0))
            # the next slab's front edge (its broken face) when it is open above too
            if j + 1 < nv and opened[i][j + 1] and not top_gone:
                _quad(part, fm, face.p(u0, vc, r1), face.p(u1, vc, r1), face.p(u1, v1, r1), face.p(u0, v1, r1), n)
            # the room's back wall
            _quad(part, BURNT if hot else wall, face.p(u0, v0, D), face.p(u1, v0, D), face.p(u1, vc, D), face.p(u0, vc, D), n)
            # side walls: at the hole's edge (the neighbour keeps its facade) and between rooms
            for side, ub, nb in ((-1, u0, i - 1), (1, u1, i + 1)):
                clipped = ub != (face.cell_u(i) if side < 0 else face.cell_u(i + 1))
                edge = clipped or nb < 0 or nb >= nu or not opened[nb][j] or face.span(nb) is None
                partition = (i if side < 0 else i + 1) % ROOM_BAYS == 0
                if edge or (partition and lod == 0):
                    wn = _mul(t, -float(side))
                    ub -= side * WALL_IN                                     # inside this room, off the shared plane
                    _quad(part, BURNT if hot else wall, face.p(ub, v0, 0.0), face.p(ub, v0, D), face.p(ub, vc, D),
                          face.p(ub, vc, 0.0), wn)
            # a broken slab hanging out over the rim of a blast (missile), with its rebar
            if lod <= 1 and heat[i][j] > 0.25 and j + 1 < nv and not opened[i][j + 1] and rng.random() < 0.35:
                L = rng.uniform(1.5, 3.5)
                droop = math.radians(rng.uniform(20.0, 55.0))
                a = face.p(rng.uniform(u0 + 0.5, u1 - 2.5), v1 - SLAB_T, 0.0)
                w = rng.uniform(1.5, 2.5)
                out = (n[0] * math.cos(droop), -math.sin(droop), n[2] * math.cos(droop))
                p0, p1 = a, _add(a, _mul(t, w))
                p2, p3 = _add(p1, _mul(out, L)), _add(p0, _mul(out, L))
                up = (-n[0] * math.sin(droop), math.cos(droop), -n[2] * math.sin(droop))
                _quad(part, BURNT, p0, p1, p2, p3, up)
                under = _mul(up, -SLAB_T)
                _quad(part, SLAB_U, _add(p0, under), _add(p3, under), _add(p2, under), _add(p1, under), _mul(up, -1.0))
                if lod == 0:
                    for k in range(3):
                        b = _add(p3, _mul(t, w * (k + 0.5) / 3))
                        cyl(part, REBAR, b, _add(b, _mul(out, rng.uniform(0.6, 1.4))), 0.03, 4)
    if furn is not None:
        _furnish(furn, face, opened, heat, use, rng, collapsed)


def _collapsed(face, opened, heat):
    """collapsed[i][j]: the slab under the cell (i, j) fell (both floors it separates are open) - double-height voids,
    more of them near a blast; seeded by the face alone, so every LOD collapses the same slabs"""
    rng = random.Random(zlib.crc32(repr((round(face.origin[0], 1), round(face.origin[1], 1), round(face.origin[2], 1),
                                         face.t)).encode()))
    out = [[False] * face.nv for _ in range(face.nu)]
    for i in range(face.nu):
        for j in range(2, face.nv - 1):
            if opened[i][j] and opened[i][j - 1] and not out[i][j - 1]:
                out[i][j] = rng.random() < (0.22 if heat[i][j] > 0.25 else 0.07)
    return out


def _furnish(furn, face, opened, heat, use, rng, collapsed):
    """LOD0: the open rooms' furniture, one room = ROOM_BAYS bays of one floor"""
    nu, nv, cw, D = face.nu, face.nv, face.cw, face.depth
    for j in range(1, nv - 1):
        for r0 in range(0, nu, ROOM_BAYS):
            cells = [i for i in range(r0, min(nu, r0 + ROOM_BAYS))
                     if opened[i][j] and not collapsed[i][j] and face.span(i) is not None]
            if not cells:
                continue
            u0, u1 = face.span(cells[0])[0] + WALL_IN, face.span(cells[-1])[1] - WALL_IN
            taken = []                       # the u spans already furnished: no two pieces overlap (coplanar tops)
            v0 = face.cell_v(j)
            hot = max(heat[i][j] for i in cells)
            items = _room_items(use, rng)
            for (w, h, d, mat, tag) in items:
                if u1 - u0 < w + 0.6 or D < d + 1.0:
                    continue
                for _try in range(6):
                    cu = rng.uniform(u0 + w / 2 + 0.3, u1 - w / 2 - 0.3)
                    if all(cu + w / 2 + 0.2 < a or cu - w / 2 - 0.2 > b for a, b in taken):
                        break
                else:
                    continue
                taken.append((cu - w / 2, cu + w / 2))
                cd = rng.uniform(d / 2 + 0.6, D - d / 2 - 0.3)
                m = "eq_furn_burnt" if (hot > 0.45 and rng.random() < 0.8) else mat
                tipped = hot > 0.3 and rng.random() < 0.4
                hh = w if tipped else h
                c = face.p(cu, v0 + hh / 2, cd)
                yaw = math.atan2(face.t[0], face.t[2]) + (rng.choice((0.0, math.pi / 2)) if tag != "desk" else 0.0)
                R = rot_y(yaw)
                if tipped:
                    R = mat_mul(R, rot_z(math.pi / 2))
                box(furn, m, c, (w, h, d), R)
                if tag == "desk":
                    # a monitor on the desk (a dim screen), a chair in front of it
                    sc = face.p(cu, v0 + h + 0.25, cd)
                    box(furn, "eq_furn_burnt" if hot > 0.45 else SCREEN, sc, (0.55, 0.35, 0.05), rot_y(yaw))
                    box(furn, "eq_furn_metal", face.p(cu, v0 + 0.25, max(0.5, cd - 0.9)), (0.5, 0.5, 0.5), rot_y(yaw))
                elif tag == "bed":
                    box(furn, "eq_furn_white" if m == mat else m, face.p(cu, v0 + h + 0.08, cd), (w * 0.95, 0.16, d * 0.95), R)


def _room_items(use, rng):
    """(w, h, d, material, tag) of the things in one room"""
    if use == "flat":
        pool = [(2.0, 0.45, 1.6, "eq_furn_wood", "bed"), (1.2, 2.0, 0.6, "eq_furn_wood", "wardrobe"),
                (1.2, 0.75, 0.8, "eq_furn_wood", "table"), (0.7, 1.8, 0.65, "eq_furn_white", "fridge"),
                (2.0, 0.8, 0.9, "eq_furn_fabric", "sofa"), (0.5, 0.9, 0.5, "eq_furn_white", "washer")]
        return rng.sample(pool, rng.randint(2, 4))
    pool = [(1.6, 0.75, 0.8, "eq_furn_metal", "desk")] * 3 + [(0.5, 1.3, 0.6, "eq_furn_metal", "cabinet"),
                                                              (1.8, 0.75, 1.0, "eq_furn_wood", "table")]
    return rng.sample(pool, rng.randint(2, 4))


# ---------------------------------------------------------------- the damaged module builders
def _damaged_builder(module, kind):
    spec = BUILDINGS[module]
    intact = spec["src"][0][module]
    facade_mats = {b[0] for b in spec["boxes"]}

    def fn(lod):
        P = intact(lod)
        # the intact module without its facade boxes (every facade-material face belongs to them)
        for part in P.values():
            for m in list(part.faces):
                if m in facade_mats:
                    del part.faces[m]
        body = next(iter(P.values()))
        furn = Part(f"{module}_furniture") if lod == 0 else None
        rng = random.Random(zlib.crc32(f"{module}:{kind}:interior".encode()))
        faces = [[_Face(c, s, fi) for fi in range(4)] for (_, c, s) in spec["boxes"]]
        plan = _plan(module, kind, faces)
        for bi, (mat, c, s) in enumerate(spec["boxes"]):
            mat = DEAD_FACADE.get(mat, mat)
            _box_caps(body, mat, c, s)
            for fi, face in enumerate(faces[bi]):
                wounds = plan.get((bi, fi), [])
                opened, burnt, heat = _mask(face, wounds)
                _facade(body, face, mat, opened, burnt)
                if any(any(col) for col in opened):
                    _interior(body, face, opened, heat, spec["use"], rng, lod, furn)
        if furn is not None and furn.faces:
            P[furn.name] = furn
        return P
    return fn


assert set(BUILDINGS) == set(DAMAGEABLE), "hub_damage_names.DAMAGEABLE and BUILDINGS differ"
ASSETS, HITS, ROOTS, PASSPORTS, DAMAGED_OF = {}, {}, {}, {}, {}
for _m, _spec in BUILDINGS.items():
    for _k, _title in KINDS:
        _id = f"{_m}_dmg_{_k}"
        ASSETS[_id] = _damaged_builder(_m, _k)
        HITS[_id] = _spec["src"][1][_m]
        ROOTS[_id] = "hub"
        PASSPORTS[_id] = (f"{_m} - повреждён: {_title}", "hub_module",
                          "Повреждённое здание хаба (tools/models/units/hub_damage.py): пробитый фасад, за ним секции, "
                          "этажи, комнаты; мебель на LOD0 отдельной группой.")
        DAMAGED_OF.setdefault(_m, []).append(_id)
