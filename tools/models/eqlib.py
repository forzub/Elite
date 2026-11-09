"""Equipment model kernel (tools/models): geometry primitives, LOD policy, shared reactor builders (cooling pad,
heat-pipe column), hit-volume helpers, family scaling, OBJ/MTL writers. Units live in tools/models/units/<maker>.py and
are collected by tools/models/build_equipment_models.py.

LOD policy (author 2026-11-10: "shift the LODs by one"):
  asset LOD0  - close inspection (a few metres; drone / repair views): everything of LOD1 plus fine detail - small bolts,
                chamfers, cable harnesses, pipes, clamps (close() is True, visibility floor off, finer segments);
  asset LOD1  - the look from ~20-30 m (the former LOD0): visibility floor MIN_BOLT 6 cm / MIN_FEATURE 2.5 cm, 32 segments;
  asset LOD2  - ~30-150 m (the former LOD1), 16 segments, no fasteners or panels;
  asset LOD3  - beyond ~150 m (the former LOD2), 12 segments, plain volumes.
Unit functions are written against the mid levels lod = 0 / 1 / 2 (= asset LOD1 / 2 / 3); asset LOD0 calls them with
lod = 0 while close() is True - units add their close-up extras under `if close():`. Use seg_for(lod) / small_for(lod)
for segment counts so the close level refines automatically.
Budgets per unit: LOD0 <= ~40k triangles, LOD1 ~10-14k, LOD2 ~2-4k, LOD3 < ~1.5k.
"""
import json
import math
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "assets", "models", "equipment")

# Visibility floor (author 2026-11-09: the models must stay light). LOD0 is the look from ~20-30 m, where one pixel
# (1080p, 60 deg FOV) is ~3 cm: chamfers below MIN_FEATURE and fasteners below MIN_BOLT across are not built at all.
MIN_FEATURE = 0.025
MIN_BOLT = 0.06
_CLOSE = [False]          # asset LOD0 (close inspection) in progress


def close():
    """True while the close-inspection asset LOD0 is being built (units add harnesses, pipes, fine fasteners)"""
    return _CLOSE[0]


def min_feature():
    return 0.0 if _CLOSE[0] else MIN_FEATURE


def min_bolt():
    return 0.0 if _CLOSE[0] else MIN_BOLT


def seg_for(lod):
    """round segment count of the big lathes (mid levels 0 / 1 / 2; 48 on the close level)"""
    return 48 if _CLOSE[0] else {0: 32, 1: 16, 2: 12}[lod]


def small_for(lod):
    return 16 if _CLOSE[0] else {0: 12, 1: 8, 2: 6}[lod]


def register_materials(mats):
    """units add their school palettes here: name -> (Kd, Ks, Ns); a name may be registered twice only identically"""
    for k, v in mats.items():
        if k in MATERIALS and MATERIALS[k] != v:
            raise ValueError(f"material {k} registered twice with different values")
        MATERIALS[k] = v

MATERIALS = {
    # name: (Kd, Ks, Ns) - stable asset-level ids
    "eq_metal_graphite": ((0.34, 0.36, 0.38), (0.25, 0.25, 0.25), 60),
    "eq_metal_band": ((0.20, 0.21, 0.23), (0.30, 0.30, 0.30), 80),
    "eq_metal_panel": ((0.47, 0.48, 0.49), (0.20, 0.20, 0.20), 40),
    "eq_steel": ((0.58, 0.59, 0.61), (0.55, 0.55, 0.55), 120),
    "eq_pad_ceramic": ((0.07, 0.07, 0.075), (0.05, 0.05, 0.05), 10),
    "eq_field_coil": ((0.32, 0.23, 0.14), (0.35, 0.30, 0.25), 70),
    "eq_copper": ((0.66, 0.38, 0.20), (0.60, 0.45, 0.35), 90),
    "eq_insulator": ((0.80, 0.78, 0.72), (0.10, 0.10, 0.10), 20),
    "eq_status_amber": ((0.95, 0.62, 0.15), (0.20, 0.20, 0.20), 30),
}


# ---------------------------------------------------------------- tiny geometry kernel
def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def apply(R, v):
    return (R[0][0] * v[0] + R[0][1] * v[1] + R[0][2] * v[2],
            R[1][0] * v[0] + R[1][1] * v[1] + R[1][2] * v[2],
            R[2][0] * v[0] + R[2][1] * v[1] + R[2][2] * v[2])


I3 = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return [[1, 0, 0], [0, c, -s], [0, s, c]]


