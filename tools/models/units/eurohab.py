"""Euro-Habitat consortium (GJ 581 New Europe; slow, careful, error-free): L-4 / L-20 / L-200 life support, MO-4 medbay.

School (DESIGN_SCHOOLS.md): over-engineered and redundant - every serviceable unit is a twin (train A / train B), white
rounded rectangles, many labelled access hatches (blue label plates, yellow train stripes), a blue skid on 6-bolt
hexagonal feet plus a redundant second (twin) connector. Close-up: twin harnesses, inspection tags.
Family: the three life-support sizes share the language (white rounded volumes, blue skid, hex feet, labels, yellow
train stripes) but not the layout - L-4 is one cabinet split A|B, L-20 two cabinets with exposed canister banks and a
bridge duct, L-200 a plant with tower / stack rows round a ventilation spine."""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_eurohab_white": ((0.85, 0.86, 0.86), (0.25, 0.25, 0.25), 50),
    "eq_eurohab_blue": ((0.12, 0.22, 0.48), (0.25, 0.25, 0.30), 60),
    "eq_eurohab_yellow": ((0.86, 0.72, 0.12), (0.20, 0.20, 0.15), 40),
    "eq_eurohab_grey": ((0.52, 0.55, 0.58), (0.30, 0.30, 0.30), 60),
    "eq_eurohab_dark": ((0.14, 0.15, 0.17), (0.10, 0.10, 0.10), 20),
    "eq_eurohab_glass": ((0.12, 0.17, 0.22), (0.70, 0.70, 0.70), 140),
})
WH, BL, YE, GR, DK, GL = ("eq_eurohab_white", "eq_eurohab_blue", "eq_eurohab_yellow", "eq_eurohab_grey",
                          "eq_eurohab_dark", "eq_eurohab_glass")

# ---------------------------------------------------------------- local helpers
PERM = {"z": I3, "x": [[0, 0, 1], [0, 1, 0], [1, 0, 0]], "y": [[1, 0, 0], [0, 0, 1], [0, 1, 0]]}
AXN = "xyz"


def rr_poly(hw, hh, rad, n):
    """counter-clockwise rounded rectangle (u, v) of half sizes hw / hh"""
    rad = min(rad, hw * 0.98, hh * 0.98)
    if rad <= 1e-6 or n < 1:
        return [(hw, -hh), (hw, hh), (-hw, hh), (-hw, -hh)]
    pts = []
    for (cu, cv), a0 in (((hw - rad, hh - rad), 0), ((-hw + rad, hh - rad), 90), ((-hw + rad, -hh + rad), 180), ((hw - rad, -hh + rad), 270)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cu + rad * math.cos(a), cv + rad * math.sin(a)))
    return pts


def rprism(part, mat, poly, w0, w1, axis="z", t=(0, 0, 0), soft=50.0, caps=(True, True)):
    """a prism over a convex (u, v) polygon from w0 to w1 along `axis` (z: (x, y); x: (z, y); y: (x, z));
    side corners flatter than `soft` degrees are smooth-shaded"""
    R = PERM[axis]
    n = len(poly)
    cu, cv = sum(p[0] for p in poly) / n, sum(p[1] for p in poly) / n
    en = []
    for i in range(n):
        (u0, v0), (u1, v1) = poly[i], poly[(i + 1) % n]
        L = math.hypot(u1 - u0, v1 - v0) or 1.0
        nu, nv = (v1 - v0) / L, -(u1 - u0) / L
        if ((u0 + u1) / 2 - cu) * nu + ((v0 + v1) / 2 - cv) * nv < 0:
            nu, nv = -nu, -nv
        en.append((nu, nv))
    cs = math.cos(math.radians(soft))

    def blend(a, b):
        if a[0] * b[0] + a[1] * b[1] < cs:
            return (a[0], a[1], 0.0)
        return norm((a[0] + b[0], a[1] + b[1], 0.0))

    for i in range(n):
        j = (i + 1) % n
        a = en[i]
        na, nb = blend(a, en[i - 1]), blend(a, en[j])
        ids = [part.vert((poly[i][0], poly[i][1], w0), na, R, t), part.vert((poly[j][0], poly[j][1], w0), nb, R, t),
               part.vert((poly[j][0], poly[j][1], w1), nb, R, t), part.vert((poly[i][0], poly[i][1], w1), na, R, t)]
        part.face(mat, ids, apply(R, (a[0], a[1], 0.0)))
    for k, w in ((0, w0), (1, w1)):
        if caps[k]:
            nn = (0.0, 0.0, 1.0 if (w > (w1 if k == 0 else w0)) else -1.0)
            part.face(mat, [part.vert((u, v, w), nn, R, t) for u, v in poly], apply(R, nn))


def rbox(part, mat, c, size, rad, axis="z", n=None, lod=0, caps=(True, True)):
    """a box with rounded edges along `axis` (the round corners lie in the plane across the axis)"""
    if n is None:
        n = {0: 4, 1: 2, 2: 1}[lod] + (2 if close() else 0)
    if rad < 0.03 and not close():
        n = 0
    a = AXN.index(axis)
    if axis == "z":
        hw, hh = size[0] / 2, size[1] / 2
    elif axis == "x":
        hw, hh = size[2] / 2, size[1] / 2
    else:
        hw, hh = size[0] / 2, size[2] / 2
    rprism(part, mat, rr_poly(hw, hh, rad, n), -size[a] / 2, size[a] / 2, axis, c, caps=caps)


