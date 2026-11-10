"""Jump-field emitters (author 2026-11-10, ACCEPTED: "emitters on the wingtips"). The jump core makes the field; the
emitters on the hull extremities (Cobra: wingtips; big hulls: 4-8 at the extremities) shape it around the whole hull -
this is the "hull volume" term of the jump-charge rule (JUMP_MECHANICS.md). Every core maker ships its own emitter,
matched to its cores (keyed to the core's field signature, so emitters of another maker do not work with the core):

  ZP-FE1  "Zero Point" (Tau Ceti)   - faceted near-black octagonal spindle, violet engraved rings, sealed pylon with a
                                      keyed socket and red tamper straps (no serviceable panels);
  He-FE8  Harmony Yards (Eps Eri)   - a rod carrying eight jade lacquered discs (mandala order) with gold rims, cream
                                      end caps, round 16-bolt flange on a round pylon;
  MJ-FE2  Bastion (Wolf 359)        - armoured hexagonal pod with wedge ends, emitter slots recessed behind armour,
                                      hazard-yellow bands, armoured wedge pylon (deliberately incompatible mount);
  K-FE1   Kapehu Systems (Lalande)  - an outrigger float (ama) on two curved struts (iako) with lashing bands, a teal
                                      emitter strip along the float.

Frame: the pod axis is Z (symmetric fore / aft), the mount face is the plane x = 0 (the wingtip), the pod lies on +X.
The left wingtip takes the unit yawed by 180 degrees (the pods are fore / aft symmetric).
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_fe_zp_black": ((0.03, 0.03, 0.04), (0.35, 0.35, 0.40), 150),
    "eq_fe_zp_violet": ((0.30, 0.18, 0.45), (0.30, 0.25, 0.40), 60),
    "eq_fe_zp_red": ((0.55, 0.12, 0.12), (0.30, 0.25, 0.25), 50),
    "eq_fe_zp_socket": ((0.16, 0.15, 0.18), (0.50, 0.50, 0.55), 120),
    "eq_fe_he_jade": ((0.24, 0.42, 0.36), (0.50, 0.55, 0.50), 120),
    "eq_fe_he_cream": ((0.84, 0.80, 0.68), (0.30, 0.30, 0.28), 60),
    "eq_fe_he_gold": ((0.72, 0.56, 0.22), (0.80, 0.70, 0.40), 200),
    "eq_fe_bn_armour": ((0.22, 0.25, 0.28), (0.20, 0.20, 0.22), 30),
    "eq_fe_bn_dark": ((0.12, 0.13, 0.15), (0.15, 0.15, 0.16), 20),
    "eq_fe_bn_hazard": ((0.75, 0.62, 0.12), (0.20, 0.20, 0.15), 30),
    "eq_fe_kp_wood": ((0.42, 0.28, 0.18), (0.20, 0.18, 0.15), 30),
    "eq_fe_kp_teal": ((0.12, 0.42, 0.42), (0.40, 0.50, 0.50), 90),
    "eq_fe_kp_bone": ((0.86, 0.82, 0.72), (0.20, 0.20, 0.20), 25),
})


def _zloft(part, mat, rings_z):
    """an extrusion along Z through (poly (x, y), z) rings with end caps (convex rings around the local axis)"""
    rings = [[(x, y, z) for x, y in poly] for poly, z in rings_z]
    for a, b in zip(rings, rings[1:]):
        n = len(a)
        cx = sum(p[0] for p in a) / n
        cy = sum(p[1] for p in a) / n
        for i in range(n):
            j = (i + 1) % n
            q = [a[i], a[j], b[j], b[i]]
            fn = facet_normal(q)
            if fn is None:
                continue
            mx, my = sum(p[0] for p in q) / 4 - cx, sum(p[1] for p in q) / 4 - cy
            if fn[0] * mx + fn[1] * my < 0:
                fn = (-fn[0], -fn[1], -fn[2])
            part.face(mat, [part.vert(p, fn) for p in q], fn)
    for ring, s in ((rings[0], -1), (rings[-1], 1)):
        nn = (0, 0, s)
        part.face(mat, [part.vert(p, nn) for p in ring], nn)


def _ngon(cx, cy, r, k, a0=0.0):
    return [(cx + r * math.cos(a0 + 2 * math.pi * i / k), cy + r * math.sin(a0 + 2 * math.pi * i / k)) for i in range(k)]


def _ring_z(part, mat, xc, z, r, w, t, seg):
    """a thin band ring around the Z axis through (xc, 0): radius r, width w along Z, radial thickness t"""
    lathe(part, mat, crect(r - t, r, z - w / 2, z + w / 2), seg, t=(xc, 0, 0))


# ---------------------------------------------------------------- ZP-FE1: faceted octagonal spindle
ZP = dict(L=3.6, r=0.45, xc=0.78)


def zp_parts(lod):
    L, r, xc = ZP["L"], ZP["r"], ZP["xc"]
    P = {}
    e = P["zpfe_emitter"] = Part("zpfe_emitter")
    h = L / 2
    prof = [(0.0, -h), (0.20, -h + 0.16), (r * 0.93, -0.95), (r, 0.0), (r * 0.93, 0.95), (0.20, h - 0.16), (0.0, h)]
    lathe(e, "eq_fe_zp_black", prof, 8, t=(xc, 0, 0), smooth=False)          # 8 facets in every LOD (the monolith look)
    if lod < 2:
        for z in (-1.15, -0.55, 0.55, 1.15):
            rr = r * (0.93 + 0.07 * (1 - abs(z) / 0.95)) if abs(z) < 0.95 else 0.20 + (r * 0.93 - 0.20) * (h - 0.16 - abs(z)) / (h - 0.16 - 0.95)
            lathe(e, "eq_fe_zp_violet", crect(rr - 0.01, rr + 0.006, z - 0.02, z + 0.02), 8, t=(xc, 0, 0), smooth=False)
    py = P["zpfe_pylon"] = Part("zpfe_pylon")
    x1 = xc - r * 0.75
    box(py, "eq_fe_zp_black", (x1 / 2 + 0.01, 0, 0), (x1, 0.20, 1.50), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        for z in (-0.45, 0.45):
            box(py, "eq_fe_zp_red", (x1 / 2 + 0.01, 0, z), (x1 + 0.004, 0.206, 0.06))
    mt = P["zpfe_mount"] = Part("zpfe_mount")
    box(mt, "eq_fe_zp_socket", (0.03, 0, 0), (0.06, 0.34, 1.70), chamfer=0.015 if lod == 0 else 0.0)
    if lod < 2:
        for z, w in ((-0.62, 0.10), (0.20, 0.16), (0.66, 0.08)):                # asymmetric key lugs
            box(mt, "eq_fe_zp_socket", (0.075, 0.13, z), (0.03, 0.06, w))
    return P


def zp_hits():
    L, r, xc = ZP["L"], ZP["r"], ZP["xc"]
    x1 = xc - r * 0.75
    return [hcyl("zpfe_emitter", (xc, 0, -L / 2 + 0.3), (xc, 0, L / 2 - 0.3), r),
            hbox("zpfe_pylon", (x1 / 2 + 0.01, 0, 0), (x1, 0.20, 1.50)),
            hbox("zpfe_mount", (0.03, 0, 0), (0.06, 0.34, 1.70))]


# ---------------------------------------------------------------- He-FE8: rod with eight lacquered discs
HE = dict(L=4.2, rd=0.55, rod=0.17, xc=0.90)


def he_parts(lod):
    L, rd, rod, xc = HE["L"], HE["rd"], HE["rod"], HE["xc"]
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    e = P["hefe_emitter"] = Part("hefe_emitter")
    h = L / 2
    cyl(e, "eq_fe_he_jade", (xc, 0, -h + 0.30), (xc, 0, h - 0.30), rod, small)
    for s in (-1, 1):                                                           # cream end caps (rounded)
        prof = [(0.0, 0.0), (0.26, 0.0), (0.24, 0.14), (0.16, 0.25), (0.0, 0.30)]
        R = I3 if s > 0 else rot_x(math.pi)
        lathe(e, "eq_fe_he_cream", prof, small, R=R, t=(xc, 0, s * (h - 0.30)), soft=40)
    for i in range(8):
        z = -1.40 + i * 0.40
        lathe(e, "eq_fe_he_jade", crect(rod, rd, z - 0.06, z + 0.06, 0.02 if lod == 0 else 0.0), seg, t=(xc, 0, 0))
        if lod < 2:
            _ring_z(e, "eq_fe_he_gold", xc, z, rd + 0.008, 0.05, 0.012, seg)
    py = P["hefe_pylon"] = Part("hefe_pylon")
    cyl(py, "eq_fe_he_cream", (0.08, 0, 0), (xc - rod, 0, 0), 0.20, small)
    mt = P["hefe_mount"] = Part("hefe_mount")
    cyl(mt, "eq_fe_he_gold", (0.0, 0, 0), (0.08, 0, 0), 0.42, seg)
    if lod == 0 and min_bolt() <= 0.05:
        for k in range(8):                                                      # 16 bolts in 8 pairs
            for d in (-0.09, 0.09):
                a = 2 * math.pi * k / 8 + d
                hex_bolt(mt, "eq_fe_he_cream", (0.08, 0.34 * math.cos(a), 0.34 * math.sin(a)), (1, 0, 0), 0.05, 0.03)
    return P


def he_hits():
    L, rd, rod, xc = HE["L"], HE["rd"], HE["rod"], HE["xc"]
    return [hcyl("hefe_emitter", (xc, 0, -1.50), (xc, 0, 1.50), rd),
            hcyl("hefe_emitter", (xc, 0, -L / 2), (xc, 0, L / 2), 0.26),
            hcyl("hefe_pylon", (0.08, 0, 0), (xc - rod, 0, 0), 0.20),
            hcyl("hefe_mount", (0.0, 0, 0), (0.08, 0, 0), 0.42)]


# ---------------------------------------------------------------- MJ-FE2: armoured hexagonal pod with wedge ends
BN = dict(L=3.4, r=0.50, xc=0.80)


def bn_parts(lod):
    L, r, xc = BN["L"], BN["r"], BN["xc"]
    h = L / 2
    P = {}
    e = P["mjfe_emitter"] = Part("mjfe_emitter")
    hexr = _ngon(xc, 0, r, 6, math.pi / 6)
    tip = _ngon(xc, -0.10, r * 0.40, 6, math.pi / 6)                            # wedge ends drop towards the belly
    _zloft(e, "eq_fe_bn_armour", [(tip, -h), (hexr, -h + 0.65), (hexr, h - 0.65), (tip, h)])
    if lod < 2:
        for s in (-1, 1):                                                       # recessed emitter slots on the flanks
            for z in (-0.6, 0.0, 0.6):
                box(e, "eq_fe_bn_dark", (xc + s * r * 0.866 * 0.98, 0, z), (0.02, 0.30, 0.42))
        for z in (-h + 0.72, h - 0.72):                                         # hazard bands
            for i in range(6):
                a0, a1 = math.pi / 6 + i * math.pi / 3, math.pi / 6 + (i + 1) * math.pi / 3
                p0 = (xc + r * math.cos(a0), r * math.sin(a0))
                p1 = (xc + r * math.cos(a1), r * math.sin(a1))
                mx, my = (p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2
                nx, ny = mx - xc, my
                L2 = math.hypot(nx, ny)
                R = rot_z(math.atan2(ny, nx))
                box(e, "eq_fe_bn_hazard", (mx + nx / L2 * 0.004, my + ny / L2 * 0.004, z), (0.006, r * 1.0 + 0.004, 0.08), R)
    py = P["mjfe_pylon"] = Part("mjfe_pylon")
    x1 = xc - r * 0.80
    wedge = [(0.02, -0.16), (0.02, 0.16), (x1 + 0.05, 0.10), (x1 + 0.05, -0.10)]
    _zloft(py, "eq_fe_bn_armour", [(wedge, -0.85), (wedge, 0.85)])
    mt = P["mjfe_mount"] = Part("mjfe_mount")
    hexm = _ngon(0.0, 0.0, 0.36, 6, 0.0)
    for k in range(6):                                                          # hexagonal flange with interlocking teeth
        a = math.pi / 3 * k
        box(mt, "eq_fe_bn_dark", (0.04, 0.30 * math.cos(a), 0.30 * math.sin(a) * 2.3), (0.08, 0.16, 0.20), rot_x(-a))
    box(mt, "eq_fe_bn_dark", (0.03, 0, 0), (0.06, 0.40, 1.60))
    del hexm
    return P


def bn_hits():
    L, r, xc = BN["L"], BN["r"], BN["xc"]
    x1 = xc - r * 0.80
    return [hbox("mjfe_emitter", (xc, 0, 0), (2 * r * 0.866 * 1.0, 2 * r, L - 1.0)),
            hbox("mjfe_emitter", (xc, -0.05, 0), (r * 1.2, r * 1.2, L)),
            hbox("mjfe_pylon", ((x1 + 0.07) / 2, 0, 0), (x1 + 0.03, 0.32, 1.70)),
            hbox("mjfe_mount", (0.04, 0, 0), (0.08, 0.76, 1.70))]


# ---------------------------------------------------------------- K-FE1: outrigger float on two curved struts
KP = dict(L=4.6, r=0.30, xc=1.05, zs=1.15)


def kp_parts(lod):
    L, r, xc, zs = KP["L"], KP["r"], KP["xc"], KP["zs"]
    seg, small = seg_for(lod), small_for(lod)
    h = L / 2
    P = {}
    e = P["kpfe_emitter"] = Part("kpfe_emitter")
    n = 10 if lod < 2 else 6
    prof = [(0.0, -h)]
    for i in range(1, n):                                                       # canoe float: sine profile, pointed ends
        z = -h + L * i / n
        prof.append((r * math.sin(math.pi * i / n) ** 0.6, z))
    prof.append((0.0, h))
    lathe(e, "eq_fe_kp_wood", prof, seg, t=(xc, 0, 0), soft=30)
    if lod < 2:
        box(e, "eq_fe_kp_teal", (xc, r * 0.97, 0), (0.08, 0.03, L * 0.62))     # emitter strip along the top
        box(e, "eq_fe_kp_teal", (xc, -r * 0.97, 0), (0.08, 0.03, L * 0.62))
    st = P["kpfe_pylon"] = Part("kpfe_pylon")
    for s in (-1, 1):                                                           # iako struts: curved, rising to the float
        pts = bezier((0.06, 0.0, s * zs * 0.85), (0.40, 0.22, s * zs * 0.95), (xc - 0.30, 0.25, s * zs), (xc, 0.10, s * zs),
                     8 if lod < 2 else 4)
        tube(st, "eq_fe_kp_wood", pts, 0.075, small)
        if lod < 2:                                                             # lashing bands where the strut meets the float
            _ring_z(st, "eq_fe_kp_bone", xc, s * zs + 0.10, r * 0.99, 0.05, 0.02, small)
            _ring_z(st, "eq_fe_kp_bone", xc, s * zs - 0.10, r * 0.99, 0.05, 0.02, small)
    mt = P["kpfe_mount"] = Part("kpfe_mount")
    for s in (-1, 1):                                                           # lashing ring with toggles at each root
        cyl(mt, "eq_fe_kp_bone", (0.0, 0, s * zs * 0.85), (0.08, 0, s * zs * 0.85), 0.16, small)
        if lod == 0:
            for k in range(8 if close() else 4):
                a = 2 * math.pi * k / (8 if close() else 4)
                box(mt, "eq_fe_kp_teal", (0.09, 0.13 * math.cos(a), s * zs * 0.85 + 0.13 * math.sin(a)), (0.02, 0.03, 0.03))
    return P


def kp_hits():
    L, r, xc, zs = KP["L"], KP["r"], KP["xc"], KP["zs"]
    H = [hcyl("kpfe_emitter", (xc, 0, -L / 2 + 0.6), (xc, 0, L / 2 - 0.6), r)]
    for s in (-1, 1):
        H.append(hbox("kpfe_pylon", ((xc + 0.06) / 2, 0.14, s * zs * 0.93), (xc - 0.06, 0.30, 0.18)))
        H.append(hcyl("kpfe_mount", (0.0, 0, s * zs * 0.85), (0.08, 0, s * zs * 0.85), 0.16))
    return H


ASSETS = {"zeropoint_zpfe1_field_emitter": zp_parts, "harmony_hefe8_field_emitter": he_parts,
          "bastion_mjfe2_field_emitter": bn_parts, "kapehu_kfe1_field_emitter": kp_parts}
HITS = {"zeropoint_zpfe1_field_emitter": zp_hits, "harmony_hefe8_field_emitter": he_hits,
        "bastion_mjfe2_field_emitter": bn_hits, "kapehu_kfe1_field_emitter": kp_hits}
MODEL_ASSETS = {"um.jfe.zp1": "zeropoint_zpfe1_field_emitter", "harmony.jfe.he8": "harmony_hefe8_field_emitter",
                "bastion.jfe.mj2": "bastion_mjfe2_field_emitter", "kapehu.jfe.k1": "kapehu_kfe1_field_emitter"}
