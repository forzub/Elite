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





# ======================================================================== Cobretty units added 2026-11-10
# N1-AX axial drive, CL-180 coolant loop, RP-10 C-SiC radiator, FC-RCS field-coupled nozzle, SR-150 service reactor,
# PD-1 point-defence turret, EC-2 escape capsule. Cobretty school: smooth graphite drums, ring bands, flush panels,
# service modules as clean side boxes, copper only on emergency lines.
register_materials({
    "eq_cob_csic": ((0.13, 0.13, 0.14), (0.30, 0.30, 0.32), 40),     # C-SiC radiator face
    "eq_cob_lens": ((0.08, 0.12, 0.18), (0.90, 0.90, 0.95), 200),    # laser aperture / viewport glass
})


def _ch(lod, c):
    return c if lod == 0 else 0.0


def cob_drive_flange(f, lod, seg, fr=2.25, ft=0.35):
    """the Cobretty drive flange standard (same as the CR-1): mating face z = 0, 24 bolts on r 2.08, centring spigot,
    busbar boss with 3 terminals on r 0.32, data socket at 6 o'clock (y = -1.15)"""
    if lod == 0:
        lathe(f, "eq_metal_band", [(0, 0), (fr - 0.04, 0), (fr, 0.04), (fr, ft - 0.04), (fr - 0.04, ft), (1.9, ft),
                                   (1.9, ft + 0.12), (0, ft + 0.12)], seg)
        for i in range(24):
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.08 * math.cos(a), 2.08 * math.sin(a), ft), (0, 0, 1))
        lathe(f, "eq_metal_graphite", crect(1.55, 1.75, -0.08, 0.0, 0.015), seg)
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
        lathe(f, "eq_metal_band", [(0, 0), (fr, 0), (fr, ft), (1.9, ft), (1.9, ft + 0.12), (0, ft + 0.12)], seg)
        lathe(f, "eq_metal_graphite", [(0, -0.24), (0.62, -0.24), (0.62, 0), (0, 0)], seg)


# ---------------------------------------------------------------- Cobretty N1-AX axial main drive
# The simple sibling of the CR-1 (no thrust vectoring): a slim core drum carrying a stack of four open solenoid coil
# rings that widen towards the pad (a stepped "trumpet" silhouette instead of the CR-1 closed shell + coil crown), the
# field control unit and the backup heat exchanger as twin side boxes in front of the stack. Same flange standard.
N1 = dict(core_r=1.55, rings=((1.95, 1.95), (3.15, 2.10), (4.35, 2.25), (5.55, 2.40)), ring_hw=0.33,
          pad_z0=6.15, pad_z1=6.50, pad_r=2.5, box_z=1.07)


