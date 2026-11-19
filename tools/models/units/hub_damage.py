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
    "eq_hub_soot": ((0.14, 0.13, 0.125), (0.04, 0.04, 0.04), 8),          # soot round a wound (the gradient to burnt)
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
SOOT = "eq_hub_soot"
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
                       "use": "flat", "src": (_BELT_ASSETS, _BELT_HITS),
                       # the belts round the tower (hub_belt.hab_tower_parts): a torn / breached tower rebuilds them
                       # per facade column, so a belt never bridges the void
                       "bands": [("eq_hub_hull", (0.0, y, 0.0), (86.0, 3.0, 86.0)) for y in (60.0, 120.0, 180.0)]},
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


# ---------------------------------------------------------------- severe damage: torn / breach (author 2026-11-19:
# «вырвано половина жилой башни, большие сквозные повреждения здания (нужно внутри изобразить этажи и интерьеры»)
#   torn    roughly half the building ripped away: a jagged diagonal tear runs from ~a third of the height on one side
#           to near the top on the other; the floors end in a staircase of open rooms, slabs stick out over the void
#           with rebar, broken slabs and chunks hang off the edges, the stair / lift core stands a few floors higher
#   breach  a big jagged hole straight through the building (one sees the sky through it), the rooms open on all its
#           sides, slab stubs stick into it, the rim burnt
# The facade boxes become voxel grids (BAY x FLOOR_H x BAY); a voxel is gone or present; a present voxel next to a gone
# one (or behind a blown-off facade cell) is an open room: its floor, ceiling, the walls towards hidden voxels, the
# inner side of its facade, partitions between rooms (rooms of 2 x 2 voxels, LOD0), furniture (LOD0).
SEVERE = ("torn", "breach")
ROOM_VOX = 2                 # a room spans 2 x 2 voxels (12 x 12 m)


class _Vox:
    """a facade box as a voxel grid: i along x, j up (floors), k along z"""

    def __init__(self, centre, size):
        self.c0 = (centre[0] - size[0] / 2, centre[1] - size[1] / 2, centre[2] - size[2] / 2)
        self.size = size
        self.n = (max(1, int(round(size[0] / BAY))), max(1, int(round(size[1] / FLOOR_H))), max(1, int(round(size[2] / BAY))))
        self.d = tuple(size[a] / self.n[a] for a in range(3))

    def lo(self, i, j, k):
        return (self.c0[0] + i * self.d[0], self.c0[1] + j * self.d[1], self.c0[2] + k * self.d[2])

    def mid(self, i, j, k):
        return (self.c0[0] + (i + 0.5) * self.d[0], self.c0[1] + (j + 0.5) * self.d[1], self.c0[2] + (k + 0.5) * self.d[2])

    def index(self, p):
        return tuple(max(0, min(self.n[a] - 1, int(math.floor((p[a] - self.c0[a]) / self.d[a])))) for a in range(3))

    def inside(self, i, j, k):
        return 0 <= i < self.n[0] and 0 <= j < self.n[1] and 0 <= k < self.n[2]


def _hash01(*key):
    return (zlib.crc32(repr(key).encode()) & 0xFFFFFF) / float(0x1000000)


_VN = {}


def _vnoise(seed, x, y, z, cell):
    """smooth lattice value noise in -1..1 over x / z (cached corners), stepping per floor in y: every floor gets its
    own outline"""
    gx, gz = x / cell, z / cell
    ix, iz = math.floor(gx), math.floor(gz)
    fx, fz = gx - ix, gz - iz
    fx, fz = fx * fx * (3 - 2 * fx), fz * fz * (3 - 2 * fz)
    iy = int(math.floor(y / FLOOR_H + 0.5))

    def L(a, c):
        k = (seed, a, iy, c, cell)
        v = _VN.get(k)
        if v is None:
            v = _VN[k] = 2.0 * _hash01(*k) - 1.0
        return v
    return ((L(ix, iz) * (1 - fx) + L(ix + 1, iz) * fx) * (1 - fz) + (L(ix, iz + 1) * (1 - fx) + L(ix + 1, iz + 1) * fx) * fz)


