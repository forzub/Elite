"""Sol Reactor Works (Sol Authority, Atlantic school): SR-4.8 / SR-2.6 linear FRC propulsion reactors."""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_srw_white": ((0.80, 0.81, 0.80), (0.25, 0.25, 0.25), 50),
    "eq_srw_coil": ((0.12, 0.17, 0.30), (0.30, 0.30, 0.35), 70),
})

# ---------------------------------------------------------------- reactor families (author 2026-11-09)
# One base concept for every propulsion reactor: a unit coaxial with the engine bay (axis = OBJ Z), flanges to the fore /
# aft drives on its ends (mating faces at z = -/+face_z), the cooling pad on a heat-pipe column on top (+Y), the same
# serviceable units (fault class reactor.fusion: pad, plasma_ctl, damper = confinement, feed, inverter, sensor).
# Each maker has its own confinement school and look:
#   Cobretty (Hellas, premium modular)   - compact coaxial tokamak drum, magnet hoops under a smooth skin (ER-5 above);
#   Sol Reactor Works (Atlantic school)  - linear FRC collider (reference: Helion Polaris): formation sections at both
#                                          ends, coil-wrapped accelerators tapering to a central compression chamber,
#                                          long capacitor banks (pulsed power + direct energy recovery); white / navy;
#   Tianhe Heavy (Pact state concern)    - heavy tokamak (reference: EAST / CFETR): flat-plate 12-sided cryostat, 12
#                                          exposed D-shaped TF coils standing out like a gear, big welded flanges;
#                                          olive steel / red coil casings.
# The smaller models of a family are the big one scaled to their passport envelope, with their own pad side.
SRW = dict(face_z=3.05, flange_r=1.70, form_r=1.55, form_z0=2.25, form_z1=2.80, comp_r=0.85, comp_z=0.75, cone_r=1.40,
           pad_y0=2.35, pad_t=0.25, pad_half=5.39)


def srw_flange(lod, seg):
    """SRW flange, local frame: mating face at z = 0 facing +Z, body towards -Z"""
    d = SRW
    f = Part("flange")
    fr = d["flange_r"]
    prof = [(fr - 0.05, -0.25), (fr, -0.25), (fr, 0), (0.55, 0), (0.55, -0.20), (0, -0.20)]
    lathe(f, "eq_metal_band", prof, seg)
    if lod < 2:
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.26 * math.cos(a), 0.26 * math.sin(a), -0.20)
            lathe(f, "eq_insulator", [(0.11, 0), (0.11, 0.03), (0, 0.03)], 12 if lod == 0 else 6, I3, p)
            lathe(f, "eq_copper", [(0.06, 0.03), (0.06, 0.06), (0, 0.06)], 12 if lod == 0 else 6, I3, p)
        box(f, "eq_metal_graphite", (0.0, -0.95, 0.004), (0.34, 0.22, 0.008))
    if lod == 0:
        for i in range(16):
            a = 2 * math.pi * (i + 0.5) / 16
            hex_bolt(f, "eq_steel", (1.58 * math.cos(a), 1.58 * math.sin(a), -0.25), (0, 0, -1), 0.08, 0.05)
    return f


