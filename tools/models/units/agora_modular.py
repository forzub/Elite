"""Agora Modular (Lalande / Hellas transit port, open-standard consortium of many small firms): bulk hold, C20 rack,
docking collar, drone bay, escape capsule, lifeboat, passenger section, field RCS cluster, jump-charge tank, pellet bunker.

School (DESIGN_SCHOOLS.md): container-like and hexagonal modules with ISO-style corner castings (the open interface:
twist locks into the oval casting holes), longitudinal side rails, skin panels from different subcontractors (patchwork
of warm white / terracotta / sand / sea blue, each bay its own panel thickness and fastener pattern); close-up: mixed
connectors, adapter plates, brass fittings."""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_agora_white": ((0.86, 0.84, 0.78), (0.15, 0.15, 0.15), 30),
    "eq_agora_terracotta": ((0.62, 0.33, 0.22), (0.12, 0.10, 0.08), 20),
    "eq_agora_blue": ((0.18, 0.36, 0.52), (0.25, 0.25, 0.28), 50),
    "eq_agora_sand": ((0.68, 0.63, 0.50), (0.10, 0.10, 0.10), 20),
    "eq_agora_casting": ((0.26, 0.26, 0.27), (0.30, 0.30, 0.30), 60),
    "eq_agora_brass": ((0.72, 0.56, 0.26), (0.60, 0.50, 0.30), 90),
    "eq_agora_hole": ((0.04, 0.04, 0.045), (0.02, 0.02, 0.02), 5),
    "eq_agora_glass": ((0.08, 0.12, 0.16), (0.70, 0.70, 0.70), 140),
    "eq_agora_rubber": ((0.09, 0.09, 0.09), (0.05, 0.05, 0.05), 10),
})
WH, TC, BL, SD = "eq_agora_white", "eq_agora_terracotta", "eq_agora_blue", "eq_agora_sand"
CA, BR, HO, GL, RB = "eq_agora_casting", "eq_agora_brass", "eq_agora_hole", "eq_agora_glass", "eq_agora_rubber"
PATCH = (WH, TC, SD, WH, BL, SD, TC, WH, SD)


def pmat(k):
    """the subcontractor of panel k (deterministic patchwork)"""
    return PATCH[(k * 5 + 3) % len(PATCH)]


# ---------------------------------------------------------------- local helpers
PERM = {"z": I3, "x": [[0, 0, 1], [0, 1, 0], [1, 0, 0]], "y": [[1, 0, 0], [0, 0, 1], [0, 1, 0]]}


def _c2(poly):
    return (sum(p[0] for p in poly) / len(poly), sum(p[1] for p in poly) / len(poly))


def loft(part, mat, rings, axis="z", t=(0, 0, 0), caps=(True, True), inward=False):
    """flat-shaded loft through convex (u, v) polygons [(poly, w)] (same vertex count) along `axis`
    (z: (u, v) = (x, y); x: (u, v) = (z, y); y: (u, v) = (x, z)); caps close the first / last ring"""
    R = PERM[axis]
    n = len(rings[0][0])
    for (pa, wa), (pb, wb) in zip(rings, rings[1:]):
        ca, cb = _c2(pa), _c2(pb)
        cu, cv, cw = (ca[0] + cb[0]) / 2, (ca[1] + cb[1]) / 2, (wa + wb) / 2
        for i in range(n):
            j = (i + 1) % n
            pts = [(pa[i][0], pa[i][1], wa), (pa[j][0], pa[j][1], wa), (pb[j][0], pb[j][1], wb), (pb[i][0], pb[i][1], wb)]
            if pts[0] == pts[3] and pts[1] == pts[2]:
                continue
            fn = facet_normal(pts if pts[0] != pts[1] else pts[1:]) or (0, 0, 1)
            mid = [sum(p[k] for p in pts) / 4 for k in range(3)]
            ref = (mid[0] - cu, mid[1] - cv, 0.0) if abs(wa - wb) > 1e-9 else (0.0, 0.0, 1.0 if wb > 0 else -1.0)
            if (fn[0] * ref[0] + fn[1] * ref[1] + fn[2] * ref[2] < 0) != inward:
                fn = (-fn[0], -fn[1], -fn[2])
            part.face(mat, [part.vert(p, fn, R, t) for p in pts], apply(R, fn))
    for k, (poly, w) in ((0, rings[0]), (1, rings[-1])):
        if not caps[k]:
            continue
        other = rings[1][1] if k == 0 else rings[-2][1]
        nn = (0.0, 0.0, 1.0 if w > other else -1.0)
        part.face(mat, [part.vert((u, v, w), nn, R, t) for u, v in poly], apply(R, nn))


def annulus(part, mat, outer, inner, w, sign, axis="z", t=(0, 0, 0)):
    """flat ring face between two convex polygons with the same vertex count, facing `sign` along the axis"""
    R = PERM[axis]
    nn = (0.0, 0.0, float(sign))
    n = len(outer)
    for i in range(n):
        j = (i + 1) % n
        pts = [(outer[i][0], outer[i][1], w), (outer[j][0], outer[j][1], w), (inner[j][0], inner[j][1], w), (inner[i][0], inner[i][1], w)]
        part.face(mat, [part.vert(p, nn, R, t) for p in pts], apply(R, nn))


def hexp(ax, bx, ay, s=1.0):
    """stretched hexagon with vertices on +-X (counter-clockwise)"""
    return [(ax * s, 0.0), (bx * s, ay * s), (-bx * s, ay * s), (-ax * s, 0.0), (-bx * s, -ay * s), (bx * s, -ay * s)]


def octp(h, c, s=1.0):
    """octagon of half size h with corner cuts c (counter-clockwise)"""
    return [(x * s, y * s) for x, y in ((h, -h + c), (h, h - c), (h - c, h), (-h + c, h), (-h, h - c), (-h, -h + c), (-h + c, -h), (h - c, -h))]


def pan(p, mat, axis, sign, plane, cu, cv, su, sv, t, chamfer=0.0, R=None):
    """a plate of thickness t lying on the face plane of `axis` (0/1/2), raised towards `sign`;
    (u, v) = the other two axes in index order"""
    u, v = [a for a in range(3) if a != axis]
    c, s = [0.0] * 3, [0.0] * 3
    c[axis], s[axis] = plane + sign * t / 2, t
    c[u], c[v], s[u], s[v] = cu, cv, su, sv
    box(p, mat, tuple(c), tuple(s), R or I3, chamfer=chamfer)


def bolt_grid(p, axis, sign, plane, u0, u1, v0, v1, nu, nv, across, h=0.025, edge_only=True):
    u, v = [a for a in range(3) if a != axis]
    d = [0.0] * 3
    d[axis] = sign
    for i in range(nu):
        for j in range(nv):
            if edge_only and 0 < i < nu - 1 and 0 < j < nv - 1:
                continue
            q = [0.0] * 3
            q[axis] = plane
            q[u] = u0 + (u1 - u0) * (i / (nu - 1) if nu > 1 else 0.5)
            q[v] = v0 + (v1 - v0) * (j / (nv - 1) if nv > 1 else 0.5)
            hex_bolt(p, "eq_steel", tuple(q), tuple(d), across, h, washer=False)


def casting(p, c, s, out, lod, R=I3):
    """ISO-style corner casting (the Agora open interface): a block with oval twist-lock holes on its outward faces;
    out = outward signs per local axis (0 = no hole on that axis)"""
    box(p, CA, c, (s, s, s), R, chamfer=0.012 if close() else 0.0)
    if lod == 0:
        for ax in range(3):
            if not out[ax]:
                continue
            q = [0.0, 0.0, 0.0]
            q[ax] = out[ax] * (s / 2 + 0.002)
            size = [s * 0.30] * 3
            size[ax] = 0.006
            size[(ax + 1) % 3] = s * 0.56
            q = apply(R, q)
            box(p, HO, (c[0] + q[0], c[1] + q[1], c[2] + q[2]), tuple(size), R)


def aframe(p, lo, hi, b, cs, lod, rail=BL, skip=()):
    """container frame: 12 edge members flush with the outer box and 8 corner castings; skip = axes without members"""
    for a in range(3):
        if a in skip:
            continue
        u, v = [x for x in range(3) if x != a]
        for su in (0, 1):
            for sv in (0, 1):
                c, s = [0.0] * 3, [0.0] * 3
                c[a], s[a] = (lo[a] + hi[a]) / 2, hi[a] - lo[a] - 2 * cs
                c[u] = lo[u] + b / 2 if su == 0 else hi[u] - b / 2
                c[v] = lo[v] + b / 2 if sv == 0 else hi[v] - b / 2
                s[u] = s[v] = b
                box(p, rail, tuple(c), tuple(s), chamfer=0.01 if close() else 0.0)
    for sx in (0, 1):
        for sy in (0, 1):
            for sz in (0, 1):
                c = (lo[0] + cs / 2 if sx == 0 else hi[0] - cs / 2, lo[1] + cs / 2 if sy == 0 else hi[1] - cs / 2,
                     lo[2] + cs / 2 if sz == 0 else hi[2] - cs / 2)
                casting(p, c, cs, (sx * 2 - 1, sy * 2 - 1, sz * 2 - 1), lod)


def brass_fitting(p, c, d, r, lod):
    """close-up: a mixed connector - brass hex body, insulator collar, cap"""
    seg = small_for(lod)
    R = axis_to(norm(d))
    lathe(p, BR, [(r * 1.25, 0), (r * 1.25, r * 0.8), (r, r * 0.8), (r, r * 2.2), (0, r * 2.2)], 6, R, c, smooth=False)
    lathe(p, "eq_insulator", [(r * 0.7, r * 2.2), (r * 0.7, r * 2.8), (0, r * 2.8)], seg, R, c)


def ring_frames(p, mat, lo, hi, b, stations, depth, prot):
    """transverse portal frames standing `prot` proud of the skin (skin plane = box hi/lo inset by b)"""
    for z in stations:
        for sy in (-1, 1):
            y = hi[1] - prot / 2 if sy > 0 else lo[1] + prot / 2
            box(p, mat, ((lo[0] + hi[0]) / 2, y, z), (hi[0] - lo[0] - 2 * b, prot, depth))
        for sx in (-1, 1):
            x = hi[0] - prot / 2 if sx > 0 else lo[0] + prot / 2
            box(p, mat, (x, (lo[1] + hi[1]) / 2, z), (prot, hi[1] - lo[1] - 2 * b, depth))


