"""Kāpehu Systems (Lalande, Tauranga; Pacific voyaging and wayfinding, its own jump mathematics outside the Cognium
standard): K-1 flight computer, K-5 / K-12 jump cores.

School (DESIGN_SCHOOLS.md): organic, navigational - canoe-hull curves (va'a), lashing bands, koru spiral relief,
outrigger-like struts; composite wood, ocean teal, bone. Interface: a round lashing ring with 8 toggles. Close-up:
rope-like lashings, spiral reliefs.
The cores are deliberately unlike the Cognium-standard ones (Harmony's octagonal cage of sealed blocks): one bone
ceramic field ovoid wound with a teal double helix, held between two wooden end discs by canoe-hull damper arcs - a
star-compass / armillary silhouette.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_kapehu_wood": ((0.42, 0.28, 0.18), (0.20, 0.18, 0.15), 40),
    "eq_kapehu_teal": ((0.12, 0.42, 0.42), (0.35, 0.40, 0.40), 90),
    "eq_kapehu_bone": ((0.86, 0.82, 0.72), (0.30, 0.30, 0.28), 60),
    "eq_kapehu_rope": ((0.62, 0.52, 0.36), (0.05, 0.05, 0.05), 10),
    "eq_kapehu_glow": ((0.30, 0.85, 0.80), (0.40, 0.50, 0.50), 60),
})

WOOD, TEAL, BONE, ROPE, GLOW = "eq_kapehu_wood", "eq_kapehu_teal", "eq_kapehu_bone", "eq_kapehu_rope", "eq_kapehu_glow"


# ---------------------------------------------------------------- local helpers
def loft(part, mat, secs, R=I3, t=(0, 0, 0), cap0=False, cap1=False):
    """skin between closed sections (lists of local 3D points, equal counts); smooth shared normals oriented away
    from the section centres; optional flat caps"""
    m, ns = len(secs[0]), len(secs)
    cen = [tuple(sum(p[k] for p in s) / m for k in range(3)) for s in secs]
    acc = [[[0.0, 0.0, 0.0] for _ in range(m)] for _ in range(ns)]
    quads = []
    for i in range(ns - 1):
        cc = tuple((cen[i][k] + cen[i + 1][k]) / 2 for k in range(3))
        for j in range(m):
            j2 = (j + 1) % m
            q = [secs[i][j], secs[i][j2], secs[i + 1][j2], secs[i + 1][j]]
            fn = facet_normal(q)
            if fn is None:
                continue
            mid = tuple(sum(p[k] for p in q) / 4 for k in range(3))
            if sum((mid[k] - cc[k]) * fn[k] for k in range(3)) < 0:
                fn = (-fn[0], -fn[1], -fn[2])
            for (a, b) in ((i, j), (i, j2), (i + 1, j2), (i + 1, j)):
                for k in range(3):
                    acc[a][b][k] += fn[k]
            quads.append(((i, j), (i, j2), (i + 1, j2), (i + 1, j), fn))
    ids = {}
    for i in range(ns):
        for j in range(m):
            ids[(i, j)] = part.vert(secs[i][j], norm(tuple(acc[i][j])), R, t)
    for a, b, c, d, fn in quads:
        part.face(mat, [ids[a], ids[b], ids[c], ids[d]], apply(R, fn))
    for on, i, o in ((cap0, 0, 1), (cap1, ns - 1, ns - 2)):
        if on:
            dn = norm(tuple(cen[i][k] - cen[o][k] for k in range(3)))
            part.face(mat, [part.vert(p, dn, R, t) for p in secs[i]], apply(R, dn))


def vaa_section(b, dep, m):
    """canoe cross-section (local u across, v down from the gunwale): flat deck line on top, full rounded bottom;
    returns m + 1 points from the right gunwale round the keel to the left gunwale (the deck closes the loop)"""
    out = []
    for i in range(m + 1):
        th = math.pi * i / m
        c, s = math.cos(th), math.sin(th)
        out.append((b * math.copysign(abs(c) ** 0.6, c), -dep * abs(s) ** 0.8))
    return out


def vaa_hull(part, mat, L, B, D, rise, nz, m, x0=0.0, y_deck=0.0, z0=0.0):
    """a va'a hull along Z: length L, beam B, depth D, sheer rising by `rise` at the ends; gunwale at y_deck"""
    secs = []
    for i in range(nz + 1):
        f = -1 + 2 * i / nz
        a = abs(f)
        b = max(0.012, B / 2 * max(0.0, 1 - a ** 2.2) ** 0.55)
        dep = max(0.02, D * max(0.0, 1 - a ** 3) ** 0.5)
        yd = y_deck + rise * a ** 3
        secs.append([(x0 + u, yd + v, z0 + f * L / 2) for u, v in vaa_section(b, dep, m)])
    loft(part, mat, secs, cap0=True, cap1=True)
    return secs