def face_c(axis, sign, plane, cu, cv, t):
    u, v = [x for x in range(3) if x != axis]
    c = [0.0] * 3
    c[axis], c[u], c[v] = plane + sign * t / 2, cu, cv
    return c


def face_s(axis, su, sv, t):
    u, v = [x for x in range(3) if x != axis]
    s = [0.0] * 3
    s[axis], s[u], s[v] = t, su, sv
    return s


def hatch(p, axis, sign, plane, cu, cv, su, sv, lod, train=0, mat=WH, t=0.025):
    """a labelled access hatch on a face: rounded raised panel, blue label plate with yellow train stripe(s),
    two handles; close-up: quarter-turn fasteners and an inspection tag. (u, v) = the other axes in index order"""
    if lod >= 1:
        return
    rbox(p, mat, face_c(axis, sign, plane, cu, cv, t), face_s(axis, su, sv, t), min(su, sv) * 0.12, AXN[axis], lod=lod)
    horiz_u = axis != 0                                   # which face axis runs horizontally
    lw, lh = min(su, sv) * 0.34, min(su, sv) * 0.12
    lu = cu + (su / 2 - lw / 2 - 0.04 if horiz_u else su / 2 - lh / 2 - 0.04)
    lv = cv + (sv / 2 - lh / 2 - 0.04 if horiz_u else -(sv / 2 - lw / 2 - 0.04))
    ls = face_s(axis, lw if horiz_u else lh, lh if horiz_u else lw, 0.008)
    box(p, BL, tuple(face_c(axis, sign, plane + sign * t, lu, lv, 0.008)), tuple(ls))
    if lod == 0:
        for k in range(train + 1):                        # train A: one stripe, train B: two
            off = (k - train / 2) * lh * 0.5
            ss = face_s(axis, (lw * 0.8 if horiz_u else lh * 0.22), (lh * 0.22 if horiz_u else lw * 0.8), 0.006)
            cc = face_c(axis, sign, plane + sign * (t + 0.008), lu + (0 if horiz_u else off), lv + (off if horiz_u else 0), 0.006)
            box(p, YE, tuple(cc), tuple(ss))
        for k in (-1, 1):                                  # handles
            hu = cu + (k * su * 0.25 if horiz_u else -su * 0.30)
            hv = cv + (-sv * 0.30 if horiz_u else k * sv * 0.25)
            box(p, GR, tuple(face_c(axis, sign, plane + sign * t, hu, hv, 0.03)),
                tuple(face_s(axis, 0.12 if horiz_u else 0.03, 0.03 if horiz_u else 0.12, 0.03)))
    if close():
        u, v = [x for x in range(3) if x != axis]
        d = [0.0] * 3
        d[axis] = sign
        for ku in (-1, 1):
            for kv in (-1, 1):
                q = [0.0] * 3
                q[axis], q[u], q[v] = plane + sign * t, cu + ku * (su / 2 - 0.04), cv + kv * (sv / 2 - 0.04)
                lathe(p, GR, [(0.018, 0), (0.018, 0.008), (0, 0.008)], 8, axis_to(tuple(d)), tuple(q))
        q = face_c(axis, sign, plane + sign * t, cu + (-su * 0.30 if horiz_u else su * 0.10), cv + (sv * 0.05 if horiz_u else -sv * 0.30), 0.004)
        box(p, YE, tuple(q), tuple(face_s(axis, 0.04, 0.06, 0.004)))                 # inspection tag


def hexfoot(p, x, z, y0, r, lod):
    """the Euro-Habitat mount: a 6-bolt hexagonal foot plate"""
    ylathe(p, GR, [(0, y0), (r, y0), (r, y0 + 0.035), (0, y0 + 0.035)], 6, t=(x, 0, z), smooth=False)
    if lod == 0:
        for k in range(6):
            a = k * math.pi / 3 + math.pi / 6
            hex_bolt(p, "eq_steel", (x + 0.72 * r * math.cos(a), y0 + 0.035, z + 0.72 * r * math.sin(a)), (0, 1, 0),
                     max(0.06, r * 0.28) if not close() else r * 0.22, 0.02)


def twin_conn(p, c, axis, sign, lod, gap=0.24):
    """the redundant second connector: two identical blind-mate connectors A / B side by side"""
    small = small_for(lod)
    u = 0 if axis != 0 else 2
    for k, tr in ((-1, 0), (1, 1)):
        q = list(c)
        q[u] += k * gap / 2
        s = [0.16, 0.16, 0.16]
        s[axis] = 0.06
        q2 = list(q)
        q2[axis] += sign * 0.03
        box(p, GR, tuple(q2), tuple(s))
        if lod < 2:
            d = [0.0] * 3
            d[axis] = sign
            q3 = list(q)
            q3[axis] += sign * 0.06
            lathe(p, BL, [(0.05, 0), (0.05, 0.05), (0, 0.05)], small, axis_to(tuple(d)), tuple(q3))
        if lod == 0:
            q4 = list(q)
            q4[axis] += sign * 0.061
            q4[1] += 0.06
            s4 = [0.03 + 0.03 * tr, 0.012, 0.03 + 0.03 * tr]
            s4[axis] = 0.004
            box(p, YE, tuple(q4), tuple(s4))