# ================================================================= GO-100 bulk hold (std.cargo.hold100)
HOLD = dict(hx=2.596, hy=1.8545, hz=3.709, cs=0.24, b=0.20, sk=0.12)


def hold_parts(lod):
    d = HOLD
    hx, hy, hz, cs, b, sk = d["hx"], d["hy"], d["hz"], d["cs"], d["b"], d["sk"]
    P = {}
    stations = [-hz + k * 2 * hz / 6 for k in range(1, 6)]

    fr = P["hold100_frame"] = Part("hold100_frame")
    aframe(fr, (-hx, -hy, -hz), (hx, hy, hz), b, cs, lod)
    ring_frames(fr, BL if lod else CA, (-hx + 0.03, -hy + 0.03, 0), (hx - 0.03, hy - 0.03, 0), b - 0.03,
                stations if lod < 2 else stations[1::2], 0.14, sk - 0.02)
    if close():                                                  # bottom twist-lock adapter plates, grounding studs
        for sx in (-1, 1):
            for sz in (-1, 1):
                box(fr, "eq_steel", (sx * (hx - cs - 0.12), -hy + 0.01, sz * (hz - cs / 2)), (0.20, 0.02, 0.18))
                brass_fitting(fr, (sx * (hx - 0.08), -hy + b + 0.02, sz * (hz - cs - 0.10)), (sx, 0, 0), 0.02, lod)

    sh = P["hold100_shell"] = Part("hold100_shell")
    box(sh, WH, (0, 0, -0.03), (2 * (hx - sk), 2 * (hy - sk), 2 * hz - 0.18))
    if lod < 2:
        edges = [-hz + cs] + stations + [hz - cs]
        for k in range(6):
            z0, z1 = edges[k] + 0.09, edges[k + 1] - 0.09
            zc, zl = (z0 + z1) / 2, z1 - z0
            for f, (axis, sign) in enumerate(((0, 1), (0, -1), (1, 1), (1, -1))):
                if lod == 1 and f == 3:
                    continue
                t = 0.02 + 0.012 * ((k + f) % 3)
                plane = sign * ((hx if axis == 0 else hy) - sk)
                other = (hy if axis == 0 else hx) - b - 0.05
                mat = pmat(k * 4 + f)
                if axis == 0:
                    pan(sh, mat, 0, sign, plane, 0.0, zc, 2 * other, zl, t)
                else:
                    pan(sh, mat, 1, sign, plane, 0.0, zc, 2 * other, zl, t)
                if lod == 0 and axis == 0 and (k + f) % 2 == 0:          # corrugated panel (one subcontractor)
                    n = int(zl / 0.36)
                    for i in range(n):
                        z = z0 + (i + 0.5) * zl / n
                        pan(sh, mat, 0, sign, plane + sign * t, 0.0, z, 2 * other - 0.1, 0.12, 0.035)
                if close():
                    style = (k * 3 + f) % 3
                    if style == 0:
                        bolt_grid(sh, axis, sign, plane + sign * t, -other + 0.06, other - 0.06, z0 + 0.06, z1 - 0.06, 6, 4, 0.05)
                    elif style == 1:
                        bolt_grid(sh, axis, sign, plane + sign * t, -other + 0.05, other - 0.05, z0 + 0.04, z1 - 0.04, 12, 2, 0.03, 0.012)
        # forward end wall: patchwork panels and a personnel hatch
        zf = -hz + 0.09
        for i, (x0, x1) in enumerate(((-hx + b, -0.6), (-0.6, 0.6), (0.6, hx - b))):
            pan(sh, pmat(i + 11), 2, -1, zf, (x0 + x1) / 2 + 0.0, 0.0, x1 - x0 - 0.06, 2 * (hy - b) - 0.06, 0.03)
        pan(sh, CA, 2, -1, zf - 0.03, 0.0, -hy + b + 1.05, 0.96, 1.96, 0.03)
        pan(sh, SD, 2, -1, zf - 0.06, 0.0, -hy + b + 1.05, 0.84, 1.84, 0.03, chamfer=0.02)
        if lod == 0:
            box(sh, BR, (0.30, -hy + b + 1.0, zf - 0.11), (0.05, 0.22, 0.04))
            box(sh, BL, (0.0, -hy + b + 1.75, zf - 0.095), (0.40, 0.10, 0.01))       # hatch label plate
        if close():
            for i in range(4):                                                      # mixed service connectors
                brass_fitting(sh, (-1.4 + 0.18 * i, -0.9, zf - 0.03), (0, 0, -1), 0.025 + 0.006 * (i % 2), lod)
            box(sh, "eq_steel", (-1.13, -0.9, zf - 0.035), (0.8, 0.25, 0.01))      # adapter plate

    dr = P["hold100_door"] = Part("hold100_door")
    zd = hz - sk
    lw = hx - b - 0.02
    for s in (-1, 1):
        pan(dr, TC if s < 0 else SD, 2, 1, zd, s * (lw / 2 + 0.01), 0.0, lw - 0.02, 2 * (hy - b) - 0.04, 0.06)
        if lod < 2:
            for xb in (0.30, lw - 0.55):                                            # locking bars, keepers, handles
                x = s * xb
                cyl(dr, "eq_steel", (x, -hy + b + 0.06, zd + 0.11), (x, hy - b - 0.06, zd + 0.11), 0.035, small_for(lod))
                for y in (-hy + b + 0.10, hy - b - 0.10):
                    box(dr, BR, (x, y, zd + 0.09), (0.14, 0.12, 0.07))
                if lod == 0:
                    for y in (-0.9, 0.3, 1.3):
                        box(dr, "eq_steel", (x, y, zd + 0.08), (0.12, 0.05, 0.05))
                    box(dr, TC, (x + s * 0.16, -0.35, zd + 0.13), (0.32, 0.06, 0.04))
            for y in (-1.15, -0.4, 0.4, 1.15):                                      # hinges on the outer edge
                cyl(dr, "eq_steel", (s * (lw - 0.02), y - 0.11, zd + 0.09), (s * (lw - 0.02), y + 0.11, zd + 0.09), 0.05, small_for(lod))
        if lod == 0:                                                                # door ribs (another subcontractor)
            for y in (-1.2, 0.0, 1.2):
                pan(dr, TC if s < 0 else SD, 2, 1, zd + 0.06, s * (lw / 2 + 0.12), y, lw - 0.6, 0.10, 0.03)
            box(dr, BL, (s * (lw / 2), 1.0, zd + 0.065), (0.6, 0.18, 0.01))         # stencil plate

    se = P["hold100_seal"] = Part("hold100_seal")
    if lod < 2:
        for y in (-1, 1):
            box(se, RB, (0, y * (hy - b - 0.005), zd + 0.03), (2 * (hx - b), 0.04, 0.06))
        for x in (-1, 1):
            box(se, RB, (x * (hx - b - 0.005), 0, zd + 0.03), (0.04, 2 * (hy - b), 0.06))
        box(se, RB, (0, 0, zd + 0.035), (0.03, 2 * (hy - b) - 0.04, 0.07))
        if close():
            for y in (-1, 1):
                for x in (-1, 1):
                    box(se, "eq_steel", (x * (hx - b - 0.05), y * (hy - b - 0.05), zd + 0.07), (0.10, 0.10, 0.02))
    return P