def _severe_plan(module, kind, spec):
    """-> (gone(p), heat(p) 0..1, core lift (m the core survives above the tear), gone_fine(p): the same wound with a
    fine, per-floor jag - the slabs and the facade end along it, off the voxel grid)"""
    rng = random.Random(zlib.crc32(f"{module}:{kind}:severe".encode()))
    seed = zlib.crc32(f"{module}:{kind}:fine".encode())
    jag = lambda p: 0.65 * _vnoise(seed, p[0], p[1], p[2], 5.0) + 0.35 * _vnoise(seed + 1, p[0], p[1], p[2], 2.1)
    _, mc, ms = spec["boxes"][0]
    ybot = mc[1] - ms[1] / 2
    ytop = max(c[1] + s[1] / 2 for _, c, s in spec["boxes"])
    H = ytop - ybot
    if kind == "torn":
        # a diagonal tear from ~1/4 of the height on one side to ~3/4 on the other: about half the volume gone, the
        # top missing from every side
        long_x = ms[0] > 1.5 * ms[2]
        long_z = ms[2] > 1.5 * ms[0]
        phi = (0.0 if long_x else math.pi / 2 if long_z else rng.uniform(0, 2 * math.pi)) + rng.uniform(-0.35, 0.35)
        if rng.random() < 0.5:
            phi += math.pi
        d0, d1 = math.cos(phi), math.sin(phi)
        E = ms[0] / 2 * abs(d0) + ms[2] / 2 * abs(d1)
        lo_f, hi_f = rng.uniform(0.20, 0.28), rng.uniform(0.66, 0.76)
        ph = [rng.uniform(0, 2 * math.pi) for _ in range(5)]
        fx, fz = rng.uniform(0.07, 0.12), rng.uniform(0.07, 0.12)

        def cut(x, z):
            s = max(-1.0, min(1.0, ((x - mc[0]) * d0 + (z - mc[2]) * d1) / E))
            base = lo_f + (hi_f - lo_f) * (s + 1) / 2
            j = (0.050 * math.sin(x * fx + ph[0]) + 0.045 * math.sin(z * fz + ph[1]) +
                 0.030 * math.sin((x + z) * 0.31 + ph[2]) + 0.025 * math.sin((x - z) * 0.23 + ph[3]))
            return ybot + H * (base + j)

        lift = rng.randint(2, 4) * FLOOR_H
        amp = 5.0
        return ((lambda p: p[1] > cut(p[0], p[2])),
                (lambda p: 0.62 * max(0.0, 1.0 - (cut(p[0], p[2]) - p[1]) / (2.5 * FLOOR_H))),   # torn: little fire
                lift,
                (lambda p: p[1] > cut(p[0], p[2]) + amp * jag(p)))
    # breach: a jagged elliptic tunnel through the building's thinner horizontal dimension, >= 1/3 of the facade wide
    # and >= 1/4 of its height
    ax = 0 if ms[0] < ms[2] - 1.0 else 2 if ms[2] < ms[0] - 1.0 else rng.choice((0, 2))
    bx = 2 - ax
    wb, depth, Hm = ms[bx], ms[ax], ms[1]
    rmax = (0.40 if wb > 30.0 else 0.36) * wb          # a slim tower keeps its corner piers
    rx = min(rmax * 0.92, max(0.34 * min(wb, Hm), 0.19 * wb, 7.0))
    ry = min(0.40 * Hm, max(rx * 0.9, 0.17 * Hm))
    cb = mc[bx] + rng.uniform(-0.10, 0.10) * wb
    yc = ybot + rng.uniform(0.40, 0.60) * Hm
    yc = max(ybot + ry + FLOOR_H, min(ybot + Hm - ry - FLOOR_H, yc))
    nz, nz2 = _noise(rng, 5), _noise(rng, 3)
    sgn = rng.choice((-1.0, 1.0))
    rmin = min(rx, ry)

    def k(p):
        t = (p[ax] - mc[ax]) / (depth / 2)
        a = math.atan2((p[1] - yc) / ry, (p[bx] - cb) / rx)
        # the jag is mostly round the rim (angle); along the tunnel it stays open - one sees through it
        return (1.0 + 0.12 * sgn * t) * (1.0 + 0.5 * nz(a)) * (1.0 + 0.15 * nz2(t * 2.0 + a))

    def dn(p):
        return math.hypot((p[bx] - cb) / rx, (p[1] - yc) / ry)

    def gone(p):
        return abs(p[bx] - cb) < rmax and dn(p) < k(p)

    def gone_fine(p):
        # slabs stick 0.5..3.5 m into the hole (less in a small one: it stays >= 1/3 of the facade wide)
        reach = (0.5 + 3.0 * (0.5 + 0.5 * jag(p))) * min(1.0, rmin / 20.0) / rmin
        return abs(p[bx] - cb) < rmax - 0.5 and dn(p) < k(p) - reach

    return gone, (lambda p: max(0.0, 1.0 - (dn(p) - k(p)) * rmin / 9.0)), 0.0, gone_fine


