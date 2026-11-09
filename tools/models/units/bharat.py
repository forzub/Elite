"""Bharat Assembly (HD 20794 New India, the Southern Circle's assembly rings): SM-100 refuge lifeboat, SP-500 settler
section (500 bunks).

School (DESIGN_SCHOOLS.md): licensed and improvised - licensed base shapes (rounded pressure hulls) with bolted-on extra
brackets, arched windows, mixed parts; saffron / white / green. Interface: licensed round flange + an extra adapter
bracket. Close-up: improvised cable runs on stand-off brackets, extra brackets, hull patches.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_bharat_saffron": ((0.90, 0.52, 0.12), (0.25, 0.22, 0.18), 50),
    "eq_bharat_white": ((0.82, 0.82, 0.80), (0.25, 0.25, 0.25), 50),
    "eq_bharat_green": ((0.10, 0.42, 0.22), (0.20, 0.25, 0.20), 50),
    "eq_bharat_glass": ((0.10, 0.14, 0.18), (0.80, 0.82, 0.85), 180),
    "eq_bharat_patch": ((0.55, 0.55, 0.52), (0.30, 0.30, 0.30), 60),      # bolted-on licensed / spare parts, raw metal
    "eq_bharat_seal": ((0.09, 0.09, 0.09), (0.10, 0.10, 0.10), 20),
    "eq_bharat_beacon": ((0.85, 0.22, 0.12), (0.50, 0.40, 0.40), 120),
})

SAF, WHT, GRN, GLS, PAT = "eq_bharat_saffron", "eq_bharat_white", "eq_bharat_green", "eq_bharat_glass", "eq_bharat_patch"


# ---------------------------------------------------------------- local helpers
def extrude(part, mat, poly, w0, w1, R=I3, t=(0, 0, 0), caps=(True, True), inward=False, soft=30.0):
    """a convex local (x, y) polygon extruded along local z from w0 to w1; corners flatter than `soft` read smooth"""
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

    def vn(v, e):
        a = en[e]
        b = en[(v - 1) % n] if v == e else en[v % n]
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
    """a hoop between two corresponding polygons, extruded from w0 to w1"""
    extrude(part, mat, outer, w0, w1, R, t, (False, False))
    extrude(part, mat, inner, w0, w1, R, t, (False, False), inward=True)
    n = len(outer)
    s0 = 1.0 if w0 > w1 else -1.0
    for w, s in ((w0, s0), (w1, -s0)):
        for i in range(n):
            j = (i + 1) % n
            pts = [(outer[i][0], outer[i][1], w), (outer[j][0], outer[j][1], w), (inner[j][0], inner[j][1], w), (inner[i][0], inner[i][1], w)]
            part.face(mat, [part.vert(p, (0, 0, s), R, t) for p in pts], apply(R, (0, 0, s)))


def rrect2d(hw, hh, r, n):
    """counter-clockwise rounded rectangle (half sizes hw, hh, corner radius r, n segments per corner)"""
    pts = []
    for cx, cy, a0 in ((hw - r, -hh + r, -90), (hw - r, hh - r, 0), (-hw + r, hh - r, 90), (-hw + r, -hh + r, 180)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def arch(w, h, n):
    """an arched window outline (convex): width w, total height h, semicircular top; base at y = 0"""
    r = w / 2
    pts = [(r, 0.0), (r, h - r)]
    pts += [(r * math.cos(math.pi * i / n), h - r + r * math.sin(math.pi * i / n)) for i in range(1, n)]
    return pts + [(-r, h - r), (-r, 0.0)]


def loft(part, mat, secs, R=I3, t=(0, 0, 0), cap0=False, cap1=False):
    """skin between closed sections (lists of local 3D points, equal counts); smooth shared normals, outward from
    the section centres; optional flat caps on the first / last section"""
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


def window(part_frame, part_glass, lod, w, h, x, yb, z, side, frame=WHT):
    """an arched window on a side wall x = const (side = +1 / -1): frame proud 0.03, glass proud 0.036"""
    n = 6 if lod == 0 else 3
    R = rot_y(side * math.pi / 2)          # local z -> world +-X, local x -> world -+Z
    if lod > 0:                            # mid distance: the glass only
        extrude(part_glass, GLS, arch(w, h, n), 0.0, 0.036, R, (x, yb, z), caps=(False, True))
        return
    extrude(part_frame, frame, [(px, py) for px, py in arch(w + 0.10, h + 0.08, n)], 0.0, 0.03, R, (x, yb - 0.04, z))
    extrude(part_glass, GLS, arch(w, h, n), 0.0, 0.036, R, (x, yb, z))


def bh_flange(f, lod, r, t, z_face, s, neck_r, neck_len, adapter=True, aa=-math.pi / 4):
    """licensed round flange: mating face at z_face facing s (+1 / -1), neck back into the unit, 12 bolts, and the
    extra Bharat adapter bracket bolted off-centre on its back face (the 'improvised' interface)"""
    seg, small = seg_for(lod), small_for(lod)
    R = I3 if s < 0 else rot_x(math.pi)    # local +Z (into the unit) -> world -s
    lathe(f, PAT, [(0, 0), (r, 0), (r, t), (neck_r, t), (neck_r, t + neck_len), (0, t + neck_len)], seg, R, (0, 0, z_face))
    if lod < 2:
        lathe(f, "eq_bharat_seal", crect(0.55 * r, 0.62 * r, -0.008, 0.0), seg, R, (0, 0, z_face))
    if adapter:                                # extra adapter bracket: arm off the flange rim with a second (adapter) socket
        Ra = rot_z(aa)
        zb = z_face - s * (t + 0.05)
        box(f, SAF, apply(Ra, (1.12 * r, 0, 0))[:2] + (zb,), (0.60 * r, 0.26 * r, 0.10), Ra)
        q = apply(Ra, (1.32 * r, 0, 0))
        cyl(f, SAF, (q[0], q[1], zb), (q[0], q[1], z_face), 0.16 * r, small)
        if lod < 2:
            cyl(f, "eq_bharat_seal", (q[0], q[1], z_face), (q[0], q[1], z_face + s * 0.006), 0.10 * r, small)
            box(f, SAF, apply(Ra, (1.12 * r, 0, 0))[:2] + (z_face - s * (t + 0.10 + neck_len * 0.4),), (0.10 * r, 0.10 * r, neck_len * 0.8), Ra)
    if lod == 0:
        for i in range(12):
            a = 2 * math.pi * (i + 0.5) / 12
            hex_bolt(f, "eq_steel", (0.85 * r * math.cos(a), 0.85 * r * math.sin(a), z_face - s * t), (0, 0, -s), max(0.06, 0.06 * r), 0.04)
        for d in (0.92, 1.05):                                           # extra bolts on the adapter arm
            q = apply(Ra, (d * r, 0, 0))
            hex_bolt(f, "eq_steel", (q[0], q[1], zb + s * 0.05), (0, 0, s), 0.06, 0.035, washer=True)


# ---------------------------------------------------------------- SM-100 refuge lifeboat (100 seats)
# Passport envelope [3.731, 2.612, 8.209]; long axis Z, the docking / release flange on -Z, saffron nose on +Z.
# Fault class lifeboat: battery (pack under the belly), release (4 pyro release locks round the flange), beacon (SOS
# beacon on its improvised mast bracket on top). Licensed rounded pressure hull, a row of arched windows on a saffron
# band, green stripe, two raw-metal hoops bolted on later.
RF = dict(hw=1.75, hh=1.10, r=0.65, z0=-3.55, z1=3.15, zn=4.06, zf=-4.1045)


def refuge100_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = RF
    hw, hh, r = d["hw"], d["hh"], d["r"]
    nc = 4 if lod == 0 else (3 if lod == 1 else 2)
    P = {}
    hl = P["refuge100_hull"] = Part("refuge100_hull")
    sec = rrect2d(hw, hh, r, nc)
    extrude(hl, WHT, sec, d["z0"], d["z1"], caps=(True, False))
    nose = [(d["z1"], 1.0, 1.0), (3.45, 0.97, 0.96), (3.72, 0.88, 0.86), (3.92, 0.72, 0.70), (d["zn"], 0.48, 0.46)]
    if lod == 2:
        nose = nose[::2]
    loft(hl, SAF, [[(x * sx, y * sy, z) for x, y in sec] for z, sx, sy in nose], cap1=True)
    if lod < 2:
        for s in (-1, 1):                                                 # saffron window band, green stripe
            box(hl, SAF, (s * (hw + 0.005), 0.0, -0.2), (0.01, 0.74, 6.3))
            box(hl, GRN, (s * (hw + 0.005), -0.43, -0.2), (0.01, 0.07, 6.3))
            for k in range(6):
                window(hl, hl, lod, 0.34, 0.56, s * hw, -0.28, -2.40 + 0.88 * k, s)
        for zz in (-1.15, 1.60):                                          # bolted-on hoops (raw metal, later fit)
            poly_ring(hl, PAT, rrect2d(hw + 0.04, hh + 0.04, r + 0.04, nc), sec, zz - 0.08, zz + 0.08)
        lathe(hl, GLS, [(0, 0.0), (0.22, 0.0), (0.22, 0.02), (0, 0.035)], small, I3, (0, 0.18, d["zn"]))       # nose port
    if lod == 0:
        for s in (-1, 1):
            for zz in (-1.15, 1.60):
                for yy in (-0.3, 0.3):
                    hex_bolt(hl, "eq_steel", (s * (hw + 0.04), yy, zz), (s, 0, 0), 0.06, 0.03)
    # release: 4 pyro release locks on the aft bulkhead round the flange
    rl = P["refuge100_release"] = Part("refuge100_release")
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        x, y = 1.12 * math.cos(a), 0.78 * math.sin(a)
        box(rl, SAF, (x, y, d["z0"] - 0.13), (0.30, 0.26, 0.26), rot_z(a), chamfer=0.03 if lod == 0 else 0.0)
        if lod < 2:
            cyl(rl, "eq_bharat_beacon", (x, y, d["z0"] - 0.26), (x, y, d["z0"] - 0.34), 0.07, small)          # pyro cap
        if lod == 0:
            box(rl, PAT, (x, y, d["z0"] - 0.02), (0.42, 0.36, 0.04), rot_z(a))                               # extra plate
            for dx in (-0.16, 0.16):
                q = apply(rot_z(a), (dx, 0.15, 0))
                hex_bolt(rl, "eq_steel", (x + q[0], y + q[1], d["z0"] - 0.04), (0, 0, -1), 0.06, 0.03)
    fl = P["refuge100_flange"] = Part("refuge100_flange")
    bh_flange(fl, lod, 0.78, 0.12, d["zf"], -1, 0.60, d["z0"] - d["zf"] - 0.12, aa=-math.pi / 2)
    if lod < 2:                                                           # hatch with hand wheel
        lathe(fl, WHT, [(0, 0), (0.40, 0), (0.40, 0.012), (0, 0.012)], seg, rot_x(math.pi), (0, 0, d["zf"]))
        lathe(fl, "eq_steel", crect(0.13, 0.16, d["zf"] - 0.05, d["zf"] - 0.02), small)
    # battery pack under the belly, on two straps
    bt = P["refuge100_battery"] = Part("refuge100_battery")
    box(bt, GRN, (0, -hh - 0.09, 0.4), (1.30, 0.18, 2.2), chamfer=0.025 if lod == 0 else 0.0)
    if lod < 2:
        for zz in (-0.35, 1.15):
            box(bt, PAT, (0, -hh - 0.10, zz), (1.42, 0.21, 0.10))
        box(bt, WHT, (0.30, -hh - 0.181, 0.4), (0.40, 0.004, 0.6))
    if lod == 0:
        for zz in (-0.35, 1.15):
            for x in (-0.66, 0.66):
                hex_bolt(bt, "eq_steel", (x, -hh - 0.205, zz), (0, -1, 0), 0.06, 0.03)
        for zz in (-0.55, 1.35):                                         # terminals
            cyl(bt, "eq_copper", (-0.45, -hh - 0.09, zz), (-0.45, -hh - 0.09, zz + (0.12 if zz > 0 else -0.12)), 0.035, 8)
    # SOS beacon on its improvised bracket (top, aft of the nose)
    bc = P["refuge100_beacon"] = Part("refuge100_beacon")
    zb = 2.55
    box(bc, PAT, (0.25, hh + 0.015, zb), (0.50, 0.03, 0.36))
    cyl(bc, PAT, (0.35, hh + 0.03, zb), (0.35, hh + 0.12, zb), 0.045, small)
    lathe(bc, "eq_bharat_beacon", [(0, 0), (0.10, 0), (0.10, 0.03), (0.07, 0.08), (0, 0.10)], small, R_Y, (0.35, hh + 0.12, zb), soft=40)
    if lod < 2:
        box(bc, SAF, (0.06, hh + 0.06, zb), (0.04, 0.09, 0.30))                                            # extra L-bracket
    if lod == 0:
        for x in (0.05, 0.45):
            for zz in (zb - 0.13, zb + 0.13):
                hex_bolt(bc, "eq_steel", (x, hh + 0.03, zz), (0, 1, 0), 0.04, 0.02)
    if close():
        hz = P["refuge100_harness"] = Part("refuge100_harness")
        # improvised run: battery -> up the +X side -> beacon, on stand-off brackets
        path = [(0.55, -hh - 0.02, 1.25), (hw - 0.30, -hh + 0.10, 1.45), (hw + 0.06, -0.60, 1.55), (hw + 0.06, 0.55, 1.75),
                (hw - 0.25, hh + 0.04, 2.0), (0.45, hh + 0.05, 2.35)]
        harness(hz, bezier(path[0], path[1], path[1], path[2], 6) + [path[3]] + bezier(path[3], path[4], path[4], path[5], 6)[1:],
                0.018, 2, ("eq_bharat_seal", "eq_bharat_beacon"), ties="eq_steel", tie_every=3)
        for p in (path[2], path[3]):
            box(hz, PAT, (hw + 0.025, p[1], p[2]), (0.05, 0.06, 0.12))
        # patches on the hull
        for s, y, z, w, h in ((1, 0.62, -2.9, 0.40, 0.30), (-1, -0.75, 0.95, 0.55, 0.28), (1, -0.70, -1.9, 0.30, 0.22)):
            box(hz, PAT, (s * (hw + 0.006), y, z), (0.012, h, w))
            for dz in (-w / 2 + 0.04, w / 2 - 0.04):
                for dy in (-h / 2 + 0.04, h / 2 - 0.04):
                    hex_bolt(hz, "eq_steel", (s * (hw + 0.012), y + dy, z + dz), (s, 0, 0), 0.025, 0.012)
        box(hz, PAT, (0.0, hh + 0.006, -1.9), (0.7, 0.012, 0.5))
    return P


def refuge100_hits():
    d = RF
    hw, hh, r = d["hw"], d["hh"], d["r"]
    H = [hbox("refuge100_hull", (0, 0, (d["z0"] + d["z1"]) / 2), (2 * hw, 2 * (hh - r * 0.3), d["z1"] - d["z0"])),
         hbox("refuge100_hull", (0, 0, (d["z0"] + d["z1"]) / 2), (2 * (hw - r * 0.3), 2 * hh, d["z1"] - d["z0"])),
         hbox("refuge100_hull", (0, 0, (d["z1"] + d["zn"]) / 2), (2 * hw * 0.85, 2 * hh * 0.82, d["zn"] - d["z1"]))]
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        H.append(hbox("refuge100_release", (1.12 * math.cos(a), 0.78 * math.sin(a), d["z0"] - 0.17), (0.30, 0.26, 0.34), (0, 0, math.degrees(a))))
    H += [hcyl("refuge100_flange", (0, 0, d["zf"]), (0, 0, d["z0"]), 0.78),
          hbox("refuge100_battery", (0, -hh - 0.10, 0.4), (1.42, 0.21, 2.2)),
          hbox("refuge100_beacon", (0.25, hh + 0.11, 2.55), (0.50, 0.22, 0.36))]
    return H


# ---------------------------------------------------------------- SP-500 settler section (500 bunks)
# Passport envelope [12.515, 8.939, 17.878]; long axis Z, licensed round flanges on both ends (with the extra adapter
# bracket). Fault class passenger.cabin: seal (side doors and end hatches with their seal rings), climate (climate unit
# bolted on top on extra legs), power (distribution cabinet under the belly with its conduits). Two decks of arched
# windows on saffron bands, green waist stripe, raw-metal end hoops, 4 bolted-on mounting brackets underneath.
BK = dict(hw=6.10, hh=4.00, r=1.90, z0=-8.0, z1=8.0, ze=8.45, zf=8.939)


def bunk500_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = BK
    hw, hh, r = d["hw"], d["hh"], d["r"]
    nc = 5 if lod == 0 else (3 if lod == 1 else 2)
    P = {}
    hl = P["bunk500_hull"] = Part("bunk500_hull")
    sec = rrect2d(hw, hh, r, nc)
    extrude(hl, WHT, sec, d["z0"], d["z1"], caps=(False, False))
    for s in (-1, 1):                                                     # domed end bulkheads
        steps = [(d["z1"], 1.0), (8.25, 0.96), (d["ze"], 0.86)]
        loft(hl, SAF, [[(x * k, y * k, s * z) for x, y in sec] for z, k in steps], cap1=True)
    rows = (-1.15, 1.15)
    wz = [-6.75 + 1.5 * k for k in range(10)]
    if lod < 2:
        for s in (-1, 1):
            for yr in rows:
                box(hl, SAF, (s * (hw + 0.005), yr, 0), (0.01, 1.25, 14.6))
            box(hl, GRN, (s * (hw + 0.005), 0.0, 0), (0.01, 0.12, 14.6))
            box(hl, GRN, (s * (hw + 0.005), -1.92, 0), (0.01, 0.10, 14.6))
            for yr in rows:
                for z in wz:
                    if yr < 0 and abs(z) < 1.0:
                        continue                                          # the side door is here
                    window(hl, hl, lod, 0.60, 0.95, s * hw, yr - 0.475, z, s)
        for zz in (-7.55, 7.55):                                          # raw-metal end hoops
            poly_ring(hl, PAT, rrect2d(hw + 0.07, hh + 0.07, r + 0.07, nc), sec, zz - 0.15, zz + 0.15)
    else:
        for s in (-1, 1):
            for yr in rows:
                box(hl, GLS, (s * (hw + 0.01), yr, 0), (0.02, 0.7, 14.0))
    if lod == 0:
        for zz in (-7.55, 7.55):
            for s in (-1, 1):
                for yy in (-1.5, 0.0, 1.5):
                    hex_bolt(hl, "eq_steel", (s * (hw + 0.07), yy, zz), (s, 0, 0), 0.08, 0.04)
            for xx in (-2.5, 0.0, 2.5):
                for sy in (-1, 1):
                    hex_bolt(hl, "eq_steel", (xx, sy * (hh + 0.07), zz), (0, sy, 0), 0.08, 0.04)
    # seal: side doors (one per side, arched, with seal frame) and the seals of the end hatches
    sl = P["bunk500_hatches"] = Part("bunk500_hatches")
    for s in (-1, 1):
        R = rot_y(s * math.pi / 2)
        n = 8 if lod == 0 else 4
        extrude(sl, "eq_bharat_seal", arch(1.34, 2.20, n), 0.0, 0.04, R, (s * hw, -2.15, 0.0))
        extrude(sl, SAF, arch(1.22, 2.12, n), 0.0, 0.06, R, (s * hw, -2.11, 0.0))
        if lod < 2:
            extrude(sl, WHT, arch(1.0, 1.98, n), 0.0, 0.075, R, (s * hw, -2.04, 0.0))
            extrude(sl, GLS, arch(0.40, 0.55, n), 0.0, 0.08, R, (s * hw, -0.85, 0.0))
        if lod == 0:
            box(sl, "eq_steel", (s * (hw + 0.10), -1.15, 0.38 * s), (0.05, 0.30, 0.06))                  # lever
            for yy in (-1.85, -0.75):
                cyl(sl, "eq_steel", (s * (hw + 0.07), yy - 0.12, -0.61 * s), (s * (hw + 0.07), yy + 0.12, -0.61 * s), 0.04, 8)
    for zs in (-1, 1):                                                    # end hatch seal rings + hatch leaves
        zf = zs * d["zf"]
        lathe(sl, "eq_bharat_seal", crect(0.70, 0.82, -0.012, 0.0), seg, rot_x(math.pi) if zs > 0 else I3, (0, 0, zf))
        lathe(sl, WHT, [(0, -0.008), (0.70, -0.008), (0.70, 0.0), (0, 0.0)], seg, rot_x(math.pi) if zs > 0 else I3, (0, 0, zf))
        if lod < 2:
            cyl(sl, "eq_steel", (0, 0, zf + zs * 0.008), (0, 0, zf + zs * 0.06), 0.10, small)
    # climate unit on top, on extra legs
    cl = P["bunk500_climate"] = Part("bunk500_climate")
    yc0, yc1 = hh + 0.08, 4.465
    box(cl, WHT, (0, (yc0 + yc1) / 2, -1.0), (3.2, yc1 - yc0, 4.2), chamfer=0.04 if lod == 0 else 0.0)
    box(cl, SAF, (0, yc1 - 0.04, -1.0), (3.24, 0.06, 0.25))
    for x in (-1.45, 1.45):
        for z in (-2.85, 0.85):
            box(cl, PAT, (x, hh + 0.04, z), (0.30, 0.08, 0.30))
    if lod < 2:
        for z in (-2.0, 0.0):                                             # fans on the top
            ylathe(cl, "eq_bharat_seal", [(0, yc1), (0.55, yc1), (0.55, yc1 + 0.004), (0, yc1 + 0.004)], seg, t=(-0.6, 0, z))
            ylathe(cl, PAT, crect(0.55, 0.62, yc1 - 0.002, yc1 + 0.012), seg, t=(-0.6, 0, z))
            ylathe(cl, PAT, [(0, yc1), (0.12, yc1), (0.12, yc1 + 0.012), (0, yc1 + 0.012)], small, t=(-0.6, 0, z))
        for k in range(6):                                                # louvres on the +X end
            box(cl, PAT, (1.61, yc0 + 0.06 + 0.045 * k, -1.0), (0.012, 0.025, 3.0))
    if lod == 0:
        for z in (-2.0, 0.0):
            for k in range(4):
                box(cl, PAT, (-0.6, yc1 + 0.008, z), (1.0, 0.012, 0.06), rot_y(k * math.pi / 4))
        for x in (-1.45, 1.45):
            for z in (-2.85, 0.85):
                for dx in (-0.09, 0.09):
                    hex_bolt(cl, "eq_steel", (x + dx, hh + 0.08, z + 0.1), (0, 1, 0), 0.06, 0.03)
    # power distribution cabinet under the belly, conduits to both ends
    pw = P["bunk500_power"] = Part("bunk500_power")
    box(pw, GRN, (0, -hh - 0.22, 0.6), (2.6, 0.44, 3.4), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        for x in (-0.7, 0.0, 0.7):
            cyl(pw, PAT, (x, -hh - 0.10, 2.3), (x, -hh - 0.10, 7.2), 0.07, small)
            cyl(pw, PAT, (x, -hh - 0.10, -1.1), (x, -hh - 0.10, -7.2), 0.07, small)
        box(pw, WHT, (0.6, -hh - 0.441, 0.6), (0.8, 0.004, 0.5))
    if lod == 0:
        for z in (-0.6, 1.8):
            box(pw, "eq_status_amber", (-0.8, -hh - 0.442, z), (0.12, 0.006, 0.08))
        for z in (-5.0, -3.0, 4.0, 6.0):                                   # conduit clamps
            box(pw, PAT, (0, -hh - 0.09, z), (1.7, 0.18, 0.10))
    # mounting brackets (bolted on underneath, outside the licensed hull)
    mt = P["bunk500_mounts"] = Part("bunk500_mounts")
    for x in (-3.6, 3.6):
        for z in (-6.0, 6.0):
            box(mt, SAF, (x, -hh - 0.22, z), (0.70, 0.44, 0.90), chamfer=0.04 if lod == 0 else 0.0)
            if lod < 2:
                box(mt, PAT, (x, -hh - 0.43, z), (0.90, 0.04, 1.10))
            if lod == 0:
                for dx in (-0.35, 0.35):
                    for dz in (-0.45, 0.45):
                        hex_bolt(mt, "eq_steel", (x + dx, -hh - 0.45, z + dz), (0, -1, 0), 0.07, 0.03)
    for zs, nm in ((-1, "bunk500_flange_fore"), (1, "bunk500_flange_aft")):
        fl = P[nm] = Part(nm)
        bh_flange(fl, lod, 1.55, 0.14, zs * d["zf"], zs, 1.20, d["zf"] - d["ze"] - 0.14 + 0.1)
    if close():
        hz = P["bunk500_harness"] = Part("bunk500_harness")
        # improvised feed: power cabinet -> up the -X side -> climate unit, on stand-off brackets
        p = [(-1.3, -hh - 0.10, -0.3), (-hw + 0.6, -hh + 0.05, -0.45), (-hw - 0.08, -2.6, -0.6), (-hw - 0.08, 2.4, -0.6),
             (-hw + 0.8, hh - 0.05, -0.8), (-1.6, hh + 0.20, -1.0)]
        run = bezier(p[0], p[1], p[1], p[2], 8) + bezier(p[2], p[2], p[3], p[3], 6)[1:] + bezier(p[3], p[4], p[4], p[5], 8)[1:]
        harness(hz, run, 0.03, 3, ("eq_bharat_seal", GRN, "eq_bharat_beacon"), ties="eq_steel", tie_every=3)
        for y in (-2.0, -0.4, 1.2):
            box(hz, PAT, (-hw - 0.04, y, -0.6), (0.09, 0.08, 0.20))
        for s, y, z, w, h in ((1, 2.6, -4.4, 1.1, 0.7), (-1, 2.7, 3.3, 0.9, 0.6), (1, -2.9, 5.2, 0.8, 0.5)):
            box(hz, PAT, (s * (hw + 0.006), y, z), (0.012, h, w))
            for dz in (-w / 2 + 0.06, w / 2 - 0.06):
                for dy in (-h / 2 + 0.06, h / 2 - 0.06):
                    hex_bolt(hz, "eq_steel", (s * (hw + 0.012), y + dy, z + dz), (s, 0, 0), 0.04, 0.02)
    return P


def bunk500_hits():
    d = BK
    hw, hh, r = d["hw"], d["hh"], d["r"]
    H = [hbox("bunk500_hull", (0, 0, 0), (2 * hw, 2 * (hh - r * 0.3), 2 * d["ze"])),
         hbox("bunk500_hull", (0, 0, 0), (2 * (hw - r * 0.3), 2 * hh, 2 * d["ze"]))]
    for s in (-1, 1):
        H.append(hbox("bunk500_hatches", (s * (hw + 0.04), -1.05, 0), (0.08, 2.2, 1.34)))
    H += [hbox("bunk500_climate", (0, (hh + 4.4695) / 2, -1.0), (3.2, 4.4695 - hh, 4.2)),
          hbox("bunk500_power", (0, -hh - 0.22, 0.6), (2.6, 0.44, 3.4))]
    for x in (-3.6, 3.6):
        for z in (-6.0, 6.0):
            H.append(hbox("bunk500_mounts", (x, -hh - 0.23, z), (0.90, 0.46, 1.10)))
    H += [hcyl("bunk500_flange_fore", (0, 0, -d["zf"]), (0, 0, -d["ze"] + 0.1), 1.55),
          hcyl("bunk500_flange_aft", (0, 0, d["ze"] - 0.1), (0, 0, d["zf"]), 1.55)]
    for s in (-1, 1):
        H.append(hcyl("bunk500_hatches", (0, 0, s * d["zf"]), (0, 0, s * (d["zf"] + 0.06)), 0.82))
    return H


ASSETS = {"bharat_sm100_refuge": refuge100_parts, "bharat_sp500_bunks": bunk500_parts}
HITS = {"bharat_sm100_refuge": refuge100_hits, "bharat_sp500_bunks": bunk500_hits}
MODEL_ASSETS = {"std.surv.refuge100": "bharat_sm100_refuge", "bharat.hab.bunk500": "bharat_sp500_bunks"}
