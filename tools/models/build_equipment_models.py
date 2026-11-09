#!/usr/bin/env python3
"""Equipment models built from the equipment passports (author 2026-11-09: no textures; a detailed model seen from
20-30 m, simplified LODs further). Geometry only: every detail is real geometry, colours are MTL materials.

The first unit is the Cobretty CR-1 Vector main drive - a FIELD-COUPLING drive (no nozzle, no exhaust, no propellant;
the thrust vector is steered by the field): outer end - the cooling pad (solid disc 5.0 m, 8 replaceable sectors on
a clamp rim; mesh part id `engine_hot_emitter_annulus` from the game passport), behind it the field coil ring,
the closed housing with ring bands and access panels, a side service module (backup heat exchanger, two coolant
ports, sensors), four mounting lugs, inner end - the bolted flange to the propulsion reactor with the busbar
coupling and a data connector. Envelope 5 x 5 x 6.5 m (passport); a 6.5 m long unit, flange at z = 0, pad at the
outer end z = 6.5 (unit axis = OBJ +Z, OBJ +Y up). Source layout: src/assets/models/SOURCE_LAYOUT.md (one OBJ per
part in LOD<N>/, each with its own MTL; material names are asset-level ids shared by every MTL). Output goes to the
editor SOURCE root assets/models/equipment/<asset>/ (listed by the Model Asset Editor under "Equipment").

The second unit is the Cobretty ER-5 propulsion reactor (see the ER5 block below).

LODs: LOD0 <= ~30 m (bolts, clamps, panels, glands, 96 segments); LOD1 ~30-150 m (32 segments, no fasteners);
LOD2 > ~150 m (16 segments, plain volumes).

usage: python tools/models/build_equipment_models.py [--check]
"""
import argparse
import math
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "assets", "models", "equipment")

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
    c = min(chamfer, r * 0.4, L * 0.4)
    if caps:
        prof = [(0, 0), (r - c, 0), (r, c), (r, L - c), (r - c, L), (0, L)] if c > 0 else [(0, 0), (r, 0), (r, L), (0, L)]
    else:
        prof = [(r, 0), (r, L)]
    lathe(part, mat, prof, seg, R, p0, smooth=smooth)


def crect(r0, r1, z0, z1, c=0.0):
    """closed counter-clockwise (r, z) rectangle with chamfered corners (a ring cross-section)"""
    if c <= 0:
        return [(r0, z0), (r1, z0), (r1, z1), (r0, z1), (r0, z0)]
    return [(r0 + c, z0), (r1 - c, z0), (r1, z0 + c), (r1, z1 - c), (r1 - c, z1), (r0 + c, z1), (r0, z1 - c), (r0, z0 + c), (r0 + c, z0)]


def hex_bolt(part, mat, p, direction, across=0.09, height=0.06, washer=True):
    R = axis_to(norm(direction))
    r = across / math.sqrt(3)
    if washer:
        lathe(part, mat, [(0, 0), (r * 1.35, 0), (r * 1.35, height * 0.25), (0, height * 0.25)], 12, R, p)
    base = height * 0.25 if washer else 0.0
    lathe(part, mat, [(0, base), (r, base), (r, base + height * 0.75), (r * 0.8, base + height), (0, base + height)], 6, R, p, smooth=False)


# ---------------------------------------------------------------- Cobretty CR-1 Vector
CR1 = dict(length=6.5, pad_r=2.5, shell_r=2.05, band_r=2.15, flange_r=2.25, flange_t=0.35,
           coil_z0=5.30, coil_z1=6.15, pad_z0=6.15, pad_z1=6.50)


