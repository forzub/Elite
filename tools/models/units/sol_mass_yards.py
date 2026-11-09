"""Sol Mass Yards / Lunar-Martian Yards (Luna-Mars; Chinese-owned serial yards, Atlantic design bureaus): AK-1 boat bay,
KO-300 coolant loop, KC-1000 cryo tank, KZ-6 holding cell, RS-50S radiator section, MS-1 workshop.

School (DESIGN_SCHOOLS.md): serial, rational, cheap to make - boxes and extruded profiles on a regular rib grid (one
pitch per unit), rounded only where the physics needs it (pipes, pressure heads). Regolith grey + slate blue. Interface
standard: a rectangular rail mount with 4 twist pins (rail shoes with T-handle pins). Close-up: ribbed cable ducts,
stencilled panel codes as relief.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_smy_regolith": ((0.55, 0.55, 0.53), (0.15, 0.15, 0.15), 30),
    "eq_smy_slate": ((0.28, 0.34, 0.42), (0.25, 0.25, 0.28), 45),
    "eq_smy_dark": ((0.20, 0.21, 0.22), (0.20, 0.20, 0.20), 40),
    "eq_smy_csic": ((0.13, 0.13, 0.14), (0.30, 0.30, 0.30), 60),
    "eq_smy_mli": ((0.66, 0.63, 0.52), (0.45, 0.45, 0.40), 70),
    "eq_smy_window": ((0.10, 0.14, 0.18), (0.80, 0.80, 0.80), 150),
})
G, SL, DK = "eq_smy_regolith", "eq_smy_slate", "eq_smy_dark"


# ---------------------------------------------------------------- local helpers
def rect_ring(part, mat, c, size, axis, pos, t, d):
    """a rectangular rib ring (4 slabs, thickness t along `axis`, depth d) round a box of `size` centred at c"""
    u, w = [a for a in range(3) if a != axis]
    for s in (-1, 1):
        cc, sz = list(c), [0, 0, 0]
        cc[axis] = pos
        cc[u] = c[u] + s * (size[u] / 2 + d / 2)
        sz[axis], sz[u], sz[w] = t, d, size[w] + 2 * d
        box(part, mat, tuple(cc), tuple(sz))
        cc, sz = list(c), [0, 0, 0]
        cc[axis] = pos
        cc[w] = c[w] + s * (size[w] / 2 + d / 2)
        sz[axis], sz[w], sz[u] = t, d, size[u]
        box(part, mat, tuple(cc), tuple(sz))


def ribbed_box(part, mat, rib, c, size, axis, pitch, lod, t=0.06, d=0.04, chamfer=0.0):
    """a box with rib rings at a regular pitch along `axis` (the yard's extruded-profile look); ribs from LOD1 only"""
    box(part, mat, c, size, chamfer=chamfer if lod == 0 else 0.0)
    if lod < 2:
        n = max(1, int(round(size[axis] / pitch)))
        for i in range(n + 1):
            pos = c[axis] - size[axis] / 2 + t / 2 + (size[axis] - t) * i / n
            rect_ring(part, rib, c, size, axis, pos, t, d)


def rail_mount(part, lod, x0, x1, z0, z1, y, along="x", h=0.10):
    """the yard interface: two rail shoes (along x or z) under the unit, 4 twist pins with T-handles at the shoe ends"""
    if along == "x":
        for z in (z0, z1):
            box(part, SL, ((x0 + x1) / 2, y + h / 2, z), (x1 - x0, h, 0.16))
        pins = [(x, z) for x in (x0 + 0.12, x1 - 0.12) for z in (z0, z1)]
    else:
        for x in (x0, x1):
            box(part, SL, (x, y + h / 2, (z0 + z1) / 2), (0.16, h, z1 - z0))
        pins = [(x, z) for x in (x0, x1) for z in (z0 + 0.12, z1 - 0.12)]
    for x, z in pins:
        if lod < 2:
            cyl(part, "eq_steel", (x, y + h, z), (x, y + h + 0.06, z), 0.05, small_for(lod))
        if lod == 0:
            box(part, "eq_steel", (x, y + h + 0.075, z), (0.16, 0.03, 0.04), rot_y(math.pi / 4))


SEG7 = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc", "7": "abc",
        "8": "abcdefg", "9": "abcdfg", "-": "g"}


def stencil(part, mat, origin, u, v, text, hgt, relief=0.008):
    """stencilled code as relief (7-segment glyphs) on a plane: origin = lower-left, u = text direction, v = up"""
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    R = [[u[0], v[0], n[0]], [u[1], v[1], n[1]], [u[2], v[2], n[2]]]
    wd, st = 0.55 * hgt, 0.13 * hgt
    segs = {"a": (wd / 2, hgt, wd, st), "g": (wd / 2, hgt / 2, wd, st), "d": (wd / 2, 0, wd, st),
            "f": (0, 0.75 * hgt, st, hgt / 2), "b": (wd, 0.75 * hgt, st, hgt / 2),
            "e": (0, 0.25 * hgt, st, hgt / 2), "c": (wd, 0.25 * hgt, st, hgt / 2)}
    for k, ch in enumerate(text):
        x0 = k * wd * 1.6
        for sgm in SEG7.get(ch, ""):
            cx, cy, sx, sy = segs[sgm]
            p = (x0 + cx, cy, relief / 2)
            q = apply(R, p)
            box(part, mat, (origin[0] + q[0], origin[1] + q[1], origin[2] + q[2]), (sx * 0.86, sy * 0.86, relief), R)


def duct(part, pts, w, lod):
    """ribbed square cable duct along an axis-aligned polyline (close-up flavour)"""
    for p, q in zip(pts, pts[1:]):
        d = [q[k] - p[k] for k in range(3)]
        ax = max(range(3), key=lambda k: abs(d[k]))
        size = [w, w, w]
        size[ax] = abs(d[ax]) + w
        c = tuple((p[k] + q[k]) / 2 for k in range(3))
        ribbed_box(part, DK, SL, c, tuple(size), ax, 0.25, lod, 0.03, 0.012)


