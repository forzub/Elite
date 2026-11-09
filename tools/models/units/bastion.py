"""Arsenal «Bastion» (Wolf 359 Citadel, Nordic PMC oligarchy): armour, comm, combat computer, interceptor hangar,
B-12 jump core, S-50 radar, BP-10 coilgun, BT-4 point defence, VPU-16 missile cells.

School (DESIGN_SCHOOLS.md): paranoid, armoured, secret - wedge facets and sloped armour (flat-shaded hexahedra and
6-sided frusta, never smooth drums), recessed hatches with tamper locks, no exposed cables (armoured conduits only),
armour grey / dark / hazard yellow. Interface: the Bastion hexagonal flange with 12 interlocking teeth on its mating
face and a keyed centre boss (deliberately incompatible with every round / square standard). Hull-skin units mount at
y = 0 (tooth tips) with +Y out; weapons rest with the barrel / emitter along +Z.
The B-12 core is Bastion's own design outside the Cognium licence system since 2855: a faceted armoured hex vessel with
six external emitter heads, armoured lotosite damper pods behind locked hatches and a top seal vault - nothing of the
licensed seamless monolith.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_bastion_armour": ((0.22, 0.25, 0.28), (0.22, 0.22, 0.22), 45),
    "eq_bastion_dark": ((0.12, 0.13, 0.15), (0.18, 0.18, 0.18), 35),
    "eq_bastion_hazard": ((0.75, 0.62, 0.12), (0.20, 0.20, 0.20), 30),
})
ARM, DRK, HAZ, STL = "eq_bastion_armour", "eq_bastion_dark", "eq_bastion_hazard", "eq_steel"
TAU = 2 * math.pi
R_X = rot_y(math.pi / 2)          # lathe local Z -> world +X


# ---------------------------------------------------------------- local faceted kernel (flat-shaded solids)
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


HEXA_FACES = ((0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7))


def hexa(part, mat, c8, R=I3, t=(0, 0, 0)):
    """convex hexahedron: 4 bottom corners, then the 4 top corners in the same order (flat facets)"""
    _solid(part, mat, c8, HEXA_FACES, R, t)


def fbox(part, mat, c, s0, s1, h, R=I3, off=(0.0, 0.0)):
    """sloped armour block: bottom rectangle s0 = (w, d) at local y = 0, top s1 at y = h shifted by off (x, z);
    R turns local +Y onto the outward direction, c = centre of the bottom face"""
    (w0, d0), (w1, d1) = s0, s1
    B = [(-w0 / 2, 0, -d0 / 2), (w0 / 2, 0, -d0 / 2), (w0 / 2, 0, d0 / 2), (-w0 / 2, 0, d0 / 2)]
    T = [(x * w1 / w0 + off[0], h, z * d1 / d0 + off[1]) for x, _, z in B]
    hexa(part, mat, B + T, R, c)


def loft(part, mat, B, yB, T, yT, R=I3, t=(0, 0, 0)):
    """convex (x, z) polygons B (at y = yB) and T (at yT), same vertex count: a sloped-sided prism"""
    n = len(B)
    P = [(x, yB, z) for x, z in B] + [(x, yT, z) for x, z in T]
    faces = [tuple(range(n)), tuple(range(n, 2 * n))] + [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    _solid(part, mat, P, faces, R, t)


def zloft(part, mat, poly_xy, z0, z1, poly1=None):
    """an extrusion along Z of a convex (x, y) section (optionally lofted to poly1 at z1)"""
    q0 = [(x, -y) for x, y in poly_xy]
    q1 = [(x, -y) for x, y in (poly1 or poly_xy)]
    loft(part, mat, q0, z0, q1, z1, rot_x(math.pi / 2))


def ngon(r, phase=0.0, n=6, cx=0.0, cz=0.0):
    return [(cx + r * math.cos(phase + TAU * i / n), cz + r * math.sin(phase + TAU * i / n)) for i in range(n)]


def cham(w, d, c, cx=0.0, cz=0.0):
    """a w x d rectangle with 45 deg corner cuts c (octagon), centre (cx, cz)"""
    a, b = w / 2, d / 2
    return [(cx + x, cz + z) for x, z in ((-a + c, -b), (a - c, -b), (a, -b + c), (a, b - c), (a - c, b), (-a + c, b), (-a, b - c), (-a, -b + c))]


def hexz(part, mat, r0, r1, z0, z1, x=0.0, y=0.0, phase=0.0, n=6):
    """n-sided frustum about Z through (x, y) (flat facets)"""
    lathe(part, mat, [(0, z0), (r0, z0), (r1, z1), (0, z1)], n, I3, (x, y, 0), phase, phase + TAU, smooth=False)


def hexx(part, mat, r0, r1, x0, x1, y=0.0, z=0.0, phase=0.0, n=6):
    """n-sided frustum about X through (y, z)"""
    lathe(part, mat, [(0, x0), (r0, x0), (r1, x1), (0, x1)], n, R_X, (0, y, z), phase, phase + TAU, smooth=False)


def hexy(part, mat, r0, r1, y0, y1, x=0.0, z=0.0, phase=0.0, n=6):
    """n-sided frustum about Y (flat facets); phase measured from +X towards +Z"""
    ylathe(part, mat, [(0, y0), (r0, y0), (r1, y1), (0, y1)], n, phase, phase + TAU, (x, 0, z), smooth=False)


def face_frame(q):
    """a quad facet (b0, b1, t1, t0, inner point): returns at(u, v, h) on it (u along b0 -> b1, v bottom -> top,
    h along the outward normal) and the normal"""
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


def local_frame(R, c, w, d):
    """facet frame of a w x d rectangle centred at c in the plane local y = 0 of rotation R (normal = R * +Y)"""
    n = apply(R, (0, 1, 0))

    def at(u, v, h=0.0):
        p = apply(R, ((u - 0.5) * w, h, (v - 0.5) * d))
        return (c[0] + p[0], c[1] + p[1], c[2] + p[2])
    return at, n


def plate_on(part, mat, at, u0, u1, v0, v1, h0, h1):
    """a raised plate on a facet frame, between heights h0 and h1 above it"""
    hexa(part, mat, [at(u0, v0, h0), at(u1, v0, h0), at(u1, v1, h0), at(u0, v1, h0),
                     at(u0, v0, h1), at(u1, v0, h1), at(u1, v1, h1), at(u0, v1, h1)])


_L = [0]          # mid level of the unit being built (set by each parts function)


def tlock(part, p, nrm, s=0.12):
    """Bastion tamper lock: hazard-yellow keeper plate with a dark armoured key boss (mid level 0 only);
    close level: key slot and a seal wire loop"""
    if _L[0] != 0:
        return
    nn = norm(nrm)
    R = axis_to(nn)
    q = tuple(p[k] + 0.012 * nn[k] for k in range(3))
    box(part, HAZ, q, (s, s * 0.6, 0.024), R)
    lathe(part, DRK, [(0, 0.012), (s * 0.24, 0.012), (s * 0.24, 0.012 + s * 0.22), (0, 0.012 + s * 0.22)], 6, R, q, smooth=False)
    if close():
        top = tuple(q[k] + apply(R, (0, 0, 0.012 + s * 0.22))[k] for k in range(3))
        box(part, STL, top, (s * 0.05, s * 0.28, 0.006), R)                           # key slot
        a, b = apply(R, (-s * 0.45, 0, 0.02)), apply(R, (s * 0.10, -s * 0.36, 0.02))
        tube(part, STL, sag(tuple(q[k] + a[k] for k in range(3)), tuple(q[k] + b[k] for k in range(3)), 0.0, 4), 0.0035, 4)


def hatch_on(part, at, n, u0, u1, v0, v1, lod, uw=0.06, vw=0.06):
    """recessed armoured hatch on a facet frame: raised armour frame, dark lid sunk inside it, tamper lock
    (mid level 1 / 2: the lid only); close level: frame bolts, hinge knuckles, a seal wire on the lock"""
    if lod == 0:
        for a, b, c, d in ((u0, u1, v0, v0 + vw), (u0, u1, v1 - vw, v1), (u0, u0 + uw, v0 + vw, v1 - vw), (u1 - uw, u1, v0 + vw, v1 - vw)):
            plate_on(part, ARM, at, a, b, c, d, 0.0, 0.04)
        plate_on(part, DRK, at, u0 + uw, u1 - uw, v0 + vw, v1 - vw, 0.0, 0.015)
        tlock(part, at(u1 - 2.5 * uw, (v0 + v1) / 2, 0.015), n, 0.10)
        if close():
            for uu in (u0 + uw / 2, u1 - uw / 2):
                for vv in (v0 + vw / 2, v1 - vw / 2, (v0 + v1) / 2):
                    hex_bolt(part, STL, at(uu, vv, 0.04), n, 0.025, 0.012)
            for vv in (v0 + (v1 - v0) * 0.22, v1 - (v1 - v0) * 0.22):            # hinge knuckles on the u0 side
                p0, p1 = at(u0 + uw * 1.2, vv - 0.04 * (v1 - v0), 0.03), at(u0 + uw * 1.2, vv + 0.04 * (v1 - v0), 0.03)
                cyl(part, STL, p0, p1, 0.018, 6)
    else:
        plate_on(part, DRK, at, u0, u1, v0, v1, 0.0, 0.02)


def bflange(lod, r, th, t, phase=math.pi / 6, mat=ARM):
    """Bastion hexagonal toothed flange, local frame: tooth tips (mating plane) at z = 0, plate z = th .. th + t with
    a sloped armour rim, body towards +Z; 12 tapered interlocking teeth, keyed centre boss, 12 bolts on the back"""
    f = Part("flange")
    lathe(f, mat, [(0, th), (r, th), (r, th + t * 0.55), (r * 0.92, th + t), (0, th + t)], 6, I3, (0, 0, 0),
          phase, phase + TAU, smooth=False)
    if lod < 2:
        ri, ro = 0.58 * r, 0.82 * r
        for i in range(12):
            a = TAU * i / 12 + phase
            wb, wt = math.pi / 12 * 0.42, math.pi / 12 * 0.62
            hexa(f, DRK, [polz(ri, a - wb, 0), polz(ro, a - wb, 0), polz(ro, a + wb, 0), polz(ri, a + wb, 0),
                          polz(ri, a - wt, th), polz(ro, a - wt, th), polz(ro, a + wt, th), polz(ri, a + wt, th)])
        hexz(f, DRK, 0.30 * r, 0.30 * r, th * 0.3, th, phase=phase)
        across = max(0.035, 0.065 * r)
        for i in range(12):
            a = phase + TAU * (i + 0.5) / 12
            hex_bolt(f, STL, polz(0.80 * r, a, th + t), (0, 0, 1), across, across * 0.6)
        if close():                                                       # tooth-root locking pins
            for i in range(12):
                a = phase + TAU * (i + 0.5) / 12
                cyl(f, STL, polz(0.70 * r, a, th), polz(0.70 * r, a, th * 0.4), 0.012 * max(1.0, r), 6)
    return f


def skin_flange(part, lod, r, th, t, x=0.0, z=0.0, phase=math.pi / 6, y=0.0):
    """the toothed flange laid on the hull skin: tooth tips at y, body towards +Y"""
    merge(part, placed(bflange(lod, r, th, t, phase), R_Y, (x, y, z)))


def bolt_rim(part, at, n, u0, u1, v0, v1, h, nu, nv, across=0.07):
    """armour bolts round a plate edge (mid level 0; the kernel culls heads below the visibility floor)"""
    if _L[0] != 0:
        return
    for i in range(nu):
        u = u0 + (u1 - u0) * (i + 0.5) / nu
        for v in (v0, v1):
            hex_bolt(part, STL, at(u, v, h), n, across, across * 0.45)
    for j in range(1, nv - 1):
        v = v0 + (v1 - v0) * j / (nv - 1)
        for u in (u0 + (u1 - u0) * 0.5 / nu, u1 - (u1 - u0) * 0.5 / nu):
            hex_bolt(part, STL, at(u, v, h), n, across, across * 0.45)


def louvres(part, at, n_, u0, u1, v0, v1, h=0.035):
    """armoured louvre slats (mid level 0 only)"""
    if _L[0] != 0:
        return
    for i in range(n_):
        a = v0 + (v1 - v0) * (i + 0.15) / n_
        b = v0 + (v1 - v0) * (i + 0.65) / n_
        plate_on(part, DRK, at, u0, u1, a, b, 0.0, h)


# ---------------------------------------------------------------- BT-1 heavy armour plate
# Passport envelope [1.567, 1.045, 0.078]; as a hull-skin unit the plate lies with its thickness on +Y:
# X 1.567 x Y 0.078 x Z 1.045 (H and L exchanged - reported).
BT1 = dict(w=1.567, d=1.045, y_mount=0.022, y_top=0.072)


def bt1_parts(lod):
    _L[0] = lod
    d = BT1
    P = {}
    m = P["bt1_mount"] = Part("bt1_mount")
    for zz in (-0.33, 0.33):                                            # mount rails with toothed pads
        box(m, ARM, (0, 0.014, zz), (1.30, 0.016, 0.07))
        if lod == 0:
            for x in (-0.55, 0.55):
                skin_flange(m, lod, 0.10, 0.006, 0.016, x, zz)
    pl = P["bt1_plate"] = Part("bt1_plate")
    B = cham(d["w"], d["d"], 0.16)
    loft(pl, ARM, B, d["y_mount"], inset(B, 0.035), d["y_top"])          # bevelled sloped rim
    if lod < 2:
        for s in (-1, 1):                                                # chevron wedge facets (raised 6 mm)
            poly = [(s * 0.07, -0.30), (s * 0.40, -0.40), (s * 0.66, -0.40), (s * 0.66, 0.40), (s * 0.40, 0.40), (s * 0.07, 0.30)]
            poly = poly if s > 0 else poly[::-1]
            loft(pl, ARM, poly, d["y_top"] - 0.001, inset(poly, 0.02), d["y_top"] + 0.006)
        box(pl, HAZ, (0.70, d["y_top"] + 0.001, -0.45), (0.10, 0.003, 0.05))      # orientation mark
        for x, zz in ((-0.68, -0.44), (0.68, 0.44), (-0.68, 0.44), (0.68, -0.44)):  # tamper-locked plate bolts
            lathe(pl, DRK, [(0, 0), (0.035, 0), (0.035, 0.006), (0, 0.006)], 6, R_Y, (x, d["y_top"] - 0.01, zz), smooth=False)
        if close():
            for x in (-0.3, 0.3):
                for zz in (-0.45, 0.45):
                    hex_bolt(pl, STL, (x, d["y_top"], zz), (0, 1, 0), 0.03, 0.004)
    return P


def bt1_hits():
    d = BT1
    return [hbox("bt1_plate", (0, (d["y_mount"] + d["y_top"] + 0.006) / 2, 0), (d["w"], d["y_top"] + 0.006 - d["y_mount"], d["d"])),
            hbox("bt1_mount", (0, d["y_mount"] / 2, -0.33), (1.30, d["y_mount"], 0.22)),
            hbox("bt1_mount", (0, d["y_mount"] / 2, 0.33), (1.30, d["y_mount"], 0.22))]


# ---------------------------------------------------------------- BS-1 comm (skin: 2.466 x 1.233 x 2.466)
BS1 = dict(half=1.233, apron0=0.13, apron1=0.30, cup_x=0.30, cup_r0=0.86, cup_r1=0.50, cup_y=1.06)


def bs1_parts(lod):
    _L[0] = lod
    d = BS1
    P = {}
    f = P["bs1_flange"] = Part("bs1_flange")
    skin_flange(f, lod, 0.95, 0.05, 0.10)
    b = P["bs1_base"] = Part("bs1_base")
    A = cham(2 * d["half"], 2 * d["half"], 0.45)
    loft(b, ARM, A, d["apron0"], inset(A, 0.14), d["apron1"])
    if lod < 2:
        for k in range(4):                                               # corner hazard bars on the apron
            a = math.pi / 4 + k * math.pi / 2
            box(b, HAZ, (1.12 * math.cos(a), d["apron1"] + 0.004, 1.12 * math.sin(a)), (0.30, 0.008, 0.07), rot_y(-a - math.pi / 2))
    # transceiver: sloped armour box on the -X side, locked hatch on its top facet, vents on the outboard slope
    tr = P["bs1_transceiver"] = Part("bs1_transceiver")
    x0, x1, y0, y1 = -1.16, -0.52, d["apron1"], 0.86
    c8 = [(x0, y0, -0.62), (x1, y0, -0.62), (x1, y0, 0.62), (x0, y0, 0.62),
          (x0 + 0.18, y1, -0.48), (x1, y1, -0.48), (x1, y1, 0.48), (x0 + 0.18, y1, 0.48)]
    hexa(tr, ARM, c8)
    at, n = face_frame((c8[4], c8[5], c8[6], c8[7], (-0.8, 0.4, 0)))
    hatch_on(tr, at, n, 0.06, 0.94, 0.08, 0.92, lod)
    if lod < 2:
        at2, n2 = face_frame((c8[3], c8[0], c8[4], c8[7], (-0.8, 0.5, 0)))
        louvres(tr, at2, 5, 0.15, 0.85, 0.12, 0.88, 0.02)
    # antenna: 6-facet armoured cupola, conformal array windows on 3 facets, dark radome top, IFF beacon
    an = P["bs1_antenna"] = Part("bs1_antenna")
    cx = d["cup_x"]
    hexy(an, ARM, d["cup_r0"], d["cup_r1"], d["apron1"], d["cup_y"], cx, 0, 0.0)
    hexy(an, DRK, d["cup_r1"] - 0.04, d["cup_r1"] - 0.12, d["cup_y"], d["cup_y"] + 0.09, cx, 0, 0.0)
    hexy(an, HAZ, 0.07, 0.05, d["cup_y"] + 0.09, 2 * d["half"] / 2, cx, 0, 0.0)
    if lod < 2:
        for k in (0, 1, 5):
            a0, a1 = TAU * k / 6, TAU * (k + 1) / 6
            q = (polar(d["cup_r0"], a0, d["apron1"]), polar(d["cup_r0"], a1, d["apron1"]),
                 polar(d["cup_r1"], a1, d["cup_y"]), polar(d["cup_r1"], a0, d["cup_y"]))
            q = tuple((p[0] + cx, p[1], p[2]) for p in q) + ((cx, 0.6, 0),)
            at, n = face_frame(q)
            plate_on(an, DRK, at, 0.12, 0.88, 0.10, 0.86, 0.0, 0.02)
            for i in range(3):                                           # array element ribs
                plate_on(an, ARM, at, 0.14, 0.86, 0.28 + 0.22 * i, 0.32 + 0.22 * i, 0.02, 0.035)
    # feeder: armoured wave-guide duct from the transceiver into the cupola (no exposed cables)
    fd = P["bs1_feeder"] = Part("bs1_feeder")
    hexa(fd, ARM, [(-0.53, 0.40, -0.17), (-0.40, 0.40, -0.17), (-0.40, 0.40, 0.17), (-0.53, 0.40, 0.17),
                   (-0.53, 0.78, -0.13), (-0.22, 0.66, -0.13), (-0.22, 0.66, 0.13), (-0.53, 0.78, 0.13)])
    if lod < 2:
        tlock(fd, (-0.40, 0.70, 0.0), (0.36, 0.93, 0), 0.08)
    return P


def bs1_hits():
    d = BS1
    return [hcyl("bs1_flange", (0, 0, 0), (0, d["apron0"] + 0.02, 0), 0.95),
            hbox("bs1_base", (0, (d["apron0"] + d["apron1"]) / 2, 0), (2 * d["half"], d["apron1"] - d["apron0"], 2 * d["half"])),
            hbox("bs1_transceiver", (-0.84, 0.59, 0), (0.64, 0.58, 1.24)),
            hcyl("bs1_antenna", (d["cup_x"], d["apron1"], 0), (d["cup_x"], d["cup_y"] + 0.17, 0), 0.72),
            hbox("bs1_feeder", (-0.38, 0.59, 0), (0.32, 0.38, 0.34))]


# ---------------------------------------------------------------- BV-1 combat computer (internal: 2.231 x 1.339 x 2.231)
BV1 = dict(w=2.10, dz=2.0, y0=0.10, y1=0.96, top=1.30, half=2.231 / 2)


def bv1_parts(lod):
    _L[0] = lod
    d = BV1
    P = {}
    mt = P["bv1_mounts"] = Part("bv1_mounts")
    for x in (-0.74, 0.74):
        for z in (-0.74, 0.74):
            skin_flange(mt, lod, 0.20, 0.03, 0.08, x, z)
    v = P["bv1_vault"] = Part("bv1_vault")
    B = cham(d["w"], d["dz"], 0.22)
    loft(v, ARM, B, d["y0"], B, d["y1"])
    T = inset(B, 0.38)
    loft(v, ARM, B, d["y1"], T, d["top"] - 0.02)                           # sloped roof facets
    loft(v, DRK, T, d["top"] - 0.02, inset(T, 0.05), d["top"])            # roof seal plate
    if lod < 2:
        # armoured conduit channels on the four vertical chamfer edges (no exposed cables)
        for sx in (-1, 1):
            for sz in (-1, 1):
                a = math.atan2(sz, sx)
                c = (sx * (d["w"] / 2 - 0.11), (d["y0"] + d["y1"]) / 2, sz * (d["dz"] / 2 - 0.11))
                Rw = mat_mul(rot_y(-a), rot_z(-math.pi / 2))
                fbox(v, ARM, c, (d["y1"] - d["y0"] - 0.04, 0.16), (d["y1"] - d["y0"] - 0.10, 0.10), 0.06, Rw)
        tlock(v, (0.0, d["top"], 0.0), (0, 1, 0), 0.14)
    # board bay (+Z): card cage behind an armoured drawer front
    bd = P["bv1_board"] = Part("bv1_board")
    zf = d["dz"] / 2
    fbox(bd, DRK, (0, 0.52, zf), (1.30, 0.62), (1.24, 0.56), 0.06, rot_x(math.pi / 2))
    if lod < 2:
        at, n = local_frame(rot_x(math.pi / 2), (0, 0.52, zf + 0.06), 1.24, 0.56)
        for i in range(9):
            plate_on(bd, ARM, at, 0.08 + 0.095 * i, 0.12 + 0.095 * i, 0.10, 0.90, 0.0, 0.03)  # card edge guards
        for u in (0.04, 0.96):
            plate_on(bd, STL, at, u - 0.025, u + 0.025, 0.25, 0.75, 0.0, 0.05)                 # pull handles
        tlock(bd, at(0.5, 0.05, 0.0), n, 0.10)
    # memory: crash-hardened hex canister set into the +X side
    me = P["bv1_memory"] = Part("bv1_memory")
    hexx(me, ARM, 0.30, 0.26, d["w"] / 2 - 0.02, d["half"], 0.60, 0.0)
    if lod < 2:
        hexx(me, HAZ, 0.31, 0.31, d["w"] / 2 + 0.006, d["w"] / 2 + 0.036, 0.60, 0.0)
        hexx(me, DRK, 0.16, 0.12, d["half"] - 0.01, d["half"] + 0.0, 0.60, 0.0, math.pi / 6)
    # psu (-Z): power unit with armoured louvres and its own locked hatch
    ps = P["bv1_psu"] = Part("bv1_psu")
    zb = -d["dz"] / 2
    fbox(ps, ARM, (0, 0.50, zb), (1.50, 0.66), (1.44, 0.60), 0.065, rot_x(-math.pi / 2))
    if lod < 2:
        at, n = local_frame(rot_x(-math.pi / 2), (0, 0.50, zb - 0.065), 1.44, 0.60)
        louvres(ps, at, 6, 0.05, 0.58, 0.06, 0.94)
        hatch_on(ps, at, n, 0.64, 0.96, 0.08, 0.92, lod)
    return P


def bv1_hits():
    d = BV1
    return [hbox("bv1_vault", (0, (d["y0"] + d["top"]) / 2, 0), (d["w"], d["top"] - d["y0"], d["dz"])),
            hbox("bv1_board", (0, 0.52, d["dz"] / 2 + 0.04), (1.30, 0.62, 0.08)),
            hcyl("bv1_memory", (d["w"] / 2 - 0.02, 0.60, 0), (d["half"], 0.60, 0), 0.30),
            hbox("bv1_psu", (0, 0.50, -d["dz"] / 2 - 0.035), (1.50, 0.66, 0.07))] + \
           [hcyl("bv1_mounts", (x, 0, z), (x, d["y0"] + 0.01, z), 0.20) for x in (-0.74, 0.74) for z in (-0.74, 0.74)]


# ---------------------------------------------------------------- AP-4 interceptor hangar (bay module 12.429 x 6.214 x 15.536)
# Four launch cells (2 x 2) open forward (+Z) behind recessed split blast doors; the cradles (launch / recovery rails)
# run inside the cells, their carriage drives in two armoured roof fairings; refuel / charge blocks on the sides.
AP4 = dict(hx=5.96, ex=6.2145, yb=-2.86, yt=2.85, ey=3.107, z0=-7.70, z1=7.30, zf=7.768,
           cells=[(-2.55, 1.35), (2.55, 1.35), (-2.55, -1.45), (2.55, -1.45)], cw=4.0, ch=2.0, fr=0.25)


def ap4_section():
    d = AP4
    hx, yb, yt = d["hx"], d["yb"], d["yt"]
    return [(-hx + 0.7, yb), (hx - 0.7, yb), (hx, yb + 0.7), (hx, 1.55), (hx - 1.30, yt), (-hx + 1.30, yt), (-hx, 1.55), (-hx, yb + 0.7)]


def ap4_parts(lod):
    _L[0] = lod
    d = AP4
    P = {}
    h = P["ap4_hull"] = Part("ap4_hull")
    zloft(h, ARM, ap4_section(), d["z0"], d["z1"])
    if lod < 2:
        for sx in (-1, 1):                                               # side armour plates with dark seams
            Rs = rot_z(-sx * math.pi / 2)
            for zc in (-5.85, -2.15, 1.55, 5.25):
                at, n = local_frame(Rs, (sx * d["hx"], -0.35, zc), 3.0, 3.5)
                plate_on(h, ARM, at, 0.02, 0.98, 0.02, 0.98, 0.0, 0.07)
                bolt_rim(h, at, n, 0.02, 0.98, 0.05, 0.95, 0.07, 8, 7, 0.08)
        for xc, w in ((0.0, 3.2), (-4.0, 1.2), (4.0, 1.2)):                                # roof armour plates
            for zc in (-5.4, -1.8, 1.8, 5.4):
                at, n = local_frame(I3, (xc, d["yt"], zc), w, 3.4)
                plate_on(h, ARM, at, 0.02, 0.98, 0.02, 0.98, 0.0, 0.05)
                bolt_rim(h, at, n, 0.02, 0.98, 0.05, 0.95, 0.05, max(2, int(w * 2.5)), 8, 0.08)
        at, n = local_frame(rot_x(-math.pi / 2), (0, -0.7, d["z0"]), 2.8, 2.6)            # rear service hatch
        hatch_on(h, at, n, 0.0, 1.0, 0.0, 1.0, lod, 0.04, 0.04)
    # front: 4 cell mouths - armour frames out to the envelope face, split blast doors recessed 0.47 m
    dr = P["ap4_doors"] = Part("ap4_doors")
    zf, fr = d["zf"], d["fr"]
    Rf = rot_x(math.pi / 2)                                              # local +Y -> world +Z
    for cx, cy in d["cells"]:
        w, hh = d["cw"], d["ch"]
        for (x0, x1, y0, y1) in ((cx - w / 2 - fr, cx + w / 2 + fr, cy + hh / 2, cy + hh / 2 + fr),
                                 (cx - w / 2 - fr, cx + w / 2 + fr, cy - hh / 2 - fr, cy - hh / 2),
                                 (cx - w / 2 - fr, cx - w / 2, cy - hh / 2, cy + hh / 2),
                                 (cx + w / 2, cx + w / 2 + fr, cy - hh / 2, cy + hh / 2)):
            fbox(dr, ARM, ((x0 + x1) / 2, (y0 + y1) / 2, d["z1"]), (x1 - x0, y1 - y0), (x1 - x0 - 0.05, y1 - y0 - 0.05), zf - d["z1"], Rf)
        for s in (-1, 1):                                                # two door leaves
            xm = cx + s * w / 4
            fbox(dr, DRK, (xm, cy, d["z1"]), (w / 2 - 0.02, hh), (w / 2 - 0.08, hh - 0.06), 0.10, Rf)
            if lod < 2:
                for yy in (cy - hh * 0.28, cy + hh * 0.28):              # wedge stiffeners on the leaves
                    fbox(dr, ARM, (xm, yy, d["z1"] + 0.10), (w / 2 - 0.35, 0.24), (w / 2 - 0.45, 0.12), 0.07, Rf)
        if lod < 2:
            box(dr, HAZ, (cx, cy, d["z1"] + 0.11), (0.05, hh - 0.12, 0.02))          # door seam
            for yy in (cy + hh / 2 + fr / 2, cy - hh / 2 - fr / 2):                   # hazard strips on the frame face
                for k in range(5):
                    box(dr, HAZ, (cx - w / 2 + 0.3 + k * (w - 0.6) / 4, yy, zf), (0.30, 0.10, 0.01))
            tlock(dr, (cx + 0.20, cy, d["z1"] + 0.10), (0, 0, 1), 0.16)
    # cradle: rail tongues in the cell sills + two armoured carriage-drive fairings along the roof
    cr = P["ap4_cradle"] = Part("ap4_cradle")
    for cx, cy in d["cells"]:
        for s in (-1, 1):
            box(cr, STL, (cx + s * 1.1, cy - d["ch"] / 2 + 0.07, (d["z1"] + zf) / 2), (0.26, 0.14, zf - d["z1"] - 0.02))
    for cx in (-2.55, 2.55):
        zloft(cr, ARM, [(cx - 0.85, d["yt"]), (cx + 0.85, d["yt"]), (cx + 0.55, d["ey"]), (cx - 0.55, d["ey"])], -6.8, 6.6)
        if lod < 2:
            for zc in (-5.0, -1.6, 1.8, 5.0):
                at, n = local_frame(I3, (cx, d["ey"], zc), 0.95, 1.4)
                hatch_on(cr, at, n, 0.06, 0.94, 0.06, 0.94, lod, 0.08, 0.05)
    # charger: refuel / charge service blocks on both sides, armoured conduit forward to the cells
    ch = P["ap4_charger"] = Part("ap4_charger")
    for sx in (-1, 1):
        Rs = rot_z(-sx * math.pi / 2)                                    # local +Y -> world +/-X
        for zc in (-4.4, 1.6):
            fbox(ch, ARM, (sx * (d["hx"] + 0.07), -1.25, zc), (1.7, 2.0), (1.4, 1.7), d["ex"] - d["hx"] - 0.07, Rs)
            if lod < 2:
                at, n = local_frame(Rs, (sx * d["ex"], -1.25, zc), 1.4, 1.7)
                hatch_on(ch, at, n, 0.08, 0.92, 0.08, 0.92, lod)
        # armoured conduit from the rear block, past the front block, to the front frame
        fbox(ch, ARM, (sx * (d["hx"] + 0.07), -0.08, 1.4), (0.36, 11.4), (0.22, 11.4), 0.12, Rs)
    # mounts: six toothed hex pads under the floor
    mt = P["ap4_mounts"] = Part("ap4_mounts")
    for x in (-3.6, 3.6):
        for z in (-5.2, 0.0, 5.2):
            skin_flange(mt, lod, 0.62, 0.08, 0.18, x, z, y=-d["ey"])
    return P


def ap4_hits():
    d = AP4
    H = [hbox("ap4_hull", (0, (d["yb"] + d["yt"]) / 2, (d["z0"] + d["z1"]) / 2), (2 * d["hx"], d["yt"] - d["yb"], d["z1"] - d["z0"]))]
    for cx, cy in d["cells"]:
        H.append(hbox("ap4_doors", (cx, cy, (d["z1"] + d["zf"]) / 2), (d["cw"] + 2 * d["fr"], d["ch"] + 2 * d["fr"], d["zf"] - d["z1"])))
        H.append(hbox("ap4_cradle", (cx, cy - d["ch"] / 2 + 0.3, 0.0), (d["cw"] - 0.4, 0.6, 14.0)))   # rails inside the cells
    for cx in (-2.55, 2.55):
        H.append(hbox("ap4_cradle", (cx, (d["yt"] + d["ey"]) / 2, -0.1), (1.7, d["ey"] - d["yt"], 13.4)))
    for sx in (-1, 1):
        for zc in (-4.4, 1.6):
            H.append(hbox("ap4_charger", (sx * (d["hx"] + d["ex"]) / 2, -1.25, zc), (d["ex"] - d["hx"], 1.7, 2.0)))
    for x in (-3.6, 3.6):
        for z in (-5.2, 0.0, 5.2):
            H.append(hcyl("ap4_mounts", (x, -d["ey"], z), (x, d["yb"] + 0.01, z), 0.62))
    return H


# ---------------------------------------------------------------- B-12 jump core (axis Z, cube 3.302, centred)
B12 = dict(half=1.651, R=1.30, ap=1.30 * math.cos(math.pi / 6), zv=1.45, head_r=1.62, fl_r=1.40)
B12_PODS = (math.pi / 6, 5 * math.pi / 6, 7 * math.pi / 6, 11 * math.pi / 6)   # damper facets (top: seal, bottom: mounts)


def b12_parts(lod):
    _L[0] = lod
    d = B12
    P = {}
    R, ap, zv = d["R"], d["ap"], d["zv"]
    v = P["b12_vessel"] = Part("b12_vessel")
    lathe(v, ARM, [(0, -zv), (0.95, -zv), (R, -1.12), (R, 1.12), (0.95, zv), (0, zv)], 6, I3, (0, 0, 0), 0.0, TAU, smooth=False)
    if lod < 2:
        for k in range(6):                                               # facet armour panels with bolt rims
            a = TAU * (k + 0.5) / 6
            for zs in (-1, 1):
                at, n = local_frame(rot_z(a - math.pi / 2), polz(d["ap"], a, zs * 0.83), 1.12, 0.70)
                plate_on(v, ARM, at, 0.0, 1.0, 0.0, 1.0, 0.0, 0.02)
                bolt_rim(v, at, n, 0.0, 1.0, 0.05, 0.95, 0.02, 6, 4, 0.07)
        for k in range(6):                                               # vertex armour strips (wedge ridges)
            a = TAU * k / 6
            for zs in (-1, 1):
                hexa(v, ARM, [polz(R - 0.02, a - 0.06, zs * 0.50), polz(R - 0.02, a + 0.06, zs * 0.50), polz(R - 0.02, a + 0.06, zs * 1.10),
                              polz(R - 0.02, a - 0.06, zs * 1.10), polz(R + 0.06, a - 0.02, zs * 0.55), polz(R + 0.06, a + 0.02, zs * 0.55),
                              polz(R + 0.06, a + 0.02, zs * 1.05), polz(R + 0.06, a - 0.02, zs * 1.05)])
    # field emitter: dark belt round the middle, six faceted emitter heads at the hex vertices
    fe = P["b12_field"] = Part("b12_field")
    lathe(fe, DRK, crect(R - 0.06, R + 0.07, -0.44, 0.44), 6, I3, (0, 0, 0), 0.0, TAU, smooth=False)
    if lod < 2:
        for zz in (-0.46, 0.44):
            lathe(fe, HAZ, crect(R + 0.0, R + 0.075, zz, zz + 0.02), 6, I3, (0, 0, 0), 0.0, TAU, smooth=False)
    for k in range(6):
        a = TAU * k / 6
        r0, r1 = R + 0.05, d["head_r"]
        hexa(fe, ARM, [polz(r0, a - 0.17, -0.40), polz(r0, a + 0.17, -0.40), polz(r0, a + 0.17, 0.40), polz(r0, a - 0.17, 0.40),
                       polz(r1, a - 0.065, -0.26), polz(r1, a + 0.065, -0.26), polz(r1, a + 0.065, 0.26), polz(r1, a - 0.065, 0.26)])
        if lod < 2:
            hexa(fe, DRK, [polz(r1 - 0.01, a - 0.05, -0.22), polz(r1 - 0.01, a + 0.05, -0.22), polz(r1 - 0.01, a + 0.05, 0.22),
                           polz(r1 - 0.01, a - 0.05, 0.22), polz(r1 + 0.02, a - 0.04, -0.20), polz(r1 + 0.02, a + 0.04, -0.20),
                           polz(r1 + 0.02, a + 0.04, 0.20), polz(r1 + 0.02, a - 0.04, 0.20)])
            for zs in (-1, 1):                                           # emitter cooling fins on the head flanks
                for i in range(3 if lod == 0 else 2):
                    zz = zs * (0.12 + 0.09 * i)
                    hexa(fe, ARM, [polz(r0 + 0.02, a - 0.20, zz - 0.015), polz(r0 + 0.02, a + 0.20, zz - 0.015), polz(r0 + 0.02, a + 0.20, zz + 0.015),
                                   polz(r0 + 0.02, a - 0.20, zz + 0.015), polz(r0 + 0.18, a - 0.13, zz - 0.015), polz(r0 + 0.18, a + 0.13, zz - 0.015),
                                   polz(r0 + 0.18, a + 0.13, zz + 0.015), polz(r0 + 0.18, a - 0.13, zz + 0.015)])
    # lotosite dampers: eight armoured wedge pods on the four side facets, each behind a locked hatch
    dp = P["b12_dampers"] = Part("b12_dampers")
    for a in B12_PODS:
        Rp = rot_z(a - math.pi / 2)                                      # local +Y -> facet normal
        for zs in (-1, 1):
            zc = zs * 0.83
            c = polz(ap, a, zc)
            fbox(dp, ARM, c, (0.98, 0.56), (0.74, 0.42), 0.24, Rp)
            at, n = local_frame(Rp, polz(ap + 0.24, a, zc), 0.74, 0.42)
            hatch_on(dp, at, n, 0.0, 1.0, 0.0, 1.0, lod, 0.06, 0.09)
            if lod < 2:                                                  # lotosite sight window
                plate_on(dp, HAZ, at, 0.18, 0.30, 0.38, 0.62, 0.015, 0.03)
    # seal: armoured seal vault on the top facet - tamper control, hazard seal band, locks, seal wires (close)
    se = P["b12_seal"] = Part("b12_seal")
    fbox(se, ARM, (0, ap, 0), (1.10, 1.90), (0.78, 1.56), d["head_r"] - ap)
    yt = d["head_r"]
    if lod < 2:
        box(se, HAZ, (0, yt + 0.006, 0), (0.78, 0.012, 0.16))
        for zz in (-0.55, 0.55):
            tlock(se, (0.0, yt, zz), (0, 1, 0), 0.16)
        at, n = local_frame(I3, (0, yt, 0), 0.78, 1.56)
        louvres(se, at, 3, 0.15, 0.85, 0.62, 0.92, 0.02)
        if close():
            for zz in (-0.55, 0.55):
                tube(se, STL, sag((-0.06, yt + 0.05, zz), (0.30, yt + 0.002, zz - 0.25 * (1 if zz > 0 else -1)), 0.0, 6), 0.004, 5)
    # flanges: toothed hex flanges on both ends (mating planes at z = -/+1.651)
    fl = bflange(lod, d["fl_r"], 0.06, 0.14)
    P["b12_flange_fore"] = placed(fl, I3, (0, 0, -d["half"]))
    P["b12_flange_fore"].name = "b12_flange_fore"
    P["b12_flange_aft"] = placed(fl, rot_x(math.pi), (0, 0, d["half"]))
    P["b12_flange_aft"].name = "b12_flange_aft"
    # mounts: two armoured saddles on the bottom facet with toothed foot pads
    mt = P["b12_mounts"] = Part("b12_mounts")
    for zz in (-0.83, 0.83):
        fbox(mt, ARM, (0, -ap, zz), (1.0, 0.50), (0.70, 0.36), 0.40, rot_z(math.pi))
        skin_flange(mt, lod, 0.30, 0.04, 0.10, 0.0, zz, y=-d["half"])
    return P


def b12_hits():
    d = B12
    H = [hcyl("b12_vessel", (0, 0, -d["zv"]), (0, 0, d["zv"]), d["ap"]),
         hcyl("b12_field", (0, 0, -0.44), (0, 0, 0.44), d["R"] + 0.07)]
    for k in range(6):
        a = TAU * k / 6
        H.append(hbox("b12_field", polz((d["R"] + d["head_r"]) / 2, a, 0), (d["head_r"] - d["R"], 0.40, 0.80), (0, 0, math.degrees(a))))
    for a in B12_PODS:
        for zs in (-1, 1):
            H.append(hbox("b12_dampers", polz(d["ap"] + 0.13, a, zs * 0.83), (0.26, 0.98, 0.56), (0, 0, math.degrees(a))))
    H += [hbox("b12_seal", (0, (d["ap"] + d["head_r"]) / 2, 0), (1.10, d["head_r"] - d["ap"], 1.90)),
          hcyl("b12_flange_fore", (0, 0, -d["half"]), (0, 0, -d["half"] + 0.20), d["fl_r"]),
          hcyl("b12_flange_aft", (0, 0, d["half"] - 0.20), (0, 0, d["half"]), d["fl_r"])]
    for zz in (-0.83, 0.83):
        H.append(hbox("b12_mounts", (0, -(d["ap"] + d["half"]) / 2, zz), (1.0, d["half"] - d["ap"], 0.50)))
    return H


# ---------------------------------------------------------------- S-50 survey radar (skin: 5.396 x 2.159 x 5.396)
S50 = dict(half=2.698, a0=0.14, a1=0.40, r0=2.30, r1=1.00, y1=1.94, top=2.10, ph=math.pi / 6)


def s50_facet(k):
    d = S50
    a0, a1 = d["ph"] + TAU * k / 6, d["ph"] + TAU * (k + 1) / 6
    return face_frame((polar(d["r0"], a0, d["a1"]), polar(d["r0"], a1, d["a1"]), polar(d["r1"], a1, d["y1"]), polar(d["r1"], a0, d["y1"]),
                       (0, 0.8, 0)))


def s50_parts(lod):
    _L[0] = lod
    d = S50
    P = {}
    f = P["s50_flange"] = Part("s50_flange")
    skin_flange(f, lod, 1.60, 0.06, 0.14)
    ap = P["s50_apron"] = Part("s50_apron")
    A = cham(2 * d["half"], 2 * d["half"], 1.1)
    loft(ap, ARM, A, d["a0"], inset(A, 0.22), d["a1"])
    if lod < 2:
        for k in range(4):                                               # corner hazard bars + tamper locks
            a = math.pi / 4 + k * math.pi / 2
            box(ap, HAZ, (2.62 * math.cos(a), d["a1"] + 0.004, 2.62 * math.sin(a)), (0.60, 0.008, 0.10), rot_y(-a - math.pi / 2))
            tlock(ap, (2.40 * math.cos(a), d["a1"], 2.40 * math.sin(a)), (0, 1, 0), 0.14)
    h = P["s50_housing"] = Part("s50_housing")
    hexy(h, ARM, d["r0"], d["r1"], d["a1"], d["y1"], 0, 0, d["ph"])
    # array: transmit / receive module tiles on all six sloped facets
    ar = P["s50_array"] = Part("s50_array")
    nu, nv = {0: (5, 4), 1: (1, 1), 2: (1, 1)}[lod]
    if close():
        nu, nv = 8, 6
    for k in range(6):
        at, n = s50_facet(k)
        if lod == 2:
            plate_on(ar, DRK, at, 0.07, 0.93, 0.06, 0.92, 0.0, 0.03)
            continue
        plate_on(ar, ARM, at, 0.05, 0.95, 0.04, 0.94, 0.0, 0.025)                    # tile carrier
        g = 0.012
        for i in range(nu):
            for j in range(nv):
                u0 = 0.07 + 0.86 * i / nu
                v0 = 0.06 + 0.86 * j / nv
                plate_on(ar, DRK, at, u0 + g, u0 + 0.86 / nu - g, v0 + g, v0 + 0.86 / nv - g, 0.025, 0.055)
    # rx: receiver crown on top, behind a locked hatch
    rx = P["s50_rx"] = Part("s50_rx")
    hexy(rx, ARM, 0.98, 0.82, d["y1"], d["top"] - 0.02, 0, 0, d["ph"])
    hexy(rx, DRK, 0.72, 0.66, d["top"] - 0.02, d["top"], 0, 0, d["ph"])
    if lod < 2:
        at, n = local_frame(I3, (0, d["top"], 0), 0.9, 0.7)
        tlock(rx, at(0.5, 0.5), n, 0.16)
    return P


def s50_hits():
    d = S50
    c30 = math.cos(math.pi / 6)
    dap, dy = (d["r0"] - d["r1"]) * c30, d["y1"] - d["a1"]
    sl, slant = math.atan2(dap, dy), math.hypot(dap, dy)                  # facet tilt from vertical, slant height
    ap_m, y_m = (d["r0"] + d["r1"]) / 2 * c30, (d["a1"] + d["y1"]) / 2
    return [hcyl("s50_flange", (0, 0, 0), (0, d["a0"] + 0.02, 0), 1.60),
            hbox("s50_apron", (0, (d["a0"] + d["a1"]) / 2, 0), (2 * d["half"], d["a1"] - d["a0"], 2 * d["half"])),
            hcyl("s50_housing", (0, d["a1"], 0), (0, (d["a1"] + d["y1"]) / 2, 0), 1.95),
            hcyl("s50_housing", (0, (d["a1"] + d["y1"]) / 2, 0), (0, d["y1"], 0), 1.35),
            hcyl("s50_rx", (0, d["y1"], 0), (0, d["top"], 0), 0.90)] + \
           [hbox("s50_array", polar(ap_m, d["ph"] + TAU * (k + 0.5) / 6, y_m), (0.12, slant, 0.9 * (d["r0"] + d["r1"]) / 2),
                 (0, -math.degrees(d["ph"] + TAU * (k + 0.5) / 6), math.degrees(sl))) for k in range(6)]


# ---------------------------------------------------------------- BP-10 coilgun (skin: 1.482 x 0.889 x 2.371, barrel +Z)
BP10 = dict(hz=2.371 / 2, by=0.52, top=0.889)


def bp10_parts(lod):
    _L[0] = lod
    d = BP10
    P = {}
    f = P["bp10_flange"] = Part("bp10_flange")
    skin_flange(f, lod, 0.70, 0.025, 0.06, phase=0.0)
    # mount (laying drive): hex yaw ring, two sloped trunnion cheeks, trunnion hubs, yaw drive box
    m = P["bp10_mount"] = Part("bp10_mount")
    hexy(m, ARM, 0.66, 0.58, 0.085, 0.22, 0, 0, 0.0)
    for s in (-1, 1):
        fbox(m, ARM, (s * 0.40, 0.22, -0.05), (0.12, 0.84), (0.12, 0.46), 0.50, I3, (0.0, -0.08))
        hexx(m, DRK, 0.11, 0.09, s * 0.46, s * 0.52, d["by"], -0.09)
    fbox(m, ARM, (0.56, 0.09, 0.30), (0.26, 0.36), (0.20, 0.28), 0.22)
    # housing: armoured gun cradle between the cheeks with a sloped glacis
    g = P["bp10_housing"] = Part("bp10_housing")
    hexa(g, ARM, [(-0.33, 0.24, -0.45), (0.33, 0.24, -0.45), (0.33, 0.24, 0.32), (-0.33, 0.24, 0.32),
                  (-0.28, 0.80, -0.42), (0.28, 0.80, -0.42), (0.28, 0.80, 0.06), (-0.28, 0.80, 0.06)])
    if lod < 2:
        at, n = face_frame(((-0.28, 0.80, -0.42), (0.28, 0.80, -0.42), (0.28, 0.80, 0.06), (-0.28, 0.80, 0.06), (0, 0.5, -0.2)))
        hatch_on(g, at, n, 0.10, 0.90, 0.08, 0.80, lod, 0.06, 0.08)
        box(g, HAZ, (0, d["top"] - 0.075, 0.03), (0.40, 0.02, 0.04))
    # coil: hex barrel with seven accelerator coil collars and an armoured muzzle
    c = P["bp10_coil"] = Part("bp10_coil")
    hexz(c, ARM, 0.09, 0.09, 0.15, d["hz"] - 0.08, 0, d["by"], 0.0)
    nco = 7 if lod < 2 else 4
    for i in range(nco):
        z0 = 0.30 + (0.78 / nco) * i
        hexz(c, DRK, 0.155, 0.155, z0, z0 + 0.07, 0, d["by"], math.pi / 6)
    hexz(c, ARM, 0.15, 0.12, d["hz"] - 0.10, d["hz"], 0, d["by"], math.pi / 6)
    if lod < 2:
        hexz(c, HAZ, 0.155, 0.155, d["hz"] - 0.13, d["hz"] - 0.10, 0, d["by"], math.pi / 6)
        hexz(c, DRK, 0.05, 0.05, d["hz"], d["hz"] + 0.001, 0, d["by"], 0.0)
    # caps: capacitor bank in a sloped armour box behind the cradle, louvres on top
    cp = P["bp10_caps"] = Part("bp10_caps")
    fbox(cp, ARM, (0, 0.22, -0.80), (0.72, 0.76), (0.56, 0.60), 0.48, I3, (0.0, 0.03))
    if lod < 2:
        at, n = local_frame(I3, (0, 0.70, -0.77), 0.56, 0.60)
        louvres(cp, at, 4, 0.12, 0.88, 0.10, 0.90, 0.025)
        box(cp, HAZ, (0, 0.55, -1.175), (0.40, 0.06, 0.012))
    # feed: projectile feed chute on the -X side, from the base into the cheek
    fd = P["bp10_feed"] = Part("bp10_feed")
    hexa(fd, ARM, [(-0.741, 0.10, -0.55), (-0.46, 0.10, -0.55), (-0.46, 0.10, 0.12), (-0.741, 0.10, 0.12),
                   (-0.68, 0.56, -0.45), (-0.46, 0.60, -0.45), (-0.46, 0.60, 0.02), (-0.68, 0.56, 0.02)])
    if lod < 2:
        at, n = face_frame(((-0.741, 0.10, 0.12), (-0.741, 0.10, -0.55), (-0.68, 0.56, -0.45), (-0.68, 0.56, 0.02), (-0.6, 0.3, -0.2)))
        hatch_on(fd, at, n, 0.10, 0.90, 0.12, 0.88, lod, 0.07, 0.08)
    return P


def bp10_hits():
    d = BP10
    return [hcyl("bp10_flange", (0, 0, 0), (0, 0.085, 0), 0.70),
            hcyl("bp10_mount", (0, 0.085, 0), (0, 0.22, 0), 0.62),
            hbox("bp10_mount", (-0.40, 0.47, -0.09), (0.12, 0.50, 0.84)),
            hbox("bp10_mount", (0.40, 0.47, -0.09), (0.12, 0.50, 0.84)),
            hbox("bp10_mount", (0.56, 0.20, 0.30), (0.26, 0.22, 0.36)),
            hbox("bp10_housing", (0, 0.52, -0.08), (0.66, 0.56, 0.76)),
            hcyl("bp10_coil", (0, d["by"], 0.15), (0, d["by"], d["hz"]), 0.155),
            hbox("bp10_caps", (0, 0.46, -0.80), (0.72, 0.48, 0.76)),
            hbox("bp10_feed", (-0.60, 0.35, -0.215), (0.28, 0.50, 0.67))]


# ---------------------------------------------------------------- BT-4 point-defence laser (skin: 1.984 x 1.191 x 3.175)
BT4 = dict(hx=1.984 / 2, hz=3.175 / 2, ey=0.72, er=0.42, top=1.191)


def bt4_parts(lod):
    _L[0] = lod
    d = BT4
    P = {}
    f = P["bt4_flange"] = Part("bt4_flange")
    skin_flange(f, lod, 0.85, 0.03, 0.06, phase=0.0)
    # mount: hex yaw ring, sloped yoke arms, elevation hubs, yaw drive under the tail
    m = P["bt4_mount"] = Part("bt4_mount")
    hexy(m, ARM, 0.80, 0.72, 0.09, 0.30, 0, 0, 0.0)
    for s in (-1, 1):
        fbox(m, ARM, (s * 0.78, 0.30, 0.0), (0.18, 0.86), (0.16, 0.52), 0.58)
        hexx(m, ARM, 0.17, 0.15, s * 0.87, s * d["hx"], d["ey"], 0.0, 0.0)
        hexx(m, DRK, 0.12, 0.12, s * 0.36, s * 0.69, d["ey"], 0.0, math.pi / 6)
        if lod < 2:
            hexx(m, HAZ, 0.175, 0.175, s * 0.90, s * 0.93, d["ey"], 0.0, 0.0)
    fbox(m, ARM, (0, 0.09, -0.85), (0.64, 0.56), (0.50, 0.42), 0.20)
    # emitter: long faceted hex housing, tapered glacis nose, dark lens, armoured shutter hood, tail fins, tracker
    e = P["bt4_emitter"] = Part("bt4_emitter")
    r, y = d["er"], d["ey"]
    lathe(e, ARM, [(0, -d["hz"]), (0.34, -d["hz"]), (r, -1.42), (r, 0.95), (0.29, 1.40), (0, 1.40)], 6, I3, (0, y, 0), 0.0, TAU, smooth=False)
    hexz(e, DRK, 0.22, 0.22, 1.40, 1.43, 0, y, 0.0)
    hexa(e, ARM, [(-0.31, y + 0.27, 1.00), (0.31, y + 0.27, 1.00), (0.24, y + 0.20, d["hz"]), (-0.24, y + 0.20, d["hz"]),
                  (-0.27, y + 0.36, 1.00), (0.27, y + 0.36, 1.00), (0.20, y + 0.27, d["hz"]), (-0.20, y + 0.27, d["hz"])])
    if lod < 2:
        for a in (math.pi / 6, 5 * math.pi / 6, 7 * math.pi / 6, 11 * math.pi / 6):            # tail cooling fins
            ra = r * math.cos(math.pi / 6)
            for i in range(4 if lod == 0 else 2):
                z0 = -1.38 + i * (0.16 if lod == 0 else 0.30)
                hexa(e, DRK, [polz(ra, a - 0.10, z0), polz(ra, a + 0.10, z0), polz(ra, a + 0.10, z0 + 0.06), polz(ra, a - 0.10, z0 + 0.06),
                              polz(ra + 0.14, a - 0.06, z0), polz(ra + 0.14, a + 0.06, z0), polz(ra + 0.14, a + 0.06, z0 + 0.06),
                              polz(ra + 0.14, a - 0.06, z0 + 0.06)], I3, (0, y, 0))
        at, n = local_frame(I3, (0, y + r * math.sin(math.pi / 3), -0.35), 0.40, 0.90)    # locked service hatch on top
        hatch_on(e, at, n, 0.0, 1.0, 0.0, 1.0, lod)
        hexz(e, HAZ, r + 0.005, r + 0.005, 0.80, 0.85, 0, y, 0.0)
    fbox(e, ARM, (0, y + r * math.sin(math.pi / 3), 0.45), (0.30, 0.52), (0.20, 0.36), d["top"] - y - r * math.sin(math.pi / 3))
    if lod < 2:
        at, n = face_frame(((-0.15, y + 0.364, 0.71), (0.15, y + 0.364, 0.71), (0.10, d["top"], 0.63), (-0.10, d["top"], 0.63), (0, y, 0.45)))
        plate_on(e, DRK, at, 0.15, 0.85, 0.15, 0.85, 0.0, 0.01)                              # tracker window
    return P


def bt4_hits():
    d = BT4
    return [hcyl("bt4_flange", (0, 0, 0), (0, 0.09, 0), 0.85),
            hcyl("bt4_mount", (0, 0.09, 0), (0, 0.30, 0), 0.76),
            hbox("bt4_mount", (-0.78, 0.59, 0), (0.18, 0.58, 0.86)),
            hbox("bt4_mount", (0.78, 0.59, 0), (0.18, 0.58, 0.86)),
            hcyl("bt4_mount", (-d["hx"], d["ey"], 0), (d["hx"], d["ey"], 0), 0.17),
            hbox("bt4_mount", (0, 0.19, -0.85), (0.64, 0.20, 0.56)),
            hcyl("bt4_emitter", (0, d["ey"], -d["hz"]), (0, d["ey"], 1.43), d["er"]),
            hbox("bt4_emitter", (0, d["ey"] + 0.30, 1.30), (0.62, 0.18, 0.58)),
            hbox("bt4_emitter", (0, (d["ey"] + 0.36 + d["top"]) / 2, 0.45), (0.30, d["top"] - d["ey"] - 0.36, 0.52))]


# ---------------------------------------------------------------- VPU-16 missile cells (skin: 4.275 x 2.565 x 6.84)
VPU = dict(hx=4.275 / 2, bx=2.0, hz=6.84 / 2, bz=3.30, deck=2.40, top=2.565,
           cols=(-1.18, -0.42, 0.42, 1.18), rows=(-2.40, -0.80, 0.80, 2.40), cw=0.66, cl=1.40)


def vpu_section(m=0.0):
    d = VPU
    return [(-d["bx"] + m, 0.12), (d["bx"] - m, 0.12), (d["bx"] - m, 0.85), (1.70 - m, d["deck"] - m * 0.5),
            (-1.70 + m, d["deck"] - m * 0.5), (-d["bx"] + m, 0.85)]


def vpu16_parts(lod):
    _L[0] = lod
    d = VPU
    P = {}
    f = P["vpu16_flange"] = Part("vpu16_flange")
    for zz in (-1.7, 1.7):
        skin_flange(f, lod, 1.0, 0.04, 0.10, 0.0, zz)
    b = P["vpu16_body"] = Part("vpu16_body")
    zloft(b, ARM, vpu_section(), -d["bz"], d["bz"])
    zloft(b, ARM, vpu_section(), d["bz"], d["hz"], vpu_section(0.10))           # sloped fore armour lip
    # central exhaust uptake spine with vent slats per row
    zloft(b, ARM, [(-0.24, d["deck"]), (0.24, d["deck"]), (0.18, d["top"]), (-0.18, d["top"])], -3.22, 3.22)
    if lod < 2:
        for zr in d["rows"]:
            at, n = local_frame(I3, (0, d["top"], zr), 0.30, 1.1)
            louvres(b, at, 5, 0.1, 0.9, 0.0, 1.0, 0.02)
        for sx in (-1, 1):                                               # glacis armour plates
            for zr in d["rows"]:
                q = ((sx * 2.0, 0.85, zr - 0.72), (sx * 2.0, 0.85, zr + 0.72), (sx * 1.70, d["deck"], zr + 0.72), (sx * 1.70, d["deck"], zr - 0.72),
                     (0, 1.0, zr))
                at, n = face_frame(q)
                plate_on(b, ARM, at, 0.03, 0.97, 0.06, 0.94, 0.0, 0.04)
                bolt_rim(b, at, n, 0.03, 0.97, 0.09, 0.91, 0.04, 6, 5, 0.07)
    # hatches: 16 armoured cell lids on dark cell rims, hinge barrel outboard, hazard tab, tamper lock
    ht = P["vpu16_hatch"] = Part("vpu16_hatch")
    for cx in d["cols"]:
        so = 1 if cx > 0 else -1
        for zr in d["rows"]:
            rim = [(cx - d["cw"] / 2 - 0.05, zr - d["cl"] / 2 - 0.05), (cx + d["cw"] / 2 + 0.05, zr - d["cl"] / 2 - 0.05),
                   (cx + d["cw"] / 2 + 0.05, zr + d["cl"] / 2 + 0.05), (cx - d["cw"] / 2 - 0.05, zr + d["cl"] / 2 + 0.05)]
            lid = [(cx - d["cw"] / 2, zr - d["cl"] / 2), (cx + d["cw"] / 2, zr - d["cl"] / 2), (cx + d["cw"] / 2, zr + d["cl"] / 2),
                   (cx - d["cw"] / 2, zr + d["cl"] / 2)]
            if lod < 2:
                loft(ht, DRK, rim, d["deck"] - 0.01, rim, d["deck"] + 0.02)
            loft(ht, ARM, lid, d["deck"], inset(lid, 0.06), d["deck"] + 0.12)
            if lod < 2:
                hexz(ht, STL, 0.04, 0.04, zr - d["cl"] / 2 + 0.1, zr + d["cl"] / 2 - 0.1, cx + so * (d["cw"] / 2 + 0.02), d["deck"] + 0.04)
                box(ht, HAZ, (cx - so * 0.15, d["deck"] + 0.122, zr), (0.12, 0.006, 0.40))
                tlock(ht, (cx - so * 0.22, d["deck"] + 0.12, zr + d["cl"] / 2 - 0.22), (0, 1, 0), 0.10)
    # ignition: launch-circuit sequencer in an armoured box on the aft (-Z) end, locked hatch
    ig = P["vpu16_ignition"] = Part("vpu16_ignition")
    Ra = rot_x(-math.pi / 2)                                             # local +Y -> world -Z
    fbox(ig, ARM, (0, 1.15, -d["bz"]), (2.6, 1.5), (2.3, 1.2), d["hz"] - d["bz"], Ra)
    if lod < 2:
        at, n = local_frame(Ra, (0, 1.15, -d["hz"]), 2.3, 1.2)
        hatch_on(ig, at, n, 0.05, 0.62, 0.08, 0.92, lod)
        louvres(ig, at, 4, 0.70, 0.95, 0.10, 0.90, 0.025)
    # cell: cell thermal-conditioning units on both sides, one per row (louvred armour blocks)
    ce = P["vpu16_cell"] = Part("vpu16_cell")
    for sx in (-1, 1):
        Rs = rot_z(-sx * math.pi / 2)
        for zr in d["rows"]:
            fbox(ce, ARM, (sx * d["bx"], 0.48, zr), (0.62, 1.30), (0.50, 1.12), d["hx"] - d["bx"], Rs)
            if lod < 2:
                at, n = local_frame(Rs, (sx * d["hx"], 0.48, zr), 0.50, 1.12)
                louvres(ce, at, 5 if lod == 0 else 3, 0.1, 0.9, 0.06, 0.94, 0.02)
    return P


def vpu16_hits():
    d = VPU
    H = [hcyl("vpu16_flange", (0, 0, zz - 0.0), (0, 0.14, zz), 1.0) for zz in (-1.7, 1.7)]
    H += [hbox("vpu16_body", (0, (0.12 + d["deck"]) / 2, 0.06), (2 * d["bx"], d["deck"] - 0.12, 2 * d["bz"] + 0.12)),
          hbox("vpu16_body", (0, (d["deck"] + d["top"]) / 2, 0), (0.48, d["top"] - d["deck"], 6.44))]
    for cx in d["cols"]:
        for zr in d["rows"]:
            H.append(hbox("vpu16_hatch", (cx, d["deck"] + 0.06, zr), (d["cw"] + 0.1, 0.12, d["cl"] + 0.1)))
    H.append(hbox("vpu16_ignition", (0, 1.15, -(d["bz"] + d["hz"]) / 2), (2.6, 1.5, d["hz"] - d["bz"])))
    for sx in (-1, 1):
        for zr in d["rows"]:
            H.append(hbox("vpu16_cell", (sx * (d["bx"] + d["hx"]) / 2, 0.48, zr), (d["hx"] - d["bx"], 0.62, 1.30)))
    return H


ASSETS = {"bastion_bt1_armour": bt1_parts, "bastion_bs1_comm": bs1_parts, "bastion_bv1_computer": bv1_parts,
          "bastion_ap4_hangar": ap4_parts, "bastion_b12_core": b12_parts, "bastion_s50_radar": s50_parts,
          "bastion_bp10_coilgun": bp10_parts, "bastion_bt4_pd": bt4_parts, "bastion_vpu16_vls": vpu16_parts}
HITS = {"bastion_bt1_armour": bt1_hits, "bastion_bs1_comm": bs1_hits, "bastion_bv1_computer": bv1_hits,
        "bastion_ap4_hangar": ap4_hits, "bastion_b12_core": b12_hits, "bastion_s50_radar": s50_hits,
        "bastion_bp10_coilgun": bp10_hits, "bastion_bt4_pd": bt4_hits, "bastion_vpu16_vls": vpu16_hits}
MODEL_ASSETS = {"bastion.armour.heavy": "bastion_bt1_armour", "bastion.comm.w1": "bastion_bs1_comm",
                "bastion.mc.w1": "bastion_bv1_computer", "bastion.bay.hangar4": "bastion_ap4_hangar",
                "bastion.core.b12": "bastion_b12_core", "bastion.radar.s50": "bastion_s50_radar",
                "bastion.wpn.coil10": "bastion_bp10_coilgun", "bastion.wpn.pd4": "bastion_bt4_pd",
                "bastion.wpn.vls16": "bastion_vpu16_vls"}