def axis_to(direction):
    """rotation taking local +Z onto a unit direction"""
    x, y, z = direction
    if abs(z - 1) < 1e-9:
        return I3
    if abs(z + 1) < 1e-9:
        return rot_x(math.pi)
    a = math.acos(max(-1.0, min(1.0, z)))
    k = (-y, x, 0.0)
    kl = math.hypot(k[0], k[1])
    k = (k[0] / kl, k[1] / kl, 0.0)
    c, s, t = math.cos(a), math.sin(a), 1 - math.cos(a)
    kx, ky, kz = k
    return [[t * kx * kx + c, t * kx * ky - s * kz, t * kx * kz + s * ky],
            [t * kx * ky + s * kz, t * ky * ky + c, t * ky * kz - s * kx],
            [t * kx * kz - s * ky, t * ky * kz + s * kx, t * kz * kz + c]]


def norm(v):
    l = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) or 1.0
    return (v[0] / l, v[1] / l, v[2] / l)


class Part:
    def __init__(self, name):
        self.name = name
        self.v, self.n = [], []
        self.faces = {}   # material -> [index tuples]

    def vert(self, p, n, R=I3, t=(0, 0, 0)):
        q = apply(R, p)
        self.v.append((q[0] + t[0], q[1] + t[1], q[2] + t[2]))
        self.n.append(norm(apply(R, n)))
        return len(self.v) - 1

    def face(self, mat, idx, hint):
        """hint: the intended outward normal (world space); the winding is flipped to match it"""
        idx = [i for k, i in enumerate(idx) if self.v[i] != self.v[idx[k - 1]]]
        if len(idx) < 3:
            return
        nx = ny = nz = 0.0
        for k in range(len(idx)):
            a, b = self.v[idx[k]], self.v[idx[(k + 1) % len(idx)]]
            nx += (a[1] - b[1]) * (a[2] + b[2])
            ny += (a[2] - b[2]) * (a[0] + b[0])
            nz += (a[0] - b[0]) * (a[1] + b[1])
        if nx * hint[0] + ny * hint[1] + nz * hint[2] < 0:
            idx = idx[::-1]
        self.faces.setdefault(mat, []).append(tuple(idx))

    def tris(self):
        return sum(len(f) - 2 for fl in self.faces.values() for f in fl)


def lathe(part, mat, profile, seg, R=I3, t=(0, 0, 0), a0=0.0, a1=2 * math.pi, smooth=True, cap=False, soft=0.0):
    """revolve a (r, z) profile about local Z. Profile order: outward normal = (dz, -dr) of each segment
    (bottom -> outer -> top for solids on the axis; counter-clockwise loops for rings). Every profile segment is its
    own strip (hard edges at profile corners). cap: close the ends of a partial revolution (convex profile).
    soft (degrees): profile corners flatter than this get shared (averaged) normals - a rounded profile reads smooth."""
    full = abs(a1 - a0 - 2 * math.pi) < 1e-9
    na = seg if full else seg + 1
    angs = [a0 + (a1 - a0) * i / seg for i in range(na)]
    pairs = [(i, (i + 1) % na) for i in range(na if full else na - 1)]
    W = lambda v: apply(R, v)
    segn = []
    for k in range(len(profile) - 1):
        (r0, z0), (r1, z1) = profile[k], profile[k + 1]
        L = math.hypot(r1 - r0, z1 - z0)
        segn.append(((z1 - z0) / L, -(r1 - r0) / L) if L > 1e-9 else None)
    closed = profile[0] == profile[-1]
    cos_soft = math.cos(math.radians(soft)) if soft > 0 else 2.0

    def blend(k, other):
        if other < 0 or other >= len(segn):
            if not closed:
                return segn[k]
            other %= len(segn)
        a, b = segn[k], segn[other]
        if b is None or a[0] * b[0] + a[1] * b[1] < cos_soft:
            return a
        l = math.hypot(a[0] + b[0], a[1] + b[1])
        return ((a[0] + b[0]) / l, (a[1] + b[1]) / l)

    for k in range(len(profile) - 1):
        (r0, z0), (r1, z1) = profile[k], profile[k + 1]
        dr, dz = r1 - r0, z1 - z0
        L = math.hypot(dr, dz)
        if L < 1e-9:
            continue
        nr, nz = dz / L, -dr / L
        if smooth:
            n0, n1 = blend(k, k - 1), blend(k, k + 1)
            ring0 = [part.vert((r0 * math.cos(a), r0 * math.sin(a), z0), (n0[0] * math.cos(a), n0[0] * math.sin(a), n0[1]), R, t) for a in angs]
            ring1 = [part.vert((r1 * math.cos(a), r1 * math.sin(a), z1), (n1[0] * math.cos(a), n1[0] * math.sin(a), n1[1]), R, t) for a in angs]
            for i, j in pairs:
                am = angs[i] + ((angs[j] - angs[i]) % (2 * math.pi)) / 2
                part.face(mat, [ring0[i], ring0[j], ring1[j], ring1[i]], W((nr * math.cos(am), nr * math.sin(am), nz)))
        else:
            for i, j in pairs:
                ai, aj = angs[i], angs[j]
                pts = [(r0 * math.cos(ai), r0 * math.sin(ai), z0), (r0 * math.cos(aj), r0 * math.sin(aj), z0),
                       (r1 * math.cos(aj), r1 * math.sin(aj), z1), (r1 * math.cos(ai), r1 * math.sin(ai), z1)]
                am = ai + ((aj - ai) % (2 * math.pi)) / 2
                hn = (nr * math.cos(am), nr * math.sin(am), nz)
                # flat face normal: the true facet normal
                fn = facet_normal(pts) or hn
                if fn[0] * hn[0] + fn[1] * hn[1] + fn[2] * hn[2] < 0:
                    fn = (-fn[0], -fn[1], -fn[2])
                ids = [part.vert(p, fn, R, t) for p in pts]
                part.face(mat, ids, W(fn))
    if cap and not full:
        pts = profile[:-1] if profile[0] == profile[-1] else profile
        for a, sgn in ((a0, -1), (a1, 1)):
            n = (-math.sin(a) * sgn, math.cos(a) * sgn, 0.0)
            ids = [part.vert((r * math.cos(a), r * math.sin(a), z), n, R, t) for r, z in pts]
            part.face(mat, ids, W(n))