# ---------------------------------------------------------------- KO-300 coolant loop (300 MW, pump skid)
# Envelope 2.924 x 1.462 x 1.462 (centred, long axis X). coolant.loop SRUs: pump, pipes, valves, fluid (expansion tank).
def ko300_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    hw, hh, hl = 1.462, 0.731, 0.731
    yb = -hh
    P = {}
    sk = P["ko300_skid"] = Part("ko300_skid")
    rail_mount(sk, lod, -hw + 0.05, hw - 0.05, -0.55, 0.55, yb, "x")
    box(sk, G, (0, yb + 0.14, 0), (2 * hw - 0.1, 0.08, 1.30))                 # deck
    if lod < 2:
        for i in range(7):                                                     # cross ribs at 0.47 m pitch
            box(sk, SL, (-hw + 0.2 + i * (2 * hw - 0.4) / 6, yb + 0.14, 0), (0.06, 0.10, 1.36))

    pu = P["ko300_pump"] = Part("ko300_pump")                                 # canned pump: ribbed box + motor + volute
    pc = (-0.80, yb + 0.58, 0.05)
    ribbed_box(pu, G, SL, pc, (0.70, 0.70, 0.80), 0, 0.23, lod, chamfer=0.04)
    cyl(pu, SL, (pc[0] - 0.35, pc[1], pc[2]), (-hw + 0.04, pc[1], pc[2]), 0.26, small if lod else seg // 2, chamfer=0.03)
    cyl(pu, G, (pc[0] + 0.35, pc[1], pc[2]), (pc[0] + 0.52, pc[1], pc[2]), 0.30, small if lod else seg // 2)
    if lod < 2:
        for i in range(6 if lod == 0 else 3):                                  # motor fins
            a = 2 * math.pi * i / (6 if lod == 0 else 3)
            box(pu, SL, (-1.29, pc[1] + 0.27 * math.sin(a), pc[2] + 0.27 * math.cos(a)), (0.30, 0.05, 0.05), rot_x(-a))
    if lod == 0:
        stencil(pu, DK, (pc[0] - 0.25, pc[1] - 0.15, pc[2] + 0.44), (1, 0, 0), (0, 1, 0), "300", 0.14)

    fl = P["ko300_fluid"] = Part("ko300_fluid")                               # expansion tank (rib-grid box), level gauge
    tc = (0.88, yb + 0.66, -0.05)
    ribbed_box(fl, G, SL, tc, (0.95, 0.96, 1.05), 0, 0.31, lod, chamfer=0.05)
    if lod < 2:
        rect_ring(fl, SL, tc, (0.95, 0.96, 1.05), 1, tc[1], 0.06, 0.04)
        box(fl, "eq_smy_window", (tc[0] - 0.30, tc[1], tc[2] + 0.5651), (0.06, 0.70, 0.02))
        cyl(fl, "eq_steel", (tc[0], tc[1] + 0.48, tc[2]), (tc[0], tc[1] + 0.58, tc[2]), 0.08, small)
    if lod == 0:
        stencil(fl, DK, (tc[0] - 0.10, tc[1] + 0.05, tc[2] + 0.56), (1, 0, 0), (0, 1, 0), "4", 0.22)

    va = P["ko300_valves"] = Part("ko300_valves")                             # distribution valve block + 3 actuators
    vc = (0.02, yb + 0.42, 0.0)
    box(va, G, vc, (0.44, 0.46, 1.10), chamfer=0.03 if lod == 0 else 0.0)
    for k in range(3):
        z = -0.36 + 0.36 * k
        box(va, SL, (vc[0], vc[1] + 0.33, z), (0.26, 0.20, 0.22), chamfer=0.02 if lod == 0 else 0.0)
        if lod == 0:
            lathe(va, "eq_steel", crect(0.07, 0.10, -0.012, 0.012), small, R_Y, (vc[0], vc[1] + 0.47, z))
            cyl(va, "eq_steel", (vc[0], vc[1] + 0.43, z), (vc[0], vc[1] + 0.47, z), 0.02, 6)

    pp = P["ko300_pipes"] = Part("ko300_pipes")                               # supply / return headers + drops
    yh = hh - 0.17
    ps = small if lod else seg // 2
    for k, z in enumerate((-0.40, 0.40)):
        cyl(pp, SL if k else G, (-hw + 0.06, yh, z), (hw - 0.06, yh, z), 0.11, ps, caps=False)
        for x in (-hw + 0.03, hw - 0.03):                                      # interface flanges at both ends
            cyl(pp, "eq_steel", (x - 0.03, yh, z), (x + 0.03, yh, z), 0.17, ps)
            if lod == 0:
                for i in range(8):
                    a = 2 * math.pi * (i + 0.5) / 8
                    hex_bolt(pp, "eq_steel", (x + (0.03 if x > 0 else -0.03), yh + 0.135 * math.sin(a), z + 0.135 * math.cos(a)),
                             (1 if x > 0 else -1, 0, 0), 0.035, 0.02, washer=False)
        for x, top in ((pc[0], pc[1] + 0.35), (vc[0], vc[1] + 0.43), (tc[0], tc[1] + 0.48)):
            if not (x == vc[0] and lod == 2):
                cyl(pp, SL if k else G, (x + (0.12 if k else -0.12), yh, z), (x + (0.12 if k else -0.12), top, z * 0.5), 0.07, ps)
        if lod < 2:
            for x in (-1.1, -0.3, 0.5, 1.2):                                   # pipe saddles
                box(pp, SL, (x, yh - 0.14, z), (0.08, 0.06, 0.26))
    if close():
        dz = P["ko300_duct"] = Part("ko300_duct")
        duct(dz, [(-1.30, yb + 0.25, -0.62), (1.30, yb + 0.25, -0.62), (1.30, yb + 0.55, -0.62)], 0.10, 0)
        harness(dz, [(1.30, yb + 0.60, -0.62), (1.15, yb + 0.95, -0.50), (0.95, tc[1] + 0.3, tc[2] - 0.53)], 0.015, 3,
                ("eq_insulator", SL), tie_every=1)
        stencil(dz, "eq_insulator", (-0.3, yb + 0.20, -0.672), (-1, 0, 0), (0, 1, 0), "70-300", 0.06)
    return P


def ko300_hits():
    hw, hh = 1.462, 0.731
    yb = -hh
    return [hbox("ko300_skid", (0, yb + 0.10, 0), (2 * hw, 0.20, 1.36)),
            hbox("ko300_pump", (-0.92, yb + 0.58, 0.05), (1.07, 0.72, 0.82)),
            hbox("ko300_fluid", (0.88, yb + 0.66, -0.05), (1.03, 1.04, 1.13)),
            hbox("ko300_valves", (0.02, yb + 0.55, 0.0), (0.44, 0.72, 1.10)),
            hcyl("ko300_pipes", (-hw, hh - 0.17, -0.40), (hw, hh - 0.17, -0.40), 0.17),
            hcyl("ko300_pipes", (-hw, hh - 0.17, 0.40), (hw, hh - 0.17, 0.40), 0.17)]


# ---------------------------------------------------------------- KC-1000 cryo tank (He-3 / D, 1000 t)
# Envelope 8.855 x 8.855 x 15.94 (centred, long axis Z): an MLI-wrapped pressure vessel (round: the physics wants it)
# inside an octagonal serial cage of box beams and ring frames on the yard's 1.73 m pitch; two cryocoolers on the +Z end
# deck, fill / drain manifold on the -Z end; rail shoes along the bottom flat. cryo_tank SRUs: insulation, cooler.
KC = dict(r=3.95, zc=5.2, head=1.70, rv=4.62, zl=7.97, half=4.4275)


def oct_ring(part, mat, rv, z, t, d, lod, ch=0.0):
    for k in range(8):
        a0 = math.pi / 8 + k * math.pi / 4
        a1 = a0 + math.pi / 4
        am = (a0 + a1) / 2
        L = 2 * rv * math.sin(math.pi / 8)
        box(part, mat, polz(rv * math.cos(math.pi / 8), am, z), (d, L, t), rot_z(am), chamfer=ch if lod == 0 else 0.0)


def kc1000_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = KC
    r, zc, hd, rv, zl = d["r"], d["zc"], d["head"], d["rv"], d["zl"]
    P = {}
    ins = P["kc1000_insulation"] = Part("kc1000_insulation")
    n = 8 if close() else {0: 6, 1: 4, 2: 3}[lod]
    head = [(r * math.cos(math.pi / 2 * i / n), zc + hd * math.sin(math.pi / 2 * i / n)) for i in range(n + 1)]
    prof = [(p[0], -p[1]) for p in head[::-1]] + head
    lathe(ins, "eq_smy_mli", prof, seg, soft=30)
    if lod < 2:
        for i in range(7):                                                     # MLI blanket seams on the frame pitch
            z = -zc + 2 * zc * i / 6
            lathe(ins, SL, crect(r - 0.01, r + 0.025, z - 0.05, z + 0.05), seg)
    if lod == 0:
        for i in range(16):                                                    # longitudinal blanket seams
            a = 2 * math.pi * (i + 0.5) / 16
            box(ins, "eq_smy_mli", polz(r + 0.01, a, 0), (0.03, 0.06, 2 * zc), rot_z(a))

    fr = P["kc1000_frame"] = Part("kc1000_frame")                              # octagonal cage
    bw = 0.30
    for k in range(8):
        a = math.pi / 8 + k * math.pi / 4
        box(fr, G, polz(rv - 0.06, a, 0), (bw, bw, 2 * (zc + 0.9)), rot_z(a), chamfer=0.03 if lod == 0 else 0.0)
    frames = [-zc - 0.75 + (2 * zc + 1.5) * i / 6 for i in range(7)]
    for z in frames if lod < 2 else frames[::3]:
        oct_ring(fr, SL, (rv - 0.06) * math.cos(math.pi / 8) / math.cos(math.pi / 8), z, 0.22, 0.26, lod, 0.02)
        if lod < 2 and abs(z) < zc:
            for k in range(8):                                                 # radial stand-offs to the vessel
                a = k * math.pi / 4
                box(fr, G, polz((r + rv * math.cos(math.pi / 8) - 0.2) / 2, a, z), (rv * math.cos(math.pi / 8) - r - 0.2, 0.14, 0.14), rot_z(a))
    for s in (-1, 1):                                                          # end decks: cross beams
        z = s * (zc + 0.9)
        for k in range(2):
            box(fr, G, (0, 0, z), (2 * rv * math.cos(math.pi / 8) - 0.2, 0.26, 0.26), rot_z(math.pi / 2 * k))
    if lod == 0:
        for z in frames:                                                       # frame-node gussets
            for k in range(8):
                a = math.pi / 8 + k * math.pi / 4
                box(fr, SL, polz(rv - 0.06, a, z), (0.36, 0.36, 0.30), rot_z(a))

    co = P["kc1000_cooler"] = Part("kc1000_cooler")                            # two cryocoolers on the +Z deck
    zd = zc + 0.9
    for s in (-1, 1):
        cc = (s * 1.55, 0.0, zd + 0.85)
        ribbed_box(co, G, SL, cc, (1.40, 1.60, 1.30), 2, 0.43, lod, chamfer=0.05)
        cyl(co, "eq_steel", (s * 1.55, 0, zd + 0.2), (s * 1.55, 0, zd - 0.6), 0.22, small)    # cold head into the dome
        if lod < 2:
            for i in range(9):                                                 # rejection fin array (rib grid)
                box(co, DK, (s * (1.55 + 0.72), -0.64 + 0.16 * i, cc[2]), (0.10, 0.05, 1.20))
        if lod == 0:
            stencil(co, DK, (s * 1.55 - 0.45, 0.82, cc[2] + 0.4), (1, 0, 0), (0, 0, -1), "1" if s < 0 else "2", 0.40)
    if lod < 2:
        box(co, SL, (0, -0.95, zd + 0.55), (3.2, 0.30, 0.50))                  # shared compressor plinth

    po = P["kc1000_ports"] = Part("kc1000_ports")                              # fill / drain / vent manifold (-Z end)
    zp = -(zc + 0.9)
    box(po, SL, (0, 1.2, zp - 0.55), (2.2, 0.8, 0.9), chamfer=0.04 if lod == 0 else 0.0)
    for x in (-0.65, 0.0, 0.65):
        cyl(po, "eq_steel", (x, 1.2, zp - 1.0), (x, 1.2, -zl + 0.03), 0.13, small)
        cyl(po, "eq_steel", (x, 1.2, -zl + 0.12), (x, 1.2, -zl), 0.20, small)
        if lod < 2:
            cyl(po, "eq_smy_mli", (x, 1.2, zp - 0.1), (x, 1.2 * (1 - 0.3), -zc - hd + 0.3), 0.09, small)
    if lod == 0:
        stencil(po, DK, (0.9, 0.95, zp - 1.0), (-1, 0, 0), (0, 1, 0), "1000", 0.30)

    mt = P["kc1000_mounts"] = Part("kc1000_mounts")                            # rail shoes on the bottom flat
    yb = -d["half"]
    rail_mount(mt, lod, -1.6, 1.6, -zc - 0.6, zc + 0.6, yb, "z", 0.16)
    if lod < 2:
        for z in frames:
            box(mt, G, (0, yb + 0.20, z), (3.6, 0.12, 0.26))
    if close():
        dz = P["kc1000_duct"] = Part("kc1000_duct")
        x = rv * math.cos(math.pi / 8) - 0.12
        duct(dz, [(1.2, x - 0.4, zd + 0.3), (1.2, x - 0.4, -zc), (1.2, x - 0.4, zp - 0.3)], 0.14, 0)
        for z in (zp - 0.2, zd + 0.2):
            harness(dz, [(1.2, x - 0.4, z), (1.0, 2.0, z), (0.6, 1.6, z)], 0.02, 3, ("eq_insulator", SL), tie_every=1)
    return P


def kc1000_hits():
    d = KC
    r, zc, hd, rv, zl = d["r"], d["zc"], d["head"], d["rv"], d["zl"]
    a = rv * math.cos(math.pi / 8)
    H = [hcyl("kc1000_insulation", (0, 0, -zc - hd), (0, 0, zc + hd), r + 0.03)]
    for k in range(8):
        H.append(hbox("kc1000_frame", polz(rv - 0.06, math.pi / 8 + k * math.pi / 4, 0), (0.30, 0.30, 2 * (zc + 0.9)),
                      (0, 0, math.degrees(math.pi / 8 + k * math.pi / 4))))
    for s in (-1, 1):
        H.append(hbox("kc1000_frame", (0, 0, s * (zc + 0.9)), (2 * a, 2 * a, 0.3)))
        H.append(hbox("kc1000_cooler", (s * 1.6, 0, zc + 1.75), (1.6, 1.68, 1.38)))
    H += [hbox("kc1000_ports", (0, 1.2, -(zc + 0.9) - 0.55), (2.2, 0.8, 0.9)),
          hbox("kc1000_ports", (0, 1.2, -zl + 0.6), (1.7, 0.42, 1.2)),
          hbox("kc1000_mounts", (0, -d["half"] + 0.1, 0), (3.4, 0.2, 2 * zc + 1.2))]
    return H


# ---------------------------------------------------------------- KZ-6 holding cell (detainees / rescued, 6 seats)
# Envelope 2.865 x 2.047 x 4.093 (centred, long axis Z): a chamfered box pressure shell on the yard's rib grid, a square
# seal hatch on +Z with a peep window, climate unit on the roof, power box + ribbed duct on -X, rail shoes underneath.
# passenger.cabin SRUs: seal, climate, power.
def kz6_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    w, h, L = 2.50, 1.66, 3.80
    P = {}
    ce = P["kz6_cell"] = Part("kz6_cell")
    ribbed_box(ce, G, SL, (0, 0, 0), (w, h, L), 2, 0.54, lod, 0.08, 0.045, chamfer=0.10)
    if lod < 2:
        box(ce, "eq_smy_window", (w / 2 + 0.002, 0.25, -0.8), (0.01, 0.22, 0.50))   # one slit viewport per side
        box(ce, "eq_smy_window", (-w / 2 - 0.002, 0.25, 0.8), (0.01, 0.22, 0.50))
    if lod == 0:
        stencil(ce, DK, (w / 2 + 0.001, -0.45, 0.30), (0, 0, -1), (0, 1, 0), "6", 0.36)
        stencil(ce, SL, (w / 2 + 0.001, -0.45, 1.25), (0, 0, -1), (0, 1, 0), "26-6", 0.16)
    hc = P["kz6_hatch"] = Part("kz6_hatch")                                     # square seal hatch (+Z)
    zf = L / 2
    box(hc, SL, (0, -0.05, zf + 0.04), (1.10, 1.40, 0.08), chamfer=0.03 if lod == 0 else 0.0)  # seal frame
    box(hc, G, (0, -0.05, zf + 0.10), (0.86, 1.16, 0.06), chamfer=0.02 if lod == 0 else 0.0)   # door leaf
    if lod < 2:
        box(hc, "eq_smy_window", (0, 0.30, zf + 0.132), (0.22, 0.16, 0.01))
        for s in (-1, 1):
            box(hc, "eq_steel", (s * 0.47, -0.05 + s * 0.0, zf + 0.12), (0.06, 0.24, 0.06))           # hinges / dogs
        box(hc, "eq_steel", (0.30, -0.15, zf + 0.16), (0.04, 0.30, 0.04))                           # lever
    if lod == 0:
        for k in range(6):
            for s in (-1, 1):
                hex_bolt(hc, "eq_steel", (s * 0.50, -0.65 + 0.24 * k, zf + 0.08), (0, 0, 1), 0.04, 0.02, washer=False)
    cl = P["kz6_climate"] = Part("kz6_climate")                                 # roof climate unit with louvres
    ribbed_box(cl, G, SL, (0, h / 2 + 0.07, -0.5), (1.4, 0.16, 1.6), 0, 0.46, lod, 0.05, 0.02)
    if lod < 2:
        for k in range(5):
            box(cl, DK, (0.45, h / 2 + 0.155, -1.0 + 0.25 * k), (0.4, 0.01, 0.06))
        cyl(cl, DK, (-0.35, h / 2 + 0.15, -0.5), (-0.35, h / 2 + 0.17, -0.5), 0.26, small)
    pw = P["kz6_power"] = Part("kz6_power")                                     # power distribution box on -X
    pc = (-w / 2 - 0.10, -0.15, -0.9)
    box(pw, SL, pc, (0.18, 0.80, 1.10), chamfer=0.02 if lod == 0 else 0.0)
    if lod < 2:
        box(pw, DK, (pc[0] - 0.095, pc[1] + 0.2, pc[2]), (0.01, 0.2, 0.7))
        ribbed_box(pw, DK, SL, (-w / 2 - 0.06, h / 2 - 0.12, 0.3), (0.10, 0.10, 2.0), 2, 0.25, lod, 0.03, 0.012)
    mt = P["kz6_mounts"] = Part("kz6_mounts")
    rail_mount(mt, lod, -0.85, 0.85, -L / 2 + 0.1, L / 2 - 0.1, -h / 2 - 0.19, "z", 0.12)
    for z in (-1.2, 0.0, 1.2) if lod < 2 else ():
        box(mt, G, (0, -h / 2 - 0.035, z), (1.9, 0.07, 0.2))
    if close():
        dz = P["kz6_duct"] = Part("kz6_duct")
        harness(dz, [(-w / 2 - 0.06, h / 2 - 0.12, 1.3), (-w / 2 - 0.06, h / 2 + 0.02, 1.5), (-0.6, h / 2 + 0.06, 1.5),
                     (-0.6, h / 2 + 0.06, 0.4)], 0.018, 3, ("eq_insulator", SL), tie_every=1)
    return P


def kz6_hits():
    w, h, L = 2.50, 1.66, 3.80
    return [hbox("kz6_cell", (0, 0, 0), (w + 0.09, h + 0.09, L)),
            hbox("kz6_hatch", (0, -0.05, L / 2 + 0.07), (1.10, 1.40, 0.14)),
            hbox("kz6_climate", (0, h / 2 + 0.085, -0.5), (1.45, 0.20, 1.6)),
            hbox("kz6_power", (-w / 2 - 0.10, -0.15, -0.9), (0.20, 0.80, 1.10)),
            hbox("kz6_mounts", (0, -h / 2 - 0.10, 0), (1.86, 0.18, L - 0.2))]


# ---------------------------------------------------------------- RS-50S radiator section (50 m2, C-SiC, deployable)
# Envelope 5 x 0.15 x 10: hull-skin unit, radiating face +Y, mounting face y = 0. A panel of 4 x 9 C-SiC tiles on a
# slate rib grid (heat-pipe ribs along Z on the 1.25 m pitch, cross ribs on 1.0 m), the root manifold duct across X at
# -Z with the deploy hinge line and drive. radiator.panel SRUs: panel, hinge, manifold.
RS = dict(W=5.0, H=0.15, L=10.0, root=0.55)


def rs50s_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    W_, H, L, rt = RS["W"], RS["H"], RS["L"], RS["root"]
    z0, z1 = -L / 2 + rt, L / 2
    P = {}
    pn = P["rs50s_panel"] = Part("rs50s_panel")
    if lod == 2:
        box(pn, "eq_smy_csic", (0, 0.05, (z0 + z1) / 2), (W_, 0.06, z1 - z0))
        box(pn, SL, (0, 0.035, (z0 + z1) / 2), (W_ + 0.0, 0.03, z1 - z0 + 0.0))
    else:
        nx, nz = 4, 9
        g = 0.05
        box(pn, SL, (0, 0.025, (z0 + z1) / 2), (W_, 0.05, z1 - z0))            # rib-grid backing sheet
        for i in range(nx):
            for j in range(nz):
                x = -W_ / 2 + (i + 0.5) * W_ / nx
                z = z0 + (j + 0.5) * (z1 - z0) / nz
                box(pn, "eq_smy_csic", (x, 0.07, z), (W_ / nx - g, 0.04, (z1 - z0) / nz - g))
        for i in range(nx + 1):                                                # heat-pipe ribs along Z
            x = -W_ / 2 + 0.04 + i * (W_ - 0.08) / nx
            box(pn, SL, (x, 0.08, (z0 + z1) / 2), (0.08, 0.06, z1 - z0))
        for j in range(nz + 1):                                                # cross ribs
            z = z0 + 0.03 + j * (z1 - z0 - 0.06) / nz
            box(pn, SL, (0, 0.075, z), (W_, 0.05, 0.06))
        if lod == 0:
            for i in range(nx):                                                # stencilled tile column codes at the tip
                stencil(pn, G, (-W_ / 2 + (i + 0.3) * W_ / nx, 0.09, z1 - 0.2), (1, 0, 0), (0, 0, -1), str(i + 1), 0.22)
    mf = P["rs50s_manifold"] = Part("rs50s_manifold")
    zm = -L / 2 + rt / 2
    box(mf, SL, (0, 0.080, zm + 0.05), (W_, 0.124, 0.30), chamfer=0.02 if lod == 0 else 0.0)
    if lod < 2:
        for i in range(5):                                                     # feed stubs into the heat-pipe ribs
            x = -W_ / 2 + 0.04 + i * (W_ - 0.08) / 4
            box(mf, SL, (x, 0.09, zm + 0.30), (0.12, 0.08, 0.25))
        for s in (-1, 1):                                                      # end caps with flex-hose ports
            cyl(mf, "eq_steel", (s * (W_ / 2 - 0.01), 0.085, zm + 0.05), (s * (W_ / 2 - 0.0), 0.085, zm + 0.05), 0.05, small)
        for i in range(10 if lod == 0 else 5):                                 # rib grid on the duct
            box(mf, G, (-W_ / 2 + 0.25 + i * (W_ - 0.5) / (9 if lod == 0 else 4), 0.146, zm + 0.05), (0.05, 0.008, 0.31))
    if lod == 0:
        stencil(mf, G, (-2.05, 0.142, zm + 0.12), (1, 0, 0), (0, 0, -1), "50", 0.12)
    hg = P["rs50s_hinge"] = Part("rs50s_hinge")                                # hinge line + drive + rail mount
    zh = -L / 2 + 0.07
    for x in (-1.9, 0.0, 1.9):
        box(hg, G, (x, 0.05, zh), (0.32, 0.10, 0.14), chamfer=0.02 if lod == 0 else 0.0)
    cyl(hg, "eq_steel", (-2.1, 0.06, zh), (2.1, 0.06, zh), 0.04, small)
    box(hg, SL, (2.20, 0.07, zh + 0.02), (0.40, 0.14, 0.18), chamfer=0.02 if lod == 0 else 0.0)   # deploy gearmotor
    if lod < 2:
        cyl(hg, DK, (2.0, 0.07, zh + 0.02), (2.44, 0.07, zh + 0.02), 0.05, small)
    for x in (-1.0, 1.0):                                                      # twist-pin rail shoes under the manifold
        box(hg, SL, (x, 0.012, zm + 0.08), (0.80, 0.024, 0.16))
        for xx in (x - 0.30, x + 0.30):
            if lod < 2:
                cyl(hg, "eq_steel", (xx, 0.024, zm + 0.17), (xx, 0.06, zm + 0.17), 0.035, small)
            if lod == 0:
                box(hg, "eq_steel", (xx, 0.07, zm + 0.17), (0.12, 0.02, 0.03), rot_y(math.pi / 4))
    if close():
        hz = P["rs50s_hoses"] = Part("rs50s_hoses")
        for s in (-1, 1):
            tube(hz, DK, bezier((s * (W_ / 2), 0.085, zm + 0.05), (s * (W_ / 2 + 0.0), 0.10, zm - 0.10),
                               (s * (W_ / 2 - 0.15), 0.06, -L / 2 + 0.02), (s * (W_ / 2 - 0.2), 0.03, -L / 2 + 0.005), 8), 0.035, 8)
        harness(hz, [(2.20, 0.05, zh + 0.11), (2.0, 0.03, zh + 0.30), (1.4, 0.03, zh + 0.30)], 0.012, 2, ("eq_insulator", SL), tie_every=1)
    return P


def rs50s_hits():
    W_, L, rt = RS["W"], RS["L"], RS["root"]
    z0, z1 = -L / 2 + rt, L / 2
    zm = -L / 2 + rt / 2
    return [hbox("rs50s_panel", (0, 0.055, (z0 + z1) / 2), (W_, 0.11, z1 - z0)),
            hbox("rs50s_manifold", (0, 0.085, zm + 0.10), (W_, 0.15, 0.42)),
            hbox("rs50s_hinge", (0, 0.06, -L / 2 + 0.07), (4.9, 0.12, 0.14)),
            hbox("rs50s_hinge", (2.22, 0.07, -L / 2 + 0.09), (0.48, 0.14, 0.18))]


# ---------------------------------------------------------------- MS-1 service-tender workshop (2 crews)
# Envelope 7 x 4 x 5 (centred): a rib-grid box compartment open to the +Z bay side (roller door rolled up), machine bay
# (lathe / mill bed + printer cabinet) on the left, workbenches and tool racks on the back wall, a bridge crane on two
# runway beams under the roof. workshop SRUs: fabricator, crane.
MS = dict(W=7.0, H=4.0, L=5.0)


def ms1_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    W_, H, L = MS["W"], MS["H"], MS["L"]
    hw, hh, hl = W_ / 2, H / 2, L / 2
    fy = -hh + 0.22                                                             # floor top
    t = 0.10
    P = {}
    sh = P["ms1_shell"] = Part("ms1_shell")
    box(sh, G, (0, -hh + 0.16, 0), (W_ - 0.1, 0.12, L - 0.1))                 # floor
    box(sh, G, (0, hh - t / 2 - 0.04, 0), (W_ - 0.1, t, L - 0.1))             # roof
    box(sh, G, (0, 0, -hl + 0.05 + t / 2), (W_ - 0.1, H - 0.2, t))            # back wall
    for s in (-1, 1):
        box(sh, G, (s * (hw - 0.05 - t / 2), 0, 0), (t, H - 0.2, L - 0.1))     # side walls
    if lod < 2:
        pitch_x, pitch_z = 7, 5
        for i in range(pitch_x + 1):                                           # rib grid, 1 m pitch
            x = -hw + 0.06 + i * (W_ - 0.12) / pitch_x
            box(sh, SL, (x, hh - 0.04 / 2, 0), (0.08, 0.04, L - 0.02))
            box(sh, SL, (x, 0, -hl + 0.03), (0.08, H - 0.1, 0.06))
            box(sh, SL, (x, -hh + 0.05, 0), (0.08, 0.10, L - 0.02))
        for j in range(pitch_z + 1):
            z = -hl + 0.06 + j * (L - 0.12) / pitch_z
            for s in (-1, 1):
                box(sh, SL, (s * (hw - 0.03), 0, z), (0.06, H - 0.1, 0.08))
            box(sh, SL, (0, hh - 0.02, z), (W_ - 0.02, 0.04, 0.08))
        for s in (-1, 1):
            box(sh, SL, (s * (hw - 0.03), s * 0.0 + 0.0, 0), (0.06, 0.08, L - 0.02))
        box(sh, SL, (0, -hh + 0.10, hl - 0.06), (W_ - 0.02, 0.20, 0.12))      # front sill
    if lod == 0:
        stencil(sh, DK, (hw - 0.049, -0.6, 1.4), (0, 0, -1), (0, 1, 0), "1", 0.9)
        stencil(sh, DK, (-hw + 0.049, -0.6, -2.0), (0, 0, 1), (0, 1, 0), "1", 0.9)
        for i in range(6):                                                     # floor tie-down rails
            box(sh, DK, (-2.5 + i, fy + 0.005, 0.6), (0.06, 0.01, 3.4))
    dr = P["ms1_door"] = Part("ms1_door")                                     # roller door (rolled up) + side guides
    box(dr, SL, (0, hh - 0.30, hl - 0.22), (W_ - 0.3, 0.44, 0.40), chamfer=0.03 if lod == 0 else 0.0)
    for s in (-1, 1):
        box(dr, SL, (s * (hw - 0.17), 0, hl - 0.10), (0.14, H - 0.3, 0.16))
    if lod == 0:
        for k in range(8):
            box(dr, DK, (-2.8 + 0.8 * k, hh - 0.30, hl - 0.015), (0.5, 0.36, 0.01))

    fb = P["ms1_fabricator"] = Part("ms1_fabricator")                         # machine bed + printer cabinet
    bx = -hw + 1.55
    box(fb, SL, (bx, fy + 0.45, -0.3), (2.4, 0.9, 0.9), chamfer=0.03 if lod == 0 else 0.0)         # lathe / mill bed
    box(fb, G, (bx - 0.95, fy + 1.15, -0.3), (0.5, 0.6, 0.8), chamfer=0.03 if lod == 0 else 0.0)   # headstock
    cyl(fb, "eq_steel", (bx - 0.7, fy + 1.15, -0.3), (bx - 0.5, fy + 1.15, -0.3), 0.20, small)    # chuck
    box(fb, G, (bx + 0.6, fy + 1.05, -0.3), (0.35, 0.3, 0.4))                                     # tool carriage
    box(fb, G, (bx + 0.95, fy + 1.4, -0.3), (0.3, 1.0, 0.3))                                     # mill column
    pc = (-hw + 0.85, fy + 1.0, -hl + 1.05)
    ribbed_box(fb, G, SL, pc, (1.30, 2.0, 1.2), 1, 0.5, lod, 0.05, 0.03, chamfer=0.04)            # printer cabinet
    if lod < 2:
        box(fb, "eq_smy_window", (pc[0] + 0.20, pc[1] + 0.2, pc[2] + 0.605), (0.70, 0.9, 0.01))
        box(fb, "eq_status_amber", (pc[0] - 0.45, pc[1] + 0.7, pc[2] + 0.605), (0.12, 0.08, 0.01))
    if lod == 0:
        stencil(fb, DK, (bx - 0.6, fy + 0.25, 0.151), (1, 0, 0), (0, 1, 0), "2", 0.4)

    cr = P["ms1_crane"] = Part("ms1_crane")                                   # bridge crane
    yr = hh - 0.42
    for s in (-1, 1):
        box(cr, SL, (0, yr, s * (hl - 0.45)), (W_ - 0.3, 0.24, 0.16))         # runway beams along X
        if lod < 2:
            for x in (-2.8, -1.4, 0.0, 1.4, 2.8):
                box(cr, SL, (x, yr + 0.20, s * (hl - 0.45)), (0.08, 0.16, 0.1))  # hangers
    bxp = 0.6
    box(cr, "eq_status_amber", (bxp, yr - 0.20, 0), (0.30, 0.30, L - 0.8))    # bridge beam (safety yellow)
    for s in (-1, 1):
        box(cr, SL, (bxp, yr - 0.02, s * (hl - 0.45)), (0.5, 0.2, 0.3))        # end trucks
    box(cr, SL, (bxp, yr - 0.48, 0.5), (0.5, 0.26, 0.5), chamfer=0.02 if lod == 0 else 0.0)   # trolley + hoist
    cyl(cr, DK, (bxp - 0.2, yr - 0.48, 0.5), (bxp + 0.2, yr - 0.48, 0.5), 0.12, small)
    cyl(cr, "eq_steel", (bxp, yr - 0.62, 0.5), (bxp, yr - 1.6, 0.5), 0.015 if lod == 0 else 0.03, 6 if lod else 8, caps=False)
    box(cr, "eq_steel", (bxp, yr - 1.7, 0.5), (0.16, 0.2, 0.06))               # hook block

    bn = P["ms1_bench"] = Part("ms1_bench")                                   # workbenches + tool racks (back wall)
    for x0 in (0.4, 2.2):
        box(bn, SL, (x0, fy + 0.85, -hl + 0.55), (1.6, 0.06, 0.8))
        box(bn, G, (x0, fy + 0.40, -hl + 0.55), (1.5, 0.8, 0.7))
        box(bn, DK, (x0, fy + 1.6, -hl + 0.2), (1.6, 1.2, 0.08))
        if lod < 2:
            for k in range(5):
                box(bn, "eq_steel", (x0 - 0.6 + 0.3 * k, fy + 1.5, -hl + 0.27), (0.04, 0.4, 0.04))
    mt = P["ms1_mounts"] = Part("ms1_mounts")
    rail_mount(mt, lod, -hw + 0.3, hw - 0.3, -1.8, 1.8, -hh, "x", 0.10)
    if close():
        dz = P["ms1_duct"] = Part("ms1_duct")
        duct(dz, [(-hw + 0.25, hh - 0.25, -hl + 0.25), (hw - 0.25, hh - 0.25, -hl + 0.25), (hw - 0.25, fy + 0.3, -hl + 0.25)], 0.14, 0)
        for x in (0.4, 2.2):
            harness(dz, [(x, hh - 0.30, -hl + 0.25), (x, fy + 1.0, -hl + 0.25), (x - 0.6, fy + 0.95, -hl + 0.5)], 0.016, 3,
                    ("eq_insulator", SL), tie_every=1)
        harness(dz, sag((-hw + 0.25, hh - 0.30, -hl + 0.32), (pc[0], pc[1] + 1.0, pc[2]), 0.1, 6), 0.02, 3, ("eq_insulator", SL))
    return P


def ms1_hits():
    W_, H, L = MS["W"], MS["H"], MS["L"]
    hw, hh, hl = W_ / 2, H / 2, L / 2
    fy = -hh + 0.22
    return [hbox("ms1_shell", (0, -hh + 0.11, 0), (W_, 0.22, L)),
            hbox("ms1_shell", (0, hh - 0.07, 0), (W_, 0.14, L)),
            hbox("ms1_shell", (0, 0, -hl + 0.08), (W_, H, 0.16)),
            hbox("ms1_shell", (hw - 0.08, 0, 0), (0.16, H, L)),
            hbox("ms1_shell", (-hw + 0.08, 0, 0), (0.16, H, L)),
            hbox("ms1_door", (0, hh - 0.30, hl - 0.22), (W_ - 0.3, 0.44, 0.40)),
            hbox("ms1_fabricator", (-hw + 1.55, fy + 0.7, -0.3), (2.4, 1.4, 0.9)),
            hbox("ms1_fabricator", (-hw + 0.85, fy + 1.0, -hl + 1.05), (1.36, 2.0, 1.26)),
            hbox("ms1_crane", (0.6, hh - 0.5, 0), (0.5, 0.5, L - 0.6)),
            hbox("ms1_crane", (0, hh - 0.42, hl - 0.45), (W_ - 0.3, 0.3, 0.2)),
            hbox("ms1_crane", (0, hh - 0.42, -hl + 0.45), (W_ - 0.3, 0.3, 0.2)),
            hbox("ms1_bench", (1.3, fy + 0.8, -hl + 0.5), (3.4, 1.6, 0.8)),
            hbox("ms1_mounts", (0, -hh + 0.06, 0), (W_ - 0.4, 0.12, 3.8))]


# ---------------------------------------------------------------- AK-1 boarding-boat bay (1 boat)
# Envelope 4.866 x 2.433 x 6.082 (centred): a rib-grid box hangar, launch opening on the +Y (hull) face closed by two
# sliding door leaves, cradle rails + saddles + clamp arms on the floor, charger / refuel boom on the -Z wall, rail
# shoes underneath. bay SRUs: doors, cradle, charger.
AK = dict(W=4.866, H=2.433, L=6.082)


def ak1_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    W_, H, L = AK["W"], AK["H"], AK["L"]
    hw, hh, hl = W_ / 2, H / 2, L / 2
    yb = -hh + 0.12
    t = 0.10
    P = {}
    fr = P["ak1_frame"] = Part("ak1_frame")
    box(fr, G, (0, yb + 0.06, 0), (W_ - 0.12, 0.12, L - 0.12))                # floor
    for s in (-1, 1):
        box(fr, G, (s * (hw - 0.06 - t / 2), 0.06, 0), (t, H - 0.24 - 0.0, L - 0.12))
        box(fr, G, (0, 0.06, s * (hl - 0.06 - t / 2)), (W_ - 0.12 - 2 * t, H - 0.24, t))
    if lod < 2:
        for i in range(7):                                                     # rib grid on the 0.99 m pitch
            z = -hl + 0.04 + i * (L - 0.08) / 6
            for s in (-1, 1):
                box(fr, SL, (s * (hw - 0.04), 0.0, z), (0.08, H - 0.12, 0.08))
            box(fr, SL, (0, -hh + 0.10, z), (W_ - 0.02, 0.08, 0.08))
        for i in range(5):
            x = -hw + 0.04 + i * (W_ - 0.08) / 4
            for s in (-1, 1):
                box(fr, SL, (x, 0.0, s * (hl - 0.04)), (0.08, H - 0.12, 0.08))
        for s in (-1, 1):                                                      # door track rails (top coaming)
            box(fr, SL, (s * (hw - 0.10), hh - 0.14, 0), (0.20, 0.08, L))
            box(fr, SL, (0, hh - 0.14, s * (hl - 0.10)), (W_, 0.08, 0.20))
    if lod == 0:
        stencil(fr, DK, (hw - 0.059, -0.6, 1.8), (0, 0, -1), (0, 1, 0), "1", 0.8)
        stencil(fr, DK, (-hw + 0.059, -0.6, -2.1), (0, 0, 1), (0, 1, 0), "1", 0.8)
    dr = P["ak1_doors"] = Part("ak1_doors")                                   # two sliding leaves on the +Y face
    for s in (-1, 1):
        c = (s * (W_ / 4 - 0.06), hh - 0.06, 0)
        box(dr, SL, c, (W_ / 2 - 0.14, 0.08, L - 0.30), chamfer=0.02 if lod == 0 else 0.0)
        if lod < 2:
            for i in range(6):
                box(dr, G, (c[0], hh - 0.015, -hl + 0.65 + i * (L - 1.3) / 5), (W_ / 2 - 0.30, 0.03, 0.08))
            box(dr, G, (c[0] - s * (W_ / 4 - 0.12), hh - 0.015, 0), (0.08, 0.03, L - 0.5))
    if lod == 0:
        for s in (-1, 1):
            for z in (-2.0, 0.0, 2.0):
                box(dr, "eq_steel", (s * 0.06, hh - 0.02, z), (0.10, 0.04, 0.24))      # latch dogs
        stencil(dr, DK, (0.5, hh + 0.001, 1.2), (1, 0, 0), (0, 0, -1), "1", 0.7)
    cd = P["ak1_cradle"] = Part("ak1_cradle")                                 # rails + 3 saddles + 4 clamp arms
    for s in (-1, 1):
        box(cd, "eq_steel", (s * 0.8, yb + 0.17, 0), (0.16, 0.10, L - 0.6))
    for z in (-1.8, 0.0, 1.8):
        box(cd, SL, (0, yb + 0.30, z), (2.0, 0.18, 0.30), chamfer=0.02 if lod == 0 else 0.0)
        for s in (-1, 1):
            box(cd, SL, (s * 0.75, yb + 0.55, z), (0.50, 0.36, 0.26), rot_z(s * 0.6))      # V-chocks
    for sx in (-1, 1):
        for z in (-1.2, 1.2):
            box(cd, G, (sx * 1.25, yb + 0.55, z), (0.20, 0.70, 0.20))
            box(cd, "eq_status_amber", (sx * 1.05, yb + 0.92, z), (0.50, 0.12, 0.16))
    ch = P["ak1_charger"] = Part("ak1_charger")                               # charger + refuel boom on the -Z wall
    cc = (-1.2, yb + 0.7, -hl + 0.45)
    ribbed_box(ch, G, SL, cc, (1.2, 1.2, 0.5), 1, 0.4, lod, 0.04, 0.025, chamfer=0.03)
    box(ch, SL, (0.3, yb + 1.3, -hl + 0.35), (1.6, 0.14, 0.14))                # boom
    box(ch, SL, (1.1, yb + 1.1, -hl + 0.55), (0.14, 0.5, 0.14))
    cyl(ch, DK, (1.1, yb + 0.85, -hl + 0.55), (1.1, yb + 0.7, -hl + 0.55), 0.09, small)       # coupling head
    if lod < 2:
        box(ch, "eq_status_amber", (cc[0] + 0.3, cc[1] + 0.35, cc[2] + 0.255), (0.16, 0.08, 0.01))
    mt = P["ak1_mounts"] = Part("ak1_mounts")
    rail_mount(mt, lod, -1.6, 1.6, -hl + 0.2, hl - 0.2, -hh, "z", 0.12)
    if close():
        dz = P["ak1_duct"] = Part("ak1_duct")
        duct(dz, [(hw - 0.25, hh - 0.35, -hl + 0.25), (-hw + 0.25, hh - 0.35, -hl + 0.25), (-hw + 0.25, yb + 0.5, -hl + 0.25)], 0.12, 0)
        tube(dz, DK, sag((cc[0] + 0.5, cc[1] - 0.3, cc[2] + 0.25), (1.1, yb + 0.72, -hl + 0.55), 0.25, 10), 0.04, 8)
    return P


def ak1_hits():
    W_, H, L = AK["W"], AK["H"], AK["L"]
    hw, hh, hl = W_ / 2, H / 2, L / 2
    yb = -hh + 0.12
    H_ = [hbox("ak1_frame", (0, -hh + 0.12, 0), (W_, 0.24, L))]
    for s in (-1, 1):
        H_ += [hbox("ak1_frame", (s * (hw - 0.09), 0, 0), (0.18, H, L)),
               hbox("ak1_frame", (0, 0, s * (hl - 0.09)), (W_, H, 0.18))]
    H_ += [hbox("ak1_doors", (0, hh - 0.05, 0), (W_ - 0.2, 0.10, L - 0.3)),
           hbox("ak1_cradle", (0, yb + 0.45, 0), (2.9, 0.6, L - 0.6)),
           hbox("ak1_charger", (-1.2, yb + 0.7, -hl + 0.45), (1.25, 1.25, 0.55)),
           hbox("ak1_charger", (0.4, yb + 1.1, -hl + 0.45), (1.8, 0.6, 0.3)),
           hbox("ak1_mounts", (0, -hh + 0.06, 0), (3.4, 0.12, L - 0.4))]
    return H_


ASSETS = {"smy_ak1_boat_bay": ak1_parts, "smy_ko300_coolant_loop": ko300_parts, "smy_kc1000_cryo_tank": kc1000_parts,
          "smy_kz6_passenger_module": kz6_parts, "smy_rs50s_radiator": rs50s_parts, "smy_ms1_workshop": ms1_parts}
HITS = {"smy_ak1_boat_bay": ak1_hits, "smy_ko300_coolant_loop": ko300_hits, "smy_kc1000_cryo_tank": kc1000_hits,
        "smy_kz6_passenger_module": kz6_hits, "smy_rs50s_radiator": rs50s_hits, "smy_ms1_workshop": ms1_hits}
MODEL_ASSETS = {"std.bay.boat": "smy_ak1_boat_bay", "sol.cool.loop300": "smy_ko300_coolant_loop",
                "sol.cryo.t1000": "smy_kc1000_cryo_tank", "std.cabin.hold6": "smy_kz6_passenger_module",
                "sol.rad.panel50": "smy_rs50s_radiator", "sol.workshop.w1": "smy_ms1_workshop"}