def badge(p, c, axis, sign, r, lod):
    """the consortium badge: a ring of 12 yellow stars on a blue disc (relief)"""
    if lod != 0:
        return
    d = [0.0] * 3
    d[axis] = sign
    R = axis_to(tuple(d))
    lathe(p, BL, [(r, 0), (r, 0.006), (0, 0.006)], 16, R, c)
    for k in range(12):
        a = 2 * math.pi * k / 12
        q = apply(R, (0.72 * r * math.cos(a), 0.72 * r * math.sin(a), 0.009))
        box(p, YE, (c[0] + q[0], c[1] + q[1], c[2] + q[2]), (r * 0.13, r * 0.13, r * 0.13), R)


def xtank(p, mat, x0, x1, y, z, r, seg):
    """a horizontal tank with domed ends along X"""
    L = x1 - x0
    d = min(r * 0.45, L * 0.25)
    prof = [(0, 0), (r * 0.55, d * 0.18), (r * 0.88, d * 0.55), (r, d), (r, L - d), (r * 0.88, L - d * 0.55), (r * 0.55, L - d * 0.18), (0, L)]
    lathe(p, mat, prof, seg, axis_to((1, 0, 0)), (x0, y, z), soft=40)


def ytank(p, mat, x, z, y0, y1, r, seg):
    """a vertical tank / tower with a domed top"""
    d = r * 0.5
    ylathe(p, mat, [(0, y0), (r, y0), (r, y1 - d), (r * 0.88, y1 - d * 0.45), (r * 0.55, y1 - d * 0.1), (0, y1)], seg, t=(x, 0, z), soft=40)


def fan_disc(p, c, axis, sign, r, depth, lod, rim=BL):
    """round fan housing: rim, dark grille, hub; close: grille bars"""
    small = small_for(lod)
    seg = max(small * 2, 12) if lod < 2 else 8
    d = [0.0] * 3
    d[axis] = sign
    R = axis_to(tuple(d))
    lathe(p, rim, [(r, 0), (r, depth), (r - 0.05, depth), (r - 0.05, depth - 0.02)], seg, R, c)
    lathe(p, DK, [(r - 0.05, depth - 0.02), (0, depth - 0.02)], seg, R, c)
    if lod < 2:
        lathe(p, GR, [(r * 0.25, depth - 0.02), (r * 0.25, depth), (0, depth + 0.01)], small, R, c)
    if lod == 0:
        for k in range(3 if not close() else 6):
            a = math.pi * k / (3 if not close() else 6)
            q = apply(R, (0, 0, depth - 0.012))
            box(p, GR, (c[0] + q[0], c[1] + q[1], c[2] + q[2]), (2 * r - 0.1, 0.018, 0.018), mat_mul(R, rot_z(a)))


# ================================================================= L-4 (4 persons): one cabinet split A | B
L4 = dict(hx=1.108, hy=0.6335, hz=0.7915, cx=0.98, cz0=-0.45, cz1=0.65, ys=-0.5135, yt=0.567)