def n1_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = N1
    cr, hw = d["core_r"], d["ring_hw"]
    P = {}
    cob_drive_flange(P.setdefault("n1_flange", Part("n1_flange")), lod, seg)

    # --- core drum: neck taper from the flange disc, slim drum through the coil stack, flared pad carrier collar
    h = P["n1_housing"] = Part("n1_housing")
    zt = d["rings"][-1][0] + hw
    lathe(h, "eq_metal_graphite", [(1.9, 0.47), (1.9, 0.55), (cr, 0.90), (cr, zt), (2.30, d["pad_z0"])], seg)
    if lod < 2:
        lathe(h, "eq_metal_band", crect(cr - 0.01, cr + 0.05, 0.92, 1.02, _ch(lod, 0.012)), seg)
        lathe(h, "eq_metal_band", crect(cr - 0.01, cr + 0.05, 1.50, 1.60, _ch(lod, 0.012)), seg)
    if lod == 0:
        for k in range(len(d["rings"]) - 1):                                   # one band in every stack gap
            zg = (d["rings"][k][0] + d["rings"][k + 1][0]) / 2
            lathe(h, "eq_metal_band", crect(cr - 0.01, cr + 0.03, zg - 0.04, zg + 0.04, 0.01), seg)
        for i in range(12):                                                    # collar stiffeners
            a = 2 * math.pi * (i + 0.5) / 12
            Rz = rot_z(a)
            box(h, "eq_metal_band", apply(Rz, (1.86, 0, 0))[:2] + (6.0,), (0.50, 0.05, 0.16), mat_mul(Rz, rot_y(-0.34)), chamfer=0.01)

    # --- coil stack (coil SRU): four open solenoid rings, exposed winding between two band rims, tie rods in the gaps
    c = P["n1_coil_stack"] = Part("n1_coil_stack")
    for zc, ro in d["rings"]:
        z0, z1 = zc - hw, zc + hw
        lathe(c, "eq_metal_graphite", [(cr, z0), (ro - 0.07, z0)], seg)
        lathe(c, "eq_metal_graphite", [(ro - 0.07, z1), (cr, z1)], seg)
        lathe(c, "eq_metal_band", [(ro - 0.07, z0), (ro + 0.02, z0), (ro + 0.02, zc - 0.22), (ro - 0.005, zc - 0.22)], seg)
        lathe(c, "eq_metal_band", [(ro - 0.005, zc + 0.22), (ro + 0.02, zc + 0.22), (ro + 0.02, z1), (ro - 0.07, z1)], seg)
        lathe(c, "eq_field_coil", [(ro - 0.005, zc - 0.22), (ro - 0.005, zc + 0.22)], seg)
        if lod == 0:
            for i in range(8):                                                 # winding retaining straps
                a = 2 * math.pi * (i + 0.5) / 8
                Rz = rot_z(a)
                box(c, "eq_steel", apply(Rz, (ro + 0.005, 0, 0))[:2] + (zc,), (0.03, 0.07, 0.46), Rz, chamfer=0.006)
            g = polz(ro - 0.30, math.radians(170), z0)                         # cable gland on the front wall
            cyl(c, "eq_insulator", g, (g[0], g[1], z0 - 0.08), 0.04, 8)
    if lod < 2:
        for k in range(len(d["rings"]) - 1):
            za, zb = d["rings"][k][0] + hw, d["rings"][k + 1][0] - hw
            for i in range(6):
                a = math.pi / 6 + i * math.pi / 3
                cyl(c, "eq_metal_band", polz(1.80, a, za), polz(1.80, a, zb), 0.05, small // 2 + 2, caps=False)

    # --- pad = engine_hot_emitter_annulus: two concentric courses of 12 ceramic tiles, centre ring, clamp rim
    p = P["engine_hot_emitter_annulus"] = Part("engine_hot_emitter_annulus")
    pz0, pz1, pr = d["pad_z0"], d["pad_z1"], d["pad_r"]
    lathe(p, "eq_metal_band", [(0, pz0), (2.38, pz0), (2.38, pz0 + 0.05), (0, pz0 + 0.05)], seg)
    if lod < 2:
        g = 0.01
        for i in range(12):
            for (r0, r1), off in (((0.55, 1.44), 0.0), ((1.50, 2.38), 0.5)):
                a0 = 2 * math.pi * (i + off) / 12 + g
                a1 = 2 * math.pi * (i + 1 + off) / 12 - g
                lathe(p, "eq_pad_ceramic", crect(r0, r1, pz0 + 0.05, pz1 - 0.04), max(2, seg // 12), I3, (0, 0, 0), a0, a1, cap=True)
        lathe(p, "eq_metal_band", crect(1.44, 1.50, pz0 + 0.05, pz1 - 0.02), seg)                    # course ring
        lathe(p, "eq_steel", crect(2.38, 2.50, pz0, pz1, _ch(lod, 0.015)), seg)                      # clamp rim
        lathe(p, "eq_metal_band", [(0, pz0 + 0.05), (0.55, pz0 + 0.05), (0.55, pz1 - 0.01), (0.47, pz1 + 0.02), (0, pz1 + 0.02)], seg)
        if lod == 0:
            hex_bolt(p, "eq_steel", (0, 0, pz1 + 0.02), (0, 0, 1), 0.14, 0.07)
            for i in range(24):
                a = 2 * math.pi * (i + 0.25) / 24
                hex_bolt(p, "eq_steel", polz(1.47, a, pz1 - 0.02), (0, 0, 1), 0.035, 0.018, washer=False)
                Rz = rot_z(a)
                box(p, "eq_steel", apply(Rz, (2.47, 0, 0))[:2] + (pz1 + 0.02,), (0.12, 0.12, 0.04), Rz, chamfer=0.008)
            for i in range(6):
                hex_bolt(p, "eq_steel", polz(0.40, 2 * math.pi * i / 6, pz1 + 0.02), (0, 0, 1), 0.035, 0.018, washer=False)
    else:
        lathe(p, "eq_pad_ceramic", [(0, pz0 + 0.05), (pr, pz0 + 0.05), (pr, pz1), (0, pz1)], seg)

    # --- twin side boxes in front of the stack: field control (-X) and backup heat exchanger (+X, copper emergency ports)
    zb = d["box_z"]
    for name, sx in (("n1_field_ctl", -1), ("n1_backup_hx", 1)):
        b = P[name] = Part(name)
        box(b, "eq_metal_panel", (sx * 1.97, 0, zb), (0.80, 1.10, 0.86), chamfer=_ch(lod, 0.04))
        if lod == 2:
            continue
        if sx < 0:
            cyl(b, "eq_metal_band", (-1.64, 0.0, zb + 0.43), (-1.64, 0.0, 5.80), 0.06, small)       # conduit along the core
            if lod == 0:
                box(b, "eq_metal_graphite", (-2.375, 0, zb), (0.012, 0.86, 0.66), chamfer=0.004)
                for yy in (-0.36, 0.36):
                    for zz in (zb - 0.27, zb + 0.27):
                        hex_bolt(b, "eq_steel", (-2.381, yy, zz), (-1, 0, 0), 0.04, 0.02, washer=False)
                for i in range(3):
                    box(b, "eq_status_amber", (-2.383, 0.25, zb - 0.12 + 0.08 * i), (0.006, 0.05, 0.05))
                for yy in (-0.15, 0.05):
                    cyl(b, "eq_metal_graphite", (-2.37, yy - 0.25, zb + 0.18), (-2.43, yy - 0.25, zb + 0.18), 0.035, 8)
        else:
            for yy in (-0.28, 0.28):
                cyl(b, "eq_copper", (2.37, yy, zb), (2.46, yy, zb), 0.08, small)
                lathe(b, "eq_steel", [(0, 0), (0.12, 0), (0.12, 0.04), (0, 0.04)], small, axis_to((1, 0, 0)), (2.46, yy, zb))
            for i in range(6):                                                                     # fin array on top
                box(b, "eq_metal_band", (1.97, 0.58, zb - 0.30 + 0.12 * i), (0.66, 0.06, 0.03))
            if lod == 0:
                for yy in (-0.28, 0.28):
                    for i in range(6):
                        a = 2 * math.pi * i / 6
                        hex_bolt(b, "eq_steel", (2.50, yy + 0.09 * math.cos(a), zb + 0.09 * math.sin(a)), (1, 0, 0), 0.025, 0.015, washer=False)
                box(b, "eq_status_amber", (2.375, 0.0, zb + 0.30), (0.006, 0.10, 0.05))

    # --- sensors: field / temperature heads on each ring front wall, a thrust strain gauge at the neck (top)
    sn = P["n1_sensors"] = Part("n1_sensors")
    if lod < 2:
        for zc, ro in d["rings"]:
            q = polz(ro - 0.25, math.pi / 2, zc - hw)
            cyl(sn, "eq_metal_graphite", q, (q[0], q[1], q[2] - 0.10), 0.06, small, chamfer=_ch(lod, 0.01))
        box(sn, "eq_metal_graphite", (0, 1.62, 1.26), (0.30, 0.10, 0.36), chamfer=_ch(lod, 0.015))
        if lod == 0:
            for zc, ro in d["rings"]:
                q = polz(ro - 0.25, math.pi / 2, zc - hw - 0.10)
                lathe(sn, "eq_insulator", [(0.04, 0), (0.03, -0.025), (0, -0.035)][::-1], 8, I3, q)

    # --- four lugs under the core (at the neck and in the second stack gap)
    m = P["n1_mounts"] = Part("n1_mounts")
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for zz in (zb, (d["rings"][1][0] + d["rings"][2][0]) / 2):
            Rz = rot_z(a)
            box(m, "eq_metal_band", apply(Rz, (1.90, 0, 0))[:2] + (zz,), (0.70, 0.40, 0.40), Rz, chamfer=_ch(lod, 0.02))
            if lod < 2:
                lathe(m, "eq_steel", crect(0.06, 0.11, -0.22, 0.22), small, I3, apply(Rz, (2.05, 0, 0))[:2] + (zz,))

    # --- close inspection: harness trunk along the conduit with drops to the ring glands, copper hx lines, data cable
    if close():
        hz = P["n1_harness"] = Part("n1_harness")
        for zc, ro in d["rings"]:
            g = polz(ro - 0.30, math.radians(170), zc - hw - 0.08)
            t0 = (-1.64, 0.10, zc - hw - 0.30)
            harness(hz, bezier(t0, (-1.70, 0.16, zc - hw - 0.20), (g[0] - 0.02, g[1], g[2] - 0.10), g, 8), 0.016, 2,
                    ("eq_insulator", "eq_metal_graphite"), tie_every=4)
        harness(hz, [(-1.64, 0.10, zb + 0.43 + 0.40 * i) for i in range(12)], 0.022, 3, ("eq_insulator", "eq_metal_graphite"), tie_every=3)
        for yy in (-0.28, 0.28):
            pipe_run(hz, "eq_copper", [(2.50, yy, zb), (2.56, yy, zb), (2.56, yy, 0.70), (2.10, yy, 0.62)], 0.06)
            box(hz, "eq_metal_graphite", (2.05, yy, 0.60), (0.18, 0.18, 0.14), chamfer=0.02)
        harness(hz, bezier((-1.97, -0.55, zb - 0.2), (-1.9, -1.2, zb - 0.3), polz(2.0, -math.pi / 2 - 0.15, 0.75),
                           polz(1.95, -math.pi / 2, 0.52), 14), 0.03, 4, ("eq_insulator", "eq_metal_graphite"))
    return P


def n1_hits():
    d = N1
    cr, hw, zb = d["core_r"], d["ring_hw"], d["box_z"]
    H = [hcyl("n1_flange", (0, 0, -0.24), (0, 0, 0), 0.62),
         hcyl("n1_flange", (0, 0, -0.08), (0, 0, 0.35), 2.25),
         hcyl("n1_housing", (0, 0, 0.35), (0, 0, 0.90), 1.85),
         hcyl("n1_housing", (0, 0, 0.90), (0, 0, d["pad_z0"]), cr),
         hcyl("n1_housing", (0, 0, d["rings"][-1][0] + hw), (0, 0, d["pad_z0"]), 2.0),
         hcyl("engine_hot_emitter_annulus", (0, 0, d["pad_z0"]), (0, 0, d["pad_z1"] + 0.03), d["pad_r"]),
         hbox("n1_field_ctl", (-1.97, 0, zb), (0.80, 1.10, 0.86)),
         hbox("n1_backup_hx", (1.97, 0, zb), (0.80, 1.10, 0.86)),
         hbox("n1_backup_hx", (2.45, 0, zb), (0.18, 0.80, 0.24)),
         hbox("n1_sensors", (0, 1.62, 1.26), (0.30, 0.10, 0.36))]
    for zc, ro in d["rings"]:
        H.append(hcyl("n1_coil_stack", (0, 0, zc - hw), (0, 0, zc + hw), ro + 0.02))
        H.append(hcyl("n1_sensors", polz(ro - 0.25, math.pi / 2, zc - hw - 0.10), polz(ro - 0.25, math.pi / 2, zc - hw), 0.06))
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for zz in (zb, (d["rings"][1][0] + d["rings"][2][0]) / 2):
            H.append(hbox("n1_mounts", polz(1.90, a, zz), (0.70, 0.40, 0.44), (0, 0, math.degrees(a))))
    return H



# ---------------------------------------------------------------- Cobretty CL-180 coolant loop + pump (kb.cool.loop200)
# Internal unit on a skid: long axis X (envelope 2.466 x 1.233 x 1.233), skid underside (mounting face) at y = 0.
# SRUs (coolant.loop): pump (canned motor drum + volute, -X), pipes (suction line on the pump axis, delivery line over
# the top), valves (distribution block with three actuator drums and four outlet ports, +X), fluid (accumulator drum).
def cl180_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    py, pz = 0.48, -0.22                                   # pump axis
    ax = axis_to((1, 0, 0))

    sk = P["cl180_skid"] = Part("cl180_skid")
    box(sk, "eq_metal_band", (0, 0.04, 0), (2.44, 0.08, 1.20), chamfer=_ch(lod, 0.02))
    if lod < 2:
        for zz in (-0.50, 0.50):
            box(sk, "eq_metal_graphite", (0, 0.105, zz), (2.30, 0.05, 0.12))
        for xx in (-0.95, -0.50):                                       # pump saddles
            box(sk, "eq_metal_band", (xx, 0.14, pz), (0.12, 0.14, 0.46), chamfer=_ch(lod, 0.01))
    if lod == 0:
        for zz in (-0.50, 0.50):
            for i in range(6):
                hex_bolt(sk, "eq_steel", (-1.05 + 0.42 * i, 0.13, zz), (0, 1, 0), 0.05, 0.025, washer=False)
        for sx in (-1, 1):
            for sz in (-1, 1):
                lathe(sk, "eq_steel", crect(0.03, 0.06, -0.02, 0.02), 8, axis_to((0, 0, 1)), (sx * 1.12, 0.12, sz * 0.58))

    pm = P["cl180_pump"] = Part("cl180_pump")
    prof = [(0, 0), (0.28, 0), (0.32, 0.04), (0.32, 0.64), (0.38, 0.68), (0.38, 0.90), (0.32, 0.96), (0, 0.96)]
    lathe(pm, "eq_metal_graphite", prof, seg, ax, (-1.18, py, pz))
    if lod < 2:
        for z0 in (0.18, 0.42):
            lathe(pm, "eq_metal_band", crect(0.315, 0.345, z0, z0 + 0.06, _ch(lod, 0.008)), seg, ax, (-1.18, py, pz))
        cyl(pm, "eq_metal_graphite", (-0.34, py + 0.30, pz), (-0.34, 0.98, pz), 0.10, small)          # delivery nozzle
        box(pm, "eq_metal_panel", (-0.82, py + 0.36, pz), (0.30, 0.10, 0.26), chamfer=_ch(lod, 0.015))   # terminal box
    if lod == 0:
        for i in range(12):
            a = 2 * math.pi * (i + 0.5) / 12
            hex_bolt(pm, "eq_steel", (-0.22, py + 0.33 * math.sin(a), pz + 0.33 * math.cos(a)), (1, 0, 0), 0.03, 0.018, washer=False)
        box(pm, "eq_status_amber", (-0.82, py + 0.413, pz + 0.07), (0.05, 0.006, 0.04))
        cyl(pm, "eq_insulator", (-0.90, py + 0.41, pz - 0.05), (-0.90, py + 0.46, pz - 0.05), 0.025, 8)

    pp = P["cl180_pipes"] = Part("cl180_pipes")
    r = 0.075
    cyl(pp, "eq_metal_graphite", (-0.22, py, pz), (0.45, py, pz), r, small, caps=False)                  # suction line
    if lod < 2:
        tube(pp, "eq_metal_graphite", [(-0.34, 0.98, pz), (-0.34, 1.06, pz), (-0.26, 1.10, pz), (0.62, 1.10, pz),
                                        (0.70, 1.06, pz), (0.70, 0.66, pz)], r, small)                 # delivery line
        tube(pp, "eq_metal_graphite", [(0.05, 0.14, 0.30), (0.05, 0.10, 0.10), (0.05, 0.12, -0.02), (0.05, py - 0.06, pz + 0.05)], 0.035, 8)
        for x in (-0.05, 0.30):
            lathe(pp, "eq_metal_band", crect(r, r + 0.03, -0.035, 0.035, _ch(lod, 0.008)), small, ax, (x, py, pz))
        lathe(pp, "eq_metal_band", crect(r, r + 0.03, -0.035, 0.035, _ch(lod, 0.008)), small, ax, (0.15, 1.10, pz))
    else:
        cyl(pp, "eq_metal_graphite", (-0.34, 1.10, pz), (0.70, 1.10, pz), r, small)
    if lod == 0:
        for x in (-0.05, 0.30):
            for i in range(6):
                a = 2 * math.pi * i / 6
                hex_bolt(pp, "eq_steel", (x + 0.04, py + 0.09 * math.sin(a), pz + 0.09 * math.cos(a)), (1, 0, 0), 0.022, 0.012, washer=False)

    vb = P["cl180_valves"] = Part("cl180_valves")
    box(vb, "eq_metal_panel", (0.83, 0.37, 0), (0.76, 0.58, 1.06), chamfer=_ch(lod, 0.03))
    for x, z in ((0.70, 0.12), (0.70, 0.40), (0.92, -0.22)):                                          # actuator drums
        ylathe(vb, "eq_metal_graphite", [(0, 0.66), (0.11, 0.66), (0.11, 0.90), (0.09, 0.94), (0, 0.94)], small, t=(x, 0, z))
        if lod < 2:
            ylathe(vb, "eq_metal_band", crect(0.105, 0.125, 0.74, 0.79), small, t=(x, 0, z))
        if lod == 0:
            ylathe(vb, "eq_steel", crect(0.07, 0.085, 0.965, 0.98), 12, t=(x, 0, z))                  # handwheel
            cyl(vb, "eq_steel", (x - 0.07, 0.972, z), (x + 0.07, 0.972, z), 0.008, 6, caps=False)
            cyl(vb, "eq_steel", (x, 0.94, z), (x, 0.975, z), 0.015, 6)
    if lod < 2:
        for z in (-0.40, -0.13, 0.14, 0.41) if lod < 2 else ():                                       # outlet ports
            cyl(vb, "eq_metal_graphite", (1.10, 0.66, z), (1.10, 0.84, z), 0.06, small)
            ylathe(vb, "eq_metal_band", [(0.06, 0.80), (0.10, 0.80), (0.10, 0.85), (0.0, 0.85)], small, t=(1.10, 0, z))
        for x in (0.66, 0.98):                                                                       # emergency ports (copper)
            cyl(vb, "eq_copper", (x, 0.30, 0.53), (x, 0.30, 0.60), 0.045, small)
            lathe(vb, "eq_steel", [(0, 0), (0.065, 0), (0.065, 0.02), (0, 0.02)], small, I3, (x, 0.30, 0.60))
    if lod == 0:
        box(vb, "eq_metal_graphite", (1.212, 0.37, 0), (0.012, 0.44, 0.86), chamfer=0.004)            # service cover
        for yy in (0.20, 0.54):
            for zz in (-0.38, 0.38):
                hex_bolt(vb, "eq_steel", (1.218, yy, zz), (1, 0, 0), 0.035, 0.018, washer=False)
        for i in range(4):
            box(vb, "eq_status_amber", (1.219, 0.56, -0.15 + 0.10 * i), (0.006, 0.04, 0.04))
        for z in (-0.40, -0.13, 0.14, 0.41):
            for i in range(4):
                a = 2 * math.pi * (i + 0.5) / 4
                hex_bolt(vb, "eq_steel", (1.10 + 0.08 * math.cos(a), 0.85, z + 0.08 * math.sin(a)), (0, 1, 0), 0.022, 0.012, washer=False)

    fl = P["cl180_fluid"] = Part("cl180_fluid")
    tx, tz, tr = 0.05, 0.30, 0.22
    prof = [(0, 0.08), (tr, 0.08), (tr, 0.98), (tr - 0.05, 1.12), (tr - 0.12, 1.18), (0, 1.20)]
    ylathe(fl, "eq_metal_graphite", prof, seg if lod < 2 else small, t=(tx, 0, tz), soft=40)
    if lod < 2:
        for y in (0.30, 0.80):
            ylathe(fl, "eq_metal_band", crect(tr - 0.005, tr + 0.025, y - 0.03, y + 0.03, _ch(lod, 0.008)), seg, t=(tx, 0, tz))
    if lod == 0:
        box(fl, "eq_cob_lens", (tx - 0.05, 0.55, tz - tr - 0.004), (0.05, 0.36, 0.012))               # level sight glass
        box(fl, "eq_metal_band", (tx - 0.05, 0.55, tz - tr - 0.002), (0.09, 0.42, 0.006))
        cyl(fl, "eq_steel", (tx, 1.19, tz), (tx, 1.225, tz), 0.03, 8)                                 # fill / vent cap
    if close():                                         # motor power cable, actuator control loom, level sensor lead
        hz = P["cl180_harness"] = Part("cl180_harness")
        harness(hz, bezier((-0.90, py + 0.46, pz - 0.05), (-0.90, py + 0.60, pz - 0.05), (-1.10, 0.40, -0.45), (-1.15, 0.14, -0.50), 10),
                0.022, 3, ("eq_insulator", "eq_metal_graphite"), tie_every=4)
        loom = [(1.08, 0.70, 0.52), (0.95, 0.70, 0.40), (0.70, 0.70, 0.27), (0.70, 0.68, 0.27)]
        harness(hz, bezier(*loom, 8), 0.012, 2, ("eq_insulator",), tie_every=3)
        harness(hz, bezier((1.10, 0.68, 0.30), (1.05, 0.70, 0.10), (0.95, 0.70, -0.05), (0.92, 0.68, -0.10), 8), 0.012, 2, ("eq_insulator",), tie_every=3)
        tube(hz, "eq_metal_graphite", sag((tx - 0.05, 0.75, tz - tr - 0.01), (0.46, 0.62, 0.05), 0.08, 8), 0.008, 6)
    return P


def cl180_hits():
    py, pz = 0.48, -0.22
    return [hbox("cl180_skid", (0, 0.065, 0), (2.44, 0.13, 1.20)),
            hcyl("cl180_pump", (-1.18, py, pz), (-0.54, py, pz), 0.32),
            hcyl("cl180_pump", (-0.54, py, pz), (-0.22, py, pz), 0.38),
            hcyl("cl180_pipes", (-0.22, py, pz), (0.45, py, pz), 0.08),
            hbox("cl180_pipes", (0.18, 1.06, pz), (1.20, 0.16, 0.16)),
            hbox("cl180_valves", (0.83, 0.37, 0), (0.76, 0.58, 1.06)),
            hbox("cl180_valves", (0.83, 0.78, 0.05), (0.62, 0.30, 1.0)),
            hcyl("cl180_fluid", (0.05, 0.08, 0.30), (0.05, 1.20, 0.30), 0.22)]


# ---------------------------------------------------------------- Cobretty RP-10 C-SiC radiator panel set (kb.rad.cs-panel)
# Hull-skin unit, stowed: the radiating face (+Y) covers the whole 2.24 x 4.47 m envelope (10 m2), 0.15 m deep.
# Under the -Z edge of the panel: the coolant manifold header and two hinge drives; four stowage latches under the panel.
RP = dict(w=2.24, l=4.47, y0=0.085, y1=0.125)


def rp10_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = RP
    hw, hl, y0, y1 = d["w"] / 2, d["l"] / 2, d["y0"], d["y1"]
    P = {}
    pn = P["rp10_panel"] = Part("rp10_panel")
    box(pn, "eq_cob_csic", (0, (y0 + y1) / 2, 0), (d["w"] - 0.12, y1 - y0, d["l"] - 0.12))
    fw = 0.06
    for s in (-1, 1):                                                      # graphite frame, flush with ribbed face
        box(pn, "eq_metal_graphite", (s * (hw - fw / 2), (y0 + y1) / 2 + 0.005, 0), (fw, y1 - y0 + 0.01, d["l"]), chamfer=_ch(lod, 0.012))
        box(pn, "eq_metal_graphite", (0, (y0 + y1) / 2 + 0.005, s * (hl - fw / 2)), (d["w"] - 2 * fw, y1 - y0 + 0.01, fw), chamfer=_ch(lod, 0.012))
    if lod < 2:
        for i in range(1, 8):                                              # heat-pipe ribs
            x = -hw + fw + i * (d["w"] - 2 * fw) / 8
            box(pn, "eq_metal_band", (x, y1 + 0.006, 0), (0.03, 0.012, d["l"] - 2 * fw))
        box(pn, "eq_metal_band", (0, y1 + 0.006, 0), (d["w"] - 2 * fw, 0.012, 0.05))
    if lod == 0:
        for s in (-1, 1):
            for i in range(14):
                z = -hl + 0.20 + i * (d["l"] - 0.40) / 13
                hex_bolt(pn, "eq_steel", (s * (hw - fw / 2), y1 + 0.01, z), (0, 1, 0), 0.03, 0.012, washer=False)
            for i in range(7):
                x = -hw + 0.25 + i * (d["w"] - 0.5) / 6
                hex_bolt(pn, "eq_steel", (x, y1 + 0.01, s * (hl - fw / 2)), (0, 1, 0), 0.03, 0.012, washer=False)
        for i in range(1, 8):
            x = -hw + fw + i * (d["w"] - 2 * fw) / 8
            for z in (-1.4, 0.0, 1.4):
                box(pn, "eq_metal_graphite", (x, y1 + 0.014, z), (0.07, 0.008, 0.06))             # rib clips
        box(pn, "eq_metal_graphite", (0, y0 - 0.006, 0), (d["w"] - 0.3, 0.012, d["l"] - 0.6))     # stiffener sheet

    mf = P["rp10_manifold"] = Part("rp10_manifold")
    zm, ym, rm = -hl + 0.12, 0.042, 0.042
    cyl(mf, "eq_metal_graphite", (-0.92, ym, zm), (0.92, ym, zm), rm, small)
    if lod < 2:
        for x in (-0.9, 0.9):                                              # ports through the mounting face
            cyl(mf, "eq_metal_band", (x, 0.0, zm + 0.10), (x, ym, zm + 0.10), 0.035, small)
            box(mf, "eq_metal_band", (x, ym, zm + 0.05), (0.07, 0.07, 0.10))
        for i in range(8):                                                 # feeders into the panel underside
            x = -0.84 + i * 0.24
            cyl(mf, "eq_metal_band", (x, ym, zm), (x, y0, zm + 0.06), 0.015, 6, caps=False)
    if lod == 0:
        for x in (-0.6, -0.2, 0.2, 0.6):
            lathe(mf, "eq_steel", crect(rm, rm + 0.008, -0.012, 0.012), 12, axis_to((1, 0, 0)), (x, ym, zm))

    hg = P["rp10_hinge"] = Part("rp10_hinge")
    for s in (-1, 1):
        x0 = s * 0.96
        x1 = s * (hw - 0.02)
        cyl(hg, "eq_metal_graphite", (x0, ym, zm), (x1, ym, zm), 0.042, small)                      # deploy drive drum
        box(hg, "eq_metal_band", ((x0 + x1) / 2, 0.012, zm), (abs(x1 - x0), 0.024, 0.12))           # base bracket
        if lod < 2:
            lathe(hg, "eq_metal_band", crect(0.042, 0.05, -0.015, 0.015), small, axis_to((1, 0, 0)), ((x0 + x1) / 2, ym, zm))
    if lod < 2:
        for x in (-0.50, 0.0, 0.50):                                       # hinge knuckles
            box(hg, "eq_metal_band", (x, 0.05, zm + 0.02), (0.10, 0.07, 0.10))
            cyl(hg, "eq_steel", (x - 0.06, 0.05, zm + 0.02), (x + 0.06, 0.05, zm + 0.02), 0.022, 6)
    if close():
        for s in (-1, 1):
            harness(hg, bezier((s * 1.0, 0.02, zm + 0.04), (s * 0.9, 0.01, zm + 0.20), (s * 0.7, 0.01, zm + 0.25),
                               (s * 0.6, 0.0, zm + 0.30), 6), 0.008, 2, ("eq_insulator",), tie_every=3)
    if lod == 0:
        for s in (-1, 1):
            for x in (s * 0.98, s * 1.08):
                hex_bolt(hg, "eq_steel", (x, 0.024, zm + 0.045), (0, 1, 0), 0.02, 0.01, washer=False)

    lt = P["rp10_latches"] = Part("rp10_latches")
    for sx in (-1, 1):
        for zz in (-0.6, 1.6):
            box(lt, "eq_metal_band", (sx * 0.85, y0 / 2, zz), (0.16, y0, 0.20), chamfer=_ch(lod, 0.01))
            if lod == 0:
                cyl(lt, "eq_copper", (sx * 0.85 - 0.08, 0.045, zz), (sx * 0.85 - 0.10, 0.045, zz), 0.018, 8)   # pyro release
    return P


def rp10_hits():
    d = RP
    hw, hl = d["w"] / 2, d["l"] / 2
    zm = -hl + 0.12
    H = [hbox("rp10_panel", (0, (d["y0"] + d["y1"]) / 2 + 0.006, 0), (d["w"], d["y1"] - d["y0"] + 0.03, d["l"])),
         hcyl("rp10_manifold", (-0.92, 0.042, zm), (0.92, 0.042, zm), 0.042),
         hcyl("rp10_hinge", (-hw + 0.02, 0.042, zm), (-0.96, 0.042, zm), 0.045),
         hcyl("rp10_hinge", (0.96, 0.042, zm), (hw - 0.02, 0.042, zm), 0.045)]
    for sx in (-1, 1):
        for zz in (-0.6, 1.6):
            H.append(hbox("rp10_latches", (sx * 0.85, d["y0"] / 2, zz), (0.16, d["y0"], 0.20)))
    return H


# ---------------------------------------------------------------- Cobretty FC-RCS field-coupled RCS nozzle (kb.rcs.fc-nozzle)
# Hull-skin unit, firing +Y, mounting face y = 0, 0.737 m cube. No propellant: a small field emitter - a coil torus
# round a dished ceramic heat emitter, the pulse driver as a clean side box. Round 24-bolt Cobretty flange.
def fcrcs_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    b = P["fcrcs_base"] = Part("fcrcs_base")
    ylathe(b, "eq_metal_band", [(0, 0), (0.355, 0), (0.355, 0.035), (0.33, 0.05), (0.21, 0.05)], seg)
    ylathe(b, "eq_metal_graphite", [(0.21, 0.05), (0.21, 0.40)], seg)                                 # body drum
    if lod == 0:
        for i in range(24):
            hex_bolt(b, "eq_steel", polar(0.30, 2 * math.pi * (i + 0.5) / 24, 0.042), (0, 1, 0), 0.022, 0.012, washer=False)
        ylathe(b, "eq_metal_band", crect(0.205, 0.225, 0.20, 0.23, 0.005), seg)

    dv = P["fcrcs_driver"] = Part("fcrcs_driver")
    box(dv, "eq_metal_panel", (0.27, 0.22, 0), (0.15, 0.30, 0.30), chamfer=_ch(lod, 0.02))
    if lod < 2:
        cyl(dv, "eq_metal_band", (0.27, 0.37, 0.08), (0.27, 0.44, 0.08), 0.025, 8)                   # lead to the coil
    if lod == 0:
        box(dv, "eq_metal_graphite", (0.347, 0.22, 0), (0.006, 0.24, 0.24), chamfer=0.003)
        for yy in (0.12, 0.32):
            for zz in (-0.10, 0.10):
                hex_bolt(dv, "eq_steel", (0.351, yy, zz), (1, 0, 0), 0.016, 0.008, washer=False)
        box(dv, "eq_status_amber", (0.351, 0.30, -0.05), (0.004, 0.025, 0.025))
        cyl(dv, "eq_insulator", (0.27, 0.07, 0.15), (0.27, 0.07, 0.19), 0.02, 8)                      # power gland
    if close():
        harness(dv, bezier((0.27, 0.07, 0.19), (0.27, 0.06, 0.26), (0.24, 0.055, 0.30), (0.20, 0.055, 0.30), 5), 0.008, 2, ("eq_insulator",), tie_every=3)

    co = P["fcrcs_coil"] = Part("fcrcs_coil")
    ylathe(co, "eq_metal_graphite", [(0.21, 0.40), (0.335, 0.40)], seg)
    ylathe(co, "eq_metal_band", [(0.335, 0.40), (0.365, 0.40), (0.365, 0.44), (0.345, 0.44)], seg)
    ylathe(co, "eq_field_coil", [(0.345, 0.44), (0.345, 0.54)], seg)
    ylathe(co, "eq_metal_band", [(0.345, 0.54), (0.365, 0.54), (0.365, 0.58), (0.30, 0.58)], seg)
    if lod == 0:
        for i in range(6):
            a = 2 * math.pi * (i + 0.5) / 6
            box(co, "eq_steel", polar(0.348, a, 0.49), (0.012, 0.10, 0.03), rot_y(-a), chamfer=0.003)

    em = P["fcrcs_emitter"] = Part("fcrcs_emitter")
    ylathe(em, "eq_steel", crect(0.27, 0.30, 0.58, 0.68, _ch(lod, 0.006)), seg)                       # clamp rim
    ylathe(em, "eq_pad_ceramic", [(0.27, 0.665), (0.08, 0.63)], seg)                                  # dished emitter
    ylathe(em, "eq_metal_band", [(0.08, 0.62), (0.08, 0.66), (0.05, 0.68), (0, 0.68)], small)         # centre hub
    if lod == 0:
        for i in range(8):
            a = 2 * math.pi * i / 8
            box(em, "eq_steel", polar(0.175, a, 0.652), (0.19, 0.008, 0.012), rot_y(-a))               # sector seams
            hex_bolt(em, "eq_steel", polar(0.285, a + 0.2, 0.68), (0, 1, 0), 0.018, 0.008, washer=False)
        hex_bolt(em, "eq_steel", (0, 0.68, 0), (0, 1, 0), 0.04, 0.02)
    return P


def fcrcs_hits():
    return [hcyl("fcrcs_base", (0, 0, 0), (0, 0.05, 0), 0.355),
            hcyl("fcrcs_base", (0, 0.05, 0), (0, 0.40, 0), 0.21),
            hbox("fcrcs_driver", (0.27, 0.22, 0), (0.15, 0.30, 0.30)),
            hcyl("fcrcs_coil", (0, 0.40, 0), (0, 0.58, 0), 0.365),
            hcyl("fcrcs_emitter", (0, 0.58, 0), (0, 0.69, 0), 0.30)]


# ---------------------------------------------------------------- Cobretty SR-150 service reactor (kb.apu.s150)
# 1.76 m cube, centred, axis Z, pad on top (+Y). A compact smooth drum with domed ends and two ring bands (a small
# cousin of the ER-5 skin), the bus coupling on the -Z end (Cobretty busbar boss, 3 terminals, data socket at 6 o'clock),
# energy cells rack (-X), starter drum and bus regulator (+X), sensor heads, a short heat-pipe saddle and a square pad.
# Passport has no cooling_pad_side_m for service reactors: the pad is the envelope's top face (1.76 m square).
SR150 = dict(half=0.88, vr=0.62, vy=-0.22, vz=0.78, pad_y0=0.64, pad_t=0.16)


def _cob_sqpad(pd, lod, y0, t, L, frame="eq_metal_band", tile="eq_pad_ceramic", hub="eq_metal_band", back="eq_metal_graphite"):
    """small square cooling pad (side 2L): backing sheet, frame, cross beams, 16 replaceable tiles, centre hub"""
    y1 = y0 + t
    if lod == 2:
        box(pd, tile, (0, (y0 + y1) / 2, 0), (2 * L, t, 2 * L))
        return
    bw, mw = 0.09, 0.06
    box(pd, back, (0, y0 + 0.015, 0), (2 * L - 0.04, 0.03, 2 * L - 0.04))
    for s in (-1, 1):
        box(pd, frame, (s * (L - bw / 2), (y0 + y1) / 2, 0), (bw, t, 2 * L), chamfer=_ch(lod, 0.012))
        box(pd, frame, (0, (y0 + y1) / 2, s * (L - bw / 2)), (2 * L - 2 * bw, t, bw), chamfer=_ch(lod, 0.012))
    for th in (0.0, math.pi / 2):
        box(pd, frame, (0, (y0 + y1) / 2 - 0.005, 0), (2 * L - 2 * bw, t - 0.01, mw), rot_y(th))
    sp = L - bw - mw / 2
    for sx in (-1, 1):
        for sz in (-1, 1):
            for i in range(2):
                for j in range(2):
                    cx = sx * (mw / 2 + sp / 4 + i * sp / 2)
                    cz = sz * (mw / 2 + sp / 4 + j * sp / 2)
                    box(pd, tile, (cx, (y0 + 0.03 + y1 - 0.012) / 2, cz), (sp / 2 - 0.02, y1 - 0.012 - y0 - 0.03, sp / 2 - 0.02))
    box(pd, hub, (0, y1 + 0.005, 0), (0.22, 0.03, 0.22), chamfer=_ch(lod, 0.01))
    if lod == 0:
        for s in (-1, 1):
            for i in range(9):
                q = -L + 0.12 + i * (2 * L - 0.24) / 8
                hex_bolt(pd, "eq_steel", (s * (L - bw / 2), y1, q), (0, 1, 0), 0.03, 0.014, washer=False)
                hex_bolt(pd, "eq_steel", (q, y1, s * (L - bw / 2)), (0, 1, 0), 0.03, 0.014, washer=False)
        for sx in (-1, 1):
            for sz in (-1, 1):
                hex_bolt(pd, "eq_steel", (sx * 0.07, y1 + 0.02, sz * 0.07), (0, 1, 0), 0.035, 0.018)
                lathe(pd, "eq_steel", crect(0.025, 0.045, -0.015, 0.015), 8, axis_to((1, 0, 0)), (sx * (L - 0.06), y1 + 0.06, sz * (L - 0.06)))


def sr150_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = SR150
    vr, vy, vz = d["vr"], d["vy"], d["vz"]
    P = {}
    v = P["sr150_vessel"] = Part("sr150_vessel")
    prof = [(0, -vz), (0.40, -vz), (0.54, -0.72), (vr, -0.58), (vr, 0.58), (0.54, 0.72), (0.40, vz), (0, vz)]
    lathe(v, "eq_metal_graphite", prof, seg, I3, (0, vy, 0), soft=40)
    if lod < 2:
        for zz in (-0.32, 0.32):
            lathe(v, "eq_metal_band", crect(vr - 0.01, vr + 0.035, zz - 0.06, zz + 0.06, _ch(lod, 0.012)), seg, I3, (0, vy, 0))
    if lod == 0:
        for ac in (math.radians(140), math.radians(220)):                  # flush access panels
            lathe(v, "eq_metal_panel", crect(vr - 0.01, vr + 0.02, -0.22, 0.22, 0.008), 6, I3, (0, vy, 0), ac - 0.32, ac + 0.32, cap=True)
            for aa in (ac - 0.27, ac + 0.27):
                for zz in (-0.17, 0.17):
                    hex_bolt(v, "eq_steel", tuple(a + b for a, b in zip(polz(vr + 0.02, aa, zz), (0, vy, 0))), polz(1, aa, 0), 0.03, 0.015, washer=False)

    bc = P["sr150_bus_coupling"] = Part("sr150_bus_coupling")
    lathe(bc, "eq_metal_band", [(0, -0.86), (0.30, -0.86), (0.30, -0.80), (0.38, -0.80), (0.38, -0.76), (0, -0.76)], seg, I3, (0, vy, 0))
    if lod < 2:
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.17 * math.cos(a), vy + 0.17 * math.sin(a), -0.86)
            lathe(bc, "eq_insulator", [(0, 0), (0.06, 0), (0.06, -0.012), (0, -0.012)][::-1], small, I3, p)
            lathe(bc, "eq_copper", [(0, -0.012), (0.03, -0.012), (0.03, -0.02), (0, -0.02)][::-1], small, I3, p)
        box(bc, "eq_metal_graphite", (0, vy - 0.48, -0.70), (0.18, 0.10, 0.10), chamfer=_ch(lod, 0.01))    # data socket, 6 o'clock
    if lod == 0:
        for i in range(24):
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(bc, "eq_steel", (0.34 * math.cos(a), vy + 0.34 * math.sin(a), -0.80), (0, 0, -1), 0.025, 0.012, washer=False)
        for i in range(4):
            cyl(bc, "eq_copper", (-0.045 + 0.03 * i, vy - 0.48, -0.75), (-0.045 + 0.03 * i, vy - 0.48, -0.758), 0.009, 6)

    ce = P["sr150_cells"] = Part("sr150_cells")
    for k in range(3):
        z = -0.42 + 0.42 * k
        box(ce, "eq_metal_panel", (-0.745, -0.30, z), (0.23, 0.56, 0.38), chamfer=_ch(lod, 0.02))
        if lod < 2:
            box(ce, "eq_metal_band", (-0.745, -0.30, z + 0.20), (0.23, 0.56, 0.025))
        if lod == 0:
            for yy in (-0.12, -0.48):
                cyl(ce, "eq_insulator", (-0.86, yy, z - 0.08), (-0.875, yy, z - 0.08), 0.022, 8)
                cyl(ce, "eq_copper", (-0.875, yy, z - 0.08), (-0.88, yy, z - 0.08), 0.012, 6)
            box(ce, "eq_status_amber", (-0.862, -0.10, z + 0.10), (0.004, 0.03, 0.06))

    st = P["sr150_starter"] = Part("sr150_starter")
    sx, sy = 0.72, -0.62
    lathe(st, "eq_metal_graphite", [(0, -0.45), (0.11, -0.45), (0.14, -0.42), (0.14, 0.28), (0.11, 0.32), (0, 0.32)], small * 2 if lod < 2 else small, I3, (sx, sy, 0))
    if lod < 2:
        lathe(st, "eq_metal_band", crect(0.135, 0.155, -0.10, -0.04, _ch(lod, 0.006)), small * 2, I3, (sx, sy, 0))
        box(st, "eq_metal_band", (sx - 0.10, sy, -0.10), (0.14, 0.10, 0.50))
    if lod == 0:
        cyl(st, "eq_insulator", (sx, sy + 0.14, 0.12), (sx, sy + 0.19, 0.12), 0.025, 8)
        for i in range(6):
            a = 2 * math.pi * i / 6
            hex_bolt(st, "eq_steel", (sx + 0.08 * math.cos(a), sy + 0.08 * math.sin(a), 0.32), (0, 0, 1), 0.02, 0.01, washer=False)

    rg = P["sr150_regulator"] = Part("sr150_regulator")
    box(rg, "eq_metal_panel", (0.745, -0.02, 0.05), (0.23, 0.46, 0.80), chamfer=_ch(lod, 0.02))
    if lod < 2:
        for i in range(5):                                                 # cooling fins on top
            box(rg, "eq_metal_band", (0.745, 0.225, -0.25 + 0.15 * i), (0.20, 0.03, 0.03))
    if lod == 0:
        box(rg, "eq_metal_graphite", (0.863, -0.02, 0.05), (0.006, 0.36, 0.62), chamfer=0.003)
        for yy in (-0.17, 0.13):
            for zz in (-0.22, 0.32):
                hex_bolt(rg, "eq_steel", (0.866, yy, zz), (1, 0, 0), 0.025, 0.012, washer=False)
        for i in range(3):
            box(rg, "eq_status_amber", (0.867, 0.10, -0.05 + 0.06 * i), (0.004, 0.03, 0.03))

    sn = P["sr150_sensors"] = Part("sr150_sensors")
    if lod < 2:
        for a, zz in ((math.radians(45), -0.50), (math.radians(135), -0.50), (math.radians(45), 0.50), (math.radians(135), 0.50)):
            p0 = polz(0.56, a, zz)
            p1 = polz(0.66, a, zz)
            cyl(sn, "eq_metal_graphite", (p0[0], p0[1] + vy, zz), (p1[0], p1[1] + vy, zz), 0.045, small, chamfer=_ch(lod, 0.008))
            if lod == 0:
                lathe(sn, "eq_insulator", [(0.03, 0), (0.025, 0.02), (0, 0.03)], 8, axis_to(polz(1, a, 0)), (p1[0], p1[1] + vy, zz))

    hc = P["sr150_heat_pipe_column"] = Part("sr150_heat_pipe_column")
    top = vy + vr
    box(hc, "eq_metal_band", (0, top + 0.02, 0), (0.50, 0.10, 0.90), chamfer=_ch(lod, 0.015))
    for x, z in ((0, -0.25), (0, 0.25), (-0.15, 0), (0.15, 0)) if lod < 2 else ((0, 0),):
        cyl(hc, "eq_steel" if lod < 2 else "eq_metal_graphite", (x, top + 0.07, z), (x, d["pad_y0"], z), 0.05 if lod < 2 else 0.20, small)
    ylathe(hc, "eq_metal_band", [(0, d["pad_y0"] - 0.03), (0.32, d["pad_y0"] - 0.03), (0.32, d["pad_y0"]), (0, d["pad_y0"])], seg)

    _cob_sqpad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["half"])

    ec = P["sr150_emergency_coolant"] = Part("sr150_emergency_coolant")
    if lod < 2:
        box(ec, "eq_metal_panel", (0, top + 0.05, 0.62), (0.26, 0.10, 0.14), chamfer=_ch(lod, 0.01))
        for s in (-1, 1):
            cyl(ec, "eq_copper", (0.07 * s, top + 0.06, 0.45), (0.07 * s, top + 0.06, 0.55), 0.03, small)
            cyl(ec, "eq_copper", (0.07 * s, top + 0.10, 0.62), (0.07 * s, top + 0.22, 0.62), 0.035, small)
            if lod == 0:
                ylathe(ec, "eq_steel", [(0, 0), (0.05, 0), (0.05, 0.02), (0, 0.02)], 12, t=(0.07 * s, top + 0.22, 0.62))

    mt = P["sr150_mounts"] = Part("sr150_mounts")
    for zz in (-0.45, 0.45):
        box(mt, "eq_metal_band", (0, -0.85, zz), (1.10, 0.06, 0.22), chamfer=_ch(lod, 0.012))
        box(mt, "eq_metal_band", (0, -0.79, zz), (0.56, 0.08, 0.18))
        if lod == 0:
            for x in (-0.45, -0.32, 0.32, 0.45):
                hex_bolt(mt, "eq_steel", (x, -0.82, zz), (0, 1, 0), 0.035, 0.018)

    if close():
        hz = P["sr150_harness"] = Part("sr150_harness")
        for k in range(3):
            z = -0.42 + 0.42 * k
            harness(hz, bezier((-0.63, -0.10, z), (-0.55, 0.05, z), (-0.20, 0.30, z * 0.6), (0.0, top + 0.03, z * 0.5), 10),
                    0.014, 2, ("eq_insulator", "eq_metal_graphite"), tie_every=4)
        harness(hz, bezier((0.63, -0.20, 0.30), (0.56, -0.40, 0.35), (0.70, -0.46, 0.20), (0.72, -0.43, 0.12), 8), 0.016, 3,
                ("eq_insulator", "eq_metal_graphite"), tie_every=3)
        harness(hz, bezier((0.63, -0.10, -0.30), (0.55, -0.15, -0.60), (0.25, vy - 0.20, -0.80), (0.20, vy - 0.10, -0.86), 10),
                0.02, 3, ("eq_insulator", "eq_metal_graphite"), tie_every=4)
    return P


def sr150_hits():
    d = SR150
    vr, vy, vz = d["vr"], d["vy"], d["vz"]
    top = vy + vr
    return [hcyl("sr150_vessel", (0, vy, -vz), (0, vy, vz), vr),
            hcyl("sr150_bus_coupling", (0, vy, -0.87), (0, vy, -0.76), 0.38),
            hbox("sr150_cells", (-0.745, -0.30, 0), (0.23, 0.56, 1.22)),
            hcyl("sr150_starter", (0.72, -0.62, -0.45), (0.72, -0.62, 0.32), 0.14),
            hbox("sr150_regulator", (0.745, -0.02, 0.05), (0.23, 0.46, 0.80)),
            hbox("sr150_heat_pipe_column", (0, (top + d["pad_y0"]) / 2, 0), (0.50, d["pad_y0"] - top + 0.06, 0.90)),
            hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.01, 0), (2 * d["half"], d["pad_t"] + 0.02, 2 * d["half"])),
            hbox("sr150_emergency_coolant", (0, top + 0.12, 0.58), (0.26, 0.20, 0.30)),
            hbox("sr150_mounts", (0, -0.82, -0.45), (1.10, 0.12, 0.22)),
            hbox("sr150_mounts", (0, -0.82, 0.45), (1.10, 0.12, 0.22))] +         [hcyl("sr150_sensors", tuple(a + b for a, b in zip(polz(0.56, t, z), (0, vy, 0))),
              tuple(a + b for a, b in zip(polz(0.69, t, z), (0, vy, 0))), 0.045)
         for t in (math.radians(45), math.radians(135)) for z in (-0.50, 0.50)]


