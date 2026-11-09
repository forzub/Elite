"""Uhuru Bio-Systems (New Maland, HD 40307; Ujamaa cooperative, agro-rings): U-500 bio-regenerative life support,
AB-100 cold-sleep passenger module.

School (DESIGN_SCHOOLS.md): bio-tech, communal - cylindrical vats, basket-weave lattice cages, domes; red ochre, leaf
green, cream; interface = woven ring cradle; close-up: woven lattice detail, flexible hoses. The lattice is a true
over/under weave: two families of helical strands cross on a regular grid and swap radius at every crossing.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_uh_ochre": ((0.62, 0.30, 0.16), (0.15, 0.15, 0.15), 25),
    "eq_uh_leaf": ((0.30, 0.52, 0.20), (0.15, 0.15, 0.15), 25),
    "eq_uh_cream": ((0.86, 0.82, 0.70), (0.15, 0.15, 0.15), 25),
    "eq_uh_weave": ((0.46, 0.25, 0.13), (0.10, 0.10, 0.10), 15),
    "eq_uh_glass": ((0.16, 0.36, 0.20), (0.70, 0.70, 0.70), 150),
    "eq_uh_hose": ((0.14, 0.15, 0.13), (0.20, 0.20, 0.20), 30),
})
_OC, _LF, _CR, _WV, _GL, _HO = "eq_uh_ochre", "eq_uh_leaf", "eq_uh_cream", "eq_uh_weave", "eq_uh_glass", "eq_uh_hose"


def wspec(table, lod):
    """weave density (strands per family, crossings per strand, samples per crossing, strand segments) or None"""
    return table["close"] if close() else table[lod]


def basket(part, mat, f, r, w0, w1, spec, sr=0.03, amp=None):
    """woven cage on a cylinder: f(u, v, w) maps cylinder-local coordinates (u, v radial, w axial) to the world.
    Two families of helical strands; at every crossing one goes over and the other under (radius +-amp)."""
    if not spec:
        return
    n, m, k, seg = spec
    amp = sr * 0.9 if amp is None else amp
    tau = math.pi * m / n                                               # twist so that each strand crosses m times
    steps = max(2, m * k)
    for fam in (1, -1):
        for i in range(n):
            th0 = 2 * math.pi * i / n
            pts = []
            for s in range(steps + 1):
                q = s / steps
                rr = r + fam * amp * math.cos(math.pi * q * m)
                th = th0 + fam * tau * q
                pts.append(f(rr * math.cos(th), rr * math.sin(th), w0 + (w1 - w0) * q))
            tube(part, mat, pts, sr, seg)


def loop(part, mat, f, r, w, rr, n=32, seg=6):
    """a closed ring tube of radius rr round a cylinder (radius r at axial position w)"""
    tube(part, mat, [f(r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n), w) for i in range(n + 1)], rr, seg)


def _fy(c):
    """cylinder frame about world Y through c: u -> X, v -> Z, w -> Y"""
    return lambda u, v, w: (c[0] + u, c[1] + w, c[2] + v)


def _fz(c, sx=1.0, sy=1.0):
    """cylinder frame about world Z through c, optionally elliptic (u scaled by sx on X, v by sy on Y)"""
    return lambda u, v, w: (c[0] + u * sx, c[1] + v * sy, c[2] + w)


def _dome(r, y1, hd, n):
    return [(r * math.cos(a), y1 + hd * math.sin(a)) for a in (math.pi / 2 * i / n for i in range(n + 1))]


def vat_y(part, mat, c, r, y0, y1, hd, seg, dome_mat=None, nd=4):
    """vertical vat on c (x, z): flat bottom at y0, wall to y1, dome of height hd (a separate material if given)"""
    ylathe(part, mat, [(0, y0), (r, y0), (r, y1)], seg, t=(c[0], 0, c[2]), soft=0)
    ylathe(part, dome_mat or mat, _dome(r, y1, hd, nd), seg, t=(c[0], 0, c[2]), soft=40)


def cradle_y(part, c, r, y_ring, lod, n=8):
    """Uhuru woven ring cradle under a vertical vat: a ring band, a foot ring and crossed woven struts between"""
    small = small_for(lod)
    sg = max(12, seg_for(lod) // 2)
    if lod < 2:
        ylathe(part, _OC, crect(r - 0.02, r + 0.06, y_ring - 0.10, y_ring + 0.02), sg, t=(c[0], 0, c[2]))
    ylathe(part, _OC, crect(r - 0.02, r + 0.08, 0.0, 0.10), sg, t=(c[0], 0, c[2]))
    if lod == 0:
        for i in range(n):
            a = 2 * math.pi * i / n
            b = a + 2 * math.pi / n
            for a0, a1 in ((a, b), (b, a)):
                p0 = (c[0] + (r + 0.03) * math.cos(a0), 0.10, c[2] + (r + 0.03) * math.sin(a0))
                p1 = (c[0] + (r + 0.03) * math.cos(a1), y_ring - 0.10, c[2] + (r + 0.03) * math.sin(a1))
                cyl(part, _WV, p0, p1, 0.035, max(4, small // 2), caps=False)


def _squash(part, start, sx, sy):
    """scale the vertices built since index `start` to an ellipse (x * sx, y * sy), normals by the inverse transpose"""
    for i in range(start, len(part.v)):
        x, y, z = part.v[i]
        part.v[i] = (x * sx, y * sy, z)
        nx, ny, nz = part.n[i]
        part.n[i] = norm((nx / sx, ny / sy, nz))


# ---------------------------------------------------------------- U-500 bio-regenerative life support (uhuru.ls.u500)
# Envelope 8.027 x 4.587 x 5.734, base on y = 0. A row of vats on woven ring cradles, every vat in a woven lattice cage:
# the cream water-reclamation vat (SRU water), two green-glass photobioreactor columns (SRU o2), the horizontal leaf
# green CO2 absorber drum (SRU scrubber) and the ochre ventilation drum with its domed grille and duct (SRU fan).
# Flexible hoses join them at close range.
U5_WEAVE = {"close": (12, 5, 3, 6), 0: (8, 4, 2, 4), 1: (6, 2, 2, 3), 2: None}
U5 = dict(water=(-2.95, 0.0, 1.00), o2=[(-0.80, -1.30, 0.85), (-0.80, 1.30, 0.85)], scr=(1.55, 1.35, 1.05), fan=(3.32, 0.0, 0.66))


def u500_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    sb = max(12, seg // 2)                                               # band segments
    ws = wspec(U5_WEAVE, lod)
    P = {}
    cr = P["u500_cradle"] = Part("u500_cradle")
    for sz in (-1, 1):                                                   # two base rails
        box(cr, _CR, (0, 0.05, sz * 1.95), (7.9, 0.10, 0.24))
    for x in (-2.95, -0.80, 1.55, 3.32):
        box(cr, _CR, (x, 0.05, 0), (0.24, 0.10, 3.66))

    # --- water reclamation vat
    wt = P["u500_water"] = Part("u500_water")
    x, z, r = U5["water"]
    vat_y(wt, _CR, (x, 0, z), r, 0.45, 3.30, 0.75, seg, _CR)
    cradle_y(cr, (x, 0, z), r, 0.55, lod)
    basket(wt, _WV, _fy((x, 0, z)), r + 0.04, 0.70, 3.20, ws)
    for yy in (0.70, 3.20) if lod < 2 else ():
        ylathe(wt, _OC, crect(r, r + 0.07, yy - 0.05, yy + 0.05), sb, t=(x, 0, z))
    ylathe(wt, _LF, [(0, 4.05), (0.28, 4.05), (0.28, 4.12), (0, 4.20)], small, t=(x, 0, z))              # top hatch
    if lod < 2:
        cyl(wt, _CR, (x + r, 1.20, z), (x + r + 0.30, 1.20, z), 0.10, small)                              # outlet port
        lathe(wt, _OC, [(0, 0), (0.16, 0), (0.16, 0.04), (0, 0.04)], small, axis_to((1, 0, 0)), (x + r + 0.30, 1.20, z))

    # --- O2 photobioreactor columns (green glass, cream domes, ochre bands)
    o2 = P["u500_o2"] = Part("u500_o2")
    for x, z, r in U5["o2"]:
        vat_y(o2, _GL, (x, 0, z), r, 0.45, 3.60, 0.70, seg, _CR)
        cradle_y(cr, (x, 0, z), r, 0.55, lod)
        basket(o2, _WV, _fy((x, 0, z)), r + 0.04, 0.70, 3.50, ws)
        for yy in ((0.70, 2.10, 3.50) if lod == 0 else (0.70, 3.50)) if lod < 2 else ():
            ylathe(o2, _OC, crect(r, r + 0.07, yy - 0.05, yy + 0.05), sb, t=(x, 0, z))
        ylathe(o2, _LF, [(0, 4.30), (0.22, 4.30), (0.22, 4.38), (0, 4.45)], small, t=(x, 0, z))
    if lod < 2:
        for x, z, r in U5["o2"]:                                         # gas outlet stacks on the domes
            cyl(o2, _CR, (x + 0.35, 3.95, z), (x + 0.35, 4.32, z), 0.08, small)

    # --- CO2 scrubber: horizontal drum along Z on two woven saddle rings
    sc = P["u500_scrubber"] = Part("u500_scrubber")
    x, y, r = U5["scr"]
    L = 2.0
    lathe(sc, _LF, [(r, -L), (r, L)], seg, I3, (x, y, 0))
    for s in (-1, 1):
        prof = [(rr, s * (L + h)) for rr, h in _dome(r, 0.0, 0.78, 4)]
        lathe(sc, _CR, prof if s > 0 else prof[::-1], seg, I3, (x, y, 0), soft=40)
    basket(sc, _WV, _fz((x, y, 0)), r + 0.04, -L + 0.15, L - 0.15, ws)
    for zz in (-L + 0.10, L - 0.10) if lod < 2 else ():
        lathe(sc, _OC, crect(r, r + 0.07, zz - 0.06, zz + 0.06), sb, I3, (x, y, 0))
    for zz in (-1.2, 1.2):                                               # saddle cradles
        lathe(cr, _OC, crect(r + 0.04, r + 0.12, zz - 0.08, zz + 0.08), sb, I3, (x, y, 0))
        if lod < 2:
            for sx in (-1, 1):
                for d in (-1, 1):
                    cyl(cr, _WV, (x + sx * 0.75, 0.10, zz + d * 0.25), (x + sx * (r * 0.72), y - r * 0.72, zz - d * 0.05), 0.04, max(4, small // 2), caps=False)
    if lod < 2:
        lathe(sc, _OC, [(0, 0), (0.22, 0), (0.22, 0.06), (0, 0.06)], small, I3, (x, y, L + 0.78))         # end port

    # --- ventilation: ochre fan drum with a domed woven grille and a duct up over the scrubber
    fn = P["u500_fan"] = Part("u500_fan")
    x, z, r = U5["fan"]
    ylathe(fn, _OC, [(0, 0.12), (r, 0.12), (r, 1.20), (r - 0.05, 1.25)], seg, t=(x, 0, z))
    ylathe(fn, _HO, [(r - 0.05, 1.25), (0, 1.25)], seg, t=(x, 0, z))                                     # intake disc
    if lod == 0:
        for i in range(12):                                             # grille ribs over the dome
            a = 2 * math.pi * i / 12
            pts = [(x + (r - 0.04) * math.cos(t) * math.cos(a), 1.25 + 0.45 * math.sin(t), z + (r - 0.04) * math.cos(t) * math.sin(a))
                   for t in (math.pi / 2 * j / 6 for j in range(7))]
            tube(fn, _WV, pts, 0.025, 4)
        loop(fn, _WV, _fy((x, 0, z)), r - 0.04, 1.26, 0.03, 24, 4)
    cyl(fn, _CR, (x, 1.20, z + r - 0.05), (x, 3.40, z + r - 0.05), 0.24, small)                            # duct riser
    cyl(fn, _CR, (x, 3.40, z + r - 0.05), (1.55 + 0.30, 3.40, z + r - 0.05), 0.24, small)                  # duct run
    cyl(fn, _CR, (1.55 + 0.30, 3.40, z + r - 0.05), (1.55 + 0.30, 2.30, z + r - 0.05), 0.24, small)          # into the scrubber top
    if lod < 2:
        for p in ((x, 3.40, z + r - 0.05), (1.85, 3.40, z + r - 0.05)):
            ylathe(fn, _OC, crect(0.0, 0.28, -0.28, 0.28), small, t=p)                                    # elbow collars
        cradle_y(cr, (x, 0, z), r, 0.30, lod)

    # --- close-up extras: flexible hoses between the vats, cable ties
    if close():
        hz = P["u500_hoses"] = Part("u500_hoses")
        wx, _, wr = U5["water"]
        (ox, oz0, orr), (_, oz1, _) = U5["o2"]
        sx, sy, sr = U5["scr"]
        runs = [((wx + wr + 0.30, 1.20, 0.0), (ox - orr - 0.05, 1.10, oz0 + 0.3)),
                ((wx + wr + 0.30, 1.20, 0.0), (ox - orr - 0.05, 1.10, oz1 - 0.3)),
                ((ox + orr + 0.05, 2.80, oz0 + 0.2), (sx - sr - 0.05, 1.80, -0.9)),
                ((ox + orr + 0.05, 2.80, oz1 - 0.2), (sx - sr - 0.05, 1.80, 0.9)),
                ((sx, sy - sr - 0.05, 1.8), (3.32 - 0.70, 0.70, 0.30))]
        for a, b in runs:
            tube(hz, _HO, sag(a, b, 0.35, 16), 0.07, 10)
            for p in (a, b):                                            # hose clamps
                d = norm(tuple(b[k] - a[k] for k in range(3)))
                lathe(hz, "eq_steel", crect(0.07, 0.09, -0.03, 0.03), 12, axis_to(d), p)
        for x, z, r in U5["o2"]:                                        # sample taps on the bioreactor columns
            for yy in (1.2, 2.1, 3.0):
                cyl(hz, "eq_steel", (x - r, yy, z), (x - r - 0.12, yy, z), 0.025, 8)
                lathe(hz, _OC, [(0, 0), (0.04, 0), (0.04, 0.05), (0, 0.05)], 8, axis_to((-1, 0, 0)), (x - r - 0.12, yy, z))
    return P


def u500_hits():
    H = [hbox("u500_cradle", (0, 0.05, sz * 1.95), (7.9, 0.10, 0.24)) for sz in (-1, 1)]
    x, z, r = U5["water"]
    H += [hcyl("u500_water", (x, 0.10, z), (x, 3.30, z), r + 0.07), hcyl("u500_water", (x, 3.30, z), (x, 4.20, z), r * 0.75)]
    for x, z, r in U5["o2"]:
        H += [hcyl("u500_o2", (x, 0.10, z), (x, 3.60, z), r + 0.07), hcyl("u500_o2", (x, 3.60, z), (x, 4.45, z), r * 0.72)]
    x, y, r = U5["scr"]
    H += [hcyl("u500_scrubber", (x, y, -2.0), (x, y, 2.0), r + 0.07),
          hcyl("u500_scrubber", (x, y, -2.78), (x, y, -2.0), r * 0.75), hcyl("u500_scrubber", (x, y, 2.0), (x, y, 2.84), r * 0.75),
          hbox("u500_cradle", (x, (y - r) / 2 + 0.05, 0), (1.6, y - r, 2.6))]
    x, z, r = U5["fan"]
    H += [hcyl("u500_fan", (x, 0.10, z), (x, 1.70, z), r), hcyl("u500_fan", (x, 1.70, z + r - 0.05), (x, 3.64, z + r - 0.05), 0.24),
          hbox("u500_fan", ((x + 1.85) / 2, 3.40, z + r - 0.05), (x - 1.85 + 0.48, 0.48, 0.48)),
          hcyl("u500_fan", (1.85, 2.30, z + r - 0.05), (1.85, 3.40, z + r - 0.05), 0.24)]
    return H


# ---------------------------------------------------------------- AB-100 cold-sleep passenger module (uhuru.cryo.pod100)
# Envelope 7.319 x 5.228 x 10.455, axis-mounted (long axis Z, centred). An elliptic cream vat with leaf-green domed ends
# under a woven lattice cage; 100 capsule hatches on the flanks (2 sides x 2 bays x 5 x 5); three woven ring cradles
# carry it and give the bay mounts (the Uhuru interface); the cryocooler head with woven fin ring sits on the aft dome
# (SRU cooler), the capsule monitoring spine with its glass display strip on the top (SRU monitor), the access hatch
# drum on the fore dome.
PD = dict(sx=3.42, sy=2.30, zc=3.85, dome=1.15, cradles=(-3.85, 0.0, 3.85))
POD_WEAVE = {"close": (16, 3, 4, 5), 0: (16, 3, 2, 4), 1: (8, 2, 2, 3), 2: None}
CRADLE_WEAVE = {"close": (12, 1, 4, 5), 0: (10, 1, 2, 4), 1: None, 2: None}


def pod_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    sx, sy, zc, hd = PD["sx"], PD["sy"], PD["zc"], PD["dome"]
    P = {}
    h = P["pod100_hull"] = Part("pod100_hull")
    s0 = len(h.v)
    lathe(h, _CR, [(1.0, -zc), (1.0, zc)], seg, I3, (0, 0, 0))
    for s in (-1, 1):
        prof = [(rr, s * (zc + hh)) for rr, hh in _dome(1.0, 0.0, hd, 5 if lod < 2 else 3)]
        lathe(h, _LF, prof if s > 0 else prof[::-1], seg, I3, (0, 0, 0), soft=40)
    _squash(h, s0, sx, sy)
    fz = _fz((0, 0, 0), sx, sy)
    for w0, w1 in ((-zc + 0.35, -0.35), (0.35, zc - 0.35)):             # woven lattice cage over the cylinder bays
        basket(h, _WV, fz, 1.0 + 0.012, w0, w1, wspec(POD_WEAVE, lod), sr=0.03, amp=0.009)
    # 100 capsule hatches: 2 sides x 2 bays x 5 columns x 5 rows
    hat = P["pod100_hatches"] = Part("pod100_hatches")
    if lod == 0:
        for side in (0.0, math.pi):
            for bay in ((-zc + 0.35, -0.35), (0.35, zc - 0.35)):
                for i in range(5):
                    zz = bay[0] + (i + 0.5) * (bay[1] - bay[0]) / 5
                    for j in range(5):
                        phi = side + math.radians(-36 + 18 * j)
                        p = (sx * math.cos(phi), sy * math.sin(phi))
                        n = norm((math.cos(phi) / sx, math.sin(phi) / sy, 0))
                        a = math.atan2(n[1], n[0])
                        box(hat, _OC, (p[0] + n[0] * 0.015, p[1] + n[1] * 0.015, zz), (0.05, 0.50, 0.56), rot_z(a))
                        box(hat, _GL, (p[0] + n[0] * 0.042, p[1] + n[1] * 0.042, zz), (0.006, 0.20, 0.30), rot_z(a))
    else:
        for side in (0.0, math.pi):
            p = (sx * math.cos(side), 0, 0)
            box(hat, _OC, (p[0] + math.cos(side) * 0.01, 0, 0), (0.04, 2.2, 2 * zc - 0.6))

    # --- woven ring cradles (interface) with bay mount lugs
    cd = P["pod100_cradles"] = Part("pod100_cradles")
    for zz in PD["cradles"]:
        s0 = len(cd.v)
        lathe(cd, _OC, crect(1.0, 1.03, zz - 0.28, zz + 0.28), seg, I3, (0, 0, 0))
        if lod < 2:
            for ww in (zz - 0.30, zz + 0.26):
                lathe(cd, _OC, crect(1.0, 1.045, ww, ww + 0.04), seg, I3, (0, 0, 0))
        _squash(cd, s0, sx, sy)
        basket(cd, _WV, fz, 1.04, zz - 0.24, zz + 0.24, wspec(CRADLE_WEAVE, lod), sr=0.03, amp=0.008)
        for xs in (-1, 1):                                               # bay mount lugs on the lower quarter
            phi = -math.pi / 2 + xs * 0.75
            p = (sx * 1.03 * math.cos(phi), sy * 1.03 * math.sin(phi))
            box(cd, _OC, (p[0], p[1] - 0.12, zz), (0.42, 0.36, 0.64))
            if lod < 2:
                lathe(cd, "eq_steel", crect(0.06, 0.11, -0.34, 0.34), small, I3, (p[0], p[1] - 0.18, zz))

    # --- cryocooler head on the aft dome (SRU cooler)
    co = P["pod100_cooler"] = Part("pod100_cooler")
    z0 = zc + hd - 0.18
    lathe(co, _CR, [(0, z0), (0.95, z0), (0.95, z0 + 0.22)], seg, I3, (0, 0, 0))
    lathe(co, _OC, _dome(0.95, 0.0, 0.18, 3), seg, I3, (0, 0, z0 + 0.22), soft=40)
    if lod < 2:
        for i in range(16 if lod == 0 else 10):                          # woven fin ring
            a = 2 * math.pi * i / (16 if lod == 0 else 10)
            box(co, _WV, polz(1.10, a, z0 + 0.11), (0.32, 0.03, 0.20), rot_z(a + 0.35))
        loop(co, _WV, _fz((0, 0, 0)), 1.25, z0 + 0.11, 0.03, 32, 4)
        for a in (math.pi / 4, 3 * math.pi / 4):
            cyl(co, _LF, polz(0.55, a, z0 + 0.30), polz(0.55, a, z0 + 0.42), 0.10, small)          # cold heads

    # --- capsule monitoring spine on the top (SRU monitor)
    mo = P["pod100_monitor"] = Part("pod100_monitor")
    yt = sy + 0.02
    box(mo, _CR, (0, yt + 0.08, 0), (0.70, 0.16, 5.6), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        box(mo, _GL, (0, yt + 0.165, 0), (0.40, 0.012, 5.2))
        for zz in (-2.4, 0.0, 2.4):
            lathe(mo, _OC, _dome(0.16, 0.0, 0.12, 3), small, R_Y, (0, yt + 0.16, zz), soft=40)

    # --- access hatch drum on the fore dome
    ah = P["pod100_access_hatch"] = Part("pod100_access_hatch")
    z1 = -(zc + hd - 0.15)
    lathe(ah, _OC, [(0, z1), (0.80, z1), (0.80, z1 - 0.18), (0.70, z1 - 0.23), (0, z1 - 0.23)], seg, I3, (0, 0, 0))
    if lod < 2:
        lathe(ah, _CR, crect(0.45, 0.62, z1 - 0.26, z1 - 0.23), seg, I3, (0, 0, 0))
        if lod == 0:
            for i in range(8):
                a = 2 * math.pi * i / 8
                hex_bolt(ah, "eq_steel", polz(0.72, a, z1 - 0.23), (0, 0, -1), 0.06, 0.03)

    # --- close-up: flexible coolant hoses cooler -> spine, sensor leads along the cradles
    if close():
        hz = P["pod100_hoses"] = Part("pod100_hoses")
        for a in (math.radians(72), math.radians(108)):               # laid on the aft dome, cooler rim -> spine end
            pts = [polz(0.98, a, z0 + 0.10)]
            for i in range(13):
                z = z0 - 0.05 - i * (z0 - 0.05 - 2.85) / 12
                rho = (math.sqrt(max(0.0, 1 - ((z - zc) / hd) ** 2)) if z > zc else 1.0) + 0.065
                pts.append((sx * rho * math.cos(a), sy * rho * math.sin(a), z))
            tube(hz, _HO, pts, 0.06, 10)
        for zz in PD["cradles"]:
            tube(hz, _HO, [fz(1.06 * math.cos(t), 1.06 * math.sin(t), zz) for t in (math.pi / 2 - 1.2 * k / 12 for k in range(13))], 0.02, 6)
    return P


def pod_hits():
    sx, sy, zc, hd = PD["sx"], PD["sy"], PD["zc"], PD["dome"]
    H = [hbox("pod100_hull", (0, 0, 0), (2 * sx, 2 * sy * 0.70, 2 * zc)), hbox("pod100_hull", (0, 0, 0), (2 * sx * 0.70, 2 * sy, 2 * zc))]
    for s in (-1, 1):
        H.append(hbox("pod100_hull", (0, 0, s * (zc + hd * 0.40)), (2 * sx * 0.80, 2 * sy * 0.80, hd * 0.80)))
    for side in (-1, 1):
        H.append(hbox("pod100_hatches", (side * (sx + 0.02), 0, 0), (0.08, 2 * sy * 0.62, 2 * zc - 0.6)))
    for zz in PD["cradles"]:
        H += [hbox("pod100_cradles", (0, sy * 0.85, zz), (2 * sx * 0.60, 0.30 * sy, 0.66)),
              hbox("pod100_cradles", (0, -sy * 0.85, zz), (2 * sx * 0.60, 0.30 * sy, 0.66)),
              hbox("pod100_cradles", (sx * 0.85, 0, zz), (0.30 * sx, 2 * sy * 0.60, 0.66)),
              hbox("pod100_cradles", (-sx * 0.85, 0, zz), (0.30 * sx, 2 * sy * 0.60, 0.66))]
    z0 = zc + hd - 0.18
    H += [hcyl("pod100_cooler", (0, 0, z0), (0, 0, z0 + 0.42), 1.30),
          hbox("pod100_monitor", (0, sy + 0.12, 0), (0.70, 0.22, 5.6)),
          hcyl("pod100_access_hatch", (0, 0, -(zc + hd - 0.15) - 0.26), (0, 0, -(zc + hd - 0.15)), 0.80)]
    return H


ASSETS = {"uhuru_u500_life_support": u500_parts, "uhuru_ab100_passenger_module": pod_parts}
HITS = {"uhuru_u500_life_support": u500_hits, "uhuru_ab100_passenger_module": pod_hits}
MODEL_ASSETS = {"uhuru.ls.u500": "uhuru_u500_life_support", "uhuru.cryo.pod100": "uhuru_ab100_passenger_module"}