def srw_parts(lod):
    seg = seg_for(lod)
    small = small_for(lod)
    d = SRW
    fz0, fz1, fr = d["form_z0"], d["form_z1"], d["form_r"]
    P = {}

    # --- vessel: accelerators tapering to the compression chamber (open ends sit inside the formation sections)
    v = P["srw_vessel"] = Part("srw_vessel")
    cz, cr, kr = d["comp_z"], d["comp_r"], d["cone_r"]
    lathe(v, "eq_srw_white", [(kr, -fz0 - 0.05), (cr - 0.05, -cz - 0.10), (cr, -cz), (cr, cz), (cr - 0.05, cz + 0.10), (kr, fz0 + 0.05)], seg, soft=30)

    # --- confinement / compression coils (damper SRU): dense rings on the chamber, spaced rings on the accelerators
    m = P["srw_coils"] = Part("srw_coils")
    step = {0: 1, 1: 2, 2: 4}[lod]
    rings = [(-cz + 0.06 + i * (2 * cz - 0.12) / 10) for i in range(0, 11, step)]
    for z in rings:
        lathe(m, "eq_srw_coil", crect(cr, cr + 0.17, z - 0.05, z + 0.05), seg)
    for s in (-1, 1):
        for i in range(0, 6, step):
            z = cz + 0.25 + i * (fz0 - cz - 0.45) / 5
            rr = cr - 0.05 + (kr - cr + 0.05) * (z - cz - 0.10) / (fz0 - cz - 0.05)
            lathe(m, "eq_srw_coil", crect(rr, rr + 0.13, s * z - 0.05, s * z + 0.05), seg)

    # --- formation sections at both ends (white drums, two navy bands, an end ring)
    for name, s in (("srw_formation_fore", -1), ("srw_formation_aft", 1)):
        fm = P[name] = Part(name)
        z0, z1 = (fz0, fz1) if s > 0 else (-fz1, -fz0)
        prof = [(kr, z0), (fr, z0), (fr, z1), (fr - 0.25, z1)] if s > 0 else [(fr - 0.25, z0), (fr, z0), (fr, z1), (kr, z1)]
        lathe(fm, "eq_srw_white", prof, seg)
        if lod < 2:
            for zz in (z0 + 0.12, z1 - 0.12):
                lathe(fm, "eq_srw_coil", crect(fr - 0.01, fr + 0.07, zz - 0.05, zz + 0.05), seg)

    # --- flanges to the drives
    fl = srw_flange(lod, seg)
    P["srw_flange_aft"] = placed(fl, I3, (0, 0, d["face_z"]))
    P["srw_flange_aft"].name = "srw_flange_aft"
    P["srw_flange_fore"] = placed(fl, rot_y(math.pi), (0, 0, -d["face_z"]))
    P["srw_flange_fore"].name = "srw_flange_fore"

    # --- capacitor banks (inverter SRU: pulsed power + direct energy recovery): 4 long boxes on the diagonals
    cb = P["srw_capacitor_banks"] = Part("srw_capacitor_banks")
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        Rz = rot_z(a)
        box(cb, "eq_metal_panel", apply(Rz, (1.50, 0, 0))[:2] + (0.0,), (0.24, 0.55, 2 * fz0 + 0.40), Rz, chamfer=0.03 if lod == 0 else 0.0)
        if lod == 0:
            for i in range(1, 6):                                                       # cell dividers
                z = -fz0 + 0.10 + i * (2 * fz0 - 0.20) / 6
                box(cb, "eq_srw_coil", apply(Rz, (1.625, 0, 0))[:2] + (z,), (0.02, 0.57, 0.05), Rz)
            box(cb, "eq_status_amber", apply(Rz, (1.625, 0.18, 0))[:2] + (0.4,), (0.01, 0.06, 0.12), Rz)
        if lod < 2:
            for s in (-1, 1):                                                           # cables to the formation coils
                p0 = apply(Rz, (1.42, 0, 0))[:2] + (s * (fz0 - 0.15),)
                p1 = apply(Rz, (1.42, 0, 0))[:2] + (s * (fz0 + 0.12),)
                cyl(cb, "eq_insulator", p0, p1, 0.06, small)

    # --- fuel feed: He-3 / D gas valves on top of each formation section (-X side), one manifold box
    fd = P["srw_fuel_feed"] = Part("srw_fuel_feed")
    box(fd, "eq_metal_panel", (-1.62, 0.0, 0.0), (0.22, 0.45, 0.70), chamfer=0.03 if lod == 0 else 0.0)
    if lod < 2:
        for s in (-1, 1):
            cyl(fd, "eq_steel", (-1.62, 0.0, s * 0.35), (-1.62, 0.0, s * (fz0 + 0.30)), 0.04, small)
            cyl(fd, "eq_metal_band", polz(fr, math.radians(160), s * (fz0 + 0.30)), polz(fr + 0.18, math.radians(160), s * (fz0 + 0.30)), 0.08, small)

    # --- plasma control unit under the compression chamber, sensor ports on its sides
    pc = P["srw_plasma_ctl"] = Part("srw_plasma_ctl")
    box(pc, "eq_metal_panel", (0, -(cr + 0.32), 0), (0.80, 0.22, 1.30), chamfer=0.03 if lod == 0 else 0.0)
    if lod == 0:
        box(pc, "eq_status_amber", (0.25, -(cr + 0.435), 0.4), (0.10, 0.006, 0.06))
        cyl(pc, "eq_metal_band", (0.3, -(cr + 0.21), 0.55), (0.3, -(cr + 0.21), cz + 0.3), 0.05, 8)
    sn = P["srw_plasma_sensors"] = Part("srw_plasma_sensors")
    if lod == 0:
        for a in (0.0, math.pi):
            for z in (-0.45, 0.45):
                cyl(sn, "eq_metal_graphite", polz(cr + 0.17, a, z), polz(cr + 0.32, a, z), 0.06, small)

    # --- heat-pipe column from the chamber coils to the pad, the pad (PT1)
    top = cr + 0.17
    heat_column(P.setdefault("srw_heat_pipe_column", Part("srw_heat_pipe_column")), lod, seg, small, top, d["pad_y0"],
                band="eq_srw_white")
    cooling_pad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["pad_half"],
                frame="eq_srw_white")

    # --- emergency coolant (the only copper) and the mounting lugs under the formation sections
    ec = P["srw_emergency_coolant"] = Part("srw_emergency_coolant")
    if lod < 2:
        box(ec, "eq_metal_panel", (0, top + 0.08, 1.15), (0.40, 0.16, 0.30))
        for s in (-1, 1):
            cyl(ec, "eq_copper", (0.11 * s, top + 0.20, 0.45), (0.11 * s, top + 0.20, 1.02), 0.045, small)
            cyl(ec, "eq_copper", (0.11 * s, top + 0.16, 1.15), (0.11 * s, top + 0.36, 1.15), 0.055, small)
    mt = P["srw_mounts"] = Part("srw_mounts")
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for s in (-1, 1):
            Rz = rot_z(a)
            zz = s * (fz0 + fz1) / 2
            box(mt, "eq_metal_band", apply(Rz, (fr + 0.10, 0, 0))[:2] + (zz,), (0.22, 0.36, 0.40), Rz)
            if lod < 2:
                lathe(mt, "eq_steel", crect(0.04, 0.08, -0.14, 0.14), small, I3, apply(Rz, (fr + 0.16, 0, 0))[:2] + (zz,))

    # --- close-inspection extras (asset LOD0): pulsed-power leads from every bank to every coil ring
    if close():
        hz = P["srw_harness"] = Part("srw_harness")
        zs = rings + [s * (cz + 0.25 + i * (fz0 - cz - 0.45) / 5) for s in (-1, 1) for i in range(6)]
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            for z in zs:
                az = abs(z)
                rr = cr + 0.17 if az <= cz else (cr - 0.05 + (kr - cr + 0.05) * (az - cz - 0.10) / (fz0 - cz - 0.05)) + 0.13
                tube(hz, "eq_srw_coil" if k % 2 else "eq_insulator", [polz(1.38, a, z), polz((1.38 + rr) / 2, a + 0.04, z), polz(rr, a, z)], 0.02, 6)
    return P