def facet_normal(pts):
    nx = ny = nz = 0.0
    for k in range(len(pts)):
        a, b = pts[k], pts[(k + 1) % len(pts)]
        nx += (a[1] - b[1]) * (a[2] + b[2])
        ny += (a[2] - b[2]) * (a[0] + b[0])
        nz += (a[0] - b[0]) * (a[1] + b[1])
    l = math.sqrt(nx * nx + ny * ny + nz * nz)
    return (nx / l, ny / l, nz / l) if l > 1e-12 else None


def box(part, mat, center, size, R=I3, chamfer=0.0):
    """axis-aligned (in its own frame R) box; chamfer > 0 bevels the 12 edges (a chamfered box = 26 faces)"""
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
    if chamfer < min_feature():
        chamfer = 0.0
    if chamfer <= 0:
        for ax in range(3):
            for s in (-1, 1):
                n = [0, 0, 0]
                n[ax] = s
                u, w = [a for a in range(3) if a != ax]
                h = (hx, hy, hz)
                pts = []
                for cu, cw in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    p = [0, 0, 0]
                    p[ax] = s * h[ax]
                    p[u] = cu * h[u]
                    p[w] = cw * h[w]
                    pts.append(tuple(p))
                ids = [part.vert(p, tuple(n), R, center) for p in pts]
                part.face(mat, ids, apply(R, n))
        return
    c = min(chamfer, hx * 0.45, hy * 0.45, hz * 0.45)
    h = (hx, hy, hz)
    # each face: inset rectangle; each edge: a chamfer strip; each corner: a triangle
    for ax in range(3):
        u, w = [a for a in range(3) if a != ax]
        for s in (-1, 1):
            n = [0, 0, 0]
            n[ax] = s
            pts = []
            for cu, cw in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                p = [0, 0, 0]
                p[ax] = s * h[ax]
                p[u] = cu * (h[u] - c)
                p[w] = cw * (h[w] - c)
                pts.append(tuple(p))
            part.face(mat, [part.vert(p, tuple(n), R, center) for p in pts], apply(R, n))
    for a in range(3):          # edges parallel to axis a
        u, w = [x for x in range(3) if x != a]
        for su in (-1, 1):
            for sw in (-1, 1):
                n = [0, 0, 0]
                n[u], n[w] = su, sw
                pts = []
                for sa in (-1, 1):
                    p1 = [0, 0, 0]
                    p1[a] = sa * (h[a] - c)
                    p1[u] = su * h[u]
                    p1[w] = sw * (h[w] - c)
                    p2 = list(p1)
                    p2[u] = su * (h[u] - c)
                    p2[w] = sw * h[w]
                    pts += [tuple(p1), tuple(p2)]
                q = [pts[0], pts[1], pts[3], pts[2]]
                part.face(mat, [part.vert(p, norm(tuple(n)), R, center) for p in q], apply(R, norm(tuple(n))))
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                s = (sx, sy, sz)
                n = norm(s)
                pts = []
                for ax in range(3):
                    p = [s[i] * (h[i] - c) for i in range(3)]
                    p[ax] = s[ax] * h[ax]
                    pts.append(tuple(p))
                part.face(mat, [part.vert(p, n, R, center) for p in pts], apply(R, n))