def hull_side_x(B, D, L, y_deck, y, z):
    """x of the hull side (right) at height y and station z (inverse of vaa_section)"""
    a = abs(2 * z / L)
    b = B / 2 * max(0.0, 1 - a ** 2.2) ** 0.55
    dep = D * max(0.0, 1 - a ** 3) ** 0.5
    s = min(1.0, max(0.0, (y_deck - y) / max(dep, 1e-6))) ** (1 / 0.8)
    c = math.sqrt(max(0.0, 1 - s * s))
    return b * c ** 0.6


def koru(n_turns, r0, r1, n):
    """an opening spiral (koru) in a local plane: list of (u, v), from the centre r0 out to r1"""
    out = []
    for i in range(n + 1):
        t = i / n
        a = 2 * math.pi * n_turns * t
        r = r0 + (r1 - r0) * t
        out.append((r * math.cos(a), r * math.sin(a)))
    return out


def rope_ring(part, pts_loop, r, seg=6):
    """a lashing band: a rope tube round a closed loop of points"""
    tube(part, ROPE, pts_loop + pts_loop[:2], r, seg)


def lashing_ring(P, name, lod, r0, r1, z_face, s, th, toggles=8):
    """Kapehu interface: a teal rope-bound ring (mating face at z_face, facing s) with 8 bone toggles on its rim"""
    f = P[name] = Part(name)
    seg, small = seg_for(lod), small_for(lod)
    R = I3 if s < 0 else rot_x(math.pi)
    th = abs(th)
    lathe(f, TEAL, rrect(r0, r1, 0.0, th, min(0.3 * th, 0.02 + 0.0), 2 if lod == 0 else 1), seg, R, (0, 0, z_face), soft=35)
    for k in range(toggles if lod < 2 else 0):
        a = 2 * math.pi * (k + 0.5) / toggles
        p0 = polz(r1 + 0.012, a, 0)
        T = (-math.sin(a), math.cos(a), 0.0)
        L = 0.10 * r1
        zc = z_face - s * th * 0.5
        cyl(f, BONE, (p0[0] - T[0] * L, p0[1] - T[1] * L, zc), (p0[0] + T[0] * L, p0[1] + T[1] * L, zc), 0.022 * r1 + 0.004, small if lod == 0 else 6)
        if lod == 0:                                  # the rope loop over the toggle
            lathe(f, ROPE, crect(r1 - 0.004, r1 + 0.012, -0.03 * r1, 0.03 * r1), 4, R, (0, 0, zc), a - 0.14, a + 0.14, cap=True)
    if lod < 2:                                       # keyed socket in the ring's centre
        lathe(f, BONE, [(0, 0), (r0 * 0.55, 0), (r0 * 0.55, th * 0.5), (0, th * 0.5)], seg, R, (0, 0, z_face))
        lathe(f, TEAL, crect(r0 * 0.55, r0, th * 0.2, th * 0.5), seg, R, (0, 0, z_face))
        for k in range(3):
            a = 2 * math.pi * k / 3 + math.pi / 2
            cyl(f, TEAL, polz(r0 * 0.30, a, z_face), polz(r0 * 0.30, a, z_face - s * 0.006), 0.06 * r0 + 0.004, 8)
    return f


# ---------------------------------------------------------------- K-1 flight computer
# Passport envelope [1.406, 0.843, 1.406]; rack unit on its lashing ring (mounting face y = 0, top +Y), long axis Z.
# Fault class computer: board (the main va'a hull - the computing boards), memory (the deck house on it), psu (the
# outrigger float, ama). Two curved iako booms lashed across, koru reliefs on the hull sides, glow line on the gunwale.
K1 = dict(L=1.40, B=0.74, D=0.40, y_deck=0.56, rise=0.07, x_hull=-0.33, x_ama=0.575, L_ama=1.06, B_ama=0.25, D_ama=0.24,
          y_ama=0.36, iako=(-0.34, 0.34))