def srw_hits():
    d = SRW
    fz0, fz1, fr = d["form_z0"], d["form_z1"], d["form_r"]
    H = [hcyl("srw_vessel", (0, 0, -fz0), (0, 0, fz0), d["cone_r"]),
         hcyl("srw_coils", (0, 0, -d["comp_z"]), (0, 0, d["comp_z"]), d["comp_r"] + 0.17),
         hcyl("srw_formation_fore", (0, 0, -fz1), (0, 0, -fz0), fr + 0.07),
         hcyl("srw_formation_aft", (0, 0, fz0), (0, 0, fz1), fr + 0.07),
         hcyl("srw_flange_fore", (0, 0, -d["face_z"]), (0, 0, -fz1), d["flange_r"]),
         hcyl("srw_flange_aft", (0, 0, fz1), (0, 0, d["face_z"]), d["flange_r"])]
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        H.append(hbox("srw_capacitor_banks", polz(1.50, a, 0), (0.26, 0.57, 2 * fz0 + 0.40), (0, 0, math.degrees(a))))
    cr, top = d["comp_r"], d["comp_r"] + 0.17
    H += [hbox("srw_fuel_feed", (-1.62, 0, 0), (0.22, 0.45, 0.70)),
          hbox("srw_plasma_ctl", (0, -(cr + 0.32), 0), (0.80, 0.22, 1.30)),
          hbox("srw_heat_pipe_column", (0, top + 0.02, 0), (0.80, 0.16, 1.30)),
          hcyl("srw_heat_pipe_column", (0, top, 0), (0, d["pad_y0"], 0), 0.65),
          hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.03, 0), (2 * d["pad_half"], d["pad_t"] + 0.06, 2 * d["pad_half"])),
          hbox("srw_emergency_coolant", (0, top + 0.20, 0.80), (0.40, 0.40, 0.80))]
    for a in (0.0, math.pi):
        for z in (-0.45, 0.45):
            H.append(hcyl("srw_plasma_sensors", polz(cr + 0.17, a, z), polz(cr + 0.32, a, z), 0.06))
    for a in (5 * math.pi / 4, 7 * math.pi / 4):
        for s in (-1, 1):
            H.append(hbox("srw_mounts", polz(fr + 0.10, a, s * (fz0 + fz1) / 2), (0.22, 0.36, 0.40), (0, 0, math.degrees(a))))
    return H



