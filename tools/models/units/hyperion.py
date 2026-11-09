"""Hyperion Yards (Tau Ceti; couriers, scouts, Beacon Authority ships; light aerospace school): R-1 field relay (comm),
HR-1 reconnaissance computer, E-1 ECM suite, PA-100 passive/active array radar, SM-1 retractable sensor mast.

School: slender tapered forms, deployable booms, gold MLI foil, carbon struts with white end fittings. Interface
standard: 3-point kinematic mount - three titanium pucks with ball seats on the mounting face (y = 0), the unit carried
on carbon bipods above them. Hull-skin units face +Y; the computer stands on the same three pucks.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_hyperion_gold": ((0.80, 0.62, 0.25), (0.70, 0.60, 0.35), 140),
    "eq_hyperion_seam": ((0.62, 0.46, 0.17), (0.50, 0.42, 0.25), 90),
    "eq_hyperion_carbon": ((0.15, 0.15, 0.16), (0.25, 0.25, 0.25), 50),
    "eq_hyperion_white": ((0.86, 0.87, 0.88), (0.30, 0.30, 0.30), 60),
    "eq_hyperion_ti": ((0.55, 0.56, 0.58), (0.50, 0.50, 0.50), 110),
    "eq_hyperion_tile": ((0.16, 0.18, 0.26), (0.45, 0.45, 0.50), 120),
})
GOLD, SEAM, CARB, HW, TI, TILE = "eq_hyperion_gold", "eq_hyperion_seam", "eq_hyperion_carbon", "eq_hyperion_white", "eq_hyperion_ti", "eq_hyperion_tile"


# ---------------------------------------------------------------- local helpers
def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _lerp(a, b, t):
    return tuple(a[k] + (b[k] - a[k]) * t for k in range(3))


AX = {"y": (0, 1, 0), "z": (0, 0, 1), "x": (1, 0, 0)}


def _ring(poly, h, c=(0, 0, 0), axis="y"):
    if axis == "y":
        return [(c[0] + u, c[1] + h, c[2] + v) for u, v in poly]
    if axis == "z":
        return [(c[0] + u, c[1] + v, c[2] + h) for u, v in poly]
    return [(c[0] + h, c[1] + u, c[2] + v) for u, v in poly]


def _ngon(rad, k, a0=0.0, sx=1.0, sy=1.0):
    return [(sx * rad * math.cos(a0 + 2 * math.pi * i / k), sy * rad * math.sin(a0 + 2 * math.pi * i / k)) for i in range(k)]


def loft(part, mat, rings, axis="y", cap0=True, cap1=True, soft=50.0):
    """skin between rings of equal vertex count ordered along +axis (lathe-like: bottom -> outer -> top)"""
    A = AX[axis]
    sgn = 1.0
    for rg in rings:
        fn = facet_normal(rg)
        if fn:
            sgn = 1.0 if _dot(fn, A) > 0 else -1.0
            break
    n = len(rings[0])
    cs = math.cos(math.radians(soft))
    for a, b in zip(rings, rings[1:]):
        fns = []
        for i in range(n):
            j = (i + 1) % n
            e = _sub(a[j], a[i])
            if _dot(e, e) < 1e-12:
                e = _sub(b[j], b[i])
            u = _sub(b[i], a[i])
            if _dot(u, u) < 1e-12:
                u = _sub(b[j], a[j])
            o = _cross(e, u)
            L = math.sqrt(_dot(o, o))
            fns.append((sgn * o[0] / L, sgn * o[1] / L, sgn * o[2] / L) if L > 1e-12 else None)
        for i in range(n):
            j = (i + 1) % n
            fn = fns[i]
            if fn is None:
                continue
            p, q = fns[i - 1], fns[j]
            ni = norm(tuple(fn[k] + p[k] for k in range(3))) if p and _dot(fn, p) > cs else fn
            nj = norm(tuple(fn[k] + q[k] for k in range(3))) if q and _dot(fn, q) > cs else fn
            part.face(mat, [part.vert(a[i], ni), part.vert(a[j], nj), part.vert(b[j], nj), part.vert(b[i], ni)], fn)
    neg = (-A[0], -A[1], -A[2])
    if cap0:
        part.face(mat, [part.vert(p, neg) for p in rings[0]], neg)
    if cap1:
        part.face(mat, [part.vert(p, A) for p in rings[-1]], A)


def _seg(lod):
    return max(6, small_for(lod) - 2)


def strut(part, p0, p1, r, lod, ends=True):
    """carbon strut with white end fittings"""
    s = _seg(lod) if lod < 2 else 5
    cyl(part, CARB, p0, p1, r, s)
    if ends and lod < 2:
        d = norm(_sub(p1, p0))
        L = math.dist(p0, p1)
        e = min(0.08, L * 0.2)
        cyl(part, HW, p0, tuple(p0[k] + d[k] * e for k in range(3)), r * 1.45, s)
        cyl(part, HW, tuple(p1[k] - d[k] * e for k in range(3)), p1, r * 1.45, s)


def ball(part, mat, c, r, seg):
    prof = [(r * math.sin(math.pi * i / 6), c[1] - r * math.cos(math.pi * i / 6)) for i in range(7)]
    ylathe(part, mat, prof, seg, t=(c[0], 0, c[2]), soft=40)


def kin_feet(part, lod, pts, top=0.12):
    """Hyperion 3-point kinematic mount: titanium puck + cone seat + ball on the mounting face at each point"""
    s = small_for(lod) if lod < 2 else 6
    for x, z in pts:
        ylathe(part, TI, [(0.10, 0.0), (0.10, 0.03), (0.07, 0.05), (0, 0.05)] if lod < 2 else [(0.10, 0.0), (0.10, 0.05), (0, 0.05)], s, t=(x, 0, z))
        if lod < 2 and top >= 0.1:
            ylathe(part, HW, [(0.06, 0.05), (0.035, 0.08), (0, 0.08)], s, t=(x, 0, z))
            ball(part, TI, (x, top - 0.035, z), 0.035, s)
        if close():   # preload flexure bolts on the puck
            for k in range(3):
                a = 2 * math.pi * k / 3 + 0.5
                hex_bolt(part, TI, (x + 0.085 * math.cos(a), 0.03, z + 0.085 * math.sin(a)), (0, 1, 0), 0.02, 0.012)


def helix(p0, p1, r, turns, n):
    """points of a cable spiral around the straight segment p0 -> p1"""
    d = norm(_sub(p1, p0))
    ref = (0, 1, 0) if abs(d[1]) < 0.9 else (1, 0, 0)
    u = norm(_cross(d, ref))
    v = _cross(d, u)
    out = []
    for i in range(n + 1):
        t = i / n
        a = 2 * math.pi * turns * t
        c = _lerp(p0, p1, t)
        out.append(tuple(c[k] + r * (u[k] * math.cos(a) + v[k] * math.sin(a)) for k in range(3)))
    return out


def foil_seams(part, rings_y, rad, k, a0, lod):
    """MLI blanket seams: thin darker-gold rings standing proud of a k-gon blanket (close level only)"""
    if not close():
        return
    for y in rings_y:
        loft(part, SEAM, [_ring(_ngon(rad, k, a0), y - 0.008), _ring(_ngon(rad + 0.006, k, a0), y - 0.008),
                          _ring(_ngon(rad + 0.006, k, a0), y + 0.008), _ring(_ngon(rad, k, a0), y + 0.008)], cap0=False, cap1=False, soft=10)


def tri_pts(r, a0=math.pi / 2):
    return [(r * math.cos(a0 + 2 * math.pi * k / 3), r * math.sin(a0 + 2 * math.pi * k / 3)) for k in range(3)]


# ---------------------------------------------------------------- R-1 field relay (feeder, transceiver)
R1 = dict(R=1.357, H=1.357, hub_y=0.90, rim_y=1.25, t=0.014)


def _r1_y(r):
    d = R1
    return d["hub_y"] + 0.02 + (d["rim_y"] - d["hub_y"] - 0.02) * (r / d["R"]) ** 2


def r1_parts(lod):
    d = R1
    seg = seg_for(lod)
    P = {}
    m = P["r1_mount"] = Part("r1_mount")
    feet = tri_pts(0.78)
    kin_feet(m, lod, feet)
    for k, (x, z) in enumerate(feet):                                     # bipods to the transceiver foot ring
        a = math.atan2(z, x)
        for s in (-1, 1):
            q = (0.27 * math.cos(a + s * 0.7), 0.22, 0.27 * math.sin(a + s * 0.7))
            strut(m, (x, 0.12, z), q, 0.022, lod)
    t = P["r1_transceiver"] = Part("r1_transceiver")
    hx = lambda r: _ngon(r, 6, math.pi / 6)
    loft(t, HW, [_ring(hx(0.30), 0.18), _ring(hx(0.33), 0.24)], cap1=False, soft=10)
    loft(t, GOLD, [_ring(hx(0.33), 0.24), _ring(hx(0.42), 0.30), _ring(hx(0.42), 0.46), _ring(hx(0.24), 0.66)], cap0=False, soft=10)
    foil_seams(t, (0.33, 0.40), 0.42, 6, math.pi / 6, lod)
    for s in (-1, 1):                                                     # stand-off radiator blades
        box(t, HW, (s * 0.62, 0.40, 0), (0.02, 0.30, 0.46))
        if lod < 2:
            for zz in (-0.15, 0.15):
                strut(t, (s * 0.40, 0.40, zz), (s * 0.61, 0.40, zz), 0.014, lod, ends=False)
    mst = P["r1_mast"] = Part("r1_mast")
    cyl(mst, CARB, (0, 0.66, 0), (0, d["hub_y"], 0), 0.055, _seg(lod))
    ylathe(mst, HW, [(0.15, d["hub_y"] - 0.04), (0.15, d["hub_y"] + 0.04), (0, d["hub_y"] + 0.04)], small_for(lod))
    # umbrella reflector: 12 gold-mesh gores on carbon ribs (deployable)
    rf = P["r1_reflector"] = Part("r1_reflector")
    ns = 7 if lod < 2 else 4
    rs = [0.14 + (d["R"] - 0.14) * i / ns for i in range(ns + 1)]
    top = [(r, _r1_y(r) + d["t"]) for r in rs]
    bot = [(r, _r1_y(r)) for r in rs]
    ylathe(rf, GOLD, bot + top[::-1] + [bot[0]], 12, smooth=False)
    if lod < 2:
        for k in range(12):
            a = 2 * math.pi * k / 12
            pts = [polar(r, a, _r1_y(r) - 0.016) for r in rs]
            tube(rf, CARB, pts, 0.016, 5)
            box(rf, HW, polar(d["R"] - 0.03, a, _r1_y(d["R"]) - 0.005), (0.06, 0.05, 0.06), rot_y(-a))
    else:
        for k in range(0, 12, 2):
            a = 2 * math.pi * k / 12
            cyl(rf, CARB, polar(0.14, a, _r1_y(0.14) - 0.02), polar(d["R"], a, _r1_y(d["R"]) - 0.02), 0.02, 4)
    # feeder: boom + feed horn housing at the top, three stays to the ribs
    f = P["r1_feeder"] = Part("r1_feeder")
    cyl(f, CARB, (0, d["hub_y"] + 0.04, 0), (0, 1.26, 0), 0.032, _seg(lod))
    ylathe(f, HW, [(0.05, 1.255), (0.11, 1.27), (0.11, 1.33), (0.08, d["H"]), (0, d["H"])], small_for(lod) + 4)
    if lod < 2:
        ylathe(f, TILE, [(0.0, 1.252), (0.05, 1.252)], small_for(lod) + 4)
        for k in range(3):
            a = 2 * math.pi * k / 3 + math.pi / 12
            strut(f, polar(0.10, a, 1.30), polar(0.95, a, _r1_y(0.95) + d["t"]), 0.008, lod, ends=False)
    if close():   # waveguide + coax spiral along the boom
        tube(f, TI, helix((0, d["hub_y"] + 0.05, 0), (0, 1.25, 0), 0.045, 2.5, 30), 0.008, 6)
        tube(mst, SEAM, helix((0, 0.67, 0), (0, d["hub_y"] - 0.05, 0), 0.065, 1.5, 20), 0.008, 6)
    return P


def r1_hits():
    d = R1
    H = [hcyl("r1_transceiver", (0, 0.18, 0), (0, 0.66, 0), 0.42),
         hbox("r1_transceiver", (0.62, 0.40, 0), (0.04, 0.30, 0.46)),
         hbox("r1_transceiver", (-0.62, 0.40, 0), (0.04, 0.30, 0.46)),
         hcyl("r1_mast", (0, 0.66, 0), (0, d["hub_y"] + 0.04, 0), 0.15),
         hcyl("r1_reflector", (0, d["hub_y"], 0), (0, 1.06, 0), 0.85),
         hcyl("r1_reflector", (0, 1.06, 0), (0, d["rim_y"] + d["t"], 0), d["R"]),
         hcyl("r1_feeder", (0, d["hub_y"] + 0.04, 0), (0, d["H"], 0), 0.11)]
    for x, z in tri_pts(0.78):
        H.append(hcyl("r1_mount", (x, 0, z), (x, 0.12, z), 0.10))
    return H


# ---------------------------------------------------------------- HR-1 reconnaissance computer (board, memory, psu)
HR1 = dict(hx=0.965, hz=0.965, y0=0.13, y1=1.17, foot_z=0.90)


def hr1_parts(lod):
    d = HR1
    hx, hz, y0, y1 = d["hx"], d["hz"], d["y0"], d["y1"]
    P = {}
    fr = P["hr1_frame"] = Part("hr1_frame")
    fz = d["foot_z"]
    feet = [(-0.75, -fz), (0.75, -fz), (0.0, fz)]
    kin_feet(fr, lod, feet)
    r = 0.026
    C = [(sx * hx, yy, sz * hz) for sx in (-1, 1) for yy in (y0, y1) for sz in (-1, 1)]
    edges = []
    for sx in (-1, 1):
        for sz in (-1, 1):
            edges.append(((sx * hx, y0, sz * hz), (sx * hx, y1, sz * hz)))
    for yy in (y0, y1):
        for s in (-1, 1):
            edges.append(((-hx, yy, s * hz), (hx, yy, s * hz)))
            edges.append(((s * hx, yy, -hz), (s * hx, yy, hz)))
    for s in (-1, 1):                                                     # diagonal braces on the side frames
        edges.append(((s * hx, y0, -hz), (s * hx, y1, hz)))
    edges.append(((-hx, y0, -hz), (hx, y1, -hz)))
    for zz in (-fz, -0.62, 0.62, fz):                                     # bottom cross members (feet, memory / psu cradles)
        edges.append(((-hx, y0, zz), (hx, y0, zz)))
    for p0, p1 in edges:
        strut(fr, p0, p1, r, lod, ends=False)
    if lod < 2:
        for c in C:
            box(fr, HW, c, (0.085, 0.085, 0.085), chamfer=0.012)
    # board: tapered MLI body suspended in the frame on four carbon links
    b = P["hr1_board"] = Part("hr1_board")
    bc = (0.0, 0.66, 0.0)
    sec = lambda w, h: [(x, y) for x, y in _ngon(1.0, 8, math.pi / 8, w / 2 / math.cos(math.pi / 8), h / 2 / math.cos(math.pi / 8))]
    loft(b, HW, [_ring(sec(0.60, 0.22), -0.86, bc, "z"), _ring(sec(0.84, 0.36), -0.70, bc, "z")], "z", cap1=False, soft=30)
    loft(b, GOLD, [_ring(sec(0.84, 0.36), -0.70, bc, "z"), _ring(sec(0.90, 0.40), -0.55, bc, "z"), _ring(sec(0.90, 0.40), 0.55, bc, "z"),
                   _ring(sec(0.84, 0.36), 0.70, bc, "z")], "z", cap0=False, cap1=False, soft=30)
    loft(b, HW, [_ring(sec(0.84, 0.36), 0.70, bc, "z"), _ring(sec(0.60, 0.22), 0.86, bc, "z")], "z", cap0=False, soft=30)
    if lod < 2:
        for sz in (-1, 1):
            for sx in (-1, 1):
                strut(b, (sx * 0.44, 0.66, sz * 0.45), (sx * hx, y1, sz * 0.45), 0.012, lod, ends=False)
    if close():
        for zz in (-0.3, 0.0, 0.3):
            loft(b, SEAM, [_ring(sec(0.90, 0.40), zz - 0.008, bc, "z"), _ring(sec(0.912, 0.412), zz - 0.008, bc, "z"),
                           _ring(sec(0.912, 0.412), zz + 0.008, bc, "z"), _ring(sec(0.90, 0.40), zz + 0.008, bc, "z")], "z", cap0=False, cap1=False, soft=30)
    # memory: two white canisters with titanium end bands in carbon cradles
    me = P["hr1_memory"] = Part("hr1_memory")
    for yy in (0.40, 0.86):
        cyl(me, HW, (-0.66, yy, -0.78), (-0.66, yy, 0.78), 0.14, small_for(lod) + 4, chamfer=0.03)
        if lod < 2:
            for zz in (-0.62, 0.62):
                lathe(me, TI, crect(0.14, 0.155, zz - 0.03, zz + 0.03), small_for(lod) + 4, I3, (-0.66, yy, 0))
                strut(me, (-0.66, yy - 0.15, zz), (-0.66, y0 if yy < 0.6 else yy - 0.30, zz), 0.012, lod, ends=False)
    # psu: gold-wrapped block with heat pipes to a white radiator plate on top
    p = P["hr1_psu"] = Part("hr1_psu")
    pc = (0.66, 0.0, 0.0)
    loft(p, GOLD, [_ring(_ngon(0.30, 4, math.pi / 4, 1.0, 2.9), 0.22, pc), _ring(_ngon(0.30, 4, math.pi / 4, 1.0, 2.9), 0.78, pc)], soft=10)
    box(p, HW, (0.66, 1.06, 0.0), (0.46, 0.02, 1.50))
    for zz in ((-0.5, 0.0, 0.5) if lod < 2 else (0.0,)):
        for xx in (0.58, 0.74):
            cyl(p, TI, (xx, 0.78, zz), (xx, 1.05, zz), 0.016, 6)
    if close():
        for zz in (-0.25, 0.25):
            loft(p, SEAM, [_ring(_ngon(0.30, 4, math.pi / 4, 1.0, 2.9), 0.50 + zz * 0.4 - 0.008, pc),
                           _ring(_ngon(0.306, 4, math.pi / 4, 1.0, 2.9), 0.50 + zz * 0.4 - 0.008, pc),
                           _ring(_ngon(0.306, 4, math.pi / 4, 1.0, 2.9), 0.50 + zz * 0.4 + 0.008, pc),
                           _ring(_ngon(0.30, 4, math.pi / 4, 1.0, 2.9), 0.50 + zz * 0.4 + 0.008, pc)], cap0=False, cap1=False, soft=10)
        h = P["hr1_harness"] = Part("hr1_harness")
        harness(h, bezier((0.45, 0.50, -0.55), (0.30, 0.50, -0.55), (0.30, 0.55, -0.75), (0.0, 0.62, -0.85), 10), 0.010, 2, (HW, CARB), TI, 4)
        harness(h, bezier((-0.52, 0.40, -0.70), (-0.30, 0.42, -0.80), (-0.25, 0.58, -0.86), (-0.10, 0.64, -0.86), 10), 0.010, 2, (HW, CARB), TI, 4)
        harness(h, bezier((-0.52, 0.86, -0.70), (-0.30, 0.80, -0.80), (-0.20, 0.70, -0.86), (0.08, 0.66, -0.86), 10), 0.010, 2, (HW, CARB), TI, 4)
    return P


def hr1_hits():
    d = HR1
    hx, hz, y0, y1 = d["hx"], d["hz"], d["y0"], d["y1"]
    H = []
    for sx in (-1, 1):
        for sz in (-1, 1):
            H.append(hbox("hr1_frame", (sx * hx, (y0 + y1) / 2, sz * hz), (0.085, y1 - y0 + 0.085, 0.085)))
    for yy in (y0, y1):
        for s in (-1, 1):
            H.append(hbox("hr1_frame", (0, yy, s * hz), (2 * hx, 0.06, 0.06)))
            H.append(hbox("hr1_frame", (s * hx, yy, 0), (0.06, 0.06, 2 * hz)))
    for x, z in [(-0.75, -d["foot_z"]), (0.75, -d["foot_z"]), (0.0, d["foot_z"])]:
        H.append(hcyl("hr1_frame", (x, 0, z), (x, y0, z), 0.10))
    H += [hcyl("hr1_board", (0, 0.66, -0.86), (0, 0.66, 0.86), 0.21),
          hbox("hr1_board", (0, 0.66, 0), (0.90, 0.40, 1.40)),
          hcyl("hr1_memory", (-0.66, 0.40, -0.78), (-0.66, 0.40, 0.78), 0.155),
          hcyl("hr1_memory", (-0.66, 0.86, -0.78), (-0.66, 0.86, 0.78), 0.155),
          hbox("hr1_psu", (0.66, 0.50, 0), (0.43, 0.56, 1.24)),
          hbox("hr1_psu", (0.66, 0.92, 0), (0.46, 0.30, 1.50))]
    return H


# ---------------------------------------------------------------- E-1 ECM suite (amp, array)
E1 = dict(W=3.218, H=1.609, L=3.862, yc=0.56, hinge_x=0.40, hinge_y=0.74, tilt=22.0, wing_t=0.05, wz0=-1.35, wz1=1.45)


def _e1_sec(s):
    return [(0.5 * s, 0.0), (0.3 * s, 0.3 * s), (-0.3 * s, 0.3 * s), (-0.5 * s, 0.0), (-0.3 * s, -0.3 * s), (0.3 * s, -0.3 * s)]


def _e1_wing_frame(side):
    """wing frame: rotation (local X along the span, local +Y = the emitting face), span length, centre"""
    d = E1
    t = math.radians(d["tilt"])
    span = (d["W"] / 2 - d["hinge_x"] - d["wing_t"] / 2 * math.sin(t)) / math.cos(t)
    c = (side * (d["hinge_x"] + span / 2 * math.cos(t)), d["hinge_y"] + span / 2 * math.sin(t), (d["wz0"] + d["wz1"]) / 2)
    return rot_z(side * t), span, c


def e1_parts(lod):
    d = E1
    P = {}
    yc = d["yc"]
    m = P["e1_mount"] = Part("e1_mount")
    feet = [(-0.40, -1.30), (0.40, -1.30), (0.0, 1.30)]
    kin_feet(m, lod, feet)
    for x, z in feet:
        for s in (-1, 1):
            strut(m, (x, 0.12, z), (x * 0.6 + s * 0.16, yc - 0.26, z), 0.02, lod)
    a = P["e1_amp"] = Part("e1_amp")
    ring = lambda s, z: _ring(_e1_sec(s), z, (0, yc, 0), "z")
    zs = [(-d["L"] / 2, 0.14), (-1.62, 0.56), (-1.15, 1.0), (1.15, 1.0), (1.62, 0.62), (d["L"] / 2, 0.16)]
    loft(a, HW, [ring(zs[0][1], zs[0][0]), ring(zs[1][1], zs[1][0])], "z", cap1=False, soft=20)
    loft(a, GOLD, [ring(s, z) for z, s in zs[1:5]], "z", cap0=False, cap1=False, soft=20)
    loft(a, HW, [ring(zs[4][1], zs[4][0]), ring(zs[5][1], zs[5][0])], "z", cap0=False, soft=20)
    if lod < 2:                                                           # radiator cheeks on the lower flanks
        for s in (-1, 1):
            box(a, HW, (s * 0.43, yc - 0.17, 0), (0.02, 0.16, 1.8), rot_z(s * math.radians(-56)))
    if close():
        for z in (-0.8, -0.4, 0.0, 0.4, 0.8):
            loft(a, SEAM, [ring(1.0, z - 0.008), ring(1.012, z - 0.008), ring(1.012, z + 0.008), ring(1.0, z + 0.008)], "z", cap0=False, cap1=False, soft=20)
    # array: two canted deployable emitter wings + a dorsal emitter blade
    ar = P["e1_array"] = Part("e1_array")
    wl = d["wz1"] - d["wz0"]
    for side in (-1, 1):
        R, span, c = _e1_wing_frame(side)
        box(ar, HW, c, (span, d["wing_t"], wl), R, chamfer=0.02 if lod == 0 else 0.0)
        if lod < 2:
            nu, nv = (6, 10) if lod == 0 else (3, 5)
            tu, tv = span / nu, wl / nv
            for i in range(nu):
                for j in range(nv):
                    lp = (-span / 2 + tu * (i + 0.5), d["wing_t"] / 2 + 0.006, -wl / 2 + tv * (j + 0.5))
                    q = apply(R, lp)
                    box(ar, TILE, (c[0] + q[0], c[1] + q[1], c[2] + q[2]), (tu - 0.04, 0.012, tv - 0.04), R)
            for zz in (-0.9, 0.9):                                        # deployment booms
                op = apply(R, (side * (span / 2 - 0.12), -d["wing_t"] / 2, 0))
                strut(ar, (side * 0.44, yc - 0.12, zz), (c[0] + op[0], c[1] + op[1], zz), 0.018, lod)
            cyl(ar, HW, (side * d["hinge_x"], d["hinge_y"], d["wz0"]), (side * d["hinge_x"], d["hinge_y"], d["wz1"]), 0.035, 6)
        if close():
            for zz in (-0.9, 0.9):
                op = apply(R, (side * (span / 2 - 0.12), -d["wing_t"] / 2, 0))
                tube(ar, SEAM, helix((side * 0.44, yc - 0.12, zz), (c[0] + op[0], c[1] + op[1], zz), 0.03, 4, 40), 0.006, 5)
    blade = [(yc + 0.28, -1.05), (yc + 0.28, 1.10), (d["H"], 0.75), (d["H"], -0.45)]
    loft(ar, HW, [_ring(blade, -0.04, axis="x"), _ring(blade, 0.04, axis="x")], "x", soft=10)
    if lod < 2:
        for s in (-1, 1):
            for z0, z1 in ((-0.80, 0.05), (0.15, 0.85)):
                yb, yt = yc + 0.38, d["H"] - 0.10
                box(ar, TILE, (s * 0.046, (yb + yt) / 2, (z0 + z1) / 2), (0.012, yt - yb, z1 - z0))
    return P


def e1_hits():
    d = E1
    yc = d["yc"]
    H = [hbox("e1_amp", (0, yc, 0), (1.0, 0.60, 2.30)),
         hbox("e1_amp", (0, yc, -1.62), (0.66, 0.40, 0.70)),
         hbox("e1_amp", (0, yc, 1.62), (0.70, 0.40, 0.70)),
         hbox("e1_array", (0, (yc + 0.28 + d["H"]) / 2, 0.02), (0.10, d["H"] - yc - 0.28, 1.6))]
    for side in (-1, 1):
        R, span, c = _e1_wing_frame(side)
        H.append(hbox("e1_array", c, (span, d["wing_t"] + 0.03, d["wz1"] - d["wz0"]), (0, 0, side * d["tilt"])))
    for x, z in [(-0.40, -1.30), (0.40, -1.30), (0.0, 1.30)]:
        H.append(hcyl("e1_mount", (x, 0, z), (x, yc - 0.27, z), 0.10))
    return H


# ---------------------------------------------------------------- PA-100 passive / active array radar (array, rx)
PA = dict(R=2.2045, H=1.764, r_in=0.60, yb_in=1.52, yb_out=1.70, t=0.05, tile=0.012)


def _pa_top(r):
    d = PA
    return d["yb_in"] + d["t"] + (d["yb_out"] - d["yb_in"]) * (r - d["r_in"]) / (d["R"] - d["r_in"])


def pa100_parts(lod):
    d = PA
    seg = seg_for(lod)
    P = {}
    m = P["pa100_mount"] = Part("pa100_mount")
    feet = tri_pts(1.30)
    kin_feet(m, lod, feet)
    for x, z in feet:                                                     # tripod legs to the mast head + bipods to the rx drum
        a = math.atan2(z, x)
        strut(m, (x, 0.12, z), polar(0.20, a, 1.40), 0.035, lod)
        for s in (-1, 1):
            strut(m, (x, 0.12, z), polar(0.50, a + s * 0.5, 0.42), 0.022, lod)
    rx = P["pa100_rx"] = Part("pa100_rx")
    ylathe(rx, GOLD, [(0, 0.36), (0.42, 0.36), (0.52, 0.44), (0.52, 0.94), (0.42, 1.02), (0, 1.02)], seg, soft=30)
    if lod < 2:
        for yy in (0.44, 0.94):
            ylathe(rx, HW, crect(0.515, 0.545, yy - 0.03, yy + 0.03), seg)
        for k in range(3):                                                # LNA boxes
            a = 2 * math.pi * k / 3 + math.pi / 6
            box(rx, HW, polar(0.58, a, 0.69), (0.14, 0.26, 0.20), rot_y(-a), chamfer=0.02 if lod == 0 else 0.0)
    foil_seams(rx, (0.60, 0.78), 0.52, seg, 0.0, lod)
    ms = P["pa100_mast"] = Part("pa100_mast")
    cyl(ms, CARB, (0, 1.02, 0), (0, d["yb_in"] - 0.06, 0), 0.12, small_for(lod) + 4)
    ylathe(ms, HW, [(0.20, 1.38), (0.30, 1.42), (d["r_in"] - 0.02, d["yb_in"] - 0.04), (d["r_in"] - 0.02, d["yb_in"] + d["t"] + 0.02),
                    (0.20, d["yb_in"] + d["t"] + 0.04), (0, d["yb_in"] + d["t"] + 0.04)], seg, soft=10)
    ar = P["pa100_array"] = Part("pa100_array")
    gap = 0.012
    for k in range(6):
        a0, a1 = math.pi / 6 + k * math.pi / 3 + gap, math.pi / 6 + (k + 1) * math.pi / 3 - gap
        prof = [(d["r_in"], d["yb_in"]), (d["R"], d["yb_out"]), (d["R"], d["yb_out"] + d["t"]), (d["r_in"], d["yb_in"] + d["t"]), (d["r_in"], d["yb_in"])]
        ylathe(ar, HW, prof, max(4, seg // 6), a0, a1, cap=True, smooth=False)
        if lod < 2:
            rows = [(0.70, 1.05, 2), (1.10, 1.45, 3), (1.50, 1.82, 4), (1.87, 2.14, 5)] if lod == 0 else [(0.70, 1.40, 2), (1.48, 2.14, 3)]
            for r0, r1, nt in rows:
                for i in range(nt):
                    b0 = a0 + (a1 - a0) * i / nt + 0.025 * (1.0 / r0)
                    b1 = a0 + (a1 - a0) * (i + 1) / nt - 0.025 * (1.0 / r0)
                    tp = [(r0, _pa_top(r0) - 0.002), (r1, _pa_top(r1) - 0.002), (r1, _pa_top(r1) + d["tile"]), (r0, _pa_top(r0) + d["tile"]), (r0, _pa_top(r0) - 0.002)]
                    ylathe(ar, TILE, tp, 2, b0, b1, cap=True, smooth=False)
            am = (a0 + a1) / 2                                            # radial boom under the petal + deploy strut
            pts = [polar(r, am, _pa_top(r) - d["t"] - 0.03) for r in (0.55, 2.10)]
            cyl(ar, CARB, pts[0], pts[1], 0.03, 6)
            strut(ar, polar(0.13, am, 1.10), polar(1.35, am, _pa_top(1.35) - d["t"] - 0.05), 0.022, lod)
        if close():   # petal hinge barrels and the spiral feed cable along the deploy strut
            th = math.pi / 6 + (k + 0.5) * math.pi / 3
            p = polar(d["r_in"] + 0.02, th, d["yb_in"] - 0.02)
            tg = (-math.sin(th), 0, math.cos(th))
            cyl(ar, TI, tuple(p[i] - tg[i] * 0.25 for i in range(3)), tuple(p[i] + tg[i] * 0.25 for i in range(3)), 0.025, 8)
            tube(ar, SEAM, helix(polar(0.13, th, 1.10), polar(1.35, th, _pa_top(1.35) - d["t"] - 0.05), 0.035, 5, 50), 0.006, 5)
    return P


def pa100_hits():
    d = PA
    H = [hcyl("pa100_array", (0, d["yb_in"], 0), (0, d["H"], 0), d["R"]),
         hcyl("pa100_mast", (0, 1.02, 0), (0, d["yb_in"] + d["t"] + 0.04, 0), 0.30),
         hcyl("pa100_rx", (0, 0.36, 0), (0, 1.02, 0), 0.52)]
    for x, z in tri_pts(1.30):
        H.append(hcyl("pa100_mount", (x, 0, z), (x, 0.12, z), 0.10))
        H.append(hbox("pa100_mount", ((x + 0.20 * x / 1.30) / 2, 0.76, (z + 0.20 * z / 1.30) / 2), (0.08, math.hypot(1.10, 1.28), 0.08),
                      (0, -math.degrees(math.atan2(z, x)), math.degrees(math.atan2(1.10, 1.28)))))
    return H


# ---------------------------------------------------------------- SM-1 retractable sensor mast (actuator, optics)
SM = dict(R=0.9085, H=6.057, sections=((0.30, 1.12, 2.86), (0.245, 2.86, 4.32), (0.195, 4.32, 5.44)), pod_y=5.81, pod_r=0.247)


def sm1_parts(lod):
    d = SM
    seg = seg_for(lod)
    P = {}
    b = P["sm1_base"] = Part("sm1_base")
    kin_feet(b, lod, tri_pts(0.80), top=0.06)
    ylathe(b, HW, crect(0.62, d["R"], 0.06, 0.16, 0.02 if lod == 0 else 0.0), seg)
    ac = P["sm1_actuator"] = Part("sm1_actuator")
    ylathe(ac, GOLD, [(0.70, 0.16), (0.70, 0.92), (0.56, 1.10), (0.40, 1.10)], seg, soft=10)
    ylathe(ac, HW, crect(0.29, 0.42, 1.10, 1.20, 0.02 if lod == 0 else 0.0), seg)
    for a in (0.0, math.pi):                                              # drive motor pods
        cyl(ac, HW, polar(0.78, a, 0.24), polar(0.78, a, 0.84), 0.10, small_for(lod) + 2, chamfer=0.03)
        if lod < 2:
            box(ac, CARB, polar(0.72, a, 0.54), (0.10, 0.40, 0.08), rot_y(-a))
    foil_seams(ac, (0.40, 0.66), 0.70, seg, 0.0, lod)
    ms = P["sm1_mast"] = Part("sm1_mast")
    for r, y0, y1 in d["sections"]:
        cyl(ms, CARB, (0, y0, 0), (0, y1, 0), r, small_for(lod) + 8 if lod < 2 else 10, caps=False)
        ylathe(ms, HW, crect(r - 0.01, r + 0.04, y1 - 0.08, y1 + 0.02, 0.01 if lod == 0 else 0.0), small_for(lod) + 8)
    if close():
        for (r, y0, y1), turns in zip(d["sections"], (4, 3.5, 2.5)):
            harness(ms, helix((0, y0 + 0.1, 0), (0, y1 - 0.12, 0), r + 0.016, turns, int(turns * 16)), 0.010, 1, (SEAM,), TI, 8)
    op = P["sm1_optics"] = Part("sm1_optics")
    y5 = d["sections"][-1][2]
    ylathe(op, HW, [(0.24, y5 - 0.02), (0.24, y5 + 0.08), (0, y5 + 0.08)], small_for(lod) + 4)
    for s in (-1, 1):                                                     # yoke arms + elevation pivots
        box(op, HW, (s * 0.30, (y5 + 0.04 + d["pod_y"]) / 2 + 0.02, 0), (0.06, d["pod_y"] - y5 + 0.04, 0.16), chamfer=0.02 if lod == 0 else 0.0)
        cyl(op, TI, (s * 0.27, d["pod_y"], 0), (s * 0.34, d["pod_y"], 0), 0.07, small_for(lod))
    box(op, HW, (0, y5 + 0.11, 0), (0.66, 0.06, 0.16))
    pr = d["pod_r"]
    prof = [(0, -0.75), (0.13, -0.75), (0.20, -0.62), (0.21, 0.40), (pr, 0.56), (pr, 0.75), (0, 0.75)]
    lathe(op, GOLD, prof[:4], small_for(lod) + 8, I3, (0, d["pod_y"], 0), soft=30)
    lathe(op, HW, prof[3:], small_for(lod) + 8, I3, (0, d["pod_y"], 0), soft=30)
    if lod < 2:
        lathe(op, TILE, [(0.19, 0.752), (0, 0.752)], small_for(lod) + 8, I3, (0, d["pod_y"], 0))      # main aperture
        for s in (-1, 1):                                                 # side apertures (wide field)
            box(op, TILE, (s * 0.205, d["pod_y"] + 0.04, 0.10), (0.012, 0.12, 0.22))
    if close():
        for z in (-0.4, -0.1, 0.2):
            lathe(op, SEAM, crect(0.205, 0.214, z - 0.008, z + 0.008), 16, I3, (0, d["pod_y"], 0))
        harness(op, bezier((0.30, y5 + 0.14, -0.05), (0.30, d["pod_y"] - 0.1, -0.05), (0.22, d["pod_y"] - 0.12, -0.3), (0.12, d["pod_y"] - 0.17, -0.3), 10),
                0.009, 2, (SEAM, CARB), TI, 4)
    return P


def sm1_hits():
    d = SM
    y5 = d["sections"][-1][2]
    H = [hcyl("sm1_base", (0, 0, 0), (0, 0.16, 0), d["R"]),
         hcyl("sm1_actuator", (0, 0.16, 0), (0, 1.20, 0), 0.70),
         hbox("sm1_actuator", (0.78, 0.54, 0), (0.20, 0.60, 0.20)),
         hbox("sm1_actuator", (-0.78, 0.54, 0), (0.20, 0.60, 0.20))]
    for r, y0, y1 in d["sections"]:
        H.append(hcyl("sm1_mast", (0, y0, 0), (0, y1 + 0.02, 0), r + 0.04))
    H += [hbox("sm1_optics", (0, (y5 + d["pod_y"]) / 2, 0), (0.66, d["pod_y"] - y5 + 0.1, 0.16)),
          hcyl("sm1_optics", (0, d["pod_y"], -0.75), (0, d["pod_y"], 0.75), d["pod_r"])]
    return H


ASSETS = {"hyperion_r1_comm": r1_parts, "hyperion_hr1_computer": hr1_parts, "hyperion_e1_ecm": e1_parts,
          "hyperion_pa100_radar": pa100_parts, "hyperion_sm1_mast": sm1_parts}
HITS = {"hyperion_r1_comm": r1_hits, "hyperion_hr1_computer": hr1_hits, "hyperion_e1_ecm": e1_hits,
        "hyperion_pa100_radar": pa100_hits, "hyperion_sm1_mast": sm1_hits}
MODEL_ASSETS = {"hyp.relay.r1": "hyperion_r1_comm", "hyp.mc.r1": "hyperion_hr1_computer", "hyp.ecm.e1": "hyperion_e1_ecm",
                "hyp.radar.pa100": "hyperion_pa100_radar", "hyp.mast.s1": "hyperion_sm1_mast"}