def k1_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = K1
    nz, m = {0: (16, 12), 1: (10, 8), 2: (6, 6)}[lod]
    if close():
        nz, m = 22, 16
    P = {}
    bd = P["k1_board"] = Part("k1_board")
    vaa_hull(bd, WOOD, d["L"], d["B"], d["D"], d["rise"], nz, m, d["x_hull"], d["y_deck"])
    if lod < 2:                                                            # glow line along both gunwales
        for sgn in (-1, 1):
            pts = []
            for i in range(13):
                z = -0.55 + 1.10 * i / 12
                x = hull_side_x(d["B"], d["D"], d["L"], d["y_deck"], d["y_deck"] - 0.035, z)
                pts.append((d["x_hull"] + sgn * (x + 0.004), d["y_deck"] - 0.035 + d["rise"] * abs(2 * z / d["L"]) ** 3, z))
            tube(bd, GLOW, pts, 0.008 if lod == 0 else 0.010, 6 if lod == 0 else 4)
    if lod == 0:                                                           # koru reliefs on both sides of the hull
        for sgn in (-1, 1):
            for zc in (-0.36, 0.36):
                pts = []
                for u, v in koru(2.2, 0.01, 0.085, 60):
                    y, z = 0.38 + v, zc + sgn * u
                    x = hull_side_x(d["B"], d["D"], d["L"], d["y_deck"], y, z)
                    pts.append((d["x_hull"] + sgn * (x + 0.002), y, z))
                tube(bd, TEAL, pts, 0.0055, 6)
    # memory: the deck house (fale) with its gable roof
    mm = P["k1_memory"] = Part("k1_memory")
    xh, yd = d["x_hull"], d["y_deck"]
    box(mm, BONE, (xh, yd + 0.065, 0.0), (0.42, 0.13, 0.62), chamfer=0.012 if close() else 0.0)
    roof = [(-0.25, 0.0), (0.25, 0.0), (0.0, 0.135)]
    nrm = None
    rp = Part("roof")
    for (x0, y0), (x1, y1) in ((roof[0], roof[2]), (roof[2], roof[1])):
        nx, ny = y1 - y0, -(x1 - x0)
        if nx * ((x0 + x1) / 2) + ny * ((y0 + y1) / 2 - 0.05) < 0:
            nx, ny = -nx, -ny
        nrm = norm((nx, ny, 0))
        q = [(xh + x0, yd + 0.13 + y0, -0.36), (xh + x1, yd + 0.13 + y1, -0.36), (xh + x1, yd + 0.13 + y1, 0.36), (xh + x0, yd + 0.13 + y0, 0.36)]
        mm.face(TEAL, [mm.vert(p, nrm) for p in q], nrm)
    for zz, s in ((-0.36, -1), (0.36, 1)):
        mm.face(TEAL, [mm.vert((xh + x, yd + 0.13 + y, zz), (0, 0, s)) for x, y in roof], (0, 0, s))
    mm.face(TEAL, [mm.vert(p, (0, -1, 0)) for p in ((xh - 0.25, yd + 0.13, -0.36), (xh + 0.25, yd + 0.13, -0.36), (xh + 0.25, yd + 0.13, 0.36), (xh - 0.25, yd + 0.13, 0.36))], (0, -1, 0))
    if lod < 2:
        box(mm, WOOD, (xh, yd + 0.13 + 0.135, 0.0), (0.035, 0.02, 0.76))   # ridge pole
        for zz in (-0.31, 0.31):                                           # memory module slots (end walls)
            box(mm, GLOW, (xh, yd + 0.07, zz * 1.0 + (0.0015 if zz > 0 else -0.0015)), (0.20, 0.02, 0.004))
    if lod == 0:
        for zz in (-0.311, 0.311):
            for k in range(3):
                cyl(mm, TEAL, (xh - 0.12 + 0.12 * k, yd + 0.035, zz), (xh - 0.12 + 0.12 * k, yd + 0.035, zz * 1.04), 0.018, 8)
    # psu: the ama (outrigger float) with its connecting stanchions
    ps = P["k1_psu"] = Part("k1_psu")
    vaa_hull(ps, TEAL, d["L_ama"], d["B_ama"], d["D_ama"], 0.04, max(6, nz - 4), max(6, m - 2), d["x_ama"], d["y_ama"])
    if lod < 2:
        box(ps, BONE, (d["x_ama"], d["y_ama"] + 0.006, 0.0), (0.10, 0.012, 0.60))
    # struts (iako): two curved booms lashed to the hull deck and to the ama stanchions
    st = P["k1_iako"] = Part("k1_iako")
    for zz in d["iako"]:
        p0 = (xh - 0.06, yd + 0.035, zz)
        p3 = (d["x_ama"], d["y_ama"] + 0.15, zz)
        boom = bezier(p0, (xh + 0.25, yd + 0.20, zz), (d["x_ama"] - 0.15, d["y_ama"] + 0.26, zz), p3, 10 if lod == 0 else 5)
        tube(st, WOOD, boom, 0.024, small if lod == 0 else 6)
        cyl(st, WOOD, (d["x_ama"], d["y_ama"] - 0.01, zz), p3, 0.020, 8 if lod == 0 else 6)                # stanchion
        if lod < 2:
            for q in (boom[1], (d["x_ama"], d["y_ama"] + 0.10, zz)):
                lathe(st, ROPE, crect(0.024, 0.034, -0.025, 0.025), 8, axis_to((1, 0, 0)) if q is boom[1] else R_Y, q)
    # mount: the lashing ring with 8 toggles, two wooden cradles under the hull, a post under the ama
    mt = P["k1_mount"] = Part("k1_mount")
    ylathe(mt, TEAL, rrect(0.42, 0.58, 0.0, 0.07, 0.02, 2 if lod == 0 else 1), seg, soft=35)
    for k in range(8):
        a = 2 * math.pi * (k + 0.5) / 8
        p = polar(0.595, a, 0.035)
        T = (-math.sin(a), 0, math.cos(a))
        cyl(mt, BONE, (p[0] - T[0] * 0.06, 0.035, p[2] - T[2] * 0.06), (p[0] + T[0] * 0.06, 0.035, p[2] + T[2] * 0.06), 0.014, 8 if lod == 0 else 6)
        if lod < 2:
            ylathe(mt, ROPE, crect(0.575, 0.60, 0.015, 0.055), 4, a - 0.12, a + 0.12, cap=True)
    for zz in d["iako"]:
        box(mt, WOOD, (xh, 0.135, zz), (0.30, 0.13, 0.10))
    cyl(mt, WOOD, (d["x_ama"], 0.07, 0.0), (d["x_ama"], d["y_ama"] - d["D_ama"] + 0.02, 0.0), 0.04, 8 if lod == 0 else 6)
    if close():
        hz = P["k1_harness"] = Part("k1_harness")
        for zz in d["iako"]:                                               # rope lashings: cross-wrapped at each joint
            for q in ((xh - 0.06, yd + 0.035, zz), (d["x_ama"], d["y_ama"] + 0.12, zz)):
                for k in range(3):
                    off = -0.03 + 0.03 * k
                    ring = [(q[0] + 0.04 * math.cos(2 * math.pi * i / 10), q[1] + 0.04 * math.sin(2 * math.pi * i / 10), q[2] + off) for i in range(10)]
                    rope_ring(hz, ring, 0.006, 5)
        # power lead from the ama along the forward boom into the deck house
        zz = d["iako"][1]
        harness(hz, bezier((d["x_ama"], d["y_ama"] + 0.02, zz + 0.05), (d["x_ama"] - 0.10, d["y_ama"] + 0.25, zz + 0.04),
                           (xh + 0.30, yd + 0.20, zz + 0.04), (xh + 0.20, yd + 0.07, zz - 0.04), 12), 0.008, 2, (TEAL, "eq_insulator"),
                ties=ROPE, tie_every=3)
    return P