def cyl(part, mat, p0, p1, r, seg, smooth=True, caps=True, chamfer=0.0):
    """a capped cylinder from p0 to p1 (world points)"""
    d = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
    L = math.sqrt(sum(x * x for x in d))
    R = axis_to(norm(d))
    c = min(chamfer, r * 0.4, L * 0.4) if chamfer >= min_feature() else 0.0
    if caps:
        prof = [(0, 0), (r - c, 0), (r, c), (r, L - c), (r - c, L), (0, L)] if c > 0 else [(0, 0), (r, 0), (r, L), (0, L)]
    else:
        prof = [(r, 0), (r, L)]
    lathe(part, mat, prof, seg, R, p0, smooth=smooth)


def crect(r0, r1, z0, z1, c=0.0):
    """closed counter-clockwise (r, z) rectangle with chamfered corners (a ring cross-section)"""
    if c < min_feature():
        return [(r0, z0), (r1, z0), (r1, z1), (r0, z1), (r0, z0)]
    return [(r0 + c, z0), (r1 - c, z0), (r1, z0 + c), (r1, z1 - c), (r1 - c, z1), (r0 + c, z1), (r0, z1 - c), (r0, z0 + c), (r0 + c, z0)]


def hex_bolt(part, mat, p, direction, across=0.09, height=0.06, washer=False):
    """a hex head (6 flat sides + top, ~18 triangles; the underside sits on the surface and is not built).
    washer: a thin 6-sided washer under the head (+12 triangles)"""
    if across < min_bolt():
        return
    R = axis_to(norm(direction))
    r = across / math.sqrt(3)
    base = 0.0
    if washer:
        base = height * 0.25
        lathe(part, mat, [(r * 1.35, 0), (r * 1.35, base), (r, base)], 6, R, p, smooth=False)
    lathe(part, mat, [(r, base), (r, base + height), (0, base + height)], 6, R, p, smooth=False)




# ---------------------------------------------------------------- helpers for units built about the vertical axis
R_Y = rot_x(-math.pi / 2)   # lathe local Z -> world +Y; a world XZ angle th (from +X towards +Z) is local angle -th


def ylathe(part, mat, prof, seg, th0=0.0, th1=2 * math.pi, t=(0, 0, 0), **kw):
    """revolve a (r, y) profile about the world Y axis (through t), world angles th0..th1 measured from +X to +Z"""
    lathe(part, mat, prof, seg, R_Y, t, -th1, -th0, **kw)


def polar(r, th, y):
    return (r * math.cos(th), y, r * math.sin(th))


def rrect(r0, r1, z0, z1, rad, n):
    """closed counter-clockwise (r, z) rectangle with round corners of radius rad, n segments per corner"""
    pts = []
    for (cr, cz), s in (((r1 - rad, z0 + rad), -90), ((r1 - rad, z1 - rad), 0), ((r0 + rad, z1 - rad), 90), ((r0 + rad, z0 + rad), 180)):
        for i in range(n + 1):
            a = math.radians(s + 90 * i / n)
            pts.append((cr + rad * math.cos(a), cz + rad * math.sin(a)))
    return [pts[-1]] + pts


def inset(poly, d):
    """convex (x, z) polygon offset inwards by d (each edge moved in, neighbours intersected)"""
    n = len(poly)
    cx, cz = sum(p[0] for p in poly) / n, sum(p[1] for p in poly) / n
    lines = []
    for i in range(n):
        (x0, z0), (x1, z1) = poly[i], poly[(i + 1) % n]
        ex, ez = x1 - x0, z1 - z0
        L = math.hypot(ex, ez)
        nx, nz = ez / L, -ex / L
        if (cx - x0) * nx + (cz - z0) * nz < 0:
            nx, nz = -nx, -nz
        lines.append(((x0 + nx * d, z0 + nz * d), (ex, ez)))
    out = []
    for i in range(n):
        (p, u), (q, v) = lines[i - 1], lines[i]
        den = u[0] * v[1] - u[1] * v[0]
        s = ((q[0] - p[0]) * v[1] - (q[1] - p[1]) * v[0]) / den
        out.append((p[0] + u[0] * s, p[1] + u[1] * s))
    return out


