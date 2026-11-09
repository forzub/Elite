"""Xinzhi Electronics (Tau Ceti Cognium; sealed-electronics school): C-1 / C-3 comm, MC-C1 / MC-C3 computers,
N-2 / N-10 radars.

School: sealed monolithic rounded boxes (gloss black + white), no visible fasteners, precise fin arrays, cyan status
strips. Interface standard: blind-mate connector rails - two sealed slide rails on the mounting face plus a sealed
connector rail block at the aft (-Z) end. Hull-skin units (comm, radar) face +Y with the mounting face at y = 0;
the computers stand on the same rails (deck mounting face at y = 0).
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_xinzhi_black": ((0.06, 0.07, 0.08), (0.55, 0.55, 0.55), 180),
    "eq_xinzhi_white": ((0.88, 0.89, 0.90), (0.40, 0.40, 0.40), 120),
    "eq_xinzhi_cyan": ((0.10, 0.70, 0.80), (0.30, 0.30, 0.30), 60),
    "eq_xinzhi_fin": ((0.20, 0.21, 0.23), (0.45, 0.45, 0.45), 100),
    "eq_xinzhi_seal": ((0.30, 0.31, 0.33), (0.35, 0.35, 0.35), 80),
})
BLK, WHT, CYN, FIN, SEAL = "eq_xinzhi_black", "eq_xinzhi_white", "eq_xinzhi_cyan", "eq_xinzhi_fin", "eq_xinzhi_seal"


# ---------------------------------------------------------------- local helpers (rounded sealed volumes)
def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cn(lod):
    """segments per rounded corner"""
    return 4 if close() else {0: 3, 1: 2, 2: 1}[lod]


def _rr(w, d, r, n):
    r = max(0.0, min(r, w / 2 - 1e-4, d / 2 - 1e-4))
    if n <= 0 or r < 1e-3:
        return [(w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2), (-w / 2, -d / 2)]
    return rrect(-w / 2, w / 2, -d / 2, d / 2, r, n)[1:]


def _ngon(rad, k, a0=0.0):
    return [(rad * math.cos(a0 + 2 * math.pi * i / k), rad * math.sin(a0 + 2 * math.pi * i / k)) for i in range(k)]


AX = {"y": (0, 1, 0), "z": (0, 0, 1), "x": (1, 0, 0)}


def _ring(poly, h, c=(0, 0, 0), axis="y"):
    if axis == "y":
        return [(c[0] + u, c[1] + h, c[2] + v) for u, v in poly]
    if axis == "z":
        return [(c[0] + u, c[1] + v, c[2] + h) for u, v in poly]
    return [(c[0] + h, c[1] + u, c[2] + v) for u, v in poly]


def loft(part, mat, rings, axis="y", cap0=True, cap1=True, soft=50.0):
    """skin between rings of equal vertex count, ordered along +axis like a lathe profile (bottom -> outer -> top);
    adjacent facets flatter than `soft` degrees share normals (rounded corners read smooth)"""
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
            ids = [part.vert(a[i], ni), part.vert(a[j], nj), part.vert(b[j], nj), part.vert(b[i], ni)]
            part.face(mat, ids, fn)
    neg = (-A[0], -A[1], -A[2])
    if cap0:
        part.face(mat, [part.vert(p, neg) for p in rings[0]], neg)
    if cap1:
        part.face(mat, [part.vert(p, A) for p in rings[-1]], A)


def rbox(part, mat, c, size, r, n, ch=0.0, axis="y", cap0=True):
    """rounded-corner sealed box: footprint corners rounded (radius r), end edges chamfered (ch) along `axis`"""
    if axis == "y":
        w, d, h, hc = size[0], size[2], size[1], c[1]
    elif axis == "z":
        w, d, h, hc = size[0], size[1], size[2], c[2]
    else:
        w, d, h, hc = size[1], size[2], size[0], c[0]
    off = {"y": (c[0], 0, c[2]), "z": (c[0], c[1], 0), "x": (0, c[1], c[2])}[axis]
    if ch < min_feature():
        ch = 0.0
    ch = min(ch, h * 0.4, w * 0.2, d * 0.2)
    y0, y1 = hc - h / 2, hc + h / 2
    if ch > 0:
        inner = _rr(w - 2 * ch, d - 2 * ch, max(r - ch, 0.0), n)
        outer = _rr(w, d, r, n)
        if len(inner) != len(outer):
            inner = _rr(w - 2 * ch, d - 2 * ch, max(r - ch, 0.002), n)
        rings = [_ring(inner, y0, off, axis), _ring(outer, y0 + ch, off, axis), _ring(outer, y1 - ch, off, axis), _ring(inner, y1, off, axis)]
    else:
        outer = _rr(w, d, r, n)
        rings = [_ring(outer, y0, off, axis), _ring(outer, y1, off, axis)]
    loft(part, mat, rings, axis, cap0=cap0)


def band(part, mat, w, d, r, n, y0, y1, e, c=(0, 0, 0)):
    """a light strip / seam band standing e proud around a rounded footprint (no inner faces)"""
    inner, outer = _rr(w, d, r, n), _rr(w + 2 * e, d + 2 * e, r + e, n)
    off = (c[0], 0, c[2])
    loft(part, mat, [_ring(inner, y0, off), _ring(outer, y0, off), _ring(outer, y1, off), _ring(inner, y1, off)], cap0=False, cap1=False)


def fin_row(part, mat, along, a0, a1, n, o0, o1, y0, y1, t):
    """n thin plates perpendicular to `along` ('x' or 'z') spread over a0..a1; o0..o1 = the other horizontal extent"""
    for i in range(n):
        a = a0 + (a1 - a0) * (i + 0.5) / n
        if along == "z":
            box(part, mat, ((o0 + o1) / 2, (y0 + y1) / 2, a), (o1 - o0, y1 - y0, t))
        else:
            box(part, mat, (a, (y0 + y1) / 2, (o0 + o1) / 2), (t, y1 - y0, o1 - o0))


def fins(part, lod, along, a0, a1, n, o0, o1, y0, y1, t=0.014):
    """a precise fin array: doubled on the close level, halved at LOD2, one slab at LOD3"""
    if lod == 2:
        if along == "z":
            box(part, FIN, ((o0 + o1) / 2, (y0 + y1) / 2, (a0 + a1) / 2), (o1 - o0, y1 - y0, a1 - a0))
        else:
            box(part, FIN, ((a0 + a1) / 2, (y0 + y1) / 2, (o0 + o1) / 2), (a1 - a0, y1 - y0, o1 - o0))
        return
    k = n * 2 if close() else (n if lod == 0 else max(3, n // 2))
    tt = t * 0.6 if close() else (t if lod == 0 else t * 1.6)
    fin_row(part, FIN, along, a0, a1, k, o0, o1, y0, y1, tt)


def rails(part, lod, w, d, x_rail, conn_w, conn_z, conn_h=0.11, rail_h=0.035):
    """Xinzhi blind-mate standard: two sealed slide rails (along Z) + a sealed connector rail block at the aft end"""
    for s in (-1, 1):
        rbox(part, SEAL, (s * x_rail, rail_h / 2, 0), (0.07, rail_h, d), 0.02, 1 if lod < 2 else 0)
    cz0, cz1 = conn_z
    rbox(part, SEAL, (0, conn_h / 2, (cz0 + cz1) / 2), (conn_w, conn_h, cz1 - cz0), 0.03, _cn(lod), 0.012)
    if lod < 2:   # sealed connector face: cyan seal line and the blind-mate keys
        box(part, CYN, (0, conn_h * 0.55, cz0 - 0.002), (conn_w * 0.7, 0.012, 0.006))
        if lod == 0:
            for s in (-1, 1):
                cyl(part, SEAL, (s * conn_w * 0.36, conn_h * 0.5, cz0), (s * conn_w * 0.36, conn_h * 0.5, cz0 - 0.03), 0.018, small_for(lod))
    if close():   # tamper seal (hologram tag) over the connector latch
        box(part, CYN, (conn_w * 0.42, conn_h + 0.001, (cz0 + cz1) / 2), (0.05, 0.002, 0.03))
        box(part, WHT, (-conn_w * 0.42, conn_h + 0.001, (cz0 + cz1) / 2), (0.06, 0.002, 0.025))


# ---------------------------------------------------------------- C-1 / C-3 comm (feeder + transceiver)
def c1_parts(lod):
    W, H, L = 1.442, 0.721, 1.442
    n, seg = _cn(lod), seg_for(lod)
    P = {}
    m = P["c1_mount"] = Part("c1_mount")
    rails(m, lod, W, L, 0.50, 0.70, (-L / 2, -L / 2 + 0.10))
    t = P["c1_transceiver"] = Part("c1_transceiver")
    hz0, hz1 = -L / 2 + 0.12, L / 2
    hw = W - 0.12
    rbox(t, BLK, (0, (0.035 + 0.30) / 2, (hz0 + hz1) / 2), (hw, 0.265, hz1 - hz0), 0.16, n, 0.03)
    rbox(t, WHT, (0, 0.31, (hz0 + hz1) / 2), (hw - 0.10, 0.02, hz1 - hz0 - 0.10), 0.12, n)          # white top deck
    if lod < 2:
        band(t, CYN, hw, hz1 - hz0, 0.16, n, 0.19, 0.205, 0.004, (0, 0, (hz0 + hz1) / 2))         # status strip
    for o0, o1 in ((hw / 2, W / 2), (-W / 2, -hw / 2)):                                       # side fin arrays
        fins(t, lod, "z", hz0 + 0.18, hz1 - 0.18, 16, o0, o1, 0.07, 0.27)
    f = P["c1_feeder"] = Part("c1_feeder")
    zc = (hz0 + hz1) / 2
    ylathe(f, WHT, [(0.22, 0.32), (0.22, 0.40), (0.60, 0.40), (0.60, 0.46)] +
           [(0.60 * math.cos(math.radians(a)), 0.46 + 0.261 * math.sin(math.radians(a))) for a in range(15, 91, 15)], seg,
           t=(0, 0, zc), soft=35)
    if lod < 2:
        ylathe(f, CYN, crect(0.598, 0.606, 0.462, 0.476), seg, t=(0, 0, zc))
        ylathe(f, BLK, [(0.20, 0.401), (0.21, 0.401), (0.21, 0.41)], seg, t=(0, 0, zc))
    if close():   # sealed feeder gland at the radome foot and the tamper seal
        box(f, SEAL, (0, 0.36, zc - 0.25), (0.14, 0.06, 0.08))
        box(f, CYN, (0, 0.391, zc - 0.25), (0.04, 0.002, 0.05))
    return P


def c1_hits():
    W, L = 1.442, 1.442
    hz0, hz1 = -L / 2 + 0.12, L / 2
    zc = (hz0 + hz1) / 2
    return [hbox("c1_mount", (0, 0.055, -L / 2 + 0.05), (0.70, 0.11, 0.10)),
            hbox("c1_mount", (0, 0.0175, 0), (1.07, 0.035, L)),
            hbox("c1_transceiver", (0, 0.17, zc), (W, 0.28, hz1 - hz0)),
            hcyl("c1_feeder", (0, 0.32, zc), (0, 0.62, zc), 0.60),
            hcyl("c1_feeder", (0, 0.62, zc), (0, 0.721, zc), 0.36)]


C3 = dict(W=2.154, H=1.077, L=2.154)


def c3_parts(lod):
    W, H, L = C3["W"], C3["H"], C3["L"]
    n, seg = _cn(lod), seg_for(lod)
    P = {}
    m = P["c3_mount"] = Part("c3_mount")
    rails(m, lod, W, L, 0.78, 1.00, (-L / 2, -L / 2 + 0.12), 0.14)
    t = P["c3_transceiver"] = Part("c3_transceiver")
    hz0, hz1 = -L / 2 + 0.14, L / 2 - 0.08
    hw, hd = W - 0.16, hz1 - hz0
    zc = (hz0 + hz1) / 2
    rbox(t, BLK, (0, (0.035 + 0.36) / 2, zc), (hw, 0.325, hd), 0.24, n, 0.035)                    # lower tier
    rbox(t, WHT, (0, 0.43, zc), (1.56, 0.14, 1.56), 0.20, n, 0.03)                               # upper tier (white)
    if lod < 2:
        band(t, CYN, hw, hd, 0.24, n, 0.25, 0.268, 0.004, (0, 0, zc))
        band(t, CYN, 1.56, 1.56, 0.20, n, 0.43, 0.44, 0.003, (0, 0, zc))
    for o0, o1 in ((hw / 2, W / 2), (-W / 2, -hw / 2)):                                       # fins on 3 sides
        fins(t, lod, "z", zc - 0.62, zc + 0.62, 20, o0, o1, 0.08, 0.33)
    fins(t, lod, "x", -0.62, 0.62, 20, hz1, L / 2, 0.08, 0.33)
    # secure-link (feeder): faceted octagonal radome with a cyan crown ring
    f = P["c3_feeder"] = Part("c3_feeder")
    oc = lambda rad: _ngon(rad, 8, math.pi / 8)
    rings = [_ring(oc(0.80), 0.50, (0, 0, zc)), _ring(oc(0.80), 0.64, (0, 0, zc)), _ring(oc(0.58), 0.94, (0, 0, zc)),
             _ring(oc(0.30), 1.077, (0, 0, zc))]
    loft(f, WHT, rings, cap0=False, soft=20)
    if lod < 2:
        loft(f, CYN, [_ring(oc(0.80), 0.62, (0, 0, zc)), _ring(oc(0.806), 0.62, (0, 0, zc)), _ring(oc(0.806), 0.64, (0, 0, zc)),
                      _ring(oc(0.80), 0.64, (0, 0, zc))], cap0=False, cap1=False)
        loft(f, BLK, [_ring(oc(0.24), 1.077, (0, 0, zc)), _ring(oc(0.24), 1.079, (0, 0, zc))], cap0=False)   # top window
        ylathe(f, BLK, [(0.86, 0.50), (0.86, 0.52), (0.40, 0.52)], seg, t=(0, 0, zc))                        # radome base ring
    if close():
        for k in range(8):           # sealed facet seams
            a = math.pi / 8 + 2 * math.pi * k / 8
            ca, sa = math.cos(a), math.sin(a)
            p0 = (0.80 * ca, 0.64, zc + 0.80 * sa)
            p1 = (0.58 * ca, 0.94, zc + 0.58 * sa)
            tube(f, SEAL, [p0, p1], 0.006, 6)
        box(f, SEAL, (0, 0.55, zc - 0.84), (0.18, 0.06, 0.10))
        box(f, CYN, (0, 0.581, zc - 0.84), (0.05, 0.002, 0.06))
    return P


def c3_hits():
    W, L = C3["W"], C3["L"]
    hz0, hz1 = -L / 2 + 0.14, L / 2 - 0.08
    zc = (hz0 + hz1) / 2
    return [hbox("c3_mount", (0, 0.07, -L / 2 + 0.06), (1.00, 0.14, 0.12)),
            hbox("c3_mount", (0, 0.0175, 0), (1.63, 0.035, L)),
            hbox("c3_transceiver", (0, 0.20, zc), (W, 0.33, hz1 - hz0 + 0.08)),
            hbox("c3_transceiver", (0, 0.43, zc), (1.56, 0.14, 1.56)),
            hcyl("c3_feeder", (0, 0.50, zc), (0, 0.80, zc), 0.80),
            hcyl("c3_feeder", (0, 0.80, zc), (0, 1.077, zc), 0.55)]


# ---------------------------------------------------------------- MC-C1 / MC-C3 computers (board, memory, psu)
def mcc1_parts(lod):
    W, H, L = 1.305, 0.783, 1.305
    n = _cn(lod)
    P = {}
    m = P["mcc1_mount"] = Part("mcc1_mount")
    rails(m, lod, W, L, 0.50, 0.70, (-L / 2, -L / 2 + 0.08))
    ch = P["mcc1_chassis"] = Part("mcc1_chassis")
    z0 = -L / 2 + 0.09
    rbox(ch, BLK, (0, 0.085, (z0 + L / 2) / 2), (W, 0.10, L / 2 - z0), 0.12, n, 0.025)              # plinth
    rbox(ch, BLK, (0, 0.46, z0 + 0.03), (W, 0.65, 0.06), 0.03, 1, 0.0)                              # rear spine
    y0 = 0.135
    # psu: rear block, black, fin crown on top, cyan status on its side
    p = P["mcc1_psu"] = Part("mcc1_psu")
    pz0, pz1 = z0 + 0.07, z0 + 0.45
    rbox(p, BLK, (0, (y0 + 0.68) / 2, (pz0 + pz1) / 2), (W - 0.04, 0.68 - y0, pz1 - pz0), 0.10, n, 0.03)
    fins(p, lod, "x", -0.58, 0.58, 22, pz0 + 0.03, pz1 - 0.03, 0.68, H)
    if lod < 2:
        for s in (-1, 1):
            box(p, CYN, (s * (W / 2 - 0.018), 0.55, (pz0 + pz1) / 2), (0.006, 0.016, 0.24))
    # board: front-left white monolith with a gloss-black inset top
    b = P["mcc1_board"] = Part("mcc1_board")
    bz0, bz1 = pz1 + 0.035, L / 2
    bx0, bx1 = -W / 2, 0.02
    rbox(b, WHT, ((bx0 + bx1) / 2, (y0 + H) / 2, (bz0 + bz1) / 2), (bx1 - bx0, H - y0, bz1 - bz0), 0.09, n, 0.03)
    rbox(b, BLK, ((bx0 + bx1) / 2, H + 0.002, (bz0 + bz1) / 2), (bx1 - bx0 - 0.12, 0.004, bz1 - bz0 - 0.12), 0.06, n)
    if lod < 2:
        box(b, CYN, (bx0 + 0.10, 0.45, bz1 + 0.002), (0.016, 0.40, 0.006))
    # memory: 4 sealed cartridges, cyan tips
    me = P["mcc1_memory"] = Part("mcc1_memory")
    mx0, mx1 = 0.06, W / 2
    k = 4
    cw = (mx1 - mx0 - 0.02 * (k - 1)) / k
    for i in range(k):
        x = mx0 + cw / 2 + i * (cw + 0.02)
        rbox(me, BLK, (x, (y0 + H - 0.03) / 2, (bz0 + bz1) / 2), (cw, H - 0.03 - y0, bz1 - bz0), 0.04, max(1, n - 1), 0.025)
        if lod < 2:
            box(me, CYN, (x, H - 0.03 + 0.004, bz1 - 0.10), (cw * 0.6, 0.008, 0.06))
        if close():   # recessed pull grip and tamper seal per cartridge
            box(me, SEAL, (x, 0.62, bz1 + 0.004), (cw * 0.5, 0.05, 0.008))
            box(me, WHT, (x, H - 0.026, (bz0 + bz1) / 2), (cw * 0.5, 0.003, 0.03))
    if close():   # flat sealed harness from the psu to the spine connector
        for s in (-1, 1):
            tube(ch, SEAL, [(s * 0.55, 0.14, z0 + 0.02), (s * 0.55, 0.30, z0 - 0.0), (s * 0.40, 0.40, z0 - 0.0)], 0.012, 6)
    return P


def mcc1_hits():
    W, H, L = 1.305, 0.783, 1.305
    z0 = -L / 2 + 0.09
    pz0, pz1 = z0 + 0.07, z0 + 0.45
    bz0 = pz1 + 0.035
    return [hbox("mcc1_mount", (0, 0.055, -L / 2 + 0.04), (0.70, 0.11, 0.08)),
            hbox("mcc1_mount", (0, 0.0175, 0), (1.07, 0.035, L)),
            hbox("mcc1_chassis", (0, 0.085, (z0 + L / 2) / 2), (W, 0.10, L / 2 - z0)),
            hbox("mcc1_chassis", (0, 0.46, z0 + 0.03), (W, 0.65, 0.06)),
            hbox("mcc1_psu", (0, (0.135 + H) / 2, (pz0 + pz1) / 2), (W - 0.04, H - 0.135, pz1 - pz0)),
            hbox("mcc1_board", ((-W / 2 + 0.02) / 2, (0.135 + H) / 2, (bz0 + L / 2) / 2), (W / 2 + 0.02, H - 0.135, L / 2 - bz0)),
            hbox("mcc1_memory", ((0.06 + W / 2) / 2, (0.135 + H) / 2, (bz0 + L / 2) / 2), (W / 2 - 0.06, H - 0.135, L / 2 - bz0))]


def mcc3_parts(lod):
    W, H, L = 1.908, 1.145, 1.908
    n = _cn(lod)
    P = {}
    m = P["mcc3_mount"] = Part("mcc3_mount")
    rails(m, lod, W, L, 0.70, 0.90, (-L / 2, -L / 2 + 0.10), 0.13)
    ch = P["mcc3_chassis"] = Part("mcc3_chassis")
    z0 = -L / 2 + 0.11
    rbox(ch, BLK, (0, 0.09, (z0 + L / 2) / 2), (W, 0.11, L / 2 - z0), 0.16, n, 0.03)                 # plinth
    rbox(ch, WHT, (0, H - 0.03, (z0 + L / 2) / 2), (1.06, 0.06, L / 2 - z0), 0.08, n, 0.025)        # top bridge
    y0 = 0.145
    # psu: twin towers at the sides, vertical fins on the outer faces
    p = P["mcc3_psu"] = Part("mcc3_psu")
    tw = 0.34
    for s in (-1, 1):
        xc = s * (W / 2 - 0.08 - tw / 2)
        rbox(p, BLK, (xc, (y0 + H) / 2, (z0 + L / 2) / 2), (tw, H - y0, L / 2 - z0), 0.10, n, 0.035)
        o0, o1 = sorted((xc + s * tw / 2, s * W / 2))
        fins(p, lod, "z", z0 + 0.10, L / 2 - 0.10, 26, o0, o1, y0 + 0.08, H - 0.08)
        if lod < 2:
            box(p, CYN, (xc, H + 0.002, (z0 + L / 2) / 2), (0.05, 0.006, L / 2 - z0 - 0.3))
    # board stack: three horizontal drawers in the front half
    b = P["mcc3_board"] = Part("mcc3_board")
    ix = W / 2 - 0.08 - tw - 0.03
    bz0, bz1 = 0.0, L / 2
    dh = (H - 0.06 - y0 - 0.04) / 3
    for i in range(3):
        yc = y0 + dh / 2 + i * (dh + 0.02)
        rbox(b, WHT, (0, yc, (bz0 + bz1) / 2), (2 * ix, dh, bz1 - bz0), 0.06, n, 0.025)
        rbox(b, BLK, (0, yc, bz1 + 0.004), (2 * ix - 0.10, dh - 0.08, 0.01), 0.03, max(1, n - 1), 0.0, axis="z")
        if lod < 2:
            box(b, CYN, (ix - 0.12, yc, bz1 + 0.011), (0.10, 0.016, 0.004))
        if close():
            box(b, SEAL, (0, yc - dh * 0.25, bz1 + 0.013), (0.30, 0.03, 0.008))
    # memory: a row of 6 cartridges standing in the rear half
    me = P["mcc3_memory"] = Part("mcc3_memory")
    mz0, mz1 = z0 + 0.03, -0.03
    k = 6
    cw = (2 * ix - 0.02 * (k - 1)) / k
    for i in range(k):
        x = -ix + cw / 2 + i * (cw + 0.02)
        rbox(me, BLK, (x, (y0 + H - 0.08) / 2, (mz0 + mz1) / 2), (cw, H - 0.08 - y0, mz1 - mz0), 0.04, max(1, n - 1), 0.025)
        if lod < 2:
            box(me, CYN, (x, H - 0.08 + 0.004, mz1 - 0.08), (cw * 0.6, 0.008, 0.06))
        if close():
            box(me, WHT, (x, H - 0.076, (mz0 + mz1) / 2), (cw * 0.5, 0.003, 0.04))
    if close():
        for s in (-1, 1):
            tube(ch, SEAL, [(s * 0.35, y0, z0 - 0.01), (s * 0.35, 0.40, z0 - 0.02), (s * 0.60, 0.70, z0 - 0.02)], 0.014, 6)
    return P


def mcc3_hits():
    W, H, L = 1.908, 1.145, 1.908
    z0 = -L / 2 + 0.11
    tw = 0.34
    ix = W / 2 - 0.08 - tw - 0.03
    H_ = [hbox("mcc3_mount", (0, 0.065, -L / 2 + 0.05), (0.90, 0.13, 0.10)),
          hbox("mcc3_mount", (0, 0.0175, 0), (1.47, 0.035, L)),
          hbox("mcc3_chassis", (0, 0.09, (z0 + L / 2) / 2), (W, 0.11, L / 2 - z0)),
          hbox("mcc3_chassis", (0, H - 0.03, (z0 + L / 2) / 2), (1.06, 0.06, L / 2 - z0))]
    for s in (-1, 1):
        H_.append(hbox("mcc3_psu", (s * (W / 2 - (tw + 0.08) / 2), (0.145 + H) / 2, (z0 + L / 2) / 2), (tw + 0.08, H - 0.145, L / 2 - z0)))
    H_ += [hbox("mcc3_board", (0, (0.145 + H - 0.06) / 2, L / 4), (2 * ix, H - 0.06 - 0.145, L / 2)),
           hbox("mcc3_memory", (0, (0.145 + H - 0.08) / 2, (z0 + 0.03 - 0.03) / 2), (2 * ix, H - 0.08 - 0.145, -0.06 - z0))]
    return H_


# ---------------------------------------------------------------- N-2 / N-10 radars (array, rx)
def _tile(part, lod, centre, outward, tangent, size, mat=BLK):
    """an array tile on a sloped face: gloss tile on a cyan frame line, oriented by outward normal and face tangent"""
    o, t = norm(outward), norm(tangent)
    b = norm(_cross(o, t))
    R = [[t[0], o[0], b[0]], [t[1], o[1], b[1]], [t[2], o[2], b[2]]]   # local x = tangent, y = outward, z = slope
    if lod < 2:
        box(part, CYN, tuple(centre[k] + o[k] * 0.006 for k in range(3)), (size[0] + 0.03, 0.012, size[1] + 0.03), R)
    box(part, mat, tuple(centre[k] + o[k] * 0.016 for k in range(3)), (size[0], 0.02, size[1]), R)
    if close():   # element grid: thin seal lines across the tile
        for i in range(1, 6):
            u = -size[0] / 2 + size[0] * i / 6
            p = tuple(centre[k] + o[k] * 0.027 + t[k] * u for k in range(3))
            box(part, SEAL, p, (0.006, 0.003, size[1] * 0.96), R)
        for i in range(1, 4):
            v = -size[1] / 2 + size[1] * i / 4
            p = tuple(centre[k] + o[k] * 0.027 + b[k] * v for k in range(3))
            box(part, SEAL, p, (size[0] * 0.96, 0.003, 0.006), R)


def n2_parts(lod):
    W, H, L = 2.047, 0.819, 2.047
    n = _cn(lod)
    P = {}
    m = P["n2_mount"] = Part("n2_mount")
    rails(m, lod, W, L, 0.80, 0.80, (-L / 2, -L / 2 + 0.06), 0.10)
    h = P["n2_housing"] = Part("n2_housing")
    pz0 = -L / 2 + 0.07
    pd = L / 2 - pz0
    zc = (pz0 + L / 2) / 2
    rbox(h, BLK, (0, (0.035 + 0.15) / 2, zc), (W, 0.115, pd), 0.30, n, 0.03)                      # plinth
    yb, yt, wb, wt = 0.15, 0.62, 1.86, 1.06
    loft(h, WHT, [_ring(_rr(wb, wb - (W - pd), 0.26, n), yb, (0, 0, zc)), _ring(_rr(wt, wt, 0.14, n), yt, (0, 0, zc))], cap0=False, soft=50)
    if lod < 2:
        band(h, CYN, W, pd, 0.30, n, 0.10, 0.115, 0.004, (0, 0, zc))
    # four array tiles on the sloped faces
    a = P["n2_array"] = Part("n2_array")
    run, rise = (wb - wt) / 2, yt - yb
    sl = math.hypot(run, rise)
    for k in range(4):
        th = k * math.pi / 2
        c, s = math.cos(th), math.sin(th)
        rmid = (wb + wt) / 4
        centre = (rmid * c, (yb + yt) / 2, zc + rmid * s)
        outward = (rise * c, run, rise * s)
        tangent = (-s, 0, c)
        _tile(a, lod, centre, outward, tangent, (0.86, sl * 0.78))
    # rx crown module with a fin crown and a cyan ring
    r = P["n2_rx"] = Part("n2_rx")
    rbox(r, BLK, (0, 0.68, zc), (0.96, 0.12, 0.96), 0.12, n, 0.03)
    fins(r, lod, "x", -0.40, 0.40, 18, zc - 0.40, zc + 0.40, 0.74, H)
    if lod < 2:
        band(r, CYN, 0.96, 0.96, 0.12, n, 0.66, 0.675, 0.004, (0, 0, zc))
    if close():
        box(r, SEAL, (0.50, 0.68, zc), (0.05, 0.06, 0.16))      # sealed rx feed gland
        box(r, CYN, (0.526, 0.68, zc), (0.002, 0.02, 0.05))
    return P


def n2_hits():
    W, H, L = 2.047, 0.819, 2.047
    pz0 = -L / 2 + 0.07
    zc = (pz0 + L / 2) / 2
    H_ = [hbox("n2_mount", (0, 0.05, -L / 2 + 0.03), (0.80, 0.10, 0.06)),
          hbox("n2_housing", (0, 0.0925, zc), (W, 0.115, L / 2 - pz0)),
          hbox("n2_housing", (0, 0.385, zc), (1.30, 0.47, 1.30))]
    for k in range(4):     # tiles: thin boxes on the 4 slopes (tilted about the face tangent)
        th = k * math.pi / 2
        rmid = (1.86 + 1.06) / 4 + 0.02
        ang = math.degrees(math.atan2(0.40, 0.47))
        if k % 2 == 0:
            H_.append(hbox("n2_array", (rmid * math.cos(th), 0.40, zc), (0.10, 0.55, 0.88), (0, 0, ang * math.cos(th))))
        else:
            H_.append(hbox("n2_array", (0, 0.40, zc + rmid * math.sin(th)), (0.88, 0.55, 0.10), (-ang * math.sin(th), 0, 0)))
    H_ += [hbox("n2_rx", (0, 0.72, zc), (0.96, 0.20, 0.96))]
    return H_


N10_A0 = math.pi / 8          # octagon flats face +-X / +-Z


def n10_parts(lod):
    W, H, L = 3.057, 1.223, 3.057
    n, seg = _cn(lod), seg_for(lod)
    P = {}
    oc = lambda across: _ngon(across / 2 / math.cos(math.pi / 8), 8, N10_A0)
    m = P["n10_mount"] = Part("n10_mount")
    for s in (-1, 1):
        rbox(m, SEAL, (s * 0.95, 0.0175, 0), (0.08, 0.035, 2.20), 0.02, 1 if lod < 2 else 0)
    # the connector rail sits in the aft-left square corner outside the octagon (the standard's aft block, diagonal)
    Rc = rot_y(math.pi / 4)
    cpos = (-1.30, 0.07, -1.30)
    box(m, SEAL, cpos, (0.50, 0.14, 0.18), Rc, chamfer=0.03)
    if lod < 2:
        box(m, CYN, (cpos[0] - 0.064, 0.08, cpos[2] - 0.064), (0.36, 0.014, 0.01), Rc)
    if close():
        box(m, CYN, (cpos[0] + 0.1, 0.141, cpos[2] - 0.1), (0.05, 0.002, 0.03), Rc)
    h = P["n10_housing"] = Part("n10_housing")
    loft(h, BLK, [_ring(oc(W), 0.035), _ring(oc(W), 0.13), _ring(oc(W - 0.10), 0.17)], soft=20)          # plinth
    loft(h, WHT, [_ring(oc(2.90), 0.17), _ring(oc(1.70), 0.86), _ring(oc(1.62), 0.90)], cap0=False, soft=20)
    if lod < 2:
        loft(h, CYN, [_ring(oc(W), 0.09), _ring(oc(W + 0.008), 0.09), _ring(oc(W + 0.008), 0.105), _ring(oc(W), 0.105)],
             cap0=False, cap1=False, soft=20)
    a = P["n10_array"] = Part("n10_array")
    run, rise = (2.90 - 1.70) / 2, 0.86 - 0.17
    sl = math.hypot(run, rise)
    for k in range(8):
        th = k * math.pi / 4
        c, s = math.cos(th), math.sin(th)
        rmid = (2.90 + 1.70) / 4
        flat = 2 * rmid * math.tan(math.pi / 8)
        _tile(a, lod, (rmid * c, (0.17 + 0.86) / 2, rmid * s), (rise * c, run, rise * s), (-s, 0, c), (flat * 0.80, sl * 0.80))
    # rx: four sealed quarter modules on the top deck around the cyan beacon
    r = P["n10_rx"] = Part("n10_rx")
    for k in range(4):
        th = math.pi / 4 + k * math.pi / 2
        c, s = math.cos(th), math.sin(th)
        ctr = (0.38 * c, 1.0, 0.38 * s)
        rbox(r, BLK, ctr, (0.46, 0.20, 0.46), 0.08, n, 0.03)
        if lod < 2:
            band(r, CYN, 0.46, 0.46, 0.08, n, 0.95, 0.962, 0.004, ctr)
        fins(r, lod, "x" if k % 2 else "z", (0.38 * (c if k % 2 else s)) - 0.18, (0.38 * (c if k % 2 else s)) + 0.18, 10,
             (0.38 * (s if k % 2 else c)) - 0.17, (0.38 * (s if k % 2 else c)) + 0.17, 1.10, 1.16)
    ylathe(r, BLK, [(0.13, 0.90), (0.13, 1.12), (0.09, 1.16), (0, 1.16)], small_for(lod))
    ylathe(r, CYN, [(0.07, 1.16), (0.07, 1.20), (0.05, H), (0, H)], small_for(lod), soft=40)
    if close():
        for k in range(4):
            th = math.pi / 4 + k * math.pi / 2
            tube(r, SEAL, [(0.16 * math.cos(th), 0.93, 0.16 * math.sin(th)), (0.20 * math.cos(th), 0.93, 0.20 * math.sin(th))], 0.012, 6)
    del seg
    return P


def n10_hits():
    W, H = 3.057, 1.223
    H_ = [hbox("n10_mount", (-1.30, 0.07, -1.30), (0.50, 0.14, 0.18), (0, 45, 0)),
          hcyl("n10_housing", (0, 0.035, 0), (0, 0.17, 0), W / 2 * 1.02),
          hcyl("n10_housing", (0, 0.17, 0), (0, 0.90, 0), 0.95)]
    for k in range(8):
        th = k * math.pi / 4
        rmid = (2.90 + 1.70) / 4 + 0.02
        ang = math.degrees(math.atan2(0.60, 0.69))
        H_.append(hbox("n10_array", (rmid * math.cos(th), 0.515, rmid * math.sin(th)), (0.10, 0.80, 0.85),
                       (0, -math.degrees(th), ang)))
    for k in range(4):
        th = math.pi / 4 + k * math.pi / 2
        H_.append(hbox("n10_rx", (0.38 * math.cos(th), 1.03, 0.38 * math.sin(th)), (0.46, 0.26, 0.46)))
    H_.append(hcyl("n10_rx", (0, 0.90, 0), (0, H, 0), 0.13))
    return H_


ASSETS = {"xinzhi_c1_comm": c1_parts, "xinzhi_c3_comm": c3_parts, "xinzhi_mcc1_computer": mcc1_parts,
          "xinzhi_mcc3_computer": mcc3_parts, "xinzhi_n2_radar": n2_parts, "xinzhi_n10_radar": n10_parts}
HITS = {"xinzhi_c1_comm": c1_hits, "xinzhi_c3_comm": c3_hits, "xinzhi_mcc1_computer": mcc1_hits,
        "xinzhi_mcc3_computer": mcc3_hits, "xinzhi_n2_radar": n2_hits, "xinzhi_n10_radar": n10_hits}
MODEL_ASSETS = {"xz.comm.c1": "xinzhi_c1_comm", "xz.comm.c3": "xinzhi_c3_comm", "xz.mc.c1": "xinzhi_mcc1_computer",
                "xz.mc.c3": "xinzhi_mcc3_computer", "xz.radar.n2": "xinzhi_n2_radar", "xz.radar.n10": "xinzhi_n10_radar"}
