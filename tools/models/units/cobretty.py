"""Cobretty Corporation (Hellas-2, premium modular school): CR-1 Vector field drive, ER-5 / ER-1 propulsion reactors."""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

# ---------------------------------------------------------------- Cobretty CR-1 Vector
CR1 = dict(length=6.5, pad_r=2.5, shell_r=2.05, band_r=2.15, flange_r=2.25, flange_t=0.35,
           coil_z0=5.30, coil_z1=6.15, pad_z0=6.15, pad_z1=6.50)


def cr1_parts(lod):
    seg = seg_for(lod)
    d = CR1
    P = {}

    # --- flange to the propulsion reactor (inner end), busbar coupling + data connector
    f = P["cr1_flange"] = Part("cr1_flange")
    ft = d["flange_t"]
    if lod == 0:
        lathe(f, "eq_metal_band", [(0, 0), (d["flange_r"] - 0.04, 0), (d["flange_r"], 0.04), (d["flange_r"], ft - 0.04),
                                   (d["flange_r"] - 0.04, ft), (1.9, ft), (1.9, ft + 0.12), (0, ft + 0.12)], seg)
        for i in range(24):
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.08 * math.cos(a), 2.08 * math.sin(a), ft), (0, 0, 1))
        # mating face: centring spigot ring, busbar coupling boss with 3 terminals, data connector
        lathe(f, "eq_metal_graphite", crect(1.55, 1.75, -0.08, 0.0, 0.015), seg)  # centring spigot
        lathe(f, "eq_metal_graphite", [(0, 0), (0.62, 0), (0.62, -0.18), (0.56, -0.24), (0, -0.24)][::-1], seg)
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.32 * math.cos(a), 0.32 * math.sin(a), -0.24)
            lathe(f, "eq_insulator", [(0, 0), (0.11, 0), (0.11, -0.05), (0, -0.05)][::-1], 24, I3, p)
            lathe(f, "eq_copper", [(0, -0.05), (0.06, -0.05), (0.06, -0.20), (0.045, -0.22), (0, -0.22)][::-1], 24, I3, p)
        box(f, "eq_metal_graphite", (0.0, -1.15, -0.06), (0.36, 0.22, 0.12), chamfer=0.015)
        for i in range(4):
            cyl(f, "eq_copper", (-0.12 + 0.08 * i, -1.15, -0.12), (-0.12 + 0.08 * i, -1.15, -0.14), 0.018, 8)
    else:
        lathe(f, "eq_metal_band", [(0, 0), (d["flange_r"], 0), (d["flange_r"], ft), (1.9, ft), (1.9, ft + 0.12), (0, ft + 0.12)][::1], seg)
        lathe(f, "eq_metal_graphite", [(0, -0.24), (0.62, -0.24), (0.62, 0), (0, 0)], seg)

    # --- housing: closed cylinder with ring bands (the neck after the flange, the shell up to the coil ring)
    h = P["cr1_housing"] = Part("cr1_housing")
    z0, z1 = ft + 0.12, d["coil_z0"]
    sr, br = d["shell_r"], d["band_r"]
    bands = [1.05, 2.45, 3.85, 5.05]
    if lod == 2:
        lathe(h, "eq_metal_graphite", [(1.9, z0), (sr, z0 + 0.15), (sr, z1), (1.8, z1)], seg)
        for bz in bands[::2]:
            lathe(h, "eq_metal_band", crect(sr - 0.02, br, bz - 0.09, bz + 0.09), seg)
    else:
        prof = [(1.9, z0), (1.9, z0 + 0.08), (sr - 0.05, z0 + 0.20), (sr, z0 + 0.25), (sr, z1), (1.8, z1)]
        lathe(h, "eq_metal_graphite", prof, seg)
        for bz in bands:
            c = 0.02 if lod == 0 else 0.0
            lathe(h, "eq_metal_band", crect(sr - 0.02, br, bz - 0.09, bz + 0.09, c), seg)
            if lod == 0:  # band joint blocks with two bolts each
                for i in range(4):
                    a = 2 * math.pi * (i + 0.5) / 4
                    Rz = rot_z(a)
                    box(h, "eq_metal_band", apply(Rz, (br + 0.03, 0, 0)) [:2] + (bz,), (0.08, 0.16, 0.22), mat_mul(Rz, I3), chamfer=0.012)
                    for s in (-1, 1):
                        hex_bolt(h, "eq_steel", apply(Rz, (br + 0.07, 0.0, 0))[:2] + (bz + s * 0.06,), apply(Rz, (1, 0, 0)), 0.045, 0.03, washer=False)
        # access panels between the bands: 4 around, the service-module side (angle 0) left free
        if lod == 0:
            bays = [(bands[i] + 0.16, bands[i + 1] - 0.16) for i in range(len(bands) - 1)]
            for zb0, zb1 in bays:
                for k in range(4):
                    ac = math.pi / 4 + k * math.pi / 2
                    if abs(((ac + math.pi) % (2 * math.pi)) - math.pi) < 0.6:
                        continue
                    a0, a1 = ac - 0.42, ac + 0.42
                    lathe(h, "eq_metal_panel", crect(sr - 0.01, sr + 0.035, zb0, zb1, 0.012), 8, I3, (0, 0, 0), a0, a1, cap=True)
                    for zz in (zb0 + 0.12, zb1 - 0.12):          # panel fasteners
                        for aa in (a0 + 0.05, a1 - 0.05):
                            hex_bolt(h, "eq_steel", (sr * math.cos(aa) * 1.017, sr * math.sin(aa) * 1.017, zz), (math.cos(aa), math.sin(aa), 0), 0.04, 0.02, washer=False)
                    am = ac  # handle
                    Rz = rot_z(am)
                    for zz in ((zb0 + zb1) / 2 - 0.12, (zb0 + zb1) / 2 + 0.12):
                        box(h, "eq_steel", apply(Rz, (sr + 0.07, 0, 0))[:2] + (zz,), (0.07, 0.035, 0.035), Rz)
                    box(h, "eq_steel", apply(Rz, (sr + 0.105, 0, 0))[:2] + ((zb0 + zb1) / 2,), (0.03, 0.035, 0.30), Rz, chamfer=0.008)
            # longitudinal stiffeners between panels
            for k in range(4):
                a = k * math.pi / 2
                Rz = rot_z(a)
                if k == 0:
                    continue
                box(h, "eq_metal_band", apply(Rz, (sr + 0.02, 0, 0))[:2] + ((z0 + z1) / 2 + 0.1,), (0.06, 0.10, z1 - z0 - 0.5), Rz, chamfer=0.01)
        # LOD1/LOD2: no panels (35 mm relief is invisible beyond ~30 m)

    # --- field coil ring behind the pad: 12 coil housings on a carrier ring
    c = P["cr1_coil_ring"] = Part("cr1_coil_ring")
    cz0, cz1 = d["coil_z0"], d["coil_z1"]
    lathe(c, "eq_metal_band", crect(1.7, 2.25, cz0, cz1, 0.03 if lod == 0 else 0.0), seg)
    if lod < 2:
        n = 12
        for i in range(n):
            a0 = 2 * math.pi * i / n + 0.06
            a1 = 2 * math.pi * (i + 1) / n - 0.06
            lathe(c, "eq_field_coil", crect(2.22, 2.44, cz0 + 0.07, cz1 - 0.07), 6 if lod == 0 else 3, I3, (0, 0, 0), a0, a1, cap=True)
            if lod == 0:
                am = (a0 + a1) / 2
                Rz = rot_z(am)
                # retaining strap and two cable glands per coil
                lathe(c, "eq_steel", crect(2.44, 2.47, (cz0 + cz1) / 2 - 0.05, (cz0 + cz1) / 2 + 0.05), 6, I3, (0, 0, 0), a0 + 0.02, a1 - 0.02, cap=True)
                for s in (1,):                                   # one cable gland per coil
                    q = apply(Rz, (2.30, s * 0.32, 0))
                    cyl(c, "eq_insulator", (q[0], q[1], cz0 + 0.07), (q[0], q[1], cz0 - 0.05), 0.035, 8)
                    cyl(c, "eq_copper", (q[0], q[1], cz0 - 0.05), (q[0], q[1], cz0 - 0.12), 0.022, 6)
    else:
        lathe(c, "eq_field_coil", crect(2.22, 2.44, cz0 + 0.07, cz1 - 0.07), seg)

    # --- cooling pad = engine_hot_emitter_annulus: 8 replaceable ceramic sectors in a clamp rim, centre hub
    p = P["engine_hot_emitter_annulus"] = Part("engine_hot_emitter_annulus")
    pz0, pz1, pr = d["pad_z0"], d["pad_z1"], d["pad_r"]
    lathe(p, "eq_metal_band", [(0, pz0), (2.3, pz0), (2.3, pz0 + 0.05), (0, pz0 + 0.05)], seg)  # backing plate
    if lod < 2:
        gap = 0.012
        for i in range(8):
            a0 = 2 * math.pi * i / 8 + gap
            a1 = 2 * math.pi * (i + 1) / 8 - gap
            lathe(p, "eq_pad_ceramic", crect(0.48, 2.36, pz0 + 0.05, pz1 - 0.03), max(2, seg // 8), I3, (0, 0, 0), a0, a1, cap=True)
        lathe(p, "eq_steel", crect(2.36, 2.44, pz0, pz1, 0.015 if lod == 0 else 0.0), seg)                      # clamp rim
        lathe(p, "eq_metal_band", [(0, pz0 + 0.05), (0.48, pz0 + 0.05), (0.48, pz1 - 0.01), (0.42, pz1 + 0.02), (0, pz1 + 0.02)], seg)  # hub
        if lod == 0:
            hex_bolt(p, "eq_steel", (0, 0, pz1 + 0.02), (0, 0, 1), 0.14, 0.07)
            for i in range(8):                                   # sector seams: radial clamp bars on the gaps
                a = 2 * math.pi * i / 8
                Rz = rot_z(a)
                box(p, "eq_steel", apply(Rz, (1.42, 0, 0))[:2] + (pz1 - 0.01,), (1.88, 0.05, 0.03), Rz, chamfer=0.006)
                for rr in (0.75, 1.43, 2.1):
                    hex_bolt(p, "eq_steel", apply(Rz, (rr, 0, 0))[:2] + (pz1 + 0.005,), (0, 0, 1), 0.035, 0.018, washer=False)
            for i in range(16):                                  # quick-release clamps on the rim
                a = 2 * math.pi * (i + 0.5) / 16
                Rz = rot_z(a)
                box(p, "eq_steel", apply(Rz, (2.42, 0, 0))[:2] + (pz1 + 0.025,), (0.16, 0.14, 0.05), Rz, chamfer=0.01)
                box(p, "eq_metal_band", apply(Rz, (2.475, 0, 0))[:2] + ((pz0 + pz1) / 2,), (0.05, 0.12, 0.30), Rz, chamfer=0.01)
    else:
        lathe(p, "eq_pad_ceramic", [(0, pz0 + 0.05), (pr, pz0 + 0.05), (pr, pz1), (0, pz1)], seg)
        lathe(p, "eq_steel", [(pr - 0.01, pz0), (pr, pz0), (pr, pz1), (pr - 0.01, pz1)], seg)

    # --- service module: backup heat exchanger, two coolant ports, field / temperature sensors (side +X)
    s = P["cr1_service_module"] = Part("cr1_service_module")
    sx, sz = sr + 0.32, 2.95
    box(s, "eq_metal_panel", (sx, 0, sz), (0.62, 1.30, 2.10), chamfer=0.05 if lod == 0 else 0.0)
    box(s, "eq_metal_graphite", (sr + 0.05, 0, sz), (0.20, 1.10, 1.90))                         # saddle to the shell
    if lod < 2:
        for yy in (-0.38, 0.38):                                                                 # two coolant ports
            zz = sz - 0.62
            cyl(s, "eq_copper", (sx + 0.31, yy, zz), (sx + 0.62, yy, zz), 0.10, 16 if lod == 0 else 8)
            lathe(s, "eq_steel", [(0, 0), (0.17, 0), (0.17, 0.06), (0, 0.06)], 16 if lod == 0 else 8, axis_to((1, 0, 0)), (sx + 0.62, yy, zz))
            if lod == 0:
                for i in range(6):
                    a = 2 * math.pi * i / 6
                    hex_bolt(s, "eq_steel", (sx + 0.68, yy + 0.13 * math.cos(a), zz + 0.13 * math.sin(a)), (1, 0, 0), 0.03, 0.02, washer=False)
        if lod == 0:
            # access door with hinges and latch, status window, sensor heads on the top
            box(s, "eq_metal_graphite", (sx + 0.315, 0, sz + 0.35), (0.02, 1.0, 1.05), chamfer=0.006)
            for zz in (sz - 0.05, sz + 0.75):
                cyl(s, "eq_steel", (sx + 0.335, -0.52, zz - 0.09), (sx + 0.335, -0.52, zz + 0.09), 0.025, 6)
            box(s, "eq_steel", (sx + 0.34, 0.45, sz + 0.35), (0.03, 0.05, 0.22), chamfer=0.006)
            box(s, "eq_status_amber", (sx + 0.33, 0.25, sz + 0.80), (0.02, 0.16, 0.05))
            box(s, "eq_status_amber", (sx + 0.33, 0.25, sz + 0.72), (0.02, 0.16, 0.05))
            for yy in (-0.35, 0.0, 0.35):
                cyl(s, "eq_metal_graphite", (sx, yy, sz + 1.05), (sx, yy, sz + 1.17), 0.06, 10)
                lathe(s, "eq_insulator", [(0.04, 0), (0.035, 0.025), (0, 0.04)], 8, I3, (sx, yy, sz + 1.17))
            # cable conduit from the module to the coil ring
            cyl(s, "eq_metal_band", (sr + 0.17, 0.45, sz + 1.05), (sr + 0.17, 0.45, d["coil_z0"]), 0.05, 8)
            cyl(s, "eq_metal_band", (sr + 0.17, -0.45, sz + 1.05), (sr + 0.17, -0.45, d["coil_z0"]), 0.05, 8)
            for zz in (sz + 1.6, sz + 2.1):
                for yy in (-0.45, 0.45):
                    box(s, "eq_steel", (sr + 0.17, yy, zz), (0.14, 0.14, 0.05), chamfer=0.01)

    # --- four mounting lugs to the engine bay frame (no floor stand)
    m = P["cr1_mounts"] = Part("cr1_mounts")
    for k, (a, zz) in enumerate(((math.pi * 3 / 4, 1.05), (-math.pi * 3 / 4, 1.05), (math.pi * 3 / 4, 3.85), (-math.pi * 3 / 4, 3.85))):
        Rz = rot_z(a)
        box(m, "eq_metal_band", apply(Rz, (2.28, 0, 0))[:2] + (zz,), (0.42, 0.46, 0.14), Rz, chamfer=0.02 if lod == 0 else 0.0)
        if lod < 2:
            q = apply(Rz, (2.36, 0, 0))
            lathe(m, "eq_steel", crect(0.06, 0.11, -0.10, 0.10), 12 if lod == 0 else 6, I3, (q[0], q[1], zz))

    # --- close-inspection extras (asset LOD0): harness ring with drops to the coil glands, coolant pipes, data cable
    if close():
        hz = P["cr1_harness"] = Part("cr1_harness")
        zr = d["coil_z0"] - 0.22
        ring = cyl_path(sr + 0.20, 0.0, 2 * math.pi, zr, zr, 48)
        harness(hz, ring, 0.022, 3, ("eq_insulator", "eq_metal_graphite"), tie_every=4)
        for i in range(12):
            a = 2 * math.pi * (i + 0.5) / 12
            g = apply(rot_z(a), (2.30, 0.32, 0))
            harness(hz, bezier(polz(sr + 0.20, math.atan2(g[1], g[0]), zr), polz(sr + 0.32, math.atan2(g[1], g[0]), zr + 0.05),
                               (g[0] * 1.05, g[1] * 1.05, d["coil_z0"] - 0.20), (g[0], g[1], d["coil_z0"] - 0.12), 6), 0.016, 2,
                    ("eq_insulator",), tie_every=10)
        for yy in (-0.38, 0.38):                                                   # coolant pipes to the flange disconnect
            zz = sz - 0.62
            pipe_run(hz, "eq_copper", [(sx + 0.62, yy, zz), (sx + 0.80, yy, zz), (sx + 0.80, yy, 0.95), (sr + 0.45, yy, 0.62)], 0.07)
            box(hz, "eq_metal_graphite", (sr + 0.42, yy, 0.58), (0.22, 0.20, 0.16), chamfer=0.02)
        harness(hz, bezier((sx, -0.62, sz - 0.9), (sx + 0.1, -1.6, sz - 1.4), polz(sr + 0.25, -math.pi / 2 + 0.15, 1.2),
                           polz(sr + 0.12, -math.pi / 2, 0.55), 18), 0.03, 4, ("eq_insulator", "eq_metal_graphite"))
    return P




# ---------------------------------------------------------------- Cobretty ER-5 propulsion reactor
# Passport (CobraCobrettyER5PropulsionReactor.h, canon kb.reactor.er5): 5000 MWe D-He3, 25 t, body 5 x 5 x 5 m flange to
# flange, square cooling pad 11 x 11 m x 0.25 m (PT1) above it on a short heat-pipe column.
# Author 2026-11-09: the reactor has the drive's diameter so drive - reactor - drive read as one cylindrical engine bay.
# So the torus is COAXIAL with the bay (axis = OBJ Z, like the CR-1 axis): outer dia 4.4 m (+ hoops / units up to 5 m),
# the two flanges sit on its end faces (OBJ -Z fore, +Z aft; mating faces at z = -/+2.5) and mate the CR-1 flange exactly
# (r 2.25, 24 bolts on r 2.08, spigot groove r 1.50-1.80, busbar pocket r 0.68 with 3 sockets on r 0.32, data socket at
# y = -1.15). The pad is on top (+Y). Serviceable units = SRUs of the fault class reactor.fusion: pad, plasma_ctl,
# damper (confinement magnet hoops), feed (pellet feed), inverter, sensor.
ER5 = dict(r_in=0.95, r_out=2.2, zh=1.75, corner=0.55, face_z=2.5, flange_r=2.25,
           pad_y0=3.0, pad_t=0.25, pad_half=5.5)




def er5_flange(lod, seg):
    """one flange in its local frame: mating face at z = 0 facing +Z, the neck going -Z onto the torus end face"""
    d = ER5
    f = Part("flange")
    fr = d["flange_r"]
    if lod == 0:   # the neck bottom is buried in the torus face and is not built
        prof = [(1.9, -0.80), (1.9, -0.30), (fr, -0.30), (fr, 0),
                (1.80, 0), (1.80, -0.10), (1.50, -0.10), (1.50, 0), (0.68, 0), (0.68, -0.27), (0, -0.27)]
    else:
        prof = [(1.9, -0.80), (1.9, -0.30), (fr, -0.30), (fr, 0), (0.68, 0), (0.68, -0.27), (0, -0.27)]
    lathe(f, "eq_metal_band", [p for i, p in enumerate(prof) if i == 0 or p != prof[i - 1]], seg)
    if lod == 0:
        for i in range(3):                                                              # busbar sockets
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.32 * math.cos(a), 0.32 * math.sin(a), -0.27)
            lathe(f, "eq_insulator", [(0.14, 0), (0.14, 0.04), (0, 0.04)], 12 if lod == 0 else 6, I3, p)
            lathe(f, "eq_copper", [(0.075, 0.04), (0.075, 0.07), (0, 0.07)], 12 if lod == 0 else 6, I3, p)
        box(f, "eq_metal_graphite", (0.0, -1.15, 0.004), (0.40, 0.26, 0.008))               # data socket plate
    if lod == 0:
        for i in range(24):                                                             # nuts of the 24 flange bolts
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.08 * math.cos(a), 2.08 * math.sin(a), -0.30), (0, 0, -1))
        for i in range(4):                                                              # data socket pins
            cyl(f, "eq_copper", (-0.12 + 0.08 * i, -1.15, 0.008), (-0.12 + 0.08 * i, -1.15, 0.012), 0.022, 8)
        for i in range(8):                                                              # gussets neck -> flange disc
            a = 2 * math.pi * i / 8 + math.pi / 8
            Rz = rot_z(a)
            box(f, "eq_metal_band", apply(Rz, (2.03, 0, 0))[:2] + (-0.52,), (0.30, 0.06, 0.44), Rz, chamfer=0.01)
    return f