def cr1_parts(lod):
    seg = {0: 96, 1: 32, 2: 16}[lod]
    d = CR1
    P = {}

    # --- flange to the propulsion reactor (inner end), busbar coupling + data connector
    f = P["cr1_flange"] = Part("cr1_flange")
    ft = d["flange_t"]
    if lod == 0:
        lathe(f, "eq_metal_band", [(0, 0), (d["flange_r"] - 0.04, 0), (d["flange_r"], 0.04), (d["flange_r"], ft - 0.04),
                                   (d["flange_r"] - 0.04, ft), (1.9, ft), (1.9, ft + 0.12), (0, ft + 0.12)], seg)
        for i in range(24):
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.08 * math.cos(a), 2.08 * math.sin(a), ft), (0, 0, 1))
        # mating face: centring spigot ring, busbar coupling boss with 3 terminals, data connector
        lathe(f, "eq_metal_graphite", crect(1.55, 1.75, -0.08, 0.0, 0.015), seg)  # centring spigot
        lathe(f, "eq_metal_graphite", [(0, 0), (0.62, 0), (0.62, -0.18), (0.56, -0.24), (0, -0.24)][::-1], seg)
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.32 * math.cos(a), 0.32 * math.sin(a), -0.24)
            lathe(f, "eq_insulator", [(0, 0), (0.11, 0), (0.11, -0.05), (0, -0.05)][::-1], 24, I3, p)
            lathe(f, "eq_copper", [(0, -0.05), (0.06, -0.05), (0.06, -0.20), (0.045, -0.22), (0, -0.22)][::-1], 24, I3, p)
        box(f, "eq_metal_graphite", (0.0, -1.15, -0.06), (0.36, 0.22, 0.12), chamfer=0.015)
        for i in range(4):
            cyl(f, "eq_copper", (-0.12 + 0.08 * i, -1.15, -0.12), (-0.12 + 0.08 * i, -1.15, -0.14), 0.018, 8)
    else:
        lathe(f, "eq_metal_band", [(0, 0), (d["flange_r"], 0), (d["flange_r"], ft), (1.9, ft), (1.9, ft + 0.12), (0, ft + 0.12)][::1], seg)
        lathe(f, "eq_metal_graphite", [(0, -0.24), (0.62, -0.24), (0.62, 0), (0, 0)], seg)

    # --- housing: closed cylinder with ring bands (the neck after the flange, the shell up to the coil ring)
    h = P["cr1_housing"] = Part("cr1_housing")
    z0, z1 = ft + 0.12, d["coil_z0"]
    sr, br = d["shell_r"], d["band_r"]
    bands = [1.05, 2.45, 3.85, 5.05]
    if lod == 2:
        lathe(h, "eq_metal_graphite", [(1.9, z0), (sr, z0 + 0.15), (sr, z1), (1.8, z1)], seg)
        for bz in bands[::2]:
            lathe(h, "eq_metal_band", crect(sr - 0.02, br, bz - 0.09, bz + 0.09), seg)
    else:
        prof = [(1.9, z0), (1.9, z0 + 0.08), (sr - 0.05, z0 + 0.20), (sr, z0 + 0.25), (sr, z1), (1.8, z1)]
        lathe(h, "eq_metal_graphite", prof, seg)
        for bz in bands:
            c = 0.02 if lod == 0 else 0.0
            lathe(h, "eq_metal_band", crect(sr - 0.02, br, bz - 0.09, bz + 0.09, c), seg)
            if lod == 0:  # band joint blocks with two bolts each
                for i in range(8):
                    a = 2 * math.pi * (i + 0.5) / 8
                    Rz = rot_z(a)
                    box(h, "eq_metal_band", apply(Rz, (br + 0.03, 0, 0)) [:2] + (bz,), (0.08, 0.16, 0.22), mat_mul(Rz, I3), chamfer=0.012)
                    for s in (-1, 1):
                        hex_bolt(h, "eq_steel", apply(Rz, (br + 0.07, 0.0, 0))[:2] + (bz + s * 0.06,), apply(Rz, (1, 0, 0)), 0.045, 0.03, washer=False)
        # access panels between the bands: 4 around, the service-module side (angle 0) left free
        if lod == 0:
            bays = [(bands[i] + 0.16, bands[i + 1] - 0.16) for i in range(len(bands) - 1)]
            for zb0, zb1 in bays:
                for k in range(4):
                    ac = math.pi / 4 + k * math.pi / 2
                    if abs(((ac + math.pi) % (2 * math.pi)) - math.pi) < 0.6:
                        continue
                    a0, a1 = ac - 0.42, ac + 0.42
                    lathe(h, "eq_metal_panel", crect(sr - 0.01, sr + 0.035, zb0, zb1, 0.012), 24, I3, (0, 0, 0), a0, a1, cap=True)
                    for zz in (zb0 + 0.12, zb1 - 0.12):          # panel fasteners
                        for aa in (a0 + 0.05, a1 - 0.05):
                            hex_bolt(h, "eq_steel", (sr * math.cos(aa) * 1.017, sr * math.sin(aa) * 1.017, zz), (math.cos(aa), math.sin(aa), 0), 0.04, 0.02, washer=False)
                    am = ac  # handle
                    Rz = rot_z(am)
                    for zz in ((zb0 + zb1) / 2 - 0.12, (zb0 + zb1) / 2 + 0.12):
                        box(h, "eq_steel", apply(Rz, (sr + 0.07, 0, 0))[:2] + (zz,), (0.07, 0.035, 0.035), Rz)
                    box(h, "eq_steel", apply(Rz, (sr + 0.105, 0, 0))[:2] + ((zb0 + zb1) / 2,), (0.03, 0.035, 0.30), Rz, chamfer=0.008)
            # longitudinal stiffeners between panels
            for k in range(4):
                a = k * math.pi / 2
                Rz = rot_z(a)
                if k == 0:
                    continue
                box(h, "eq_metal_band", apply(Rz, (sr + 0.02, 0, 0))[:2] + ((z0 + z1) / 2 + 0.1,), (0.06, 0.10, z1 - z0 - 0.5), Rz, chamfer=0.01)
        elif lod == 1:
            for i in range(len(bands) - 1):
                zb0, zb1 = bands[i] + 0.16, bands[i + 1] - 0.16
                for k in range(4):
                    ac = math.pi / 4 + k * math.pi / 2
                    lathe(h, "eq_metal_panel", crect(sr - 0.01, sr + 0.035, zb0, zb1), 8, I3, (0, 0, 0), ac - 0.42, ac + 0.42, cap=True)

    # --- field coil ring behind the pad: 12 coil housings on a carrier ring
    c = P["cr1_coil_ring"] = Part("cr1_coil_ring")
    cz0, cz1 = d["coil_z0"], d["coil_z1"]
    lathe(c, "eq_metal_band", crect(1.7, 2.25, cz0, cz1, 0.03 if lod == 0 else 0.0), seg)
    if lod < 2:
        n = 12
        for i in range(n):
            a0 = 2 * math.pi * i / n + 0.06
            a1 = 2 * math.pi * (i + 1) / n - 0.06
            lathe(c, "eq_field_coil", crect(2.22, 2.44, cz0 + 0.07, cz1 - 0.07, 0.025 if lod == 0 else 0.0), 12 if lod == 0 else 4, I3, (0, 0, 0), a0, a1, cap=True)
            if lod == 0:
                am = (a0 + a1) / 2
                Rz = rot_z(am)
                # retaining strap and two cable glands per coil
                lathe(c, "eq_steel", crect(2.44, 2.47, (cz0 + cz1) / 2 - 0.05, (cz0 + cz1) / 2 + 0.05, 0.01), 12, I3, (0, 0, 0), a0 + 0.02, a1 - 0.02, cap=True)
                for s in (-1, 1):
                    q = apply(Rz, (2.30, s * 0.32, 0))
                    cyl(c, "eq_insulator", (q[0], q[1], cz0 + 0.07), (q[0], q[1], cz0 - 0.05), 0.035, 12, chamfer=0.006)
                    cyl(c, "eq_copper", (q[0], q[1], cz0 - 0.05), (q[0], q[1], cz0 - 0.12), 0.022, 10)
    else:
        lathe(c, "eq_field_coil", crect(2.22, 2.44, cz0 + 0.07, cz1 - 0.07), seg)

    # --- cooling pad = engine_hot_emitter_annulus: 8 replaceable ceramic sectors in a clamp rim, centre hub
    p = P["engine_hot_emitter_annulus"] = Part("engine_hot_emitter_annulus")
    pz0, pz1, pr = d["pad_z0"], d["pad_z1"], d["pad_r"]
    lathe(p, "eq_metal_band", [(0, pz0), (2.3, pz0), (2.3, pz0 + 0.05), (0, pz0 + 0.05)], seg)  # backing plate
    if lod < 2:
        gap = 0.012
        for i in range(8):
            a0 = 2 * math.pi * i / 8 + gap
            a1 = 2 * math.pi * (i + 1) / 8 - gap
            lathe(p, "eq_pad_ceramic", crect(0.48, 2.36, pz0 + 0.05, pz1 - 0.03, 0.012 if lod == 0 else 0.0), seg // 8 * 2, I3, (0, 0, 0), a0, a1, cap=True)
        lathe(p, "eq_steel", crect(2.36, 2.44, pz0, pz1, 0.015 if lod == 0 else 0.0), seg)                      # clamp rim
        lathe(p, "eq_metal_band", [(0, pz0 + 0.05), (0.48, pz0 + 0.05), (0.48, pz1 - 0.01), (0.42, pz1 + 0.02), (0, pz1 + 0.02)], seg)  # hub
        if lod == 0:
            hex_bolt(p, "eq_steel", (0, 0, pz1 + 0.02), (0, 0, 1), 0.14, 0.07)
            for i in range(8):                                   # sector seams: radial clamp bars on the gaps
                a = 2 * math.pi * i / 8
                Rz = rot_z(a)
                box(p, "eq_steel", apply(Rz, (1.42, 0, 0))[:2] + (pz1 - 0.01,), (1.88, 0.05, 0.03), Rz, chamfer=0.006)
                for rr in (0.75, 1.43, 2.1):
                    hex_bolt(p, "eq_steel", apply(Rz, (rr, 0, 0))[:2] + (pz1 + 0.005,), (0, 0, 1), 0.035, 0.018, washer=False)
            for i in range(16):                                  # quick-release clamps on the rim
                a = 2 * math.pi * (i + 0.5) / 16
                Rz = rot_z(a)
                box(p, "eq_steel", apply(Rz, (2.42, 0, 0))[:2] + (pz1 + 0.025,), (0.16, 0.14, 0.05), Rz, chamfer=0.01)
                box(p, "eq_metal_band", apply(Rz, (2.475, 0, 0))[:2] + ((pz0 + pz1) / 2,), (0.05, 0.12, 0.30), Rz, chamfer=0.01)
                cyl(p, "eq_steel", apply(Rz, (2.48, -0.07, 0))[:2] + (pz1 - 0.03,), apply(Rz, (2.48, 0.07, 0))[:2] + (pz1 - 0.03,), 0.02, 10)
    else:
        lathe(p, "eq_pad_ceramic", [(0, pz0 + 0.05), (pr, pz0 + 0.05), (pr, pz1), (0, pz1)], seg)
        lathe(p, "eq_steel", [(pr - 0.01, pz0), (pr, pz0), (pr, pz1), (pr - 0.01, pz1)], seg)

    # --- service module: backup heat exchanger, two coolant ports, field / temperature sensors (side +X)
    s = P["cr1_service_module"] = Part("cr1_service_module")
    sx, sz = sr + 0.32, 2.95
    box(s, "eq_metal_panel", (sx, 0, sz), (0.62, 1.30, 2.10), chamfer=0.05 if lod == 0 else 0.0)
    box(s, "eq_metal_graphite", (sr + 0.05, 0, sz), (0.20, 1.10, 1.90))                         # saddle to the shell
    if lod < 2:
        for yy in (-0.38, 0.38):                                                                 # two coolant ports
            zz = sz - 0.62
            cyl(s, "eq_copper", (sx + 0.31, yy, zz), (sx + 0.62, yy, zz), 0.10, 32 if lod == 0 else 12)
            lathe(s, "eq_steel", [(0, 0), (0.17, 0), (0.17, 0.06), (0, 0.06)], 32 if lod == 0 else 12, axis_to((1, 0, 0)), (sx + 0.62, yy, zz))
            if lod == 0:
                for i in range(6):
                    a = 2 * math.pi * i / 6
                    hex_bolt(s, "eq_steel", (sx + 0.68, yy + 0.13 * math.cos(a), zz + 0.13 * math.sin(a)), (1, 0, 0), 0.03, 0.02, washer=False)
        if lod == 0:
            # access door with hinges and latch, status window, sensor heads on the top
            box(s, "eq_metal_graphite", (sx + 0.315, 0, sz + 0.35), (0.02, 1.0, 1.05), chamfer=0.006)
            for zz in (sz - 0.05, sz + 0.75):
                cyl(s, "eq_steel", (sx + 0.335, -0.52, zz - 0.09), (sx + 0.335, -0.52, zz + 0.09), 0.025, 12)
            box(s, "eq_steel", (sx + 0.34, 0.45, sz + 0.35), (0.03, 0.05, 0.22), chamfer=0.006)
            box(s, "eq_status_amber", (sx + 0.33, 0.25, sz + 0.80), (0.02, 0.16, 0.05))
            box(s, "eq_status_amber", (sx + 0.33, 0.25, sz + 0.72), (0.02, 0.16, 0.05))
            for yy in (-0.35, 0.0, 0.35):
                cyl(s, "eq_metal_graphite", (sx, yy, sz + 1.05), (sx, yy, sz + 1.17), 0.06, 20, chamfer=0.01)
                lathe(s, "eq_insulator", [(0, 0), (0.04, 0), (0.035, 0.025), (0, 0.04)], 16, I3, (sx, yy, sz + 1.17))
            # cable conduit from the module to the coil ring
            cyl(s, "eq_metal_band", (sr + 0.17, 0.45, sz + 1.05), (sr + 0.17, 0.45, d["coil_z0"]), 0.05, 16)
            cyl(s, "eq_metal_band", (sr + 0.17, -0.45, sz + 1.05), (sr + 0.17, -0.45, d["coil_z0"]), 0.05, 16)
            for zz in (sz + 1.6, sz + 2.1):
                for yy in (-0.45, 0.45):
                    box(s, "eq_steel", (sr + 0.17, yy, zz), (0.14, 0.14, 0.05), chamfer=0.01)

    # --- four mounting lugs to the engine bay frame (no floor stand)
    m = P["cr1_mounts"] = Part("cr1_mounts")
    for k, (a, zz) in enumerate(((math.pi * 3 / 4, 1.05), (-math.pi * 3 / 4, 1.05), (math.pi * 3 / 4, 3.85), (-math.pi * 3 / 4, 3.85))):
        Rz = rot_z(a)
        box(m, "eq_metal_band", apply(Rz, (2.28, 0, 0))[:2] + (zz,), (0.42, 0.46, 0.14), Rz, chamfer=0.02 if lod == 0 else 0.0)
        if lod < 2:
            q = apply(Rz, (2.36, 0, 0))
            lathe(m, "eq_steel", crect(0.06, 0.11, -0.10, 0.10, 0.01 if lod == 0 else 0.0), 24 if lod == 0 else 8, I3, (q[0], q[1], zz))
    return P


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


# ---------------------------------------------------------------- Cobretty ER-5 propulsion reactor
# Passport (CobraCobrettyER5PropulsionReactor.h, canon kb.reactor.er5): 5000 MWe D-He3, 25 t, body 4 x 4 x 5 m flange to
# flange, square cooling pad 11 x 11 m x 0.25 m (PT1) above it on a short heat-pipe column. A flat torus (axis = OBJ +Y)
# hangs in the engine bay between the two drives on two axial flanges (OBJ -Z fore, +Z aft; mating faces at z = -/+2.5).
# The flanges mate the CR-1 flange exactly (r 2.25, 24 bolts on r 2.08, spigot groove r 1.50-1.80, busbar pocket
# r 0.68 with 3 sockets on r 0.32, data socket at y = -1.15), so the flange rims (dia 4.5 m) stand 0.25 m out of the
# 4 m body envelope. Serviceable units = SRUs of the fault class reactor.fusion: pad, plasma_ctl, damper (confinement
# magnets), feed (pellet feed), inverter, sensor.
ER5 = dict(r_in=0.62, r_out=1.95, h=1.0, corner=0.40, face_z=2.5, flange_r=2.25,
           pad_y0=2.3, pad_t=0.25, pad_half=5.5)


def er5_flange(lod, seg):
    """one flange in its local frame: mating face at z = 0 facing +Z, the neck going -Z into the torus"""
    d = ER5
    f = Part("flange")
    fr = d["flange_r"]
    c = 0.04 if lod == 0 else 0.0
    if lod < 2:
        prof = [(0, -1.05), (1.1, -1.05), (1.1, -0.75), (1.9, -0.30), (fr - c, -0.30), (fr, -0.30 + c), (fr, -c), (fr - c, 0),
                (1.80, 0), (1.80, -0.10), (1.50, -0.10), (1.50, 0), (0.68, 0), (0.68, -0.27), (0, -0.27)]
    else:
        prof = [(0, -1.05), (1.1, -1.05), (1.1, -0.75), (1.9, -0.30), (fr, -0.30), (fr, 0), (0.68, 0), (0.68, -0.27), (0, -0.27)]
    lathe(f, "eq_metal_band", [p for i, p in enumerate(prof) if i == 0 or p != prof[i - 1]], seg)
    if lod < 2:
        lathe(f, "eq_metal_graphite", crect(1.505, 1.795, -0.10, -0.095), seg)          # groove liner (darker ring)
        for i in range(3):                                                              # busbar sockets
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.32 * math.cos(a), 0.32 * math.sin(a), -0.27)
            lathe(f, "eq_insulator", [(0, 0), (0.14, 0), (0.14, 0.04), (0, 0.04)], 24 if lod == 0 else 8, I3, p)
            lathe(f, "eq_copper", [(0, 0.04), (0.075, 0.04), (0.075, 0.07), (0, 0.07)], 24 if lod == 0 else 8, I3, p)
        box(f, "eq_metal_graphite", (0.0, -1.15, 0.004), (0.40, 0.26, 0.008))               # data socket plate
    if lod == 0:
        for i in range(24):                                                             # nuts of the 24 flange bolts
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.08 * math.cos(a), 2.08 * math.sin(a), -0.30), (0, 0, -1))
        for i in range(4):                                                              # data socket pins
            cyl(f, "eq_copper", (-0.12 + 0.08 * i, -1.15, 0.008), (-0.12 + 0.08 * i, -1.15, 0.012), 0.022, 8)
        for i in range(12):                                                             # stiffening ribs on the bell
            a = 2 * math.pi * i / 12 + math.pi / 12
            Rz = rot_z(a)
            box(f, "eq_metal_band", apply(Rz, (1.58, 0, 0))[:2] + (-0.52,), (0.62, 0.06, 0.40), Rz, chamfer=0.01)
    return f


