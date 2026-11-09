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
