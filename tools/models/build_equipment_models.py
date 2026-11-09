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

LODs: LOD0 <= ~30 m (32 segments; fasteners >= 6 cm, chamfers >= 2.5 cm: MIN_BOLT / MIN_FEATURE), LOD1 ~30-150 m
(16 segments, no fasteners or panels), LOD2 > ~150 m (12 segments, plain volumes). Budget (author 2026-11-09: one unit
must not eat a ship's memory): LOD0 ~10k triangles per unit, LOD1 ~2-3k, LOD2 < 1k.

usage: python tools/models/build_equipment_models.py [--check]
"""
import argparse
import json
import math
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "assets", "models", "equipment")

# Visibility floor (author 2026-11-09: the models must stay light). LOD0 is the look from ~20-30 m, where one pixel
# (1080p, 60 deg FOV) is ~3 cm: chamfers below MIN_FEATURE and fasteners below MIN_BOLT across are not built at all.
MIN_FEATURE = 0.025
MIN_BOLT = 0.06

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
    # Sol Reactor Works: white panels, navy coils; Tianhe Heavy: olive steel, red-lacquered coil casings
    "eq_srw_white": ((0.80, 0.81, 0.80), (0.25, 0.25, 0.25), 50),
    "eq_srw_coil": ((0.12, 0.17, 0.30), (0.30, 0.30, 0.35), 70),
    "eq_th_steel": ((0.33, 0.36, 0.31), (0.20, 0.20, 0.20), 40),
    "eq_th_coil": ((0.45, 0.16, 0.12), (0.30, 0.25, 0.25), 60),
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
    if chamfer < MIN_FEATURE:
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
    c = min(chamfer, r * 0.4, L * 0.4) if chamfer >= MIN_FEATURE else 0.0
    if caps:
        prof = [(0, 0), (r - c, 0), (r, c), (r, L - c), (r - c, L), (0, L)] if c > 0 else [(0, 0), (r, 0), (r, L), (0, L)]
    else:
        prof = [(r, 0), (r, L)]
    lathe(part, mat, prof, seg, R, p0, smooth=smooth)


def crect(r0, r1, z0, z1, c=0.0):
    """closed counter-clockwise (r, z) rectangle with chamfered corners (a ring cross-section)"""
    if c < MIN_FEATURE:
        return [(r0, z0), (r1, z0), (r1, z1), (r0, z1), (r0, z0)]
    return [(r0 + c, z0), (r1 - c, z0), (r1, z0 + c), (r1, z1 - c), (r1 - c, z1), (r0 + c, z1), (r0, z1 - c), (r0, z0 + c), (r0 + c, z0)]


def hex_bolt(part, mat, p, direction, across=0.09, height=0.06, washer=False):
    """a hex head (6 flat sides + top, ~18 triangles; the underside sits on the surface and is not built).
    washer: a thin 6-sided washer under the head (+12 triangles)"""
    if across < MIN_BOLT:
        return
    R = axis_to(norm(direction))
    r = across / math.sqrt(3)
    base = 0.0
    if washer:
        base = height * 0.25
        lathe(part, mat, [(r * 1.35, 0), (r * 1.35, base), (r, base)], 6, R, p, smooth=False)
    lathe(part, mat, [(r, base), (r, base + height), (0, base + height)], 6, R, p, smooth=False)


# ---------------------------------------------------------------- Cobretty CR-1 Vector
CR1 = dict(length=6.5, pad_r=2.5, shell_r=2.05, band_r=2.15, flange_r=2.25, flange_t=0.35,
           coil_z0=5.30, coil_z1=6.15, pad_z0=6.15, pad_z1=6.50)


def cr1_parts(lod):
    seg = {0: 32, 1: 16, 2: 12}[lod]
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
                for i in range(4):
                    a = 2 * math.pi * (i + 0.5) / 4
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
                    lathe(h, "eq_metal_panel", crect(sr - 0.01, sr + 0.035, zb0, zb1, 0.012), 8, I3, (0, 0, 0), a0, a1, cap=True)
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
        # LOD1/LOD2: no panels (35 mm relief is invisible beyond ~30 m)

    # --- field coil ring behind the pad: 12 coil housings on a carrier ring
    c = P["cr1_coil_ring"] = Part("cr1_coil_ring")
    cz0, cz1 = d["coil_z0"], d["coil_z1"]
    lathe(c, "eq_metal_band", crect(1.7, 2.25, cz0, cz1, 0.03 if lod == 0 else 0.0), seg)
    if lod < 2:
        n = 12
        for i in range(n):
            a0 = 2 * math.pi * i / n + 0.06
            a1 = 2 * math.pi * (i + 1) / n - 0.06
            lathe(c, "eq_field_coil", crect(2.22, 2.44, cz0 + 0.07, cz1 - 0.07), 6 if lod == 0 else 3, I3, (0, 0, 0), a0, a1, cap=True)
            if lod == 0:
                am = (a0 + a1) / 2
                Rz = rot_z(am)
                # retaining strap and two cable glands per coil
                lathe(c, "eq_steel", crect(2.44, 2.47, (cz0 + cz1) / 2 - 0.05, (cz0 + cz1) / 2 + 0.05), 6, I3, (0, 0, 0), a0 + 0.02, a1 - 0.02, cap=True)
                for s in (1,):                                   # one cable gland per coil
                    q = apply(Rz, (2.30, s * 0.32, 0))
                    cyl(c, "eq_insulator", (q[0], q[1], cz0 + 0.07), (q[0], q[1], cz0 - 0.05), 0.035, 8)
                    cyl(c, "eq_copper", (q[0], q[1], cz0 - 0.05), (q[0], q[1], cz0 - 0.12), 0.022, 6)
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
            lathe(p, "eq_pad_ceramic", crect(0.48, 2.36, pz0 + 0.05, pz1 - 0.03), max(2, seg // 8), I3, (0, 0, 0), a0, a1, cap=True)
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
            cyl(s, "eq_copper", (sx + 0.31, yy, zz), (sx + 0.62, yy, zz), 0.10, 16 if lod == 0 else 8)
            lathe(s, "eq_steel", [(0, 0), (0.17, 0), (0.17, 0.06), (0, 0.06)], 16 if lod == 0 else 8, axis_to((1, 0, 0)), (sx + 0.62, yy, zz))
            if lod == 0:
                for i in range(6):
                    a = 2 * math.pi * i / 6
                    hex_bolt(s, "eq_steel", (sx + 0.68, yy + 0.13 * math.cos(a), zz + 0.13 * math.sin(a)), (1, 0, 0), 0.03, 0.02, washer=False)
        if lod == 0:
            # access door with hinges and latch, status window, sensor heads on the top
            box(s, "eq_metal_graphite", (sx + 0.315, 0, sz + 0.35), (0.02, 1.0, 1.05), chamfer=0.006)
            for zz in (sz - 0.05, sz + 0.75):
                cyl(s, "eq_steel", (sx + 0.335, -0.52, zz - 0.09), (sx + 0.335, -0.52, zz + 0.09), 0.025, 6)
            box(s, "eq_steel", (sx + 0.34, 0.45, sz + 0.35), (0.03, 0.05, 0.22), chamfer=0.006)
            box(s, "eq_status_amber", (sx + 0.33, 0.25, sz + 0.80), (0.02, 0.16, 0.05))
            box(s, "eq_status_amber", (sx + 0.33, 0.25, sz + 0.72), (0.02, 0.16, 0.05))
            for yy in (-0.35, 0.0, 0.35):
                cyl(s, "eq_metal_graphite", (sx, yy, sz + 1.05), (sx, yy, sz + 1.17), 0.06, 10)
                lathe(s, "eq_insulator", [(0.04, 0), (0.035, 0.025), (0, 0.04)], 8, I3, (sx, yy, sz + 1.17))
            # cable conduit from the module to the coil ring
            cyl(s, "eq_metal_band", (sr + 0.17, 0.45, sz + 1.05), (sr + 0.17, 0.45, d["coil_z0"]), 0.05, 8)
            cyl(s, "eq_metal_band", (sr + 0.17, -0.45, sz + 1.05), (sr + 0.17, -0.45, d["coil_z0"]), 0.05, 8)
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
            lathe(m, "eq_steel", crect(0.06, 0.11, -0.10, 0.10), 12 if lod == 0 else 6, I3, (q[0], q[1], zz))
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
# Passport (CobraCobrettyER5PropulsionReactor.h, canon kb.reactor.er5): 5000 MWe D-He3, 25 t, body 5 x 5 x 5 m flange to
# flange, square cooling pad 11 x 11 m x 0.25 m (PT1) above it on a short heat-pipe column.
# Author 2026-11-09: the reactor has the drive's diameter so drive - reactor - drive read as one cylindrical engine bay.
# So the torus is COAXIAL with the bay (axis = OBJ Z, like the CR-1 axis): outer dia 4.4 m (+ hoops / units up to 5 m),
# the two flanges sit on its end faces (OBJ -Z fore, +Z aft; mating faces at z = -/+2.5) and mate the CR-1 flange exactly
# (r 2.25, 24 bolts on r 2.08, spigot groove r 1.50-1.80, busbar pocket r 0.68 with 3 sockets on r 0.32, data socket at
# y = -1.15). The pad is on top (+Y). Serviceable units = SRUs of the fault class reactor.fusion: pad, plasma_ctl,
# damper (confinement magnet hoops), feed (pellet feed), inverter, sensor.
ER5 = dict(r_in=0.95, r_out=2.2, zh=1.75, corner=0.55, face_z=2.5, flange_r=2.25,
           pad_y0=3.0, pad_t=0.25, pad_half=5.5)


def polz(r, a, z):
    return (r * math.cos(a), r * math.sin(a), z)


def er5_flange(lod, seg):
    """one flange in its local frame: mating face at z = 0 facing +Z, the neck going -Z onto the torus end face"""
    d = ER5
    f = Part("flange")
    fr = d["flange_r"]
    if lod == 0:   # the neck bottom is buried in the torus face and is not built
        prof = [(1.9, -0.80), (1.9, -0.30), (fr, -0.30), (fr, 0),
                (1.80, 0), (1.80, -0.10), (1.50, -0.10), (1.50, 0), (0.68, 0), (0.68, -0.27), (0, -0.27)]
    else:
        prof = [(1.9, -0.80), (1.9, -0.30), (fr, -0.30), (fr, 0), (0.68, 0), (0.68, -0.27), (0, -0.27)]
    lathe(f, "eq_metal_band", [p for i, p in enumerate(prof) if i == 0 or p != prof[i - 1]], seg)
    if lod == 0:
        for i in range(3):                                                              # busbar sockets
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.32 * math.cos(a), 0.32 * math.sin(a), -0.27)
            lathe(f, "eq_insulator", [(0.14, 0), (0.14, 0.04), (0, 0.04)], 12 if lod == 0 else 6, I3, p)
            lathe(f, "eq_copper", [(0.075, 0.04), (0.075, 0.07), (0, 0.07)], 12 if lod == 0 else 6, I3, p)
        box(f, "eq_metal_graphite", (0.0, -1.15, 0.004), (0.40, 0.26, 0.008))               # data socket plate
    if lod == 0:
        for i in range(24):                                                             # nuts of the 24 flange bolts
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.08 * math.cos(a), 2.08 * math.sin(a), -0.30), (0, 0, -1))
        for i in range(4):                                                              # data socket pins
            cyl(f, "eq_copper", (-0.12 + 0.08 * i, -1.15, 0.008), (-0.12 + 0.08 * i, -1.15, 0.012), 0.022, 8)
        for i in range(8):                                                              # gussets neck -> flange disc
            a = 2 * math.pi * i / 8 + math.pi / 8
            Rz = rot_z(a)
            box(f, "eq_metal_band", apply(Rz, (2.03, 0, 0))[:2] + (-0.52,), (0.30, 0.06, 0.44), Rz, chamfer=0.01)
    return f


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


def er5_parts(lod):
    seg = {0: 32, 1: 16, 2: 12}[lod]
    small = {0: 12, 1: 8, 2: 6}[lod]
    d = ER5
    ri, ro, zh, cr = d["r_in"], d["r_out"], d["zh"], d["corner"]
    nc = {0: 3, 1: 1, 2: 1}[lod]
    P = {}
    hoops = [math.pi / 8 + k * math.pi / 4 for k in range(8)]          # 22.5 deg + k * 45 deg: units sit between them

    # --- vessel: the coaxial torus, a coolant manifold band round the middle, access panels on the upper shoulders
    v = P["er5_vessel"] = Part("er5_vessel")
    lathe(v, "eq_metal_graphite", rrect(ri, ro, -zh, zh, cr, nc), seg, soft=35)
    if lod < 2:
        lathe(v, "eq_metal_band", crect(ro - 0.02, ro + 0.07, -0.09, 0.09, 0.015 if lod == 0 else 0.0), seg)
        for ac in (math.pi / 4, 3 * math.pi / 4):
            for z0, z1 in ((0.18, zh - cr - 0.05), (-(zh - cr - 0.05), -0.18)):
                lathe(v, "eq_metal_panel", crect(ro - 0.01, ro + 0.035, z0, z1, 0.01 if lod == 0 else 0.0), max(4, seg // 16),
                      I3, (0, 0, 0), ac - 0.30, ac + 0.30, cap=True)
                if lod == 0:
                    for aa in (ac - 0.25, ac + 0.25):
                        for zz in (z0 + 0.08, z1 - 0.08):
                            hex_bolt(v, "eq_steel", polz(ro + 0.035, aa, zz), polz(1, aa, 0), 0.04, 0.02, washer=False)
                    Rz = rot_z(ac)
                    box(v, "eq_steel", apply(Rz, (ro + 0.06, 0, 0))[:2] + ((z0 + z1) / 2,), (0.03, 0.035, 0.30), Rz, chamfer=0.008)

    # --- confinement magnets / dampers: 8 poloidal hoop housings round the tube cross-section
    m = P["er5_confinement"] = Part("er5_confinement")
    for a in hoops if lod < 2 else []:
        lathe(m, "eq_field_coil", rrect(ri - 0.07, ro + 0.07, -zh - 0.07, zh + 0.07, cr + 0.06, min(nc, 2)), small // 2, I3, (0, 0, 0),
              a - 0.07, a + 0.07, cap=True, soft=35)
        if lod == 0:
            lathe(m, "eq_metal_band", rrect(ri - 0.09, ro + 0.09, -zh - 0.09, zh + 0.09, cr + 0.07, 2), 2, I3, (0, 0, 0),
                  a - 0.02, a + 0.02, cap=True, soft=35)
        if lod == 0:
            Rz = rot_z(a)
            box(m, "eq_metal_band", apply(Rz, (ro + 0.13, 0, 0))[:2] + (0.0,), (0.10, 0.36, 0.70), Rz, chamfer=0.02)   # clamp block
            for zz in (-0.22, 0.22):
                hex_bolt(m, "eq_steel", polz(ro + 0.18, a, zz), polz(1, a, 0), 0.05, 0.03, washer=False)
            cyl(m, "eq_insulator", polz(ro + 0.18, a, 0.0), polz(ro + 0.28, a, 0.0), 0.05, 8)      # lead-in

    # --- flanges to the two drives (on the torus end faces)
    fl = er5_flange(lod, seg)
    P["er5_flange_aft"] = placed(fl, I3, (0, 0, d["face_z"]))
    P["er5_flange_aft"].name = "er5_flange_aft"
    P["er5_flange_fore"] = placed(fl, rot_y(math.pi), (0, 0, -d["face_z"]))
    P["er5_flange_fore"].name = "er5_flange_fore"

    # --- power inverter to the engine bus (+X side), two busbars to each flange
    iv = P["er5_inverter"] = Part("er5_inverter")
    xi = ro + 0.13
    box(iv, "eq_metal_panel", (xi, 0, 0), (0.30, 1.0, 1.25), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        for zs in (-1, 1):
            for yy in (-0.24, 0.24):
                a, b, c2 = (xi, yy, zs * 0.62), (xi, yy, zs * 1.70), (2.05, yy, zs * 2.18)
                cyl(iv, "eq_insulator", a, b, 0.07, small)
                cyl(iv, "eq_insulator", b, c2, 0.07, small)
                lathe(iv, "eq_insulator", [(0, -0.07), (0.07, -0.07), (0.07, 0.07), (0, 0.07)], small, I3, b)   # elbow
                if lod == 0:
                    cyl(iv, "eq_copper", a, (xi, yy, zs * 0.70), 0.085, small)
                    cyl(iv, "eq_steel", (xi, yy, zs * 1.15 - 0.04), (xi, yy, zs * 1.15 + 0.04), 0.09, small)   # support clamp
    if lod == 0:
        box(iv, "eq_metal_graphite", (xi + 0.155, 0.0, 0.0), (0.012, 0.80, 1.0), chamfer=0.004)           # front cover
        for yy in (-0.34, 0.34):
            for zz in (-0.44, 0.44):
                hex_bolt(iv, "eq_steel", (xi + 0.161, yy, zz), (1, 0, 0), 0.04, 0.02, washer=False)
        for i in range(3):
            box(iv, "eq_status_amber", (xi + 0.163, 0.22, -0.20 + 0.08 * i), (0.006, 0.05, 0.05))
        for i in range(7):                                                                             # cooling fins
            box(iv, "eq_metal_band", (xi, 0.51, -0.48 + 0.16 * i), (0.24, 0.02, 0.06))

    # --- pellet feed from the fuel bunker (-X side): feed housing, bunker port on top
    fd = P["er5_fuel_feed"] = Part("er5_fuel_feed")
    xf = -(ro + 0.13)
    box(fd, "eq_metal_panel", (xf, 0.02, 0), (0.30, 0.90, 0.95), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        cyl(fd, "eq_metal_band", (xf, 0.47, 0), (xf, 0.85, 0), 0.12, small)
        lathe(fd, "eq_steel", [(0, 0), (0.20, 0), (0.20, 0.05), (0, 0.05)], small, R_Y, (xf, 0.85, 0))
    if lod == 0:
        for i in range(6):
            a = 2 * math.pi * i / 6
            hex_bolt(fd, "eq_steel", (xf + 0.15 * math.cos(a), 0.90, 0.15 * math.sin(a)), (0, 1, 0), 0.03, 0.02, washer=False)
        box(fd, "eq_metal_graphite", (xf - 0.155, 0.0, 0.0), (0.012, 0.62, 0.66), chamfer=0.004)            # hatch
        box(fd, "eq_status_amber", (xf - 0.163, 0.22, 0.20), (0.006, 0.08, 0.10))                           # sight window
        for zz in (-0.24, 0.24):
            cyl(fd, "eq_steel", (xf - 0.17, -0.33, zz - 0.06), (xf - 0.17, -0.33, zz + 0.06), 0.02, 6)      # hinges

    # --- plasma control unit (under the vessel, -Y)
    pc = P["er5_plasma_ctl"] = Part("er5_plasma_ctl")
    yb = -(ro + 0.13)
    box(pc, "eq_metal_panel", (0, yb, 0), (0.90, 0.26, 1.0), chamfer=0.03 if lod == 0 else 0.0)
    if lod == 0:
        box(pc, "eq_status_amber", (0.30, yb - 0.135, 0.30), (0.10, 0.006, 0.06))
        for xx in (-0.30, -0.15, 0.0):
            cyl(pc, "eq_metal_graphite", (xx, yb - 0.13, -0.25), (xx, yb - 0.19, -0.25), 0.04, 8)
        cyl(pc, "eq_metal_band", (0.45, yb + 0.05, 0.0), polz(ro + 0.04, -math.pi / 2 + 0.55, 0.0), 0.05, 8)   # conduit

    # --- plasma sensor ports on the shoulders
    sn = P["er5_plasma_sensors"] = Part("er5_plasma_sensors")
    if lod == 0:
        for a, zz in ((math.pi / 4, 0.9), (math.pi / 4, -0.9), (3 * math.pi / 4, 0.9), (3 * math.pi / 4, -0.9),
                      (5 * math.pi / 4, 0.0), (7 * math.pi / 4, 0.0)):
            p0, p1 = polz(ro + 0.03, a, zz), polz(ro + 0.17, a, zz)
            cyl(sn, "eq_metal_graphite", p0, p1, 0.07, small, chamfer=0.01 if lod == 0 else 0.0)
            if lod == 0:
                lathe(sn, "eq_insulator", [(0.05, 0), (0.04, 0.03), (0, 0.05)], 8, axis_to(polz(1, a, 0)), p1)

    # --- heat-pipe column from the vessel top to the pad, cooling pad PT1 (11 x 11 x 0.25 m)
    heat_column(P.setdefault("er5_heat_pipe_column", Part("er5_heat_pipe_column")), lod, seg, small, ro, d["pad_y0"])
    cooling_pad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["pad_half"])

    # --- emergency connection to the shared coolant loop: the only copper lines (column foot -> port block, aft top)
    ec = P["er5_emergency_coolant"] = Part("er5_emergency_coolant")
    if lod < 2:
        box(ec, "eq_metal_panel", (0, ro + 0.08, 1.15), (0.46, 0.18, 0.34), chamfer=0.02 if lod == 0 else 0.0)
        for s in (-1, 1):
            x = 0.12 * s
            cyl(ec, "eq_copper", (x, ro + 0.22, 0.45), (x, ro + 0.22, 1.02), 0.05, small)
            cyl(ec, "eq_copper", (x, ro + 0.17, 1.15), (x, ro + 0.40, 1.15), 0.06, small)
            if lod == 0:
                lathe(ec, "eq_steel", [(0, 0), (0.10, 0), (0.10, 0.04), (0, 0.04)], small, R_Y, (x, ro + 0.40, 1.15))
                cyl(ec, "eq_copper", (x, ro + 0.22, 1.02), (x, ro + 0.22, 1.15), 0.05, small)

    # --- mounting lugs to the engine bay frame on the lower shoulders (no floor stand)
    mt = P["er5_mounts"] = Part("er5_mounts")
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for zz in (-0.9, 0.9):
            Rz = rot_z(a)
            box(mt, "eq_metal_band", apply(Rz, (ro + 0.10, 0, 0))[:2] + (zz,), (0.26, 0.42, 0.46), Rz, chamfer=0.02 if lod == 0 else 0.0)
            if lod < 2:
                c0 = apply(Rz, (ro + 0.18, 0, 0))[:2] + (zz,)
                lathe(mt, "eq_steel", crect(0.05, 0.10, -0.16, 0.16, 0.01 if lod == 0 else 0.0), small, I3, c0)
    return P


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


def cr1_hits():
    d = CR1
    sr, br, ft = d["shell_r"], d["band_r"], d["flange_t"]
    sx, sz = sr + 0.32, 2.95
    H = [hcyl("cr1_flange", (0, 0, -0.24), (0, 0, 0), 0.62),                       # busbar coupling boss
         hcyl("cr1_flange", (0, 0, -0.08), (0, 0, ft), d["flange_r"]),
         hcyl("cr1_housing", (0, 0, ft), (0, 0, d["coil_z0"]), br),
         hcyl("cr1_coil_ring", (0, 0, d["coil_z0"]), (0, 0, d["coil_z1"]), 2.47),
         hcyl("engine_hot_emitter_annulus", (0, 0, d["pad_z0"]), (0, 0, d["pad_z1"] + 0.05), d["pad_r"]),
         hbox("cr1_service_module", ((sr + sx + 0.31) / 2, 0, sz), (sx + 0.31 - sr, 1.30, 2.10)),
         hbox("cr1_service_module", (sx + 0.50, 0, sz - 0.62), (0.38, 1.10, 0.34))]   # coolant ports
    for a, zz in ((math.pi * 3 / 4, 1.05), (-math.pi * 3 / 4, 1.05), (math.pi * 3 / 4, 3.85), (-math.pi * 3 / 4, 3.85)):
        H.append(hbox("cr1_mounts", polz(2.28, a, zz), (0.42, 0.46, 0.20), (0, 0, math.degrees(a))))
    return H


def er5_hits():
    d = ER5
    ro, zh, fz = d["r_out"], d["zh"], d["face_z"]
    xi, xf, yb = ro + 0.13, -(ro + 0.13), -(ro + 0.13)
    H = [hcyl("er5_vessel", (0, 0, -zh), (0, 0, zh), ro)]
    for k in range(8):                                                                 # magnet hoops on the outer wall
        a = math.pi / 8 + k * math.pi / 4
        H.append(hbox("er5_confinement", polz(ro + 0.05, a, 0), (0.18, 0.34, 2 * zh + 0.14), (0, 0, math.degrees(a))))
    H += [hcyl("er5_flange_aft", (0, 0, fz - 0.80), (0, 0, fz), d["flange_r"]),
          hcyl("er5_flange_fore", (0, 0, -fz), (0, 0, -fz + 0.80), d["flange_r"]),
          hbox("er5_inverter", (xi, 0, 0), (0.32, 1.0, 1.25))]
    for zs in (-1, 1):                                                                 # busbar runs to the flanges
        H.append(hbox("er5_inverter", (2.20, 0, zs * 1.40), (0.40, 0.64, 1.56)))
    H += [hbox("er5_fuel_feed", (xf, 0.02, 0), (0.32, 0.90, 0.95)),
          hcyl("er5_fuel_feed", (xf, 0.47, 0), (xf, 0.90, 0), 0.20),
          hbox("er5_plasma_ctl", (0, yb, 0), (0.90, 0.26, 1.0))]
    for a, zz in ((math.pi / 4, 0.9), (math.pi / 4, -0.9), (3 * math.pi / 4, 0.9), (3 * math.pi / 4, -0.9),
                  (5 * math.pi / 4, 0.0), (7 * math.pi / 4, 0.0)):
        H.append(hcyl("er5_plasma_sensors", polz(ro + 0.03, a, zz), polz(ro + 0.22, a, zz), 0.07))
    H += [hbox("er5_heat_pipe_column", (0, ro + 0.02, 0), (0.80, 0.16, 1.30)),
          hcyl("er5_heat_pipe_column", (0, ro, 0), (0, d["pad_y0"], 0), 0.65),
          hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.03, 0), (2 * d["pad_half"], d["pad_t"] + 0.06, 2 * d["pad_half"])),
          hbox("er5_emergency_coolant", (0, ro + 0.23, 0.82), (0.46, 0.46, 0.86))]
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for zz in (-0.9, 0.9):
            H.append(hbox("er5_mounts", polz(ro + 0.12, a, zz), (0.30, 0.42, 0.46), (0, 0, math.degrees(a))))
    return H


# ---------------------------------------------------------------- reactor families (author 2026-11-09)
# One base concept for every propulsion reactor: a unit coaxial with the engine bay (axis = OBJ Z), flanges to the fore /
# aft drives on its ends (mating faces at z = -/+face_z), the cooling pad on a heat-pipe column on top (+Y), the same
# serviceable units (fault class reactor.fusion: pad, plasma_ctl, damper = confinement, feed, inverter, sensor).
# Each maker has its own confinement school and look:
#   Cobretty (Hellas, premium modular)   - compact coaxial tokamak drum, magnet hoops under a smooth skin (ER-5 above);
#   Sol Reactor Works (Atlantic school)  - linear FRC collider (reference: Helion Polaris): formation sections at both
#                                          ends, coil-wrapped accelerators tapering to a central compression chamber,
#                                          long capacitor banks (pulsed power + direct energy recovery); white / navy;
#   Tianhe Heavy (Pact state concern)    - heavy tokamak (reference: EAST / CFETR): flat-plate 12-sided cryostat, 12
#                                          exposed D-shaped TF coils standing out like a gear, big welded flanges;
#                                          olive steel / red coil casings.
# The smaller models of a family are the big one scaled to their passport envelope, with their own pad side.
SRW = dict(face_z=3.05, flange_r=1.70, form_r=1.55, form_z0=2.25, form_z1=2.80, comp_r=0.85, comp_z=0.75, cone_r=1.40,
           pad_y0=2.35, pad_t=0.25, pad_half=5.39)


def srw_flange(lod, seg):
    """SRW flange, local frame: mating face at z = 0 facing +Z, body towards -Z"""
    d = SRW
    f = Part("flange")
    fr = d["flange_r"]
    prof = [(fr - 0.05, -0.25), (fr, -0.25), (fr, 0), (0.55, 0), (0.55, -0.20), (0, -0.20)]
    lathe(f, "eq_metal_band", prof, seg)
    if lod < 2:
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.26 * math.cos(a), 0.26 * math.sin(a), -0.20)
            lathe(f, "eq_insulator", [(0.11, 0), (0.11, 0.03), (0, 0.03)], 12 if lod == 0 else 6, I3, p)
            lathe(f, "eq_copper", [(0.06, 0.03), (0.06, 0.06), (0, 0.06)], 12 if lod == 0 else 6, I3, p)
        box(f, "eq_metal_graphite", (0.0, -0.95, 0.004), (0.34, 0.22, 0.008))
    if lod == 0:
        for i in range(16):
            a = 2 * math.pi * (i + 0.5) / 16
            hex_bolt(f, "eq_steel", (1.58 * math.cos(a), 1.58 * math.sin(a), -0.25), (0, 0, -1), 0.08, 0.05)
    return f


def srw_parts(lod):
    seg = {0: 32, 1: 16, 2: 12}[lod]
    small = {0: 12, 1: 8, 2: 6}[lod]
    d = SRW
    fz0, fz1, fr = d["form_z0"], d["form_z1"], d["form_r"]
    P = {}

    # --- vessel: accelerators tapering to the compression chamber (open ends sit inside the formation sections)
    v = P["srw_vessel"] = Part("srw_vessel")
    cz, cr, kr = d["comp_z"], d["comp_r"], d["cone_r"]
    lathe(v, "eq_srw_white", [(kr, -fz0 - 0.05), (cr - 0.05, -cz - 0.10), (cr, -cz), (cr, cz), (cr - 0.05, cz + 0.10), (kr, fz0 + 0.05)], seg, soft=30)

    # --- confinement / compression coils (damper SRU): dense rings on the chamber, spaced rings on the accelerators
    m = P["srw_coils"] = Part("srw_coils")
    step = {0: 1, 1: 2, 2: 4}[lod]
    rings = [(-cz + 0.06 + i * (2 * cz - 0.12) / 10) for i in range(0, 11, step)]
    for z in rings:
        lathe(m, "eq_srw_coil", crect(cr, cr + 0.17, z - 0.05, z + 0.05), seg)
    for s in (-1, 1):
        for i in range(0, 6, step):
            z = cz + 0.25 + i * (fz0 - cz - 0.45) / 5
            rr = cr - 0.05 + (kr - cr + 0.05) * (z - cz - 0.10) / (fz0 - cz - 0.05)
            lathe(m, "eq_srw_coil", crect(rr, rr + 0.13, s * z - 0.05, s * z + 0.05), seg)

    # --- formation sections at both ends (white drums, two navy bands, an end ring)
    for name, s in (("srw_formation_fore", -1), ("srw_formation_aft", 1)):
        fm = P[name] = Part(name)
        z0, z1 = (fz0, fz1) if s > 0 else (-fz1, -fz0)
        prof = [(kr, z0), (fr, z0), (fr, z1), (fr - 0.25, z1)] if s > 0 else [(fr - 0.25, z0), (fr, z0), (fr, z1), (kr, z1)]
        lathe(fm, "eq_srw_white", prof, seg)
        if lod < 2:
            for zz in (z0 + 0.12, z1 - 0.12):
                lathe(fm, "eq_srw_coil", crect(fr - 0.01, fr + 0.07, zz - 0.05, zz + 0.05), seg)

    # --- flanges to the drives
    fl = srw_flange(lod, seg)
    P["srw_flange_aft"] = placed(fl, I3, (0, 0, d["face_z"]))
    P["srw_flange_aft"].name = "srw_flange_aft"
    P["srw_flange_fore"] = placed(fl, rot_y(math.pi), (0, 0, -d["face_z"]))
    P["srw_flange_fore"].name = "srw_flange_fore"

    # --- capacitor banks (inverter SRU: pulsed power + direct energy recovery): 4 long boxes on the diagonals
    cb = P["srw_capacitor_banks"] = Part("srw_capacitor_banks")
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        Rz = rot_z(a)
        box(cb, "eq_metal_panel", apply(Rz, (1.50, 0, 0))[:2] + (0.0,), (0.24, 0.55, 2 * fz0 + 0.40), Rz, chamfer=0.03 if lod == 0 else 0.0)
        if lod == 0:
            for i in range(1, 6):                                                       # cell dividers
                z = -fz0 + 0.10 + i * (2 * fz0 - 0.20) / 6
                box(cb, "eq_srw_coil", apply(Rz, (1.625, 0, 0))[:2] + (z,), (0.02, 0.57, 0.05), Rz)
            box(cb, "eq_status_amber", apply(Rz, (1.625, 0.18, 0))[:2] + (0.4,), (0.01, 0.06, 0.12), Rz)
        if lod < 2:
            for s in (-1, 1):                                                           # cables to the formation coils
                p0 = apply(Rz, (1.42, 0, 0))[:2] + (s * (fz0 - 0.15),)
                p1 = apply(Rz, (1.42, 0, 0))[:2] + (s * (fz0 + 0.12),)
                cyl(cb, "eq_insulator", p0, p1, 0.06, small)

    # --- fuel feed: He-3 / D gas valves on top of each formation section (-X side), one manifold box
    fd = P["srw_fuel_feed"] = Part("srw_fuel_feed")
    box(fd, "eq_metal_panel", (-1.62, 0.0, 0.0), (0.22, 0.45, 0.70), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        for s in (-1, 1):
            cyl(fd, "eq_steel", (-1.62, 0.0, s * 0.35), (-1.62, 0.0, s * (fz0 + 0.30)), 0.04, small)
            cyl(fd, "eq_metal_band", polz(fr, math.radians(160), s * (fz0 + 0.30)), polz(fr + 0.18, math.radians(160), s * (fz0 + 0.30)), 0.08, small)

    # --- plasma control unit under the compression chamber, sensor ports on its sides
    pc = P["srw_plasma_ctl"] = Part("srw_plasma_ctl")
    box(pc, "eq_metal_panel", (0, -(cr + 0.32), 0), (0.80, 0.22, 1.30), chamfer=0.03 if lod == 0 else 0.0)
    if lod == 0:
        box(pc, "eq_status_amber", (0.25, -(cr + 0.435), 0.4), (0.10, 0.006, 0.06))
        cyl(pc, "eq_metal_band", (0.3, -(cr + 0.21), 0.55), (0.3, -(cr + 0.21), cz + 0.3), 0.05, 8)
    sn = P["srw_plasma_sensors"] = Part("srw_plasma_sensors")
    if lod == 0:
        for a in (0.0, math.pi):
            for z in (-0.45, 0.45):
                cyl(sn, "eq_metal_graphite", polz(cr + 0.17, a, z), polz(cr + 0.32, a, z), 0.06, small)

    # --- heat-pipe column from the chamber coils to the pad, the pad (PT1)
    top = cr + 0.17
    heat_column(P.setdefault("srw_heat_pipe_column", Part("srw_heat_pipe_column")), lod, seg, small, top, d["pad_y0"],
                band="eq_srw_white")
    cooling_pad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["pad_half"],
                frame="eq_srw_white")

    # --- emergency coolant (the only copper) and the mounting lugs under the formation sections
    ec = P["srw_emergency_coolant"] = Part("srw_emergency_coolant")
    if lod < 2:
        box(ec, "eq_metal_panel", (0, top + 0.08, 1.15), (0.40, 0.16, 0.30))
        for s in (-1, 1):
            cyl(ec, "eq_copper", (0.11 * s, top + 0.20, 0.45), (0.11 * s, top + 0.20, 1.02), 0.045, small)
            cyl(ec, "eq_copper", (0.11 * s, top + 0.16, 1.15), (0.11 * s, top + 0.36, 1.15), 0.055, small)
    mt = P["srw_mounts"] = Part("srw_mounts")
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for s in (-1, 1):
            Rz = rot_z(a)
            zz = s * (fz0 + fz1) / 2
            box(mt, "eq_metal_band", apply(Rz, (fr + 0.10, 0, 0))[:2] + (zz,), (0.22, 0.36, 0.40), Rz)
            if lod < 2:
                lathe(mt, "eq_steel", crect(0.04, 0.08, -0.14, 0.14), small, I3, apply(Rz, (fr + 0.16, 0, 0))[:2] + (zz,))
    return P


TH = dict(face_z=3.105, flange_r=2.30, cryo_r=1.95, cryo_z=2.25, cone_z=2.55, neck_r=1.45, coil_r0=1.75, coil_out=0.70,
          coil_h=1.95, pad_y0=3.25, pad_t=0.25, pad_half=8.205)
TH_FACET0 = math.pi / 12          # 12-gon vertices at 15 + 30k deg: facet centres at 0, 30, 60 ... (units sit there)


def th_flange(lod):
    """Tianhe flange, local frame: mating face at z = 0 facing +Z; 12-sided welded plate, big bolts"""
    d = TH
    f = Part("flange")
    fr = d["flange_r"]
    lathe(f, "eq_th_steel", [(d["neck_r"], -0.70), (d["neck_r"], -0.35), (fr, -0.35), (fr, 0), (0.60, 0), (0.60, -0.22), (0, -0.22)],
          12, I3, (0, 0, 0), TH_FACET0, TH_FACET0 + 2 * math.pi, smooth=False)
    if lod < 2:
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.30 * math.cos(a), 0.30 * math.sin(a), -0.22)
            lathe(f, "eq_insulator", [(0.13, 0), (0.13, 0.04), (0, 0.04)], 12 if lod == 0 else 6, I3, p)
            lathe(f, "eq_copper", [(0.07, 0.04), (0.07, 0.07), (0, 0.07)], 12 if lod == 0 else 6, I3, p)
        box(f, "eq_metal_graphite", (0.0, -1.20, 0.004), (0.42, 0.26, 0.008))
        for i in range(12):                                                         # welded gussets neck -> plate
            a = TH_FACET0 + 2 * math.pi * i / 12
            Rz = rot_z(a)
            box(f, "eq_th_steel", apply(Rz, ((d["neck_r"] + fr) / 2, 0, 0))[:2] + (-0.52,), (fr - d["neck_r"], 0.10, 0.34), Rz)
    if lod == 0:
        for i in range(24):
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.10 * math.cos(a), 2.10 * math.sin(a), -0.35), (0, 0, -1), 0.12, 0.08)
    return f


def th_parts(lod):
    seg = {0: 32, 1: 16, 2: 12}[lod]
    small = {0: 12, 1: 8, 2: 6}[lod]
    d = TH
    R, cz, kz = d["cryo_r"], d["cryo_z"], d["cone_z"]
    ap = R * math.cos(math.pi / 12)                                                 # 12-gon apothem
    P = {}

    # --- cryostat: flat-plate 12-gon drum with conical end plates, two welded belts, diagnostic port plates
    v = P["th_cryostat"] = Part("th_cryostat")
    poly = lambda prof: lathe(v, "eq_th_steel", prof, 12, I3, (0, 0, 0), TH_FACET0, TH_FACET0 + 2 * math.pi, smooth=False)
    poly([(d["neck_r"], -kz), (R, -cz), (R, cz), (d["neck_r"], kz)])
    if lod < 2:
        for zz in (-1.25, 1.25):
            lathe(v, "eq_metal_band", crect(R - 0.02, R + 0.08, zz - 0.10, zz + 0.10), 12, I3, (0, 0, 0), TH_FACET0, TH_FACET0 + 2 * math.pi, smooth=False)
        for k in (2, 4, 8, 10):                                                     # port plates on facets 60/120/240/300 deg
            a = k * math.pi / 6
            Rz = rot_z(a)
            box(v, "eq_metal_panel", apply(Rz, (ap + 0.04, 0, 0))[:2] + (0.0,), (0.08, 0.70, 0.90), Rz)
            if lod == 0:
                for yy in (-0.28, 0.28):
                    for zz in (-0.38, 0.38):
                        hex_bolt(v, "eq_steel", apply(Rz, (ap + 0.08, yy, 0))[:2] + (zz,), apply(Rz, (1, 0, 0)), 0.07, 0.04)
    if lod == 0:                                                                    # longitudinal weld seams at the vertices
        for k in range(12):
            a = TH_FACET0 + k * math.pi / 6
            Rz = rot_z(a)
            box(v, "eq_metal_band", apply(Rz, (R + 0.01, 0, 0))[:2] + (0.0,), (0.04, 0.06, 2 * cz - 0.1), Rz)

    # --- 12 exposed D-shaped TF coils (damper / confinement SRU) standing out at the 12-gon vertices
    m = P["th_tf_coils"] = Part("th_tf_coils")
    n_arc = {0: 10, 1: 6, 2: 4}[lod]
    r0, ro, h = d["coil_r0"], d["coil_out"], d["coil_h"]
    pts = [(r0 + ro * math.cos(t), h * math.sin(t)) for t in (math.pi / 2 - math.pi * i / n_arc for i in range(n_arc + 1))]
    for k in range(12):
        a = TH_FACET0 + k * math.pi / 6
        Rz = rot_z(a)
        for (r1, z1), (r2, z2) in zip(pts, pts[1:]):
            L = math.hypot(r2 - r1, z2 - z1) + 0.05
            phi = math.atan2(r2 - r1, z2 - z1)
            c = apply(Rz, ((r1 + r2) / 2, 0, 0))
            box(m, "eq_th_coil", (c[0], c[1], (z1 + z2) / 2), (0.22, 0.30, L), mat_mul(Rz, rot_y(phi)))
        if lod == 0:                                                                # coil clamps on the cryostat belts
            for zz in (-1.25, 1.25):
                box(m, "eq_metal_band", apply(Rz, (R + 0.14, 0, 0))[:2] + (zz,), (0.20, 0.42, 0.24), Rz)

    # --- flanges
    fl = th_flange(lod)
    P["th_flange_aft"] = placed(fl, I3, (0, 0, d["face_z"]))
    P["th_flange_aft"].name = "th_flange_aft"
    P["th_flange_fore"] = placed(fl, rot_y(math.pi), (0, 0, -d["face_z"]))
    P["th_flange_fore"].name = "th_flange_fore"

    # --- inverter (+X facet) with busbars along the drum to both flanges
    iv = P["th_inverter"] = Part("th_inverter")
    box(iv, "eq_metal_panel", (ap + 0.17, 0, 0), (0.30, 0.62, 1.70))
    if lod < 2:
        for zs in (-1, 1):
            for yy in (-0.16, 0.16):
                cyl(iv, "eq_insulator", (ap + 0.17, yy, zs * 0.85), (ap + 0.17, yy, zs * 2.78), 0.07, small)
    if lod == 0:
        box(iv, "eq_metal_graphite", (ap + 0.325, 0, 0), (0.012, 0.50, 1.40))
        for i in range(3):
            box(iv, "eq_status_amber", (ap + 0.333, 0.16, -0.5 + 0.1 * i), (0.006, 0.06, 0.06))

    # --- fuel feed (-X facet), plasma control (bottom facet), sensors on the upper facets
    fd = P["th_fuel_feed"] = Part("th_fuel_feed")
    box(fd, "eq_metal_panel", (-(ap + 0.17), 0, 0), (0.30, 0.62, 1.10))
    if lod < 2:
        cyl(fd, "eq_metal_band", (-(ap + 0.17), 0.31, 0), (-(ap + 0.17), 0.70, 0), 0.12, small)
    pc = P["th_plasma_ctl"] = Part("th_plasma_ctl")
    box(pc, "eq_metal_panel", (0, -(ap + 0.15), 0), (0.62, 0.26, 1.40))
    sn = P["th_plasma_sensors"] = Part("th_plasma_sensors")
    if lod == 0:
        for a in (math.pi / 6, 5 * math.pi / 6):
            for zz in (-0.7, 0.7):
                cyl(sn, "eq_metal_graphite", polz(ap, a, zz), polz(ap + 0.18, a, zz), 0.08, small)

    # --- column + pad, emergency coolant, lugs
    heat_column(P.setdefault("th_heat_pipe_column", Part("th_heat_pipe_column")), lod, seg, small, ap, d["pad_y0"],
                band="eq_th_steel")
    cooling_pad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["pad_half"],
                frame="eq_th_steel")
    ec = P["th_emergency_coolant"] = Part("th_emergency_coolant")
    if lod < 2:
        box(ec, "eq_metal_panel", (0, ap + 0.08, 1.30), (0.44, 0.16, 0.34))
        for s in (-1, 1):
            cyl(ec, "eq_copper", (0.12 * s, ap + 0.20, 0.45), (0.12 * s, ap + 0.20, 1.15), 0.05, small)
            cyl(ec, "eq_copper", (0.12 * s, ap + 0.16, 1.30), (0.12 * s, ap + 0.40, 1.30), 0.06, small)
    mt = P["th_mounts"] = Part("th_mounts")
    for a in (7 * math.pi / 6, 11 * math.pi / 6):
        for zz in (-1.8, 1.8):
            Rz = rot_z(a)
            box(mt, "eq_metal_band", apply(Rz, (ap + 0.14, 0, 0))[:2] + (zz,), (0.28, 0.46, 0.50), Rz)
            if lod < 2:
                lathe(mt, "eq_steel", crect(0.05, 0.11, -0.18, 0.18), small, I3, apply(Rz, (ap + 0.22, 0, 0))[:2] + (zz,))
    return P


# --- smaller family members: the big unit scaled to the passport envelope, own pad side, part prefix renamed
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


def srw_hits():
    d = SRW
    fz0, fz1, fr = d["form_z0"], d["form_z1"], d["form_r"]
    H = [hcyl("srw_vessel", (0, 0, -fz0), (0, 0, fz0), d["cone_r"]),
         hcyl("srw_coils", (0, 0, -d["comp_z"]), (0, 0, d["comp_z"]), d["comp_r"] + 0.17),
         hcyl("srw_formation_fore", (0, 0, -fz1), (0, 0, -fz0), fr + 0.07),
         hcyl("srw_formation_aft", (0, 0, fz0), (0, 0, fz1), fr + 0.07),
         hcyl("srw_flange_fore", (0, 0, -d["face_z"]), (0, 0, -fz1), d["flange_r"]),
         hcyl("srw_flange_aft", (0, 0, fz1), (0, 0, d["face_z"]), d["flange_r"])]
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        H.append(hbox("srw_capacitor_banks", polz(1.50, a, 0), (0.26, 0.57, 2 * fz0 + 0.40), (0, 0, math.degrees(a))))
    cr, top = d["comp_r"], d["comp_r"] + 0.17
    H += [hbox("srw_fuel_feed", (-1.62, 0, 0), (0.22, 0.45, 0.70)),
          hbox("srw_plasma_ctl", (0, -(cr + 0.32), 0), (0.80, 0.22, 1.30)),
          hbox("srw_heat_pipe_column", (0, top + 0.02, 0), (0.80, 0.16, 1.30)),
          hcyl("srw_heat_pipe_column", (0, top, 0), (0, d["pad_y0"], 0), 0.65),
          hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.03, 0), (2 * d["pad_half"], d["pad_t"] + 0.06, 2 * d["pad_half"])),
          hbox("srw_emergency_coolant", (0, top + 0.20, 0.80), (0.40, 0.40, 0.80))]
    for a in (0.0, math.pi):
        for z in (-0.45, 0.45):
            H.append(hcyl("srw_plasma_sensors", polz(cr + 0.17, a, z), polz(cr + 0.32, a, z), 0.06))
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for s in (-1, 1):
            H.append(hbox("srw_mounts", polz(fr + 0.10, a, s * (fz0 + fz1) / 2), (0.22, 0.36, 0.40), (0, 0, math.degrees(a))))
    return H


def th_hits():
    d = TH
    R = d["cryo_r"]
    ap = R * math.cos(math.pi / 12)
    H = [hcyl("th_cryostat", (0, 0, -d["cone_z"]), (0, 0, d["cone_z"]), R)]
    for k in range(12):
        a = TH_FACET0 + k * math.pi / 6
        H.append(hbox("th_tf_coils", polz(R + 0.25, a, 0), (0.50, 0.30, 3.70), (0, 0, math.degrees(a))))
    H += [hcyl("th_flange_fore", (0, 0, -d["face_z"]), (0, 0, -d["face_z"] + 0.70), d["flange_r"]),
          hcyl("th_flange_aft", (0, 0, d["face_z"] - 0.70), (0, 0, d["face_z"]), d["flange_r"]),
          hbox("th_inverter", (ap + 0.17, 0, 0), (0.30, 0.62, 1.70))]
    for zs in (-1, 1):
        H.append(hbox("th_inverter", (ap + 0.17, 0, zs * 1.82), (0.16, 0.48, 1.94)))
    H += [hbox("th_fuel_feed", (-(ap + 0.17), 0, 0), (0.30, 0.62, 1.10)),
          hcyl("th_fuel_feed", (-(ap + 0.17), 0.31, 0), (-(ap + 0.17), 0.70, 0), 0.12),
          hbox("th_plasma_ctl", (0, -(ap + 0.15), 0), (0.62, 0.26, 1.40)),
          hbox("th_heat_pipe_column", (0, ap + 0.02, 0), (0.80, 0.16, 1.30)),
          hcyl("th_heat_pipe_column", (0, ap, 0), (0, d["pad_y0"], 0), 0.65),
          hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.03, 0), (2 * d["pad_half"], d["pad_t"] + 0.06, 2 * d["pad_half"])),
          hbox("th_emergency_coolant", (0, ap + 0.22, 0.88), (0.44, 0.44, 1.0))]
    for a in (math.pi / 6, 5 * math.pi / 6):
        for zz in (-0.7, 0.7):
            H.append(hcyl("th_plasma_sensors", polz(ap, a, zz), polz(ap + 0.18, a, zz), 0.08))
    for a in (7 * math.pi / 6, 11 * math.pi / 6):
        for zz in (-1.8, 1.8):
            H.append(hbox("th_mounts", polz(ap + 0.14, a, zz), (0.28, 0.46, 0.50), (0, 0, math.degrees(a))))
    return H


# family scale factors: passport envelope / big unit envelope (ER-1 2.34 m / 5; SR-2.6 2.85 / 3.4; TR-2.6V 3.04 / 4.97)
ER1_S, SR26_S, TR26_S = 2.34 / 5.0, 2.85 / 3.4, 3.04 / 4.97
HITS = {"cobretty_cr1_vector": cr1_hits, "cobretty_er5_reactor": er5_hits,
        "cobretty_er1_reactor": scaled_hits(er5_hits, ER5, ER1_S, 4.48, {"er5_": "er1_"}),
        "srw_sr48_reactor": srw_hits, "srw_sr26_reactor": scaled_hits(srw_hits, SRW, SR26_S, 7.92),
        "tianhe_tr10_reactor": th_hits, "tianhe_tr26v_reactor": scaled_hits(th_hits, TH, TR26_S, 7.78)}


def hits_text(asset):
    vols = HITS[asset]()
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


# ---------------------------------------------------------------- output
ASSETS = {"cobretty_cr1_vector": cr1_parts, "cobretty_er5_reactor": er5_parts,
          "cobretty_er1_reactor": scaled_family(er5_parts, ER5, ER1_S, 4.48, {"er5_": "er1_"}),
          "srw_sr48_reactor": srw_parts, "srw_sr26_reactor": scaled_family(srw_parts, SRW, SR26_S, 7.92),
          "tianhe_tr10_reactor": th_parts, "tianhe_tr26v_reactor": scaled_family(th_parts, TH, TR26_S, 7.78)}


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
        if asset in HITS:
            files[os.path.join(OUT, asset, "hit_volumes.json")] = hits_text(asset)
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