SR26_S = 2.85 / 3.4         # SR-2.6 passport envelope / SR-4.8 envelope

ASSETS = {"srw_sr48_reactor": srw_parts, "srw_sr26_reactor": scaled_family(srw_parts, SRW, SR26_S, 7.92)}
HITS = {"srw_sr48_reactor": srw_hits, "srw_sr26_reactor": scaled_hits(srw_hits, SRW, SR26_S, 7.92)}
MODEL_ASSETS = {"srw.reactor.sr48": "srw_sr48_reactor", "srw.reactor.sr26": "srw_sr26_reactor"}





# ======================================================================== SRW service reactors added 2026-11-10
# S-200 (1.937 m cube) and S-60 (the S-200 scaled to its 1.397 m cube). Centred, axis Z, pad on top (+Y).
# Atlantic lab school: a long white cylinder ringed by navy compression coils, an open external rack of energy cells
# (-X), a navy starter drum and a white instrument rack with a navy face (bus regulator, +X), many thin instrument taps
# (sensor SRU), the SRW round output flange (r = 0.5 * vessel diameter, 16 bolts) on the -Z end.
# Service reactor passports carry no cooling_pad_side_m: the pad is the envelope's top face.
S200 = dict(pad_half=0.9685, vr=0.50, vy=-0.26, vz=0.80, coil=0.58, pad_y0=0.60, pad_t=0.17)


def _srw_sqpad(pd, lod, y0, t, L):
    """SRW square pad: white frame and cross beams, 4 x 4 ceramic tiles, navy hub plate"""
    y1 = y0 + t
    if lod == 2:
        box(pd, "eq_pad_ceramic", (0, (y0 + y1) / 2, 0), (2 * L, t, 2 * L))
        return
    bw, mw = 0.08, 0.05
    box(pd, "eq_metal_graphite", (0, y0 + 0.015, 0), (2 * L - 0.04, 0.03, 2 * L - 0.04))
    for s in (-1, 1):
        box(pd, "eq_srw_white", (s * (L - bw / 2), (y0 + y1) / 2, 0), (bw, t, 2 * L))
        box(pd, "eq_srw_white", (0, (y0 + y1) / 2, s * (L - bw / 2)), (2 * L - 2 * bw, t, bw))
    for th in (0.0, math.pi / 2):
        box(pd, "eq_srw_white", (0, (y0 + y1) / 2 - 0.005, 0), (2 * L - 2 * bw, t - 0.01, mw), rot_y(th))
    sp = L - bw - mw / 2
    for sx in (-1, 1):
        for sz in (-1, 1):
            for i in range(2):
                for j in range(2):
                    cx = sx * (mw / 2 + sp / 4 + i * sp / 2)
                    cz = sz * (mw / 2 + sp / 4 + j * sp / 2)
                    box(pd, "eq_pad_ceramic", (cx, (y0 + 0.03 + y1 - 0.012) / 2, cz), (sp / 2 - 0.02, y1 - 0.012 - y0 - 0.03, sp / 2 - 0.02))
    box(pd, "eq_srw_coil", (0, y1 + 0.005, 0), (0.26, 0.03, 0.26))
    if lod == 0:
        for s in (-1, 1):
            for i in range(10):
                q = -L + 0.10 + i * (2 * L - 0.20) / 9
                hex_bolt(pd, "eq_steel", (s * (L - bw / 2), y1, q), (0, 1, 0), 0.028, 0.012, washer=False)
                hex_bolt(pd, "eq_steel", (q, y1, s * (L - bw / 2)), (0, 1, 0), 0.028, 0.012, washer=False)
        for sx in (-1, 1):
            for sz in (-1, 1):
                hex_bolt(pd, "eq_steel", (sx * 0.08, y1 + 0.02, sz * 0.08), (0, 1, 0), 0.035, 0.018)


