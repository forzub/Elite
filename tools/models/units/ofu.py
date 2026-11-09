"""OFU «Ural-Kourou» (Sol; Russian-Guianan launcher heritage, freight union, lifters): U-33 / U-65M / U-70 / U-73L field
drives, TZ-12 heat shield, PO-4 landing leg, O-4 life support, P20 passenger module.

School (DESIGN_SCHOOLS.md): rugged, launcher-like, robust over pretty - stringered cylinders, open truss interstages,
spherical / toroidal tanks, interstage rings, Kourou-orange bands on white. Interface standard: a truss ring with 4 hard
points (interstage style; pyrobolt shear pins at 45 deg + k * 90 deg). Close-up: exposed pyrobolts, bundled pipes on
stand-off brackets.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_ofu_white": ((0.78, 0.79, 0.76), (0.25, 0.25, 0.25), 40),
    "eq_ofu_orange": ((0.80, 0.38, 0.12), (0.20, 0.20, 0.20), 30),
    "eq_ofu_truss": ((0.42, 0.43, 0.44), (0.35, 0.35, 0.35), 70),
    "eq_ofu_ablator": ((0.20, 0.15, 0.11), (0.04, 0.04, 0.04), 8),
    "eq_ofu_tile": ((0.09, 0.09, 0.10), (0.06, 0.06, 0.06), 10),
    "eq_ofu_window": ((0.10, 0.14, 0.18), (0.80, 0.80, 0.80), 150),
})
W, OR, TR = "eq_ofu_white", "eq_ofu_orange", "eq_ofu_truss"


# ---------------------------------------------------------------- local helpers
def sphere(part, mat, c, r, seg, R=I3):
    n = max(3, seg // 2)
    lathe(part, mat, [(r * math.sin(math.pi * i / n), -r * math.cos(math.pi * i / n)) for i in range(n + 1)], seg, R, c, soft=40)


def torus(part, mat, c, rc, rt, seg, n):
    lathe(part, mat, [(rc + rt * math.cos(2 * math.pi * i / n), rt * math.sin(2 * math.pi * i / n)) for i in range(n + 1)], seg, I3, c, soft=50)


def capsule(part, mat, p0, p1, r, seg, n=3):
    d = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
    L = math.sqrt(sum(x * x for x in d))
    prof = [(r * math.sin(math.pi / 2 * i / n), r - r * math.cos(math.pi / 2 * i / n)) for i in range(n + 1)]
    prof += [(r * math.cos(math.pi / 2 * i / n), L - r + r * math.sin(math.pi / 2 * i / n)) for i in range(n + 1)]
    lathe(part, mat, prof, seg, axis_to(norm(d)), p0, soft=40)


def strut(part, mat, a, b, r, lod):
    cyl(part, mat, a, b, r, 6 if close() else {0: 6, 1: 4, 2: 3}[lod], caps=False)


def pyrobolt(part, p, d):
    """exposed pyrobolt: steel body + orange initiator cap (close-up only)"""
    if not close():
        return
    q = tuple(p[k] + d[k] * 0.06 for k in range(3))
    cyl(part, "eq_steel", p, q, 0.035, 8)
    cyl(part, OR, q, tuple(q[k] + d[k] * 0.035 for k in range(3)), 0.025, 8)


def standoff_pipes(part, pts, n, r, side, spacing=None, bracket_to=None):
    """a bundle of n parallel pipes along pts, offset along `side`, with stand-off brackets at the inner vertices"""
    spacing = spacing or r * 2.6
    for i in range(n):
        o = (i - (n - 1) / 2) * spacing
        pipe_run(part, "eq_steel" if i % 2 else W, [tuple(p[k] + side[k] * o for k in range(3)) for p in pts], r, TR, 0.9, 8)
    if bracket_to:
        for p in pts[1:-1]:
            q = bracket_to(p)
            cyl(part, TR, p, q, 0.025, 6)
            box(part, TR, p, (0.06, 0.06, 0.06))


def ring_bands(part, mat, r, zs, w, seg, t=0.035):
    for z in zs:
        lathe(part, mat, crect(r - 0.01, r + t, z - w / 2, z + w / 2), seg)


# ---------------------------------------------------------------- field drives (common builder, four layouts)
# Every drive: reactor flange = an OFU truss ring with 4 hard points at z = 0 (mating face), an open strut interstage to
# the stringered housing, field coil ring(s) and the emitter pad (engine_hot_emitter_annulus, diameter = passport
# nozzle_or_emitter_diameter_m) at the +Z end. Serviceable units (drive.field): field_ctl, coil, pad, hx, busbar, sensor.
DRIVES = {
    # U-33: the small workhorse - short 8-node V-truss, plain barrel, one coil row, plate exchanger + 2 spherical tanks
    "u33": dict(D=3.77, L=4.9, fr=1.45, br=1.35, zb=1.0, z1=3.95, skirt=None, rows=[(3.95, 4.58)], ncoil=8, rout=1.74,
                truss=8, xbrace=False, hx="spheres", nstr=16, bands=[1.55, 3.6], frames=[2.25, 2.95], gimbal=False),
    # U-65M: the service drive with a 60 deg gimbal - cardan ring on a 4-leg thrust frame, two orange gimbal jacks
    "u65m": dict(D=4.32, L=5.61, fr=1.70, br=1.30, zb=1.55, z1=4.55, skirt=None, rows=[(4.55, 5.25)], ncoil=10, rout=1.95,
                 truss=4, xbrace=False, hx="capsules", nstr=12, bands=[2.2, 4.2], frames=[3.0, 3.6], gimbal=True),
    # U-70: the efficient cruise drive - slender barrel, launcher fairing skirt, two coil rows, toroidal exchanger tank
    "u70": dict(D=4.61, L=6.0, fr=1.75, br=1.40, zb=1.0, z1=3.95, skirt=(1.95, 4.75), rows=[(4.75, 5.17), (5.22, 5.64)],
                ncoil=12, rout=2.16, truss=12, xbrace=False, hx="torus", nstr=24, bands=[1.4, 2.6, 4.3], frames=[2.0, 3.2],
                gimbal=False),
    # U-73L: the forced lifter - fat barrel, X-braced heavy truss, 4 strap-on exchanger pods (booster-like), thick coils
    "u73l": dict(D=4.88, L=6.34, fr=1.95, br=1.62, zb=0.95, z1=4.9, skirt=None, rows=[(4.9, 5.9)], ncoil=16, rout=2.30,
                 truss=12, xbrace=True, hx="pods", nstr=20, bands=[1.6, 4.4], frames=[2.4, 3.3], gimbal=False),
}


def ofu_pad(p, lod, z0, z1, R, seg, inner_n=6, outer_n=12):
    """emitter pad: two rows of replaceable ceramic sectors in an orange clamp rim, steel centre plug"""
    lathe(p, TR, [(0, z0), (R - 0.10, z0), (R - 0.10, z0 + 0.05), (0, z0 + 0.05)], seg)
    if lod == 2:
        lathe(p, "eq_pad_ceramic", [(0, z0 + 0.05), (R - 0.12, z0 + 0.05), (R - 0.12, z1 - 0.02), (0, z1 - 0.02)], seg)
        lathe(p, OR, [(R - 0.12, z0), (R, z0), (R, z1), (R - 0.12, z1)], seg)
        return
    rm = 0.36 + (R - 0.48) * 0.45
    for rows, (ra, rb, n) in enumerate(((0.38, rm - 0.02, inner_n), (rm + 0.02, R - 0.13, outer_n))):
        g = 0.02 / max(ra, 0.5)
        for i in range(n):
            a0 = 2 * math.pi * (i + 0.5 * rows) / n + g
            a1 = 2 * math.pi * (i + 1 + 0.5 * rows) / n - g
            lathe(p, "eq_pad_ceramic", crect(ra, rb, z0 + 0.05, z1 - 0.03), max(2, seg // n), I3, (0, 0, 0), a0, a1, cap=True)
    lathe(p, TR, crect(rm - 0.02, rm + 0.02, z0 + 0.05, z1 - 0.025), seg)                              # row separator ring
    lathe(p, OR, crect(R - 0.13, R, z0, z1, 0.02 if lod == 0 else 0.0), seg)                            # clamp rim
    lathe(p, "eq_steel", [(0, z0 + 0.05), (0.38, z0 + 0.05), (0.38, z1), (0.30, z1 + 0.04), (0, z1 + 0.04)], seg)
    if lod == 0:
        for i in range(outer_n * 2):                                                                    # rim clamp bolts
            a = 2 * math.pi * (i + 0.25) / (outer_n * 2)
            hex_bolt(p, "eq_steel", polz(R - 0.065, a, z1), (0, 0, 1), 0.06, 0.03, washer=False)
        for i in range(inner_n):
            hex_bolt(p, "eq_steel", polz(0.30, 2 * math.pi * i / inner_n, z1 + 0.04), (0, 0, 1), 0.06, 0.03)
    if close():
        for i in range(outer_n):                                                                        # sector retainer pins
            a = 2 * math.pi * (i + 0.5) / outer_n
            for rr in (rm + 0.12, R - 0.22):
                cyl(p, "eq_steel", polz(rr, a, z1 - 0.03), polz(rr, a, z1 + 0.0), 0.02, 6)


def drive_parts_fn(key):
    S = DRIVES[key]

    def parts(lod):
        seg, small = seg_for(lod), small_for(lod)
        p, D, L, fr, br, zb, z1 = key, S["D"], S["L"], S["fr"], S["br"], S["zb"], S["z1"]
        R = D / 2
        P = {}
        hp_angles = [math.pi / 4 + k * math.pi / 2 for k in range(4)]

        # --- reactor flange: truss ring with 4 hard points (pyrobolt shear pins), 3-spoke spider for the busbar
        f = P[p + "_flange"] = Part(p + "_flange")
        lathe(f, TR, crect(fr - 0.22, fr, 0.0, 0.18, 0.02 if lod == 0 else 0.0), seg)
        if lod < 2:
            lathe(f, OR, crect(fr - 0.01, fr + 0.02, 0.04, 0.14), seg)
        for a in hp_angles:
            Rz = rot_z(a)
            box(f, TR, polz(fr + 0.10, a, 0.16), (0.44, 0.40, 0.32), Rz, chamfer=0.03 if lod == 0 else 0.0)
            if lod == 0:
                lathe(f, "eq_steel", crect(0.06, 0.12, 0.0, 0.02), small, I3, polz(fr + 0.14, a, 0.0))   # shear-pin socket
            for s in (-1, 1):
                pyrobolt(f, polz(fr + 0.14, a + s * 0.07, 0.32), (0, 0, 1))
        if lod < 2:
            for i in range(3):
                a = 2 * math.pi * i / 3 + math.pi / 2
                Rz = rot_z(a)
                box(f, TR, apply(Rz, ((0.36 + fr - 0.20) / 2, 0, 0))[:2] + (0.09,), (fr - 0.56, 0.12, 0.10), Rz)
        if lod == 0:
            for i in range(24):
                a = 2 * math.pi * (i + 0.5) / 24
                hex_bolt(f, "eq_steel", polz(fr - 0.11, a, 0.18), (0, 0, 1), 0.07, 0.035)

        # --- busbar: power input from the reactor bus through the flange centre, insulated trunk to the aft bulkhead
        b = P[p + "_busbar"] = Part(p + "_busbar")
        bs = seg // 2 if lod == 0 else (8 if lod == 1 else 6)
        lathe(b, "eq_insulator", [(0, 0.04), (0.36, 0.04), (0.36, 0.16), (0.24, 0.24), (0.24, zb), (0, zb)], bs)
        if lod == 0:
            lathe(b, "eq_insulator", [(0.30, 0.0), (0.36, 0.0), (0.36, 0.04)], bs)
            for i in range(3):                                                       # three busbar terminals (mating face)
                a = 2 * math.pi * i / 3 + math.pi / 2
                lathe(b, "eq_copper", [(0, 0.0), (0.08, 0.0), (0.08, 0.04), (0, 0.04)], small, I3, polz(0.17, a, 0.0))
        if lod == 0:
            for zz in (0.5 * zb, 0.8 * zb):
                lathe(b, OR, crect(0.24, 0.29, zz - 0.04, zz + 0.04), small)

        # --- interstage: open strut truss from the flange ring to the housing aft ring (U-65M: 4-leg thrust frame + gimbal)
        it = P[p + "_interstage"] = Part(p + "_interstage")
        n = S["truss"]
        rt0, rt1 = fr - 0.12, br - 0.02
        if S["gimbal"]:
            zg = zb - 0.35
            rg = R - 0.30
            for a in hp_angles:                                                      # 4 legs (A-frames) to the gimbal ring
                for s in (-1, 1):
                    strut(it, TR, polz(rt0, a + s * 0.16, 0.18), polz(rg, a + s * 0.42, zg), 0.07, lod)
            lathe(it, W, crect(rg - 0.08, rg + 0.10, zg - 0.14, zg + 0.14, 0.02 if lod == 0 else 0.0), seg)   # outer gimbal ring
            if lod < 2:
                lathe(it, OR, crect(rg + 0.10, rg + 0.12, zg - 0.08, zg + 0.08), seg)
            for a in (0.0, math.pi):                                                 # outer pivots (pitch), +/-X
                box(it, TR, polz(rg + 0.08, a, zg), (0.30, 0.42, 0.40), rot_z(a), chamfer=0.03 if lod == 0 else 0.0)
            for a in (math.pi / 2, -math.pi / 2):                                    # inner pivots (yaw) to the body ring
                cyl(it, "eq_steel", polz(br - 0.05, a, zb + 0.15), polz(rg - 0.05, a, zb + 0.15), 0.13, small)
                box(it, TR, polz(rg - 0.04, a, zg + 0.10), (0.26, 0.30, 0.50), rot_z(a))
            lathe(it, TR, crect(br - 0.10, br + 0.08, zb, zb + 0.30), seg)          # body gimbal collar
        else:
            for i in range(n):
                a0 = 2 * math.pi * i / n
                a1 = 2 * math.pi * (i + 0.5) / n
                a2 = 2 * math.pi * (i - 0.5) / n
                strut(it, TR, polz(rt0, a0, 0.18), polz(rt1, a1, zb), 0.06, lod)
                strut(it, TR, polz(rt0, a0, 0.18), polz(rt1, a2, zb), 0.06, lod)
                if S["xbrace"]:
                    strut(it, TR, polz(rt0, a0, 0.18), polz(rt1, a0, zb), 0.075, lod)
            lathe(it, W, crect(br - 0.16, br + 0.05, zb - 0.14, zb, 0.02 if lod == 0 else 0.0), seg)
            if lod < 2:
                lathe(it, OR, crect(br + 0.05, br + 0.07, zb - 0.11, zb - 0.03), seg)
            if lod == 0:
                for i in range(n):                                                   # strut end fittings
                    a = 2 * math.pi * (i + 0.5) / n
                    box(it, TR, polz(rt1 + 0.02, a, zb - 0.18), (0.10, 0.18, 0.10), rot_z(a))
                    box(it, TR, polz(rt0 + 0.02, 2 * math.pi * i / n, 0.22), (0.10, 0.20, 0.10), rot_z(2 * math.pi * i / n))

        # --- housing: stringered barrel (+ fairing skirt on the U-70), ring frames, orange bands
        h = P[p + "_housing"] = Part(p + "_housing")
        zc0 = S["rows"][0][0]
        prof = [(0, zb), (br - 0.05, zb), (br, zb + 0.05), (br, z1)]
        if S["skirt"]:
            prof.append(S["skirt"])
        lathe(h, W, prof, seg)
        rend = S["skirt"][0] if S["skirt"] else br
        if lod == 0:
            nst = S["nstr"]
            for i in range(nst):
                a = 2 * math.pi * (i + 0.5) / nst
                box(h, W, polz(br + 0.025, a, (zb + z1) / 2 + 0.05), (0.05, 0.08, z1 - zb - 0.15), rot_z(a))
                if S["skirt"]:
                    cyl(h, W, polz(br + 0.02, a, z1), polz(rend + 0.02, a, S["skirt"][1]), 0.035, 4 if lod else 6)
            for fz in S["frames"]:
                lathe(h, TR, crect(br, br + 0.06, fz - 0.05, fz + 0.05), seg)
        ring_bands(h, OR, br, S["bands"] if lod < 2 else S["bands"][::2], 0.30, seg, 0.065)
        if S["skirt"]:
            lathe(h, OR, crect(rend - 0.01, rend + 0.06, S["skirt"][1] - 0.14, S["skirt"][1]), seg)

        # --- field coil ring(s): carrier + coil segments held by orange retaining straps
        c = P[p + "_coil_ring"] = Part(p + "_coil_ring")
        rout, nc = S["rout"], S["ncoil"]
        cin = rend - 0.30
        cmid = rout - 0.24
        for (cz0, cz1) in S["rows"]:
            lathe(c, TR, crect(cin, cmid, cz0, cz1, 0.02 if lod == 0 else 0.0), seg)
            if lod < 2:
                for i in range(nc):
                    a0 = 2 * math.pi * i / nc + 0.05
                    a1 = 2 * math.pi * (i + 1) / nc - 0.05
                    lathe(c, "eq_field_coil", crect(cmid - 0.02, rout, cz0 + 0.05, cz1 - 0.05, 0.02 if lod == 0 else 0.0),
                          max(2, seg // nc), I3, (0, 0, 0), a0, a1, cap=True)
                    am = (a0 + a1) / 2
                    if lod == 0:
                        lathe(c, OR, crect(rout - 0.01, rout + 0.025, (cz0 + cz1) / 2 - 0.06, (cz0 + cz1) / 2 + 0.06),
                              2, I3, (0, 0, 0), a0 + 0.03, a1 - 0.03, cap=True)
                        for s in (-1, 1):                                            # strap buckles
                            box(c, "eq_steel", polz(rout + 0.03, am + s * (a1 - a0) * 0.3, (cz0 + cz1) / 2), (0.05, 0.10, 0.14),
                                rot_z(am))
                        cyl(c, "eq_insulator", polz(cmid + 0.06, am, cz0 + 0.05), polz(cmid + 0.06, am, cz0 - 0.06), 0.04, 8)
            else:
                lathe(c, "eq_field_coil", crect(cmid - 0.02, rout, cz0 + 0.05, cz1 - 0.05), seg)

        # --- emitter pad (runtime thermal emitter mesh)
        pz0 = S["rows"][-1][1]
        ofu_pad(P.setdefault("engine_hot_emitter_annulus", Part("engine_hot_emitter_annulus")), lod, pz0, L - 0.07, R, seg,
                6 if key != "u73l" else 8, nc if nc >= 12 else 12)

        # --- field control unit: ribbed box on the +X side, on stand-off brackets
        fc = P[p + "_field_ctl"] = Part(p + "_field_ctl")
        dpt = min(R - br - 0.10, 0.48)
        zc = (zb + z1) / 2 - (0.4 if S["hx"] == "torus" else 0.0)
        fl = min(1.4, (z1 - zb) * 0.45)
        fx = br + 0.06 + dpt / 2
        box(fc, W, (fx, 0, zc), (dpt, 0.95, fl), chamfer=0.03 if lod == 0 else 0.0)
        if lod < 2:
            for k in range(3):
                box(fc, TR, (fx, 0, zc - fl / 2 + fl * (k + 0.5) / 3), (dpt + 0.04, 1.0, 0.06))
            box(fc, OR, (fx + dpt / 2 + 0.005, 0.30, zc), (0.01, 0.20, fl * 0.7))
            for s in (-1, 1):
                box(fc, TR, (br + 0.03, s * 0.35, zc), (0.10, 0.10, fl * 0.8))
        if lod == 0:
            for k in range(4):
                box(fc, "eq_status_amber", (fx + dpt / 2 + 0.01, -0.25, zc - 0.3 + 0.12 * k), (0.01, 0.08, 0.06))
            for s in (-1, 1):
                cyl(fc, "eq_steel", (fx + dpt / 2, s * 0.2, zc + fl / 2 - 0.15), (fx + dpt / 2 + 0.10, s * 0.2, zc + fl / 2 - 0.15), 0.06, 8)
        if S["gimbal"]:                                                              # the two gimbal jacks (thrust vector)
            for a in (math.pi / 4, -3 * math.pi / 4):
                a0p, a1p = polz(fr - 0.1, a, 0.20), polz(br + 0.06, a, zb + 1.3)
                mid = tuple(a0p[k] + (a1p[k] - a0p[k]) * 0.55 for k in range(3))
                cyl(fc, OR, a0p, mid, 0.11, small)
                cyl(fc, "eq_steel", mid, a1p, 0.065, small)
                box(fc, TR, a1p, (0.22, 0.22, 0.22), rot_z(a))

        # --- backup heat exchanger (four layouts)
        hx = P[p + "_hx"] = Part(p + "_hx")
        kind = S["hx"]
        if kind == "spheres":                                                        # plate exchanger + 2 spherical tanks, -X
            ex = -(br + 0.06 + dpt / 2)
            box(hx, W, (ex, 0, zc + 0.35), (dpt, 0.80, 0.9), chamfer=0.03 if lod == 0 else 0.0)
            if lod < 2:
                for k in range(7):
                    box(hx, TR, (ex - dpt / 2 - 0.02, -0.33 + 0.11 * k, zc + 0.35), (0.04, 0.03, 0.8))
            rs = dpt / 2
            for s in (-1, 1):
                sphere(hx, W, (-(br + 0.04 + rs), s * 0.62, zc - 0.55), rs, small)
                if lod < 2:
                    lathe(hx, OR, crect(rs - 0.01, rs + 0.02, -0.04, 0.04), small, I3, (-(br + 0.04 + rs), s * 0.62, zc - 0.55))
                    cyl(hx, TR, (-(br + 0.02), s * 0.62, zc - 0.55), (-(br + 0.08), s * 0.62, zc - 0.55), 0.08, 6)
        elif kind == "capsules":                                                     # two capsule tanks along Z on +/-Y
            rr = min(0.36, R - br - 0.10)
            for s in (-1, 1):
                capsule(hx, W, (0, s * (br + rr + 0.04), zb + 0.6), (0, s * (br + rr + 0.04), z1 - 0.4), rr, small, 3 if lod < 2 else 2)
                if lod < 2:
                    for zz in (zb + 1.0, z1 - 0.9):
                        lathe(hx, OR, crect(rr - 0.01, rr + 0.025, zz - 0.06, zz + 0.06), small, I3, (0, s * (br + rr + 0.04), 0))
                        box(hx, TR, (0, s * (br + 0.03), zz), (0.16, 0.10, 0.12))
        elif kind == "torus":                                                        # stage-2 style toroidal tank round the barrel
            zt = z1 - 0.55
            torus(hx, W, (0, 0, zt), br + 0.30, 0.24, seg, 12 if lod == 0 else (8 if lod == 1 else 6))
            if lod < 2:
                lathe(hx, OR, crect(br + 0.53, br + 0.555, zt - 0.06, zt + 0.06), seg)
                for i in range(8):
                    a = 2 * math.pi * (i + 0.5) / 8
                    box(hx, TR, polz(br + 0.07, a, zt), (0.14, 0.10, 0.10), rot_z(a))
        elif kind == "pods":                                                         # 4 strap-on exchanger pods (booster-like)
            rp, rpc = 0.36, R - 0.40
            for a in hp_angles:
                c0, c1 = polz(rpc, a, zb + 0.95), polz(rpc, a, z1 - 0.15)
                lathe(hx, W, [(0, -0.75), (0.10, -0.66), (0.24, -0.42), (rp, -0.12), (rp, 0)], small if lod else seg // 2,
                      I3, c0)                                                         # nose cone toward the flange
                lathe(hx, OR, [(rp, 0), (rp, 0.5)], small if lod else seg // 2, I3, c0)
                cyl(hx, W, polz(rpc, a, zb + 1.45), c1, rp, small if lod else seg // 2, caps=False)
                lathe(hx, TR, [(rp, 0), (0.22, 0.12), (0, 0.12)], small if lod else seg // 2, I3, c1)
                if lod < 2:
                    for zz in (zb + 1.6, z1 - 0.6):                                   # strap-on attach struts
                        strut(hx, TR, polz(rpc - rp + 0.02, a, zz), polz(br + 0.03, a, zz), 0.07, lod)
                        strut(hx, TR, polz(rpc - rp + 0.04, a, zz), polz(br + 0.03, a + 0.25, zz + 0.25), 0.05, lod)
                    for k in range(5 if lod == 0 else 0):
                        lathe(hx, TR, crect(rp, rp + 0.03, -0.03, 0.03), small, I3, polz(rpc, a, zb + 1.9 + k * 0.45))

        # --- sensors: field / temperature / thrust heads on the coil ring and the housing (-Y side)
        sn = P[p + "_sensors"] = Part(p + "_sensors")
        if lod == 0:
            for a, zz, r0 in ((-math.pi / 2 + 0.35, (zb + z1) / 2, br + 0.06), (-math.pi / 2 - 0.35, (zb + z1) / 2 + 0.5, br + 0.06),
                              (math.pi * 0.6, S["rows"][0][0] - 0.12, rend), (-math.pi * 0.4, S["rows"][0][0] - 0.12, rend)):
                cyl(sn, TR, polz(r0 - 0.02, a, zz), polz(r0 + 0.14, a, zz), 0.07, small)
                cyl(sn, OR, polz(r0 + 0.14, a, zz), polz(r0 + 0.18, a, zz), 0.05, small)
        else:
            box(sn, TR, polz(br + 0.05, -math.pi / 2, (zb + z1) / 2), (0.14, 0.2, 0.8), rot_z(-math.pi / 2))

        # --- close inspection: bundled coolant pipes on stand-off brackets hx -> coil ring, ctl harness, pyro lines
        if close():
            hz = P[p + "_pipes"] = Part(p + "_pipes")
            za = S["rows"][0][0] - 0.05
            for a in (math.pi + 0.45, math.pi - 0.45, 0.55):
                ro = br + 0.16
                pts = [polz(ro, a, zb + 0.4), polz(ro, a, (zb + z1) / 2), polz(ro, a, z1 - 0.2),
                       polz((rend + ro) / 2 + 0.1, a, za - 0.25), polz(cmid - 0.05, a, za)]
                standoff_pipes(hz, pts, 3, 0.035, polz(1, a + math.pi / 2, 0)[:2] + (0.0,),
                               bracket_to=lambda q: (q[0] * (br + 0.01) / math.hypot(q[0], q[1]), q[1] * (br + 0.01) / math.hypot(q[0], q[1]), q[2]))
            harness(hz, [(br + 0.06 + dpt, 0.35, zc + fl / 2 - 0.15), (br + 0.15, 0.6, zc + fl / 2 + 0.2)]
                    + cyl_path(br + 0.08, math.atan2(0.6, br), 1.2, zc + fl / 2 + 0.4, za - 0.08, 10)[1:], 0.02, 3,
                    ("eq_insulator", OR), tie_every=3)
            for a in hp_angles:                                                      # pyro firing lines to the hard points
                harness(hz, sag(polz(fr - 0.05, a + 0.1, 0.30), polz(rt1, a + 0.3, zb - 0.2), 0.1, 8, (0, 0, -1)), 0.012, 1, (OR,),
                        tie_every=4)
        return P
    return parts


def drive_hits_fn(key):
    S = DRIVES[key]

    def hits():
        p, D, L, fr, br, zb, z1 = key, S["D"], S["L"], S["fr"], S["br"], S["zb"], S["z1"]
        R = D / 2
        rend = S["skirt"][0] if S["skirt"] else br
        H = [hcyl(p + "_flange", (0, 0, 0), (0, 0, 0.18), fr),
             hcyl(p + "_busbar", (0, 0, 0), (0, 0, zb), 0.36)]
        for a in (math.pi / 4 + k * math.pi / 2 for k in range(4)):
            H.append(hbox(p + "_flange", polz(fr + 0.10, a, 0.16), (0.44, 0.40, 0.32), (0, 0, math.degrees(a))))
        if S["gimbal"]:
            rg = R - 0.30
            H += [hcyl(p + "_interstage", (0, 0, zb - 0.49), (0, 0, zb - 0.21), rg + 0.12),
                  hcyl(p + "_interstage", (0, 0, 0.18), (0, 0, zb - 0.49), (fr + rg) / 2)]
        else:
            H.append(hcyl(p + "_interstage", (0, 0, 0.18), (0, 0, zb), max(fr, br) - 0.05))
        H.append(hcyl(p + "_housing", (0, 0, zb), (0, 0, z1), br + 0.06))
        if S["skirt"]:
            H.append(hcyl(p + "_housing", (0, 0, z1), (0, 0, S["skirt"][1]), (br + rend) / 2 + 0.05))
        for cz0, cz1 in S["rows"]:
            H.append(hcyl(p + "_coil_ring", (0, 0, cz0), (0, 0, cz1), S["rout"] + 0.03))
        H.append(hcyl("engine_hot_emitter_annulus", (0, 0, S["rows"][-1][1]), (0, 0, L + 0.04), R))
        dpt = min(R - br - 0.10, 0.48)
        zc = (zb + z1) / 2 - (0.4 if S["hx"] == "torus" else 0.0)
        fl = min(1.4, (z1 - zb) * 0.45)
        H.append(hbox(p + "_field_ctl", (br + 0.06 + dpt / 2, 0, zc), (dpt + 0.12, 1.0, fl)))
        kind = S["hx"]
        if kind == "spheres":
            H.append(hbox(p + "_hx", (-(br + 0.06 + dpt / 2), 0, zc + 0.35), (dpt + 0.06, 0.80, 0.9)))
            for s in (-1, 1):
                H.append(hbox(p + "_hx", (-(br + 0.04 + dpt / 2), s * 0.62, zc - 0.55), (dpt, dpt, dpt)))
        elif kind == "capsules":
            rr = min(0.36, R - br - 0.10)
            for s in (-1, 1):
                H.append(hcyl(p + "_hx", (0, s * (br + rr + 0.04), zb + 0.6), (0, s * (br + rr + 0.04), z1 - 0.4), rr))
            for a in (math.pi / 4, -3 * math.pi / 4):
                c = polz((fr + br) / 2, a, (zb + 1.5) / 2)
                H.append(hbox(p + "_field_ctl", c, (0.26, 0.26, zb + 1.3), (0, 0, math.degrees(a))))
        elif kind == "torus":
            H.append(hcyl(p + "_hx", (0, 0, z1 - 0.79), (0, 0, z1 - 0.31), br + 0.54))
        elif kind == "pods":
            for k in range(4):
                a = math.pi / 4 + k * math.pi / 2
                H.append(hcyl(p + "_hx", polz(R - 0.40, a, zb + 0.2), polz(R - 0.40, a, z1 - 0.03), 0.36))
        H.append(hbox(p + "_sensors", polz(br + 0.1, -math.pi / 2, (zb + z1) / 2 + 0.25), (0.2, 0.9, 0.9), (0, 0, -90)))
        return H
    return hits


# ---------------------------------------------------------------- TZ-12 heat shield (lifter): +Y outward, mount face y = 0
# Envelope 6.694 x 0.335 x 6.694 m: a round shallow-dome shield (dia 6.694) - ablator cap in the middle, three rows of
# replaceable tiles round it, orange rim; the square envelope corners carry the 4 OFU hard-point truss lugs.
TZ = dict(R=3.347, H=0.335, rc=2.25, ri=3.22, hp=4.15)


def tz12_tile_y(r):
    """tile top height along the dome slope"""
    return 0.300 - 0.060 * (r - TZ["rc"]) / (TZ["ri"] - TZ["rc"])


def tz12_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    R, H, rc, ri = TZ["R"], TZ["H"], TZ["rc"], TZ["ri"]
    P = {}
    st = P["tz12_structure"] = Part("tz12_structure")
    ylathe(st, TR, [(0, 0), (R - 0.04, 0), (R, 0.04), (R, 0.16), (ri, 0.20), (ri, 0.14), (0, 0.14)], seg)   # back shell
    ylathe(st, OR, crect(ri, R + 0.005, 0.15, 0.25, 0.02 if lod == 0 else 0.0), seg)                    # orange clamp rim
    for k in range(4):                                                     # 4 hard-point truss lugs in the envelope corners
        a = math.pi / 4 + k * math.pi / 2
        hp = polar(TZ["hp"], a, 0.0)
        box(st, TR, (hp[0], 0.11, hp[2]), (0.46, 0.22, 0.46), rot_y(-a), chamfer=0.03 if lod == 0 else 0.0)
        for s in (-1, 1):
            q = polar(R - 0.05, a + s * 0.20, 0.10)
            cyl(st, TR, (q[0], 0.10, q[2]), (hp[0], 0.10, hp[2]), 0.07, 6 if lod == 0 else 4)
        if lod < 2:
            ylathe(st, "eq_steel", crect(0.07, 0.13, 0.22, 0.25), small, t=(hp[0], 0, hp[2]))
        pyrobolt(st, (hp[0] + 0.12, 0.22, hp[2]), (0, 1, 0))
        pyrobolt(st, (hp[0] - 0.12, 0.22, hp[2]), (0, 1, 0))
    if lod == 0:
        for i in range(48):
            hex_bolt(st, "eq_steel", polar(R - 0.065, 2 * math.pi * (i + 0.5) / 48, 0.25), (0, 1, 0), 0.06, 0.03, washer=False)

    ab = P["tz12_ablator"] = Part("tz12_ablator")
    n = 6 if lod == 0 else (4 if lod == 1 else 2)
    prof = [(rc * i / n, H - (H - 0.30) * (i / n) ** 2) for i in range(n, -1, -1)]
    ylathe(ab, "eq_ofu_ablator", [(rc, 0.14)] + prof, seg)
    if lod < 2:
        for rr in (0.8, 1.6):                                              # ablator gore seams (relief)
            y = H - (H - 0.30) * (rr / rc) ** 2
            ylathe(ab, "eq_ofu_ablator", crect(rr - 0.02, rr + 0.02, y - 0.03, y + 0.008), seg)
        for i in range(6):
            a = 2 * math.pi * i / 6
            box(ab, "eq_ofu_ablator", polar(1.2, a, H - 0.012), (1.6, 0.03, 0.04), rot_y(-a))
    if close():
        for i in range(12):                                                # thermocouple plugs
            a = 2 * math.pi * i / 12
            for rr in (0.6, 1.4, 2.0):
                y = H - (H - 0.30) * (rr / rc) ** 2
                ylathe(ab, "eq_steel", [(0.03, y - 0.01), (0.03, y + 0.008), (0, y + 0.008)], 6, t=polar(rr, a + rr, 0))

    tl = P["tz12_tiles"] = Part("tz12_tiles")
    rows = [(rc + 0.02, rc + 0.33, 28), (rc + 0.35, rc + 0.64, 32), (rc + 0.66, ri - 0.01, 36)]
    if lod == 2:
        ylathe(tl, "eq_ofu_tile", [(rc, 0.14), (ri, 0.14), (ri, tz12_tile_y(ri)), (rc, tz12_tile_y(rc)), (rc, 0.14)], seg)
    else:
        g = 0.010 if lod == 0 else 0.0
        for k, (r0, r1, cnt) in enumerate(rows):
            for i in range(cnt):
                a0 = 2 * math.pi * (i + 0.5 * k) / cnt + g / r0
                a1 = 2 * math.pi * (i + 1 + 0.5 * k) / cnt - g / r0
                ylathe(tl, "eq_ofu_tile", [(r0, 0.14), (r1, 0.14), (r1, tz12_tile_y(r1)), (r0, tz12_tile_y(r0)), (r0, 0.14)],
                       1 if lod else 2, a0, a1, cap=True)
        if lod == 0:
            ylathe(tl, TR, [(rc - 0.005, 0.14), (rc + 0.005, 0.14), (rc + 0.005, 0.295), (rc - 0.005, 0.295), (rc - 0.005, 0.14)], seg)
    return P


def tz12_hits():
    R, H, rc, ri = TZ["R"], TZ["H"], TZ["rc"], TZ["ri"]
    Ht = [hcyl("tz12_structure", (0, 0, 0), (0, 0.14, 0), R),
          hcyl("tz12_ablator", (0, 0.14, 0), (0, H, 0), rc),
          hcyl("tz12_tiles", (0, 0.14, 0), (0, 0.29, 0), ri - 0.02)]
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        hp = polar(TZ["hp"], a, 0.0)
        Ht.append(hbox("tz12_structure", (hp[0], 0.12, hp[2]), (0.46, 0.24, 0.46), (0, -math.degrees(a), 0)))
        Ht.append(hbox("tz12_structure", polar((TZ["hp"] + R) / 2, a, 0.10), (TZ["hp"] - R + 0.3, 0.16, 0.5), (0, -math.degrees(a), 0)))
    return Ht


# ---------------------------------------------------------------- PO-4 landing leg (one of the set of four)
# Envelope 2.047 x 2.865 x 2.047: mount plate at y = 0 (hull side), the leg hangs along -Y and is drawn DEPLOYED.
# Stowed + deployable split: po4_mount and po4_actuator cylinder stay on the hull; po4_leg (strut + V-frame), po4_damper
# and po4_foot swing about the hinge axis X through (y = -0.42, z = 0.0) (stowing folds them toward +Z, launcher-leg style).
PO = dict(hinge_y=-0.42, foot_r=0.98, foot_y=-2.865)


def po4_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    hy, fr, fy = PO["hinge_y"], PO["foot_r"], PO["foot_y"]
    P = {}
    m = P["po4_mount"] = Part("po4_mount")
    oct_ = [(0.98 * math.cos(math.pi / 8 + k * math.pi / 4), 0.98 * math.sin(math.pi / 8 + k * math.pi / 4)) for k in range(8)]
    prism(m, TR, oct_, -0.16, 0.0)                                        # octagonal mount plate (truss ring standard)
    if lod < 2:
        prism(m, W, inset(oct_, 0.16), -0.20, -0.16)
    for k in range(4):                                                    # 4 hard points
        a = math.pi / 4 + k * math.pi / 2
        q = polar(0.80, a, 0.0)
        box(m, TR, (q[0], -0.10, q[2]), (0.26, 0.20, 0.26), rot_y(-a), chamfer=0.02 if lod == 0 else 0.0)
        pyrobolt(m, (q[0], -0.20, q[2]), (0, -1, 0))
    for s in (-1, 1):                                                     # hinge clevises
        box(m, TR, (s * 0.62, (hy - 0.16) / 2, 0.0), (0.16, -hy + 0.04, 0.42), chamfer=0.02 if lod == 0 else 0.0)
    box(m, TR, (0, -0.30, -0.70), (0.40, 0.24, 0.30), chamfer=0.02 if lod == 0 else 0.0)        # jack anchor
    if lod < 2:
        cyl(m, "eq_steel", (-0.74, hy, 0), (0.74, hy, 0), 0.07, small)                         # hinge pin

    a_ = P["po4_actuator"] = Part("po4_actuator")                           # deploy jack (orange barrel, steel rod)
    j0, j1 = (0, -0.34, -0.70), (0, -1.45, -0.12)
    jm = tuple(j0[k] + (j1[k] - j0[k]) * 0.55 for k in range(3))
    cyl(a_, OR, j0, jm, 0.11, small)
    cyl(a_, "eq_steel", jm, j1, 0.06, small)
    if lod < 2:
        box(a_, TR, j1, (0.20, 0.18, 0.18))
    if close():
        tube(a_, "eq_insulator", bezier((0.10, -0.34, -0.62), (0.2, -0.5, -0.5), (0.15, -0.7, -0.45), (0.11, -0.8, -0.42), 8), 0.018, 6)

    lg = P["po4_leg"] = Part("po4_leg")                                     # deployable structure: V-frame + upper strut
    cyl(lg, W, (0, hy + 0.05, 0), (0, -1.45, 0), 0.20, small if lod else seg // 2)
    lathe(lg, OR, crect(0.20, 0.23, -0.2, 0.2), small if lod else seg // 2, R_Y, (0, -0.85, 0))
    for s in (-1, 1):
        strut(lg, TR, (s * 0.60, hy, 0.0), (s * 0.12, -1.45, 0.0), 0.06, lod)
        strut(lg, TR, (s * 0.60, hy, 0.0), (0.0, -1.45, 0.14 * s), 0.045, lod)
    box(lg, TR, (0, hy, 0), (1.30, 0.16, 0.18), chamfer=0.02 if lod == 0 else 0.0)              # hinge beam
    ylathe(lg, TR, crect(0.18, 0.27, -1.50, -1.38), small if lod else seg // 2)               # lower collar
    if close():
        for s in (-1, 1):
            pipe_run(lg, "eq_steel", [(0.22 * s, hy, 0.10), (0.22 * s, -0.9, 0.22), (0.10 * s, -1.6, 0.20)], 0.018, TR, 0.4, 6)
            box(lg, TR, (0.22 * s, -0.9, 0.20), (0.06, 0.06, 0.06))

    dm = P["po4_damper"] = Part("po4_damper")                               # shock absorber: piston + boot
    cyl(dm, "eq_steel", (0, -1.50, 0), (0, -2.55, 0), 0.12, small if lod else seg // 2)
    if lod < 2:
        for k in range(5):                                                  # dust boot rings
            ylathe(dm, "eq_metal_band", crect(0.12, 0.16, -1.62 - 0.1 * k, -1.57 - 0.1 * k), small)
        ylathe(dm, TR, crect(0.12, 0.20, -2.20, -2.12), small)
    da = P["po4_foot"] = Part("po4_foot")                                   # foot pad (ball joint, orange rim)
    ylathe(da, TR, [(0, fy), (fr - 0.06, fy), (fr, fy + 0.04), (fr, fy + 0.10), (0.30, fy + 0.20), (0.16, fy + 0.30), (0, fy + 0.30)], seg)
    ylathe(da, OR, crect(fr - 0.004, fr + 0.02, fy + 0.03, fy + 0.11), seg)
    sphere(da, "eq_steel", (0, -2.55, 0), 0.17, small)
    if lod < 2:
        for k in range(8):
            a = 2 * math.pi * k / 8
            box(da, TR, polar(0.55, a, fy + 0.15), (0.70, 0.10, 0.05), rot_y(-a))
    return P


def po4_hits():
    hy = PO["hinge_y"]
    return [hbox("po4_mount", (0, -0.10, 0), (1.96, 0.20, 1.96)),
            hbox("po4_mount", (0, (hy - 0.16) / 2, 0), (1.40, -hy + 0.04, 0.42)),
            hbox("po4_actuator", (0, -0.90, -0.41), (0.24, 1.25, 0.24), (-27.8, 0, 0)),
            hcyl("po4_leg", (0, hy, 0), (0, -1.50, 0), 0.27),
            hbox("po4_leg", (0, (hy - 1.45) / 2, 0), (1.3, 1.0, 0.3)),
            hcyl("po4_damper", (0, -1.50, 0), (0, -2.55, 0), 0.16),
            hcyl("po4_foot", (0, PO["foot_y"], 0), (0, PO["foot_y"] + 0.30, 0), PO["foot_r"])]


# ---------------------------------------------------------------- O-4 life support (shuttle): open truss frame, spheres
# Envelope 2.014 x 1.151 x 1.438 (centred): O2 generator = big sphere + electrolyser box, water = sphere, CO2 scrubber =
# twin vertical canisters, fan = blower drum; 4 hard-point feet. life_support SRUs: scrubber, o2, water, fan.
O4 = dict(W=2.014, H=1.151, L=1.438)


def o4_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    hw, hh, hl = O4["W"] / 2 - 0.03, O4["H"] / 2 - 0.03, O4["L"] / 2 - 0.03
    P = {}
    fr = P["o4_frame"] = Part("o4_frame")
    t = 0.07
    for sy in (-1, 1):                                                     # 12 square-tube edges
        for sz in (-1, 1):
            box(fr, TR, (0, sy * (hh - t / 2), sz * (hl - t / 2)), (2 * hw, t, t))
        for sx in (-1, 1):
            box(fr, TR, (sx * (hw - t / 2), sy * (hh - t / 2), 0), (t, t, 2 * hl - 2 * t))
    for sx in (-1, 1):
        for sz in (-1, 1):
            box(fr, TR, (sx * (hw - t / 2), 0, sz * (hl - t / 2)), (t, 2 * hh - 2 * t, t))
    box(fr, W, (0, -hh + 0.05, 0), (2 * hw - 0.1, 0.03, 2 * hl - 0.1))   # floor plate
    for sx in (-1, 1):                                                     # hard-point feet
        for sz in (-1, 1):
            box(fr, OR if lod < 2 else TR, (sx * (hw - 0.09), -hh - 0.015, sz * (hl - 0.09)), (0.18, 0.03, 0.18))
            pyrobolt(fr, (sx * (hw - 0.09), -hh + 0.07, sz * (hl - 0.09)), (0, 1, 0))
    if lod < 2:
        for sz in (-1, 1):                                                 # diagonal braces on the long faces
            strut(fr, TR, (-hw + 0.05, -hh + 0.05, sz * (hl - 0.035)), (0.0, hh - 0.05, sz * (hl - 0.035)), 0.025, lod)
            if sz < 0:
                strut(fr, TR, (hw - 0.05, -hh + 0.05, sz * (hl - 0.035)), (0.0, hh - 0.05, sz * (hl - 0.035)), 0.025, lod)

    o2 = P["o4_o2"] = Part("o4_o2")                                        # O2 generator: sphere + electrolyser box
    c2 = (-0.18, 0.02, -0.12)
    sphere(o2, W, c2, 0.48, seg)
    ylathe(o2, OR, crect(0.475, 0.50, -0.05, 0.05), seg, t=c2)
    box(o2, W, (-0.18, -0.36, 0.48), (0.62, 0.36, 0.34), chamfer=0.02 if lod == 0 else 0.0)
    if lod < 2:
        box(o2, TR, (-0.18, -hh + 0.12, -0.12), (0.5, 0.10, 0.5))                                   # sphere cradle
        cyl(o2, "eq_steel", (-0.18, 0.50, -0.12), (-0.18, 0.58, -0.12), 0.07, small)               # top valve

    wa = P["o4_water"] = Part("o4_water")                                  # water recovery: sphere + filter drum
    cw = (0.58, 0.10, -0.22)
    sphere(wa, W, cw, 0.40, seg)
    ylathe(wa, OR, crect(0.395, 0.42, -0.05, 0.05), seg, t=cw)
    cyl(wa, TR, (0.58, -0.30, -0.22), (0.58, -hh + 0.06, -0.22), 0.16, small)
    if lod < 2:
        cyl(wa, "eq_steel", (0.58, 0.50, -0.22), (0.58, 0.56, -0.22), 0.06, small)

    sc = P["o4_scrubber"] = Part("o4_scrubber")                            # CO2 scrubber: twin canisters (swing beds)
    for z in (-0.35, 0.18):
        cyl(sc, W, (-0.82, -hh + 0.06, z), (-0.82, hh - 0.12, z), 0.15, small, chamfer=0.03 if lod == 0 else 0.0)
        if lod < 2:
            ylathe(sc, OR, crect(0.15, 0.17, 0.1, 0.18), small, t=(-0.82, 0, z))
            cyl(sc, "eq_steel", (-0.82, hh - 0.12, z), (-0.82, hh - 0.06, z), 0.07, small)
    fn = P["o4_fan"] = Part("o4_fan")                                      # blower drum on the front face
    cf = (0.55, -0.20, 0.38)
    cyl(fn, W, cf, (0.55, -0.20, hl - 0.02), 0.28, small if lod else seg // 2)
    box(fn, TR, (0.55, -0.2 - 0.14, 0.38), (0.36, 0.30, 0.40))
    if lod < 2:
        for k in range(4):                                                  # intake grille bars
            box(fn, TR, (0.55, -0.20 - 0.18 + 0.12 * k, hl - 0.015), (0.48, 0.03, 0.02))
    if close():
        hz = P["o4_pipes"] = Part("o4_pipes")
        for k, (a, b) in enumerate((((-0.18, 0.58, -0.12), (-0.82, hh - 0.06, -0.35)), ((-0.18, 0.58, -0.12), (0.58, 0.56, -0.22)),
                                    ((0.58, 0.56, -0.22), (0.55, 0.0, 0.38)), ((-0.82, hh - 0.06, 0.18), (0.55, 0.0, 0.38)))):
            mid = ((a[0] + b[0]) / 2, hh - 0.10, (a[2] + b[2]) / 2)
            pipe_run(hz, "eq_steel" if k % 2 else W, [a, (a[0], hh - 0.10, a[2]), mid, (b[0], hh - 0.10, b[2]), b], 0.022, TR, 0.35, 6)
        for x in (-0.5, 0.2):
            box(hz, TR, (x, hh - 0.07, 0.0), (0.05, 0.06, 2 * hl - 0.1))     # stand-off bracket rails
    return P


def o4_hits():
    hh, hl = O4["H"] / 2, O4["L"] / 2
    return [hbox("o4_frame", (0, -hh + 0.05, 0), (O4["W"], 0.10, O4["L"])),
            hbox("o4_o2", (-0.18, 0.02, -0.12), (0.96, 0.96, 0.96)),
            hbox("o4_o2", (-0.18, -0.36, 0.48), (0.62, 0.36, 0.34)),
            hbox("o4_water", (0.58, 0.0, -0.22), (0.80, 1.0, 0.80)),
            hcyl("o4_scrubber", (-0.82, -hh, -0.35), (-0.82, hh - 0.06, -0.35), 0.17),
            hcyl("o4_scrubber", (-0.82, -hh, 0.18), (-0.82, hh - 0.06, 0.18), 0.17),
            hcyl("o4_fan", (0.55, -0.20, 0.24), (0.55, -0.20, hl), 0.28)]


# ---------------------------------------------------------------- P20 passenger module (12 seats, jettisonable)
# Envelope 3.61 x 2.578 x 5.157 (centred, long axis Z): a flattened stringered pressure barrel with domed ends, round
# fore hatch (+Z) and side hatch (-X), climate unit on the roof, power distribution under the floor, separation system
# at the aft (-Z) interface: OFU truss ring with 4 hard points + pyrobolts and 4 separation motors on the roof corners.
P20 = dict(w=3.30, h=2.20, rad=0.95, z0=-2.30, z1=2.35)
P20_LZ = 5.157 / 2


def rr_pts(w, h, rad, n):
    out = []
    for (cx, cy), a0 in (((w / 2 - rad, -(h / 2 - rad)), -90), ((w / 2 - rad, h / 2 - rad), 0),
                         ((-(w / 2 - rad), h / 2 - rad), 90), ((-(w / 2 - rad), -(h / 2 - rad)), 180)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            out.append((cx + rad * math.cos(a), cy + rad * math.sin(a), math.cos(a), math.sin(a)))
    return out


def loft(part, mat, secs, n, R=I3, t=(0, 0, 0), caps=(True, True)):
    """rounded-rectangle sections (w, h, rad, z) lofted along local Z; smooth around, per-band slope normals"""
    rings = [rr_pts(w, h, min(rad, w / 2, h / 2), n) for w, h, rad, z in secs]
    for k in range(len(secs) - 1):
        (w0, h0, _, z0), (w1, h1, _, z1) = secs[k], secs[k + 1]
        s = -((w1 - w0) + (h1 - h0)) / 4 / max(z1 - z0, 1e-9)
        a = [part.vert((x, y, z0), norm((nx, ny, s)), R, t) for x, y, nx, ny in rings[k]]
        b = [part.vert((x, y, z1), norm((nx, ny, s)), R, t) for x, y, nx, ny in rings[k + 1]]
        m = len(a)
        for i in range(m):
            j = (i + 1) % m
            nx = rings[k][i][2] + rings[k][j][2]
            ny = rings[k][i][3] + rings[k][j][3]
            part.face(mat, [a[i], a[j], b[j], b[i]], apply(R, norm((nx, ny, s * 2))))
    for idx, sgn in ((0, -1), (-1, 1)):
        if caps[0 if idx == 0 else 1]:
            w, h, rad, z = secs[idx]
            if w > 1e-6 and h > 1e-6:
                part.face(mat, [part.vert((x, y, z), (0, 0, sgn), R, t) for x, y, _, _ in rings[idx]], apply(R, (0, 0, sgn)))


def p20_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = P20
    w, h, rad, z0, z1 = d["w"], d["h"], d["rad"], d["z0"], d["z1"]
    nc = 6 if close() else {0: 4, 1: 2, 2: 2}[lod]
    P = {}
    hu = P["p20_hull"] = Part("p20_hull")
    secs = [(w - 0.9, h - 0.7, rad - 0.35, z0), (w - 0.25, h - 0.18, rad - 0.1, z0 + 0.22), (w, h, rad, z0 + 0.45),
            (w, h, rad, z1 - 0.45), (w - 0.25, h - 0.18, rad - 0.1, z1 - 0.22), (w - 0.9, h - 0.7, rad - 0.35, z1)]
    loft(hu, W, secs, nc)
    if lod < 2:
        for k, fz in enumerate([z0 + 0.45 + i * (z1 - z0 - 0.9) / 5 for i in range(6)]):     # ring frames (rib bands)
            loft(hu, OR if k in (0, 5) else TR, [(w + 0.08, h + 0.08, rad + 0.04, fz - 0.06), (w + 0.08, h + 0.08, rad + 0.04, fz + 0.06)],
                 nc, caps=(False, False))
        for s in (-1, 1):                                                    # stringers on the flat sides
            for yy in (-0.35, 0.35):
                box(hu, TR, (s * (w / 2 + 0.02), yy, (z0 + z1) / 2), (0.05, 0.07, z1 - z0 - 0.95))
        for s in (-1, 1):                                                    # portholes, 4 per side
            for k in range(4):
                zz = z0 + 0.9 + k * 0.85
                if s < 0 and k == 1:
                    continue
                lathe(hu, TR, crect(0.13, 0.20, 0.0, 0.05), small, axis_to((s, 0, 0)), (s * w / 2, 0.30, zz))
                lathe(hu, "eq_ofu_window", [(0.13, 0.02), (0.0, 0.025)], small, axis_to((s, 0, 0)), (s * w / 2, 0.30, zz))
    hc = P["p20_hatch"] = Part("p20_hatch")                                  # seals: fore round hatch + side hatch
    lathe(hc, TR, crect(0.0, 0.62, 0.0, 0.10, 0.02 if lod == 0 else 0.0), seg, I3, (0, 0, z1 - 0.02))
    if lod < 2:
        lathe(hc, OR, crect(0.62, 0.68, -0.02, 0.06), seg, I3, (0, 0, z1 - 0.02))
        lathe(hc, "eq_metal_band", crect(0.30, 0.40, 0.10, 0.14), small, I3, (0, 0, z1 - 0.02))      # hand wheel
        for i in range(3):
            box(hc, "eq_metal_band", (0.17 * math.cos(i * 2.09), 0.17 * math.sin(i * 2.09), z1 + 0.10), (0.34, 0.04, 0.03), rot_z(i * 2.09))
    loft(hc, W, [(0.95, 1.65, 0.30, 0.0), (0.95, 1.65, 0.30, 0.06)], nc, axis_to((-1, 0, 0)), (-w / 2 - 0.03, -0.05, z0 + 1.75))
    if lod < 2:
        loft(hc, OR, [(1.07, 1.77, 0.34, 0.0), (1.07, 1.77, 0.34, 0.03)], nc, axis_to((-1, 0, 0)), (-w / 2 - 0.02, -0.05, z0 + 1.75),
             caps=(False, True))
        box(hc, "eq_steel", (-w / 2 - 0.11, 0.10, z0 + 1.75 + 0.30), (0.05, 0.40, 0.06))
    if lod == 0:
        for i in range(16):
            hex_bolt(hc, "eq_steel", polz(0.52, 2 * math.pi * i / 16, z1 + 0.08), (0, 0, 1), 0.06, 0.03, washer=False)

    cl = P["p20_climate"] = Part("p20_climate")                              # climate unit on the roof
    box(cl, W, (0.0, h / 2 + 0.04, z0 + 1.4), (1.5, 0.22, 1.5), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        for k in range(6):
            box(cl, TR, (-0.6 + 0.24 * k, h / 2 + 0.16, z0 + 1.4), (0.08, 0.03, 1.3))
        cyl(cl, TR, (0.45, h / 2 + 0.15, z0 + 0.85), (0.45, h / 2 + 0.20, z0 + 0.85), 0.20, small)
    pw = P["p20_power"] = Part("p20_power")                                  # power distribution under the floor
    box(pw, W, (0.0, -h / 2 - 0.04, 0.6), (1.2, 0.20, 1.8), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        box(pw, OR, (0.0, -h / 2 - 0.145, 0.6), (1.0, 0.01, 0.12))
        for k in range(3):
            cyl(pw, "eq_steel", (-0.3 + 0.3 * k, -h / 2 - 0.10, 1.5), (-0.3 + 0.3 * k, -h / 2 - 0.10, 1.62), 0.05, small)
    sp = P["p20_separation"] = Part("p20_separation")                        # aft truss ring + 4 hard points + motors
    zr = -P20_LZ
    lathe(sp, TR, crect(0.90, 1.12, zr, zr + 0.16, 0.02 if lod == 0 else 0.0), seg)
    lathe(sp, OR, crect(1.12, 1.15, zr + 0.03, zr + 0.13), seg)
    for k in range(6 if lod < 2 else 3):
        a = 2 * math.pi * k / (6 if lod < 2 else 3)
        strut(sp, TR, polz(1.0, a, zr + 0.16), polz(1.25, a + 0.4, z0 + 0.15), 0.05, lod)
        strut(sp, TR, polz(1.0, a, zr + 0.16), polz(1.25, a - 0.4, z0 + 0.15), 0.05, lod)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        box(sp, TR, polz(1.15, a, zr + 0.12), (0.28, 0.26, 0.24), rot_z(a))
        pyrobolt(sp, polz(1.22, a, zr + 0.24), (0, 0, 1))
    for sx in (-1, 1):                                                       # separation motors on the roof corners
        for zz in (z0 + 0.3, z1 - 0.3):
            c0 = (sx * 1.25, h / 2 - 0.20, zz)
            c1 = (c0[0] + sx * 0.12, c0[1] + 0.18, zz)
            cyl(sp, W, c0, c1, 0.13, small)
            lathe(sp, OR, [(0.07, 0), (0.12, 0.10), (0.11, 0.10), (0.06, 0.02)], small, axis_to(norm((sx * 0.12, 0.18, 0))), c1)
    for sx in (-1, 1):                                                       # lower hard-point lugs to the hull frame
        for zz in (z0 + 0.8, z1 - 0.8):
            box(sp, TR, (sx * 1.15, -h / 2 + 0.05, zz), (0.30, 0.20, 0.36), chamfer=0.02 if lod == 0 else 0.0)
            pyrobolt(sp, (sx * 1.15, -h / 2 - 0.05, zz), (0, -1, 0))
    if close():
        hz = P["p20_pipes"] = Part("p20_pipes")
        for k in range(2):
            yy = h / 2 - 0.30 - 0.08 * k
            standoff_pipes(hz, [(w / 2 + 0.10, yy, z0 + 0.4), (w / 2 + 0.10, yy, z1 - 0.5)], 1, 0.025, (0, 0, 0))
        for zz in (z0 + 0.9, z0 + 2.6, z1 - 0.6):
            box(hz, TR, (w / 2 + 0.06, h / 2 - 0.34, zz), (0.10, 0.20, 0.05))
        harness(hz, [(0.45, -h / 2 - 0.10, 1.5), (0.45, -h / 2 - 0.10, 1.9), (0.40, -h / 2 + 0.05, z1 - 0.15),
                     (0.35, -0.45, z1 - 0.05)], 0.02, 3, ("eq_insulator", OR), tie_every=1)
    return P


def p20_hits():
    d = P20
    w, h, z0, z1 = d["w"], d["h"], d["z0"], d["z1"]
    return [hbox("p20_hull", (0, 0, (z0 + z1) / 2), (w + 0.08, h + 0.08, z1 - z0)),
            hcyl("p20_hatch", (0, 0, z1 - 0.02), (0, 0, z1 + 0.14), 0.68),
            hbox("p20_hatch", (-w / 2 - 0.05, -0.05, z0 + 1.75), (0.10, 1.77, 1.07)),
            hbox("p20_climate", (0, h / 2 + 0.07, z0 + 1.4), (1.5, 0.28, 1.5)),
            hbox("p20_power", (0, -h / 2 - 0.05, 0.6), (1.2, 0.22, 1.8)),
            hcyl("p20_separation", (0, 0, -P20_LZ), (0, 0, z0 + 0.15), 1.20)] + \
        [hbox("p20_separation", (sx * 1.31, h / 2 - 0.08, zz), (0.34, 0.34, 0.30)) for sx in (-1, 1) for zz in (z0 + 0.3, z1 - 0.3)]




ASSETS = {f"ofu_{k}_drive": drive_parts_fn(k) for k in DRIVES}
ASSETS.update({"ofu_tz12_heat_shield": tz12_parts, "ofu_po4_landing_gear": po4_parts, "ofu_o4_life_support": o4_parts,
               "ofu_p20_passenger_module": p20_parts})
HITS = {f"ofu_{k}_drive": drive_hits_fn(k) for k in DRIVES}
HITS.update({"ofu_tz12_heat_shield": tz12_hits, "ofu_po4_landing_gear": po4_hits, "ofu_o4_life_support": o4_hits,
             "ofu_p20_passenger_module": p20_hits})
MODEL_ASSETS = {"ofu.drive.u33": "ofu_u33_drive", "ofu.drive.u65m": "ofu_u65m_drive", "ofu.drive.u70": "ofu_u70_drive",
                "ofu.drive.u73l": "ofu_u73l_drive", "ofu.lift.shield": "ofu_tz12_heat_shield",
                "ofu.lift.gear": "ofu_po4_landing_gear", "ofu.ls.o4": "ofu_o4_life_support", "ofu.cargo.p20": "ofu_p20_passenger_module"}
