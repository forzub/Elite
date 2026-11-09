"""Beacon Authority core fab "Zero Point" (Tau Ceti state secret fab; the only reference jump cores): jump cores
Z-0.5 / Z-1 / Z-5 and the T-1 Beacon Authority transponder.

School: seamless near-black monoliths with engraved (violet-inlaid) geometry; no serviceable panels. Only the sealed
exterior is modelled (jump-core internals are author-only): the field-emitter monolith, the lotosite damper /
annealing blocks as sealed modules, the tamper-seal set (red straps, seal wires, crimp pucks) and the interfaces.
Interface standard: sealed keyed socket (asymmetric key lugs, so only a matching core seats) under a tamper seal.
Jump cores are ship-axis units centred on the origin, socket on the -Z face, mounting lugs on -Y. The transponder is
a hull-skin box: mounting face at y = 0, stub antenna on +Y. The three cores differ in layout, not only in scale:
Z-0.5 octagonal monolith with two side damper slabs; Z-1 cube with four corner damper pillars and a round emitter
face; Z-5 block sandwiched between eight annealing tiles, twin side emitter discs and twin sockets.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_zeropoint_black": ((0.03, 0.03, 0.04), (0.35, 0.35, 0.40), 150),
    "eq_zeropoint_matte": ((0.07, 0.07, 0.085), (0.10, 0.10, 0.12), 20),
    "eq_zeropoint_violet": ((0.30, 0.18, 0.45), (0.30, 0.25, 0.40), 60),
    "eq_zeropoint_red": ((0.55, 0.12, 0.12), (0.30, 0.25, 0.25), 50),
    "eq_zeropoint_socket": ((0.16, 0.15, 0.18), (0.50, 0.50, 0.55), 120),
})
BLK, MAT, VIO, RED, SOC = ("eq_zeropoint_black", "eq_zeropoint_matte", "eq_zeropoint_violet", "eq_zeropoint_red",
                           "eq_zeropoint_socket")


# ---------------------------------------------------------------- local helpers
def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _face(axis, sign, off):
    """frame of an axis-aligned face: (normal, u, v, centre) with u x v = normal"""
    n = [0, 0, 0]
    n[axis] = sign
    u = [0, 0, 0]
    u[(axis + 1) % 3] = 1
    v = list(_cross(tuple(n), tuple(u)))
    c = [0.0, 0.0, 0.0]
    c[axis] = sign * off
    return tuple(n), tuple(u), tuple(v), tuple(c)


def _engrave(part, frame, segs, lod, w=None, mat=VIO, centre=(0, 0, 0)):
    """violet-inlaid engraving lines on a face: segs = [((u0, v0), (u1, v1)), ...] in face coordinates"""
    if lod == 2:
        return
    n, u, v, c = frame
    w = w or (0.012 if close() else 0.026)
    for (u0, v0), (u1, v1) in segs:
        du, dv = u1 - u0, v1 - v0
        L = math.hypot(du, dv)
        if L < 1e-6:
            continue
        d = tuple((u[k] * du + v[k] * dv) / L for k in range(3))
        b = _cross(d, n)
        R = [[d[0], n[0], b[0]], [d[1], n[1], b[1]], [d[2], n[2], b[2]]]
        p = tuple(centre[k] + c[k] + n[k] * 0.0015 + u[k] * (u0 + u1) / 2 + v[k] * (v0 + v1) / 2 for k in range(3))
        box(part, mat, p, (L + w, 0.003, w), R)


def _poly_segs(pts):
    return [(pts[i], pts[(i + 1) % len(pts)]) for i in range(len(pts))]


def _ngon(rad, k, a0=0.0):
    return [(rad * math.cos(a0 + 2 * math.pi * i / k), rad * math.sin(a0 + 2 * math.pi * i / k)) for i in range(k)]


def _socket_round(part, lod, centre, r, depth, keys, sign=-1):
    """Zero Point keyed socket standing `depth` out of a Z face (sign -1: the -Z face): gunmetal boss, recess with
    contact pins, asymmetric key lugs in the recess wall (only a matching connector seats)"""
    seg = seg_for(lod)
    cx, cy, cz = centre
    R = I3 if sign > 0 else rot_x(math.pi)
    t = (cx, cy, cz)
    c = min(0.015, depth * 0.3) if lod == 0 else 0.0
    ri, fl = r * 0.62, depth * 0.35
    if lod < 2:
        lathe(part, SOC, [(r, 0), (r, depth - c), (r - c, depth), (ri, depth), (ri, fl)], seg, R, t)
        lathe(part, MAT, [(ri, fl), (0, fl)], seg, R, t)
        for a in keys:
            p = (cx + ri * 0.93 * math.cos(a), cy + ri * 0.93 * math.sin(a), cz + sign * (fl + depth) / 2)
            box(part, SOC, p, (r * 0.14, r * 0.14, depth - fl), rot_z(a))
        for k in range(6):
            a = 2 * math.pi * k / 6 + 0.3
            q = (cx + ri * 0.5 * math.cos(a), cy + ri * 0.5 * math.sin(a))
            cyl(part, VIO, (q[0], q[1], cz + sign * fl), (q[0], q[1], cz + sign * (fl + depth * 0.25)), r * 0.04, 6)
    else:
        lathe(part, SOC, [(r, 0), (r, depth), (0, depth)], seg, R, t)


def _seal_tag(part, p, normal, lod, size=0.06):
    """tamper seal: a red crimp puck with a seal wire loop (wire on the close level only)"""
    if lod == 2:
        return
    n = norm(normal)
    cyl(part, RED, p, tuple(p[k] + n[k] * 0.02 for k in range(3)), size / 2, small_for(lod))
    if close():
        ref = (0, 1, 0) if abs(n[1]) < 0.9 else (1, 0, 0)
        a = norm(_cross(n, ref))
        b = _cross(n, a)
        pts = bezier(tuple(p[k] + a[k] * size * 0.4 + n[k] * 0.01 for k in range(3)),
                     tuple(p[k] + a[k] * size * 1.6 + n[k] * 0.012 for k in range(3)),
                     tuple(p[k] + b[k] * size * 1.8 + n[k] * 0.012 for k in range(3)),
                     tuple(p[k] - a[k] * size * 0.4 + n[k] * 0.01 for k in range(3)), 12)
        tube(part, "eq_steel", pts, 0.0025, 4)


def _serial(part, frame, u0, v0, n_chars, lod, h=0.03, centre=(0, 0, 0)):
    """engraved serial (relief) on the close level: a row of tiny raised glyph blocks"""
    if not close():
        return
    n, u, v, c = frame
    for i in range(n_chars):
        hh = h * (0.6 + 0.4 * ((i * 7) % 3) / 2)
        p = tuple(centre[k] + c[k] + n[k] * 0.001 + u[k] * (u0 + i * h * 0.8) + v[k] * v0 for k in range(3))
        R = [[u[0], n[0], v[0]], [u[1], n[1], v[1]], [u[2], n[2], v[2]]]
        box(part, MAT, p, (h * 0.5, 0.002, hh), R)


def _oct(a, c):
    """a square of half side a with 45-degree corners cut by c (8 points, counter-clockwise)"""
    return [(a, -a + c), (a, a - c), (a - c, a), (-a + c, a), (-a, a - c), (-a, -a + c), (-a + c, -a), (a - c, -a)]


def _zloft(part, mat, rings_z):
    """an extrusion along Z through (poly (x, y), z) rings with end caps; first / last bands are end chamfers"""
    rings = [[(x, y, z) for x, y in poly] for poly, z in rings_z]
    nb = len(rings) - 1
    for k, (a, b) in enumerate(zip(rings, rings[1:])):
        n = len(a)
        ez = -0.6 if (k == 0 and nb > 1) else (0.6 if (k == nb - 1 and nb > 1) else 0.0)
        for i in range(n):
            j = (i + 1) % n
            q = [a[i], a[j], b[j], b[i]]
            fn = facet_normal(q)
            if fn is None:
                continue
            mx, my = sum(p[0] for p in q) / 4, sum(p[1] for p in q) / 4
            if fn[0] * mx + fn[1] * my + fn[2] * ez < 0:
                fn = (-fn[0], -fn[1], -fn[2])
            part.face(mat, [part.vert(p, fn) for p in q], fn)
    for ring, s in ((rings[0], -1), (rings[-1], 1)):
        nn = (0, 0, s)
        part.face(mat, [part.vert(p, nn) for p in ring], nn)


# ---------------------------------------------------------------- Z-0.5: octagonal monolith, two side damper slabs
Z05 = dict(h=1.339 / 2, a=0.60, cut=0.20)


def z05_parts(lod):
    d = Z05
    h, a, cut = d["h"], d["a"], d["cut"]
    P = {}
    f = P["z05_field"] = Part("z05_field")
    ch = 0.03 if lod == 0 else 0.0
    zb, zf = -a, h
    if ch:
        _zloft(f, BLK, [(_oct(a - ch, cut), zb), (_oct(a, cut), zb + ch), (_oct(a, cut), zf - ch), (_oct(a - ch, cut), zf)])
    else:
        _zloft(f, BLK, [(_oct(a, cut), zb), (_oct(a, cut), zf)])
    front = _face(2, 1, zf)
    _engrave(f, front, _poly_segs(_oct(0.48, 0.16)) + _poly_segs(_oct(0.32, 0.11)) + _poly_segs(_oct(0.16, 0.05)), lod)
    _engrave(f, front, [((0.16, 0.0), (0.32, 0.0)), ((-0.16, 0.0), (-0.32, 0.0)), ((0.0, 0.16), (0.0, 0.32)), ((0.0, -0.16), (0.0, -0.32))], lod)
    bottom = _face(1, -1, a)
    _engrave(f, bottom, [((-0.45, -0.3), (0.45, -0.3)), ((-0.45, 0.3), (0.45, 0.3))], lod, centre=(0, 0, (zb + zf) / 2))
    _serial(f, front, -0.30, -0.54, 10, lod)
    # dampers: sealed lotosite slabs on +-X with alignment ribs (recessed so the seals stay inside the envelope)
    dm = P["z05_damper"] = Part("z05_damper")
    dt = h - a - 0.026
    xo = a + dt
    for s in (-1, 1):
        x = s * (a + dt / 2)
        box(dm, MAT, (x, 0, (zb + zf) / 2), (dt, 0.80, 0.96), chamfer=0.02 if lod == 0 else 0.0)
        if lod < 2:
            for yy in (-0.25, 0.0, 0.25):
                box(dm, BLK, (s * (xo + 0.004), yy, (zb + zf) / 2), (0.008, 0.035, 0.80))
            box(dm, VIO, (s * (xo + 0.004), -0.36, (zb + zf) / 2 + 0.32), (0.008, 0.03, 0.18))
    # seals: tamper-control block on top, red straps over each damper joint
    se = P["z05_seal"] = Part("z05_seal")
    box(se, BLK, (0, a + (h - a) / 2, 0.05), (0.50, h - a, 0.40), chamfer=0.02 if lod == 0 else 0.0)
    box(se, RED, (0, a + (h - a) / 2, 0.05), (0.512, 0.03, 0.412))
    for s in (-1, 1):
        for zz in (-0.38, 0.45):
            box(se, RED, (s * (xo + 0.003), 0, zz), (0.006, 0.84, 0.06))
            box(se, RED, (s * (a + dt / 2), 0.403, zz), (dt + 0.012, 0.006, 0.06))
        _seal_tag(se, (s * (xo + 0.002), 0.30, 0.45), (s, 0, 0), lod)
    _seal_tag(se, (0.20, h + 0.001, 0.20), (0, 1, 0), lod)
    so = P["z05_socket"] = Part("z05_socket")
    _socket_round(so, lod, (0, 0, zb), 0.30, h - a, [math.radians(90), math.radians(205), math.radians(320)])
    _seal_tag(so, (0.0, -0.30 - 0.05, zb), (0, 0, -1), lod, 0.05)
    mt = P["z05_mount"] = Part("z05_mount")
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(mt, SOC, (sx * 0.30, -a - (h - a) / 2, (zb + zf) / 2 + sz * 0.40), (0.22, h - a, 0.26), chamfer=0.015 if lod == 0 else 0.0)
    return P


def z05_hits():
    d = Z05
    h, a = d["h"], d["a"]
    zc = (h - a) / 2
    H = [hbox("z05_field", (0, 0, zc), (2 * a, 2 * a, h + a)),
         hbox("z05_seal", (0, a + (h - a) / 2, 0.05), (0.50, h - a, 0.40))]
    for s in (-1, 1):
        H.append(hbox("z05_damper", (s * (a + (h - a - 0.026) / 2), 0, zc), (h - a - 0.026, 0.80, 0.96)))
    H.append(hcyl("z05_socket", (0, 0, -h), (0, 0, -a), 0.30))
    for sx in (-1, 1):
        for sz in (-1, 1):
            H.append(hbox("z05_mount", (sx * 0.30, -a - (h - a) / 2, zc + sz * 0.40), (0.22, h - a, 0.26)))
    return H


# ---------------------------------------------------------------- Z-1: cube with four corner damper pillars
Z1 = dict(h=1.587 / 2, b=0.62, bz=0.70, p=0.30)


def z1_parts(lod):
    d = Z1
    h, b, bz, pw = d["h"], d["b"], d["bz"], d["p"]
    seg = seg_for(lod)
    P = {}
    f = P["z1_field"] = Part("z1_field")
    box(f, BLK, (0, 0, 0), (2 * b, 2 * b, 2 * bz), chamfer=0.04 if lod == 0 else 0.0)
    # round emitter face on +Z with engraved rings
    lathe(f, BLK, [(0.50, bz), (0.50, h - 0.04), (0.46, h), (0, h)], seg, I3, (0, 0, 0), soft=30)
    if lod < 2:
        for rr in (0.14, 0.27, 0.40):
            lathe(f, VIO, crect(rr - 0.012, rr + 0.012, h, h + 0.003), seg)
        for k in range(6):                                                # radial spokes
            a0 = 2 * math.pi * k / 6
            _engrave(f, _face(2, 1, h), [((0.14 * math.cos(a0), 0.14 * math.sin(a0)), (0.27 * math.cos(a0), 0.27 * math.sin(a0)))], lod)
    for ax, s in ((0, 1), (0, -1), (1, 1), (1, -1)):                      # diamond-in-square engraving on the 4 sides
        fr = _face(ax, s, b)
        q = 0.42 if ax == 0 else 0.42
        sq = [(-q, -q * 1.1), (q, -q * 1.1), (q, q * 1.1), (-q, q * 1.1)]
        dia = [(0, -q * 1.1), (q, 0), (0, q * 1.1), (-q, 0)]
        segs = _poly_segs(sq) + _poly_segs(dia) if ax == 0 else _poly_segs(dia)
        # face coordinates: for X faces u = Y, v = Z; for Y faces u = Z, v = X
        _engrave(f, fr, [((p0[0], p0[1]), (p1[0], p1[1])) for p0, p1 in segs] if ax == 0 else
                 [((p0[1], p0[0]), (p1[1], p1[0])) for p0, p1 in segs], lod)
    _serial(f, _face(0, 1, b), -0.35, -0.55, 12, lod)
    # dampers: four corner pillars along Z
    dm = P["z1_damper"] = Part("z1_damper")
    pc = h - pw / 2
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(dm, MAT, (sx * pc, sy * pc, 0), (pw, pw, 2 * bz - 0.06), chamfer=0.03 if lod == 0 else 0.0)
            if lod < 2:
                for zz in (-0.40, 0.0, 0.40):                            # rib bands
                    box(dm, BLK, (sx * pc, sy * pc, zz), (pw + 0.01, pw + 0.01, 0.04))
                box(dm, VIO, (sx * (h - 0.004), sy * pc, 0.20), (0.004, 0.10, 0.025))
    se = P["z1_seal"] = Part("z1_seal")
    for sx in (-1, 1):
        for sy in (-1, 1):
            for zz in (-bz + 0.12, bz - 0.12):                           # red seal bands round each pillar end
                box(se, RED, (sx * pc, sy * pc, zz), (pw + 0.012, pw + 0.012, 0.05))
            _seal_tag(se, (sx * (h + 0.002), sy * pc, bz - 0.20), (sx, 0, 0), lod, 0.05)
    box(se, BLK, (0, b + 0.03, -0.30), (0.40, 0.06, 0.30), chamfer=0.02 if lod == 0 else 0.0)   # tamper monitor
    box(se, RED, (0, b + 0.03, -0.30), (0.412, 0.02, 0.312))
    so = P["z1_socket"] = Part("z1_socket")
    box(so, SOC, (0, 0, -bz - 0.03), (0.66, 0.66, 0.06), chamfer=0.02 if lod == 0 else 0.0)
    _socket_round(so, lod, (0, 0, -bz - 0.06), 0.26, h - bz - 0.06, [math.radians(45), math.radians(160), math.radians(250), math.radians(290)])
    _seal_tag(so, (0.25, 0.25, -bz - 0.06), (0, 0, -1), lod, 0.05)
    mt = P["z1_mount"] = Part("z1_mount")
    for sz in (-1, 1):
        box(mt, SOC, (0, -b - 0.05, sz * 0.40), (0.50, 0.10, 0.22), chamfer=0.02 if lod == 0 else 0.0)
    return P


def z1_hits():
    d = Z1
    h, b, bz, pw = d["h"], d["b"], d["bz"], d["p"]
    pc = h - pw / 2
    H = [hbox("z1_field", (0, 0, 0), (2 * b, 2 * b, 2 * bz)),
         hcyl("z1_field", (0, 0, bz), (0, 0, h), 0.50)]
    for sx in (-1, 1):
        for sy in (-1, 1):
            H.append(hbox("z1_damper", (sx * pc, sy * pc, 0), (pw, pw, 2 * bz - 0.06)))
    H += [hbox("z1_seal", (0, b + 0.03, -0.30), (0.40, 0.06, 0.30)),
          hbox("z1_socket", (0, 0, -bz - 0.03), (0.66, 0.66, 0.06)),
          hcyl("z1_socket", (0, 0, -h), (0, 0, -bz - 0.06), 0.26)]
    for sz in (-1, 1):
        H.append(hbox("z1_mount", (0, -b - 0.05, sz * 0.40), (0.50, 0.10, 0.22)))
    return H


# ---------------------------------------------------------------- Z-5: block between eight annealing tiles
Z5 = dict(h=2.41 / 2, bx=0.95, by=0.92, bz=1.05)


def z5_parts(lod):
    d = Z5
    h, bx, by, bz = d["h"], d["bx"], d["by"], d["bz"]
    seg = seg_for(lod)
    P = {}
    f = P["z5_field"] = Part("z5_field")
    box(f, BLK, (0, 0, 0), (2 * bx, 2 * by, 2 * bz), chamfer=0.08 if lod < 2 else 0.0)
    Rx = axis_to((1, 0, 0))
    for s in (-1, 1):                                                     # twin field-emitter discs on +-X
        R = Rx if s > 0 else axis_to((-1, 0, 0))
        lathe(f, BLK, [(0.72, 0), (0.72, 0.12), (0.66, 0.14), (0.50, 0.14), (0.50, h - bx - 0.03), (0.47, h - bx), (0, h - bx)], seg, R, (s * bx, 0, 0), soft=30)
        if lod < 2:
            for rr in (0.18, 0.32, 0.44):
                lathe(f, VIO, crect(rr - 0.013, rr + 0.013, h - bx, h - bx + 0.003), seg, R, (s * bx, 0, 0))
            lathe(f, VIO, crect(0.60, 0.625, 0.14, 0.143), seg, R, (s * bx, 0, 0))
    # triangle-lattice engraving on the +-Z faces (around the sockets / seal block)
    tri = []
    q = 0.80
    for k in range(-2, 3):
        o = k * 0.40
        tri.append(((-q, o), (q, o)))
    for k in range(-3, 4):
        x0 = k * 0.40
        tri.append(((x0 - 0.46, -0.80), (x0 + 0.46, 0.80)))
        tri.append(((x0 + 0.46, -0.80), (x0 - 0.46, 0.80)))
    clip = []
    for (u0, v0), (u1, v1) in tri:                                        # clip to the face square |u|,|v| <= 0.8
        pts = []
        for t in [i / 40 for i in range(41)]:
            u, v = u0 + (u1 - u0) * t, v0 + (v1 - v0) * t
            if abs(u) <= 0.80 and abs(v) <= 0.80:
                pts.append((u, v))
        if len(pts) > 1:
            clip.append((pts[0], pts[-1]))
    for s in (-1, 1):
        fr = _face(2, s, bz)
        # face u = X, v = Y; leave the socket / seal block zones free (lower band)
        _engrave(f, fr, [((u0, v0 * 0.55 + 0.38 * s), (u1, v1 * 0.55 + 0.38 * s)) for (u0, v0), (u1, v1) in clip], lod)
    _serial(f, _face(2, 1, bz), -0.40, 0.88, 14, lod, 0.035)
    # dampers: 2 x 2 sealed annealing tiles above and below
    dm = P["z5_damper"] = Part("z5_damper")
    tw, td, g = 0.86, 0.98, 0.06
    for sy in (-1, 1):
        for sx in (-1, 1):
            for sz in (-1, 1):
                c = (sx * (g / 2 + tw / 2), sy * (by + (h - by) / 2), sz * (g / 2 + td / 2))
                box(dm, MAT, c, (tw, h - by, td), chamfer=0.03 if lod == 0 else 0.0)
                if lod < 2:
                    box(dm, BLK, (c[0], sy * (h + 0.002), c[2]), (tw - 0.20, 0.004, td - 0.20))
                    box(dm, VIO, (c[0] + sx * (tw / 2 - 0.08), sy * (h + 0.004), c[2]), (0.03, 0.004, td - 0.30))
    se = P["z5_seal"] = Part("z5_seal")
    box(se, BLK, (0, -0.45, bz + (h - bz) / 2), (1.10, 0.50, h - bz), chamfer=0.03 if lod == 0 else 0.0)
    box(se, RED, (0, -0.45, bz + (h - bz) / 2), (1.112, 0.05, h - bz + 0.012))
    if lod < 2:
        for k in range(4):                                                # tamper-status windows
            box(se, VIO, (-0.36 + 0.24 * k, -0.30, h + 0.002), (0.14, 0.06, 0.004))
    for sy in (-1, 1):
        for sx in (-1, 1):
            for sz in (-1, 1):                                            # straps tile -> body on the +-Z faces
                box(se, RED, (sx * (g / 2 + tw / 2), sy * (by + 0.02), sz * 1.028), (0.08, 0.10, 0.056))
                _seal_tag(se, (sx * (g / 2 + tw / 2) + 0.14, sy * (by + (h - by) / 2), sz * (g / 2 + td)), (0, 0, sz), lod, 0.05)
    so = P["z5_socket"] = Part("z5_socket")
    for sx, keys in ((-1, (60, 170, 300)), (1, (20, 135, 230))):
        _socket_round(so, lod, (sx * 0.45, -0.30, -bz), 0.28, h - bz, [math.radians(k) for k in keys])
        _seal_tag(so, (sx * 0.45, 0.04, -bz), (0, 0, -1), lod, 0.05)
    mt = P["z5_mount"] = Part("z5_mount")
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(mt, SOC, (sx * (bx + (h - bx) / 2), -0.76, sz * 0.82), (h - bx, 0.26, 0.26), chamfer=0.02 if lod == 0 else 0.0)
    return P


def z5_hits():
    d = Z5
    h, bx, by, bz = d["h"], d["bx"], d["by"], d["bz"]
    H = [hbox("z5_field", (0, 0, 0), (2 * bx, 2 * by, 2 * bz)),
         hcyl("z5_field", (bx, 0, 0), (h, 0, 0), 0.72),
         hcyl("z5_field", (-h, 0, 0), (-bx, 0, 0), 0.72)]
    tw, td, g = 0.86, 0.98, 0.06
    for sy in (-1, 1):
        for sx in (-1, 1):
            for sz in (-1, 1):
                H.append(hbox("z5_damper", (sx * (g / 2 + tw / 2), sy * (by + (h - by) / 2), sz * (g / 2 + td / 2)), (tw, h - by, td)))
    H.append(hbox("z5_seal", (0, -0.45, bz + (h - bz) / 2), (1.10, 0.50, h - bz)))
    for sx in (-1, 1):
        H.append(hcyl("z5_socket", (sx * 0.45, -0.30, -h), (sx * 0.45, -0.30, -bz), 0.28))
        for sz in (-1, 1):
            H.append(hbox("z5_mount", (sx * (bx + (h - bx) / 2), -0.76, sz * 0.82), (h - bx, 0.26, 0.26)))
    return H


# ---------------------------------------------------------------- T-1 Beacon Authority transponder (coder)
T1 = dict(w=0.464, hy=0.30, z0=-0.20, z1=0.17)


def t1_parts(lod):
    d = T1
    w, hy, z0, z1 = d["w"], d["hy"], d["z0"], d["z1"]
    P = {}
    mt = P["t1_mount"] = Part("t1_mount")
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(mt, SOC, (sx * (w / 2 - 0.04), 0.01, (z0 + z1) / 2 + sz * 0.13), (0.08, 0.02, 0.08))
    hs = P["t1_housing"] = Part("t1_housing")
    box(hs, BLK, (0, (0.02 + hy) / 2, (z0 + z1) / 2), (w, hy - 0.02, z1 - z0), chamfer=0.025)
    top = _face(1, 1, hy)
    _engrave(hs, top, [((-0.15, -0.12), (-0.15, 0.12)), ((0.15, -0.12), (0.15, 0.12))], lod, 0.008 if close() else 0.025,
             centre=(0, 0, 0))
    _serial(hs, _face(0, 1, w / 2), 0.08, -0.10, 8, lod, 0.02)
    _socket_round(hs, lod, (0, (0.02 + hy) / 2, z0), 0.08, z0 + w / 2, [math.radians(90), math.radians(225)])
    co = P["t1_coder"] = Part("t1_coder")
    cf = w / 2 if lod == 2 else w / 2 - 0.024                            # coder face (the seal puck stands on it)
    box(co, MAT, (0, 0.15, (z1 + cf) / 2), (0.30, 0.18, cf - z1), chamfer=0.015 if lod == 0 else 0.0)
    if lod < 2:
        box(co, VIO, (0, 0.20, cf + 0.001), (0.20, 0.02, 0.003))
        box(co, RED, (0.12, 0.15, (z1 + cf) / 2 + 0.002), (0.03, 0.188, cf - z1 + 0.006))
    _seal_tag(co, (0.12, 0.10, cf), (0, 0, 1), lod, 0.03)
    an = P["t1_antenna"] = Part("t1_antenna")
    s = small_for(lod)
    ylathe(an, SOC, [(0.06, hy), (0.06, hy + 0.03), (0.03, hy + 0.04)], s, t=(0.10, 0, -0.05))
    ylathe(an, BLK, [(0.025, hy + 0.04), (0.022, w - 0.03), (0.04, w - 0.025), (0.04, w), (0, w)], s, t=(0.10, 0, -0.05))
    if lod < 2:
        ylathe(an, RED, crect(0.023, 0.028, hy + 0.08, hy + 0.10), s, t=(0.10, 0, -0.05))
    return P


def t1_hits():
    d = T1
    w, hy, z0, z1 = d["w"], d["hy"], d["z0"], d["z1"]
    return [hbox("t1_housing", (0, (0.02 + hy) / 2, (z0 + z1) / 2), (w, hy - 0.02, z1 - z0)),
            hcyl("t1_housing", (0, (0.02 + hy) / 2, -w / 2), (0, (0.02 + hy) / 2, z0), 0.08),
            hbox("t1_coder", (0, 0.15, (z1 + w / 2 - 0.024) / 2), (0.30, 0.18, w / 2 - 0.024 - z1)),
            hcyl("t1_antenna", (0.10, hy, -0.05), (0.10, w, -0.05), 0.06),
            hbox("t1_mount", (0, 0.01, (z0 + z1) / 2), (w, 0.02, 0.34))]


ASSETS = {"zeropoint_z05_jump_core": z05_parts, "zeropoint_z1_jump_core": z1_parts, "zeropoint_z5_jump_core": z5_parts,
          "zeropoint_t1_transponder": t1_parts}
HITS = {"zeropoint_z05_jump_core": z05_hits, "zeropoint_z1_jump_core": z1_hits, "zeropoint_z5_jump_core": z5_hits,
        "zeropoint_t1_transponder": t1_hits}
MODEL_ASSETS = {"um.core.z05": "zeropoint_z05_jump_core", "um.core.z1": "zeropoint_z1_jump_core",
                "um.core.z5": "zeropoint_z5_jump_core", "um.xpdr.t1": "zeropoint_t1_transponder"}