def _drop_box_faces(part, mat, centre, size):
    """remove the faces of one box (material mat) the intact builder placed at centre/size"""
    lo = [centre[a] - size[a] / 2 - 0.01 for a in range(3)]
    hi = [centre[a] + size[a] / 2 + 0.01 for a in range(3)]
    if mat not in part.faces:
        return
    part.faces[mat] = [f for f in part.faces[mat]
                       if not all(lo[a] <= part.v[i][a] <= hi[a] for i in f for a in range(3))]


def _clip_gone(part, gone):
    """drop the intact module's remaining faces that reach into the void (roof plant, masts, belts, the top block's cap)"""
    for m in list(part.faces):
        part.faces[m] = [f for f in part.faces[m] if not any(gone(part.v[i]) for i in f)]


def _q(part, mat, a, b, c, d, n):
    part.face(mat, [part.vert(p, n) for p in (a, b, c, d)], n)


def _axis_quad(part, mat, axis, s, plane, lo, hi):
    """a quad in the plane coordinate[axis] = plane, spanning lo..hi in the other two axes, facing s along axis"""
    u, w = [a for a in range(3) if a != axis]
    pts = []
    for cu, cw in ((0, 0), (1, 0), (1, 1), (0, 1)):
        p = [0.0, 0.0, 0.0]
        p[axis] = plane
        p[u] = hi[u] if cu else lo[u]
        p[w] = hi[w] if cw else lo[w]
        pts.append(tuple(p))
    n = [0.0, 0.0, 0.0]
    n[axis] = float(s)
    _q(part, mat, *pts, tuple(n))


def _severe_builder(module, kind):
    spec = BUILDINGS[module]
    intact = spec["src"][0][module]
    facade_mats = {b[0] for b in spec["boxes"]}
    use = spec["use"]
    wallm = "eq_hub_wall_flat" if use == "flat" else "eq_hub_wall_office"

    def fn(lod):
        P = intact(lod)
        gone, heat, lift, gf = _severe_plan(module, kind, spec)
        for part in P.values():
            for m in list(part.faces):
                if m in facade_mats:
                    del part.faces[m]
            for bm, bc, bs in spec.get("bands", []):
                _drop_box_faces(part, bm, bc, bs)
            _clip_gone(part, gone)
        body = next(iter(P.values()))
        furn = Part(f"{module}_furniture") if lod == 0 else None
        rng = random.Random(zlib.crc32(f"{module}:{kind}:severe:interior:{lod}".encode()))
        for bi, (mat, c, s) in enumerate(spec["boxes"]):
            _severe_box(body, furn, module, kind, bi, DEAD_FACADE.get(mat, mat), c, s, gone, heat, lift, gf, wallm, use, lod, rng,
                        spec.get("bands", []) if bi == 0 else [])
        if furn is not None and furn.faces:
            P[furn.name] = furn
        return P
    return fn


def _facade_levels(part, face, mats):
    """_facade with a material per cell (None = open): the pieces share the face's grid vertices (no T-junctions)"""
    grid = {}
    n = face.n

    def gv(i, j):
        k = (i, j)
        if k not in grid:
            grid[k] = part.vert(face.p(face.cell_u(i), face.cell_v(j)), n)
        return grid[k]

    for m in sorted({x for col in mats for x in col if x}):
        for i0, j0, i1, j1 in _merged_rects(face.nu, face.nv, lambda i, j: mats[i][j] == m):
            ring = ([gv(i, j0) for i in range(i0, i1)] + [gv(i1, j) for j in range(j0, j1)] +
                    [gv(i, j1) for i in range(i1, i0, -1)] + [gv(i0, j) for j in range(j1, j0, -1)])
            c = part.vert(face.p((face.cell_u(i0) + face.cell_u(i1)) / 2, (face.cell_v(j0) + face.cell_v(j1)) / 2), n)
            for k in range(len(ring)):
                part.face(m, [c, ring[k], ring[(k + 1) % len(ring)]], n)


def _soot(kind, h, base):
    """the soot gradient of an inner surface by its heat: burnt at a breach's rim, sooty around, clean further in"""
    if kind == "breach" and h > 0.8:
        return BURNT
    if h > (0.4 if kind == "breach" else 0.45):
        return SOOT
    return base