def er5_parts(lod):
    seg = {0: 96, 1: 32, 2: 16}[lod]
    small = {0: 24, 1: 10, 2: 6}[lod]
    d = ER5
    ri, ro, h, cr = d["r_in"], d["r_out"], d["h"], d["corner"]
    P = {}
    nc = {0: 8, 1: 3, 2: 1}[lod]

    # --- vessel: the flat torus, equator coolant manifold, top access panels
    v = P["er5_vessel"] = Part("er5_vessel")
    ylathe(v, "eq_metal_graphite", rrect(ri, ro, -h, h, cr, nc), seg, soft=35)
    if lod < 2:
        for yy in (-0.30, 0.30):                                                         # equator manifold pipes
            ylathe(v, "eq_metal_band", rrect(ro - 0.01, ro + 0.11, yy - 0.06, yy + 0.06, 0.05, 3 if lod == 0 else 1), seg, soft=50)
        free = [(-0.42, -0.05), (0.05, 0.42), (math.pi - 0.42, math.pi - 0.05), (math.pi + 0.05, math.pi + 0.42),
                (0.80, 2.34), (math.pi + 0.80, math.pi + 2.34)]
        for t0, t1 in free:                                                              # top access panels
            ylathe(v, "eq_metal_panel", crect(ri + cr + 0.06, ro - cr - 0.06, h - 0.01, h + 0.03, 0.01 if lod == 0 else 0.0),
                   max(4, int(seg * (t1 - t0) / (2 * math.pi))), t0 + 0.03, t1 - 0.03, cap=True)
            if lod == 0:
                for th in (t0 + 0.08, t1 - 0.08):
                    for rr in (ri + cr + 0.14, ro - cr - 0.14):
                        hex_bolt(v, "eq_steel", polar(rr, th, h + 0.03), (0, 1, 0), 0.04, 0.02, washer=False)
        if lod == 0:                                                                     # manifold clips
            for k in range(16):
                th = 2 * math.pi * (k + 0.5) / 16
                if abs(math.sin(th)) > 0.5:
                    continue
                Ry = rot_y(-th)
                box(v, "eq_steel", polar(ro + 0.06, th, 0.0), (0.10, 0.80, 0.08), Ry, chamfer=0.01)

    # --- confinement magnets / dampers: 4 hoop housings round the tube cross-section (between the boxes and the necks)
    m = P["er5_confinement"] = Part("er5_confinement")
    hoops = (math.radians(35), math.radians(-35), math.radians(145), math.radians(215))
    for th in hoops:
        ylathe(m, "eq_field_coil", rrect(ri - 0.07, ro + 0.07, -h - 0.07, h + 0.07, cr + 0.06, nc), small, th - 0.11, th + 0.11, cap=True, soft=35)
        if lod < 2:
            ylathe(m, "eq_metal_band", rrect(ri - 0.09, ro + 0.09, -h - 0.09, h + 0.09, cr + 0.07, nc), small, th - 0.03, th + 0.03, cap=True, soft=35)
        if lod == 0:
            Ry = rot_y(-th)
            box(m, "eq_metal_band", polar(ro + 0.14, th, 0.0), (0.12, 0.62, 0.34), Ry, chamfer=0.02)     # clamp block
            for yy in (-0.2, 0.2):
                hex_bolt(m, "eq_steel", polar(ro + 0.20, th, yy), polar(1, th, 0), 0.05, 0.03, washer=False)
            for rr in ((ri + ro) / 2 - 0.25, (ri + ro) / 2 + 0.25):                                   # top bolts
                hex_bolt(m, "eq_steel", polar(rr, th, h + 0.09), (0, 1, 0), 0.05, 0.03, washer=False)
            cyl(m, "eq_insulator", polar((ri + ro) / 2, th, h + 0.09), polar((ri + ro) / 2, th, h + 0.20), 0.05, 12, chamfer=0.01)

    # --- flanges to the two drives
    fl = er5_flange(lod, seg)
    P["er5_flange_aft"] = placed(fl, I3, (0, 0, d["face_z"]))
    P["er5_flange_aft"].name = "er5_flange_aft"
    P["er5_flange_fore"] = placed(fl, rot_y(math.pi), (0, 0, -d["face_z"]))
    P["er5_flange_fore"].name = "er5_flange_fore"

    # --- power inverter to the engine bus (+X), busbars to both flanges
    iv = P["er5_inverter"] = Part("er5_inverter")
    box(iv, "eq_metal_panel", (2.05, 0, 0), (0.36, 1.0, 1.05), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        for zs in (-1, 1):
            for yy in (-0.22, 0.22):
                a, b = (2.0, yy, zs * 0.50), (1.60, yy, zs * 2.05)
                cyl(iv, "eq_insulator", a, b, 0.07, small)
                if lod == 0:
                    cyl(iv, "eq_copper", a, (a[0] - 0.015, yy, a[2] + zs * 0.06), 0.085, small)
                    mid = tuple((p + q) / 2 for p, q in zip(a, b))
                    cyl(iv, "eq_steel", (mid[0], mid[1], mid[2] - 0.04), (mid[0], mid[1], mid[2] + 0.04), 0.09, small)  # support clamp
    if lod == 0:
        box(iv, "eq_metal_graphite", (2.235, 0.0, 0.0), (0.012, 0.80, 0.86), chamfer=0.004)            # front cover
        for yy in (-0.34, 0.34):
            for zz in (-0.37, 0.37):
                hex_bolt(iv, "eq_steel", (2.241, yy, zz), (1, 0, 0), 0.04, 0.02, washer=False)
        for i in range(3):
            box(iv, "eq_status_amber", (2.243, 0.22, -0.20 + 0.08 * i), (0.006, 0.05, 0.05))
        for i in range(6):                                                                             # cooling fins
            box(iv, "eq_metal_band", (2.05, 0.51, -0.40 + 0.16 * i), (0.28, 0.02, 0.06))

    # --- pellet feed from the fuel bunker (-X): feed housing, bunker port on top
    fd = P["er5_fuel_feed"] = Part("er5_fuel_feed")
    box(fd, "eq_metal_panel", (-2.05, 0.02, 0), (0.36, 0.90, 0.90), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        cyl(fd, "eq_metal_band", (-2.05, 0.47, 0), (-2.05, 0.90, 0), 0.12, small)
        lathe(fd, "eq_steel", [(0, 0), (0.20, 0), (0.20, 0.05), (0, 0.05)], small, R_Y, (-2.05, 0.90, 0))
    if lod == 0:
        for i in range(6):
            a = 2 * math.pi * i / 6
            hex_bolt(fd, "eq_steel", (-2.05 + 0.15 * math.cos(a), 0.95, 0.15 * math.sin(a)), (0, 1, 0), 0.03, 0.02, washer=False)
        box(fd, "eq_metal_graphite", (-2.235, 0.0, 0.0), (0.012, 0.62, 0.62), chamfer=0.004)            # hatch
        box(fd, "eq_status_amber", (-2.243, 0.22, 0.20), (0.006, 0.08, 0.10))                           # sight window
        for zz in (-0.22, 0.22):
            cyl(fd, "eq_steel", (-2.25, -0.33, zz - 0.06), (-2.25, -0.33, zz + 0.06), 0.02, 10)          # hinges

    # --- plasma control unit (under the vessel, below the inverter)
    pc = P["er5_plasma_ctl"] = Part("er5_plasma_ctl")
    rm = (ri + ro) / 2
    box(pc, "eq_metal_panel", (rm, -h - 0.13, 0), (0.50, 0.26, 0.90), chamfer=0.03 if lod == 0 else 0.0)
    if lod == 0:
        box(pc, "eq_status_amber", (rm, -h - 0.265, 0.30), (0.10, 0.006, 0.06))
        for zz in (-0.30, -0.15, 0.0):
            cyl(pc, "eq_metal_graphite", (rm, -h - 0.26, zz), (rm, -h - 0.32, zz), 0.04, 12, chamfer=0.006)
        cyl(pc, "eq_metal_band", (rm + 0.26, -h - 0.10, 0.0), (ro - 0.06, -h + 0.40, 0.0), 0.05, 12)                # conduit to the inverter

    # --- plasma sensor ports on the vessel top
    sn = P["er5_plasma_sensors"] = Part("er5_plasma_sensors")
    if lod < 2:
        for th in (math.radians(20), math.radians(-20), math.radians(160), math.radians(200), math.pi / 2, -math.pi / 2):
            p0 = polar((ri + ro) / 2, th, h)
            cyl(sn, "eq_metal_graphite", p0, (p0[0], h + 0.14, p0[2]), 0.07, small, chamfer=0.01 if lod == 0 else 0.0)
            if lod == 0:
                lathe(sn, "eq_insulator", [(0, 0), (0.05, 0), (0.04, 0.03), (0, 0.05)], 16, R_Y, (p0[0], h + 0.14, p0[2]))

    # --- heat-pipe column from the vessel to the pad: spider over the torus hole, 7 heat pipes, collars, top plate
    hc = P["er5_heat_pipe_column"] = Part("er5_heat_pipe_column")
    y0, y1 = h - 0.06, d["pad_y0"]
    for k in range(4):
        th = math.pi / 4 + k * math.pi / 2
        box(hc, "eq_metal_band", polar(0.62, th, h + 0.02), (1.20, 0.16, 0.16), rot_y(-th), chamfer=0.015 if lod == 0 else 0.0)
    ylathe(hc, "eq_metal_band", [(0, y0), (0.50, y0), (0.50, h + 0.14), (0, h + 0.14)], seg)
    pipes = [(0.0, 0.0, 0.20)] + [(0.34 * math.cos(2 * math.pi * i / 6), 0.34 * math.sin(2 * math.pi * i / 6), 0.08) for i in range(6)]
    for x, z, r in pipes if lod < 2 else pipes[:1]:
        cyl(hc, "eq_steel" if r < 0.1 else "eq_metal_graphite", (x, h + 0.14, z), (x, y1 - 0.08, z), r if lod < 2 else 0.30, small)
    if lod < 2:
        for yy in (h + 0.45, h + 0.75, h + 1.05):
            ylathe(hc, "eq_metal_band", crect(0.15, 0.46, yy - 0.03, yy + 0.03, 0.01 if lod == 0 else 0.0), seg)
    ylathe(hc, "eq_metal_band", [(0, y1 - 0.08), (0.65, y1 - 0.08), (0.65, y1), (0, y1)], seg)
    if lod == 0:
        for i in range(8):
            a = 2 * math.pi * (i + 0.5) / 8
            hex_bolt(hc, "eq_steel", polar(0.56, a, h + 0.14), (0, 1, 0), 0.04, 0.025, washer=False)
            hex_bolt(hc, "eq_steel", polar(0.56, a, y1 - 0.08), (0, -1, 0), 0.04, 0.025, washer=False)

    # --- cooling pad PT1: 11 x 11 m, 0.25 m; frame (rim, diagonals, midlines), 8 triangular ceramic sections
    pd = P["reactor_cooling_pad"] = Part("reactor_cooling_pad")
    py0, pt, L = d["pad_y0"], d["pad_t"], d["pad_half"]
    py1 = py0 + pt
    bw = 0.22
    box(pd, "eq_metal_graphite", (0, py0 + 0.02, 0), (2 * L - 0.2, 0.04, 2 * L - 0.2))                  # backing sheet
    for s in (-1, 1):                                                                                  # rim beams
        box(pd, "eq_metal_band", (s * (L - bw / 2), (py0 + py1) / 2, 0), (bw, pt, 2 * L), chamfer=0.02 if lod == 0 else 0.0)
        box(pd, "eq_metal_band", (0, (py0 + py1) / 2, s * (L - bw / 2)), (2 * L - 2 * bw, pt, bw), chamfer=0.02 if lod == 0 else 0.0)
    for th in (0.0, math.pi / 2):                                                                      # midline beams
        box(pd, "eq_metal_band", (0, (py0 + py1) / 2, 0), (2 * L - 2 * bw, pt - 0.01, 0.18), rot_y(th))
    for th in (math.pi / 4, -math.pi / 4):                                                             # diagonal beams
        box(pd, "eq_metal_band", (0, (py0 + py1) / 2, 0), (2 * L * math.sqrt(2) - 0.6, pt - 0.01, 0.18), rot_y(th))
    box(pd, "eq_metal_band", (0, py1 - 0.10, 0), (1.0, 0.30, 1.0), chamfer=0.04 if lod == 0 else 0.0)  # centre hub
    corners = [(sx * L, sz * L) for sx, sz in ((1, 1), (-1, 1), (-1, -1), (1, -1))]
    for x, z in corners:                                                                               # corner blocks
        box(pd, "eq_metal_band", (x - math.copysign(0.24, x), (py0 + py1) / 2 + 0.03, z - math.copysign(0.24, z)), (0.50, pt + 0.08, 0.50),
            chamfer=0.04 if lod == 0 else 0.0)
    for x, z in ((L, 0), (-L, 0), (0, L), (0, -L)):                                                    # edge-mid blocks
        box(pd, "eq_metal_band", (x - math.copysign(0.2, x) if x else 0, (py0 + py1) / 2 + 0.02, z - math.copysign(0.2, z) if z else 0),
            (0.42 if x else 0.60, pt + 0.04, 0.60 if x else 0.42), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        for k in range(8):                                                                             # 8 sections
            ca = corners[k // 2]
            mid = (ca[0], 0.0) if k % 2 == 0 else (0.0, ca[1])
            tri = inset([(0.0, 0.0), ca, mid], 0.13)
            prism(pd, "eq_pad_ceramic", tri, py0 + 0.04, py1 - 0.012)
    else:
        box(pd, "eq_pad_ceramic", (0, py1 - 0.06, 0), (2 * L - 0.3, 0.10, 2 * L - 0.3))
    if lod == 0:
        def bolts_along(p0, p1, step):
            n = max(1, int(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / step))
            for i in range(1, n):
                t = i / n
                hex_bolt(pd, "eq_steel", (p0[0] + (p1[0] - p0[0]) * t, py1 - 0.005, p0[1] + (p1[1] - p0[1]) * t), (0, 1, 0), 0.05, 0.025, washer=False)
        for x, z in corners:
            bolts_along((math.copysign(0.75, x), math.copysign(0.75, z)), (x - math.copysign(0.6, x), z - math.copysign(0.6, z)), 1.0)
        for x, z in ((L, 0), (-L, 0), (0, L), (0, -L)):
            bolts_along((math.copysign(0.6, x) if x else 0, math.copysign(0.6, z) if z else 0), (x - math.copysign(0.5, x) if x else 0, z - math.copysign(0.5, z) if z else 0), 1.0)
        for s in (-1, 1):                                                                              # rim bolts + side slots
            for i in range(10):
                q = -L + 0.85 + i * (2 * L - 1.7) / 9
                hex_bolt(pd, "eq_steel", (s * (L - bw / 2), py1 + 0.0, q), (0, 1, 0), 0.05, 0.025, washer=False)
                hex_bolt(pd, "eq_steel", (q, py1 + 0.0, s * (L - bw / 2)), (0, 1, 0), 0.05, 0.025, washer=False)
            for q in (-L / 2, L / 2):
                box(pd, "eq_metal_graphite", (s * L + s * 0.004, (py0 + py1) / 2, q), (0.01, 0.08, 1.6))
                box(pd, "eq_metal_graphite", (q, (py0 + py1) / 2, s * L + s * 0.004), (1.6, 0.08, 0.01))
        for x, z in corners:                                                                           # lifting eyes
            lathe(pd, "eq_steel", crect(0.05, 0.09, -0.03, 0.03, 0.01), 16, axis_to((1, 0, 0)),
                  (x - math.copysign(0.24, x), py1 + 0.14, z - math.copysign(0.24, z)))
        for i in range(4):
            hex_bolt(pd, "eq_steel", (0.32 * (1 if i % 2 else -1), py1 + 0.05, 0.32 * (1 if i // 2 else -1)), (0, 1, 0), 0.07, 0.035)

    # --- emergency connection to the shared coolant loop: the only copper lines (column foot -> port block)
    ec = P["er5_emergency_coolant"] = Part("er5_emergency_coolant")
    if lod < 2:
        th = math.radians(150)
        Ry = rot_y(-th)
        box(ec, "eq_metal_panel", polar(rm, th, h + 0.10), (0.34, 0.20, 0.46), Ry, chamfer=0.02 if lod == 0 else 0.0)
        for s in (-1, 1):
            off = (-math.sin(th) * 0.12 * s, 0, math.cos(th) * 0.12 * s)
            a = tuple(p + o for p, o in zip(polar(0.50, th, h + 0.24), off))
            b = tuple(p + o for p, o in zip(polar(rm - 0.16, th, h + 0.24), off))
            cyl(ec, "eq_copper", a, b, 0.05, small)
            top = tuple(p + o for p, o in zip(polar(rm, th, h + 0.20), off))
            cyl(ec, "eq_copper", top, (top[0], h + 0.42, top[2]), 0.06, small)
            if lod == 0:
                lathe(ec, "eq_steel", [(0, 0), (0.10, 0), (0.10, 0.04), (0, 0.04)], small, R_Y, (top[0], h + 0.42, top[2]))
                cyl(ec, "eq_copper", a, (a[0], h + 0.12, a[2]), 0.05, small)

    # --- mounting lugs to the engine bay frame under the four magnet hoops (no floor stand)
    mt = P["er5_mounts"] = Part("er5_mounts")
    for th in hoops:
        Ry = rot_y(-th)
        box(mt, "eq_metal_band", polar(rm, th, -h - 0.20), (0.46, 0.30, 0.28), Ry, chamfer=0.02 if lod == 0 else 0.0)
        if lod < 2:
            ax = polar(1, th + math.pi / 2, 0)
            c0 = polar(rm, th, -h - 0.26)
            lathe(mt, "eq_steel", crect(0.05, 0.10, -0.16, 0.16, 0.01 if lod == 0 else 0.0), small, axis_to(ax), c0)
    return P


# ---------------------------------------------------------------- output
ASSETS = {"cobretty_cr1_vector": cr1_parts, "cobretty_er5_reactor": er5_parts}


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


def build():
    files = {}
    stats = {}
    for asset, fn in ASSETS.items():
        for lod in (0, 1, 2):
            parts = fn(lod)
            total = 0
            for name, part in parts.items():
                if not part.tris():          # a detail dropped at this LOD: no empty OBJ
                    continue
                base = os.path.join(OUT, asset, f"LOD{lod}", name)
                files[base + ".obj"] = obj_text(part, name + ".mtl")
                files[base + ".mtl"] = mtl_text(part.faces.keys())
                total += part.tris()
            stats[(asset, lod)] = (total, {k: v.tris() for k, v in parts.items()})
    return files, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    files, stats = build()
    stale = [p for p, t in files.items() if not os.path.exists(p) or open(p, encoding="utf-8", newline="").read() != t]
    for (asset, lod), (total, per) in stats.items():
        print(f"{asset} LOD{lod}: {total} triangles  " + ", ".join(f"{k} {v}" for k, v in per.items()))
    if a.check:
        if stale:
            print("STALE:", *[os.path.relpath(p, ROOT) for p in stale], sep="\n  ")
            sys.exit(1)
        print("equipment models up to date")
        return
    for p in stale:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, "w", encoding="utf-8", newline="\n").write(files[p])
    print(f"written {len(stale)} of {len(files)} files")


if __name__ == "__main__":
    main()