def er5_parts(lod):
    seg = seg_for(lod)
    small = small_for(lod)
    d = ER5
    ri, ro, zh, cr = d["r_in"], d["r_out"], d["zh"], d["corner"]
    nc = {0: 3, 1: 1, 2: 1}[lod]
    P = {}
    hoops = [math.pi / 8 + k * math.pi / 4 for k in range(8)]          # 22.5 deg + k * 45 deg: units sit between them

    # --- vessel: the coaxial torus, a coolant manifold band round the middle, access panels on the upper shoulders
    v = P["er5_vessel"] = Part("er5_vessel")
    lathe(v, "eq_metal_graphite", rrect(ri, ro, -zh, zh, cr, nc), seg, soft=35)
    if lod < 2:
        lathe(v, "eq_metal_band", crect(ro - 0.02, ro + 0.07, -0.09, 0.09, 0.015 if lod == 0 else 0.0), seg)
        for ac in (math.pi / 4, 3 * math.pi / 4):
            for z0, z1 in ((0.18, zh - cr - 0.05), (-(zh - cr - 0.05), -0.18)):
                lathe(v, "eq_metal_panel", crect(ro - 0.01, ro + 0.035, z0, z1, 0.01 if lod == 0 else 0.0), max(4, seg // 16),
                      I3, (0, 0, 0), ac - 0.30, ac + 0.30, cap=True)
                if lod == 0:
                    for aa in (ac - 0.25, ac + 0.25):
                        for zz in (z0 + 0.08, z1 - 0.08):
                            hex_bolt(v, "eq_steel", polz(ro + 0.035, aa, zz), polz(1, aa, 0), 0.04, 0.02, washer=False)
                    Rz = rot_z(ac)
                    box(v, "eq_steel", apply(Rz, (ro + 0.06, 0, 0))[:2] + ((z0 + z1) / 2,), (0.03, 0.035, 0.30), Rz, chamfer=0.008)

    # --- confinement magnets / dampers: 8 poloidal hoop housings round the tube cross-section
    m = P["er5_confinement"] = Part("er5_confinement")
    for a in hoops if lod < 2 else []:
        lathe(m, "eq_field_coil", rrect(ri - 0.07, ro + 0.07, -zh - 0.07, zh + 0.07, cr + 0.06, min(nc, 2)), small // 2, I3, (0, 0, 0),
              a - 0.07, a + 0.07, cap=True, soft=35)
        if lod == 0:
            lathe(m, "eq_metal_band", rrect(ri - 0.09, ro + 0.09, -zh - 0.09, zh + 0.09, cr + 0.07, 2), 2, I3, (0, 0, 0),
                  a - 0.02, a + 0.02, cap=True, soft=35)
        if lod == 0:
            Rz = rot_z(a)
            box(m, "eq_metal_band", apply(Rz, (ro + 0.13, 0, 0))[:2] + (0.0,), (0.10, 0.36, 0.70), Rz, chamfer=0.02)   # clamp block
            for zz in (-0.22, 0.22):
                hex_bolt(m, "eq_steel", polz(ro + 0.18, a, zz), polz(1, a, 0), 0.05, 0.03, washer=False)
            cyl(m, "eq_insulator", polz(ro + 0.18, a, 0.0), polz(ro + 0.28, a, 0.0), 0.05, 8)      # lead-in

    # --- flanges to the two drives (on the torus end faces)
    fl = er5_flange(lod, seg)
    P["er5_flange_aft"] = placed(fl, I3, (0, 0, d["face_z"]))
    P["er5_flange_aft"].name = "er5_flange_aft"
    P["er5_flange_fore"] = placed(fl, rot_y(math.pi), (0, 0, -d["face_z"]))
    P["er5_flange_fore"].name = "er5_flange_fore"

    # --- power inverter to the engine bus (+X side), two busbars to each flange
    iv = P["er5_inverter"] = Part("er5_inverter")
    xi = ro + 0.13
    box(iv, "eq_metal_panel", (xi, 0, 0), (0.30, 1.0, 1.25), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        for zs in (-1, 1):
            for yy in (-0.24, 0.24):
                a, b, c2 = (xi, yy, zs * 0.62), (xi, yy, zs * 1.70), (2.05, yy, zs * 2.18)
                cyl(iv, "eq_insulator", a, b, 0.07, small)
                cyl(iv, "eq_insulator", b, c2, 0.07, small)
                lathe(iv, "eq_insulator", [(0, -0.07), (0.07, -0.07), (0.07, 0.07), (0, 0.07)], small, I3, b)   # elbow
                if lod == 0:
                    cyl(iv, "eq_copper", a, (xi, yy, zs * 0.70), 0.085, small)
                    cyl(iv, "eq_steel", (xi, yy, zs * 1.15 - 0.04), (xi, yy, zs * 1.15 + 0.04), 0.09, small)   # support clamp
    if lod == 0:
        box(iv, "eq_metal_graphite", (xi + 0.155, 0.0, 0.0), (0.012, 0.80, 1.0), chamfer=0.004)           # front cover
        for yy in (-0.34, 0.34):
            for zz in (-0.44, 0.44):
                hex_bolt(iv, "eq_steel", (xi + 0.161, yy, zz), (1, 0, 0), 0.04, 0.02, washer=False)
        for i in range(3):
            box(iv, "eq_status_amber", (xi + 0.163, 0.22, -0.20 + 0.08 * i), (0.006, 0.05, 0.05))
        for i in range(7):                                                                             # cooling fins
            box(iv, "eq_metal_band", (xi, 0.51, -0.48 + 0.16 * i), (0.24, 0.02, 0.06))

    # --- pellet feed from the fuel bunker (-X side): feed housing, bunker port on top
    fd = P["er5_fuel_feed"] = Part("er5_fuel_feed")
    xf = -(ro + 0.13)
    box(fd, "eq_metal_panel", (xf, 0.02, 0), (0.30, 0.90, 0.95), chamfer=0.04 if lod == 0 else 0.0)
    if lod < 2:
        cyl(fd, "eq_metal_band", (xf, 0.47, 0), (xf, 0.85, 0), 0.12, small)
        lathe(fd, "eq_steel", [(0, 0), (0.20, 0), (0.20, 0.05), (0, 0.05)], small, R_Y, (xf, 0.85, 0))
    if lod == 0:
        for i in range(6):
            a = 2 * math.pi * i / 6
            hex_bolt(fd, "eq_steel", (xf + 0.15 * math.cos(a), 0.90, 0.15 * math.sin(a)), (0, 1, 0), 0.03, 0.02, washer=False)
        box(fd, "eq_metal_graphite", (xf - 0.155, 0.0, 0.0), (0.012, 0.62, 0.66), chamfer=0.004)            # hatch
        box(fd, "eq_status_amber", (xf - 0.163, 0.22, 0.20), (0.006, 0.08, 0.10))                           # sight window
        for zz in (-0.24, 0.24):
            cyl(fd, "eq_steel", (xf - 0.17, -0.33, zz - 0.06), (xf - 0.17, -0.33, zz + 0.06), 0.02, 6)      # hinges

    # --- plasma control unit (under the vessel, -Y)
    pc = P["er5_plasma_ctl"] = Part("er5_plasma_ctl")
    yb = -(ro + 0.13)
    box(pc, "eq_metal_panel", (0, yb, 0), (0.90, 0.26, 1.0), chamfer=0.03 if lod == 0 else 0.0)
    if lod == 0:
        box(pc, "eq_status_amber", (0.30, yb - 0.135, 0.30), (0.10, 0.006, 0.06))
        for xx in (-0.30, -0.15, 0.0):
            cyl(pc, "eq_metal_graphite", (xx, yb - 0.13, -0.25), (xx, yb - 0.19, -0.25), 0.04, 8)
        cyl(pc, "eq_metal_band", (0.45, yb + 0.05, 0.0), polz(ro + 0.04, -math.pi / 2 + 0.55, 0.0), 0.05, 8)   # conduit

    # --- plasma sensor ports on the shoulders
    sn = P["er5_plasma_sensors"] = Part("er5_plasma_sensors")
    if lod == 0:
        for a, zz in ((math.pi / 4, 0.9), (math.pi / 4, -0.9), (3 * math.pi / 4, 0.9), (3 * math.pi / 4, -0.9),
                      (5 * math.pi / 4, 0.0), (7 * math.pi / 4, 0.0)):
            p0, p1 = polz(ro + 0.03, a, zz), polz(ro + 0.17, a, zz)
            cyl(sn, "eq_metal_graphite", p0, p1, 0.07, small, chamfer=0.01 if lod == 0 else 0.0)
            if lod == 0:
                lathe(sn, "eq_insulator", [(0.05, 0), (0.04, 0.03), (0, 0.05)], 8, axis_to(polz(1, a, 0)), p1)

    # --- heat-pipe column from the vessel top to the pad, cooling pad PT1 (11 x 11 x 0.25 m)
    heat_column(P.setdefault("er5_heat_pipe_column", Part("er5_heat_pipe_column")), lod, seg, small, ro, d["pad_y0"])
    cooling_pad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["pad_half"])

    # --- emergency connection to the shared coolant loop: the only copper lines (column foot -> port block, aft top)
    ec = P["er5_emergency_coolant"] = Part("er5_emergency_coolant")
    if lod < 2:
        box(ec, "eq_metal_panel", (0, ro + 0.08, 1.15), (0.46, 0.18, 0.34), chamfer=0.02 if lod == 0 else 0.0)
        for s in (-1, 1):
            x = 0.12 * s
            cyl(ec, "eq_copper", (x, ro + 0.22, 0.45), (x, ro + 0.22, 1.02), 0.05, small)
            cyl(ec, "eq_copper", (x, ro + 0.17, 1.15), (x, ro + 0.40, 1.15), 0.06, small)
            if lod == 0:
                lathe(ec, "eq_steel", [(0, 0), (0.10, 0), (0.10, 0.04), (0, 0.04)], small, R_Y, (x, ro + 0.40, 1.15))
                cyl(ec, "eq_copper", (x, ro + 0.22, 1.02), (x, ro + 0.22, 1.15), 0.05, small)

    # --- mounting lugs to the engine bay frame on the lower shoulders (no floor stand)
    mt = P["er5_mounts"] = Part("er5_mounts")
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for zz in (-0.9, 0.9):
            Rz = rot_z(a)
            box(mt, "eq_metal_band", apply(Rz, (ro + 0.10, 0, 0))[:2] + (zz,), (0.26, 0.42, 0.46), Rz, chamfer=0.02 if lod == 0 else 0.0)
            if lod < 2:
                c0 = apply(Rz, (ro + 0.18, 0, 0))[:2] + (zz,)
                lathe(mt, "eq_steel", crect(0.05, 0.10, -0.16, 0.16, 0.01 if lod == 0 else 0.0), small, I3, c0)

    # --- close-inspection extras (asset LOD0): magnet feed cables from the plasma control unit, sensor cables
    if close():
        hz = P["er5_harness"] = Part("er5_harness")
        rr = ro + 0.16
        for k, a in enumerate(hoops):                                               # each hoop lead-in -> bottom unit
            zz = 0.12 + 0.06 * (k % 4)
            target = -math.pi / 2 + (0.18 if math.cos(a) > 0 else -0.18)
            span = (target - a) % (2 * math.pi)
            if span > math.pi:
                span -= 2 * math.pi
            harness(hz, [polz(ro + 0.28, a, 0.0)] + cyl_path(rr, a, a + span, zz, zz, 20) + [(0.3 * math.cos(target), yb + 0.13, zz)],
                    0.018, 2, ("eq_insulator", "eq_metal_graphite"), tie_every=5)
        for a, zz in ((math.pi / 4, 0.9), (3 * math.pi / 4, 0.9), (math.pi / 4, -0.9), (3 * math.pi / 4, -0.9)):
            harness(hz, cyl_path(rr + 0.04, a, a - math.pi * 0.62 if math.cos(a) > 0 else a + math.pi * 0.62, zz, zz * 0.4, 18),
                    0.012, 1, ("eq_metal_graphite",), tie_every=6)
    return P




def cr1_hits():
    d = CR1
    sr, br, ft = d["shell_r"], d["band_r"], d["flange_t"]
    sx, sz = sr + 0.32, 2.95
    H = [hcyl("cr1_flange", (0, 0, -0.24), (0, 0, 0), 0.62),                       # busbar coupling boss
         hcyl("cr1_flange", (0, 0, -0.08), (0, 0, ft), d["flange_r"]),
         hcyl("cr1_housing", (0, 0, ft), (0, 0, d["coil_z0"]), br),
         hcyl("cr1_coil_ring", (0, 0, d["coil_z0"]), (0, 0, d["coil_z1"]), 2.47),
         hcyl("engine_hot_emitter_annulus", (0, 0, d["pad_z0"]), (0, 0, d["pad_z1"] + 0.05), d["pad_r"]),
         hbox("cr1_service_module", ((sr + sx + 0.31) / 2, 0, sz), (sx + 0.31 - sr, 1.30, 2.10)),
         hbox("cr1_service_module", (sx + 0.50, 0, sz - 0.62), (0.38, 1.10, 0.34))]   # coolant ports
    for a, zz in ((math.pi * 3 / 4, 1.05), (-math.pi * 3 / 4, 1.05), (math.pi * 3 / 4, 3.85), (-math.pi * 3 / 4, 3.85)):
        H.append(hbox("cr1_mounts", polz(2.28, a, zz), (0.42, 0.46, 0.20), (0, 0, math.degrees(a))))
    return H


def er5_hits():
    d = ER5
    ro, zh, fz = d["r_out"], d["zh"], d["face_z"]
    xi, xf, yb = ro + 0.13, -(ro + 0.13), -(ro + 0.13)
    H = [hcyl("er5_vessel", (0, 0, -zh), (0, 0, zh), ro)]
    for k in range(8):                                                                 # magnet hoops on the outer wall
        a = math.pi / 8 + k * math.pi / 4
        H.append(hbox("er5_confinement", polz(ro + 0.05, a, 0), (0.18, 0.34, 2 * zh + 0.14), (0, 0, math.degrees(a))))
    H += [hcyl("er5_flange_aft", (0, 0, fz - 0.80), (0, 0, fz), d["flange_r"]),
          hcyl("er5_flange_fore", (0, 0, -fz), (0, 0, -fz + 0.80), d["flange_r"]),
          hbox("er5_inverter", (xi, 0, 0), (0.32, 1.0, 1.25))]
    for zs in (-1, 1):                                                                 # busbar runs to the flanges
        H.append(hbox("er5_inverter", (2.20, 0, zs * 1.40), (0.40, 0.64, 1.56)))
    H += [hbox("er5_fuel_feed", (xf, 0.02, 0), (0.32, 0.90, 0.95)),
          hcyl("er5_fuel_feed", (xf, 0.47, 0), (xf, 0.90, 0), 0.20),
          hbox("er5_plasma_ctl", (0, yb, 0), (0.90, 0.26, 1.0))]
    for a, zz in ((math.pi / 4, 0.9), (math.pi / 4, -0.9), (3 * math.pi / 4, 0.9), (3 * math.pi / 4, -0.9),
                  (5 * math.pi / 4, 0.0), (7 * math.pi / 4, 0.0)):
        H.append(hcyl("er5_plasma_sensors", polz(ro + 0.03, a, zz), polz(ro + 0.22, a, zz), 0.07))
    H += [hbox("er5_heat_pipe_column", (0, ro + 0.02, 0), (0.80, 0.16, 1.30)),
          hcyl("er5_heat_pipe_column", (0, ro, 0), (0, d["pad_y0"], 0), 0.65),
          hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.03, 0), (2 * d["pad_half"], d["pad_t"] + 0.06, 2 * d["pad_half"])),
          hbox("er5_emergency_coolant", (0, ro + 0.23, 0.82), (0.46, 0.46, 0.86))]
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for zz in (-0.9, 0.9):
            H.append(hbox("er5_mounts", polz(ro + 0.12, a, zz), (0.30, 0.42, 0.46), (0, 0, math.degrees(a))))
    return H



ER1_S = 2.34 / 5.0          # ER-1 passport envelope / ER-5 envelope

ASSETS = {"cobretty_cr1_vector": cr1_parts, "cobretty_er5_reactor": er5_parts,
          "cobretty_er1_reactor": scaled_family(er5_parts, ER5, ER1_S, 4.48, {"er5_": "er1_"})}
HITS = {"cobretty_cr1_vector": cr1_hits, "cobretty_er5_reactor": er5_hits,
        "cobretty_er1_reactor": scaled_hits(er5_hits, ER5, ER1_S, 4.48, {"er5_": "er1_"})}
MODEL_ASSETS = {"kb.drive.cr1-vector": "cobretty_cr1_vector", "kb.reactor.er5": "cobretty_er5_reactor",
                "kb.reactor.er1": "cobretty_er1_reactor"}