def s200_parts(lod):
    seg, small = seg_for(lod), small_for(lod)
    d = S200
    vr, vy, vz, cr = d["vr"], d["vy"], d["vz"], d["coil"]
    T = (0, vy, 0)
    P = {}

    v = P["s200_vessel"] = Part("s200_vessel")
    lathe(v, "eq_srw_white", [(vr, -vz), (vr, vz), (0.40, vz + 0.06), (0, vz + 0.07)], seg, I3, T, soft=30)
    zs = [-0.60 + 0.20 * i for i in range(7)]
    for z in (zs if lod < 2 else zs[::3]):                                                          # navy coils
        lathe(v, "eq_srw_coil", crect(vr - 0.01, cr, z - 0.035, z + 0.035), seg, I3, T)
    if lod < 2:
        lathe(v, "eq_srw_coil", crect(0.28, 0.42, vz + 0.04, vz + 0.075), seg, I3, T)              # end-cap ring

    fl = P["s200_flange"] = Part("s200_flange")
    lathe(fl, "eq_metal_band", [(0, -0.90), (cr, -0.90), (cr, -0.83), (vr, -0.80)], seg, I3, T)       # r = 0.5 * D
    if lod < 2:
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.22 * math.cos(a), vy + 0.22 * math.sin(a), -0.90)
            lathe(fl, "eq_insulator", [(0, 0), (0.08, 0), (0.08, -0.025), (0, -0.025)][::-1], small, I3, p)
            lathe(fl, "eq_copper", [(0, -0.025), (0.045, -0.025), (0.045, -0.05), (0, -0.05)][::-1], small, I3, p)
        box(fl, "eq_metal_graphite", (0, vy - 0.42, -0.904), (0.22, 0.10, 0.008))
    if lod == 0:
        for i in range(16):
            a = 2 * math.pi * (i + 0.5) / 16
            hex_bolt(fl, "eq_steel", (0.53 * math.cos(a), vy + 0.53 * math.sin(a), -0.90), (0, 0, -1), 0.06, 0.03)

    ce = P["s200_cells"] = Part("s200_cells")
    for z in (-0.66, 0.66):                                                                          # open rack: end plates
        box(ce, "eq_srw_white", (-0.80, -0.30, z), (0.34, 0.66, 0.05))
    if lod < 2:
        for y in (-0.62, 0.02):
            box(ce, "eq_srw_white", (-0.80, y, 0), (0.30, 0.03, 1.36))
    for x in (-0.71, -0.89):
        for y in (-0.46, -0.15):
            cyl(ce, "eq_srw_white", (x, y, -0.64), (x, y, 0.64), 0.07, small if lod < 2 else 6)
            if lod < 2:
                for z in (-0.58, 0.58):
                    lathe(ce, "eq_srw_coil", crect(0.069, 0.078, z - 0.03, z + 0.03), small, I3, (x, y, 0))
            if lod == 0:
                box(ce, "eq_status_amber", (x, y + 0.07, 0.3), (0.03, 0.004, 0.05))
    if lod == 0:
        for z in (-0.69, 0.69):
            for x in (-0.71, -0.89):
                for y in (-0.46, -0.15):
                    hex_bolt(ce, "eq_steel", (x, y, z), (0, 0, 1 if z > 0 else -1), 0.04, 0.02, washer=False)

    st = P["s200_starter"] = Part("s200_starter")
    sx, sy = 0.76, -0.62
    lathe(st, "eq_srw_coil", [(0, -0.45), (0.13, -0.45), (0.13, 0.35), (0, 0.35)], small * 2 if lod < 2 else small, I3, (sx, sy, 0))
    if lod < 2:
        for z0, z1 in ((-0.47, -0.42), (0.32, 0.37)):
            lathe(st, "eq_srw_white", crect(0.0, 0.14, z0, z1), small * 2, I3, (sx, sy, 0))
        box(st, "eq_srw_white", (sx - 0.12, sy, -0.05), (0.10, 0.08, 0.60))
    if lod == 0:
        for i in range(8):
            a = 2 * math.pi * i / 8
            cyl(st, "eq_srw_white", (sx + 0.13 * math.cos(a), sy + 0.13 * math.sin(a), -0.30),
                (sx + 0.13 * math.cos(a), sy + 0.13 * math.sin(a), 0.20), 0.012, 6)

    rg = P["s200_regulator"] = Part("s200_regulator")
    box(rg, "eq_srw_white", (0.78, -0.12, 0.10), (0.30, 0.48, 0.90))
    if lod < 2:
        box(rg, "eq_srw_coil", (0.934, -0.12, 0.10), (0.012, 0.40, 0.80))
    if lod == 0:
        for i in range(4):                                                                           # instrument modules
            z = -0.22 + 0.21 * i
            box(rg, "eq_srw_white", (0.942, -0.12, z), (0.006, 0.36, 0.17))
            box(rg, "eq_status_amber", (0.946, 0.02, z + 0.05), (0.004, 0.03, 0.03))
            for k in range(3):
                cyl(rg, "eq_steel", (0.945, -0.22 + 0.06 * k, z - 0.04), (0.965, -0.22 + 0.06 * k, z - 0.04), 0.012, 6)

    sn = P["s200_sensors"] = Part("s200_sensors")
    if lod < 2:
        for a in (math.radians(30), math.radians(150)):
            for z in (-0.50, -0.10, 0.30, 0.70):
                p0 = polz(cr - 0.08, a, z)
                p1 = polz(cr + 0.06, a, z)
                cyl(sn, "eq_steel", (p0[0], p0[1] + vy, z), (p1[0], p1[1] + vy, z), 0.025, small // 2 + 2)
        for i in range(6):                                                                           # end-cap taps
            a = 2 * math.pi * i / 6
            cyl(sn, "eq_srw_white", (0.20 * math.cos(a), vy + 0.20 * math.sin(a), vz + 0.06),
                (0.20 * math.cos(a), vy + 0.20 * math.sin(a), vz + 0.14), 0.03, small // 2 + 2)

    hc = P["s200_heat_pipe_column"] = Part("s200_heat_pipe_column")
    top = vy + vr
    box(hc, "eq_srw_white", (0, top + 0.04, 0), (0.50, 0.10, 1.25))
    for x, z in ((-0.15, -0.40), (0.15, -0.40), (-0.15, 0.40), (0.15, 0.40)) if lod < 2 else ((0, 0),):
        cyl(hc, "eq_steel" if lod < 2 else "eq_srw_white", (x, top + 0.09, z), (x, d["pad_y0"], z), 0.05 if lod < 2 else 0.22, small)
        if lod < 2:
            ylathe(hc, "eq_srw_coil", crect(0.05, 0.07, (top + d["pad_y0"]) / 2 - 0.02, (top + d["pad_y0"]) / 2 + 0.02), small, t=(x, 0, z))

    _srw_sqpad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["pad_half"])

    mt = P["s200_mounts"] = Part("s200_mounts")
    for x in (-0.30, 0.30):
        for z in (-0.50, 0.50):
            box(mt, "eq_srw_white", (x, -0.84, z), (0.14, 0.26, 0.12))
            box(mt, "eq_metal_band", (x, -0.95, z), (0.28, 0.035, 0.22))
            if lod == 0:
                for s in (-1, 1):
                    hex_bolt(mt, "eq_steel", (x + s * 0.10, -0.932, z), (0, 1, 0), 0.035, 0.018)

    if close():                                                                                      # thin leads
        hz = P["s200_harness"] = Part("s200_harness")
        for k, z in enumerate(zs):
            a = math.radians(-25)
            p0 = (cr * math.cos(a), vy + cr * math.sin(a), z)
            harness(hz, bezier(p0, (0.62, vy - 0.15, z), (0.62, -0.20, 0.10 + (z - 0.1) * 0.7), (0.63, -0.20, 0.10 + (z - 0.1) * 0.75), 8),
                    0.009, 2, ("eq_srw_coil", "eq_insulator"), tie_every=4)
        for a in (math.radians(30), math.radians(150)):
            for z in (-0.50, -0.10, 0.30, 0.70):
                p1 = polz(cr + 0.06, a, z)
                tube(hz, "eq_insulator", sag((p1[0], p1[1] + vy, z), (math.copysign(0.63, p1[0]) if p1[0] > 0 else -0.64, 0.05, z * 0.9), 0.06, 6), 0.006, 5)
        for y in (-0.46, -0.15):
            harness(hz, bezier((-0.64, y, -0.64), (-0.55, y, -0.75), (-0.30, vy - 0.10, -0.86), (-0.20, vy - 0.05, -0.90), 8),
                    0.012, 2, ("eq_srw_coil", "eq_insulator"), tie_every=4)
    return P


def s200_hits():
    d = S200
    vr, vy, vz, cr = d["vr"], d["vy"], d["vz"], d["coil"]
    top = vy + vr
    return [hcyl("s200_vessel", (0, vy, -0.80), (0, vy, vz + 0.07), cr),
            hcyl("s200_flange", (0, vy, -0.95), (0, vy, -0.80), cr),
            hbox("s200_cells", (-0.80, -0.30, 0), (0.32, 0.66, 1.37)),
            hcyl("s200_starter", (0.76, -0.62, -0.47), (0.76, -0.62, 0.37), 0.14),
            hbox("s200_regulator", (0.79, -0.12, 0.10), (0.32, 0.48, 0.90)),
            hcyl("s200_sensors", (0, vy, vz + 0.06), (0, vy, vz + 0.14), 0.23),
            hbox("s200_heat_pipe_column", (0, (top + d["pad_y0"]) / 2, 0), (0.50, d["pad_y0"] - top, 1.25)),
            hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.01, 0), (2 * d["pad_half"], d["pad_t"] + 0.02, 2 * d["pad_half"])),
            hbox("s200_mounts", (0, -0.89, -0.50), (0.88, 0.16, 0.22)),
            hbox("s200_mounts", (0, -0.89, 0.50), (0.88, 0.16, 0.22))] +         [hcyl("s200_sensors", tuple(a + b for a, b in zip(polz(cr - 0.02, t, z), (0, vy, 0))),
              tuple(a + b for a, b in zip(polz(cr + 0.06, t, z), (0, vy, 0))), 0.025)
         for t in (math.radians(30), math.radians(150)) for z in (-0.50, -0.10, 0.30, 0.70)]


S60_S = 1.397 / 1.937       # S-60 passport envelope / S-200 envelope

ASSETS.update({"srw_s200_service_reactor": s200_parts,
               "srw_s60_service_reactor": scaled_family(s200_parts, S200, S60_S, 1.397, {"s200_": "s60_"})})
HITS.update({"srw_s200_service_reactor": s200_hits,
             "srw_s60_service_reactor": scaled_hits(s200_hits, S200, S60_S, 1.397, {"s200_": "s60_"})})
MODEL_ASSETS.update({"srw.apu.s200": "srw_s200_service_reactor", "srw.apu.s60": "srw_s60_service_reactor"})