# ---------------------------------------------------------------- Cobretty PD-1 point-defence laser turret (kb.wpn.pd1)
# Hull-skin unit, outward +Y, mounting face y = 0; envelope 1.25 x 0.75 x 2.0 with the emitter stowed along +Z.
# SRUs: mount (traverse drum, yoke, elevation drives) and emitter (laser tube, ring-band heat sinks, aperture).
def pd1_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    ey = 0.52
    P = {}
    b = P["pd1_base"] = Part("pd1_base")
    ylathe(b, "eq_metal_band", [(0, 0), (0.58, 0), (0.58, 0.04), (0.55, 0.06), (0, 0.06)], seg)
    if lod == 0:
        for i in range(24):
            hex_bolt(b, "eq_steel", polar(0.545, 2 * math.pi * (i + 0.5) / 24, 0.06), (0, 1, 0), 0.025, 0.012, washer=False)

    m = P["pd1_mount"] = Part("pd1_mount")
    ylathe(m, "eq_metal_graphite", [(0.50, 0.06), (0.50, 0.22), (0.44, 0.28), (0, 0.28)], seg)
    if lod < 2:
        ylathe(m, "eq_metal_band", crect(0.495, 0.525, 0.11, 0.16, _ch(lod, 0.008)), seg)
    for s in (-1, 1):
        box(m, "eq_metal_graphite", (s * 0.36, 0.40, 0), (0.12, 0.24, 0.42), chamfer=_ch(lod, 0.015))   # yoke cheeks
        cyl(m, "eq_metal_graphite", (s * 0.30, ey, 0), (s * 0.42, ey, 0), 0.21, seg if lod < 2 else small)
        cyl(m, "eq_metal_band", (s * 0.42, ey, 0), (s * 0.54, ey, 0), 0.13, seg // 2 if lod < 2 else small, chamfer=_ch(lod, 0.01))  # elevation drive
    cyl(m, "eq_steel", (-0.30, ey, 0), (0.30, ey, 0), 0.07, small, caps=False)                       # trunnion
    if lod < 2:
        box(m, "eq_metal_panel", (0, 0.17, -0.50), (0.24, 0.14, 0.16), chamfer=_ch(lod, 0.012))      # traverse drive
    if lod == 0:
        box(m, "eq_status_amber", (0.12, 0.20, -0.581), (0.03, 0.03, 0.004))
        for s in (-1, 1):
            for i in range(6):
                a = 2 * math.pi * i / 6
                hex_bolt(m, "eq_steel", (s * 0.54, ey + 0.08 * math.cos(a), 0.08 * math.sin(a)), (s, 0, 0), 0.02, 0.01, washer=False)
    if close():
        harness(m, bezier((0.10, 0.28, -0.25), (0.12, 0.40, -0.40), (0.10, ey + 0.05, -0.70), (0.06, ey + 0.12, -0.74), 8), 0.018, 2,
                ("eq_insulator", "eq_metal_graphite"), tie_every=4)

    e = P["pd1_emitter"] = Part("pd1_emitter")
    prof = [(0, -0.80), (0.13, -0.80), (0.19, -0.74), (0.19, 0.82), (0.16, 0.90), (0.15, 0.96), (0.12, 0.97), (0.12, 0.955)]
    lathe(e, "eq_metal_graphite", prof, seg, I3, (0, ey, 0))
    lathe(e, "eq_cob_lens", [(0.12, 0.955), (0, 0.965)], seg if lod < 2 else small, I3, (0, ey, 0))
    if lod < 2:
        for z in (-0.64, -0.50, -0.36, -0.22):
            lathe(e, "eq_metal_band", crect(0.185, 0.22, z - 0.03, z + 0.03, _ch(lod, 0.006)), seg, I3, (0, ey, 0))
        lathe(e, "eq_metal_band", crect(0.185, 0.21, 0.66, 0.74, _ch(lod, 0.006)), seg, I3, (0, ey, 0))
    if lod == 0:
        for i in range(8):
            a = 2 * math.pi * (i + 0.5) / 8
            hex_bolt(e, "eq_steel", (0.135 * math.cos(a), ey + 0.135 * math.sin(a), 0.966), (0, 0, 1), 0.016, 0.008, washer=False)
        box(e, "eq_metal_panel", (0, ey + 0.19, 0.35), (0.10, 0.012, 0.40), chamfer=0.003)            # alignment hatch
    return P


def pd1_hits():
    ey = 0.52
    return [hcyl("pd1_base", (0, 0, 0), (0, 0.06, 0), 0.58),
            hcyl("pd1_mount", (0, 0.06, 0), (0, 0.28, 0), 0.50),
            hbox("pd1_mount", (-0.42, 0.45, 0), (0.24, 0.40, 0.42)),
            hbox("pd1_mount", (0.42, 0.45, 0), (0.24, 0.40, 0.42)),
            hcyl("pd1_emitter", (0, ey, -0.80), (0, ey, 0.97), 0.21)]


# ---------------------------------------------------------------- Cobretty EC-2 two-seat ejectable cabin capsule (kb.surv.capsule2)
# Axis Z, centred: a smooth graphite capsule with a rounded nose (+Z) and the cabin hatch on the flat -Z end, three ring
# bands, two viewports. It sits on two separation rails (-Y) and is ejected along +Y. SRUs: pyro (separation bolts on
# the rails, copper caps = emergency), battery (flush side box), beacon (dome on top), seal (hatch seal ring).
def ec2_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    P = {}
    h = P["ec2_hull"] = Part("ec2_hull")
    prof = [(0, -1.32), (0.82, -1.32), (0.90, -1.24), (0.90, 0.50), (0.86, 0.72), (0.74, 0.95), (0.55, 1.16), (0.30, 1.30), (0, 1.345)]
    lathe(h, "eq_metal_graphite", prof, seg, soft=35)
    if lod < 2:
        for z in (-1.05, -0.25, 0.45):
            lathe(h, "eq_metal_band", crect(0.895, 0.935, z - 0.06, z + 0.06, _ch(lod, 0.012)), seg)
        for a in (math.radians(65), math.radians(115)):                    # viewports
            n = norm((0.88 * math.cos(a), 0.88 * math.sin(a), 0.47))
            p = (0.80 * math.cos(a), 0.80 * math.sin(a), 0.82)
            R = axis_to(n)
            lathe(h, "eq_metal_band", crect(0.12, 0.16, -0.02, 0.025, _ch(lod, 0.008)), small * 2, R, p)
            lathe(h, "eq_cob_lens", [(0.12, 0.008), (0, 0.008)], small * 2, R, p)
    if lod == 0:
        for z0, z1 in ((-0.95, -0.35), (-0.15, 0.35)):                   # flush side panels
            for ac in (math.radians(20), math.radians(160), math.radians(200)):
                lathe(h, "eq_metal_panel", crect(0.895, 0.915, z0, z1, 0.006), 6, I3, (0, 0, 0), ac - 0.30, ac + 0.30, cap=True)
        box(h, "eq_metal_panel", (0, 0.86, -0.80), (0.30, 0.10, 0.20), chamfer=0.01)                   # lifting / recovery eye
        lathe(h, "eq_steel", crect(0.04, 0.07, -0.02, 0.02), 8, axis_to((1, 0, 0)), (0, 0.93, -0.80))

    sl = P["ec2_hatch_seal"] = Part("ec2_hatch_seal")
    lathe(sl, "eq_metal_panel", [(0, -1.335), (0.47, -1.335), (0.47, -1.32)][::-1], seg)
    lathe(sl, "eq_insulator", crect(0.47, 0.54, -1.338, -1.318), seg)
    if lod < 2:
        box(sl, "eq_steel", (0.26, 0, -1.345), (0.04, 0.26, 0.02))                                     # handle
        box(sl, "eq_metal_band", (-0.50, 0, -1.33), (0.08, 0.40, 0.035))                                # hinge block
    if lod == 0:
        for i in range(12):
            a = 2 * math.pi * (i + 0.5) / 12
            hex_bolt(sl, "eq_steel", (0.43 * math.cos(a), 0.43 * math.sin(a), -1.335), (0, 0, -1), 0.02, 0.008, washer=False)
        box(sl, "eq_cob_lens", (0, 0.22, -1.338), (0.16, 0.10, 0.006))                                  # hatch window
        box(sl, "eq_status_amber", (0.26, -0.20, -1.338), (0.04, 0.04, 0.004))

    rl = P["ec2_rails"] = Part("ec2_rails")
    for s in (-1, 1):
        box(rl, "eq_metal_band", (s * 0.40, -0.87, -0.35), (0.12, 0.18, 1.60), chamfer=_ch(lod, 0.015))

    py = P["ec2_pyro"] = Part("ec2_pyro")
    for s in (-1, 1):
        for z in (-1.00, -0.35, 0.30):
            cyl(py, "eq_metal_graphite", (s * 0.52, -0.95, z), (s * 0.52, -0.70, z), 0.055, small, chamfer=_ch(lod, 0.008))
            cyl(py, "eq_copper", (s * 0.52, -0.955, z), (s * 0.52, -0.95, z), 0.04, small)
            if close():
                harness(py, bezier((s * 0.52, -0.72, z + 0.05), (s * 0.55, -0.70, z + 0.15), (s * 0.60, -0.62, z + 0.30),
                                   (s * 0.62, -0.60, z + 0.55 if z < 0.3 else z + 0.12), 6), 0.01, 1, ("eq_copper",), tie_every=10)

    bt = P["ec2_battery"] = Part("ec2_battery")
    box(bt, "eq_metal_panel", (0.87, -0.10, -0.55), (0.14, 0.50, 0.70), chamfer=_ch(lod, 0.02))
    if lod == 0:
        for i in range(6):
            box(bt, "eq_metal_band", (0.942, -0.25 + 0.06 * i, -0.55), (0.006, 0.02, 0.40))
        box(bt, "eq_status_amber", (0.943, 0.10, -0.80), (0.004, 0.04, 0.06))

    bc = P["ec2_beacon"] = Part("ec2_beacon")
    ylathe(bc, "eq_metal_band", [(0.14, 0.86), (0.14, 0.925), (0.08, 0.925)], small * 2, t=(0, 0, 0.20))
    ylathe(bc, "eq_status_amber", [(0.08, 0.925), (0.065, 0.945), (0, 0.958)], small * 2, t=(0, 0, 0.20), soft=40)
    if lod == 0:
        box(bc, "eq_metal_graphite", (0, 0.905, -0.10), (0.12, 0.012, 0.22), chamfer=0.003)            # patch antenna
    return P


def ec2_hits():
    H = [hcyl("ec2_hull", (0, 0, -1.32), (0, 0, 0.55), 0.90),
         hcyl("ec2_hull", (0, 0, 0.55), (0, 0, 1.00), 0.78),
         hcyl("ec2_hull", (0, 0, 1.00), (0, 0, 1.345), 0.48),
         hcyl("ec2_hatch_seal", (0, 0, -1.345), (0, 0, -1.318), 0.54),
         hbox("ec2_battery", (0.87, -0.10, -0.55), (0.14, 0.50, 0.70)),
         hcyl("ec2_beacon", (0, 0.86, 0.20), (0, 0.96, 0.20), 0.14)]
    for s in (-1, 1):
        H.append(hbox("ec2_rails", (s * 0.40, -0.87, -0.35), (0.12, 0.18, 1.60)))
        for z in (-1.00, -0.35, 0.30):
            H.append(hcyl("ec2_pyro", (s * 0.52, -0.955, z), (s * 0.52, -0.70, z), 0.055))
    return H


ASSETS.update({"cobretty_n1ax_drive": n1_parts, "cobretty_cl180_coolant_loop": cl180_parts,
               "cobretty_rp10_radiator": rp10_parts, "cobretty_fcrcs_rcs": fcrcs_parts,
               "cobretty_sr150_service_reactor": sr150_parts, "cobretty_pd1_turret": pd1_parts,
               "cobretty_ec2_capsule": ec2_parts})
HITS.update({"cobretty_n1ax_drive": n1_hits, "cobretty_cl180_coolant_loop": cl180_hits,
             "cobretty_rp10_radiator": rp10_hits, "cobretty_fcrcs_rcs": fcrcs_hits,
             "cobretty_sr150_service_reactor": sr150_hits, "cobretty_pd1_turret": pd1_hits,
             "cobretty_ec2_capsule": ec2_hits})
MODEL_ASSETS.update({"kb.drive.n1-axial": "cobretty_n1ax_drive", "kb.cool.loop200": "cobretty_cl180_coolant_loop",
                     "kb.rad.cs-panel": "cobretty_rp10_radiator", "kb.rcs.fc-nozzle": "cobretty_fcrcs_rcs",
                     "kb.apu.s150": "cobretty_sr150_service_reactor", "kb.wpn.pd1": "cobretty_pd1_turret",
                     "kb.surv.capsule2": "cobretty_ec2_capsule"})
