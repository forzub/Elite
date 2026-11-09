"""Harmony Yards (Eps Eri, "harmony and order", the Pact's largest state concern): fighter hangar AP-2, licensed jump
cores H-5 / H-12 / H-80 (sealed Cognium blocks in Harmony's own housing), jump-train modules MP-4800 (cargo) and
MC-4200 (tank), PT-3 point-defence laser turret, VPU-8 vertical launcher.

School (DESIGN_SCHOOLS.md): refined mass production, concentric / 8-fold mandala symmetry, rounded octagons, lacquered
cream and jade panels with gold trim. Interface: round flange with 16 bolts in 8 pairs. Close-up: symmetric cable
looms, decorative end caps.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_harmony_jade": ((0.24, 0.42, 0.36), (0.40, 0.42, 0.40), 90),
    "eq_harmony_cream": ((0.84, 0.80, 0.68), (0.30, 0.30, 0.28), 70),
    "eq_harmony_gold": ((0.72, 0.56, 0.22), (0.75, 0.62, 0.35), 140),
    "eq_harmony_dark": ((0.07, 0.08, 0.08), (0.15, 0.15, 0.15), 30),
    "eq_harmony_lens": ((0.06, 0.10, 0.16), (0.85, 0.85, 0.90), 200),
    # the sealed Cognium ("Zero Point") core blocks assembled under licence: their maker's own finish
    "eq_harmony_cognium": ((0.03, 0.03, 0.04), (0.30, 0.30, 0.32), 110),
    "eq_harmony_cognium_seal": ((0.55, 0.12, 0.12), (0.25, 0.20, 0.20), 40),
    "eq_harmony_cognium_violet": ((0.30, 0.18, 0.45), (0.35, 0.30, 0.40), 80),
    # third-party C20 containers carried by the cargo module
    "eq_harmony_c20_red": ((0.55, 0.20, 0.15), (0.15, 0.15, 0.15), 20),
    "eq_harmony_c20_blue": ((0.18, 0.30, 0.48), (0.15, 0.15, 0.15), 20),
    "eq_harmony_c20_grey": ((0.50, 0.50, 0.48), (0.15, 0.15, 0.15), 20),
})

JADE, CREAM, GOLD, DARK = "eq_harmony_jade", "eq_harmony_cream", "eq_harmony_gold", "eq_harmony_dark"
C8 = math.cos(math.pi / 8)
T8 = math.tan(math.pi / 8)


# ---------------------------------------------------------------- local helpers
def merge(dst, src):
    off = len(dst.v)
    dst.v += src.v
    dst.n += src.n
    for m, fl in src.faces.items():
        dst.faces.setdefault(m, []).extend(tuple(i + off for i in f) for f in fl)


def extrude(part, mat, poly, w0, w1, R=I3, t=(0, 0, 0), caps=(True, True), inward=False, soft=30.0):
    """a convex local (x, y) polygon extruded along local z from w0 to w1 (R / t place it); corners flatter than
    `soft` degrees share normals (rounded octagons read smooth)"""
    n = len(poly)
    cx, cy = sum(p[0] for p in poly) / n, sum(p[1] for p in poly) / n
    sg = -1.0 if inward else 1.0
    en = []
    for i in range(n):
        (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % n]
        nx, ny = y1 - y0, -(x1 - x0)
        if ((x0 + x1) / 2 - cx) * nx + ((y0 + y1) / 2 - cy) * ny < 0:
            nx, ny = -nx, -ny
        l = math.hypot(nx, ny) or 1.0
        en.append((sg * nx / l, sg * ny / l))
    cs = math.cos(math.radians(soft))

    def vn(i, e):
        a, b = en[e], en[(i - 1) % n] if e == i else en[i % n]
        if a[0] * b[0] + a[1] * b[1] < cs:
            return (a[0], a[1], 0.0)
        return norm((a[0] + b[0], a[1] + b[1], 0.0))
    for i in range(n):
        j = (i + 1) % n
        (x0, y0), (x1, y1) = poly[i], poly[j]
        ids = [part.vert((x0, y0, w0), vn(i, i), R, t), part.vert((x1, y1, w0), vn(j, i), R, t),
               part.vert((x1, y1, w1), vn(j, i), R, t), part.vert((x0, y0, w1), vn(i, i), R, t)]
        part.face(mat, ids, apply(R, (en[i][0], en[i][1], 0.0)))
    s0 = 1.0 if w0 > w1 else -1.0
    for w, s, on in ((w0, s0, caps[0]), (w1, -s0, caps[1])):
        if on:
            part.face(mat, [part.vert((x, y, w), (0, 0, s), R, t) for x, y in poly], apply(R, (0, 0, s)))


def poly_ring(part, mat, outer, inner, w0, w1, R=I3, t=(0, 0, 0)):
    """a frame between two corresponding polygons (same vertex count), extruded from w0 to w1"""
    extrude(part, mat, outer, w0, w1, R, t, (False, False))
    extrude(part, mat, inner, w0, w1, R, t, (False, False), inward=True)
    n = len(outer)
    s0 = 1.0 if w0 > w1 else -1.0
    for w, s in ((w0, s0), (w1, -s0)):
        for i in range(n):
            j = (i + 1) % n
            pts = [(outer[i][0], outer[i][1], w), (outer[j][0], outer[j][1], w), (inner[j][0], inner[j][1], w), (inner[i][0], inner[i][1], w)]
            part.face(mat, [part.vert(p, (0, 0, s), R, t) for p in pts], apply(R, (0, 0, s)))


def roct(half, cr=0.0, n=2):
    """rounded octagon (flats at distance `half`, facing 0, 45, 90 ... deg), corner radius cr with n segments"""
    pts = []
    for k in range(8):
        a = math.pi / 8 + k * math.pi / 4
        if cr <= 0 or n < 1:
            pts.append((half / C8 * math.cos(a), half / C8 * math.sin(a)))
            continue
        d = (half - cr) / C8
        for i in range(n + 1):
            b = a - math.pi / 8 + (math.pi / 4) * i / n
            pts.append((d * math.cos(a) + cr * math.cos(b), d * math.sin(a) + cr * math.sin(b)))
    return pts


def soct(hw, hh, c):
    """stretched octagon: a hw x hh half-size rectangle with 45-degree corners of size c"""
    return [(hw, -hh + c), (hw, hh - c), (hw - c, hh), (-hw + c, hh), (-hw, hh - c), (-hw, -hh + c), (-hw + c, -hh), (hw - c, -hh)]


def olathe(part, mat, prof, R=I3, t=(0, 0, 0)):
    """an octagonal lathe (flats at 0, 45 ... deg); profile radii are flat distances"""
    lathe(part, mat, [(r / C8, z) for r, z in prof], 8, R, t, math.pi / 8, math.pi / 8 + 2 * math.pi, smooth=False)


def lens(r0, r1, w, n=4):
    """a petal: convex lens from radius r0 to r1 along local x, half width w"""
    up = [(r0 + (r1 - r0) * (1 - math.cos(math.pi * i / n)) / 2, w * math.sin(math.pi * i / n)) for i in range(n + 1)]
    return up + [(x, -y) for x, y in up[-2:0:-1]]


def mandala(part, lod, r, R=I3, o=(0, 0, 0), h=None):
    """the Harmony emblem on a face (local +Z out): gold ring, 8 jade petals, gold boss"""
    h = h or 0.03 * r
    seg = seg_for(lod)
    lathe(part, GOLD, crect(0.80 * r, 0.90 * r, 0.0, h), seg, R, o)
    for k in range(8 if lod == 0 else 0):
        extrude(part, JADE, lens(0.30 * r, 0.76 * r, 0.12 * r, 3), 0.0, h * 0.8, mat_mul(R, rot_z(k * math.pi / 4 + math.pi / 8)), o)
    lathe(part, GOLD, [(0, 0), (0.24 * r, 0), (0.24 * r, h), (0.14 * r, 1.8 * h), (0, 1.9 * h)], small_for(lod), R, o)


def hy_flange(f, lod, r, t, R=I3, o=(0, 0, 0), neck_r=0.0, neck=0.0, seg=None):
    """Harmony round flange in a local frame: mating face at z = 0, body (and neck) towards +Z; 16 bolts in 8 pairs"""
    seg = seg or seg_for(lod)
    prof = [(0, 0), (r, 0), (r, t)]
    prof += [(neck_r, t), (neck_r, t + neck), (0, t + neck)] if neck_r else [(0, t)]
    lathe(f, JADE, prof, seg, R, o)
    if lod == 0:
        lathe(f, GOLD, crect(r - 0.03 * r, r + 0.006, 0.3 * t, 0.7 * t), seg, R, o)          # gold edge
        lathe(f, GOLD, crect(0.50 * r, 0.56 * r, -0.006, 0.0), seg, R, o)                      # spigot ring
    if lod == 0:
        rb = 0.86 * r if not neck_r else (neck_r + r) / 2
        for k in range(8):
            for s in (-1, 1):
                a = k * math.pi / 4 + math.pi / 8 + s * 0.11
                q = apply(R, (rb * math.cos(a), rb * math.sin(a), t))
                hex_bolt(f, "eq_steel", (q[0] + o[0], q[1] + o[1], q[2] + o[2]), apply(R, (0, 0, 1)), max(0.045, 0.07 * r), 0.6 * max(0.045, 0.07 * r))


def oct_hits(part, half, z0, z1, xy=(0.0, 0.0)):
    """an octagonal prism (flats at `half`) along Z as four band boxes"""
    s = 2 * half * T8
    return [hbox(part, (xy[0], xy[1], (z0 + z1) / 2), (2 * half, s, z1 - z0), (0, 0, d)) for d in (0, 45, 90, 135)]


# ---------------------------------------------------------------- jump cores H-5 / H-12 / H-80
# Passport: sealed jump core (fault class jump_core: field = emitter, damper = lotosite dampers, seal = seals and tamper
# control), cube envelope E. Harmony assembles them from sealed Cognium ("Zero Point") blocks under licence: an open
# rounded-octagon cage (cream lacquer panels, jade pillars, gold trim) with window bands that show the ring of black
# sealed blocks (violet engraved geometry, red tamper seals) round the emitter column. Lotosite dampers = 8 lotus pods on
# each end plate (mandala), the Harmony flange on the -Z face, the seal boss in the centre of the +Z end cap.
def hcore(E, tiers, pre):
    h = E / 2
    zp0, zp1 = -h + 0.07 * E, -h + 0.13 * E          # fore end plate (outer, inner face)
    za0, za1 = h - 0.13 * E, h - 0.07 * E            # aft end plate
    rc = 0.06 * E                                    # corner pillar radius
    tp = 0.04 * E                                    # panel thickness
    span = (za0 - zp1) / tiers
    bands = [zp1 + (i + 0.5) * span for i in range(tiers)]
    hb = 0.27 * span if tiers == 1 else 0.30 * span   # window band half length
    wp = 2 * h * T8 - 1.3 * rc                        # panel width between pillars
    bd, bw = 0.14 * E, 0.24 * E                       # sealed block radial depth / width
    rb = h - tp - 0.015 * E - bd / 2
    pod_r, pod_d, pod_rr = 0.075 * E, 0.056 * E, 0.36 * E
    fl_r = 0.24 * E

    def parts(lod):
        seg, small = seg_for(lod), small_for(lod)
        P = {}
        # --- housing: end plates, corner pillars, panel strips between the window bands, gold trim, petal end cap
        hs = P[pre + "housing"] = Part(pre + "housing")
        nr = 3 if lod == 0 else (2 if lod == 1 else 1)
        for w0, w1 in ((zp0, zp1), (za0, za1)):
            extrude(hs, CREAM, roct(h, 0.10 * E, nr), w0, w1)
        dp = (h - rc) / C8
        for k in range(8):
            a = math.pi / 8 + k * math.pi / 4
            cyl(hs, JADE, polz(dp, a, zp1), polz(dp, a, za0), rc, small if lod == 0 else 6)
        edges = [zp1] + sum([[c - hb, c + hb] for c in bands], []) + [za0]
        strips = [(edges[2 * i], edges[2 * i + 1]) for i in range(len(edges) // 2)]
        for k in range(8):
            Rz = rot_z(k * math.pi / 4)
            for z0, z1 in strips:
                box(hs, CREAM, apply(Rz, (h - tp / 2, 0, 0))[:2] + ((z0 + z1) / 2,), (tp, wp, z1 - z0), Rz)
                if lod == 0:                                                          # lacquered jade inlay
                    box(hs, JADE, apply(Rz, (h - 0.002, 0, 0))[:2] + ((z0 + z1) / 2,), (0.006, wp * 0.7, max(0.02, z1 - z0 - 0.08 * E)), Rz)
            if lod < 2:
                for c in bands:                                                       # gold trim at the windows
                    for zz in (c - hb, c + hb):
                        box(hs, GOLD, apply(Rz, (h - 0.004, 0, 0))[:2] + (zz,), (0.012, wp, 0.022 * E), Rz)
        if lod == 0:
            for w, s in ((zp0, -1), (za1, 1)):                                        # gold trim rings on the plates
                poly_ring(hs, GOLD, roct(h - 0.025 * E, 0.08 * E, nr), roct(h - 0.04 * E, 0.07 * E, nr), w, w + s * 0.006)
            for k in range(8):                                                        # petals between the pods (aft)
                extrude(hs, GOLD, lens(0.20 * E, 0.44 * E, 0.035 * E, 3), za1, za1 + 0.008, rot_z(k * math.pi / 4))
            lathe(hs, GOLD, crect(0.15 * E, 0.165 * E, za1, za1 + 0.008), seg)
        # --- field: the sealed Cognium blocks round the emitter column (one ring of 8 per window band)
        fd = P[pre + "field"] = Part(pre + "field")
        cyl(fd, "eq_harmony_cognium_violet", (0, 0, zp1), (0, 0, za0), 0.20 * E, seg if lod < 2 else 8)
        for c in bands:
            for k in range(8):
                Rz = rot_z(k * math.pi / 4)
                ctr = apply(Rz, (rb, 0, 0))[:2] + (c,)
                box(fd, "eq_harmony_cognium", ctr, (bd, bw, 2 * hb + 0.06 * E), Rz, chamfer=0.012 * E if lod == 0 else 0.0)
                if lod < 2:                                                           # engraved geometry (relief)
                    xo = rb + bd / 2
                    for zz in (c - hb * 0.55, c + hb * 0.55):
                        box(fd, "eq_harmony_cognium_violet", apply(Rz, (xo, 0, 0))[:2] + (zz,), (0.006, bw * 0.75, 0.012 * E), Rz)
                    if lod == 0:
                        lathe(fd, "eq_harmony_cognium_violet", crect(0.045 * E, 0.055 * E, 0, 0.004), 16, mat_mul(Rz, axis_to((1, 0, 0))), apply(Rz, (xo, 0, 0))[:2] + (c,))
                        box(fd, "eq_harmony_cognium_violet", apply(Rz, (xo + 0.002, 0, 0))[:2] + (c,), (0.004, 0.006 * E + 0.004, hb * 0.9), Rz)
        # --- dampers: 8 lotosite lotus pods on each end plate (between the flats, i.e. on the corner lines)
        dm = P[pre + "dampers"] = Part(pre + "dampers")
        for zf, s in ((zp0, -1), (za1, 1)):
            for k in range(8):
                a = math.pi / 8 + k * math.pi / 4
                o = polz(pod_rr, a, zf)
                Rr = I3 if s > 0 else rot_x(math.pi)
                if lod > 0:
                    lathe(dm, JADE, [(0, 0), (pod_r, 0), (pod_r, pod_d * 0.6), (0, pod_d)], 8 if lod == 1 else 6, Rr, o)
                    continue
                lathe(dm, JADE, [(0, 0), (pod_r, 0), (pod_r, 0.35 * pod_d), (0.72 * pod_r, 0.82 * pod_d), (0.3 * pod_r, 0.98 * pod_d), (0, pod_d)],
                      small, Rr, o, soft=40)
                lathe(dm, GOLD, crect(pod_r - 0.004, pod_r + 0.012 * E, 0.0, 0.22 * pod_d), small, Rr, o)
                if lod == 0:
                    lathe(dm, GOLD, [(0.12 * pod_r, 0), (0.12 * pod_r, 0.012 * E), (0, 0.016 * E)], 8, Rr, (o[0], o[1], o[2] + s * 0.85 * pod_d))
        # --- seal: tamper seal boss in the aft end cap, red seal tags across every sealed block
        sl = P[pre + "seal"] = Part(pre + "seal")
        lathe(sl, GOLD, [(0, za1), (0.13 * E, za1), (0.13 * E, za1 + 0.03 * E), (0.10 * E, h - 0.012 * E), (0, h - 0.012 * E)], seg if lod < 2 else 12)
        lathe(sl, "eq_harmony_cognium_seal", [(0, h - 0.012 * E), (0.085 * E, h - 0.012 * E), (0.085 * E, h - 0.004 * E), (0.05 * E, h), (0, h)], small)
        if lod < 2:
            for c in bands:
                for k in range(8):
                    Rz = rot_z(k * math.pi / 4)
                    box(sl, "eq_harmony_cognium_seal", apply(Rz, (rb + bd / 2 + 0.004, bw * 0.33, 0))[:2] + (c,),
                        (0.01, 0.045 * E, 0.09 * E), Rz)
        if lod == 0:
            for k in range(8):                                                        # seal monitor contacts on the boss
                a = k * math.pi / 4
                cyl(sl, "eq_harmony_cognium_violet", polz(0.115 * E, a, za1 + 0.03 * E), polz(0.115 * E, a, za1 + 0.036 * E), 0.008 * E, 8)
        # --- flange (fore face): Harmony round flange with 16 bolts in 8 pairs on a short neck
        fl = P[pre + "flange"] = Part(pre + "flange")
        hy_flange(fl, lod, fl_r, 0.03 * E, I3, (0, 0, -h), 0.20 * E, zp0 - (-h) - 0.03 * E)
        if lod < 2:
            for x in (-0.07 * E, 0.07 * E):                                          # power contacts, data pin row
                lathe(fl, GOLD, [(0, 0.004), (0.022 * E, 0.004), (0.022 * E, 0.0), (0, 0.0)], small, I3, (x, 0.06 * E, -h))
            box(fl, DARK, (0, -0.08 * E, -h + 0.002), (0.10 * E, 0.03 * E, 0.004))
        # --- close-up: symmetric cable looms (flange neck -> each fore pod), seal wires (boss -> aft pods), plate bolts
        if close():
            hz = P[pre + "harness"] = Part(pre + "harness")
            for k in range(8):
                a = math.pi / 8 + k * math.pi / 4
                p0 = polz(0.205 * E, a, zp0 - 0.012 * E)
                p3 = polz(pod_rr - pod_r - 0.004, a, zp0 - 0.012 * E)
                harness(hz, bezier(p0, polz(0.25 * E, a, zp0 - 0.04 * E), polz(0.27 * E, a, zp0 - 0.04 * E), p3, 8), 0.006 * E, 2,
                        (JADE, "eq_insulator"), ties=GOLD, tie_every=4)
                q0 = polz(0.13 * E, a, za1 + 0.02 * E)
                tube(hz, "eq_steel", sag(q0, polz(pod_rr - pod_r, a, za1 + 0.02 * E), -0.01 * E, 6, (0, 0, 1)), 0.0025 * E, 6)
                for w, s in ((zp0, -1), (za1, 1)):
                    hex_bolt(hz, GOLD, polz((h - 0.06 * E) / C8, a, w), (0, 0, s), 0.022 * E, 0.012 * E)
        return P

    def hits():
        H = oct_hits(pre + "housing", h, zp0, za1)
        H.append(hcyl(pre + "field", (0, 0, zp1), (0, 0, za0), rb + bd / 2))
        for zf, s in ((zp0, -1), (za1, 1)):
            for k in range(8):
                a = math.pi / 8 + k * math.pi / 4
                H.append(hcyl(pre + "dampers", polz(pod_rr, a, zf), polz(pod_rr, a, zf + s * pod_d), pod_r))
        H.append(hcyl(pre + "seal", (0, 0, za1), (0, 0, h), 0.13 * E))
        H.append(hcyl(pre + "flange", (0, 0, -h), (0, 0, zp0), fl_r))
        return H
    return parts, hits


H5_PARTS, H5_HITS = hcore(2.477, 1, "h5_")
H12_PARTS, H12_HITS = hcore(3.175, 1, "h12_")
H80_PARTS, H80_HITS = hcore(5.04, 2, "h80_")


# ---------------------------------------------------------------- PT-3 point-defence laser turret
# Passport envelope [1.867, 1.12, 2.988] (W, H, L); skin unit: mounting face y = 0, fires along +Y in the drawing frame,
# stowed with the emitter along +Z. Fault class weapon.pd: emitter, mount (traverse drive). Rounded-octagon base plate with
# the Harmony bolt pairs, octagonal azimuth ring and faceted octagonal dome (mount), emitter tube with gold trim rings and
# an octagonal lens hood; trunnion bosses on the dome flats.
PD3 = dict(zc=-0.5605, base=0.9335, y_ax=0.66)


def pd3_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    zc, bh, ya = PD3["zc"], PD3["base"], PD3["y_ax"]
    P = {}
    b = P["pd3_base"] = Part("pd3_base")
    nr = 3 if lod == 0 else (2 if lod == 1 else 1)
    extrude(b, CREAM, roct(bh, 0.22, nr), 0.0, 0.09, R_Y, (0, 0, zc))
    if lod < 2:
        poly_ring(b, GOLD, roct(bh - 0.03, 0.19, nr), roct(bh - 0.06, 0.16, nr), 0.09, 0.096, R_Y, (0, 0, zc))
    if lod == 0:
        for k in range(8):
            for s in (-1, 1):
                a = k * math.pi / 4 + s * 0.10
                hex_bolt(b, "eq_steel", (0.80 * math.cos(a), 0.09, zc + 0.80 * math.sin(a)), (0, 1, 0), 0.06, 0.035, washer=True)
    m = P["pd3_mount"] = Part("pd3_mount")
    olathe(m, JADE, [(0, 0.09), (0.70, 0.09), (0.70, 0.27), (0.66, 0.31), (0, 0.31)], R_Y, (0, 0, zc))
    if lod < 2:
        olathe(m, GOLD, crect(0.695, 0.712, 0.18, 0.21), R_Y, (0, 0, zc))
    olathe(m, CREAM, [(0, 0.31), (0.64, 0.31), (0.64, 0.56), (0.55, 0.86), (0.36, 1.03), (0.22, 1.07), (0, 1.07)], R_Y, (0, 0, zc))
    if lod < 2:
        mandala(m, lod, 0.20, R_Y, (0, 1.07, zc), 0.025)
        for s in (-1, 1):                                                             # trunnion bosses on the side flats
            cyl(m, GOLD, (s * 0.62, ya, zc), (s * 0.69, ya, zc), 0.17, small)
            cyl(m, JADE, (s * 0.69, ya, zc), (s * 0.72, ya, zc), 0.10, small)
    if lod == 0:
        for k in range(8):                                                            # azimuth drive covers
            a = math.pi / 8 + k * math.pi / 4
            box(m, CREAM, (0.70 / C8 * 0.93 * math.cos(a), 0.18, zc + 0.70 / C8 * 0.93 * math.sin(a)), (0.10, 0.12, 0.16), rot_y(-a), chamfer=0.02)
    e = P["pd3_emitter"] = Part("pd3_emitter")
    z_root, z_hood, z_tip = zc + 0.50, 1.24, 1.494
    olathe(e, JADE, [(0, z_root), (0.26, z_root), (0.26, z_root + 0.22), (0.18, z_root + 0.30), (0, z_root + 0.30)], I3, (0, ya, 0))
    cyl(e, CREAM, (0, ya, z_root + 0.30), (0, ya, z_hood), 0.15, seg if lod < 2 else 12)
    if lod < 2:
        for zz in (z_root + 0.55, z_hood - 0.18):
            lathe(e, GOLD, crect(0.149, 0.175, zz - 0.03, zz + 0.03, 0.01 if lod == 0 else 0.0), seg, I3, (0, ya, 0))
    olathe(e, JADE, [(0.15, z_hood), (0.22, z_hood), (0.22, z_tip - 0.02), (0.205, z_tip), (0.17, z_tip), (0.17, z_tip - 0.06)] if lod < 2 else
           [(0, z_hood), (0.22, z_hood), (0.22, z_tip), (0, z_tip)], I3, (0, ya, 0))
    if lod < 2:
        lathe(e, "eq_harmony_lens", [(0.0, z_tip - 0.10), (0.172, z_tip - 0.10), (0.172, z_tip - 0.06), (0.0, z_tip - 0.055)], seg, I3, (0, ya, 0))
        olathe(e, GOLD, crect(0.205, 0.228, z_tip - 0.04, z_tip - 0.01), I3, (0, ya, 0))
    if lod == 0:
        for k in range(8):                                                            # cooling fins on the barrel
            a = k * math.pi / 4
            Rz = rot_z(a)
            box(e, JADE, (0.17 * math.cos(a), ya + 0.17 * math.sin(a), (z_root + z_hood) / 2 + 0.05), (0.05, 0.016, 0.42), Rz)
    if close():
        hz = P["pd3_harness"] = Part("pd3_harness")
        for s in (-1, 1):                                                             # symmetric looms base -> ring
            p0 = (s * 0.55, 0.10, zc - 0.30)
            harness(hz, bezier(p0, (s * 0.80, 0.12, zc - 0.30), (s * 0.80, 0.24, zc - 0.10), (s * 0.71, 0.24, zc), 10), 0.014, 3,
                    (JADE, "eq_insulator"), ties=GOLD, tie_every=3)
            harness(hz, bezier((s * 0.5, 0.31, zc + 0.35), (s * 0.40, 0.45, zc + 0.6), (s * 0.24, ya - 0.05, z_root - 0.05),
                               (s * 0.20, ya - 0.08, z_root + 0.05), 8), 0.012, 2, (JADE, "eq_insulator"), ties=GOLD, tie_every=3)
    return P


def pd3_hits():
    zc, ya = PD3["zc"], PD3["y_ax"]
    return [hcyl("pd3_base", (0, 0, zc), (0, 0.10, zc), PD3["base"]),
            hcyl("pd3_mount", (0, 0.10, zc), (0, 0.60, zc), 0.70),
            hcyl("pd3_mount", (0, 0.60, zc), (0, 1.11, zc), 0.50),
            hcyl("pd3_mount", (-0.72, PD3["y_ax"], zc), (0.72, PD3["y_ax"], zc), 0.17),
            hcyl("pd3_emitter", (0, ya, zc + 0.50), (0, ya, zc + 0.80), 0.27),
            hcyl("pd3_emitter", (0, ya, zc + 0.80), (0, ya, 1.24), 0.17),
            hcyl("pd3_emitter", (0, ya, 1.24), (0, ya, 1.494), 0.235)]


# ---------------------------------------------------------------- VPU-8 vertical launcher (8 cells)
# Passport envelope [3.15, 1.89, 5.04]; skin unit, cells open along +Y. Fault class weapon.vls: hatch (cell lids), ignition
# (launch circuit), cell (cell thermal control). Cream stretched-octagon block with jade bands, 2 x 4 octagonal lids with
# gold rims and the emblem, a firing-circuit spine between the lid rows and firing boxes on both ends, four finned thermal
# units on each long side, jade base plate with the bolt pairs.
VLS = dict(hw=1.30, hl=2.40, y_deck=1.70, cells=[(sx * 0.66, sz) for sz in (-1.8, -0.6, 0.6, 1.8) for sx in (-1, 1)])


def vls8_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    hw, hl, yd = VLS["hw"], VLS["hl"], VLS["y_deck"]
    P = {}
    bd = P["vls8_body"] = Part("vls8_body")
    extrude(bd, CREAM, soct(hw, hl, 0.30), 0.10, yd, R_Y)
    extrude(bd, JADE, soct(hw + 0.02, hl + 0.02, 0.31), yd, yd + 0.06, R_Y)
    if lod < 2:
        for y0, y1 in ((0.10, 0.28), (yd - 0.26, yd - 0.10)):
            poly_ring(bd, JADE, soct(hw + 0.015, hl + 0.015, 0.31), soct(hw - 0.01, hl - 0.01, 0.29), y0, y1, R_Y)
        poly_ring(bd, GOLD, soct(hw + 0.02, hl + 0.02, 0.31), soct(hw - 0.01, hl - 0.01, 0.29), yd - 0.30, yd - 0.26, R_Y)
    hm = P["vls8_hatches"] = Part("vls8_hatches")
    y0 = yd + 0.06
    for x, z in VLS["cells"]:
        o = (x, 0, z)
        if lod == 2:
            olathe(hm, JADE, [(0, y0), (0.50, y0), (0.50, y0 + 0.08), (0, y0 + 0.08)], R_Y, o)
            continue
        olathe(hm, JADE, [(0, y0), (0.50, y0), (0.52, y0 + 0.02), (0.52, y0 + 0.06), (0.50, y0 + 0.08), (0, y0 + 0.08)], R_Y, o)
        olathe(hm, GOLD, crect(0.40, 0.44, y0 + 0.08, y0 + 0.088), R_Y, o)
        mandala(hm, lod, 0.22, R_Y, (x, y0 + 0.08, z), 0.012)
        s = 1 if x > 0 else -1                                                        # hinge on the outer side
        cyl(hm, GOLD, (x + s * 0.50, y0 + 0.05, z - 0.30), (x + s * 0.50, y0 + 0.05, z + 0.30), 0.045, small)
        if lod == 0:
            box(hm, CREAM, (x - s * 0.53, y0 + 0.04, z), (0.06, 0.06, 0.16), chamfer=0.01)    # latch
            for k in range(8):
                a = math.pi / 8 + k * math.pi / 4
                hex_bolt(hm, "eq_steel", (x + 0.47 * math.cos(a), y0 + 0.08, z + 0.47 * math.sin(a)), (0, 1, 0), 0.04, 0.012)
    ig = P["vls8_ignition"] = Part("vls8_ignition")
    box(ig, JADE, (0, yd + 0.09, 0), (0.20, 0.06, 2 * hl - 0.3), chamfer=0.015 if lod == 0 else 0.0)
    for s in (-1, 1):
        box(ig, CREAM, (0, 0.95, s * (hl + 0.05)), (1.20, 0.80, 0.10), chamfer=0.02 if lod == 0 else 0.0)
        if lod < 2:
            box(ig, GOLD, (0, 0.95, s * (hl + 0.102)), (1.0, 0.012, 0.004))
            for x in (-0.3, 0.0, 0.3):
                lathe(ig, GOLD, [(0, 0), (0.07, 0), (0.07, 0.015), (0, 0.015)], small, axis_to((0, 0, s)), (x, 0.80, s * (hl + 0.10)))
        if lod == 0:
            for x in (-0.48, 0.48):
                for y in (0.66, 1.24):
                    hex_bolt(ig, "eq_steel", (x, y, s * (hl + 0.10)), (0, 0, s), 0.04, 0.015)
            box(ig, "eq_status_amber", (0.40, 1.15, s * (hl + 0.103)), (0.10, 0.05, 0.006))
    if lod < 2:
        for x, z in VLS["cells"]:                                                     # firing-line drop to each cell
            box(ig, JADE, (x * 0.40, yd + 0.075, z), (abs(x) * 0.25, 0.03, 0.10))
    ce = P["vls8_cell_thermal"] = Part("vls8_cell_thermal")
    for sx in (-1, 1):
        for z in (-1.8, -0.6, 0.6, 1.8):
            box(ce, JADE, (sx * (hw + 0.10), 0.85, z), (0.20, 1.0, 0.95), chamfer=0.02 if lod == 0 else 0.0)
            if lod < 2:
                box(ce, GOLD, (sx * (hw + 0.202), 1.30, z), (0.004, 0.025, 0.80))
            if lod == 0:
                for i in range(7):
                    box(ce, CREAM, (sx * (hw + 0.23), 0.80, z - 0.36 + 0.12 * i), (0.06, 0.75, 0.02))
    mt = P["vls8_mount"] = Part("vls8_mount")
    extrude(mt, JADE, soct(1.40, 2.52, 0.40), 0.0, 0.10, R_Y)
    if lod == 0:
        pts = [(sx * 1.0, sz * 2.45) for sx in (-1, 1) for sz in (-1, 1)] + [(sx * 1.35, sz * 2.18) for sx in (-1, 1) for sz in (-1, 1)]
        for x, z in pts:
            for d in (-0.08, 0.08):
                q = (x + d, z) if abs(z) > 2.3 else (x, z + d)
                hex_bolt(mt, "eq_steel", (q[0], 0.10, q[1]), (0, 1, 0), 0.06, 0.035, washer=True)
    if close():
        hz = P["vls8_harness"] = Part("vls8_harness")
        for s in (-1, 1):                                                             # symmetric looms, firing box -> body
            for x in (-0.3, 0.3):
                harness(hz, bezier((x, 0.62, s * (hl + 0.085)), (x, 0.45, s * (hl + 0.09)), (x * 1.5, 0.30, s * (hl + 0.10)),
                                   (x * 1.6, 0.30, s * (hl - 0.01)), 8), 0.012, 2, (JADE, "eq_insulator"), ties=GOLD, tie_every=3)
    return P


def vls8_hits():
    hw, hl, yd = VLS["hw"], VLS["hl"], VLS["y_deck"]
    H = [hbox("vls8_mount", (0, 0.05, 0), (2.80, 0.10, 5.04)),
         hbox("vls8_body", (0, (0.10 + yd + 0.06) / 2, 0), (2 * hw, yd - 0.04, 2 * hl))]
    for x, z in VLS["cells"]:
        H.append(hcyl("vls8_hatches", (x, yd + 0.06, z), (x, yd + 0.17, z), 0.52))
    H.append(hbox("vls8_ignition", (0, yd + 0.09, 0), (0.20, 0.06, 2 * hl - 0.3)))
    for s in (-1, 1):
        H.append(hbox("vls8_ignition", (0, 0.95, s * (hl + 0.06)), (1.20, 0.80, 0.12)))
        for z in (-1.8, -0.6, 0.6, 1.8):
            H.append(hbox("vls8_cell_thermal", (s * (hw + 0.13), 0.85, z), (0.26, 1.0, 0.95)))
    return H


# ---------------------------------------------------------------- AP-2 interceptor hangar (2 craft)
# Passport envelope [10.079, 5.04, 12.599]; mounted along the ship axis, the two bay doors on the +Z end face, crew /
# umbilical flange on -Z. Fault class bay: doors, cradle (cradles and guides = the overhead cradle tracks in the roof
# blisters), charger (refuel / charge units on the long sides). Cream stretched-octagon shell with jade frames.
HG = dict(hw=4.62, hh=2.28, c=1.0, z0=-6.0, z1=5.85, zf=6.2995, frames=(-5.4, -2.7, 0.0, 2.7))


def hangar2_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = HG
    hw, hh, c, z0, z1 = d["hw"], d["hh"], d["c"], d["z0"], d["z1"]
    P = {}
    sh = P["hangar2_shell"] = Part("hangar2_shell")
    extrude(sh, CREAM, soct(hw, hh, c), z0, z1)
    for zz in d["frames"]:
        poly_ring(sh, JADE, soct(hw + 0.15, hh + 0.13, c + 0.07), soct(hw - 0.01, hh - 0.01, c - 0.005), zz - 0.18, zz + 0.18)
        if lod < 2:
            for s in (-1, 1):
                poly_ring(sh, GOLD, soct(hw + 0.155, hh + 0.135, c + 0.07), soct(hw + 0.10, hh + 0.08, c + 0.05), zz + s * 0.18, zz + s * 0.19)
    poly_ring(sh, JADE, soct(hw + 0.15, hh + 0.13, c + 0.07), soct(hw - 0.01, hh - 0.01, c - 0.005), z0 - 0.04, z0 + 0.36)
    # portal round the door openings (+Z), with the mullion between the two bays
    poly_ring(sh, JADE, soct(hw + 0.15, hh + 0.13, c + 0.07), soct(hw - 0.35, hh - 0.30, c - 0.15), z1 - 0.20, d["zf"])
    box(sh, JADE, (0, 0, (z1 + d["zf"]) / 2), (0.36, 2 * (hh - 0.30), d["zf"] - z1))
    if lod < 2:
        poly_ring(sh, GOLD, soct(hw + 0.10, hh + 0.08, c + 0.05), soct(hw + 0.02, hh + 0.0, c + 0.02), d["zf"], d["zf"] + 0.006)
    if lod == 0:
        for zz in d["frames"]:                                                        # lacquered panel seams on the sides
            for zp in (zz + 0.9, zz + 1.8):
                if zp < z1 - 0.3:
                    for s in (-1, 1):
                        box(sh, JADE, (s * (hw + 0.004), 0, zp), (0.01, 2 * (hh - c) - 0.1, 0.03))
                    box(sh, JADE, (0, -(hh + 0.004), zp), (2 * (hw - c) - 0.1, 0.01, 0.03))
    dr = P["hangar2_doors"] = Part("hangar2_doors")
    yo, gap = hh - 0.30, 0.012
    for bx in (-1, 1):
        xa, xb = bx * 0.18, bx * (hw - 0.35)                                          # bay opening edges
        xm = (xa + xb) / 2
        lo, hi = min(xa, xb), max(xa, xb)
        cc = 0.55
        for side in (-1, 1):                                                          # two leaves per bay
            if side < 0:
                poly = [(lo, -yo), (xm - gap, -yo), (xm - gap, yo), (lo, yo)]
                if bx < 0:
                    poly = [(lo + cc, -yo), (xm - gap, -yo), (xm - gap, yo), (lo + cc, yo), (lo, yo - cc), (lo, -yo + cc)]
            else:
                poly = [(xm + gap, -yo), (hi, -yo), (hi, yo), (xm + gap, yo)]
                if bx > 0:
                    poly = [(xm + gap, -yo), (hi - cc, -yo), (hi, -yo + cc), (hi, yo - cc), (hi - cc, yo), (xm + gap, yo)]
            extrude(dr, JADE, poly, z1 - 0.05, d["zf"] - 0.10)
            if lod == 0:
                xe = lo if side < 0 else hi                                           # hinge knuckles on the outer edges
                for yy in (-yo + 0.5, 0.0, yo - 0.5):
                    cyl(dr, GOLD, (xe + (0.06 if side < 0 else -0.06), yy - 0.18, d["zf"] - 0.06), (xe + (0.06 if side < 0 else -0.06), yy + 0.18, d["zf"] - 0.06), 0.05, 8)
        if lod < 2:
            mandala(dr, lod, 1.05, I3, (xm, 0.25, d["zf"] - 0.10), 0.03)
            for yy in (-yo + 0.35, yo - 0.35):
                box(dr, GOLD, (xm, yy, d["zf"] - 0.095), (abs(xb - xa) - 1.3, 0.04, 0.012))
    cr = P["hangar2_cradles"] = Part("hangar2_cradles")
    for s in (-1, 1):
        extrude(cr, JADE, soct(0.70, 0.12, 0.09), -5.3, 5.3, I3, (s * 2.1, hh + 0.12, 0))
        if lod < 2:
            box(cr, GOLD, (s * 2.1, hh + 0.243, 0), (0.30, 0.006, 10.2))
            for zz in (-5.3, 5.3):
                cyl(cr, CREAM, (s * 2.1 - 0.35, hh + 0.12, zz), (s * 2.1 + 0.35, hh + 0.12, zz), 0.115, small)
        if lod == 0:
            for zz in (-3.4, -1.0, 1.0, 3.4):
                box(cr, CREAM, (s * 2.1, hh + 0.22, zz), (0.9, 0.04, 0.24), chamfer=0.01)
    ch = P["hangar2_chargers"] = Part("hangar2_chargers")
    for s in (-1, 1):
        for zz in (-2.7 / 2 - 0.0, 2.7 / 2):
            box(ch, CREAM, (s * (hw + 0.20), 0.0, zz), (0.40, 1.40, 2.0), chamfer=0.05 if lod == 0 else 0.0)
            if lod < 2:
                cyl(ch, GOLD, (s * (hw + 0.40), 0.20, zz), (s * (hw + 0.415), 0.20, zz), 0.42, seg)
                cyl(ch, JADE, (s * (hw + 0.40), 0.20, zz), (s * (hw + 0.43), 0.20, zz), 0.18, small)
                box(ch, JADE, (s * (hw + 0.405), -0.48, zz), (0.01, 0.20, 1.6))
            if lod == 0:
                for i in range(3):
                    box(ch, "eq_status_amber", (s * (hw + 0.405), -0.48, zz - 0.4 + 0.4 * i), (0.012, 0.10, 0.16))
                for yy in (-0.6, 0.6):
                    for zq in (zz - 0.9, zz + 0.9):
                        hex_bolt(ch, "eq_steel", (s * (hw + 0.40), yy, zq), (s, 0, 0), 0.06, 0.03)
    mt = P["hangar2_mounts"] = Part("hangar2_mounts")
    for x in (-2.6, 2.6):
        for zz in (-4.0, 4.0):
            hy_flange(mt, lod, 0.62, 0.10, rot_x(-math.pi / 2), (x, -2.52, zz), 0.45, 0.14, seg=small * 2 if lod == 0 else small)
    fl = P["hangar2_flange"] = Part("hangar2_flange")
    hy_flange(fl, lod, 0.95, 0.12, I3, (0, 0, -d["zf"]), 0.75, (z0 - 0.04) - (-d["zf"]) - 0.12)
    if lod == 0:
        lathe(fl, CREAM, crect(0.32, 0.46, -d["zf"] - 0.002, -d["zf"] + 0.0), seg)
    if close():
        hz = P["hangar2_harness"] = Part("hangar2_harness")
        for s in (-1, 1):                                                             # charger feed looms, mirrored
            for zz in (-2.7 / 2, 2.7 / 2):
                harness(hz, bezier((s * (hw + 0.10), 0.72, zz - 0.6), (s * (hw + 0.25), 1.30, zz - 0.9), (s * (hw - 0.1), hh - 0.2, zz - 1.0),
                                   (s * (hw - c * 0.5 + 0.04), hh - c * 0.5 + 0.04, zz - 1.1), 10), 0.03, 3, (JADE, "eq_insulator"), ties=GOLD, tie_every=3)
    return P


def hangar2_hits():
    d = HG
    hw, hh = d["hw"], d["hh"]
    c = d["c"] + 0.07
    H = []
    for part, z0, z1 in (("hangar2_shell", d["z0"] - 0.04, d["z1"]), ("hangar2_doors", d["z1"], d["zf"])):
        for sx, sy in ((hw + 0.15, hh + 0.13 - c), (hw + 0.15 - c, hh + 0.13)):           # stretched octagon as 2 boxes
            H.append(hbox(part, (0, 0, (z0 + z1) / 2), (2 * sx, 2 * sy, z1 - z0)))
    for s in (-1, 1):
        H.append(hbox("hangar2_cradles", (s * 2.1, hh + 0.13, 0), (1.40, 0.26, 10.6)))
        for zz in (-1.35, 1.35):
            H.append(hbox("hangar2_chargers", (s * (hw + 0.21), 0, zz), (0.42, 1.40, 2.0)))
    for x in (-2.6, 2.6):
        for zz in (-4.0, 4.0):
            H.append(hcyl("hangar2_mounts", (x, -2.52, zz), (x, -2.28, zz), 0.62))
    H.append(hcyl("hangar2_flange", (0, 0, -d["zf"]), (0, 0, d["z0"]), 0.95))
    return H


# ---------------------------------------------------------------- jump-train modules MP-4800 (cargo) / MC-4200 (tank)
# Long axis Z, couplers on both ends (fore -Z = probe, aft +Z = drogue; Harmony coupler disc with 16 bolts in 8 pairs).
# Fault class train_module: coupler, locks (cargo locks / tank saddle clamps on every ring frame), valve (tank only).
# Rounded-octagon ring frames (jade, gold trim), cream corner longerons, 8-spoke mandala spiders carrying the couplers.
def train_frame(P, lod, pre, half, hl, nframes, fd=0.50, fw=0.60):
    seg, small = seg_for(lod), small_for(lod)
    nr = 2 if lod == 0 else (1 if lod == 1 else 0)
    cr = 0.9
    zf = hl - 0.75 - fw / 2                               # end frame centre
    zs = [-zf + 2 * zf * i / (nframes - 1) for i in range(nframes)]
    fr = P[pre + "frame"] = Part(pre + "frame")
    for zz in zs:
        poly_ring(fr, JADE, roct(half, cr, nr), roct(half - fd, cr * 0.6, nr), zz - fw / 2, zz + fw / 2)
        if lod == 0:
            for s in ((1,) if zz < 0 else (-1,)):
                poly_ring(fr, GOLD, roct(half - 0.06, cr - 0.05, nr), roct(half - 0.14, cr - 0.1, nr), zz + s * fw / 2, zz + s * (fw / 2 + 0.012))
    dl = (half - cr) / C8 + cr - 0.30                     # longeron centre on the corner lines
    for k in range(8):
        a = math.pi / 8 + k * math.pi / 4
        Rz = rot_z(a)
        box(fr, CREAM, apply(Rz, (dl, 0, 0))[:2] + (0.0,), (0.55, 0.60, 2 * zf - fw), Rz)
        if lod < 2:
            box(fr, GOLD, apply(Rz, (dl + 0.278, 0, 0))[:2] + (0.0,), (0.006, 0.10, 2 * zf - fw), Rz)
    r_hub = 2.0
    for s in (-1, 1):                                                                 # end spiders
        z = s * zf
        lathe(fr, JADE, [(0, z - 0.4), (r_hub, z - 0.4), (r_hub, z + 0.4), (0, z + 0.4)], seg if lod < 2 else 12)
        for k in range(8):
            Rz = rot_z(k * math.pi / 4)
            L = half - fd + 0.1 - r_hub
            box(fr, JADE, apply(Rz, (r_hub + L / 2 - 0.05, 0, 0))[:2] + (z,), (L + 0.1, 0.45, 0.50), Rz)
    for s, kind in ((-1, "fore"), (1, "aft")):                                        # couplers
        c = P[pre + "coupler_" + kind] = Part(pre + "coupler_" + kind)
        R = I3 if s > 0 else rot_x(math.pi)
        zb = s * (zf + 0.4)
        hy_flange(c, lod, 1.7, 0.18, rot_x(math.pi) if s > 0 else I3, (0, 0, s * (zf + 0.58)), 1.35, 0.0)
        if kind == "fore":                                                            # probe
            lathe(c, CREAM, [(0, 0), (0.80, 0), (0.80, 0.25), (0.40, hl - zf - 0.40 - 0.05), (0.18, hl - zf - 0.40), (0, hl - zf - 0.40)], seg if lod < 2 else 12, R, (0, 0, zb))
        else:                                                                         # drogue cup
            lathe(c, CREAM, [(0.95, 0.18), (1.25, 0.18), (1.25, hl - zf - 0.40 - 0.05), (1.20, hl - zf - 0.40), (0.95, hl - zf - 0.40), (0.95, 0.18)] if lod < 2 else
                  [(0, 0.18), (1.25, 0.18), (1.25, hl - zf - 0.40), (0, hl - zf - 0.40)], seg if lod < 2 else 12, R, (0, 0, zb))
            if lod < 2:
                lathe(c, DARK, [(0.0, 0.20), (0.951, 0.20), (0.951, 0.181), (0.0, 0.181)], seg, R, (0, 0, zb))
        if lod < 2:
            lathe(c, GOLD, crect(0.79 if kind == "fore" else 1.249, 0.83 if kind == "fore" else 1.29, 0.30, 0.40), seg, R, (0, 0, zb))
    lk = P[pre + "locks"] = Part(pre + "locks")
    for zz in zs if lod < 2 else []:
        for k in range(8):
            Rz = rot_z(k * math.pi / 4)
            for s in ((1,) if zz < 0 else (-1,)):
                zq = zz + s * (fw / 2 + 0.16)
                rq = half - fd - 0.10
                box(lk, JADE, apply(Rz, (rq, 0, 0))[:2] + (zq,), (0.36, 0.70, 0.32), Rz, chamfer=0.03 if close() else 0.0)
                if lod == 0:
                    box(lk, GOLD, apply(Rz, (rq - 0.185, 0, 0))[:2] + (zq,), (0.012, 0.50, 0.20), Rz)
                if lod == 0:
                    box(lk, "eq_steel", apply(Rz, (rq - 0.26, 0.20, 0))[:2] + (zq,), (0.18, 0.08, 0.08), Rz)
    return zs, zf


def train_frame_hits(pre, half, hl, zs, zf, fd=0.50, fw=0.60):
    H = []
    for zz in zs:
        for k in range(8):
            Rz = rot_z(k * math.pi / 4)
            H.append(hbox(pre + "frame", apply(Rz, (half - fd / 2, 0, 0))[:2] + (zz,), (fd, 2 * half * T8 + 0.2, fw), (0, 0, k * 45)))
    dl = (half - 0.9) / C8 + 0.9 - 0.30
    for k in range(8):
        a = math.pi / 8 + k * math.pi / 4
        H.append(hbox(pre + "frame", polz(dl, a, 0), (0.55, 0.60, 2 * zf), (0, 0, math.degrees(a))))
    for s, kind in ((-1, "fore"), (1, "aft")):
        H.append(hcyl(pre + "frame", (0, 0, s * (zf - 0.4)), (0, 0, s * (zf + 0.4)), 2.0))
        H.append(hcyl(pre + "coupler_" + kind, (0, 0, s * (zf + 0.4)), (0, 0, s * (zf + 0.58)), 1.7))
        H.append(hcyl(pre + "coupler_" + kind, (0, 0, s * (zf + 0.58)), (0, 0, s * hl), 0.8 if kind == "fore" else 1.25))
    for zz in zs:
        for k in range(8):
            Rz = rot_z(k * math.pi / 4)
            for s in ((1,) if zz < 0 else (-1,)):
                H.append(hbox(pre + "locks", apply(Rz, (half - fd - 0.10, 0, 0))[:2] + (zz + s * (fw / 2 + 0.16),), (0.36, 0.70, 0.32), (0, 0, k * 45)))
    return H


CG = dict(half=7.0665, hl=17.666, nframes=6)
C20 = (2.44, 2.59, 6.06)


def cargo_slots():
    """C20 stack per bay: 5 columns x 5 rows inside the rounded-octagon frame, corners left out"""
    out = []
    for i in range(-2, 3):
        for j in range(-2, 3):
            x, y = i * 2.50, j * 2.62
            ox, oy = abs(x) + 1.22, abs(y) + 1.295
            if ox <= 6.5 and oy <= 6.5 and ox + oy <= 8.95:
                out.append((x, y))
    return out


def cargo4800_parts(lod):
    P = {}
    d = CG
    zs, zf = train_frame(P, lod, "cargo4800_", d["half"], d["hl"], d["nframes"])
    ld = P["cargo4800_load"] = Part("cargo4800_load")
    mats = ("eq_harmony_c20_red", "eq_harmony_c20_blue", "eq_harmony_c20_grey", "eq_harmony_c20_blue", "eq_harmony_c20_grey")
    slots = cargo_slots()
    for b in range(len(zs) - 1):
        zc = (zs[b] + zs[b + 1]) / 2
        if lod == 2:                                                                  # far: the stack as 5 blocks
            for (x0, x1, y0, y1) in ((-3.72, 3.72, -3.915, 3.915), (-1.22, 1.22, 3.915, 6.535), (-1.22, 1.22, -6.535, -3.915),
                                     (3.78, 6.22, -2.6, 2.6), (-6.22, -3.78, -2.6, 2.6)):
                box(ld, "eq_harmony_c20_grey", ((x0 + x1) / 2, (y0 + y1) / 2, zc), (x1 - x0, y1 - y0, C20[2]))
            continue
        for n, (x, y) in enumerate(slots):
            m = mats[(n * 7 + b * 3) % len(mats)]
            box(ld, m, (x, y, zc), C20, chamfer=0.03 if close() else 0.0)
            if close():
                outer = [(sx, sy) for sx, sy in ((1, 0), (-1, 0), (0, 1), (0, -1)) if (x + sx * 2.5, y + sy * 2.62) not in slots]
                for sx, sy in outer:                                                  # corrugation on the outer faces
                    for i in range(9):
                        zq = zc - 2.6 + i * 0.65
                        if sx:
                            box(ld, m, (x + sx * 1.235, y, zq), (0.03, 2.2, 0.16))
                        else:
                            box(ld, m, (x, y + sy * 1.31, zq), (2.1, 0.03, 0.16))
    if close():
        hz = P["cargo4800_harness"] = Part("cargo4800_harness")
        half = d["half"]
        for k in range(8):                                                            # lock control loom along each longeron
            a = math.pi / 8 + k * math.pi / 4
            dl = (half - 0.9) / C8 + 0.9 - 0.30 - 0.33
            harness(hz, [polz(dl, a, zf * (2 * i / 16 - 1)) for i in range(17)], 0.04, 2, (JADE, "eq_insulator"), ties=GOLD, tie_every=2)
    return P


def cargo4800_hits():
    d = CG
    zf = d["hl"] - 1.05
    zs = [-zf + 2 * zf * i / (d["nframes"] - 1) for i in range(d["nframes"])]
    H = train_frame_hits("cargo4800_", d["half"], d["hl"], zs, zf)
    for b in range(len(zs) - 1):
        zc = (zs[b] + zs[b + 1]) / 2
        for (x0, x1, y0, y1) in ((-3.72, 3.72, -3.915, 3.915), (-1.22, 1.22, 3.915, 6.535), (-1.22, 1.22, -6.535, -3.915),
                                 (3.78, 6.22, -2.6, 2.6), (-6.22, -3.78, -2.6, 2.6)):
            H.append(hbox("cargo4800_load", ((x0 + x1) / 2, (y0 + y1) / 2, zc), (x1 - x0, y1 - y0, C20[2])))
    return H


TK = dict(half=6.7705, hl=16.926, nframes=6, r=6.0, body=12.2, head=3.0)


def tank4200_parts(lod):
    P = {}
    d = TK
    seg, small = seg_for(lod), small_for(lod)
    zs, zf = train_frame(P, lod, "tank4200_", d["half"], d["hl"], d["nframes"])
    tk = P["tank4200_tank"] = Part("tank4200_tank")
    r, zb, hd = d["r"], d["body"], d["head"]
    nh = 8 if lod == 0 else (6 if lod == 1 else 3)
    head = [(r * math.cos(math.pi / 2 * i / nh), zb + hd * math.sin(math.pi / 2 * i / nh)) for i in range(nh + 1)]
    prof = [(x, -z) for x, z in head[::-1]] + head
    lathe(tk, CREAM, prof, seg if lod < 2 else 16, soft=30)
    if lod < 2:
        seg = seg if lod == 0 else 16
        for i in range(len(zs) - 1):                                                  # jade band + gold trim per bay
            zc = (zs[i] + zs[i + 1]) / 2
            if abs(zc) > zb:
                continue
            lathe(tk, JADE, crect(r - 0.01, r + 0.05, zc - 0.45, zc + 0.45), seg)
            for s in (-1, 1) if lod == 0 else ():
                lathe(tk, GOLD, crect(r - 0.01, r + 0.06, zc + s * 0.45 - 0.04, zc + s * 0.45 + 0.04), seg)
    vl = P["tank4200_valves"] = Part("tank4200_valves")
    for zv in (zs[1] / 2 + zs[2] / 2, zs[3] / 2 + zs[4] / 2):
        for s in (-1, 1):
            y0 = s * (r + 0.05)
            box(vl, JADE, (0, s * (r + 0.20), zv - 1.0), (1.0, 0.36, 1.1), chamfer=0.04 if lod == 0 else 0.0)
            cyl(vl, CREAM, (0, s * (r + 0.36), zv - 1.0), (0, s * (r + 0.62), zv - 1.0), 0.22, small)
            if lod == 0:
                lathe(vl, GOLD, crect(0.24, 0.36, 0, 0.05), small, axis_to((0, s, 0)), (0, s * (r + 0.56), zv - 1.0))
                cyl(vl, JADE, (0, s * (r + 0.30), zv - 0.40), (0, s * (r + 0.30), zv + 0.40), 0.16, small)
                for zq in (zv + 0.40,):
                    lathe(vl, GOLD, crect(0.16, 0.30, -0.04, 0.04), small, I3, (0, s * (r + 0.30), zq))
            if lod == 0:
                for k in range(8):                                                    # 8-bolt valve flange
                    a = k * math.pi / 4
                    hex_bolt(vl, "eq_steel", (0.30 * math.cos(a), s * (r + 0.61), zv - 1.0 + 0.30 * math.sin(a)), (0, s, 0), 0.06, 0.03)
                cyl(vl, "eq_steel", (0, s * (r + 0.30), zv + 0.44), (0, s * (r + 0.30), zv + 0.60), 0.05, 8)
                lathe(vl, GOLD, crect(0.30, 0.36, -0.03, 0.03), 16, I3, (0, s * (r + 0.30), zv + 0.62))     # hand wheel
                for k in range(4):
                    box(vl, GOLD, (0.0, s * (r + 0.30), zv + 0.62), (0.62, 0.035, 0.035), rot_z(k * math.pi / 4))
    if close():
        hz = P["tank4200_harness"] = Part("tank4200_harness")
        for zv in (zs[1] / 2 + zs[2] / 2, zs[3] / 2 + zs[4] / 2):
            for s in (-1, 1):                                                         # valve control looms, mirrored
                for sx in (-1, 1):
                    p0 = (sx * 0.5, s * (r + 0.20), zv - 1.0)
                    harness(hz, cyl_path(r + 0.10, math.pi / 2 * s - sx * 0.09, math.pi / 2 * s - sx * 0.55, zv - 1.0, zv - 2.6, 14), 0.035, 2,
                            (JADE, "eq_insulator"), ties=GOLD, tie_every=3)
    return P


def tank4200_hits():
    d = TK
    zf = d["hl"] - 1.05
    zs = [-zf + 2 * zf * i / (d["nframes"] - 1) for i in range(d["nframes"])]
    H = train_frame_hits("tank4200_", d["half"], d["hl"], zs, zf)
    H.append(hcyl("tank4200_tank", (0, 0, -d["body"]), (0, 0, d["body"]), d["r"]))
    for s in (-1, 1):
        H.append(hcyl("tank4200_tank", (0, 0, s * d["body"]), (0, 0, s * (d["body"] + d["head"] * 0.7)), d["r"] * 0.72))
    for zv in (zs[1] / 2 + zs[2] / 2, zs[3] / 2 + zs[4] / 2):
        for s in (-1, 1):
            H.append(hbox("tank4200_valves", (0, s * (d["r"] + 0.33), zv - 0.55), (1.0, 0.66, 2.1)))
    return H


ASSETS = {"harmony_h5_core": H5_PARTS, "harmony_h12_core": H12_PARTS, "harmony_h80_core": H80_PARTS,
          "harmony_pd3_turret": pd3_parts, "harmony_vls8_launcher": vls8_parts, "harmony_ap2_hangar": hangar2_parts,
          "harmony_mp4800_cargo": cargo4800_parts, "harmony_mc4200_tank": tank4200_parts}
HITS = {"harmony_h5_core": H5_HITS, "harmony_h12_core": H12_HITS, "harmony_h80_core": H80_HITS,
        "harmony_pd3_turret": pd3_hits, "harmony_vls8_launcher": vls8_hits, "harmony_ap2_hangar": hangar2_hits,
        "harmony_mp4800_cargo": cargo4800_hits, "harmony_mc4200_tank": tank4200_hits}
MODEL_ASSETS = {"harmony.core.h5": "harmony_h5_core", "harmony.core.h12": "harmony_h12_core", "harmony.core.h80": "harmony_h80_core",
                "harmony.wpn.pd3": "harmony_pd3_turret", "harmony.wpn.vls8": "harmony_vls8_launcher",
                "harmony.bay.hangar2": "harmony_ap2_hangar", "harmony.mod.cargo4800": "harmony_mp4800_cargo",
                "harmony.mod.tank4200": "harmony_mc4200_tank"}
