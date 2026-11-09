"""Kern Heavy Mining (61 Cygni, frugal mining corporation): KT-1 tug drive, MK-1 mining cutter, DR-5 debris radar,
TR-H heavy grapple, TR-20K raft tow frame, MD-40 impactor mass driver.

School (DESIGN_SCHOOLS.md): brutal mining machinery - square / chamfered-square sections instead of drums, thick bolted
wear plates (rust), skid rails, segmented guards, lifting eyes everywhere, hydraulic hoses; mining yellow, black, rust.
Interface: the Kern square heavy flange - a black square plate with 8 big washered bolts (corners + mid-sides) and
four jacking lugs; nothing round, so a round / hex foreign flange always needs an adapter plate.
Hull-skin units mount at y = 0 with +Y out (the impactor fires along +Z); the tug drive and the tow rigs are axial units
(mounting flange at z = 0, working end towards +Z).
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_kern_yellow": ((0.78, 0.60, 0.12), (0.25, 0.22, 0.15), 35),
    "eq_kern_black": ((0.10, 0.10, 0.10), (0.15, 0.15, 0.15), 25),
    "eq_kern_rust": ((0.42, 0.24, 0.14), (0.10, 0.08, 0.06), 15),
})
YEL, BLK, RST, STL = "eq_kern_yellow", "eq_kern_black", "eq_kern_rust", "eq_steel"
TAU = 2 * math.pi
R_X = rot_y(math.pi / 2)          # lathe local Z -> world +X
FACE = {"+y": I3, "-y": rot_z(math.pi), "+x": rot_z(-math.pi / 2), "-x": rot_z(math.pi / 2),
        "+z": rot_x(math.pi / 2), "-z": rot_x(-math.pi / 2)}          # local +Y -> that direction


# ---------------------------------------------------------------- local kernel (flat solids, frames, Kern details)
def merge(dst, src):
    off = len(dst.v)
    dst.v += src.v
    dst.n += src.n
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + off for i in f) for f in fl)


def _solid(part, mat, P, faces, R=I3, t=(0, 0, 0)):
    W = [tuple(a + b for a, b in zip(apply(R, p), t)) for p in P]
    cen = [sum(p[k] for p in W) / len(W) for k in range(3)]
    for f in faces:
        pts = [W[i] for i in f]
        q = [p for k, p in enumerate(pts) if p != pts[k - 1]]
        if len(q) < 3:
            continue
        n = facet_normal(q)
        if n is None:
            continue
        fc = [sum(p[k] for p in q) / len(q) for k in range(3)]
        if sum((fc[k] - cen[k]) * n[k] for k in range(3)) < 0:
            n = (-n[0], -n[1], -n[2])
        part.face(mat, [part.vert(p, n) for p in q], n)


def hexa(part, mat, c8, R=I3, t=(0, 0, 0)):
    """convex hexahedron: 4 bottom corners, then the 4 top corners in the same order"""
    _solid(part, mat, c8, ((0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)), R, t)


def loft(part, mat, B, yB, T, yT, R=I3, t=(0, 0, 0)):
    """convex (x, z) polygons B (at y = yB) and T (at yT), same vertex count"""
    n = len(B)
    P = [(x, yB, z) for x, z in B] + [(x, yT, z) for x, z in T]
    faces = [tuple(range(n)), tuple(range(n, 2 * n))] + [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    _solid(part, mat, P, faces, R, t)


def zloft(part, mat, poly_xy, z0, z1, poly1=None):
    """an extrusion along Z of a convex (x, y) section (optionally lofted to poly1 at z1)"""
    loft(part, mat, [(x, -y) for x, y in poly_xy], z0, [(x, -y) for x, y in (poly1 or poly_xy)], z1, rot_x(math.pi / 2))


def cham(w, d, c, cx=0.0, cz=0.0):
    """a w x d rectangle with 45 deg corner cuts c (octagon)"""
    a, b = w / 2, d / 2
    return [(cx + x, cz + z) for x, z in ((-a + c, -b), (a - c, -b), (a, -b + c), (a, b - c), (a - c, b), (-a + c, b), (-a, b - c), (-a, -b + c))]


def face_frame(q):
    """quad facet (b0, b1, t1, t0, inner point) -> at(u, v, h), outward normal"""
    b0, b1, t1, t0, inner = q
    n = facet_normal([b0, b1, t1, t0])
    fc = [(b0[k] + b1[k] + t1[k] + t0[k]) / 4 for k in range(3)]
    if sum((fc[k] - inner[k]) * n[k] for k in range(3)) < 0:
        n = (-n[0], -n[1], -n[2])

    def at(u, v, h=0.0):
        lo = [b0[k] + (b1[k] - b0[k]) * u for k in range(3)]
        hi = [t0[k] + (t1[k] - t0[k]) * u for k in range(3)]
        return tuple(lo[k] + (hi[k] - lo[k]) * v + n[k] * h for k in range(3))
    return at, n


def local_frame(face, c, w, d):
    """frame of a w x d rectangle centred at c on a face looking along FACE[face] (u = local x, v = local z)"""
    R = FACE[face] if isinstance(face, str) else face
    n = apply(R, (0, 1, 0))

    def at(u, v, h=0.0):
        p = apply(R, ((u - 0.5) * w, h, (v - 0.5) * d))
        return (c[0] + p[0], c[1] + p[1], c[2] + p[2])
    return at, n


def plate_on(part, mat, at, u0, u1, v0, v1, h0, h1):
    hexa(part, mat, [at(u0, v0, h0), at(u1, v0, h0), at(u1, v1, h0), at(u0, v1, h0),
                     at(u0, v0, h1), at(u1, v0, h1), at(u1, v1, h1), at(u0, v1, h1)])


def frame3(xa, za):
    """rotation with columns x = xa, z = za (orthonormalised), y = z x x"""
    z = norm(za)
    d = sum(a * b for a, b in zip(xa, z))
    x = norm(tuple(a - d * b for a, b in zip(xa, z)))
    y = (z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0])
    return [[x[0], y[0], z[0]], [x[1], y[1], z[1]], [x[2], y[2], z[2]]]


_L = [0]          # mid level of the unit being built


def wear(part, at, n, u0, u1, v0, v1, h=0.03, bolts=(2, 2), across=0.07):
    """Kern wear plate: a rust plate bolted on with big washered bolts (bolts from mid level 0)"""
    plate_on(part, RST, at, u0, u1, v0, v1, 0.0, h)
    if close():                                                   # fillet weld beads on two edges
        du, dv = (u1 - u0) * 0.01, (v1 - v0) * 0.01
        plate_on(part, BLK, at, u0 - du, u1 + du, v0 - dv * 2, v0, 0.0, h * 0.5)
        plate_on(part, BLK, at, u0 - du, u1 + du, v1, v1 + dv * 2, 0.0, h * 0.5)
    if _L[0] == 0:
        nu, nv = bolts
        for i in range(nu):
            for j in range(nv):
                uu = u0 + (u1 - u0) * (0.12 + 0.76 * i / max(1, nu - 1))
                vv = v0 + (v1 - v0) * (0.12 + 0.76 * j / max(1, nv - 1))
                hex_bolt(part, STL, at(uu, vv, h), n, across, across * 0.5, washer=True)


def lift_eye(part, p, up, side, s=0.14):
    """lifting eye: a black lug welded on the surface (normal up) and a steel ring in the plane of up and side"""
    if _L[0] == 2:
        return
    R = frame3(side, up)
    upn = norm(up)
    box(part, BLK, tuple(p[k] + upn[k] * s * 0.30 for k in range(3)), (s * 0.30, s * 1.1, s * 0.6), R)
    c = tuple(p[k] + upn[k] * s * 0.78 for k in range(3))
    lathe(part, STL, crect(s * 0.30, s * 0.48, -s * 0.09, s * 0.09), 12 if close() else 8, frame3(up, side), c)   # ring axis = side


def hose(part, pts, r=0.035):
    """hydraulic / coolant hose (mid level 0): black hose with steel crimp fittings at both ends"""
    if _L[0] != 0:
        return
    tube(part, BLK, pts, r, 8 if close() else 6)
    if close():
        for i in range(1, len(pts) - 1, 2):
            dd = norm(tuple(pts[i + 1][k] - pts[i - 1][k] for k in range(3)))
            lathe(part, STL, crect(r, r * 1.3, -0.012, 0.012), 8, axis_to(dd), pts[i])
    for a, b in ((pts[0], pts[1]), (pts[-1], pts[-2])):
        d = norm(tuple(b[k] - a[k] for k in range(3)))
        cyl(part, STL, a, tuple(a[k] + d[k] * r * 3 for k in range(3)), r * 1.35, 8 if close() else 6)


def grease(part, p, n):
    """grease nipple (close level only)"""
    if not close():
        return
    nn = norm(n)
    cyl(part, STL, p, tuple(p[k] + nn[k] * 0.025 for k in range(3)), 0.009, 6)
    lathe(part, "eq_copper", [(0, 0), (0.008, 0), (0.008, 0.008), (0, 0.014)], 6, axis_to(nn), tuple(p[k] + nn[k] * 0.025 for k in range(3)))


def kflange(lod, s, t, gap=0.0):
    """Kern square heavy flange, local frame: mating plane z = 0, plate z = gap .. gap + t, body towards +Z;
    8 big washered bolts on the back face (corners + mid-sides), four jacking lugs on the edges"""
    f = Part("flange")
    box(f, BLK, (0, 0, gap + t / 2), (s, s, t), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        q = 0.40 * s
        across = max(0.07, 0.07 * s)
        for x, y in ((q, q), (-q, q), (q, -q), (-q, -q), (q, 0), (-q, 0), (0, q), (0, -q)):
            hex_bolt(f, STL, (x, y, gap + t), (0, 0, 1), across, across * 0.6, washer=True)
    if lod == 0:
        for k in range(4):
            Rz = rot_z(k * math.pi / 2)
            box(f, YEL, apply(Rz, (s / 2 + 0.04, 0.0, gap + t * 0.5)), (0.08, 0.16 * s, t * 0.8), Rz)
            if close():                                           # jacking screws and edge dowels
                hex_bolt(f, STL, apply(Rz, (s / 2 + 0.08, 0.0, gap + t * 0.5)), apply(Rz, (1, 0, 0)), 0.05, 0.03)
                for q in (-0.3, 0.3):
                    hex_bolt(f, STL, apply(Rz, (q * s, s / 2 - 0.06, gap + t)), (0, 0, 1), 0.04, 0.02)
    return f


def skin_flange(part, lod, s, t, x=0.0, z=0.0):
    merge(part, placed(kflange(lod, s, t), rot_x(-math.pi / 2), (x, 0.0, z)))


# ---------------------------------------------------------------- KT-1 tug drive (axial: 2.82 x 2.82 x 3.66, pad 2.82 at +Z)
# Field-coupling drive (no nozzle / exhaust). Kern layout: a chamfered-square yellow body on two skid rails, the coil
# ring under eight bolted guard segments, a 4-quadrant pad under a black clamp cross, the field control box on top,
# the backup heat exchanger as a finned block on -X, guarded sensor heads, lifting eyes on the corners.
KT1 = dict(L=3.66, pr=1.41, hw=2.20, hc=0.34, z_h0=0.30, z_h1=2.72, cz0=2.72, cz1=3.28, fl=2.30)


def kt1_parts(lod):
    _L[0] = lod
    seg, small = seg_for(lod), small_for(lod)
    d = KT1
    P = {}
    hw = d["hw"] / 2
    # --- flange to the reactor: square heavy flange, plate z 0.02 .. 0.30
    f = P["kt1_flange"] = Part("kt1_flange")
    merge(f, kflange(lod, d["fl"], 0.28, 0.02))
    lathe(f, BLK, crect(0.70, 0.80, 0.0, 0.02), 4, I3, (0, 0, 0), math.pi / 4, math.pi / 4 + TAU, smooth=False)   # square spigot
    # --- busbar: three square copper terminals on an insulator block in the mating face, cable box behind the flange
    bb = P["kt1_busbar"] = Part("kt1_busbar")
    box(bb, "eq_insulator", (0, -0.45, 0.01), (0.95, 0.30, 0.02))
    for x in (-0.30, 0.0, 0.30):
        box(bb, "eq_copper", (x, -0.45, 0.005), (0.16, 0.16, 0.012))
    box(bb, BLK, (0, -hw + 0.12, 0.52), (0.80, 0.30, 0.44), chamfer=0.03 if lod == 0 else 0.0)
    # --- housing: chamfered-square body, two black stiffener hoops, rust wear plates on the lower chamfers
    h = P["kt1_housing"] = Part("kt1_housing")
    S = cham(d["hw"], d["hw"], d["hc"])
    zloft(h, YEL, S, d["z_h0"], d["z_h1"])
    if lod < 2:
        for zc in (0.90, 1.80):
            zloft(h, BLK, cham(d["hw"] + 0.10, d["hw"] + 0.10, d["hc"] + 0.04), zc - 0.07, zc + 0.07)
    zloft(h, BLK, cham(d["hw"] + 0.06, d["hw"] + 0.06, d["hc"] + 0.02), d["z_h1"] - 0.12, d["z_h1"])     # coil carrier collar
    if lod < 2:
        a0 = hw - d["hc"]
        for sx in (-1, 1):
            for z0, z1 in ((1.0, 1.68), (1.92, 2.52)):
                at, n = face_frame(((sx * a0, -hw, z0), (sx * hw, -a0, z0), (sx * hw, -a0, z1), (sx * a0, -hw, z1), (0, 0, z0)))
                wear(h, at, n, 0.06, 0.94, 0.04, 0.96, 0.03, (1, 2))
        for z0, z1 in ((0.38, 0.80), (1.0, 1.68), (1.92, 2.52)):                    # +X face wear plates
            at, n = local_frame("+x", (hw, 0.0, (z0 + z1) / 2), 1.3, z1 - z0)
            wear(h, at, n, 0.0, 1.0, 0.02, 0.98, 0.035, (2, 2))
        for sx in (-1, 1):
            for zz in (0.55, 2.50):
                lift_eye(h, (sx * 0.80, hw, zz), (0, 1, 0), (0, 0, 1), 0.16)
    # --- coil ring: carrier, 8 field-coil segments, 8 bolted yellow guard segments over them
    c = P["kt1_coil"] = Part("kt1_coil")
    cz0, cz1 = d["cz0"], d["cz1"]
    lathe(c, BLK, crect(0.90, 1.24, cz0, cz1), seg)
    for k in range(8):
        a0, a1 = TAU * k / 8 + 0.05, TAU * (k + 1) / 8 - 0.05
        lathe(c, "eq_field_coil", crect(1.23, 1.32, cz0 + 0.04, cz1 - 0.04), max(2, seg // 8), I3, (0, 0, 0), a0, a1, cap=True)
        if lod < 2:
            am, wa = (a0 + a1) / 2, (a1 - a0) * 0.32
            hexa(c, YEL, [polz(1.32, am - wa, cz0 + 0.07), polz(1.32, am + wa, cz0 + 0.07), polz(1.32, am + wa, cz1 - 0.07), polz(1.32, am - wa, cz1 - 0.07),
                          polz(1.39, am - wa * 0.9, cz0 + 0.10), polz(1.39, am + wa * 0.9, cz0 + 0.10), polz(1.39, am + wa * 0.9, cz1 - 0.10),
                          polz(1.39, am - wa * 0.9, cz1 - 0.10)])
            if lod == 0:
                for s_ in (-1, 1):
                    for zz in (cz0 + 0.16, cz1 - 0.16):
                        hex_bolt(c, STL, polz(1.39, am + s_ * wa * 0.6, zz), polz(1, am, 0), 0.06, 0.03)
                grease(c, polz(1.32, a0 + 0.02, (cz0 + cz1) / 2), polz(1, a0, 0))
    # --- pad = engine_hot_emitter_annulus: 4 ceramic quadrants, rust wear rim, black clamp cross with a square hub
    p = P["engine_hot_emitter_annulus"] = Part("engine_hot_emitter_annulus")
    pz0, pr, L = cz1, d["pr"], d["L"]
    lathe(p, BLK, [(0, pz0), (1.33, pz0), (1.33, pz0 + 0.06), (0, pz0 + 0.06)], seg)
    if lod < 2:
        for k in range(4):
            a0, a1 = TAU * k / 4 + 0.03, TAU * (k + 1) / 4 - 0.03
            lathe(p, "eq_pad_ceramic", crect(0.30, 1.33, pz0 + 0.06, L - 0.05), max(2, seg // 4), I3, (0, 0, 0), a0, a1, cap=True)
        lathe(p, RST, crect(1.33, pr, pz0, L - 0.02), seg)                                          # wear rim
        for k in range(2):                                                                         # clamp cross
            Rz = rot_z(k * math.pi / 2)
            box(p, BLK, (0, 0, L - 0.06), (2.70, 0.18, 0.07), Rz)
        box(p, BLK, (0, 0, L - 0.065), (0.62, 0.62, 0.08))
        if lod == 0:
            for k in range(4):
                a = k * math.pi / 2
                for rr in (0.55, 0.90, 1.22):
                    hex_bolt(p, STL, polz(rr, a, L - 0.025), (0, 0, 1), 0.07, 0.025, washer=False)
            for i in range(8):                                                                     # rim clamp dogs
                a = TAU * (i + 0.5) / 8
                Rz = rot_z(a)
                box(p, BLK, apply(Rz, (1.37, 0, 0))[:2] + (L - 0.03,), (0.12, 0.20, 0.06), Rz)
    else:
        lathe(p, "eq_pad_ceramic", [(0, pz0 + 0.06), (pr, pz0 + 0.06), (pr, L), (0, L)], seg)
    # --- field control: ruggedised box on top, bump bars, bolted cover, hoses to the coil ring
    fc = P["kt1_field_ctl"] = Part("kt1_field_ctl")
    box(fc, YEL, (0, hw + 0.13, 1.35), (1.10, 0.26, 1.20), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        for sx in (-1, 1):
            box(fc, BLK, (sx * 0.58, hw + 0.17, 1.35), (0.08, 0.22, 1.28))                       # bump bars
        at, n = local_frame("+y", (0, hw + 0.26, 1.35), 0.90, 1.0)
        plate_on(fc, BLK, at, 0.05, 0.95, 0.05, 0.95, 0.0, 0.02)
        if lod == 0:
            for u in (0.10, 0.5, 0.90):
                for v in (0.10, 0.90):
                    hex_bolt(fc, STL, at(u, v, 0.02), n, 0.06, 0.03)
            box(fc, "eq_status_amber", at(0.80, 0.5, 0.025), (0.10, 0.01, 0.06))
        for sx in (-1, 1):
            hose(fc, bezier((sx * 0.30, hw + 0.15, 1.95), (sx * 0.32, hw + 0.25, 2.35), (sx * 0.40, 1.30, 2.55), (sx * 0.40, 1.22, cz0 + 0.02), 8), 0.04)
    # --- hx: backup heat exchanger on -X: finned black block, yellow frame, coolant hoses towards the pad
    hx = P["kt1_hx"] = Part("kt1_hx")
    box(hx, BLK, (-hw - 0.08, 0, 1.55), (0.16, 1.20, 1.50))
    if lod < 2:
        nf = 12 if lod == 0 else 5
        for i in range(nf):
            zz = 0.85 + 1.40 * (i + 0.5) / nf
            box(hx, BLK, (-hw - 0.20, 0, zz), (0.10, 1.10, 0.045 if lod == 0 else 0.10))
        for yy in (-0.62, 0.62):
            box(hx, YEL, (-hw - 0.16, yy, 1.55), (0.30, 0.06, 1.56))
        for yy in (-0.35, 0.35):
            hose(hx, [(-hw - 0.22, yy, 2.31), (-hw - 0.24, yy, 2.45), (-1.33, yy, 2.62), (-1.37, yy, cz0 + 0.02), (-1.36, yy * 0.8, pz0 + 0.05)], 0.04)
    # --- sensors: field / temperature / thrust heads on the upper chamfers and +X, each in a guard cage
    sn = P["kt1_sensor"] = Part("kt1_sensor")
    a0 = hw - d["hc"]
    spots = [((a0 + hw) / 2 * 1.0, (a0 + hw) / 2, 2.40, (1, 1, 0)), (-(a0 + hw) / 2, (a0 + hw) / 2, 2.40, (-1, 1, 0)), (hw, -0.75, 0.60, (1, 0, 0))]
    for x, y, z, nn in spots:
        nn = norm(nn)
        base = (x, y, z)
        cyl(sn, BLK, base, tuple(base[k] + nn[k] * 0.05 for k in range(3)), 0.10, small)
        cyl(sn, STL, tuple(base[k] + nn[k] * 0.05 for k in range(3)), tuple(base[k] + nn[k] * 0.14 for k in range(3)), 0.05, small)
        if lod == 0:
            Rg = frame3((0, 0, 1), nn)
            for s_ in (-1, 1):
                q0 = tuple(base[k] + apply(Rg, (0, s_ * 0.13, 0))[k] for k in range(3))
                box(sn, YEL, tuple(q0[k] + nn[k] * 0.09 for k in range(3)), (0.30, 0.03, 0.18), Rg)
            box(sn, YEL, tuple(base[k] + nn[k] * 0.19 for k in range(3)), (0.30, 0.29, 0.03), Rg)
    if close():
        for x, y, z, nn in spots[:2]:
            sx = 1 if x > 0 else -1
            harness(sn, bezier((x - sx * 0.08, y + 0.08, z - 0.12), (x - sx * 0.10, y + 0.20, z - 0.4), (sx * 0.62, hw + 0.10, 2.10),
                               (sx * 0.45, hw + 0.10, 1.96), 10), 0.015, 2, (BLK, "eq_insulator"), tie_every=4)
        box(sn, STL, (hw + 0.005, 0.55, 1.35), (0.01, 0.20, 0.36))
    # --- skids: two skid rails under the body with sloped ends and rust wear strips
    sk = P["kt1_skids"] = Part("kt1_skids")
    for sx in (-1, 1):
        x0, x1 = sx * 0.58, sx * 0.82
        hexa(sk, BLK, [(x0, -d["pr"], 0.55), (x1, -d["pr"], 0.55), (x1, -d["pr"], 2.45), (x0, -d["pr"], 2.45),
                       (x0, -hw, 0.36), (x1, -hw, 0.36), (x1, -hw, 2.62), (x0, -hw, 2.62)])
        if lod == 0:
            box(sk, RST, ((x0 + x1) / 2, -d["pr"] + 0.012, 1.5), (0.20, 0.025, 1.70))
            for zz in (0.8, 1.5, 2.2):
                grease(sk, (x1, -1.28, zz), (sx, 0, 0))
    if lod < 2:
        box(sk, BLK, (0, -hw - 0.08, 0.80), (1.40, 0.16, 0.18))
        box(sk, BLK, (0, -hw - 0.08, 2.20), (1.40, 0.16, 0.18))
    return P


def kt1_hits():
    d = KT1
    hw = d["hw"] / 2
    return [hbox("kt1_flange", (0, 0, 0.16), (d["fl"], d["fl"], 0.30)),
            hbox("kt1_busbar", (0, -hw + 0.12, 0.52), (0.80, 0.30, 0.44)),
            hbox("kt1_busbar", (0, -0.45, 0.01), (0.95, 0.30, 0.02)),
            hbox("kt1_housing", (0, 0, (d["z_h0"] + d["z_h1"]) / 2), (d["hw"], d["hw"] - 2 * d["hc"], d["z_h1"] - d["z_h0"])),
            hbox("kt1_housing", (0, 0, (d["z_h0"] + d["z_h1"]) / 2), (d["hw"] - 2 * d["hc"], d["hw"], d["z_h1"] - d["z_h0"])),
            hbox("kt1_housing", (0, 0, (d["z_h0"] + d["z_h1"]) / 2), (1.45, 1.45, d["z_h1"] - d["z_h0"]), (0, 0, 45)),
            hcyl("kt1_coil", (0, 0, d["cz0"]), (0, 0, d["cz1"]), 1.39),
            hcyl("engine_hot_emitter_annulus", (0, 0, d["cz1"]), (0, 0, d["L"]), d["pr"]),
            hbox("kt1_field_ctl", (0, hw + 0.13, 1.35), (1.20, 0.26, 1.28)),
            hbox("kt1_hx", (-hw - 0.15, 0, 1.55), (0.30, 1.30, 1.56)),
            hbox("kt1_sensor", (0.80, 0.80, 2.40), (0.30, 0.30, 0.30), (0, 0, 45)),
            hbox("kt1_sensor", (-0.80, 0.80, 2.40), (0.30, 0.30, 0.30), (0, 0, -45)),
            hbox("kt1_sensor", (hw + 0.10, -0.75, 0.60), (0.20, 0.30, 0.30)),
            hbox("kt1_skids", (-0.70, -(d["pr"] + hw) / 2, 1.49), (0.24, d["pr"] - hw, 2.26)),
            hbox("kt1_skids", (0.70, -(d["pr"] + hw) / 2, 1.49), (0.24, d["pr"] - hw, 2.26))]


def ram(part, p0, p1, r, lod):
    """hydraulic ram: yellow barrel over 60 % of the stroke, steel rod, black gland"""
    m = tuple(p0[k] + (p1[k] - p0[k]) * 0.6 for k in range(3))
    sm = small_for(lod)
    cyl(part, YEL, p0, m, r, sm)
    cyl(part, STL, m, p1, r * 0.55, sm)
    if lod == 0:
        cyl(part, BLK, tuple(m[k] - (p1[k] - p0[k]) * 0.03 for k in range(3)), m, r * 1.15, sm)
        grease(part, p0, (0, 1, 0))


def beam(part, mat, p0, p1, w, h, up=(0, 1, 0)):
    """a box beam from p0 to p1 (section: h along `up`, w across)"""
    d = tuple(p1[k] - p0[k] for k in range(3))
    L = math.sqrt(sum(x * x for x in d))
    R = frame3(norm(up), d)                       # local x = up (orthogonalised), z = beam axis
    box(part, mat, tuple((p0[k] + p1[k]) / 2 for k in range(3)), (h, w, L), R)


# ---------------------------------------------------------------- MK-1 mining cutter (skin: 3.9 x 3.9 x 2.47)
# A road-header style drum head on a boom: the crusher box on the hull, the boom on a pivot with two rams, the cutter
# drum across the boom end (picks in helical rows), bearing caps at the drum ends. Boom shown raised (working).
MK1 = dict(hx=1.95, hz=1.235, top=3.9, dy=3.12, dz=0.42, dr=0.62, piv=(0.0, 1.62, -0.80))


def mk1_parts(lod):
    _L[0] = lod
    d = MK1
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    f = P["mk1_flange"] = Part("mk1_flange")
    skin_flange(f, lod, 1.60, 0.18, 0.0, -0.35)
    # --- crusher: heavy yellow box, hopper under the drum, discharge chute, drive motor, wear plates, lifting eyes
    cr = P["mk1_crusher"] = Part("mk1_crusher")
    box(cr, YEL, (0, 0.82, -0.35), (1.80, 1.28, 1.50), chamfer=0.04 if lod == 0 else 0.0)
    hop = [(-0.70, -0.20), (0.70, -0.20), (0.70, 0.40), (-0.70, 0.40)]
    top = [(-0.82, -0.28), (0.82, -0.28), (0.82, 0.92), (-0.82, 0.92)]
    loft(cr, YEL, hop, 1.46, top, 1.86)
    loft(cr, BLK, inset(top, 0.07), 1.857, inset(top, 0.07), 1.866)                    # hopper mouth
    box(cr, BLK, (0, 0.55, -1.17), (0.70, 0.55, 0.13))
    cyl(cr, YEL, (-0.90, 0.75, -0.65), (-1.32, 0.75, -0.65), 0.30, small)
    cyl(cr, BLK, (-1.32, 0.75, -0.65), (-1.40, 0.75, -0.65), 0.24, small)
    if lod < 2:
        for sx in (-1, 1):
            at, n = local_frame("+x" if sx > 0 else "-x", (sx * 0.90, 0.80, -0.35), 1.05, 1.30)
            wear(cr, at, n, 0.05, 0.95, 0.04, 0.96, 0.035, (2, 3))
        at, n = local_frame("+z", (0, 0.82, 0.40), 1.70, 1.10)
        wear(cr, at, n, 0.05, 0.95, 0.10, 0.90, 0.035, (3, 2))
        for sx in (-1, 1):
            lift_eye(cr, (sx * 0.72, 1.46, -1.0), (0, 1, 0), (1, 0, 0), 0.18)
        for i in range(4):
            grease(cr, (-1.40, 0.75 + 0.15 * math.cos(i * 1.57), -0.65 + 0.15 * math.sin(i * 1.57)), (-1, 0, 0))
    # --- boom: pivot brackets and pin, box-section arm to the head, two hydraulic rams, hoses
    bm = P["mk1_boom"] = Part("mk1_boom")
    pv, hd = d["piv"], (0.0, d["dy"], d["dz"])
    for sx in (-1, 1):
        box(bm, BLK, (sx * 0.40, pv[1] - 0.10, pv[2]), (0.10, 0.40, 0.50))
    cyl(bm, STL, (-0.48, pv[1], pv[2]), (0.48, pv[1], pv[2]), 0.12, small)
    up = (0, 0.6, -0.8)
    beam(bm, YEL, pv, hd, 0.56, 0.44, up)
    if lod < 2:
        off = (0, 0.6 * 0.235, -0.8 * 0.235)                                          # rust wear strip on the boom back
        beam(bm, RST, tuple(pv[k] + (hd[k] - pv[k]) * 0.15 + off[k] for k in range(3)),
             tuple(pv[k] + (hd[k] - pv[k]) * 0.75 + off[k] for k in range(3)), 0.40, 0.04, up)
    for sx in (-1, 1):
        a, b = (sx * 0.42, 1.48, 0.25), (sx * 0.42, 2.55, -0.05)
        ram(bm, a, b, 0.09, lod)
        box(bm, BLK, (sx * 0.42, 1.50, 0.25), (0.10, 0.14, 0.22))
    if lod == 0:
        for sx in (-1, 1):
            hose(bm, bezier((sx * 0.20, 1.50, -0.60), (sx * 0.25, 2.0, -0.70), (sx * 0.34, 2.70, -0.10), (sx * 0.30, 2.90, 0.10), 10), 0.035)
    # --- head: gearbox at the boom end, two drum halves, picks in helical rows, bearing caps
    hdp = P["mk1_head"] = Part("mk1_head")
    y0, z0, dr = d["dy"], d["dz"], d["dr"]
    box(hdp, BLK, (0, y0, z0), (0.66, 0.80, 0.80), chamfer=0.04 if lod == 0 else 0.0)
    for s in (-1, 1):
        cyl(hdp, YEL, (s * 0.34, y0, z0), (s * 1.48, y0, z0), dr, seg)
        cyl(hdp, BLK, (s * 1.48, y0, z0), (s * 1.62, y0, z0), dr * 0.70, small)
        lathe(hdp, YEL, [(0, 0), (0.40, 0), (0.40, 0.25), (0.30, 0.33), (0, 0.33)], 8, R_X if s > 0 else rot_y(-math.pi / 2),
              (s * 1.62, y0, z0), smooth=False)
        if lod < 2:
            for xx in (0.52, 0.95, 1.36):
                lathe(hdp, RST, crect(dr - 0.01, dr + 0.035, -0.035, 0.035), seg, R_X, (s * xx, y0, z0))
        if lod == 0:
            for i in range(8):
                a = TAU * i / 8
                hex_bolt(hdp, STL, (s * 1.95, y0 + 0.24 * math.cos(a), z0 + 0.24 * math.sin(a)), (s, 0, 0), 0.07, 0.04)
    if lod < 2:
        nr, na = (5, 10) if lod == 0 else (3, 6)
        for s in (-1, 1):
            for i in range(nr):
                xx = s * (0.45 + 0.98 * (i + 0.5) / nr)
                for j in range(na):
                    th = TAU * j / na + i * 0.45 * s
                    rd = (0, math.cos(th), math.sin(th))
                    base = (xx, y0 + dr * rd[1], z0 + dr * rd[2])
                    box(hdp, BLK, tuple(base[k] + rd[k] * 0.04 for k in range(3)), (0.07, 0.09, 0.10), frame3((1, 0, 0), rd))
                    tip = norm((0, rd[1] - 0.35 * rd[2], rd[2] + 0.35 * rd[1]))
                    lathe(hdp, STL, [(0, 0), (0.032, 0), (0.026, 0.05), (0, 0.10)], 6 if lod == 0 else 4, axis_to(tip),
                          tuple(base[k] + rd[k] * 0.08 for k in range(3)), smooth=False)
    return P


def mk1_hits():
    d = MK1
    pv = d["piv"]
    L = math.dist(pv, (0, d["dy"], d["dz"]))
    ang = math.degrees(math.atan2(d["dz"] - pv[2], d["dy"] - pv[1]))
    return [hbox("mk1_flange", (0, 0.09, -0.35), (1.60, 0.18, 1.60)),
            hbox("mk1_crusher", (0, 0.82, -0.35), (1.80, 1.28, 1.50)),
            hbox("mk1_crusher", (0, 1.66, 0.32), (1.64, 0.40, 1.20)),
            hbox("mk1_crusher", (0, 0.55, -1.17), (0.70, 0.55, 0.13)),
            hcyl("mk1_crusher", (-0.90, 0.75, -0.65), (-1.40, 0.75, -0.65), 0.30),
            hbox("mk1_boom", (0, (pv[1] + d["dy"]) / 2, (pv[2] + d["dz"]) / 2), (0.56, L, 0.44), (ang, 0, 0)),
            hcyl("mk1_boom", (-0.48, pv[1], pv[2]), (0.48, pv[1], pv[2]), 0.14),
            hbox("mk1_boom", (0.42, 2.0, 0.10), (0.20, 1.15, 0.34), (-16, 0, 0)),
            hbox("mk1_boom", (-0.42, 2.0, 0.10), (0.20, 1.15, 0.34), (-16, 0, 0)),
            hbox("mk1_head", (0, d["dy"], d["dz"]), (0.66, 0.80, 0.80)),
            hcyl("mk1_head", (-d["hx"], d["dy"], d["dz"]), (-0.34, d["dy"], d["dz"]), d["dr"] + 0.14),
            hcyl("mk1_head", (0.34, d["dy"], d["dz"]), (d["hx"], d["dy"], d["dz"]), d["dr"] + 0.14)]


# ---------------------------------------------------------------- DR-5 debris radar (skin: 2.778 x 1.111 x 2.778)
# A tilted flat array on a yoke inside a square bumper frame with four guarded corner posts - debris knocks the cage,
# not the panel. The receiver box sits on the plinth behind the yoke.
DR5 = dict(h=1.389, top=1.111, th=math.radians(40.0), pc=(0.0, 0.80, 0.08))


def dr5_parts(lod):
    _L[0] = lod
    d = DR5
    small = small_for(lod)
    h = d["h"]
    P = {}
    f = P["dr5_flange"] = Part("dr5_flange")
    skin_flange(f, lod, 1.20, 0.14)
    b = P["dr5_base"] = Part("dr5_base")
    box(b, YEL, (0, 0.28, 0), (1.50, 0.28, 1.50), chamfer=0.03 if lod == 0 else 0.0)
    for k in range(4):                                                      # bumper frame and struts
        Rz = rot_y(k * math.pi / 2)
        box(b, BLK, apply(Rz, (0, 0.22, h - 0.12)), (2 * h - 0.10, 0.20, 0.14), Rz)
        beam(b, BLK, apply(Rz, (0.60, 0.20, 0.60)), apply(Rz, (h - 0.10, 0.20, h - 0.10)), 0.10, 0.10)
        if lod < 2:
            at, n = local_frame(mat_mul(Rz, FACE["+z"]), apply(Rz, (0, 0.22, h - 0.05)), 2 * h - 0.4, 0.16)
            wear(b, at, n, 0.05, 0.95, 0.0, 1.0, 0.02, (4, 1), 0.06)
    for sx in (-1, 1):                                                      # guard posts, guard plates, top rails
        for sz in (-1, 1):
            box(b, BLK, (sx * 1.27, 0.71, sz * 0.92), (0.10, 0.78, 0.10))
            if lod < 2:
                box(b, YEL, (sx * 1.33, 0.70, sz * 0.92), (0.03, 0.50, 0.24))
            lift_eye(b, (sx * 0.62, 0.42, sz * 0.62), (0, 1, 0), (1, 0, sx * sz), 0.14)
        box(b, BLK, (sx * 1.27, d["top"] - 0.05, 0), (0.10, 0.10, 1.94))
    # rx: receiver box on the plinth (-Z), black cover, hose to the array
    rx = P["dr5_rx"] = Part("dr5_rx")
    box(rx, YEL, (0, 0.55, -0.50), (0.80, 0.26, 0.36), chamfer=0.02 if lod == 0 else 0.0)
    if lod < 2:
        box(rx, BLK, (0, 0.69, -0.50), (0.70, 0.02, 0.30))
        if lod == 0:
            for x in (-0.30, 0.30):
                hex_bolt(rx, STL, (x, 0.70, -0.62), (0, 1, 0), 0.06, 0.02)
            hose(rx, bezier((0.25, 0.60, -0.32), (0.30, 0.62, -0.10), (0.70, 0.62, -0.05), (0.98, 0.70, 0.0), 8), 0.03)
    # array: turntable, yoke, tilted flat panel with a rib grid and a rust back plate
    ar = P["dr5_array"] = Part("dr5_array")
    lathe(ar, BLK, [(0, 0.42), (0.36, 0.42), (0.36, 0.52), (0, 0.52)], 8, R_Y, (0, 0, 0), smooth=False)
    box(ar, YEL, (0, 0.57, 0.05), (2.30, 0.10, 0.30))
    for sx in (-1, 1):
        box(ar, YEL, (sx * 1.17, 0.70, 0.05), (0.10, 0.36, 0.26))
        cyl(ar, STL, (sx * 1.12, d["pc"][1], d["pc"][2]), (sx * 1.22, d["pc"][1], d["pc"][2]), 0.07, small)
    R = rot_x(d["th"])
    box(ar, BLK, d["pc"], (2.20, 0.10, 0.80), R)
    if lod < 2:
        at, n = local_frame(R, tuple(d["pc"][k] + apply(R, (0, 0.05, 0))[k] for k in range(3)), 2.20, 0.80)
        nu = 8 if lod == 0 else 4
        for i in range(nu + 1):
            u = 0.02 + 0.96 * i / nu
            plate_on(ar, STL, at, u - 0.006, u + 0.006, 0.03, 0.97, 0.0, 0.015)
        for v in (0.03, 0.5, 0.97):
            plate_on(ar, STL, at, 0.02, 0.98, v - 0.008, v + 0.008, 0.0, 0.015)
        plate_on(ar, YEL, at, 0.0, 1.0, 0.0, 0.03, 0.0, 0.03)
        plate_on(ar, YEL, at, 0.0, 1.0, 0.97, 1.0, 0.0, 0.03)
        atb, nb = local_frame(mat_mul(R, rot_z(math.pi)), tuple(d["pc"][k] - apply(R, (0, 0.05, 0))[k] for k in range(3)), 2.20, 0.80)
        wear(ar, atb, nb, 0.15, 0.85, 0.15, 0.85, 0.02, (3, 2), 0.06)
    return P


def dr5_hits():
    d = DR5
    h = d["h"]
    H = [hbox("dr5_flange", (0, 0.07, 0), (1.20, 0.14, 1.20)),
         hbox("dr5_base", (0, 0.28, 0), (1.50, 0.28, 1.50))]
    for k in range(4):
        H.append(hbox("dr5_base", apply(rot_y(k * math.pi / 2), (0, 0.22, h - 0.10)), (2 * h - 0.10, 0.20, 0.18), (0, 90.0 * k, 0)))
    for sx in (-1, 1):
        H.append(hbox("dr5_base", (sx * 1.27, 0.66, 0), (0.12, 0.90, 1.96)))
    H += [hbox("dr5_rx", (0, 0.55, -0.50), (0.80, 0.30, 0.36)),
          hcyl("dr5_array", (0, 0.42, 0), (0, 0.52, 0), 0.36),
          hbox("dr5_array", (0, 0.64, 0.05), (2.44, 0.24, 0.30)),
          hbox("dr5_array", d["pc"], (2.20, 0.14, 0.80), (math.degrees(d["th"]), 0, 0))]
    return H


# ---------------------------------------------------------------- TR-H heavy grapple (axial: 5.623 x 5.623 x 2.109)
# Square box-beam frame on the hull flange, four hydraulic claws at the corners reaching forward (+Z) and inward,
# a central winch with a fairlead and a towing hook between them.
TRH = dict(h=5.623 / 2, L=2.109, c=2.53, bw=0.50, z0=0.25, z1=0.82)


def trh_parts(lod):
    _L[0] = lod
    d = TRH
    seg, small = seg_for(lod), small_for(lod)
    c, bw, z0, z1 = d["c"], d["bw"], d["z0"], d["z1"]
    zm = (z0 + z1) / 2
    P = {}
    f = P["trh_flange"] = Part("trh_flange")
    merge(f, kflange(lod, 1.80, 0.25))
    fr = P["trh_frame"] = Part("trh_frame")
    for s in (-1, 1):
        box(fr, YEL, (0, s * c, zm), (2 * c - bw, bw, z1 - z0))
        box(fr, YEL, (s * c, 0, zm), (bw, 2 * c - bw, z1 - z0))
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(fr, BLK, (sx * c, sy * c, zm), (0.62, 0.62, z1 - z0 + 0.06), chamfer=0.03 if lod == 0 else 0.0)
            beam(fr, YEL, (sx * 0.80, sy * 0.80, zm), (sx * (c - 0.30), sy * (c - 0.30), zm), 0.34, 0.40, (0, 0, 1))
            lift_eye(fr, (sx * (c + 0.10), sy * (c + 0.10), z1 + 0.03), (0, 0, 1), (sx, -sy, 0), 0.20)
    if lod < 2:
        for s in (-1, 1):
            for ax in (0, 1):
                ctr = (0, s * c, z1) if ax == 0 else (s * c, 0, z1)
                Rf = FACE["+z"] if ax == 0 else mat_mul(rot_z(math.pi / 2), FACE["+z"])
                at, n = local_frame(Rf, ctr, 2 * c - 0.9, bw - 0.06)
                for u0 in (0.05, 0.53):
                    wear(fr, at, n, u0, u0 + 0.42, 0.06, 0.94, 0.03, (3, 1), 0.08)
    # winch: drum across X on two cheek pedestals, cable wraps, hydraulic motor, fairlead and hook forward
    w = P["trh_winch"] = Part("trh_winch")
    wy, wz = 0.0, 1.20
    cyl(w, BLK, (-0.78, wy, wz), (0.78, wy, wz), 0.42, seg)
    for sx in (-1, 1):
        box(w, YEL, (sx * 0.86, wy, (0.25 + wz + 0.55) / 2), (0.10, 1.10, wz + 0.55 - 0.25))
    cyl(w, YEL, (0.91, wy, wz), (1.28, wy, wz), 0.24, small)
    cyl(w, BLK, (1.28, wy, wz), (1.36, wy, wz), 0.18, small)
    box(w, BLK, (0, -0.30, 1.80), (0.70, 0.36, 0.36))
    hexa(w, RST, [(-0.08, -0.48, 1.98), (0.08, -0.48, 1.98), (0.08, -0.12, 1.98), (-0.08, -0.12, 1.98),
                  (-0.06, -0.40, d["L"]), (0.06, -0.40, d["L"]), (0.06, -0.20, d["L"]), (-0.06, -0.20, d["L"])])
    if lod < 2:
        for i in range(9 if lod == 0 else 4):
            xx = -0.70 + 1.40 * i / (8 if lod == 0 else 3)
            lathe(w, STL, crect(0.42, 0.46, -0.05, 0.05), seg, R_X, (xx, wy, wz))
        for zz in (1.70, 1.90):
            cyl(w, STL, (-0.30, -0.30, zz), (0.30, -0.30, zz), 0.06, small)
        cyl(w, STL, (0, -0.44, wz), (0, -0.30, 1.80), 0.045, small)
        hose(w, bezier((1.10, 0.20, wz + 0.15), (1.10, 0.6, wz + 0.2), (1.2, 1.2, z1 + 0.1), (1.30, 1.40, z1 + 0.02), 8), 0.04)
    # clamps: four hydraulic claws at the corners
    cl = P["trh_clamp"] = Part("trh_clamp")
    for sx in (-1, 1):
        for sy in (-1, 1):
            u = (-sx / math.sqrt(2), -sy / math.sqrt(2))
            base = (sx * (c - 0.15), sy * (c - 0.15), z1 + 0.12)
            tip = (base[0] + u[0] * 0.50, base[1] + u[1] * 0.50, 1.72)
            jaw = (base[0] + u[0] * 0.95, base[1] + u[1] * 0.95, 1.96)
            box(cl, BLK, (base[0], base[1], z1 + 0.06), (0.46, 0.46, 0.12))
            cyl(cl, STL, (base[0] - u[1] * 0.25, base[1] + u[0] * 0.25, base[2]), (base[0] + u[1] * 0.25, base[1] - u[0] * 0.25, base[2]), 0.09, small)
            beam(cl, YEL, base, tip, 0.34, 0.30, (u[0], u[1], 0))
            beam(cl, BLK, tip, jaw, 0.30, 0.24, (u[0], u[1], 0))
            if lod < 2:
                for t in (0.25, 0.55, 0.85):                                   # rust teeth on the jaw's inner face
                    p = tuple(tip[k] + (jaw[k] - tip[k]) * t for k in range(3))
                    box(cl, RST, (p[0] + u[0] * 0.14, p[1] + u[1] * 0.14, p[2]), (0.10, 0.10, 0.08), frame3((u[1], -u[0], 0), (u[0], u[1], 0)))
                r0 = (sx * (c - 1.05), sy * (c - 1.05), z1)
                ram(cl, r0, tuple(base[k] + (tip[k] - base[k]) * 0.6 for k in range(3)), 0.08, lod)
    return P


def trh_hits():
    d = TRH
    c, bw, z0, z1 = d["c"], d["bw"], d["z0"], d["z1"]
    zm = (z0 + z1) / 2
    H = [hbox("trh_flange", (0, 0, 0.125), (1.80, 1.80, 0.25))]
    for s in (-1, 1):
        H.append(hbox("trh_frame", (0, s * c, zm), (2 * c + bw, bw, z1 - z0)))
        H.append(hbox("trh_frame", (s * c, 0, zm), (bw, 2 * c - bw, z1 - z0)))
    for a in (45, -45):
        H.append(hbox("trh_frame", (0, 0, zm), (2 * math.sqrt(2) * (c - 0.30), 0.40, 0.34), (0, 0, a)))
    H += [hcyl("trh_winch", (-0.91, 0, 1.20), (1.36, 0, 1.20), 0.46),
          hbox("trh_winch", (0, -0.30, 1.85), (0.70, 0.40, 0.50))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            H.append(hbox("trh_clamp", (sx * (c - 0.45), sy * (c - 0.45), (z1 + d["L"]) / 2), (0.80, 0.80, d["L"] - z1)))
    return H


# ---------------------------------------------------------------- TR-20K raft tow frame (axial: 5.109 x 5.109 x 1.916)
# The light sister of the TR-H: an open square truss with X-bracing and a pyramid of struts to a central tow coupling,
# raft lashing clamps on the corner nodes, two cable winches inboard on the X sides.
T20 = dict(h=5.109 / 2, L=1.916, c=2.34, z0=0.24, z1=0.52, apex=1.40)


def tr20k_parts(lod):
    _L[0] = lod
    d = T20
    small = small_for(lod)
    c, z0, z1, za = d["c"], d["z0"], d["z1"], d["apex"]
    zm = (z0 + z1) / 2
    P = {}
    f = P["tr20k_flange"] = Part("tr20k_flange")
    merge(f, kflange(lod, 1.40, 0.24))
    fr = P["tr20k_frame"] = Part("tr20k_frame")
    for s in (-1, 1):
        box(fr, YEL, (0, s * c, zm), (2 * c, 0.26, z1 - z0))
        box(fr, YEL, (s * c, 0, zm), (0.26, 2 * c, z1 - z0))
    for sx in (-1, 1):
        for sy in (-1, 1):
            box(fr, BLK, (sx * c, sy * c, zm), (0.42, 0.42, z1 - z0 + 0.06))
            beam(fr, YEL, (sx * 0.55, sy * 0.55, zm), (sx * (c - 0.2), sy * (c - 0.2), zm), 0.20, 0.22, (0, 0, 1))
            beam(fr, YEL, (sx * (c - 0.15), sy * (c - 0.15), z1), (sx * 0.28, sy * 0.28, za), 0.18, 0.18, (sx, sy, 0))
            lift_eye(fr, (sx * (c + 0.12), sy * (c - 0.12), z1 + 0.03), (0, 0, 1), (1, 0, 0), 0.16)
    box(fr, BLK, (0, 0, za), (0.70, 0.70, 0.36), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        for s in (-1, 1):                                                     # mid-side gussets
            box(fr, BLK, (0, s * c, z1 + 0.02), (0.50, 0.30, 0.04))
            box(fr, BLK, (s * c, 0, z1 + 0.02), (0.30, 0.50, 0.04))
    # clamp: central tow coupling (hook jaw on the apex hub) + raft lashing clamps on the four corner nodes
    cl = P["tr20k_clamp"] = Part("tr20k_clamp")
    box(cl, BLK, (0, 0, za + 0.24), (0.46, 0.46, 0.12))
    hexa(cl, STL, [(-0.10, -0.20, za + 0.30), (0.10, -0.20, za + 0.30), (0.10, 0.20, za + 0.30), (-0.10, 0.20, za + 0.30),
                   (-0.08, -0.22, d["L"]), (0.08, -0.22, d["L"]), (0.08, -0.04, d["L"]), (-0.08, -0.04, d["L"])])
    if lod < 2:
        hexa(cl, RST, [(-0.09, 0.06, za + 0.30), (0.09, 0.06, za + 0.30), (0.09, 0.22, za + 0.30), (-0.09, 0.22, za + 0.30),
                       (-0.07, 0.10, d["L"] - 0.06), (0.07, 0.10, d["L"] - 0.06), (0.07, 0.20, d["L"] - 0.06), (-0.07, 0.20, d["L"] - 0.06)])
        cyl(cl, STL, (-0.26, 0, za + 0.40), (0.26, 0, za + 0.40), 0.05, small)
    for sx in (-1, 1):
        for sy in (-1, 1):
            p = (sx * c, sy * c)
            box(cl, BLK, (p[0], p[1], z1 + 0.15), (0.36, 0.36, 0.24))
            if lod < 2:
                box(cl, RST, (p[0] - sx * 0.10, p[1], z1 + 0.35), (0.10, 0.30, 0.18))
                box(cl, RST, (p[0], p[1] - sy * 0.10, z1 + 0.35), (0.30, 0.10, 0.18))
    # winch: two cable winches inboard on the X sides, cables to the apex fairlead
    w = P["tr20k_winch"] = Part("tr20k_winch")
    for sx in (-1, 1):
        xw = sx * 1.96
        cyl(w, BLK, (xw, -0.50, z1 + 0.30), (xw, 0.50, z1 + 0.30), 0.26, small * 2 if lod == 0 else small)
        for sy in (-1, 1):
            box(w, YEL, (xw, sy * 0.56, z1 + 0.14), (0.66, 0.08, 0.60))
        box(w, BLK, (xw, 0, z1 - 0.02), (0.40, 1.20, 0.08))
        cyl(w, YEL, (xw, 0.60, z1 + 0.30), (xw, 0.90, z1 + 0.30), 0.18, small)
        if lod < 2:
            for i in range(6 if lod == 0 else 3):
                yy = -0.42 + 0.84 * i / (5 if lod == 0 else 2)
                lathe(w, STL, crect(0.26, 0.29, -0.03, 0.03), small * 2 if lod == 0 else small, R_Y, (xw, yy, z1 + 0.30))
            cyl(w, STL, (xw - sx * 0.27, 0, z1 + 0.30), (sx * 0.36, 0, za), 0.035, 6)
            hose(w, bezier((xw, 0.90, z1 + 0.30), (xw, 1.2, z1 + 0.3), (xw, 1.6, z1), (xw + sx * 0.1, 2.25, z1 - 0.02), 8), 0.035)
    return P


def tr20k_hits():
    d = T20
    c, z0, z1, za = d["c"], d["z0"], d["z1"], d["apex"]
    zm = (z0 + z1) / 2
    H = [hbox("tr20k_flange", (0, 0, 0.12), (1.40, 1.40, 0.24))]
    for s in (-1, 1):
        H.append(hbox("tr20k_frame", (0, s * c, zm), (2 * c + 0.42, 0.42, z1 - z0)))
        H.append(hbox("tr20k_frame", (s * c, 0, zm), (0.42, 2 * c, z1 - z0)))
    for a in (45, -45):
        H.append(hbox("tr20k_frame", (0, 0, zm), (2 * math.sqrt(2) * c, 0.22, 0.22), (0, 0, a)))
    H.append(hbox("tr20k_frame", (0, 0, za), (0.70, 0.70, 0.36)))
    for sx in (-1, 1):
        for sy in (-1, 1):
            H.append(hbox("tr20k_clamp", (sx * c, sy * c, z1 + 0.22), (0.40, 0.40, 0.38)))
    H.append(hbox("tr20k_clamp", (0, 0, (za + 0.18 + d["L"]) / 2), (0.46, 0.46, d["L"] - za - 0.18)))
    for sx in (-1, 1):
        H.append(hcyl("tr20k_winch", (sx * 1.96, -0.60, z1 + 0.30), (sx * 1.96, 0.90, z1 + 0.30), 0.30))
    return H


# ---------------------------------------------------------------- MD-40 impactor mass driver (skin: 4.894 x 2.936 x 7.83)
# Civil dual use: rock deflection / breaking. A square-section coil tube (yellow coil segments between black spacer
# frames, power ducts along the sides) on a skid cradle, fed from a big rock magazine with a hopper at the breech (-Z).
MD = dict(hx=4.894 / 2, top=2.936, hz=7.83 / 2, cy=1.75, cs=1.70, t0=-1.70, t1=3.70)


def md_sec(s, c, cy):
    return [(x, y + cy) for x, y in cham(s, s, c)]


def md40_parts(lod):
    _L[0] = lod
    d = MD
    cy, cs, hz = d["cy"], d["cs"], d["hz"]
    P = {}
    f = P["md40_flange"] = Part("md40_flange")
    for zz in (-2.6, 1.6):
        skin_flange(f, lod, 1.40, 0.14, 0.0, zz)
    # cradle: two skids, cross beams, saddles under the tube, lifting eyes
    cr = P["md40_cradle"] = Part("md40_cradle")
    for sx in (-1, 1):
        x0, x1 = sx * 1.32, sx * 1.62
        hexa(cr, BLK, [(x0, 0.10, -3.55), (x1, 0.10, -3.55), (x1, 0.10, 3.40), (x0, 0.10, 3.40),
                       (x0, 0.55, -3.85), (x1, 0.55, -3.85), (x1, 0.55, 3.62), (x0, 0.55, 3.62)])
        lift_eye(cr, (sx * 1.47, 0.55, 3.40), (0, 1, 0), (1, 0, 0), 0.22)
        if lod == 0:
            box(cr, RST, ((x0 + x1) / 2, 0.11, 0.0), (0.26, 0.02, 6.2))
    for zz in (-1.3, 0.3, 1.6, 3.2):
        box(cr, YEL, (0, 0.40, zz), (2.94, 0.30, 0.36))
        loft(cr, YEL, [(-0.80, zz - 0.18), (0.80, zz - 0.18), (0.80, zz + 0.18), (-0.80, zz + 0.18)], 0.55,
             [(-0.55, zz - 0.18), (0.55, zz - 0.18), (0.55, zz + 0.18), (-0.55, zz + 0.18)], cy - cs / 2 + 0.05)
    # coil: segments between spacer frames, muzzle block with rust wear plates, side power ducts, lifting eyes
    co = P["md40_coil"] = Part("md40_coil")
    ns = 10 if lod < 2 else 5
    t0, t1 = d["t0"], d["t1"]
    pitch = (t1 - t0) / ns
    for i in range(ns):
        za, zb = t0 + i * pitch, t0 + (i + 1) * pitch
        zloft(co, YEL, md_sec(cs, 0.30, cy), za + 0.04, zb - 0.04)
        if i < ns - 1:
            zloft(co, BLK, md_sec(cs + 0.12, 0.33, cy), zb - 0.05, zb + 0.05)
            if lod == 0:
                for sx in (-1, 1):
                    for yy in (cy - 0.45, cy + 0.45):
                        hex_bolt(co, STL, (sx * (cs / 2 + 0.06), yy, zb), (sx, 0, 0), 0.08, 0.04, washer=True)
            if i % 3 == 1:
                lift_eye(co, (0, cy + (cs + 0.12) / 2, zb), (0, 1, 0), (0, 0, 1), 0.22)
    zloft(co, BLK, md_sec(cs + 0.30, 0.38, cy), t1, hz - 0.065)
    if lod < 2:
        at, n = local_frame("+z", (0, cy, hz - 0.065), cs + 0.30, cs + 0.30)
        for u0, u1, v0, v1 in ((0.05, 0.95, 0.05, 0.25), (0.05, 0.95, 0.75, 0.95), (0.05, 0.25, 0.25, 0.75), (0.75, 0.95, 0.25, 0.75)):
            wear(co, at, n, u0, u1, v0, v1, 0.02, (2, 1), 0.08)
    for sx in (-1, 1):
        box(co, BLK, (sx * (cs / 2 + 0.11), cy, (t0 + t1) / 2), (0.20, 0.50, t1 - t0 - 0.3))
        if lod == 0:
            for i in range(ns):
                zc = t0 + (i + 0.5) * pitch
                hose(co, [(sx * (cs / 2 + 0.21), cy + 0.12, zc - 0.10), (sx * (cs / 2 + 0.26), cy + 0.30, zc), (sx * (cs / 2 + 0.03), cy + 0.62, zc + 0.08)], 0.03)
    # feed: rock magazine at the breech, hopper with a black mouth, side elevator, wear plates
    fd = P["md40_feed"] = Part("md40_feed")
    zf0, zf1 = -hz + 0.085, -1.72
    box(fd, YEL, (0, 1.62, (zf0 + zf1) / 2), (4.40, 2.14, zf1 - zf0), chamfer=0.05 if lod == 0 else 0.0)
    hp = [(-1.90, -3.70), (1.90, -3.70), (1.90, -2.0), (-1.90, -2.0)]
    hpt = [(-2.10, -3.82), (2.10, -3.82), (2.10, -1.85), (-2.10, -1.85)]
    loft(fd, YEL, hp, 2.69, hpt, d["top"] - 0.01)
    loft(fd, BLK, inset(hpt, 0.08), d["top"] - 0.012, inset(hpt, 0.08), d["top"])
    box(fd, BLK, (2.32, 1.55, -2.85), (0.24, 1.90, 1.60))
    if lod < 2:
        at, n = local_frame("-x", (-2.20, 1.62, (zf0 + zf1) / 2), 1.90, zf1 - zf0 - 0.2)
        wear(fd, at, n, 0.05, 0.95, 0.05, 0.48, 0.04, (2, 2))
        wear(fd, at, n, 0.05, 0.95, 0.52, 0.95, 0.04, (2, 2))
        at, n = local_frame("-z", (0, 1.62, zf0), 4.2, 1.9)
        wear(fd, at, n, 0.05, 0.48, 0.10, 0.90, 0.04, (2, 2))
        wear(fd, at, n, 0.52, 0.95, 0.10, 0.90, 0.04, (2, 2))
        for i in range(6 if lod == 0 else 3):                                     # elevator flights
            box(fd, YEL, (2.42, 0.75 + 1.6 * i / (5 if lod == 0 else 2), -2.85), (0.05, 0.08, 1.50))
        for zz in (-3.55, -2.05):
            lift_eye(fd, (-2.20, 2.40, zz), (-1, 0, 0), (0, 0, 1), 0.20)
    return P


def md40_hits():
    d = MD
    cy, cs, hz = d["cy"], d["cs"], d["hz"]
    H = [hbox("md40_flange", (0, 0.07, zz), (1.40, 0.14, 1.40)) for zz in (-2.6, 1.6)]
    for sx in (-1, 1):
        H.append(hbox("md40_cradle", (sx * 1.47, 0.325, -0.11), (0.30, 0.45, 7.47)))
    for zz in (-1.3, 0.3, 1.6, 3.2):
        H.append(hbox("md40_cradle", (0, (0.25 + cy - cs / 2) / 2, zz), (2.94, cy - cs / 2 - 0.25, 0.36)))
    H += [hbox("md40_coil", (0, cy, (d["t0"] + d["t1"]) / 2), (cs + 0.42, cs + 0.12, d["t1"] - d["t0"])),
          hbox("md40_coil", (0, cy, (d["t1"] + hz) / 2), (cs + 0.30, cs + 0.30, hz - d["t1"])),
          hbox("md40_feed", (0, 1.62, (-hz - 1.72) / 2), (4.40, 2.14, hz - 1.72)),
          hbox("md40_feed", (0, (2.69 + d["top"]) / 2, -2.84), (4.20, d["top"] - 2.69, 1.97)),
          hbox("md40_feed", (2.32, 1.55, -2.85), (0.26, 1.90, 1.60))]
    return H


ASSETS = {"kern_kt1_drive": kt1_parts, "kern_mk1_cutter": mk1_parts, "kern_dr5_radar": dr5_parts,
          "kern_trh_tow": trh_parts, "kern_tr20k_tow": tr20k_parts, "kern_md40_impactor": md40_parts}
HITS = {"kern_kt1_drive": kt1_hits, "kern_mk1_cutter": mk1_hits, "kern_dr5_radar": dr5_hits,
        "kern_trh_tow": trh_hits, "kern_tr20k_tow": tr20k_hits, "kern_md40_impactor": md40_hits}
MODEL_ASSETS = {"kern.drive.kt1": "kern_kt1_drive", "kern.mine.cutter": "kern_mk1_cutter", "kern.radar.deb5": "kern_dr5_radar",
                "kern.tow.heavy": "kern_trh_tow", "kern.tow.raft20k": "kern_tr20k_tow", "kern.md.impactor": "kern_md40_impactor"}