def l4_parts(lod):
    d = L4
    hy, hz, cx, z0, z1, ys, yt = d["hy"], d["hz"], d["cx"], d["cz0"], d["cz1"], d["ys"], d["yt"]
    seg, small = seg_for(lod), small_for(lod)
    zc = (z0 + z1) / 2
    P = {}
    fr = P["l4_frame"] = Part("l4_frame")
    rbox(fr, WH, (0, (ys + yt) / 2, zc), (2 * cx, yt - ys, z1 - z0), 0.12, "z", lod=lod)            # cabinet, rounded edges
    for z in (-0.30, 0.50):                                                                          # skid rails + hex feet
        box(fr, BL, (0, -hy + 0.035 + 0.0425, z), (2 * cx - 0.1, 0.085, 0.12))
        for x in (-0.80, 0.80):
            hexfoot(fr, x, z, -hy, 0.11, lod)
    rbox(fr, BL, (0, (ys + yt) / 2, z1 + 0.01), (0.12, yt - ys - 0.06, 0.03), 0.04, "z", lod=lod)    # A | B spine
    rbox(fr, BL, (0, yt + 0.005, zc), (0.12, 0.012, z1 - z0 - 0.1), 0.0, "y", lod=lod)
    for s, tr in ((-1, 0), (1, 1)):                                                                  # lower access hatches
        hatch(fr, 2, 1, z1, s * 0.50, ys + 0.24, 0.70, 0.30, lod, tr)
    twin_conn(fr, (0, yt - 0.12, z0), 2, -1, lod)
    badge(fr, (0, yt - 0.14, z1 + 0.025), 2, 1, 0.05, lod)

    sc = P["l4_scrubber"] = Part("l4_scrubber")                                                      # twin CO2 cartridges
    for s in (-1, 1):
        x, y = s * 0.50, 0.12
        rbox(sc, GR, (x, y, z1 + 0.015), (0.52, 0.52, 0.03), 0.08, "z", lod=lod)
        cyl(sc, WH, (x, y, z1 + 0.03), (x, y, z1 + 0.10), 0.19, seg)
        if lod < 2:
            lathe(sc, BL, crect(0.19, 0.205, z1 + 0.06, z1 + 0.09), seg, I3, (x, y, 0))
            box(sc, GR, (x, y, z1 + 0.115), (0.24, 0.04, 0.03))                                       # T-handle
        if lod == 0:
            box(sc, YE, (x + 0.16, y + 0.20, z1 + 0.033), (0.08 if s < 0 else 0.14, 0.03, 0.006))
        if close():
            for k in range(6):
                a = k * math.pi / 3
                hex_bolt(sc, "eq_steel", (x + 0.23 * math.cos(a), y + 0.23 * math.sin(a), z1 + 0.03), (0, 0, 1), 0.03, 0.012)

    o2 = P["l4_o2"] = Part("l4_o2")                                                                  # twin O2 generators on the ends
    for s, tr in ((-1, 0), (1, 1)):
        xo = s * (cx + 0.04)
        rbox(o2, WH, (xo, 0.02, zc), (0.08, 0.72, 0.82), 0.06, "x", lod=lod)
        hatch(o2, 0, s, xo + s * 0.04, 0.07, zc, 0.50, 0.60, lod, tr, mat=GR, t=0.012)
        if lod < 2:
            cyl(o2, GR, (xo, 0.38, zc - 0.25), (xo, yt - 0.02, zc - 0.25), 0.035, small)
    if close():
        for s in (-1, 1):
            harness(o2, sag((s * (cx + 0.10), -0.30, zc + 0.30), (s * (cx + 0.01), -0.40, z1 - 0.05), 0.06, 8), 0.012, 2, (BL, YE))

    wa = P["l4_water"] = Part("l4_water")                                                            # twin water reclaim tanks (back)
    for y in (-0.22, 0.18):
        xtank(wa, WH, -0.88, 0.88, y, -0.62, 0.165, seg)
        if lod < 2:
            for x in (-0.55, 0.0, 0.55):
                lathe(wa, BL, crect(0.165, 0.18, -0.025, 0.025), seg // 2, axis_to((1, 0, 0)), (x, y, -0.62))
    if lod < 2:
        cyl(wa, GR, (0.70, -0.22, -0.45), (0.70, 0.18, -0.45), 0.03, small)
    if close():
        for y in (-0.22, 0.18):
            lathe(wa, YE, crect(0.18, 0.185, -0.02, 0.02), 16, axis_to((1, 0, 0)), (-0.70, y, -0.62))

    fn = P["l4_fan"] = Part("l4_fan")                                                                # twin fans on top
    for s in (-1, 1):
        fan_disc(fn, (s * 0.50, yt, zc), 1, 1, 0.24, 0.06, lod)
    return P


def l4_hits():
    d = L4
    hy, cx, z0, z1, ys, yt = d["hy"], d["cx"], d["cz0"], d["cz1"], d["ys"], d["yt"]
    zc = (z0 + z1) / 2
    H = [hbox("l4_frame", (0, (ys + yt) / 2, zc), (2 * cx, yt - ys, z1 - z0)),
         hbox("l4_frame", (0, (-hy + ys) / 2, 0.10), (2 * cx, ys + hy, 1.0))]
    for s in (-1, 1):
        H.append(hcyl("l4_scrubber", (s * 0.50, 0.12, z1), (s * 0.50, 0.12, z1 + 0.13), 0.26))
        H.append(hbox("l4_o2", (s * (cx + 0.045), 0.02, zc), (0.09, 0.72, 0.82)))
        H.append(hcyl("l4_fan", (s * 0.50, yt, zc), (s * 0.50, yt + 0.067, zc), 0.24))
    for y in (-0.22, 0.18):
        H.append(hcyl("l4_water", (-0.88, y, -0.62), (0.88, y, -0.62), 0.17))
    return H


# ================================================================= L-20 (20 persons): two cabinets, canister banks, bridge duct
L20 = dict(hx=1.6825, hy=0.9615, hz=1.202, ys=-0.8015, cab_x=1.02, cab_w=1.20, cab_h=1.30, cab_z=-0.245, cab_d=1.69, can_z=1.0)


def l20_parts(lod):
    d = L20
    hx, hy, hz, ys = d["hx"], d["hy"], d["hz"], d["ys"]
    cxm, cw, ch, cz, cd = d["cab_x"], d["cab_w"], d["cab_h"], d["cab_z"], d["cab_d"]
    yt = ys + ch
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    fr = P["l20_frame"] = Part("l20_frame")
    for z in (-0.85, 0.85):                                                                          # skid + feet
        box(fr, BL, (0, (ys - hy + 0.035) / 2, z), (2 * hx - 0.08, ys + hy - 0.035, 0.16))
        for x in (-1.40, 0.0, 1.40):
            hexfoot(fr, x, z, -hy, 0.13, lod)
    for x in (-1.40, 1.40):
        box(fr, BL, (x, (ys - hy + 0.035) / 2, 0), (0.14, ys + hy - 0.035, 1.54))
    duct_y0, duct_y1 = yt, yt + 0.34
    for s, tr in ((-1, 0), (1, 1)):
        x = s * cxm
        rbox(fr, WH, (x, (ys + yt) / 2, cz), (cw, ch, cd), 0.14, "y", lod=lod)                      # cabinet A / B
        hatch(fr, 0, s, x + s * cw / 2, (ys + yt) / 2 + 0.05, cz - 0.33, 0.85, 0.55, lod, tr)        # outer-side hatches
        hatch(fr, 0, s, x + s * cw / 2, (ys + yt) / 2 + 0.05, cz + 0.33, 0.85, 0.55, lod, tr)
        twin_conn(fr, (x, ys + 0.45, cz - cd / 2), 2, -1, lod)
        if lod < 2:                                                                                  # canister cage
            for y in (ys + 0.30, ys + 0.85):
                box(fr, GR, (x, y, d["can_z"] + 0.17), (1.10, 0.04, 0.04))
            for xx in (x - 0.55, x + 0.55):
                box(fr, GR, (xx, ys + 0.55, d["can_z"] + 0.17), (0.05, 1.10, 0.05))
                if lod == 0:
                    box(fr, GR, (xx, ys + 0.55, (cz + cd / 2 + d["can_z"] + 0.17) / 2), (0.05, 0.05, d["can_z"] + 0.17 - cz - cd / 2))
    rbox(fr, WH, (0, (duct_y0 + duct_y1) / 2, -0.30), (2 * hx - 0.30, 0.34, 0.60), 0.12, "x", lod=lod)   # bridge duct
    if lod < 2:
        for x in (-1.2, 1.2):
            rbox(fr, BL, (x, (duct_y0 + duct_y1) / 2, -0.30), (0.06, 0.36, 0.62), 0.12, "x", lod=lod)
    badge(fr, (0, (duct_y0 + duct_y1) / 2, 0.0), 2, 1, 0.09, lod)

    sc = P["l20_scrubber"] = Part("l20_scrubber")                                                    # 4 canisters (twin per train)
    for s in (-1, 1):
        for xx in (0.72, 1.32):
            x = s * xx
            ytank(sc, WH, x, d["can_z"], ys, ys + 1.15, 0.15, seg if lod else 24)
            if lod < 2:
                ylathe(sc, BL, crect(0.15, 0.165, ys + 0.75, ys + 0.82), small * 2, t=(x, 0, d["can_z"]))
                cyl(sc, GR, (x, ys + 1.02, d["can_z"]), (x, ys + 1.02, cz + cd / 2), 0.04, small)
            if lod == 0:
                ylathe(sc, YE, crect(0.15, 0.16, ys + 0.62, ys + 0.66), 16, t=(x, 0, d["can_z"]))
    if close():
        for s in (-1, 1):
            for k, xx in enumerate((0.72, 1.32)):
                box(sc, YE, (s * xx + 0.1, ys + 0.5, d["can_z"] + 0.155), (0.05, 0.07, 0.004))

    o2 = P["l20_o2"] = Part("l20_o2")                                                                # O2 generators on the cabinet tops
    for s, tr in ((-1, 0), (1, 1)):
        x = s * cxm
        rbox(o2, WH, (x, yt + 0.13, 0.225), (0.90, 0.26, 0.40), 0.08, "y", lod=lod)
        hatch(o2, 2, 1, 0.425, x, yt + 0.13, 0.60, 0.18, lod, tr, mat=GR, t=0.012)
        if close():
            pipe_run(o2, GR, [(x + 0.3, yt + 0.20, 0.10), (x + 0.3, yt + 0.20, -0.05), (x + 0.3, yt + 0.24, -0.05)], 0.025)

    wa = P["l20_water"] = Part("l20_water")                                                          # twin water columns in the gap
    for z in (-0.65, 0.05):
        ytank(wa, WH, 0, z, ys, ys + 1.10, 0.28, seg)
        if lod < 2:
            for y in (ys + 0.25, ys + 0.70):
                ylathe(wa, BL, crect(0.28, 0.295, y - 0.03, y + 0.03), seg, t=(0, 0, z))
    if lod < 2:
        cyl(wa, GR, (0, ys + 0.15, -0.37), (0, ys + 0.15, -0.23), 0.05, small)
    if close():
        for z in (-0.65, 0.05):
            hatch(wa, 2, 1, z + 0.28, 0, ys + 0.45, 0.20, 0.24, 0, 0 if z < 0 else 1, mat=GR, t=0.01)

    fn = P["l20_fan"] = Part("l20_fan")                                                              # twin fans on the bridge duct
    for s in (-1, 1):
        fan_disc(fn, (s * 0.45, duct_y1, -0.30), 1, 1, 0.26, 0.10, lod)
    if close():
        for s in (-1, 1):
            harness(fn, bezier((s * 0.75, duct_y1 + 0.02, -0.30), (s * 1.0, duct_y1 + 0.1, -0.30), (s * 1.3, duct_y1, -0.1),
                               (s * 1.4, yt + 0.02, 0.10), 10), 0.012, 2, (BL, YE))
    return P


def l20_hits():
    d = L20
    hx, hy, ys, cxm, cw, ch, cz, cd = d["hx"], d["hy"], d["ys"], d["cab_x"], d["cab_w"], d["cab_h"], d["cab_z"], d["cab_d"]
    yt = ys + ch
    H = [hbox("l20_frame", (0, (ys - hy) / 2, 0), (2 * hx, ys + hy, 1.86)),
         hbox("l20_frame", (0, yt + 0.17, -0.30), (2 * hx - 0.30, 0.34, 0.60))]
    for s in (-1, 1):
        H.append(hbox("l20_frame", (s * cxm, (ys + yt) / 2, cz), (cw + 0.06, ch, cd)))
        H.append(hbox("l20_o2", (s * cxm, yt + 0.13, 0.225), (0.90, 0.26, 0.42)))
        H.append(hcyl("l20_fan", (s * 0.45, yt + 0.34, -0.30), (s * 0.45, yt + 0.44, -0.30), 0.26))
        for xx in (0.72, 1.32):
            H.append(hcyl("l20_scrubber", (s * xx, ys, d["can_z"]), (s * xx, ys + 1.15, d["can_z"]), 0.16))
    for z in (-0.65, 0.05):
        H.append(hcyl("l20_water", (0, ys, z), (0, ys + 1.10, z), 0.28))
    return H


# ================================================================= L-200 (liner plant): rows round a ventilation spine
L200 = dict(hx=3.1285, hy=1.788, hz=2.235, ys=-1.568, sp_y=1.30, sp_h=0.80, sp_d=1.0, sp_x=2.55)
L200_X = (-2.35, -0.95, 0.95, 2.35)


def l200_parts(lod):
    d = L200
    hx, hy, hz, ys = d["hx"], d["hy"], d["hz"], d["ys"]
    spy, sph, spd, spx = d["sp_y"], d["sp_h"], d["sp_d"], d["sp_x"]
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    fr = P["l200_frame"] = Part("l200_frame")
    for z in (-1.90, 1.90):                                                                          # skid + 8 hex feet
        box(fr, BL, (0, (ys - hy + 0.04) / 2, z), (2 * hx - 0.1, ys + hy - 0.04, 0.30))
        for x in (-2.80, -0.95, 0.95, 2.80):
            hexfoot(fr, x, z, -hy, 0.17, lod)
    for x in (-2.80, -0.95, 0.95, 2.80):
        box(fr, BL, (x, (ys - hy + 0.04) / 2, 0), (0.22, ys + hy - 0.04, 3.5))
    box(fr, DK, (0, ys - 0.02, 0), (2 * hx - 0.6, 0.04, 1.2))                                       # walkway grating
    rbox(fr, WH, (0, spy, 0), (2 * spx, sph, spd), 0.22, "x", lod=lod)                              # ventilation spine
    if lod < 2:
        for x in (-1.65, 0.0, 1.65) if lod == 0 else (0.0,):
            rbox(fr, BL, (x, spy, 0), (0.08, sph + 0.04, spd + 0.04), 0.24, "x", lod=lod)
        for s in (-1, 1):
            for x in (-1.65, 1.65):
                rbox(fr, GR, (x, (ys + spy - sph / 2) / 2, s * 0.62), (0.20, spy - sph / 2 - ys, 0.20), 0.05, "y", lod=lod)
        for x in (-1.45, 1.45):                                                                      # drum saddles
            for s in (-1, 1):
                box(fr, GR, (x + s * 0.55, ys + 0.09, 0), (0.12, 0.18, 0.80))
        hatch(fr, 2, 1, spd / 2, -0.85, spy, 0.80, 0.50, lod, 0)
        hatch(fr, 2, 1, spd / 2, 0.85, spy, 0.80, 0.50, lod, 1)
    twin_conn(fr, (0, ys + 0.30, 2.05), 2, 1, lod, gap=0.30)
    badge(fr, (0, spy, spd / 2 + 0.002), 2, 1, 0.16, lod)
    badge(fr, (0, spy, -spd / 2 - 0.002), 2, -1, 0.16, lod)

    sc = P["l200_scrubber"] = Part("l200_scrubber")                                                  # 4 CO2 towers (front row)
    for k, x in enumerate(L200_X):
        ytank(sc, WH, x, 1.70, ys, 1.05, 0.48, seg)
        if lod < 2:
            for y in (ys + 0.45, 0.25):
                ylathe(sc, BL, crect(0.48, 0.50, y - 0.05, y + 0.05), seg, t=(x, 0, 1.70))
            cyl(sc, GR, (x, 0.95, 1.70), (x, 0.95, 1.25), 0.06, small)
            cyl(sc, GR, (x, 0.95, 1.25), (x, spy - 0.10, 1.25), 0.06, small)
            cyl(sc, WH, (x, spy - 0.1, 1.25), (x, spy - 0.1, spd / 2 - 0.02), 0.14, small * 2)
            hatch(sc, 2, 1, 2.18, x, -0.40, 0.42, 0.62, lod, k // 2)
        if lod == 0:
            ylathe(sc, YE, crect(0.48, 0.49, ys + 0.28, ys + 0.31 + 0.04 * (k // 2)), seg, t=(x, 0, 1.70))

    o2 = P["l200_o2"] = Part("l200_o2")                                                              # 4 electrolysis stacks (back row)
    sy1 = 0.53
    for k, x in enumerate(L200_X):
        rbox(o2, WH, (x, (ys + sy1) / 2, -1.70), (1.05, sy1 - ys, 1.0), 0.12, "y", lod=lod)
        if lod < 2:
            n = 7 if lod == 0 else 0
            for i in range(n):
                y = ys + 0.25 + i * (sy1 - ys - 0.45) / (n - 1)
                rbox(o2, GR if i % 2 else BL, (x, y, -1.70), (1.09, 0.035, 1.04), 0.13, "y", lod=lod)
            hatch(o2, 2, -1, -2.20, x, 0.20, 0.70, 0.40, lod, k // 2)
            cyl(o2, GR, (x, sy1, -1.50), (x, spy - 0.1, -1.50), 0.07, small)
            cyl(o2, GR, (x, spy - 0.1, -1.50), (x, spy - 0.1, -spd / 2 + 0.02), 0.07, small)
        if close():
            harness(o2, sag((x - 0.45, sy1 - 0.1, -1.04), (x + 0.45, sy1 - 0.1, -1.04), 0.12, 10), 0.015, 2, (BL, YE))

    wa = P["l200_water"] = Part("l200_water")                                                        # twin water drums under the spine
    for s, tr in ((-1, 0), (1, 1)):
        x0, x1 = (-2.65, -0.25) if s < 0 else (0.25, 2.65)
        xtank(wa, WH, x0, x1, -0.95, 0.0, 0.50, seg)
        if lod < 2:
            for x in (x0 + 0.6, x1 - 0.6) if lod == 0 else ():
                lathe(wa, BL, crect(0.50, 0.52, -0.04, 0.04), seg, axis_to((1, 0, 0)), (x, -0.95, 0.0))
            hatch(wa, 2, 1, 0.48, (x0 + x1) / 2, -0.95, 0.50, 0.36, lod, tr, mat=GR, t=0.02)
    if close():
        pipe_run(wa, GR, [(-0.25, -0.75, 0.0), (0.0, -0.75, 0.0), (0.25, -0.75, 0.0)], 0.04)

    fn = P["l200_fan"] = Part("l200_fan")                                                            # twin fans at the spine ends
    for s in (-1, 1):
        x = s * spx
        lathe(fn, WH, [(0.46, 0), (0.46, 0.44)], seg, axis_to((s, 0, 0)), (x, spy, 0))
        fan_disc(fn, (x + s * 0.44, spy, 0), 0, s, 0.46, 0.12, lod)
        if lod < 2:
            lathe(fn, BL, crect(0.46, 0.49, 0.05, 0.15), seg, axis_to((s, 0, 0)), (x, spy, 0))
        if lod == 0:
            box(fn, YE, (x + s * 0.2, spy + 0.47, 0.0), (0.12 if s < 0 else 0.20, 0.006, 0.05))
    return P


def l200_hits():
    d = L200
    hx, hy, ys, spy, sph, spd, spx = d["hx"], d["hy"], d["ys"], d["sp_y"], d["sp_h"], d["sp_d"], d["sp_x"]
    H = [hbox("l200_frame", (0, (ys - hy) / 2, 0), (2 * hx, ys + hy, 4.10)),
         hbox("l200_frame", (0, spy, 0), (2 * spx, sph, spd))]
    for x in L200_X:
        H.append(hcyl("l200_scrubber", (x, ys, 1.70), (x, 1.05, 1.70), 0.50))
        H.append(hbox("l200_scrubber", (x, spy - 0.1, 0.875), (0.30, 0.30, 0.75)))
        H.append(hbox("l200_o2", (x, (ys + 0.53) / 2, -1.70), (1.09, 0.53 - ys, 1.04)))
    for x0, x1 in ((-2.65, -0.25), (0.25, 2.65)):
        H.append(hcyl("l200_water", (x0, -0.95, 0), (x1, -0.95, 0), 0.50))
    for s in (-1, 1):
        H.append(hcyl("l200_fan", (s * spx, spy, 0), (s * (spx + 0.56), spy, 0), 0.49))
    return H


# ================================================================= MO-4 medbay (std.med.bay): shell + twin autodocs + twin sterilisers
MO = dict(hx=1.738, hy=0.869, hz=1.2415, ys=-0.729, sx=1.60, sh=1.36, sd=2.40, rad=0.35)


def mo_parts(lod):
    d = MO
    hx, hy, hz, ys, sx, sh, sd = d["hx"], d["hy"], d["hz"], d["ys"], d["sx"], d["sh"], d["sd"]
    yc, yt = ys + sh / 2, ys + sh
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    fr = P["mo4_frame"] = Part("mo4_frame")
    for z in (-0.75, 0.75):
        box(fr, BL, (0, (ys - hy + 0.035) / 2, z), (2 * sx - 0.2, ys + hy - 0.035, 0.16))
        for x in (-1.30, 1.30):
            hexfoot(fr, x, z, -hy, 0.12, lod)
    twin_conn(fr, (-0.9, ys + 0.12, -sd / 2 + 0.07), 2, -1, lod)

    s = P["mo4_shell"] = Part("mo4_shell")
    rbox(s, WH, (0, yc, 0), (2 * sx, sh, sd), d["rad"], "x", lod=lod)                               # pressure shell
    if lod < 2:
        for x in (-sx + 0.06, -0.55, 0.55, sx - 0.06):                                              # blue frame rings
            rbox(s, BL, (x, yc, 0), (0.08, sh + 0.03, sd + 0.03), d["rad"] + 0.015, "x", lod=lod)
        for x in (-1.08, 1.08):                                                                      # windows (+Z)
            rbox(s, GL, (x, yc + 0.12, sd / 2 + 0.008), (0.55, 0.40, 0.02), 0.08, "z", lod=lod)
            if lod == 0:
                rbox(s, WH, (x, yc + 0.12, sd / 2 + 0.004), (0.66, 0.51, 0.02), 0.10, "z", lod=lod)
        box(s, BL, (0, yc + 0.05, sd / 2 + 0.01), (0.46, 0.14, 0.02))                                # medical cross
        box(s, BL, (0, yc + 0.05, sd / 2 + 0.01), (0.14, 0.46, 0.02))
        for k, x in enumerate((-1.08, 0.0, 1.08)):                                                   # service hatches (-Z)
            hatch(s, 2, -1, -sd / 2, x, yc - 0.05, 0.62, 0.70, lod, k % 2)
        # main door (+X end) with window and seal
        rbox(s, DK, (sx + 0.01, yc - 0.05, 0), (0.02, 1.08, 0.86), 0.16, "x", lod=lod)
        rbox(s, WH, (sx + 0.03, yc - 0.05, 0), (0.03, 1.0, 0.78), 0.14, "x", lod=lod)
        rbox(s, GL, (sx + 0.05, yc + 0.20, 0), (0.012, 0.26, 0.30), 0.06, "x", lod=lod)
        box(s, GR, (sx + 0.07, yc - 0.15, 0.30), (0.04, 0.20, 0.04))
        hatch(s, 0, 1, sx + 0.045, yc - 0.40, -0.18, 0.18, 0.26, lod, 0, mat=BL, t=0.008)
    badge(s, (0.0, yc + 0.40, sd / 2 + 0.002), 2, 1, 0.10, lod)
    if close():
        for x in (-0.55, 0.55):
            harness(s, [(x + 0.06, yt - 0.01, -0.4 + 0.2 * i) for i in range(5)], 0.012, 2, (BL, YE))

    ad = P["mo4_autodoc"] = Part("mo4_autodoc")                                                      # twin autodoc cores on the roof
    for k, x in enumerate((-0.70, 0.70)):
        rbox(ad, WH, (x, yt + 0.09, 0), (0.95, 0.18, 0.95), 0.12, "y", lod=lod)
        rbox(ad, BL, (x, yt + 0.005, 0), (1.0, 0.03, 1.0), 0.13, "y", lod=lod)
        hatch(ad, 1, 1, yt + 0.18, x, 0.0, 0.62, 0.62, lod, k, mat=GR, t=0.015)
        if lod < 2:
            for z in (-0.38, 0.38):
                cyl(ad, GR, (x + 0.40, yt + 0.06, z), (x + 0.40, yt + 0.16, z), 0.04, small)
    if close():
        for x in (-0.70, 0.70):
            harness(ad, sag((x + 0.40, yt + 0.16, -0.38), (x + 0.40, yt + 0.16, 0.38), -0.05, 10), 0.012, 2, (BL, YE))

    st = P["mo4_steril"] = Part("mo4_steril")                                                        # twin sterilisers on the -X end
    for k, y in enumerate((yc + 0.30, yc - 0.32)):
        x = -sx - 0.065
        rbox(st, WH, (x, y, 0), (0.13, 0.52, 0.86), 0.08, "x", lod=lod)
        if lod < 2:
            rbox(st, GL, (x - 0.07, y + 0.06, -0.18), (0.012, 0.22, 0.34), 0.04, "x", lod=lod)        # UV window
            for i in range(5):
                box(st, DK, (x - 0.068, y - 0.02 - 0.12 + 0.06 * i, 0.22), (0.01, 0.025, 0.30))        # vent grille
        if lod == 0:
            box(st, BL, (x - 0.068, y + 0.18, 0.22), (0.006, 0.07, 0.30))
            for i in range(k + 1):
                box(st, YE, (x - 0.072, y + 0.18, 0.12 + 0.07 * i), (0.004, 0.04, 0.04))
    if close():
        for z in (-0.33, 0.33):
            pipe_run(st, GR, [(-sx - 0.06, yc + 0.04, z), (-sx - 0.06, yc - 0.06, z)], 0.02)
    return P


def mo_hits():
    d = MO
    hy, ys, sx, sh, sd = d["hy"], d["ys"], d["sx"], d["sh"], d["sd"]
    yc, yt = ys + sh / 2, ys + sh
    H = [hbox("mo4_shell", (0, yc, 0), (2 * sx + 0.1, sh, sd)),
         hbox("mo4_frame", (0, (ys - hy) / 2, 0), (2 * sx - 0.2, ys + hy, 1.66))]
    for x in (-0.70, 0.70):
        H.append(hbox("mo4_autodoc", (x, yt + 0.09, 0), (1.0, 0.18, 1.0)))
    for y in (yc + 0.30, yc - 0.32):
        H.append(hbox("mo4_steril", (-sx - 0.065, y, 0), (0.14, 0.52, 0.86)))
    return H


ASSETS = {"eurohab_l4_lifesupport": l4_parts, "eurohab_l20_lifesupport": l20_parts,
          "eurohab_l200_lifesupport": l200_parts, "eurohab_mo4_medbay": mo_parts}
HITS = {"eurohab_l4_lifesupport": l4_hits, "eurohab_l20_lifesupport": l20_hits,
        "eurohab_l200_lifesupport": l200_hits, "eurohab_mo4_medbay": mo_hits}
MODEL_ASSETS = {"eurohab.ls.l4": "eurohab_l4_lifesupport", "eurohab.ls.l20": "eurohab_l20_lifesupport",
                "eurohab.ls.l200": "eurohab_l200_lifesupport", "std.med.bay": "eurohab_mo4_medbay"}
