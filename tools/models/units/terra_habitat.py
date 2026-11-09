"""Terra Habitat (Sol; Earth consumer quality, "museum Earth"): T-10, T-60, T-200 life-support plants.

School (DESIGN_SCHOOLS.md): domestic, comfortable, rounded - capsule-like pillow boxes with soft corners, inspection
windows on every serviceable unit, warm beige with sage service faces. Interface standard: a sage base plate on 4
rubber-isolated feet. Close-up: hoses, quick couplings, soft grommets.
Family: the same pillow cabinet / sage drawer / round fan / window vocabulary; the layouts differ with the size class:
T-10 one cabinet with three front drawers and a roof fan; T-60 a two-tier plant (drawers below, water capsule and twin
fans above); T-200 a plinth with three towers (scrubber column bank, O2 tower, water tank) under a roof air duct.
life_support SRUs: scrubber, o2, water, fan.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_terra_beige": ((0.80, 0.74, 0.62), (0.18, 0.18, 0.16), 35),
    "eq_terra_sage": ((0.35, 0.48, 0.38), (0.15, 0.15, 0.15), 30),
    "eq_terra_rubber": ((0.10, 0.10, 0.10), (0.05, 0.05, 0.05), 8),
    "eq_terra_glass": ((0.12, 0.20, 0.24), (0.80, 0.80, 0.80), 160),
    "eq_terra_cream": ((0.90, 0.88, 0.82), (0.20, 0.20, 0.20), 40),
})
BG, SG, RB, GL, CR = "eq_terra_beige", "eq_terra_sage", "eq_terra_rubber", "eq_terra_glass", "eq_terra_cream"


# ---------------------------------------------------------------- local helpers
def nc_for(lod):
    """corner segments of the rounded sections"""
    return 5 if close() else {0: 3, 1: 2, 2: 1}[lod]


def rr_pts(w, h, rad, n):
    out = []
    for (cx, cy), a0 in (((w / 2 - rad, -(h / 2 - rad)), -90), ((w / 2 - rad, h / 2 - rad), 0),
                         ((-(w / 2 - rad), h / 2 - rad), 90), ((-(w / 2 - rad), -(h / 2 - rad)), 180)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            out.append((cx + rad * math.cos(a), cy + rad * math.sin(a), math.cos(a), math.sin(a)))
    return out


def loft(part, mat, secs, n, R=I3, t=(0, 0, 0), caps=(True, True)):
    """rounded-rectangle sections (w, h, rad, z), z ascending, lofted along local Z with shared (soft) normals"""
    secs = [(w, h, max(1e-4, min(rad, w / 2 - 1e-4, h / 2 - 1e-4)), z) for w, h, rad, z in secs]
    rings = [rr_pts(w, h, rad, n) for w, h, rad, z in secs]
    slopes = [-((secs[k + 1][0] - secs[k][0]) + (secs[k + 1][1] - secs[k][1])) / 4 / max(secs[k + 1][3] - secs[k][3], 1e-6)
              for k in range(len(secs) - 1)]
    ids = []
    for k, ring in enumerate(rings):
        sl = [slopes[j] for j in (k - 1, k) if 0 <= j < len(slopes)]
        s = sum(sl) / len(sl)
        z = secs[k][3]
        ids.append([part.vert((x, y, z), norm((nx, ny, s)), R, t) for x, y, nx, ny in ring])
    for k in range(len(rings) - 1):
        a, b, s = ids[k], ids[k + 1], slopes[k]
        m = len(a)
        for i in range(m):
            j = (i + 1) % m
            nx = rings[k][i][2] + rings[k][j][2]
            ny = rings[k][i][3] + rings[k][j][3]
            part.face(mat, [a[i], a[j], b[j], b[i]], apply(R, norm((nx, ny, 2 * s))))
    for idx, sgn, on in ((0, -1, caps[0]), (-1, 1, caps[1])):
        if on:
            w, h, rad, z = secs[idx]
            part.face(mat, [part.vert((x, y, z), (0, 0, sgn), R, t) for x, y, _, _ in rings[idx]], apply(R, (0, 0, sgn)))


def frame_for(axis):
    """rotation taking the loft local frame onto a world axis; returns (R, index of the w, h, L world sizes)"""
    if axis == "x":
        return rot_y(math.pi / 2), (2, 1, 0)
    if axis == "y":
        return rot_x(-math.pi / 2), (0, 2, 1)
    return I3, (0, 1, 2)


def pillow(part, mat, c, size, rad, edge, lod, axis="z", ends=(True, True)):
    """a soft box: rounded-rect section (corner rad) along `axis`, both ends rounded over by `edge`"""
    R, (iw, ih, il) = frame_for(axis)
    w, h, L = size[iw], size[ih], size[il]
    m = 1 if lod == 2 else (2 if lod == 1 else (3 if not close() else 4))
    e = min(edge, L / 2 - 1e-3, w / 2 - 1e-3, h / 2 - 1e-3) if edge > 0 else 0.0
    secs = []
    if e > 0 and ends[0]:
        for j in range(m + 1):
            ph = math.pi / 2 * j / m
            ins = e * (1 - math.sin(ph))
            secs.append((w - 2 * ins, h - 2 * ins, rad - ins * 0.7, -L / 2 + e * (1 - math.cos(ph))))
    else:
        secs.append((w, h, rad, -L / 2))
    if e > 0 and ends[1]:
        for j in range(m, -1, -1):
            ph = math.pi / 2 * j / m
            ins = e * (1 - math.sin(ph))
            secs.append((w - 2 * ins, h - 2 * ins, rad - ins * 0.7, L / 2 - e * (1 - math.cos(ph))))
    else:
        secs.append((w, h, rad, L / 2))
    loft(part, mat, secs, nc_for(lod), R, c)


def window(part, c, nrm, w, h, lod, rad=0.08):
    """inspection window: sage frame + glass, on a face with outward normal nrm (an axis)"""
    if lod == 2:
        return
    R = axis_to(nrm)
    loft(part, SG, [(w + 0.10, h + 0.10, rad + 0.05, 0.0), (w + 0.10, h + 0.10, rad + 0.05, 0.025)], nc_for(lod), R, c, (False, True))
    loft(part, GL, [(w, h, rad, 0.0), (w, h, rad, 0.035)], nc_for(lod), R, c, (False, True))


def round_window(part, c, nrm, r, lod):
    if lod == 2 or (lod == 1 and r < 0.15):
        return
    s = small_for(lod) * 2
    R = axis_to(nrm)
    lathe(part, SG, [(r, 0), (r + 0.06, 0), (r + 0.06, 0.02), (r + 0.03, 0.035), (r, 0.035)], s, R, c)
    lathe(part, GL, [(r, 0.025), (r * 0.5, 0.04), (0, 0.045)], s, R, c, soft=60)


def fan(part, c, r, lod):
    """round roof fan (axis +Y): housing with a soft lip, dark intake, concentric grille rings, spokes and hub"""
    s = (seg_for(lod) if r > 0.35 else small_for(lod) * 2) if lod == 0 else (12 if lod == 1 else 8)
    R = R_Y
    lathe(part, BG, [(r, 0), (r, 0.10), (r - 0.03, 0.15), (r - 0.08, 0.16), (r - 0.10, 0.14)], s, R, c, soft=50)
    lathe(part, RB, [(r - 0.10, 0.10), (0, 0.10)], s, R, c)                                    # dark intake
    if lod < 2:
        for k in range(3 if lod == 0 else 1):
            rr = (r - 0.12) * (k + 1) / 3.4
            lathe(part, SG, crect(rr - 0.012, rr + 0.012, 0.10, 0.135), s, R, c)
        lathe(part, SG, [(0, 0.10), (0.10, 0.10), (0.10, 0.14), (0.06, 0.16), (0, 0.16)], small_for(lod), R, c, soft=50)
        for k in range(6 if lod == 0 else 0):
            a = math.pi * k / 6
            box(part, SG, (c[0], c[1] + 0.12, c[2]), (2 * (r - 0.12), 0.02, 0.025), rot_y(a))


def coupling(part, p, d, lod):
    """quick coupling: sage collar + steel nipple, a soft grommet where it leaves the shell"""
    if lod == 2:
        return
    s = small_for(lod)
    R = axis_to(d)
    lathe(part, RB, [(0.075, 0), (0.075, 0.03), (0.05, 0.04)], s, R, p)                        # grommet
    lathe(part, SG, [(0.05, 0.03), (0.05, 0.10), (0.065, 0.10), (0.065, 0.16), (0.04, 0.18), (0, 0.18)], s, R, p)
    if close():
        lathe(part, "eq_steel", crect(0.066, 0.072, 0.12, 0.14), s, R, p)


def base(P, p, lod, w, l, y0, feet):
    """the Terra interface: sage base plate (soft corners) on 4 rubber-isolated feet; y0 = underside"""
    m = P[p + "_mounts"] = Part(p + "_mounts")
    for x, z in feet:
        if lod > 0:
            cyl(m, RB, (x, y0, z), (x, y0 + 0.16, z), 0.13, 8 if lod == 1 else 5, caps=False)
            continue
        cyl(m, "eq_steel", (x, y0, z), (x, y0 + 0.02, z), 0.15, small_for(lod))                   # foot plate
        lathe(m, RB, [(0.13, 0), (0.13, 0.05), (0.11, 0.07), (0.11, 0.09), (0.13, 0.11), (0.13, 0.14)], small_for(lod) * 2 if lod < 2 else 6,
              R_Y, (x, y0 + 0.02, z), soft=60)                                                       # rubber isolator
        if lod == 0:
            hex_bolt(m, "eq_steel", (x + 0.11, y0 + 0.02, z), (0, 1, 0), 0.06, 0.03)
            hex_bolt(m, "eq_steel", (x - 0.11, y0 + 0.02, z), (0, 1, 0), 0.06, 0.03)
    pillow(m, SG, (0, y0 + 0.16 + 0.04, 0), (w, 0.08, l), 0.25, 0.03, lod, "y")
    return y0 + 0.24                                                                                # top of the plate


def hose(part, pts, r=0.04):
    tube(part, RB, pts, r, 10)
    for p, q in ((pts[0], pts[1]), (pts[-1], pts[-2])):
        d = norm(tuple(q[k] - p[k] for k in range(3)))
        lathe(part, SG, crect(r, r + 0.02, 0.0, 0.07), 10, axis_to(d), p)


# ---------------------------------------------------------------- T-10 (10 persons, 60 days)
# Envelope 2.793 x 1.596 x 1.995 (centred): one pillow cabinet on the plate, three sage drawers on the +Z service face
# (scrubber, O2 generator, water recovery) each with an inspection window and a pull, the fan on the roof.
T10 = dict(W=2.793, H=1.596, L=1.995)


def t10_parts(lod):
    W_, H, L = T10["W"], T10["H"], T10["L"]
    y0 = -H / 2
    P = {}
    yp = base(P, "t10", lod, W_ - 0.05, L - 0.15, y0, [(sx * (W_ / 2 - 0.35), sz * (L / 2 - 0.35)) for sx in (-1, 1) for sz in (-1, 1)])
    cab = P["t10_cabinet"] = Part("t10_cabinet")
    ch = 1.12
    cc = (0, yp + ch / 2, -0.10)
    pillow(cab, BG, cc, (W_ - 0.10, ch, L - 0.45), 0.22, 0.10, lod, "z")
    if lod < 2:
        for x in (-0.95, 0.95):                                                          # soft vertical seams
            pillow(cab, CR, (x * 1.0, cc[1], cc[2]), (0.04, ch + 0.02, L - 0.47), 0.015, 0.0, lod, "z")
        for x in (-1.15, -0.85):                                                         # supply / return couplings
            coupling(cab, (x, yp + ch, cc[2] - 0.55), (0, 1, 0), lod)
    zf = cc[2] + (L - 0.45) / 2
    units = (("t10_scrubber", -0.90), ("t10_o2", 0.0), ("t10_water", 0.90))
    for name, x in units:
        u = P[name] = Part(name)
        pillow(u, SG, (x, cc[1], zf + 0.10), (0.80, ch - 0.16, 0.24), 0.14, 0.06, lod, "z", (False, True))
        if name == "t10_water":
            round_window(u, (x, cc[1] + 0.18, zf + 0.22), (0, 0, 1), 0.16, lod)
        else:
            window(u, (x, cc[1] + 0.18, zf + 0.22), (0, 0, 1), 0.46, 0.28, lod)
        if lod < 2:
            capsule_bar(u, (x - 0.22, cc[1] - 0.25, zf + 0.25), (x + 0.22, cc[1] - 0.25, zf + 0.25), 0.03, lod)
            box(u, "eq_status_amber" if name == "t10_o2" else SG, (x + 0.28, cc[1] + 0.40, zf + 0.225), (0.06, 0.04, 0.01))
    fn = P["t10_fan"] = Part("t10_fan")
    fan(fn, (0.75, yp + ch - 0.02, -0.25), 0.36, lod)
    if lod < 2:
        pillow(fn, BG, (-0.55, yp + ch + 0.04, -0.25), (0.7, 0.10, 0.5), 0.1, 0.04, lod, "y")        # exhaust louvre box
        for k in range(4):
            box(fn, SG, (-0.55, yp + ch + 0.095, -0.4 + 0.1 * k), (0.55, 0.012, 0.035))
    if close():
        hz = P["t10_hoses"] = Part("t10_hoses")
        ry = yp + ch + 0.18
        for x in (-1.15, -0.85):                                                         # hoses over the back to the plate
            hose(hz, bezier((x, ry, cc[2] - 0.55), (x, ry + 0.15, cc[2] - 0.80), (x, ry - 0.1, -L / 2 + 0.05),
                            (x, yp + 0.05, -L / 2 + 0.06), 12), 0.035)
    return P


def capsule_bar(part, a, b, r, lod):
    """a soft pull handle: bar with two stand-offs"""
    cyl(part, CR, a, b, r, small_for(lod))
    for p in (a, b):
        cyl(part, CR, p, (p[0], p[1], p[2] - 0.05), r * 0.8, small_for(lod), caps=False)


def t10_hits():
    W_, H, L = T10["W"], T10["H"], T10["L"]
    y0 = -H / 2
    yp = y0 + 0.24
    ch = 1.12
    cc = (0, yp + ch / 2, -0.10)
    zf = cc[2] + (L - 0.45) / 2
    Hs = [hbox("t10_mounts", (0, y0 + 0.12, 0), (W_ - 0.05, 0.24, L - 0.15)),
          hbox("t10_cabinet", cc, (W_ - 0.10, ch, L - 0.45))]
    for name, x in (("t10_scrubber", -0.90), ("t10_o2", 0.0), ("t10_water", 0.90)):
        Hs.append(hbox(name, (x, cc[1], zf + 0.13), (0.80, ch - 0.16, 0.26)))
    Hs += [hcyl("t10_fan", (0.75, yp + ch - 0.02, -0.25), (0.75, yp + ch + 0.14, -0.25), 0.36),
           hbox("t10_fan", (-0.55, yp + ch + 0.04, -0.25), (0.7, 0.12, 0.5))]
    return Hs


# ---------------------------------------------------------------- T-60 (60 persons, fleet)
# Envelope 4.929 x 2.817 x 3.521 (centred): lower pillow cabinet with the scrubber and O2 drawers on the +Z face and a
# sage service strip between them, a horizontal water capsule along X on the roof (rear) with a round inspection window
# and level band, twin roof fans in front of it.
T60 = dict(W=4.929, H=2.817, L=3.521)


def t60_parts(lod):
    W_, H, L = T60["W"], T60["H"], T60["L"]
    y0 = -H / 2
    P = {}
    yp = base(P, "t60", lod, W_ - 0.05, L - 0.10, y0, [(sx * (W_ / 2 - 0.45), sz * (L / 2 - 0.45)) for sx in (-1, 1) for sz in (-1, 1)])
    cab = P["t60_cabinet"] = Part("t60_cabinet")
    ch = 1.42
    cc = (0, yp + ch / 2, -0.12)
    cl = L - 0.40
    pillow(cab, BG, cc, (W_ - 0.12, ch, cl), 0.30, 0.14, lod, "z")
    zf = cc[2] + cl / 2
    if lod < 2:
        pillow(cab, SG, (0, cc[1], zf + 0.03), (0.70, ch - 0.30, 0.10), 0.10, 0.03, lod, "z", (False, True))   # service strip
        for k in range(3):
            box(cab, "eq_status_amber" if k == 0 else CR, (0, cc[1] + 0.35 - 0.18 * k, zf + 0.085), (0.30, 0.06, 0.01))
        for x in (2.05, 2.25, -2.05, -2.25):                                            # roof couplings (rear corners)
            coupling(cab, (x, yp + ch, cc[2] - 0.95), (0, 1, 0), lod)
    for name, x in (("t60_scrubber", -1.30), ("t60_o2", 1.30)):
        u = P[name] = Part(name)
        pillow(u, SG, (x, cc[1], zf + 0.12), (1.70, ch - 0.22, 0.26), 0.18, 0.07, lod, "z", (False, True))
        window(u, (x - 0.25, cc[1] + 0.15, zf + 0.25), (0, 0, 1), 0.70, 0.50, lod)
        if lod < 2:
            capsule_bar(u, (x + 0.42, cc[1] - 0.30, zf + 0.28), (x + 0.42, cc[1] + 0.30, zf + 0.28), 0.035, lod)
            for k in range(4):                                                     # cartridge slots (scrubber) / cells (O2)
                box(u, CR, (x - 0.55 + 0.20 * k, cc[1] - 0.38, zf + 0.255), (0.12, 0.14, 0.01))
    wa = P["t60_water"] = Part("t60_water")                                         # water capsule on the roof (rear)
    rw = 0.56
    wy = yp + ch + rw + 0.02
    wz = -L / 2 + 0.80
    pillow(wa, BG, (0, wy, wz), (3.6, 2 * rw, 2 * rw), rw - 0.01, rw - 0.02, lod, "x")
    if lod < 2:
        for x in (-1.2, 1.2):                                                          # saddles
            pillow(wa, SG, (x, yp + ch + 0.12, wz), (0.22, 0.24, 0.9), 0.06, 0.03, lod, "x")
        lathe(wa, SG, crect(rw - 0.02, rw + 0.025, -0.08, 0.08), seg_for(lod), axis_to((1, 0, 0)), (0.6, wy, wz))   # level band
        round_window(wa, (-0.6, wy, wz + rw - 0.02), (0, 0, 1), 0.17, lod)
        coupling(wa, (1.80, wy, wz), (1, 0, 0), lod)
    fn = P["t60_fan"] = Part("t60_fan")
    for x in (-0.95, 0.95):
        fan(fn, (x, yp + ch - 0.03, 0.75), 0.48, lod)
    if close():
        hz = P["t60_hoses"] = Part("t60_hoses")
        hose(hz, bezier((1.98, wy, wz), (2.25, wy, wz), (2.05, yp + ch + 0.45, cc[2] - 0.95), (2.05, yp + ch + 0.18, cc[2] - 0.95), 14))
        hose(hz, bezier((-2.25, yp + ch + 0.18, cc[2] - 0.95), (-2.25, yp + ch + 0.5, cc[2] - 0.7), (-1.9, wy + 0.2, wz + 0.3),
                        (-1.8, wy, wz), 14))
        hose(hz, bezier((2.25, yp + ch + 0.18, cc[2] - 0.95), (2.3, yp + ch + 0.4, cc[2] - 1.3), (2.3, yp + 0.6, -L / 2 + 0.06),
                        (2.2, yp + 0.05, -L / 2 + 0.08), 14))
    return P


def t60_hits():
    W_, H, L = T60["W"], T60["H"], T60["L"]
    y0 = -H / 2
    yp = y0 + 0.24
    ch = 1.42
    cc = (0, yp + ch / 2, -0.12)
    zf = cc[2] + (L - 0.40) / 2
    rw = 0.56
    return [hbox("t60_mounts", (0, y0 + 0.12, 0), (W_ - 0.05, 0.24, L - 0.10)),
            hbox("t60_cabinet", cc, (W_ - 0.12, ch, L - 0.40)),
            hbox("t60_scrubber", (-1.30, cc[1], zf + 0.14), (1.70, ch - 0.22, 0.28)),
            hbox("t60_o2", (1.30, cc[1], zf + 0.14), (1.70, ch - 0.22, 0.28)),
            hbox("t60_water", (0, yp + ch + rw + 0.02, -L / 2 + 0.80), (3.6, 2 * rw, 2 * rw)),
            hcyl("t60_fan", (-0.95, yp + ch - 0.03, 0.75), (-0.95, yp + ch + 0.13, 0.75), 0.48),
            hcyl("t60_fan", (0.95, yp + ch - 0.03, 0.75), (0.95, yp + ch + 0.13, 0.75), 0.48)]


# ---------------------------------------------------------------- T-200 (200 persons, fleet)
# Envelope 7.25 x 4.143 x 5.179 (centred): a pillow plinth on the plate carrying three towers - the CO2 scrubber as a
# bank of 3 x 2 capsule columns (left), the O2 electrolyser tower with a window per cell stack (centre), the water
# processing tank (right, big vertical capsule with a round window and level band) - and a roof air duct along X with
# three fans that ties them together.
T200 = dict(W=7.25, H=4.143, L=5.179)


def t200_parts(lod):
    W_, H, L = T200["W"], T200["H"], T200["L"]
    y0 = -H / 2
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    yp = base(P, "t200", lod, W_ - 0.05, L - 0.10, y0,
              [(sx * (W_ / 2 - 0.55), sz * (L / 2 - 0.55)) for sx in (-1, 1) for sz in (-1, 1)])
    cab = P["t200_cabinet"] = Part("t200_cabinet")                                  # plinth (pumps, controls)
    ph = 0.55
    pillow(cab, BG, (0, yp + ph / 2, 0), (W_ - 0.15, ph, L - 0.25), 0.35, 0.12, lod, "z")
    yt = yp + ph                                                                     # tower floor
    if lod < 2:
        for x in (-2.6, 0.0, 2.6):
            box(cab, CR, (x, yp + ph / 2, (L - 0.25) / 2 + 0.002), (1.4, 0.20, 0.01))
            box(cab, "eq_status_amber", (x + 0.55, yp + ph / 2, (L - 0.25) / 2 + 0.006), (0.12, 0.08, 0.01))
    th = 2.78                                                                        # tower height
    sc = P["t200_scrubber"] = Part("t200_scrubber")                                 # 3 x 2 capsule column bank
    for i in range(3):
        for j in range(2):
            x, z = -2.95 + 0.62 * i, -0.65 + 1.3 * j
            pillow(sc, BG if (i + j) % 2 else CR, (x, yt + th / 2, z), (0.56, th, 0.56), 0.27, 0.26, lod, "y")
            if lod == 0:
                lathe(sc, SG, crect(0.28, 0.30, -0.06, 0.06), small * 2, R_Y, (x, yt + th * 0.62, z))
                if j == 1:
                    round_window(sc, (x, yt + th * 0.45, z + 0.27), (0, 0, 1), 0.10, lod)
    if lod < 2:
        pillow(sc, SG, (-2.33, yt + 0.12, 0.0), (1.9, 0.24, 2.0), 0.15, 0.05, lod, "y")
    o2 = P["t200_o2"] = Part("t200_o2")                                              # O2 electrolyser tower
    oc = (-0.15, yt + th / 2, 0.15)
    pillow(o2, BG, oc, (1.9, th, 2.3), 0.40, 0.18, lod, "y")
    pillow(o2, SG, (oc[0], oc[1], oc[2] + 1.15), (1.5, th - 0.6, 0.12), 0.25, 0.04, lod, "z", (False, True))
    for k in range(3):
        window(o2, (oc[0], oc[1] + 0.85 - 0.85 * k, oc[2] + 1.21), (0, 0, 1), 1.0, 0.40, lod)
    if lod < 2:
        capsule_bar(o2, (oc[0] + 0.62, oc[1] - 0.4, oc[2] + 1.26), (oc[0] + 0.62, oc[1] + 0.4, oc[2] + 1.26), 0.04, lod)
    wa = P["t200_water"] = Part("t200_water")                                        # water processing tank
    rw = 1.12
    wc = (2.35, yt + th / 2 - 0.05, -0.15)
    pillow(wa, BG, wc, (2 * rw, th - 0.1, 2 * rw), rw - 0.01, 0.55, lod, "y")
    if lod < 2:
        lathe(wa, SG, crect(rw - 0.02, rw + 0.03, -0.09, 0.09), seg, R_Y, (wc[0], wc[1] + 0.35, wc[2]))
        lathe(wa, SG, crect(rw - 0.02, rw + 0.03, -0.09, 0.09), seg, R_Y, (wc[0], wc[1] - 0.75, wc[2]))
        round_window(wa, (wc[0], wc[1] - 0.2, wc[2] + rw - 0.01), (0, 0, 1), 0.22, lod)
        round_window(wa, (wc[0] + rw - 0.01, wc[1] - 0.2, wc[2]), (1, 0, 0), 0.22, lod)
        pillow(wa, SG, (wc[0] + 0.0, yt + 0.12, wc[2]), (2.4, 0.24, 2.4), 0.6, 0.05, lod, "y")
    fn = P["t200_fan"] = Part("t200_fan")                                            # roof air duct + 3 fans
    dy = yt + th + 0.24
    pillow(fn, SG, (-0.30, dy, -0.10), (6.4, 0.42, 1.10), 0.20, 0.18, lod, "x")
    for x in (-2.33, -0.15, 2.35):
        fan(fn, (x, dy + 0.18, -0.10), 0.42, lod)
    pillow(fn, SG, (-2.33, yt + th - 0.12, 0.0), (1.95, 0.26, 2.0), 0.22, 0.08, lod, "y")          # scrubber bank header
    for x, y_lo in ((-2.33, yt + th), (-0.15, yt + th - 0.10), (2.35, yt + th - 0.60)):           # risers into the towers
        pillow(fn, BG, (x, (y_lo + dy) / 2, -0.10), (0.50, dy - y_lo, 0.60), 0.12, 0.0, lod, "y")
    if close():
        hz = P["t200_hoses"] = Part("t200_hoses")
        for k, (a, b) in enumerate((((wc[0] - rw * 0.7, yt + 0.6, wc[2] + rw * 0.7), (oc[0] + 0.95, yt + 0.6, oc[2] + 0.5)),
                                    ((oc[0] - 0.95, yt + 0.8, oc[2] - 0.5), (-1.71, yt + 0.8, -0.65)))):
            mid = ((a[0] + b[0]) / 2, a[1] - 0.25, (a[2] + b[2]) / 2 + 0.3)
            hose(hz, bezier(a, (a[0], a[1] - 0.2, a[2] + 0.2), mid, b, 14), 0.05)
        for x in (-2.95, -2.33, -1.71):
            coupling(hz, (x, yt + 0.25, 0.65 + 0.28), (0, 0, 1), 0)
    return P


def t200_hits():
    W_, H, L = T200["W"], T200["H"], T200["L"]
    y0 = -H / 2
    yp = y0 + 0.24
    ph, th = 0.55, 2.78
    yt = yp + ph
    rw = 1.12
    Hs = [hbox("t200_mounts", (0, y0 + 0.12, 0), (W_ - 0.05, 0.24, L - 0.10)),
          hbox("t200_cabinet", (0, yp + ph / 2, 0), (W_ - 0.15, ph, L - 0.25))]
    for i in range(3):
        for j in range(2):
            Hs.append(hcyl("t200_scrubber", (-2.95 + 0.62 * i, yt, -0.65 + 1.3 * j), (-2.95 + 0.62 * i, yt + th, -0.65 + 1.3 * j), 0.28))
    Hs += [hbox("t200_o2", (-0.15, yt + th / 2, 0.15), (1.9, th, 2.3)),
           hbox("t200_o2", (-0.15, yt + th / 2, 1.33), (1.5, th - 0.6, 0.12)),
           hcyl("t200_water", (2.35, yt, -0.15), (2.35, yt + th - 0.1, -0.15), rw + 0.03),
           hbox("t200_fan", (-0.30, yt + th + 0.24, -0.10), (6.4, 0.42, 1.10))]
    for x in (-2.33, -0.15, 2.35):
        Hs.append(hcyl("t200_fan", (x, yt + th + 0.42, -0.10), (x, yt + th + 0.58, -0.10), 0.42))
    return Hs


ASSETS = {"terra_t10_life_support": t10_parts, "terra_t60_life_support": t60_parts, "terra_t200_life_support": t200_parts}
HITS = {"terra_t10_life_support": t10_hits, "terra_t60_life_support": t60_hits, "terra_t200_life_support": t200_hits}
MODEL_ASSETS = {"terra.ls.t10": "terra_t10_life_support", "terra.ls.t60": "terra_t60_life_support",
                "terra.ls.t200": "terra_t200_life_support"}