def prism(part, mat, poly, y0, y1):
    """a vertical prism over a convex (x, z) polygon"""
    n = len(poly)
    cx, cz = sum(p[0] for p in poly) / n, sum(p[1] for p in poly) / n
    part.face(mat, [part.vert((x, y1, z), (0, 1, 0)) for x, z in poly], (0, 1, 0))
    part.face(mat, [part.vert((x, y0, z), (0, -1, 0)) for x, z in poly], (0, -1, 0))
    for i in range(n):
        (x0, z0), (x1, z1) = poly[i], poly[(i + 1) % n]
        nx, nz = z1 - z0, -(x1 - x0)
        if ((x0 + x1) / 2 - cx) * nx + ((z0 + z1) / 2 - cz) * nz < 0:
            nx, nz = -nx, -nz
        nn = norm((nx, 0, nz))
        part.face(mat, [part.vert(p, nn) for p in ((x0, y0, z0), (x1, y0, z1), (x1, y1, z1), (x0, y1, z0))], nn)


def placed(src, R, t):
    """a copy of a part moved by rotation R and translation t (det R = +1, windings stay valid)"""
    out = Part(src.name)
    out.v = [tuple(a + b for a, b in zip(apply(R, v), t)) for v in src.v]
    out.n = [norm(apply(R, n)) for n in src.n]
    out.faces = {m: list(f) for m, f in src.faces.items()}
    return out




def polz(r, a, z):
    return (r * math.cos(a), r * math.sin(a), z)




def heat_column(hc, lod, seg, small, y0, y1, band="eq_metal_band", pipe="eq_steel", core="eq_metal_graphite"):
    """heat-pipe column from a unit top (y0) to its pad (y1): saddle, 7 heat pipes, collars, top plate (shared by all reactors)"""
    ro = y0
    box(hc, band, (0, ro + 0.02, 0), (0.80, 0.16, 1.30), chamfer=0.03 if lod == 0 else 0.0)            # saddle
    ylathe(hc, band, [(0, y0), (0.50, y0), (0.50, y0 + 0.16), (0, y0 + 0.16)], seg)
    pipes = [(0.0, 0.0, 0.20)] + [(0.34 * math.cos(2 * math.pi * i / 6), 0.34 * math.sin(2 * math.pi * i / 6), 0.08) for i in range(6)]
    for x, z, r in pipes if lod == 0 else pipes[:1]:
        cyl(hc, pipe if r < 0.1 else core, (x, y0 + 0.16, z), (x, y1 - 0.08, z), r if lod == 0 else 0.36, small)
    if lod < 2:
        for yy in (y0 + 0.36, y0 + 0.58):
            ylathe(hc, band, crect(0.15, 0.46, yy - 0.03, yy + 0.03, 0.01 if lod == 0 else 0.0), seg)
    ylathe(hc, band, [(0, y1 - 0.08), (0.65, y1 - 0.08), (0.65, y1), (0, y1)], seg)
    if lod == 0:
        for i in range(8):
            a = 2 * math.pi * (i + 0.5) / 8
            hex_bolt(hc, pipe, polar(0.42, a, y0 + 0.16), (0, 1, 0), 0.04, 0.025, washer=False)
            hex_bolt(hc, pipe, polar(0.56, a, y1 - 0.08), (0, -1, 0), 0.04, 0.025, washer=False)
        for sx in (-1, 1):
            for sz in (-1, 1):
                hex_bolt(hc, pipe, (sx * 0.32, ro + 0.10, sz * 0.55), (0, 1, 0), 0.05, 0.03, washer=False)