def k1_hits():
    d = K1
    xh = d["x_hull"]
    return [hbox("k1_board", (xh, (d["y_deck"] - d["D"] + d["y_deck"]) / 2 + 0.01, 0), (d["B"], d["D"], d["L"] * 0.92)),
            hbox("k1_memory", (xh, d["y_deck"] + 0.135, 0), (0.50, 0.27, 0.72)),
            hbox("k1_psu", (d["x_ama"], d["y_ama"] - d["D_ama"] / 2, 0), (d["B_ama"], d["D_ama"], d["L_ama"] * 0.9)),
            hbox("k1_iako", ((xh + d["x_ama"]) / 2, d["y_deck"] + 0.05, d["iako"][0]), (d["x_ama"] - xh + 0.1, 0.20, 0.06)),
            hbox("k1_iako", ((xh + d["x_ama"]) / 2, d["y_deck"] + 0.05, d["iako"][1]), (d["x_ama"] - xh + 0.1, 0.20, 0.06)),
            hcyl("k1_mount", (0, 0, 0), (0, 0.07, 0), 0.60)]


# ---------------------------------------------------------------- K-5 / K-12 jump cores
# Passport: sealed jump core (fault class jump_core: field = emitter, damper = lotosite dampers, seal = seals and tamper
# control), cube envelope E, long axis Z. Field = the ovoid with its double helix and axial necks; dampers = the canoe-hull
# arcs (lotosite inside the fairings) with their lashed outrigger struts and lotosite pads at the ovoid; seal = the koru
# seal boss on the aft disc and the bone seal tablets on the lashings; structure = the two wooden end discs; interface =
# the lashing ring on the -Z face (K-5: 4 arcs, K-12: 8 arcs).
def kcore(E, n_arcs, pre):
    h = E / 2
    a_ov, r_ov = 0.36 * E, 0.25 * E               # ovoid half length / radius
    r_disc, t_disc = 0.42 * E, 0.05 * E
    zf0, zf1 = -h + 0.06 * E, -h + 0.06 * E + t_disc          # fore disc
    za1, za0 = h - 0.06 * E, h - 0.06 * E - t_disc            # aft disc
    zA = h - 0.085 * E                                         # arc ends (disc mid planes)
    W, Dk = 0.075 * E, 0.07 * E                                # arc section half width / keel depth
    angs = [math.pi / n_arcs + 2 * math.pi * k / n_arcs for k in range(n_arcs)]
    def crown_ext(rb):                                         # half extent in X / Y of the arc crowns
        m = 0.0
        for a in angs:
            for u, v in vaa_section(W, Dk, 24):
                rr = rb - v
                m = max(m, abs(rr * math.cos(a) - u * math.sin(a)), abs(rr * math.sin(a) + u * math.cos(a)))
        return m
    lo_, hi_ = 0.0, 2 * h
    for _ in range(40):                                        # crowns just inside the cube envelope
        mid = (lo_ + hi_) / 2
        lo_, hi_ = (lo_, mid) if crown_ext(mid) > h - 0.002 * E else (mid, hi_)
    rB = lo_
    r0 = r_disc - 0.05 * E

    def arc_path(t):
        return r0 + (rB - r0) * math.sin(math.pi * t), -zA + 2 * zA * t

    def parts(lod):
        seg, small = seg_for(lod), small_for(lod)
        P = {}
        # --- field: the ovoid, its double helix, the axial necks to the discs
        fd = P[pre + "field"] = Part(pre + "field")
        ne = 10 if lod == 0 else (7 if lod == 1 else 5)
        prof = [(r_ov * math.sin(math.pi * i / ne), -a_ov * math.cos(math.pi * i / ne)) for i in range(ne + 1)]
        lathe(fd, BONE, prof, seg, soft=40)
        cyl(fd, TEAL, (0, 0, zf1), (0, 0, -a_ov + 0.02 * E), 0.07 * E, small)
        cyl(fd, TEAL, (0, 0, a_ov - 0.02 * E), (0, 0, za0), 0.07 * E, small)
        if lod < 2:
            n = 90 if lod == 0 else 24
            for st in range(2):
                pts = []
                for i in range(n + 1):
                    tt = i / n
                    z = -a_ov * 0.92 + 1.84 * a_ov * tt
                    rr = r_ov * math.sqrt(max(0.0, 1 - (z / a_ov) ** 2)) + 0.004 * E
                    ph = st * math.pi + 2 * math.pi * 2.25 * tt
                    pts.append((rr * math.cos(ph), rr * math.sin(ph), z))
                tube(fd, TEAL, pts, 0.012 * E, 6 if lod == 0 else 4)
            lathe(fd, GLOW, crect(r_ov - 0.002, r_ov + 0.006 * E, -0.01 * E, 0.01 * E), seg)   # equator glow band
        # --- structure: the two wooden end discs (koru relief on the aft one)
        ds = P[pre + "discs"] = Part(pre + "discs")
        for z0, z1 in ((zf0, zf1), (za0, za1)):
            lathe(ds, WOOD, [(0, z0), (r_disc - 0.01 * E, z0), (r_disc, z0 + 0.01 * E), (r_disc, z1 - 0.01 * E), (r_disc - 0.01 * E, z1), (0, z1)],
                  seg, soft=50)
            if lod < 2:
                lathe(ds, TEAL, crect(r_disc - 0.004, r_disc + 0.006 * E, (z0 + z1) / 2 - 0.008 * E, (z0 + z1) / 2 + 0.008 * E), seg)
        if lod == 0:
            for k in range(n_arcs):                                            # koru per arc on the aft face
                a = angs[k]
                ctr = polz(0.25 * E, a, za1)
                pts = [(ctr[0] + u * math.cos(a) - v * math.sin(a), ctr[1] + u * math.sin(a) + v * math.cos(a), za1 + 0.003 * E)
                       for u, v in koru(2.0, 0.004 * E, 0.07 * E, 50 if close() else 28)]
                tube(ds, TEAL, pts, 0.004 * E, 5 if close() else 4)
        # --- dampers: canoe-hull arcs with lotosite, outrigger struts to the ovoid with lotosite pads
        dm = P[pre + "dampers"] = Part(pre + "dampers")
        nt = (18 if n_arcs <= 4 else 13) if lod == 0 else (12 if n_arcs <= 4 else 8) if lod == 1 else (6 if n_arcs <= 4 else 4)
        msec = (10 if n_arcs <= 4 else 8) if lod == 0 else ((6 if n_arcs <= 4 else 4) if lod == 1 else 4)
        if close():
            nt, msec = 26, 14
        for a in angs:
            B = (-math.sin(a), math.cos(a), 0.0)
            secs = []
            for i in range(nt + 1):
                tt = i / nt
                r, z = arc_path(tt)
                r2, z2 = arc_path(min(1, tt + 1e-3))
                r1, z1 = arc_path(max(0, tt - 1e-3))
                Tm = norm((r2 - r1, z2 - z1, 0))               # tangent in the (r, z) plane
                Nm = (Tm[1], -Tm[0])                          # outward normal in the (r, z) plane
                if Nm[0] < 0:
                    Nm = (-Nm[0], -Nm[1])
                taper = 0.45 + 0.55 * math.sin(math.pi * tt) ** 0.5
                sec = []
                for u, v in vaa_section(W * taper, Dk * taper, msec):
                    rr, zz = r - v * Nm[0], z - v * Nm[1]     # v < 0 is the keel: outward
                    sec.append((rr * math.cos(a) + u * B[0], rr * math.sin(a) + u * B[1], zz))
                secs.append(sec)
            loft(dm, WOOD, secs, cap0=True, cap1=True)
            # outrigger strut from the arc crown to the ovoid equator, lotosite pad at the ovoid
            rc, _ = arc_path(0.5)
            cyl(dm, WOOD, polz(r_ov + 0.03 * E, a, 0), polz(rc, a, 0), 0.025 * E, small if lod == 0 else 6)
            if lod < 2:
                lathe(dm, BONE, [(0, 0), (0.06 * E, 0), (0.06 * E, 0.025 * E), (0, 0.03 * E)], small, axis_to(polz(1, a, 0)), polz(r_ov - 0.006 * E, a, 0))
            if lod < 2:
                for tt in (0.04, 0.96) if lod == 0 else ():  # rope lashing bands at the disc ends
                    r, z = arc_path(tt)
                    taper = 0.45 + 0.55 * math.sin(math.pi * tt) ** 0.5
                    ring = []
                    for i in range(12):
                        ph = 2 * math.pi * i / 12
                        u, v = W * taper * 1.08 * math.cos(ph), Dk * taper * 0.6 * math.sin(ph) - Dk * taper * 0.45
                        rr2 = r - v
                        ring.append((rr2 * math.cos(a) + u * B[0], rr2 * math.sin(a) + u * B[1], z + (0.012 * E if tt < 0.5 else -0.012 * E)))
                    rope_ring(dm, ring, 0.01 * E, 5 if close() else 4)
                lathe(dm, ROPE, crect(0.025 * E, 0.035 * E, -0.02 * E, 0.02 * E), 8, axis_to(polz(1, a, 0)), polz((r_ov + rc) / 2, a, 0))
        # --- seal: koru seal boss on the aft disc, bone seal tablets on the aft lashings
        sl = P[pre + "seal"] = Part(pre + "seal")
        lathe(sl, BONE, [(0, za1), (0.12 * E, za1), (0.12 * E, za1 + 0.02 * E), (0.09 * E, h), (0, h)], seg if lod < 2 else 12)
        if lod < 2:
            pts = [(u, v, h + 0.002) for u, v in koru(2.5, 0.004 * E, 0.075 * E, 60 if lod == 0 else 30)]
            tube(sl, TEAL, pts, 0.005 * E, 5 if lod == 0 else 4)
            for a in angs:
                r, z = arc_path(0.93)
                q = polz(r + 0.02 * E, a + 0.0, z - 0.02 * E)
                box(sl, BONE, q, (0.05 * E, 0.012 * E + 0.004, 0.07 * E), rot_z(a + math.pi / 2))
        if lod == 0:
            for a in angs:                                    # seal cords from the tablets to the boss
                r, z = arc_path(0.93)
                tube(sl, ROPE, sag(polz(r - 0.01 * E, a, z), polz(0.12 * E, a, za1 + 0.01 * E), 0.03 * E, 8, (0, 0, -1)), 0.003 * E + 0.001, 4)
        # --- interface: the lashing ring on the fore face
        lashing_ring(P, pre + "lashing_ring", lod, 0.25 * E, 0.33 * E, -h, -1, zf0 + h)
        # (the neck through the fore disc is part of the field)
        if close():
            hz = P[pre + "harness"] = Part(pre + "harness")
            for a in angs:                                    # rope cross-lashing at every strut joint
                rc, _ = arc_path(0.5)
                for k in range(3):
                    p = polz(rc - Dk * 0.2 - 0.01 * E * k, a, 0)
                    B = (-math.sin(a), math.cos(a), 0.0)
                    ring = [(p[0] + B[0] * 0.04 * E * math.cos(2 * math.pi * i / 10), p[1] + B[1] * 0.04 * E * math.cos(2 * math.pi * i / 10),
                             0.04 * E * math.sin(2 * math.pi * i / 10) + (k - 1) * 0.015 * E) for i in range(10)]
                    rope_ring(hz, ring, 0.004 * E, 5)
        return P

    def hits():
        H = [hcyl(pre + "field", (0, 0, -a_ov), (0, 0, a_ov), r_ov),
             hcyl(pre + "field", (0, 0, zf1), (0, 0, za0), 0.07 * E),
             hcyl(pre + "discs", (0, 0, zf0), (0, 0, zf1), r_disc),
             hcyl(pre + "discs", (0, 0, za0), (0, 0, za1), r_disc),
             hcyl(pre + "seal", (0, 0, za1), (0, 0, h), 0.12 * E),
             hcyl(pre + "lashing_ring", (0, 0, -h), (0, 0, zf0), 0.35 * E)]
        for a in angs:                                        # each arc as 5 stepped boxes in its meridian plane
            for i in range(5):
                t0, t1 = i / 5, (i + 1) / 5
                (ra, za), (rb, zb) = arc_path(t0), arc_path(t1)
                rlo, rhi = min(ra, rb), max(ra, rb) + Dk
                H.append(hbox(pre + "dampers", polz((rlo + rhi) / 2, a, (za + zb) / 2), (rhi - rlo, 2 * W, zb - za), (0, 0, math.degrees(a))))
            rc, _ = arc_path(0.5)
            H.append(hbox(pre + "dampers", polz((r_ov + rc) / 2, a, 0), (rc - r_ov, 0.06 * E, 0.06 * E), (0, 0, math.degrees(a))))
        return H
    return parts, hits


K5_PARTS, K5_HITS = kcore(2.52, 4, "k5_")
K12_PARTS, K12_HITS = kcore(3.175, 8, "k12_")

ASSETS = {"kapehu_k1_computer": k1_parts, "kapehu_k5_core": K5_PARTS, "kapehu_k12_core": K12_PARTS}
HITS = {"kapehu_k1_computer": k1_hits, "kapehu_k5_core": K5_HITS, "kapehu_k12_core": K12_HITS}
MODEL_ASSETS = {"kapehu.mc.k1": "kapehu_k1_computer", "kapehu.core.k5": "kapehu_k5_core", "kapehu.core.k12": "kapehu_k12_core"}