def _severe_box(body, furn, module, kind, bi, fmat, c, s, gone, heat, lift, gf, wallm, use, lod, rng, bands):
    V = _Vox(c, s)
    nx, ny, nz = V.n
    dx, dy, dz = V.d
    core = set()
    if nx >= 8 and nz >= 8 and abs(nx - nz) <= 4:                       # the stair / lift core: 2 x 2 columns
        core = {(i, k) for i in (nx // 2 - 1, nx // 2) for k in (nz // 2 - 1, nz // 2)}
    pres = [[[False] * nz for _ in range(ny)] for _ in range(nx)]
    for i in range(nx):
        for k in range(nz):
            for j in range(ny):
                p = V.mid(i, j, k)
                if (i, k) in core and lift > 0:
                    q = (p[0], p[1] - lift, p[2])
                    pres[i][j][k] = (j == 0 and bi == 0) or not gone(q)
                    continue
                ok = (j == 0 and bi == 0) or not gone(p)
                if ok and j > 0 and kind == "torn" and heat(p) > 0.40 and _hash01(module, kind, bi, i, j, k, "pocket") < 0.22:
                    ok = False                                           # a rubble pocket just under the tear
                pres[i][j][k] = ok
    if kind == "torn":                    # a voxel left hanging (nothing under it, little beside it) falls too
        for j in range(1, ny):
            for i in range(nx):
                for k in range(nz):
                    if pres[i][j][k] and not pres[i][j - 1][k]:
                        side = sum(1 for di, dk in ((1, 0), (-1, 0), (0, 1), (0, -1))
                                   if 0 <= i + di < nx and 0 <= k + dk < nz and pres[i + di][j][k + dk])
                        if side < 2:
                            pres[i][j][k] = False

    def P(i, j, k):
        return V.inside(i, j, k) and pres[i][j][k]

    # ---- the facade: sub-cells (a third of a bay) open along the fine jag, a soot gradient round the opening
    sub = 3 if lod <= 1 else 1
    faces, blown = [], set()
    for fi in range(4):
        face = _Face(c, s, fi)
        face.nu *= sub
        face.cw /= sub
        nu, nv = face.nu, face.nv
        opened = [[False] * nv for _ in range(nu)]
        vox = [[None] * nv for _ in range(nu)]
        for i in range(nu):
            for j in range(nv):
                pt = face.p(face.cell_u(i) + face.cw / 2, face.cell_v(j) + face.ch / 2, 0.05)
                vi = V.index(pt)
                vox[i][j] = vi
                if bi == 0 and j == 0:
                    continue
                if not pres[vi[0]][vi[1]][vi[2]]:
                    opened[i][j] = gf(pt) or not gone(V.mid(*vi))
                else:
                    if gf(pt):
                        opened[i][j] = True
                        blown.add((vi, fi))
        mats = [[None] * nv for _ in range(nu)]
        reach = 7.0 if kind == "breach" else 3.5
        for i in range(nu):
            for j in range(nv):
                if opened[i][j]:
                    continue
                pt = face.p(face.cell_u(i) + face.cw / 2, face.cell_v(j) + face.ch / 2, 0.05)
                d = 1e9
                ri, rj = int(reach / face.cw) + 1, int(reach / face.ch) + 1
                for a in range(max(0, i - ri), min(nu, i + ri + 1)):
                    for b in range(max(0, j - rj), min(nv, j + rj + 1)):
                        if opened[a][b]:
                            d = min(d, math.hypot((abs(a - i) - 0.5) * face.cw, (abs(b - j) - 0.5) * face.ch))
                m = fmat
                if heat(pt) > 0.02:
                    if kind == "breach" and d <= 2.0:
                        m = BURNT
                    elif d <= reach * (0.7 + 0.3 * _vnoise(fi * 7 + bi, pt[0], 0.0, pt[2] + pt[1], 6.0)):   # a ragged band
                        m = SOOT
                mats[i][j] = m
        _facade_levels(body, face, mats)
        faces.append((fi, face, opened, vox))
    # the underside, per column
    if bi == 0:
        for i0, k0, i1, k1 in _merged_rects(nx, nz, lambda i, k: pres[i][0][k]):
            a, b = V.lo(i0, 0, k0), V.lo(i1, 0, k1)
            _axis_quad(body, fmat, 1, -1.0, c[1] - s[1] / 2, (a[0], 0, a[2]), (b[0], 0, b[2]))
    # the belts: per facade column where the voxel at the belt's height stands, merged into runs
    for bm, bc, bs in bands:
        for fi in range(4):
            face = _Face(c, s, fi)
            out = (bs[0] - s[0]) / 2
            ext = out if abs(face.n[2]) > 0.5 else 0.0                   # the z faces own the corners
            yb = bc[1] - face.origin[1]
            run = None
            for i in range(face.nu + 1):
                ok = False
                if i < face.nu:
                    vi = V.index(face.p(face.cell_u(i) + face.cw / 2, yb - 0.5 * FLOOR_H, 0.05))
                    ok = pres[vi[0]][vi[1]][vi[2]] and pres[vi[0]][min(ny - 1, vi[1] + 1)][vi[2]]
                if ok and run is None:
                    run = i
                if not ok and run is not None:
                    u0, u1 = face.cell_u(run), face.cell_u(i)
                    if run == 0:
                        u0 -= ext
                    if i == face.nu:
                        u1 += ext
                    pa, pb = face.p(u0, yb, -out), face.p(u1, yb, 0.0)
                    cc = tuple((pa[a] + pb[a]) / 2 for a in range(3))
                    sz = tuple(max(abs(pa[a] - pb[a]), 0.01) for a in range(3))
                    box(body, bm, cc, (sz[0], bs[1], sz[2]))
                    run = None

    # ---- open rooms
    exposed = [[[False] * nz for _ in range(ny)] for _ in range(nx)]
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                if not pres[i][j][k] or (i, k) in core:
                    continue
                e = any(V.inside(i + di, j + dj, k + dk) and not pres[i + di][j + dj][k + dk]
                        for di, dj, dk in ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1)))
                exposed[i][j][k] = e or any(((i, j, k), fi) in blown for fi in range(4))

    def E(i, j, k):
        return V.inside(i, j, k) and exposed[i][j][k]

    def hidden(i, j, k):
        return V.inside(i, j, k) and pres[i][j][k] and not exposed[i][j][k]

    # the inner side of every remaining facade piece in front of an open room or the void
    for fi, face, opened, vox in (faces if lod <= 1 else []):
        for j in range(face.nv):
            v0 = face.cell_v(j)
            run = None                                                   # (i0, material)
            for i in range(face.nu + 1):
                m = None
                if i < face.nu:
                    vi = vox[i][j]
                    if not (opened[i][j] or hidden(*vi) or (vi[0], vi[2]) in core):
                        m = _soot(kind, heat(face.p(face.cell_u(i) + face.cw / 2, v0 + face.ch / 2, WALL_IN)), wallm)
                if run and run[1] != m:
                    u0, u1 = face.cell_u(run[0]), face.cell_u(i)
                    _q(body, run[1], face.p(u0, v0, WALL_IN), face.p(u1, v0, WALL_IN),
                       face.p(u1, v0 + face.ch - SLAB_T, WALL_IN), face.p(u0, v0 + face.ch - SLAB_T, WALL_IN), _mul(face.n, -1.0))
                    run = None
                if m and not run:
                    run = (i, m)
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                if (i, k) in core:
                    if pres[i][j][k]:
                        _core_voxel(body, V, P, i, j, k, heat, lod)
                    continue
                if not exposed[i][j][k] or lod >= 2:
                    continue
                lo = V.lo(i, j, k)
                hi = V.lo(i + 1, j + 1, k + 1)
                h = heat(V.mid(i, j, k))
                wm = _soot(kind, h, wallm)
                yc = hi[1] - SLAB_T
                for axis, sd in ((0, 1), (0, -1), (2, 1), (2, -1)):
                    ni = (i + sd, j, k) if axis == 0 else (i, j, k + sd)
                    if not V.inside(*ni) or not P(*ni):
                        continue                                                # facade side / the void: open
                    plane = hi[axis] if sd > 0 else lo[axis]
                    wlo, whi = list(lo), list(hi)
                    whi[1] = yc
                    if (ni[0], ni[2]) in core or not exposed[ni[0]][ni[1]][ni[2]]:
                        _axis_quad(body, wm, axis, -sd, plane - sd * WALL_IN, wlo, whi)   # a wall towards hidden rooms
                        continue
                    same = (i // ROOM_VOX, k // ROOM_VOX) == (ni[0] // ROOM_VOX, ni[2] // ROOM_VOX)
                    if not same and lod == 0:
                        _axis_quad(body, wm, axis, -sd, plane - sd * WALL_IN, wlo, whi)   # a partition (both sides)
                above_gone = j + 1 < ny and not P(i, j + 1, k)
                if above_gone and lod <= 1 and _hash01(module, kind, bi, i, j, k, "col") < 0.08:
                    _column_stub(body, rng, lo, hi, lod)
                if above_gone and lod == 0 and _hash01(module, kind, bi, i, j, k, "rubble") < 0.2:
                    for _ in range(rng.randint(1, 2)):
                        sz = (rng.uniform(0.8, 2.6), rng.uniform(0.3, 0.9), rng.uniform(0.8, 2.2))
                        cc = (rng.uniform(lo[0] + 1.2, hi[0] - 1.2), hi[1] + sz[1] * 0.35, rng.uniform(lo[2] + 1.2, hi[2] - 1.2))
                        box(body, rng.choice((SLAB, SOOT, SLAB_U)), cc, sz,
                            mat_mul(rot_y(rng.uniform(0, math.pi)), rot_x(rng.uniform(-0.25, 0.25))))
                if furn is not None and h < 0.95 and _hash01(module, kind, bi, i, j, k, "furn") < 0.55:
                    _severe_furnish(furn, rng, use, lo, hi, h)
    _severe_slabs(body, module, kind, bi, V, pres, exposed, core, gf, heat, fmat, lod, rng)


def _severe_slabs(body, module, kind, bi, V, pres, exposed, core, gf, heat, fmat, lod, rng):
    """the floor slabs on a fine raster: each slab ends along the fine jag of its own floor (off the voxel grid) -
    broken edges, slabs sticking out over the void at different depths, broken tips hanging, rebar tufts"""
    nx, ny, nz = V.n
    dx, dy, dz = V.d
    F = {0: 1.5, 1: 2.0}.get(lod, 3.0)
    mx, mz = max(1, int(round(V.size[0] / F))), max(1, int(round(V.size[2] / F)))
    fx, fz = V.size[0] / mx, V.size[2] / mz
    x0, z0 = V.c0[0], V.c0[2]
    col = [[(min(nx - 1, int((a + 0.5) * fx / dx)), min(nz - 1, int((b + 0.5) * fz / dz))) for b in range(mz)] for a in range(mx)]

    def st(i, j, k):           # 0 out / absent, 1 exposed, 2 hidden
        if not (0 <= j < ny) or not pres[i][j][k]:
            return 0
        return 1 if exposed[i][j][k] else 2

    def near_present(i, j, k):
        for di, dk in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
            a, b = i + di, k + dk
            if 0 <= a < nx and 0 <= b < nz and 0 <= j < ny and pres[a][j][b]:
                return True
        return False

    for lev in range(ny + 1):
        y = V.c0[1] + lev * dy
        R = [[None] * mz for _ in range(mx)]
        topv = [[False] * mz for _ in range(mx)]
        undv = [[False] * mz for _ in range(mx)]
        cache = {}
        for a in range(mx):
            for b in range(mz):
                i, k = col[a][b]
                A, B = st(i, lev - 1, k), (st(i, lev, k) if lev < ny else -1)
                if (i, k) in core and (A or B > 0):
                    continue                                    # the core draws its own faces
                if lev == 0:
                    if B == 1:
                        R[a][b], topv[a][b] = True, True
                    continue
                tv = B in (0, 1) if lev < ny else True          # the top: a room's floor, over the void, or the roof
                uv = A in (0, 1)                                # the underside: a room's ceiling or over the void
                if not (tv or uv):
                    continue
                if A == 2 or B == 2:
                    R[a][b] = True                              # a hidden room behind: the slab stays
                elif A == 0 and (B <= 0) and not (near_present(i, lev - 1, k) or near_present(i, lev, k)):
                    R[a][b] = False                             # deep in the void
                else:
                    R[a][b] = not gf((x0 + (a + 0.5) * fx, y, z0 + (b + 0.5) * fz))
                topv[a][b], undv[a][b] = tv, uv
        top_m = fmat if lev == ny else SLAB
        yt = y + (0.05 if lev == 0 and bi > 0 else 0.0)            # off the roof of the box below
        for a0, b0, a1, b1 in _merged_rects(mx, mz, lambda a, b: bool(R[a][b]) and topv[a][b]):
            _axis_quad(body, top_m, 1, 1.0, yt, (x0 + a0 * fx, 0, z0 + b0 * fz), (x0 + a1 * fx, 0, z0 + b1 * fz))
        if lev > 0:
            for a0, b0, a1, b1 in _merged_rects(mx, mz, lambda a, b: bool(R[a][b]) and undv[a][b]):
                cc = (x0 + (a0 + a1) / 2 * fx, y - SLAB_T, z0 + (b0 + b1) / 2 * fz)
                m = SOOT if heat(cc) > (0.4 if kind == "breach" else 0.45) else SLAB_U
                _axis_quad(body, m, 1, -1.0, y - SLAB_T, (x0 + a0 * fx, 0, z0 + b0 * fz), (x0 + a1 * fx, 0, z0 + b1 * fz))
        if lod >= 2 or lev == 0:
            continue
        # the broken edges, merged along straight runs
        segs = {}
        for a in range(mx):
            for b in range(mz):
                if not R[a][b]:
                    continue
                for da, db, axis, sd in ((1, 0, 0, 1), (-1, 0, 0, -1), (0, 1, 2, 1), (0, -1, 2, -1)):
                    na, nb = a + da, b + db
                    if not (0 <= na < mx and 0 <= nb < mz) or R[na][nb] is not False:
                        continue
                    line = (a + (1 if da > 0 else 0)) if axis == 0 else (b + (1 if db > 0 else 0))
                    segs.setdefault((axis, sd, line), []).append(b if axis == 0 else a)
        for (axis, sd, line), lst in segs.items():
            lst.sort()
            runs, s0 = [], lst[0]
            for p, q in zip(lst, lst[1:] + [None]):
                if q != p + 1:
                    runs.append((s0, p + 1))
                    s0 = q
            for r0, r1 in runs:
                if axis == 0:
                    pl = x0 + line * fx
                    lo, hi = (pl, y - SLAB_T, z0 + r0 * fz), (pl, y, z0 + r1 * fz)
                else:
                    pl = z0 + line * fz
                    lo, hi = (x0 + r0 * fx, y - SLAB_T, pl), (x0 + r1 * fx, y, pl)
                _axis_quad(body, SLAB_U, axis, sd, pl, lo, hi)
                key = (module, kind, bi, lev, axis, sd, line, r0)
                u = 2 - axis
                if _hash01(*key, "tip") < 0.14 and hi[u] - lo[u] > 1.4:
                    _slab_tip(body, rng, axis, sd, pl, lo[u], hi[u], y, heat, kind, lod)
                elif lod == 0 and _hash01(*key, "rebar") < 0.12 and hi[u] - lo[u] > 2.0:
                    out = [0.0, 0.0, 0.0]                           # a tuft of rebar out of the broken edge
                    out[axis] = float(sd)
                    for t in range(3):
                        b0 = [0.0, y - SLAB_T / 2, 0.0]
                        b0[axis] = pl
                        b0[u] = rng.uniform(lo[u] + 0.1, hi[u] - 0.1)
                        k2 = _add(tuple(b0), _add(_mul(tuple(out), rng.uniform(0.5, 1.3)), (0.0, -rng.uniform(0.1, 1.0), 0.0)))
                        _rebar(body, tuple(b0), k2)


def _rebar(part, p0, p1):
    """a bar of rebar: an open 3-sided tube (no end caps - 6 triangles)"""
    d = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
    L = math.sqrt(_dot(d, d))
    if L > 0.05:
        lathe(part, REBAR, [(0.04, 0.0), (0.04, L)], 3, axis_to(_mul(d, 1.0 / L)), p0, smooth=False, ends=None)


def _slab_tip(body, rng, axis, sd, plane, a0, a1, y, heat, kind, lod):
    """a broken piece of slab still hanging on the edge by its rebar, angled down over the void"""
    u = 2 - axis
    w = min(a1 - a0, rng.uniform(1.2, 3.5))
    s0 = rng.uniform(a0, a1 - w)
    out = [0.0, 0.0, 0.0]
    out[axis] = float(sd)
    out = tuple(out)
    tang = [0.0, 0.0, 0.0]
    tang[u] = 1.0
    tang = tuple(tang)
    L = rng.uniform(1.2, 3.8)
    droop = math.radians(rng.uniform(25.0, 75.0))
    p0 = [0.0, y - SLAB_T, 0.0]
    p0[axis], p0[u] = plane, s0
    p0 = tuple(p0)
    p1 = _add(p0, _mul(tang, w))
    dv = _add(_mul(out, math.cos(droop)), (0.0, -math.sin(droop), 0.0))
    p2, p3 = _add(p1, _mul(dv, L)), _add(p0, _mul(dv, L))
    up = _add(_mul(out, math.sin(droop)), (0.0, math.cos(droop), 0.0))
    t = _mul(up, SLAB_T)
    m = SOOT if heat(p0) > 0.5 else SLAB
    _q(body, m, _add(p0, t), _add(p1, t), _add(p2, t), _add(p3, t), up)
    _q(body, SLAB_U, p0, p3, p2, p1, _mul(up, -1.0))
    _q(body, SLAB_U, p3, p2, _add(p2, t), _add(p3, t), dv)
    _q(body, SLAB_U, p0, p3, _add(p3, t), _add(p0, t), _mul(tang, -1.0))
    _q(body, SLAB_U, p1, p2, _add(p2, t), _add(p1, t), tang)
    if lod == 0:
        for k in range(2):
            b = _add(p0, _mul(tang, w * (k + 0.5) / 2))
            _rebar(body, _add(b, _mul(out, -0.2)), _add(b, _mul(dv, 0.5)))


def _core_voxel(body, V, P, i, j, k, heat, lod):
    """the solid concrete stair / lift core: faces only towards the void"""
    lo, hi = V.lo(i, j, k), V.lo(i + 1, j + 1, k + 1)
    m = BURNT if heat(V.mid(i, j, k)) > 0.7 else SLAB
    for axis, sd in ((0, 1), (0, -1), (1, 1), (1, -1), (2, 1), (2, -1)):
        ni = [i, j, k]
        ni[axis] += sd
        if not V.inside(*ni) or P(*ni):
            continue
        _axis_quad(body, m, axis, sd, hi[axis] if sd > 0 else lo[axis], lo, hi)
    if lod == 0 and V.inside(i, j + 1, k) and not P(i, j + 1, k):            # rebar out of the core's broken top
        for a in range(4):
            x = lo[0] + 0.6 + (hi[0] - lo[0] - 1.2) * (a % 2)
            z = lo[2] + 0.6 + (hi[2] - lo[2] - 1.2) * (a // 2)
            cyl(body, REBAR, (x, hi[1], z), (x + 0.4 * (a % 2 - 0.5), hi[1] + 1.6, z + 0.5 * (a // 2 - 0.5)), 0.05, 4)


def _column_stub(body, rng, lo, hi, lod):
    """a broken column standing out of a torn floor, its top bent"""
    x = rng.choice((lo[0] + 0.4, hi[0] - 0.4))
    z = rng.choice((lo[2] + 0.4, hi[2] - 0.4))
    h1 = rng.uniform(1.0, 3.2)
    box(body, SLAB, (x, hi[1] + h1 / 2, z), (0.6, h1, 0.6))
    if lod == 0:
        a = rng.uniform(0, 2 * math.pi)
        for t in range(4):
            b = (x + 0.2 * (t % 2 * 2 - 1), hi[1] + h1, z + 0.2 * (t // 2 * 2 - 1))
            cyl(body, REBAR, b, (b[0] + math.cos(a) * 0.9, b[1] + rng.uniform(0.3, 1.0), b[2] + math.sin(a) * 0.9), 0.035, 4)


def _severe_furnish(furn, rng, use, lo, hi, h):
    """LOD0: one voxel's furniture (1-2 pieces), burnt / tipped near the wound"""
    pool = _room_items(use, rng)[:2]
    taken = []
    for (w, ht, d, mat, tag) in pool:
        for _try in range(5):
            yaw = rng.choice((0.0, math.pi / 2))
            ex, ez = (w, d) if yaw == 0.0 else (d, w)
            cx = rng.uniform(lo[0] + ex / 2 + 0.4, hi[0] - ex / 2 - 0.4)
            cz = rng.uniform(lo[2] + ez / 2 + 0.4, hi[2] - ez / 2 - 0.4)
            if all(abs(cx - a) > (ex + ae) / 2 + 0.2 or abs(cz - b) > (ez + be) / 2 + 0.2 for a, b, ae, be in taken):
                break
        else:
            continue
        taken.append((cx, cz, max(ex, ht), max(ez, ht)))
        m = "eq_furn_burnt" if (h > 0.4 and rng.random() < 0.8) else mat
        tipped = h > 0.25 and rng.random() < 0.45
        R = rot_y(yaw)
        hh = ht
        if tipped:
            R = mat_mul(R, rot_z(math.pi / 2))
            hh = w
        y0 = lo[1]
        box(furn, m, (cx, y0 + hh / 2, cz), (w, ht, d), R)
        if tag == "desk" and not tipped:
            box(furn, "eq_furn_burnt" if h > 0.45 else SCREEN, (cx, y0 + ht + 0.25, cz), (0.55, 0.35, 0.05), R)
            box(furn, "eq_furn_metal", (cx + 0.9 * math.sin(yaw), y0 + 0.25, cz + 0.9 * math.cos(yaw)), (0.5, 0.5, 0.5), R)
        elif tag == "bed" and not tipped:
            box(furn, "eq_furn_white" if m == mat else m, (cx, y0 + ht + 0.08, cz), (w * 0.95, 0.16, d * 0.95), R)


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
        ASSETS[_id] = _severe_builder(_m, _k) if _k in SEVERE else _damaged_builder(_m, _k)
        HITS[_id] = _spec["src"][1][_m]
        ROOTS[_id] = "hub"
        PASSPORTS[_id] = (f"{_m} - повреждён: {_title}", "hub_module",
                          "Повреждённое здание хаба (tools/models/units/hub_damage.py): пробитый фасад, за ним секции, "
                          "этажи, комнаты; мебель на LOD0 отдельной группой.")
        DAMAGED_OF.setdefault(_m, []).append(_id)
assert all(k in [x for x, _ in KINDS] for k in SEVERE), "hub_damage_names.KINDS lacks a severe kind"