def cooling_pad(pd, lod, py0, pt, L, frame="eq_metal_band", ceramic="eq_pad_ceramic", backing="eq_metal_graphite", steel="eq_steel"):
    """square cooling pad (side 2L, thickness pt, underside at py0): rim, midline and diagonal beams, 8 triangular sections"""
    py1 = py0 + pt
    bw = 0.22
    if lod == 2:
        box(pd, ceramic, (0, (py0 + py1) / 2, 0), (2 * L, pt, 2 * L))     # far pad: one slab
    else:
        box(pd, backing, (0, py0 + 0.02, 0), (2 * L - 0.2, 0.04, 2 * L - 0.2))                  # backing sheet
        for s in (-1, 1):                                                                                  # rim beams
            box(pd, frame, (s * (L - bw / 2), (py0 + py1) / 2, 0), (bw, pt, 2 * L), chamfer=0.02 if lod == 0 else 0.0)
            box(pd, frame, (0, (py0 + py1) / 2, s * (L - bw / 2)), (2 * L - 2 * bw, pt, bw), chamfer=0.02 if lod == 0 else 0.0)
        for th in (0.0, math.pi / 2):                                                                      # midline beams
            box(pd, frame, (0, (py0 + py1) / 2, 0), (2 * L - 2 * bw, pt - 0.01, 0.18), rot_y(th))
        for th in (math.pi / 4, -math.pi / 4):                                                             # diagonal beams
            box(pd, frame, (0, (py0 + py1) / 2, 0), (2 * L * math.sqrt(2) - 0.6, pt - 0.01, 0.18), rot_y(th))
        box(pd, frame, (0, py1 - 0.10, 0), (1.0, 0.30, 1.0), chamfer=0.04 if lod == 0 else 0.0)  # centre hub
        corners = [(sx * L, sz * L) for sx, sz in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
        for x, z in corners:                                                                               # corner blocks
            box(pd, frame, (x - math.copysign(0.24, x), (py0 + py1) / 2 + 0.03, z - math.copysign(0.24, z)), (0.50, pt + 0.08, 0.50),
                chamfer=0.04 if lod == 0 else 0.0)
        for x, z in ((L, 0), (-L, 0), (0, L), (0, -L)):                                                    # edge-mid blocks
            box(pd, frame, (x - math.copysign(0.2, x) if x else 0, (py0 + py1) / 2 + 0.02, z - math.copysign(0.2, z) if z else 0),
                (0.42 if x else 0.60, pt + 0.04, 0.60 if x else 0.42), chamfer=0.03 if lod == 0 else 0.0)
        if lod < 2:
            for k in range(8):                                                                             # 8 sections
                ca = corners[k // 2]
                mid = (ca[0], 0.0) if k % 2 == 0 else (0.0, ca[1])
                prism(pd, ceramic, inset([(0.0, 0.0), ca, mid], 0.13), py0 + 0.04, py1 - 0.012)
        else:
            box(pd, ceramic, (0, py1 - 0.06, 0), (2 * L - 0.3, 0.10, 2 * L - 0.3))
        if lod == 0:
            def bolts_along(p0, p1, step):
                n = max(1, int(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / step))
                for i in range(1, n):
                    t = i / n
                    hex_bolt(pd, steel, (p0[0] + (p1[0] - p0[0]) * t, py1 - 0.005, p0[1] + (p1[1] - p0[1]) * t), (0, 1, 0), 0.05, 0.025, washer=False)
            for x, z in corners:
                bolts_along((math.copysign(0.75, x), math.copysign(0.75, z)), (x - math.copysign(0.6, x), z - math.copysign(0.6, z)), 1.0)
            for x, z in ((L, 0), (-L, 0), (0, L), (0, -L)):
                bolts_along((math.copysign(0.6, x) if x else 0, math.copysign(0.6, z) if z else 0), (x - math.copysign(0.5, x) if x else 0, z - math.copysign(0.5, z) if z else 0), 1.0)
            for s in (-1, 1):                                                                              # rim bolts + side slots
                for i in range(10):
                    q = -L + 0.85 + i * (2 * L - 1.7) / 9
                    hex_bolt(pd, steel, (s * (L - bw / 2), py1, q), (0, 1, 0), 0.05, 0.025, washer=False)
                    hex_bolt(pd, steel, (q, py1, s * (L - bw / 2)), (0, 1, 0), 0.05, 0.025, washer=False)
                for q in (-L / 2, L / 2):
                    box(pd, backing, (s * L + s * 0.004, (py0 + py1) / 2, q), (0.01, 0.08, 1.6))
                    box(pd, backing, (q, (py0 + py1) / 2, s * L + s * 0.004), (1.6, 0.08, 0.01))
            for x, z in corners:                                                                           # lifting eyes
                lathe(pd, steel, crect(0.05, 0.09, -0.03, 0.03), 8, axis_to((1, 0, 0)),
                      (x - math.copysign(0.24, x), py1 + 0.14, z - math.copysign(0.24, z)))
            for i in range(4):
                hex_bolt(pd, steel, (0.32 * (1 if i % 2 else -1), py1 + 0.05, 0.32 * (1 if i // 2 else -1)), (0, 1, 0), 0.07, 0.035)




# ---------------------------------------------------------------- hit volumes
# Author 2026-11-09: hit volumes are drawn together with the model, as close to its shape as boxes and cylinders allow;
# gameplay keeps the hit volumes, not the render mesh. Format = the engine CollisionVolume (src/model_asset/ModelAsset.h):
# localPosition, localRotationDeg (applied as T * Rx * Ry * Rz), halfSize for a box; radius + halfHeight along local +Y
# for a cylinder (CollisionShape::Cylinder). The editor imports the file in PHYSICS ("SOURCE HITS").
# "part" = the OBJ part (semantic node / module) the volume belongs to; same OBJ frame and metres as the model.
def hbox(part, center, size, rot=(0.0, 0.0, 0.0)):
    return {"part": part, "shape": "box", "position": list(center), "rotation_deg": list(rot),
            "half_size": [size[0] / 2, size[1] / 2, size[2] / 2]}


def hcyl(part, p0, p1, r):
    """a cylinder between two points; only axis-aligned (X/Y/Z) or XY-plane radial axes are needed here"""
    c = [(a + b) / 2 for a, b in zip(p0, p1)]
    dx, dy, dz = (b - a for a, b in zip(p0, p1))
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    if abs(dz) > 1e-9 and abs(dx) < 1e-9 and abs(dy) < 1e-9:
        rot = (90.0, 0.0, 0.0)                                   # local +Y -> +Z
    elif abs(dz) < 1e-9:
        rot = (0.0, 0.0, math.degrees(math.atan2(dy, dx)) - 90.0)  # local +Y -> (dx, dy, 0)
    else:
        raise ValueError("hcyl: axis must be along Z or in the XY plane")
    return {"part": part, "shape": "cylinder", "position": c, "rotation_deg": list(rot), "radius": r, "half_height": L / 2}




def scaled_family(base_fn, D, s, pad_side, rename=None):
    def fn(lod):
        saved = dict(D)
        D["pad_half"] = pad_side / 2 / s
        try:
            P = base_fn(lod)
        finally:
            D.clear()
            D.update(saved)
        out = {}
        for k, part in P.items():
            q = Part(part.name)
            q.v = [(x * s, y * s, z * s) for x, y, z in part.v]
            q.n = list(part.n)
            q.faces = part.faces
            name = k
            for a, b in (rename or {}).items():
                name = name.replace(a, b)
            q.name = name
            out[name] = q
        return out
    return fn


def scaled_hits(base_hits, D, s, pad_side, rename=None):
    def fn():
        saved = dict(D)
        D["pad_half"] = pad_side / 2 / s
        try:
            H = base_hits()
        finally:
            D.clear()
            D.update(saved)
        for v in H:
            v["position"] = [x * s for x in v["position"]]
            if "half_size" in v:
                v["half_size"] = [x * s for x in v["half_size"]]
            for k in ("radius", "half_height"):
                if k in v:
                    v[k] *= s
            for a, b in (rename or {}).items():
                v["part"] = v["part"].replace(a, b)
        return H
    return fn




# ---------------------------------------------------------------- close-up detail helpers (asset LOD0)
def bezier(p0, p1, p2, p3, n):
    """n+1 points of a cubic Bezier"""
    out = []
    for i in range(n + 1):
        t = i / n
        a, b, c, d = (1 - t) ** 3, 3 * t * (1 - t) ** 2, 3 * t * t * (1 - t), t ** 3
        out.append(tuple(a * p0[k] + b * p1[k] + c * p2[k] + d * p3[k] for k in range(3)))
    return out


def sag(p0, p1, droop, n, down=(0.0, -1.0, 0.0)):
    """a hanging cable between two points: a parabola sagging by droop along `down`"""
    out = []
    for i in range(n + 1):
        t = i / n
        s = 4 * t * (1 - t) * droop
        out.append(tuple(p0[k] + (p1[k] - p0[k]) * t + down[k] * s for k in range(3)))
    return out


def tube(part, mat, pts, r, seg=8):
    """a smooth tube swept along a polyline (parallel-transport frames, open ends)"""
    if len(pts) < 2:
        return
    def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
    def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
    tans = []
    for i in range(len(pts)):
        a, b = pts[max(i - 1, 0)], pts[min(i + 1, len(pts) - 1)]
        tans.append(norm(sub(b, a)))
    ref = (0.0, 1.0, 0.0) if abs(tans[0][1]) < 0.9 else (1.0, 0.0, 0.0)
    nrm = norm(cross(tans[0], ref))
    rings = []
    for i, p in enumerate(pts):
        t = tans[i]
        nrm = norm(sub(nrm, tuple(t[k] * dot(nrm, t) for k in range(3))))
        bi = cross(t, nrm)
        ring = []
        for j in range(seg):
            a = 2 * math.pi * j / seg
            d = tuple(nrm[k] * math.cos(a) + bi[k] * math.sin(a) for k in range(3))
            ring.append((part.vert(tuple(p[k] + d[k] * r for k in range(3)), d), d))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for j in range(seg):
            j2 = (j + 1) % seg
            (a, da), (b, db) = rings[i][j], rings[i][j2]
            (c, _), (e, _) = rings[i + 1][j2], rings[i + 1][j]
            part.face(mat, [a, b, c, e], norm(tuple(da[k] + db[k] for k in range(3))))


def harness(part, pts, r=0.025, n=3, mats=("eq_insulator",), ties="eq_steel", tie_every=4, seg=6):
    """a cable bundle of n strands along a path, with tie wraps; the path is the bundle centre line"""
    for s in range(n):
        a = 2 * math.pi * s / n
        off = (math.cos(a) * r * 1.1, math.sin(a) * r * 1.1, 0.0) if n > 1 else (0.0, 0.0, 0.0)
        strand = [(p[0] + off[0], p[1] + off[1], p[2] + off[2]) for p in pts]
        tube(part, mats[s % len(mats)], strand, r, seg)
    for i in range(tie_every // 2, len(pts) - 1, tie_every):
        p, q = pts[i], pts[i + 1]
        d = norm((q[0] - p[0], q[1] - p[1], q[2] - p[2]))
        R = axis_to(d)
        lathe(part, ties, crect(r * (1.0 if n == 1 else 2.0), r * (1.0 if n == 1 else 2.0) + 0.008, -0.01, 0.01), 8, R, p)


def cyl_path(r, a0, a1, z0, z1, n=16):
    """points on a cylinder about Z (radius r) from angle a0 / z0 to a1 / z1 - a cable laid along a drum"""
    return [(r * math.cos(a0 + (a1 - a0) * i / n), r * math.sin(a0 + (a1 - a0) * i / n), z0 + (z1 - z0) * i / n) for i in range(n + 1)]


def pipe_run(part, mat, pts, r, clamp_mat="eq_steel", clamp_every=0.6, seg=10):
    """a rigid pipe along straight segments (bends where the polyline turns), with clamp collars"""
    tube(part, mat, pts, r, seg)
    acc = 0.0
    for p, q in zip(pts, pts[1:]):
        L = math.dist(p, q)
        if L < 1e-6:
            continue
        d = norm((q[0] - p[0], q[1] - p[1], q[2] - p[2]))
        R = axis_to(d)
        x = clamp_every - acc
        while x < L:
            c = tuple(p[k] + d[k] * x for k in range(3))
            lathe(part, clamp_mat, crect(r, r * 1.35, -0.02, 0.02), 8, R, c)
            x += clamp_every
        acc = (acc + L) % clamp_every


def hits_text(asset, hits_fn):
    vols = hits_fn()
    counts = {}
    for v in vols:
        counts[v["part"]] = counts.get(v["part"], 0) + 1
        v["id"] = f'{v["part"]}.{counts[v["part"]]}'
        for k in ("position", "rotation_deg", "half_size"):
            if k in v:
                v[k] = [round(x, 4) + 0.0 for x in v[k]]
        for k in ("radius", "half_height"):
            if k in v:
                v[k] = round(v[k], 4)
    doc = {"asset": asset, "generator": "tools/models/build_equipment_models.py (do not edit)",
           "frame": "OBJ frame of the LOD meshes, metres, +Y up; rotation_deg applied as T * Rx * Ry * Rz",
           "shapes": {"box": "half_size", "cylinder": "radius + half_height along local +Y (CollisionShape::Cylinder)"},
           "volumes": [{k: v[k] for k in ("id", "part", "shape", "position", "rotation_deg", "half_size", "radius", "half_height") if k in v}
                       for v in vols]}
    return json.dumps(doc, ensure_ascii=False, indent=1) + "\n"


def obj_text(part, mtl_name):
    out = [f"# {part.name} - generated by tools/models/build_equipment_models.py (do not edit)", f"mtllib {mtl_name}", f"o {part.name}"]
    out += [f"v {x:.5f} {y:.5f} {z:.5f}" for x, y, z in part.v]
    out += [f"vn {x:.4f} {y:.4f} {z:.4f}" for x, y, z in part.n]
    for mat in sorted(part.faces):
        out.append(f"usemtl {mat}")
        out += ["f " + " ".join(f"{i + 1}//{i + 1}" for i in f) for f in part.faces[mat]]
    return "\n".join(out) + "\n"


def mtl_text(mats):
    out = ["# generated by tools/models/build_equipment_models.py - material names are shared asset-level ids"]
    for m in sorted(mats):
        kd, ks, ns = MATERIALS[m]
        out += [f"newmtl {m}", "Ka 0 0 0", f"Kd {kd[0]:.3f} {kd[1]:.3f} {kd[2]:.3f}", f"Ks {ks[0]:.3f} {ks[1]:.3f} {ks[2]:.3f}", f"Ns {ns}", "d 1", "illum 2", ""]
    return "\n".join(out)