def hold_hits():
    d = HOLD
    hx, hy, hz, b, sk = d["hx"], d["hy"], d["hz"], d["b"], d["sk"]
    H = [hbox("hold100_shell", (0, 0, -0.03), (2 * (hx - sk) + 0.08, 2 * (hy - sk) + 0.08, 2 * hz - 0.18))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            H.append(hbox("hold100_frame", (sx * (hx - b / 2), sy * (hy - b / 2), 0), (b, b, 2 * hz)))
        H.append(hbox("hold100_frame", (sx * (hx - b / 2), 0, hz - b / 2), (b, 2 * hy, b)))
        H.append(hbox("hold100_frame", (sx * (hx - b / 2), 0, -hz + b / 2), (b, 2 * hy, b)))
    H.append(hbox("hold100_door", (0, 0, hz - sk + 0.08), (2 * (hx - b), 2 * (hy - b), 0.16)))
    for y in (-1, 1):
        H.append(hbox("hold100_seal", (0, y * (hy - b - 0.005), hz - sk + 0.03), (2 * (hx - b), 0.05, 0.08)))
    for x in (-1, 1):
        H.append(hbox("hold100_seal", (x * (hx - b - 0.005), 0, hz - sk + 0.03), (0.05, 2 * (hy - b), 0.08)))
    return H


# ================================================================= C20-R container rack (std.cargo.c20lock)
C20 = dict(hx=1.22, hy=1.295, hz=3.03, rx=1.08)


def c20_parts(lod):
    d = C20
    hx, hy, hz, rx = d["hx"], d["hy"], d["hz"], d["rx"]
    small = small_for(lod)
    P = {}
    ra = P["c20_rail"] = Part("c20_rail")
    for s in (-1, 1):                                                     # two I-beam floor rails
        x = s * rx
        box(ra, BL, (x, -hy + 0.015, 0), (0.24, 0.03, 2 * hz))
        if lod < 2:
            box(ra, BL, (x, -hy + 0.10, 0), (0.04, 0.14, 2 * hz - 0.04))
            box(ra, BL, (x, -hy + 0.185, 0), (0.20, 0.03, 2 * hz - 0.04))
        else:
            box(ra, BL, (x, -hy + 0.11, 0), (0.12, 0.16, 2 * hz))
        if lod == 0:                                                      # guide rollers in the web
            for k in range(7):
                z = -hz + 0.5 + k * (2 * hz - 1.0) / 6
                cyl(ra, "eq_steel", (x - s * 0.02, -hy + 0.17, z), (x - s * 0.13, -hy + 0.17, z), 0.045, small)
        # upper guide rails along the top edges
        box(ra, BL if lod else SD, (s * (hx - 0.05), hy - 0.05, 0), (0.06, 0.10, 2 * hz - 0.36))
    for z in (-2.3, -0.77, 0.77, 2.3):                                    # cross members
        box(ra, CA, (0, -hy + 0.07, z), (2 * rx - 0.24, 0.10, 0.16))
    box(ra, CA, (0, -hy + 0.32, -hz + 0.10), (2 * rx - 0.3, 0.20, 0.20))     # end stop beam
    if lod < 2:
        for s in (-1, 1):
            cyl(ra, RB, (s * 0.6, -hy + 0.32, -hz + 0.20), (s * 0.6, -hy + 0.32, -hz + 0.32), 0.09, small)
    if close():
        for s in (-1, 1):
            for z in (-2.3, -0.77, 0.77, 2.3):
                bolt_grid(ra, 1, 1, -hy + 0.03, s * rx - 0.09, s * rx + 0.09, z - 0.06, z + 0.06, 2, 2, 0.035)

    fr = P["c20_frame"] = Part("c20_frame")
    yb, yt = -hy + 0.20, hy
    for sx in (-1, 1):
        for sz in (-1, 1):
            x, z = sx * (hx - 0.012), sz * (hz - 0.012)
            box(fr, TC if (sx + sz) == 0 else CA, (sx * (hx - 0.08), (yb + yt) / 2, z), (0.16, yt - yb, 0.025))     # L-angle posts
            box(fr, TC if (sx + sz) == 0 else CA, (x, (yb + yt) / 2, sz * (hz - 0.08)), (0.025, yt - yb, 0.16))
            box(fr, CA, (sx * (hx - 0.12), yb - 0.01, sz * (hz - 0.12)), (0.24, 0.02, 0.24))                      # base plate
            if lod < 2:                                                   # lead-in flares at the post tops
                box(fr, CA, (sx * (hx - 0.03), yt - 0.12, sz * (hz - 0.20)), (0.02, 0.22, 0.10), rot_x(sz * 0.3))
        box(fr, CA, (sx * (hx - 0.06), (yb + yt) / 2, 0), (0.08, yt - yb, 0.14))                                # mid posts
    box(fr, BL, (0, yt - 0.08, -hz + 0.08), (2 * hx - 0.3, 0.16, 0.16))                                         # back portal
    if lod < 2:
        L = math.hypot(2 * hx - 0.4, yt - yb - 0.4)
        a = math.atan2(yt - yb - 0.4, 2 * hx - 0.4)
        for s in (-1, 1):
            box(fr, CA, (0, (yb + yt) / 2, -hz + 0.05), (L, 0.08, 0.04), rot_z(s * a))
    if close():
        for sx in (-1, 1):
            for sz in (-1, 1):
                bolt_grid(fr, 1, 1, yb, sx * (hx - 0.20), sx * (hx - 0.04), sz * (hz - 0.20), sz * (hz - 0.04), 2, 2, 0.03)
        box(fr, BR, (-0.8, yt - 0.08, -hz + 0.165), (0.3, 0.08, 0.01))                                        # maker plate

    la = P["c20_latch"] = Part("c20_latch")
    for sx in (-1, 1):
        for sz in (-1, 1):
            x, z = sx * rx, sz * (hz - 0.20)
            box(la, BR, (x, -hy + 0.235, z), (0.18, 0.07, 0.22), chamfer=0.012 if close() else 0.0)               # twist lock
            box(la, TC, (x - sx * 0.15, -hy + 0.235, z), (0.16, 0.04, 0.05))                                     # lever (inboard)
            if lod < 2:
                lathe(la, "eq_steel", [(0.06, 0.0), (0.035, 0.07), (0.0, 0.08)], small, R_Y, (x, -hy + 0.27, z))
            xt = sx * (hx - 0.05)
            box(la, BR, (xt, hy - 0.13, sz * (hz - 0.25)), (0.12, 0.10, 0.20))                                   # top bridge clamp
            if lod < 2:
                box(la, TC, (xt - sx * 0.10, hy - 0.10, sz * (hz - 0.25)), (0.12, 0.04, 0.05))
    return P


def c20_hits():
    d = C20
    hx, hy, hz, rx = d["hx"], d["hy"], d["hz"], d["rx"]
    H = []
    for s in (-1, 1):
        H.append(hbox("c20_rail", (s * rx, -hy + 0.10, 0), (0.24, 0.20, 2 * hz)))
        H.append(hbox("c20_rail", (s * (hx - 0.05), hy - 0.05, 0), (0.08, 0.10, 2 * hz - 0.36)))
        H.append(hbox("c20_frame", (s * (hx - 0.06), 0.1, 0), (0.08, 2 * hy - 0.2, 0.14)))
        for sz in (-1, 1):
            H.append(hbox("c20_frame", (s * (hx - 0.08), 0.1, sz * (hz - 0.08)), (0.16, 2 * hy - 0.2, 0.16)))
            H.append(hbox("c20_latch", (s * rx, -hy + 0.24, sz * (hz - 0.20)), (0.30, 0.10, 0.22)))
            H.append(hbox("c20_latch", (s * (hx - 0.07), hy - 0.13, sz * (hz - 0.25)), (0.16, 0.12, 0.20)))
    H.append(hbox("c20_frame", (0, hy - 0.08, -hz + 0.08), (2 * hx - 0.3, 0.16, 0.16)))
    H.append(hbox("c20_rail", (0, -hy + 0.32, -hz + 0.16), (2 * rx - 0.3, 0.20, 0.32)))
    return H


# ================================================================= SU-1 docking collar (std.dock.collar), outward +Y
DOCK = dict(r=0.855, plate=0.07, tube_in=0.52, tube_out=0.62, top=0.60, ring_out=0.74, ring_top=0.70)


def collar_parts(lod):
    d = DOCK
    seg = seg_for(lod)
    small = small_for(lod)
    P = {}
    f = P["collar_frame"] = Part("collar_frame")
    ylathe(f, BL, [(0, 0), (d["r"], 0), (d["r"], d["plate"]), (0, d["plate"])], 6, smooth=False)         # hex adapter plate
    ylathe(f, WH, crect(d["tube_in"], d["tube_out"], d["plate"], d["top"]), seg)                          # tunnel
    ylathe(f, CA, crect(0.50, d["ring_out"], d["top"], d["ring_top"], 0.015 if lod == 0 else 0.0), seg)  # capture ring
    ylathe(f, SD, [(0, 0.38), (d["tube_in"], 0.38), (d["tube_in"], 0.42), (0, 0.42)], seg)               # hatch
    for k in range(6):                                                                                     # corner castings
        th = k * math.pi / 3
        R = rot_y(-th)
        c = polar(0.70, th, d["plate"] + 0.07)
        casting(f, c, 0.14, (1, 1, 0), lod, R)
    if lod < 2:
        for k in range(6):                                                                                 # patchwork tunnel panels
            th = k * math.pi / 3 + math.pi / 6
            ylathe(f, pmat(k + 2), crect(d["tube_out"], d["tube_out"] + 0.02, 0.24, 0.52), max(3, seg // 12),
                   th - 0.42, th + 0.42, cap=True)
    if lod == 0:
        ylathe(f, BR, crect(0.20, 0.24, 0.42, 0.46), small * 2)                                           # handwheel
        for k in range(4):
            box(f, BR, polar(0.11, k * math.pi / 2, 0.44), (0.20, 0.025, 0.025), rot_y(-k * math.pi / 2))
        box(f, BL, (0.0, 0.425, 0.33), (0.28, 0.01, 0.08))                                                # hatch label
    if close():
        for k in range(12):
            a = (k + 0.5) * math.pi / 6
            hex_bolt(f, "eq_steel", polar(0.79 if k % 2 else 0.68, a, d["plate"]), (0, 1, 0), 0.04, 0.02)
        for k in range(3):
            brass_fitting(f, polar(d["tube_out"], k * 2 * math.pi / 3 + 0.5, 0.15), (math.cos(k * 2 * math.pi / 3 + 0.5), 0, math.sin(k * 2 * math.pi / 3 + 0.5)), 0.025, lod)

    se = P["collar_seal"] = Part("collar_seal")
    rt = d["ring_top"]
    ylathe(se, RB, crect(0.54, 0.59, rt, rt + 0.035, 0.01 if lod == 0 else 0.0), seg)
    if lod < 2:
        ylathe(se, RB, crect(0.64, 0.69, rt, rt + 0.035, 0.01 if lod == 0 else 0.0), seg)

    la = P["collar_latch"] = Part("collar_latch")
    for k in range(6):
        th = k * math.pi / 3
        R = rot_y(-th)
        box(la, TC, polar(0.775, th, 0.55), (0.10, 0.60, 0.16), R, chamfer=0.015 if lod == 0 else 0.0)    # latch arm
        box(la, BR, polar(0.72, th, 0.82), (0.24, 0.07, 0.14), R)                                          # hook over the ring
        if lod < 2:
            cyl(la, "eq_steel", polar(0.69, th, d["plate"] + 0.14), polar(0.69, th, d["top"] - 0.02), 0.035, small)   # actuator
        if lod == 0:
            hex_bolt(la, "eq_steel", polar(0.825, th, 0.40), (math.cos(th), 0, math.sin(th)), 0.06, 0.025)
    for k in range(3):                                                                                     # guide pins
        th = k * 2 * math.pi / 3 + math.pi / 6
        cyl(la, BR, polar(0.705, th, rt), polar(0.705, th, rt + 0.12), 0.03, small)
    return P


def collar_hits():
    d = DOCK
    H = [hcyl("collar_frame", (0, 0, 0), (0, d["plate"], 0), 0.80),
         hcyl("collar_frame", (0, d["plate"], 0), (0, d["ring_top"], 0), d["ring_out"]),
         hcyl("collar_seal", (0, d["ring_top"], 0), (0, d["ring_top"] + 0.04, 0), 0.69)]
    for k in range(6):
        th = k * math.pi / 3
        H.append(hbox("collar_latch", polar(0.76, th, 0.55), (0.16, 0.62, 0.16), (0, -math.degrees(th), 0)))
    return H


# ================================================================= OD-4 drone bay (std.bay.drone4)
DRONE = dict(hx=1.687, hy=0.8435, hz=2.1085, cs=0.18, b=0.14, wall=0.06)


def drone_parts(lod):
    d = DRONE
    hx, hy, hz, cs, b, w = d["hx"], d["hy"], d["hz"], d["cs"], d["b"], d["wall"]
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    fr = P["drone4_frame"] = Part("drone4_frame")
    aframe(fr, (-hx, -hy, -hz), (hx, hy, hz), b, cs, lod)
    box(fr, SD, (0, -hy + 0.09, 0), (2 * hx - 0.2, 0.06, 2 * hz - 0.2))                                  # floor
    for s in (-1, 1):
        box(fr, WH, (s * (hx - 0.05), 0.0, 0), (w, 2 * hy - 0.24, 2 * hz - 0.24))                       # side walls
        box(fr, SD, (0, 0.0, s * (hz - 0.05)), (2 * hx - 0.24, 2 * hy - 0.24, w))                       # end walls
        box(fr, BL, (s * (hx - 0.16), hy - 0.14, 0), (0.08, 0.06, 2 * hz - 0.36))                       # door rails
    if lod < 2:
        for k in range(3):                                                                               # side patch panels
            zc = -hz + 0.2 + (k + 0.5) * (2 * hz - 0.4) / 3
            for s in (-1, 1):
                pan(fr, pmat(k * 2 + (s > 0)), 0, s, s * (hx - 0.02), -0.02, zc, 2 * hy - 0.5, (2 * hz - 0.4) / 3 - 0.12, 0.018)
        for s in (-1, 1):                                                                                # end-wall panels
            for k in range(2):
                pan(fr, pmat(k + 7 + (s > 0) * 3), 2, s, s * (hz - 0.02), (k - 0.5) * (hx - 0.1), -0.02, hx - 0.4, 2 * hy - 0.5, 0.018)
        if close():
            for k in range(3):
                zc = -hz + 0.2 + (k + 0.5) * (2 * hz - 0.4) / 3
                bolt_grid(fr, 0, 1, hx - 0.002, -0.55, 0.5, zc - 0.5, zc + 0.5, 2 + k, 2, 0.04)

    dr = P["drone4_doors"] = Part("drone4_doors")
    dw, dl = 2 * hx - 0.30, hz - 0.18
    box(dr, SD, (0, hy - 0.115, -hz / 2 + 0.05), (dw, 0.04, dl))                                         # leaf A (closed)
    box(dr, TC, (0, hy - 0.07, -hz / 2 + 0.17), (dw - 0.04, 0.04, dl))                                    # leaf B slid over A
    if lod < 2:
        for k in range(4):
            box(dr, TC, (0, hy - 0.04, -hz + 0.45 + k * 0.48), (dw - 0.3, 0.025, 0.08))
        for s in (-1, 1):
            box(dr, BR, (s * 0.6, hy - 0.035, -0.12), (0.30, 0.03, 0.05))                                 # pull handles
    if lod == 0:
        for s in (-1, 1):
            for z in (-hz + 0.3, -0.25):
                cyl(dr, "eq_steel", (s * (hx - 0.16), hy - 0.09, z), (s * (hx - 0.16), hy - 0.09, z + 0.12), 0.03, 8)
        box(dr, BL, (-0.8, hy - 0.048, -1.4), (0.5, 0.01, 0.16))

    cr = P["drone4_cradle"] = Part("drone4_cradle")
    yf = -hy + 0.12
    for sx in (-1, 1):
        for sz in (-1, 1):
            cx, cz = sx * 0.80, sz * 1.0
            box(cr, CA, (cx, yf + 0.025, cz), (1.2, 0.05, 1.4))
            if lod < 2:
                for s in (-1, 1):
                    box(cr, BL, (cx + s * 0.45, yf + 0.10, cz), (0.08, 0.10, 1.3))
                for s in (-1, 1):
                    for t in (-1, 1):
                        box(cr, TC, (cx + s * 0.45, yf + 0.25, cz + t * 0.55), (0.06, 0.30, 0.06))
                        box(cr, BR, (cx + s * 0.40, yf + 0.40, cz + t * 0.55), (0.14, 0.04, 0.08))
                lathe(cr, "eq_steel", [(0, 0), (0.10, 0), (0.10, 0.10), (0, 0.10)], small, R_Y, (cx, yf + 0.05, cz))
    dn = P["drone4_drones"] = Part("drone4_drones")                     # stowed service drones (readable blocks)
    if lod < 2:
        for sx in (-1, 1):
            for sz in (-1, 1):
                cx, cz = sx * 0.80, sz * 1.0
                ylathe(dn, "eq_metal_panel", [(0, yf + 0.15), (0.40, yf + 0.15), (0.44, yf + 0.25), (0.40, yf + 0.36), (0, yf + 0.36)],
                       8, t=(cx, 0, cz), smooth=False)
                for k in range(4):
                    a = math.pi / 4 + k * math.pi / 2
                    q = (cx + 0.46 * math.cos(a), yf + 0.20, cz + 0.46 * math.sin(a))
                    cyl(dn, "eq_metal_band", q, (q[0], yf + 0.34, q[2]), 0.09, small)
                if lod == 0:
                    box(dn, "eq_status_amber", (cx, yf + 0.37, cz + 0.2), (0.12, 0.02, 0.06))

    ch = P["drone4_charger"] = Part("drone4_charger")
    zc = hz - 0.08 - 0.15
    box(ch, BL, (0, -hy + 0.62, zc), (2 * hx - 0.5, 0.86, 0.26), chamfer=0.02 if lod == 0 else 0.0)
    if lod < 2:
        for i, x in enumerate((-1.1, -0.45, 0.45, 1.1)):
            cyl(ch, BR, (x, -hy + 0.45, zc - 0.13), (x, -hy + 0.45, zc - 0.20), 0.07, small)
            box(ch, "eq_status_amber" if i % 2 else "eq_insulator", (x, -hy + 0.80, zc - 0.135), (0.10, 0.06, 0.01))
    if lod == 0:
        box(ch, WH, (0, -hy + 0.82, zc - 0.135), (0.7, 0.30, 0.01))                                       # panel
    if close():
        for x, cz in ((-1.1, 1.0), (-0.45, -1.0), (0.45, -1.0), (1.1, 1.0)):
            cx = -0.8 if x < 0 else 0.8
            harness(ch, bezier((x, -hy + 0.45, zc - 0.20), (x, -hy + 0.25, zc - 0.5), (cx, yf + 0.06, cz + 0.8), (cx, yf + 0.08, cz + 0.12), 12),
                    0.02, 1, (TC if x < 0 else "eq_insulator",), tie_every=6)
    return P


def drone_hits():
    d = DRONE
    hx, hy, hz, b = d["hx"], d["hy"], d["hz"], d["b"]
    H = [hbox("drone4_frame", (0, -hy + 0.06, 0), (2 * hx, 0.12, 2 * hz))]
    for s in (-1, 1):
        H.append(hbox("drone4_frame", (s * (hx - 0.06), 0, 0), (0.12, 2 * hy, 2 * hz)))
        H.append(hbox("drone4_frame", (0, 0, s * (hz - 0.06)), (2 * hx - 0.24, 2 * hy, 0.12)))
    H.append(hbox("drone4_doors", (0, hy - 0.09, -hz / 2 + 0.11), (2 * hx - 0.30, 0.10, hz - 0.06)))
    for sx in (-1, 1):
        for sz in (-1, 1):
            H.append(hbox("drone4_cradle", (sx * 0.80, -hy + 0.30, sz * 1.0), (1.2, 0.36, 1.4)))
    H.append(hbox("drone4_charger", (0, -hy + 0.62, hz - 0.23), (2 * hx - 0.5, 0.86, 0.30)))
    return H


# ================================================================= hexagonal survival craft (pod SK-4, lifeboat SSh-20)
def hex_panels(p, lod, poly, z0, z1, nb, k0=0):
    """patchwork plates on the faces of a hexagonal hull between z0 and z1 in nb bays"""
    n = len(poly)
    for i in range(n):
        (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % n]
        L = math.hypot(x1 - x0, y1 - y0)
        a = math.atan2(-(x1 - x0), (y1 - y0))            # outward normal of the edge (counter-clockwise polygon)
        nx, ny = math.cos(a), math.sin(a)
        for k in range(nb):
            za, zb = z0 + (z1 - z0) * k / nb + 0.05, z0 + (z1 - z0) * (k + 1) / nb - 0.05
            t = 0.015 + 0.01 * ((i + k) % 2)
            box(p, pmat(k0 + i * 3 + k), ((x0 + x1) / 2 + nx * t / 2, (y0 + y1) / 2 + ny * t / 2, (za + zb) / 2),
                (t, L * 0.80, zb - za), rot_z(a))
            if close() and (i + k) % 3 != 1:
                for s in (-1, 1):
                    for zz in (za + 0.05, zb - 0.05):
                        q = ((x0 + x1) / 2 + nx * t - ny * s * L * 0.36, (y0 + y1) / 2 + ny * t + nx * s * L * 0.36, zz)
                        hex_bolt(p, "eq_steel", q, (nx, ny, 0), 0.035, 0.015)


def face_box(p, mat, poly, i, zc, size, out=0.0):
    """a box sitting on face i of a (u, v) = (x, y) polygon; size = (thickness, along-edge, along-z)"""
    (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % len(poly)]
    a = math.atan2(-(x1 - x0), (y1 - y0))
    nx, ny = math.cos(a), math.sin(a)
    box(p, mat, ((x0 + x1) / 2 + nx * (size[0] / 2 + out), (y0 + y1) / 2 + ny * (size[0] / 2 + out), zc), size, rot_z(a))


POD = dict(ax=0.80, bx=0.42, ay=0.76, z0=-0.98, z1=0.55, zn=1.02, sn=0.64, ze=1.162, zr=-1.10, rs=1.0375)


def pod_parts(lod):
    d = POD
    seg, small = seg_for(lod), small_for(lod)
    hx = lambda s: hexp(d["ax"], d["bx"], d["ay"], s)
    P = {}
    h = P["pod4_hull"] = Part("pod4_hull")
    loft(h, WH, [(hx(1.0), d["z0"]), (hx(1.0), d["z1"]), (hx(d["sn"]), d["zn"])], caps=(True, False))
    loft(h, "eq_pad_ceramic", [(hx(d["sn"]), d["zn"]), (hx(d["sn"] * 0.94), d["zn"] + 0.08), (hx(d["sn"] * 0.78), d["ze"])])
    for s in (-1, 1):                                                                               # side rails
        box(h, BL, (s * (d["ax"] - 0.01), 0, -0.2), (0.06, 0.09, 1.40))
    if lod < 2:
        hex_panels(h, lod, hx(1.0), -0.88, 0.52, 2)
        for i in (0, 2):                                                                            # viewports
            face_box(h, GL, hx(1.0), i, 0.05, (0.03, 0.30, 0.28), 0.0)
            if lod == 0:
                face_box(h, WH, hx(1.0), i, 0.05, (0.025, 0.40, 0.38), 0.0)
        if lod == 0:
            face_box(h, BL, hx(1.0), 1, -0.6, (0.01, 0.40, 0.12), 0.026)                             # label plate
    if close():
        for i in range(6):                                                                          # shield clamp blocks
            (x0, y0), (x1, y1) = hx(d["sn"])[i], hx(d["sn"])[(i + 1) % 6]
            box(h, "eq_steel", ((x0 + x1) / 2 * 0.98, (y0 + y1) / 2 * 0.98, d["zn"] + 0.01), (0.08, 0.08, 0.06), rot_z(math.atan2(y0 + y1, x0 + x1)))

    py = P["pod4_pyro"] = Part("pod4_pyro")                                                           # release ring
    zr, rs = d["zr"], d["rs"]
    loft(py, CA, [(hx(rs), zr), (hx(rs), -0.90)], caps=(False, False))
    annulus(py, CA, hx(rs), hx(0.86), zr, -1)
    annulus(py, CA, hx(rs), hx(1.0), -0.90, 1)
    loft(py, CA, [(hx(0.86), zr), (hx(0.86), d["z0"])], caps=(False, False), inward=True)
    for k, (x, y) in enumerate(hx(0.90)):
        casting(py, (x, y, -1.10), 0.124, (0, 0, -1), lod, rot_z(math.atan2(y, x)))
    if lod < 2:
        poly = hx(0.93)
        for i in range(6):
            (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % 6]
            q = ((x0 + x1) / 2, (y0 + y1) / 2)
            cyl(py, TC, (q[0], q[1], zr), (q[0], q[1], zr - 0.05), 0.04, small)                      # pyro cartridges
            if lod == 0:
                hex_bolt(py, "eq_steel", (q[0], q[1], zr - 0.05), (0, 0, -1), 0.05, 0.02)

    se = P["pod4_seal"] = Part("pod4_seal")
    z0 = d["z0"]
    lathe(se, SD, [(0, z0 - 0.05), (0.44, z0 - 0.05), (0.46, z0 - 0.03), (0.46, z0)], seg)
    lathe(se, RB, crect(0.46, 0.53, z0 - 0.025, z0), seg)
    if lod < 2:
        lathe(se, BR, crect(0.14, 0.18, z0 - 0.09, z0 - 0.05), small * 2)                          # handwheel
        for k in range(3):
            box(se, BR, (0, 0, z0 - 0.07), (0.34, 0.025, 0.025), rot_z(k * math.pi / 3))
    if close():
        for k in range(8):
            hex_bolt(se, "eq_steel", polz(0.40, k * math.pi / 4 + 0.2, z0 - 0.05), (0, 0, -1), 0.035, 0.015)

    bt = P["pod4_battery"] = Part("pod4_battery")
    box(bt, BL, (0, -d["ay"] - 0.035, -0.15), (0.62, 0.07, 1.10), chamfer=0.015 if lod == 0 else 0.0)
    if lod == 0:
        box(bt, SD, (0, -d["ay"] - 0.072, -0.35), (0.50, 0.006, 0.50))
        box(bt, "eq_status_amber", (0.2, -d["ay"] - 0.072, 0.25), (0.08, 0.006, 0.04))
    if close():
        bolt_grid(bt, 1, -1, -d["ay"] - 0.07, -0.27, 0.27, -0.65, 0.35, 2, 4, 0.03)
        brass_fitting(bt, (0.0, -d["ay"] - 0.035, 0.40), (0, 0, 1), 0.02, lod)

    bc = P["pod4_beacon"] = Part("pod4_beacon")
    ay = d["ay"]
    box(bc, CA, (0, ay + 0.01, 0.25), (0.30, 0.02, 0.30))
    ylathe(bc, "eq_status_amber", [(0, ay + 0.02), (0.11, ay + 0.02), (0.11, ay + 0.04), (0.07, ay + 0.065), (0, ay + 0.07)],
           small, t=(0, 0, 0.25), soft=40)
    if lod < 2:
        cyl(bc, "eq_steel", (0.10, ay + 0.035, 0.40), (0.10, ay + 0.035, 0.90), 0.012, 6)            # stowed whip antenna
    return P


def pod_hits():
    d = POD
    H = [hbox("pod4_hull", (0, 0, (d["z0"] + d["z1"]) / 2), (2 * d["ax"] + 0.06, 2 * d["ay"], d["z1"] - d["z0"])),
         hbox("pod4_hull", (0, 0, (d["z1"] + d["ze"]) / 2), (2 * d["ax"] * 0.8, 2 * d["ay"] * 0.8, d["ze"] - d["z1"])),
         hbox("pod4_pyro", (0, 0, -1.04), (2 * d["ax"] * d["rs"], 2 * d["ay"] * d["rs"], 0.25)),
         hcyl("pod4_seal", (0, 0, d["z0"] - 0.09), (0, 0, d["z0"]), 0.53),
         hbox("pod4_battery", (0, -d["ay"] - 0.035, -0.15), (0.62, 0.07, 1.10)),
         hbox("pod4_beacon", (0, d["ay"] + 0.035, 0.25), (0.30, 0.07, 0.30))]
    return H


BOAT = dict(ax=1.40, bx=0.78, ay=0.92, zs=-3.10, z1=2.30, zn=3.12, sn=0.52, ze=3.2575, hy=1.0365)


def boat_parts(lod):
    d = BOAT
    seg, small = seg_for(lod), small_for(lod)
    hx = lambda s: hexp(d["ax"], d["bx"], d["ay"], s)
    ay = d["ay"]
    P = {}
    h = P["boat20_hull"] = Part("boat20_hull")
    loft(h, WH, [(hx(0.94), d["zs"]), (hx(1.0), d["zs"] + 0.12), (hx(1.0), d["z1"]), (hx(d["sn"]), d["zn"])], caps=(True, False))
    loft(h, "eq_pad_ceramic", [(hx(d["sn"]), d["zn"]), (hx(d["sn"] * 0.95), d["zn"] + 0.07), (hx(d["sn"] * 0.80), d["ze"])])
    for s in (-1, 1):                                                                               # side rails
        box(h, BL, (s * (d["ax"] + 0.02), 0, -0.3), (0.08, 0.12, 5.0))
        for z in (-2.6, 2.0):
            casting(h, (s * (d["ax"] + 0.0), 0, z), 0.14, (s, 0, 0), lod)
    # stern docking hatch (the boarding end at -Z)
    zs = d["zs"]
    lathe(h, CA, [(0.50, zs), (0.50, zs - 0.15), (0.62, zs - 0.15), (0.62, zs)], seg)
    lathe(h, SD, [(0, zs - 0.11), (0.50, zs - 0.11)], seg)
    if lod < 2:
        lathe(h, RB, crect(0.50, 0.56, zs - 0.18, zs - 0.15), seg)
        hex_panels(h, lod, hx(1.0), d["zs"] + 0.2, d["z1"] - 0.05, 4, 3)
        for s, i in ((1, 0), (-1, 2)):                                                              # window strips
            for k in range(6):
                face_box(h, GL, hx(1.0), i, -1.5 + k * 0.62, (0.04, 0.42, 0.40), 0.0)
        face_box(h, SD, hx(1.0), 0, -2.35, (0.04, 0.70, 0.75), 0.0)                                 # side hatch
        if lod == 0:
            face_box(h, BR, hx(1.0), 0, -2.6, (0.07, 0.06, 0.16), 0.0)
            face_box(h, BL, hx(1.0), 2, -2.35, (0.035, 0.30, 0.60), 0.0)
            for k in range(6):
                face_box(h, WH, hx(1.0), 0, -1.5 + k * 0.62, (0.03, 0.52, 0.50), 0.0)
                face_box(h, WH, hx(1.0), 2, -1.5 + k * 0.62, (0.03, 0.52, 0.50), 0.0)
    if close():
        for k in range(8):
            hex_bolt(h, "eq_steel", polz(0.56, k * math.pi / 4, zs - 0.15), (0, 0, -1), 0.04, 0.015)
        for k in range(3):
            brass_fitting(h, (-0.9 + 0.25 * k, -0.5, zs + 0.03), (0, 0, -1), 0.025, lod)

    rl = P["boat20_release"] = Part("boat20_release")
    for sx in (-1, 1):
        for z in (-1.9, 1.9):
            box(rl, CA, (sx * 0.45, ay + 0.045, z), (0.26, 0.09, 0.34), chamfer=0.012 if lod == 0 else 0.0)
            box(rl, BR, (sx * 0.45, ay + 0.10, z), (0.12, 0.03, 0.22))
            if lod < 2:
                cyl(rl, TC, (sx * 0.58, ay + 0.045, z - 0.08), (sx * 0.66, ay + 0.045, z - 0.08), 0.035, small)
            if lod == 0:
                box(rl, HO, (sx * 0.45, ay + 0.092, z + 0.12), (0.10, 0.006, 0.05))
                hex_bolt(rl, "eq_steel", (sx * 0.58, ay + 0.045, z + 0.10), (sx, 0, 0), 0.05, 0.02)

    bt = P["boat20_battery"] = Part("boat20_battery")
    for z in (-1.4, 0.6):
        box(bt, BL, (0, -ay - 0.05, z), (1.0, 0.10, 1.4), chamfer=0.02 if lod == 0 else 0.0)
        if lod == 0:
            box(bt, SD, (0, -ay - 0.103, z - 0.25), (0.8, 0.006, 0.6))
            box(bt, "eq_status_amber", (0.35, -ay - 0.103, z + 0.5), (0.10, 0.006, 0.05))
        if close():
            bolt_grid(bt, 1, -1, -ay - 0.10, -0.45, 0.45, z - 0.65, z + 0.65, 3, 4, 0.03)
            brass_fitting(bt, (0.3, -ay - 0.05, z + 0.70), (0, 0, 1), 0.025, lod)

    bc = P["boat20_beacon"] = Part("boat20_beacon")
    box(bc, CA, (0, ay + 0.015, 2.30), (0.34, 0.03, 0.34))
    ylathe(bc, "eq_status_amber", [(0, ay + 0.03), (0.12, ay + 0.03), (0.12, ay + 0.06), (0.08, ay + 0.10), (0, ay + 0.11)],
           small, t=(0, 0, 2.30), soft=40)
    if lod < 2:
        cyl(bc, "eq_steel", (0.15, ay + 0.04, 1.2), (0.15, ay + 0.04, 2.15), 0.015, 6)
        box(bc, CA, (0.15, ay + 0.03, 2.15), (0.06, 0.04, 0.06))
    return P


def boat_hits():
    d = BOAT
    ay = d["ay"]
    H = [hbox("boat20_hull", (0, 0, (d["zs"] + d["z1"]) / 2), (2 * d["ax"] + 0.12, 2 * ay, d["z1"] - d["zs"])),
         hbox("boat20_hull", (0, 0, (d["z1"] + d["ze"]) / 2), (2 * d["ax"] * 0.75, 2 * ay * 0.75, d["ze"] - d["z1"])),
         hcyl("boat20_hull", (0, 0, d["zs"] - 0.18), (0, 0, d["zs"]), 0.62)]
    for sx in (-1, 1):
        for z in (-1.9, 1.9):
            H.append(hbox("boat20_release", (sx * 0.47, ay + 0.06, z), (0.40, 0.12, 0.34)))
    for z in (-1.4, 0.6):
        H.append(hbox("boat20_battery", (0, -ay - 0.05, z), (1.0, 0.10, 1.4)))
    H.append(hcyl("boat20_beacon", (0, ay, 2.30), (0, ay + 0.11, 2.30), 0.17))
    return H


# ================================================================= PS-100 passenger section (agora.hab.pax100)
PAX = dict(hx=3.6595, hy=2.614, hz=5.2275, roof=0.50, cs=0.32, b=0.26, sk=0.16)


def pax_parts(lod):
    d = PAX
    hx, hy, hz, cs, b, sk = d["hx"], d["hy"], d["hz"], d["cs"], d["b"], d["sk"]
    yt = hy - d["roof"]                                                    # frame top (roof units above)
    small = small_for(lod)
    P = {}
    stations = [-hz + k * 2 * hz / 6 for k in range(1, 6)]
    edges = [-hz + cs] + stations + [hz - cs]
    decks = ((-hy + 0.75, -hy + 1.95), (-hy + 2.75, -hy + 3.95))           # window bands (y) of the two decks

    fr = P["pax100_frame"] = Part("pax100_frame")
    aframe(fr, (-hx, -hy, -hz), (hx, yt, hz), b, cs, lod)
    ring_frames(fr, BL if lod else CA, (-hx + 0.04, -hy + 0.04, 0), (hx - 0.04, yt - 0.04, 0), b - 0.04,
                stations if lod < 2 else stations[1::2], 0.18, sk - 0.03)
    if lod < 2:
        for s in (-1, 1):                                                  # mid-height side rail (deck line)
            box(fr, TC, (s * (hx - sk / 2 - 0.02), -hy + 2.35, 0), (sk - 0.02, 0.14, 2 * hz - 2 * cs))

    sh = P["pax100_shell"] = Part("pax100_shell")
    box(sh, WH, (0, (yt - hy) / 2, 0), (2 * (hx - sk), yt + hy - 2 * sk, 2 * hz - 0.24))
    if lod == 2:
        for s in (-1, 1):
            for y0, y1 in decks:
                box(sh, GL, (s * (hx - sk + 0.01), (y0 + y1) / 2 + 0.1, 0), (0.03, 0.55, 2 * hz - 1.0))
    else:
        for k in range(6):
            z0, z1 = edges[k] + 0.12, edges[k + 1] - 0.12
            zc, zl = (z0 + z1) / 2, z1 - z0
            for s in (-1, 1):
                plane = s * (hx - sk)
                bands = [(-hy + b + 0.05, decks[0][0]), (decks[0][1], -hy + 2.28), (-hy + 2.42, decks[1][0]), (decks[1][1], yt - b - 0.05)]
                for j, (y0, y1) in enumerate(bands):
                    pan(sh, pmat(k * 5 + j + (s > 0)), 0, s, plane, (y0 + y1) / 2, zc, y1 - y0 - 0.04, zl, 0.02 + 0.012 * ((k + j) % 2))
                for j, (y0, y1) in enumerate(decks):                         # window bands: frame strip + 3 windows
                    pan(sh, SD if (k + j) % 2 else WH, 0, s, plane, (y0 + y1) / 2, zc, y1 - y0 - 0.04, zl, 0.015)
                    for i in range(3):
                        zw = z0 + (i + 0.5) * zl / 3
                        pan(sh, GL, 0, s, plane + s * 0.015, (y0 + y1) / 2 + 0.1, zw, 0.62, 0.40, 0.02, chamfer=0.03 if lod == 0 else 0.0)
                        if lod == 0:
                            pan(sh, WH, 0, s, plane + s * 0.015, (y0 + y1) / 2 + 0.1, zw, 0.74, 0.52, 0.012)
                if close():
                    bolt_grid(sh, 0, s, plane + s * 0.03, -hy + b + 0.12, decks[0][0] - 0.08, z0 + 0.06, z1 - 0.06, 2, 6 if k % 2 else 3, 0.04)
            for s in (-1, 1):                                                 # roof / floor patchwork
                pan(sh, pmat(k * 3 + 7 + s), 1, s, yt - sk if s > 0 else -hy + sk, 0.0, zc, 2 * (hx - b) - 0.1, zl, 0.025)
        # end bulkheads
        for s in (-1, 1):
            zf = s * (hz - 0.12)
            for i, (x0, x1) in enumerate(((-hx + b, -1.2), (-1.2, 1.2), (1.2, hx - b))):
                pan(sh, pmat(i + 4 + s), 2, s, zf, (x0 + x1) / 2, (yt - hy) / 2, x1 - x0 - 0.06, yt + hy - 2 * b - 0.06, 0.03)
            if close():
                for i in range(5):
                    brass_fitting(sh, (2.0 + 0.16 * i, -1.6, zf + s * 0.03), (0, 0, s), 0.025 + 0.005 * (i % 2), lod)
                box(sh, "eq_steel", (2.32, -1.6, zf + s * 0.035), (0.95, 0.24, 0.01))

    se = P["pax100_seal"] = Part("pax100_seal")                            # four end hatches (two per deck) with seals
    for s in (-1, 1):
        zf = s * (hz - 0.12) + s * 0.03
        for y0, _ in ((-hy + b + 0.10, 0), (-hy + 2.50, 0)):
            yc = y0 + 1.0
            pan(se, RB, 2, s, zf, 0.0, yc, 1.10, 2.06, 0.03)
            pan(se, BL if y0 < 0 else TC, 2, s, zf + s * 0.03, 0.0, yc, 0.96, 1.92, 0.04, chamfer=0.03 if lod == 0 else 0.0)
            if lod < 2:
                pan(se, GL, 2, s, zf + s * 0.07, 0.0, yc + 0.5, 0.36, 0.42, 0.01)
                pan(se, BR, 2, s, zf + s * 0.07, 0.36, yc - 0.1, 0.06, 0.30, 0.05)
            if close():
                bolt_grid(se, 2, s, zf + s * 0.07, -0.42, 0.42, yc - 0.90, yc + 0.90, 2, 5, 0.035)

    cl = P["pax100_climate"] = Part("pax100_climate")                      # two roof climate units (different makers)
    for k, z in enumerate((-2.6, 2.6)):
        mat = SD if k == 0 else WH
        box(cl, mat, (0, yt + 0.21, z), (2.8, 0.42, 2.2), chamfer=0.04 if lod == 0 else 0.0)
        for x in (-0.7, 0.7):
            ylathe(cl, CA, crect(0.38, 0.48, yt + 0.42, yt + 0.48), small * 2, t=(x, 0, z))
            ylathe(cl, "eq_agora_hole", [(0, yt + 0.43), (0.38, yt + 0.43)][::-1], small * 2, t=(x, 0, z))
            if lod == 0:
                for i in range(3):
                    box(cl, CA, (x, yt + 0.445, z), (0.76, 0.02, 0.03), rot_y(i * math.pi / 3))
        if lod < 2:
            box(cl, BL if k == 0 else TC, (0, yt + 0.21, z + (1.12 if k == 0 else -1.12)), (2.4, 0.26, 0.04))  # louvre band
            for s in (-1, 1):
                box(cl, CA, (s * 1.2, yt + 0.03, z), (0.25, 0.06, 2.3))                                           # feet rails
        if lod == 0:
            for i in range(8):
                box(cl, CA, (-1.05 + i * 0.3, yt + 0.21, z + (1.15 if k == 0 else -1.15)), (0.04, 0.22, 0.03))
        if close():
            pipe_run(cl, "eq_steel", [(1.4, yt + 0.15, z), (1.7, yt + 0.15, z), (1.7, yt - 0.02, z)], 0.05)
            brass_fitting(cl, (-1.4, yt + 0.3, z + 0.5), (-1, 0, 0), 0.025, lod)

    pw = P["pax100_power"] = Part("pax100_power")                          # power distribution cabinet + conduits, -X side
    xc = -(hx - sk) - 0.12
    box(pw, BL, (xc, -hy + 1.15, -2.6), (0.24, 1.10, 1.30), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        for y in (-hy + 0.62, -hy + 0.70):
            cyl(pw, CA, (xc + 0.04, y, -2.0), (xc + 0.04, y, hz - 0.6), 0.04, small)
            cyl(pw, CA, (xc + 0.04, y, -3.2), (xc + 0.04, y, -hz + 0.6), 0.04, small)
        box(pw, SD, (xc - 0.125, -hy + 1.15, -2.6), (0.01, 0.90, 1.10))
    if lod == 0:
        for i in range(3):
            box(pw, "eq_status_amber", (xc - 0.132, -hy + 1.5, -3.0 + 0.12 * i), (0.006, 0.05, 0.06))
    if close():
        for i in range(4):
            brass_fitting(pw, (xc - 0.12, -hy + 0.80, -2.9 + 0.2 * i), (-1, 0, 0), 0.03, lod)
        harness(pw, bezier((xc, -hy + 0.62, -2.0), (xc - 0.1, -hy + 0.3, -1.5), (xc, -hy + 0.4, -1.0), (xc + 0.05, -hy + 0.62, -0.5), 10),
                0.025, 3, ("eq_insulator", TC), tie_every=4)
    return P


def pax_hits():
    d = PAX
    hx, hy, hz, b, sk = d["hx"], d["hy"], d["hz"], d["b"], d["sk"]
    yt = hy - d["roof"]
    H = [hbox("pax100_shell", (0, (yt - hy) / 2, 0), (2 * (hx - sk) + 0.06, yt + hy - 2 * sk + 0.06, 2 * hz - 0.24))]
    for sx in (-1, 1):
        for y in (-hy + b / 2, yt - b / 2):
            H.append(hbox("pax100_frame", (sx * (hx - b / 2), y, 0), (b, b, 2 * hz)))
    for s in (-1, 1):
        for y0 in (-hy + b + 0.10, -hy + 2.50):
            H.append(hbox("pax100_seal", (0, y0 + 1.0, s * (hz - 0.04)), (1.10, 2.06, 0.10)))
    for z in (-2.6, 2.6):
        H.append(hbox("pax100_climate", (0, yt + 0.24, z), (2.8, 0.48, 2.3)))
    H.append(hbox("pax100_power", (-(hx - sk) - 0.12, -hy + 1.15, -2.6), (0.24, 1.10, 1.30)))
    return H


# ================================================================= RCS-4 field thruster cluster (std.rcs.cluster), outward +Y
RCS = dict(h=0.63, base=0.06, cs=0.16, ped_r=0.24, y0=0.58, y1=1.10, hb=0.33, yc=0.84)
RCS_DIRS = ((1, 0, 0), (-1, 0, 0), (0, 0, 1), (0, 0, -1))


def rcs_parts(lod):
    d = RCS
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    m = P["rcs4_mount"] = Part("rcs4_mount")
    box(m, BL, (0, d["base"] / 2, 0), (1.10, d["base"], 1.10))
    for sx in (-1, 1):
        for sz in (-1, 1):
            casting(m, (sx * 0.55, d["cs"] / 2, sz * 0.55), d["cs"], (sx, -1, sz), lod)
    ylathe(m, WH, [(d["ped_r"], d["base"]), (d["ped_r"], d["y0"])], 6, smooth=False)                    # hex pedestal
    ylathe(m, CA, crect(d["ped_r"] - 0.01, d["ped_r"] + 0.04, d["base"], d["base"] + 0.06), 6, smooth=False)
    box(m, WH, (0, (d["y0"] + d["y1"]) / 2, 0), (2 * d["hb"], d["y1"] - d["y0"], 2 * d["hb"]), chamfer=0.04 if lod == 0 else 0.0)
    ylathe(m, SD, [(0.26, d["y1"]), (0.20, d["y1"] + 0.10), (0, d["y1"] + 0.12)], 6, smooth=False)         # top cap
    if lod < 2:
        for k, (dx, _, dz) in enumerate(RCS_DIRS):                                                         # patch plates
            if dx:
                pan(m, pmat(k + 1), 0, dx, dx * d["hb"], d["yc"], 0, 0.44, 0.56, 0.012)
            else:
                pan(m, pmat(k + 1), 2, dz, dz * d["hb"], 0, d["yc"], 0.56, 0.44, 0.012)
    if close():
        lathe(m, "eq_steel", crect(0.025, 0.04, -0.01, 0.01), 8, axis_to((1, 0, 0)), (0, d["y1"] + 0.13, 0))  # lifting eye
        for sx in (-1, 1):
            for sz in (-1, 1):
                hex_bolt(m, "eq_steel", (sx * 0.36, d["base"], sz * 0.36), (0, 1, 0), 0.04, 0.02)

    co = P["rcs4_coil"] = Part("rcs4_coil")
    em = P["rcs4_emitter"] = Part("rcs4_emitter")
    hb, ns = d["hb"], max(8, seg // 2)
    for k, dirn in enumerate(RCS_DIRS):
        R, t = axis_to(dirn), (0, d["yc"], 0)
        lathe(co, "eq_field_coil", crect(0.17, 0.29, hb, hb + 0.13, 0.015 if lod == 0 else 0.0), ns, R, t)       # field coil
        if lod < 2:
            lathe(co, TC if k % 2 else BL, crect(0.285, 0.30, hb + 0.03, hb + 0.10), ns, R, t)                 # retaining strap
        lathe(em, "eq_pad_ceramic", [(0.17, hb), (0.17, hb + 0.24), (0, hb + 0.21)], ns, R, t)                 # emitter dish
        lathe(em, BR, crect(0.17, 0.25, hb + 0.13, hb + 0.27, 0.01 if lod == 0 else 0.0), ns, R, t)            # brass rim
        if lod == 0:
            for i in range(6):
                a = 2 * math.pi * (i + 0.5) / 6
                p = apply(R, (0.21 * math.cos(a), 0.21 * math.sin(a), hb + 0.27))
                hex_bolt(em, "eq_steel", (p[0], p[1] + d["yc"], p[2]), dirn, 0.06 if not close() else 0.04, 0.02)

    dv = P["rcs4_driver"] = Part("rcs4_driver")
    box(dv, SD, (0, 0.32, 0.36), (0.36, 0.30, 0.22), chamfer=0.02 if lod == 0 else 0.0)
    if lod < 2:
        cyl(dv, CA, (0.10, 0.47, 0.30), (0.10, d["y0"] + 0.03, 0.30), 0.035, small)
        cyl(dv, CA, (-0.10, 0.47, 0.30), (-0.10, d["y0"] + 0.03, 0.30), 0.035, small)
    if lod == 0:
        for i in range(5):
            box(dv, CA, (-0.12 + i * 0.06, 0.32, 0.48), (0.02, 0.24, 0.03))
        box(dv, "eq_status_amber", (0.13, 0.42, 0.472), (0.05, 0.03, 0.006))
    if close():
        brass_fitting(dv, (-0.18, 0.25, 0.36), (-1, 0, 0), 0.02, lod)
        brass_fitting(dv, (0.18, 0.25, 0.36), (1, 0, 0), 0.02, lod)
    return P


def rcs_hits():
    d = RCS
    H = [hbox("rcs4_mount", (0, d["cs"] / 2, 0), (1.26, d["cs"], 1.26)),
         hcyl("rcs4_mount", (0, d["base"], 0), (0, d["y0"], 0), d["ped_r"]),
         hbox("rcs4_mount", (0, (d["y0"] + d["y1"] + 0.12) / 2, 0), (2 * d["hb"], d["y1"] - d["y0"] + 0.12, 2 * d["hb"])),
         hbox("rcs4_driver", (0, 0.32, 0.36), (0.36, 0.30, 0.22))]
    for dx, _, dz in RCS_DIRS:
        p0 = (dx * d["hb"], d["yc"], dz * d["hb"])
        p1 = (dx * (d["hb"] + 0.13), d["yc"], dz * (d["hb"] + 0.13))
        p2 = (dx * (d["hb"] + 0.27), d["yc"], dz * (d["hb"] + 0.27))
        H.append(hcyl("rcs4_coil", p0, p1, 0.30))
        H.append(hcyl("rcs4_emitter", p1, p2, 0.25))
    return H


# ================================================================= BZ-1 jump-charge tank (std.tank.jumpcharge): a mini tank container
JC = dict(hx=0.465, hy=0.465, hz=0.744, cs=0.10, b=0.06, r=0.38, zc=0.52, zh=0.70)


def jc_parts(lod):
    d = JC
    hx, hy, hz, cs, b, r = d["hx"], d["hy"], d["hz"], d["cs"], d["b"], d["r"]
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    fr = P["jcharge_frame"] = Part("jcharge_frame")
    aframe(fr, (-hx, -hy, -hz), (hx, hy, hz), b, cs, lod, rail=BL)
    for z in (-0.45, 0.45):                                                # saddle frames + struts to the vessel
        for s in (-1, 1):
            box(fr, CA, (0, s * (hy - b / 2), z), (2 * hx - 2 * b, b * 0.8, 0.05))
            box(fr, CA, (s * (hx - b / 2), 0, z), (b * 0.8, 2 * hy - 2 * b, 0.05))
        if lod < 2:
            for k in range(4):
                a = math.pi / 4 + k * math.pi / 2
                box(fr, CA, (0.43 * math.cos(a), 0.43 * math.sin(a), z), (0.16, 0.035, 0.05), rot_z(a))
    if close():
        for sx in (-1, 1):
            for sz in (-1, 1):
                hex_bolt(fr, "eq_steel", (sx * (hx - b / 2), -hy + b, sz * 0.45), (0, 1, 0), 0.03, 0.012)

    sh = P["jcharge_shell"] = Part("jcharge_shell")
    zc, zh = d["zc"], d["zh"]
    lathe(sh, WH, [(0, -zh), (0.20, -zh + 0.02), (0.31, -zh + 0.08), (r, -zc), (r, zc), (0.31, zh - 0.08), (0.20, zh - 0.02), (0, zh)],
          seg, soft=40)
    if lod < 2:
        for z in (-0.25, 0.25):
            lathe(sh, BL, crect(r - 0.005, r + 0.012, z - 0.05, z + 0.05), seg)                          # marking bands
        lathe(sh, TC, crect(r - 0.005, r + 0.012, -0.03, 0.03), seg)
    if close():
        for z in (-0.45, 0.45):
            lathe(sh, "eq_steel", crect(r, r + 0.008, z - 0.02, z + 0.02), seg)                          # saddle straps

    va = P["jcharge_valve"] = Part("jcharge_valve")
    ylathe(va, BR, [(0, r - 0.03), (0.12, r - 0.03), (0.12, r + 0.02), (0, r + 0.02)], small * 2)          # manlid
    box(va, BR, (0, r + 0.045, 0.18), (0.12, 0.05, 0.12))                                                  # top valve block
    if lod < 2:
        cyl(va, BR, (0, r + 0.03, 0.06), (0, r + 0.03, 0.12), 0.03, small)
        ylathe(va, TC, crect(0.05, 0.065, r + 0.07, r + 0.082), small * 2, t=(0, 0, 0.18))                 # handwheel
        cyl(va, BR, (0, -0.25, zh - 0.04), (0, -0.25, hz - 0.05), 0.05, small)                           # bottom outlet
        lathe(va, "eq_steel", [(0, 0), (0.08, 0), (0.08, 0.03), (0, 0.03)], small, I3, (0, -0.25, hz - 0.05))
        cyl(va, BR, (0, -0.25, zh - 0.10), (0, -0.25, zh - 0.04), 0.05, small)
        cyl(va, BR, (0, -0.22, zh - 0.12), (0, -0.30, zh - 0.12), 0.03, small)
    if close():
        for k in range(8):
            hex_bolt(va, "eq_steel", polar(0.10, k * math.pi / 4, r + 0.02), (0, 1, 0), 0.025, 0.01)
        ylathe(va, "eq_steel", [(0, r + 0.07), (0.015, r + 0.07), (0.015, r + 0.085), (0, r + 0.085)], 8, t=(0, 0, 0.18))
        brass_fitting(va, (0.06, r + 0.05, 0.18), (1, 0, 0), 0.015, lod)
    return P


def jc_hits():
    d = JC
    hx, hy, hz, b = d["hx"], d["hy"], d["hz"], d["b"]
    H = [hcyl("jcharge_shell", (0, 0, -d["zh"]), (0, 0, d["zh"]), d["r"])]
    for sx in (-1, 1):
        for sy in (-1, 1):
            H.append(hbox("jcharge_frame", (sx * (hx - b / 2), sy * (hy - b / 2), 0), (b, b, 2 * hz)))
    H += [hbox("jcharge_valve", (0, d["r"] + 0.04, 0.09), (0.24, 0.12, 0.36)),
          hcyl("jcharge_valve", (0, -0.25, d["zh"] - 0.12), (0, -0.25, hz - 0.02), 0.08)]
    return H


# ================================================================= BT-1 fusion pellet bunker (std.tank.pellets)
PT = dict(hx=0.459, hy=0.459, hz=0.734, h=0.40, c=0.13, yo=0.03, zb=-0.70, zf=0.22, zn=0.48, sn=0.30)


def pt_parts(lod):
    d = PT
    hx, hy, hz, yo = d["hx"], d["hy"], d["hz"], d["yo"]
    seg, small = seg_for(lod), small_for(lod)
    oc = lambda s: octp(d["h"], d["c"], s)
    P = {}
    fr = P["pellet_frame"] = Part("pellet_frame")                          # skid: two rails, castings only at the bottom
    for s in (-1, 1):
        box(fr, BL, (s * 0.36, -hy + 0.035, -0.05), (0.07, 0.07, 2 * hz - 0.30))
        for sz in (-1, 1):
            casting(fr, (s * (hx - 0.05), -hy + 0.05, sz * (hz - 0.05)), 0.10, (s, -1, sz), lod)
    for z in (-0.55, 0.05):
        box(fr, CA, (0, -hy + 0.06, z), (2 * hx - 0.2, 0.05, 0.07))
        for s in (-1, 1):
            box(fr, CA, (s * 0.30, -hy + 0.10, z), (0.10, 0.08, 0.07))
    for s in (-1, 1):
        box(fr, BL, (0, -hy + 0.035, s * (hz - 0.05)), (2 * hx - 0.20, 0.05, 0.06))

    sh = P["pellet_shell"] = Part("pellet_shell")
    t = (0, yo, 0)
    loft(sh, WH, [(oc(1.0), d["zb"]), (oc(1.0), d["zf"]), (oc(d["sn"]), d["zn"])], t=t, caps=(True, True))
    if lod < 2:
        poly = oc(1.0)
        for i in range(8):                                                  # patchwork face plates (flat faces only)
            if i % 2:
                continue
            (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % 8]
            a = math.atan2(-(x1 - x0), (y1 - y0))
            L = math.hypot(x1 - x0, y1 - y0)
            for k in range(2):
                za, zb = d["zb"] + 0.05 + k * 0.46, d["zb"] + 0.46 + k * 0.46
                box(sh, pmat(i + k * 3), ((x0 + x1) / 2 + math.cos(a) * 0.008, (y0 + y1) / 2 + yo + math.sin(a) * 0.008, (za + zb) / 2),
                    (0.016, L * 0.85, zb - za), rot_z(a))
        ylathe(sh, SD, [(0, d["h"] + yo), (0.13, d["h"] + yo), (0.13, d["h"] + yo + 0.02), (0, d["h"] + yo + 0.02)],
               small * 2, t=(0, 0, -0.30))                                  # fill hatch
        if lod == 0:
            for k in range(4):
                box(sh, BR, polar(0.15, k * math.pi / 2 + math.pi / 4, d["h"] + yo + 0.02), (0.06, 0.03, 0.03),
                    rot_y(-(k * math.pi / 2 + math.pi / 4)))
    if close():
        bolt_grid(sh, 1, 1, d["h"] + yo + 0.02, -0.08, 0.08, -0.38, -0.22, 2, 2, 0.025, 0.008)

    au = P["pellet_auger"] = Part("pellet_auger")
    zy = yo
    cyl(au, CA, (0, zy, d["zn"] - 0.04), (0, zy, 0.64), 0.10, small)                                      # auger tube
    box(au, TC, (0, zy, 0.68), (0.24, 0.26, 0.10), chamfer=0.015 if lod == 0 else 0.0)                     # drive motor
    if lod < 2:
        cyl(au, CA, (0, zy - 0.04, 0.58), (0, -0.30, 0.58), 0.065, small)                                  # outlet spout
        lathe(au, BR, [(0, 0), (0.10, 0), (0.10, 0.03), (0, 0.03)], small, axis_to((0, -1, 0)), (0, -0.30, 0.58))
        lathe(au, "eq_steel", crect(0.10, 0.12, -0.02, 0.02), small, I3, (0, zy, 0.52))
    if lod == 0:
        for i in range(4):
            box(au, CA, (0, zy + 0.135, 0.65 + 0.025 * i), (0.20, 0.012, 0.012))
    if close():
        harness(au, bezier((0.10, zy + 0.10, 0.70), (0.25, zy + 0.2, 0.70), (0.35, zy + 0.25, 0.30), (0.30, zy + 0.30, 0.10), 10),
                0.012, 2, ("eq_insulator", TC), tie_every=5)

    lv = P["pellet_level"] = Part("pellet_level")
    xs = d["h"]                                                                                           # sight-glass strip on +X face
    box(lv, GL, (xs + 0.02, yo + 0.12, -0.25), (0.04, 0.06, 0.80))
    box(lv, BR, (xs + 0.025, yo + 0.12, 0.17), (0.05, 0.10, 0.06))
    box(lv, BR, (xs + 0.025, yo + 0.12, -0.67), (0.05, 0.10, 0.06))
    box(lv, BL, (0, yo + 0.05, d["zb"] - 0.02), (0.16, 0.16, 0.04))                                      # rear probe head
    if lod < 2:
        cyl(lv, BR, (0.05, yo + 0.05, d["zb"] - 0.04), (0.05, yo + 0.05, -hz + 0.002), 0.025, small)
    if close():
        for z in (-0.55, -0.35, -0.15, 0.05):
            box(lv, "eq_steel", (xs + 0.042, yo + 0.12, z), (0.006, 0.07, 0.01))
        harness(lv, sag((0.06, yo + 0.10, d["zb"] - 0.04), (0.30, yo + 0.30, d["zb"] - 0.01), 0.05, 8), 0.01, 1, ("eq_insulator",))
    return P


def pt_hits():
    d = PT
    hx, hy, hz, yo = d["hx"], d["hy"], d["hz"], d["yo"]
    return [hbox("pellet_frame", (0, -hy + 0.05, 0), (2 * hx, 0.10, 2 * hz)),
            hbox("pellet_shell", (0, yo, (d["zb"] + d["zf"]) / 2), (2 * d["h"], 2 * d["h"], d["zf"] - d["zb"])),
            hbox("pellet_shell", (0, yo, (d["zf"] + d["zn"]) / 2), (2 * d["h"] * 0.65, 2 * d["h"] * 0.65, d["zn"] - d["zf"])),
            hcyl("pellet_auger", (0, yo, d["zn"] - 0.04), (0, yo, 0.73), 0.13),
            hcyl("pellet_auger", (0, yo, 0.58), (0, -0.33, 0.58), 0.10),
            hbox("pellet_level", (d["h"] + 0.025, yo + 0.12, -0.25), (0.05, 0.10, 0.90)),
            hbox("pellet_level", (0, yo + 0.05, d["zb"] - 0.02), (0.16, 0.16, 0.06))]


ASSETS = {"agora_hold100_hold": hold_parts, "agora_c20r_rack": c20_parts, "agora_su1_collar": collar_parts,
          "agora_od4_dronebay": drone_parts, "agora_sk4_pod": pod_parts, "agora_sh20_lifeboat": boat_parts,
          "agora_ps100_passenger": pax_parts, "agora_rcs4_cluster": rcs_parts, "agora_bz1_jumptank": jc_parts,
          "agora_bt1_pellettank": pt_parts}
HITS = {"agora_hold100_hold": hold_hits, "agora_c20r_rack": c20_hits, "agora_su1_collar": collar_hits,
        "agora_od4_dronebay": drone_hits, "agora_sk4_pod": pod_hits, "agora_sh20_lifeboat": boat_hits,
        "agora_ps100_passenger": pax_hits, "agora_rcs4_cluster": rcs_hits, "agora_bz1_jumptank": jc_hits,
        "agora_bt1_pellettank": pt_hits}
MODEL_ASSETS = {"std.cargo.hold100": "agora_hold100_hold", "std.cargo.c20lock": "agora_c20r_rack",
                "std.dock.collar": "agora_su1_collar", "std.bay.drone4": "agora_od4_dronebay",
                "std.surv.pod4": "agora_sk4_pod", "std.surv.boat20": "agora_sh20_lifeboat",
                "agora.hab.pax100": "agora_ps100_passenger", "std.rcs.cluster": "agora_rcs4_cluster",
                "std.tank.jumpcharge": "agora_bz1_jumptank", "std.tank.pellets": "agora_bt1_pellettank"}
