"""Tianhe Heavy (Heavenly Pact state concern): TR-10 / TR-2.6V heavy tokamak propulsion reactors."""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)

register_materials({
    "eq_th_steel": ((0.33, 0.36, 0.31), (0.20, 0.20, 0.20), 40),
    "eq_th_coil": ((0.45, 0.16, 0.12), (0.30, 0.25, 0.25), 60),
})

TH = dict(face_z=3.105, flange_r=2.30, cryo_r=1.95, cryo_z=2.25, cone_z=2.55, neck_r=1.45, coil_r0=1.75, coil_out=0.70,
          coil_h=1.95, pad_y0=3.25, pad_t=0.25, pad_half=8.205)
TH_FACET0 = math.pi / 12          # 12-gon vertices at 15 + 30k deg: facet centres at 0, 30, 60 ... (units sit there)


def th_flange(lod):
    """Tianhe flange, local frame: mating face at z = 0 facing +Z; 12-sided welded plate, big bolts"""
    d = TH
    f = Part("flange")
    fr = d["flange_r"]
    lathe(f, "eq_th_steel", [(d["neck_r"], -0.70), (d["neck_r"], -0.35), (fr, -0.35), (fr, 0), (0.60, 0), (0.60, -0.22), (0, -0.22)],
          12, I3, (0, 0, 0), TH_FACET0, TH_FACET0 + 2 * math.pi, smooth=False)
    if lod < 2:
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = (0.30 * math.cos(a), 0.30 * math.sin(a), -0.22)
            lathe(f, "eq_insulator", [(0.13, 0), (0.13, 0.04), (0, 0.04)], 12 if lod == 0 else 6, I3, p)
            lathe(f, "eq_copper", [(0.07, 0.04), (0.07, 0.07), (0, 0.07)], 12 if lod == 0 else 6, I3, p)
        box(f, "eq_metal_graphite", (0.0, -1.20, 0.004), (0.42, 0.26, 0.008))
        for i in range(12):                                                         # welded gussets neck -> plate
            a = TH_FACET0 + 2 * math.pi * i / 12
            Rz = rot_z(a)
            box(f, "eq_th_steel", apply(Rz, ((d["neck_r"] + fr) / 2, 0, 0))[:2] + (-0.52,), (fr - d["neck_r"], 0.10, 0.34), Rz)
    if lod == 0:
        for i in range(24):
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", (2.10 * math.cos(a), 2.10 * math.sin(a), -0.35), (0, 0, -1), 0.12, 0.08)
    return f


def th_parts(lod):
    seg = seg_for(lod)
    small = small_for(lod)
    d = TH
    R, cz, kz = d["cryo_r"], d["cryo_z"], d["cone_z"]
    ap = R * math.cos(math.pi / 12)                                                 # 12-gon apothem
    P = {}

    # --- cryostat: flat-plate 12-gon drum with conical end plates, two welded belts, diagnostic port plates
    v = P["th_cryostat"] = Part("th_cryostat")
    poly = lambda prof: lathe(v, "eq_th_steel", prof, 12, I3, (0, 0, 0), TH_FACET0, TH_FACET0 + 2 * math.pi, smooth=False)
    poly([(d["neck_r"], -kz), (R, -cz), (R, cz), (d["neck_r"], kz)])
    if lod < 2:
        for zz in (-1.25, 1.25):
            lathe(v, "eq_metal_band", crect(R - 0.02, R + 0.08, zz - 0.10, zz + 0.10), 12, I3, (0, 0, 0), TH_FACET0, TH_FACET0 + 2 * math.pi, smooth=False)
        for k in (2, 4, 8, 10):                                                     # port plates on facets 60/120/240/300 deg
            a = k * math.pi / 6
            Rz = rot_z(a)
            box(v, "eq_metal_panel", apply(Rz, (ap + 0.04, 0, 0))[:2] + (0.0,), (0.08, 0.70, 0.90), Rz)
            if lod == 0:
                for yy in (-0.28, 0.28):
                    for zz in (-0.38, 0.38):
                        hex_bolt(v, "eq_steel", apply(Rz, (ap + 0.08, yy, 0))[:2] + (zz,), apply(Rz, (1, 0, 0)), 0.07, 0.04)
    if lod == 0:                                                                    # longitudinal weld seams at the vertices
        for k in range(12):
            a = TH_FACET0 + k * math.pi / 6
            Rz = rot_z(a)
            box(v, "eq_metal_band", apply(Rz, (R + 0.01, 0, 0))[:2] + (0.0,), (0.04, 0.06, 2 * cz - 0.1), Rz)

    # --- 12 exposed D-shaped TF coils (damper / confinement SRU) standing out at the 12-gon vertices
    m = P["th_tf_coils"] = Part("th_tf_coils")
    n_arc = {0: 10, 1: 6, 2: 4}[lod]
    r0, ro, h = d["coil_r0"], d["coil_out"], d["coil_h"]
    pts = [(r0 + ro * math.cos(t), h * math.sin(t)) for t in (math.pi / 2 - math.pi * i / n_arc for i in range(n_arc + 1))]
    for k in range(12):
        a = TH_FACET0 + k * math.pi / 6
        Rz = rot_z(a)
        for (r1, z1), (r2, z2) in zip(pts, pts[1:]):
            L = math.hypot(r2 - r1, z2 - z1) + 0.05
            phi = math.atan2(r2 - r1, z2 - z1)
            c = apply(Rz, ((r1 + r2) / 2, 0, 0))
            box(m, "eq_th_coil", (c[0], c[1], (z1 + z2) / 2), (0.22, 0.30, L), mat_mul(Rz, rot_y(phi)))
        if lod == 0:                                                                # coil clamps on the cryostat belts
            for zz in (-1.25, 1.25):
                box(m, "eq_metal_band", apply(Rz, (R + 0.14, 0, 0))[:2] + (zz,), (0.20, 0.42, 0.24), Rz)

    # --- flanges
    fl = th_flange(lod)
    P["th_flange_aft"] = placed(fl, I3, (0, 0, d["face_z"]))
    P["th_flange_aft"].name = "th_flange_aft"
    P["th_flange_fore"] = placed(fl, rot_y(math.pi), (0, 0, -d["face_z"]))
    P["th_flange_fore"].name = "th_flange_fore"

    # --- inverter (+X facet) with busbars along the drum to both flanges
    iv = P["th_inverter"] = Part("th_inverter")
    box(iv, "eq_metal_panel", (ap + 0.17, 0, 0), (0.30, 0.62, 1.70))
    if lod < 2:
        for zs in (-1, 1):
            for yy in (-0.16, 0.16):
                cyl(iv, "eq_insulator", (ap + 0.17, yy, zs * 0.85), (ap + 0.17, yy, zs * 2.78), 0.07, small)
    if lod == 0:
        box(iv, "eq_metal_graphite", (ap + 0.325, 0, 0), (0.012, 0.50, 1.40))
        for i in range(3):
            box(iv, "eq_status_amber", (ap + 0.333, 0.16, -0.5 + 0.1 * i), (0.006, 0.06, 0.06))

    # --- fuel feed (-X facet), plasma control (bottom facet), sensors on the upper facets
    fd = P["th_fuel_feed"] = Part("th_fuel_feed")
    box(fd, "eq_metal_panel", (-(ap + 0.17), 0, 0), (0.30, 0.62, 1.10))
    if lod < 2:
        cyl(fd, "eq_metal_band", (-(ap + 0.17), 0.31, 0), (-(ap + 0.17), 0.70, 0), 0.12, small)
    pc = P["th_plasma_ctl"] = Part("th_plasma_ctl")
    box(pc, "eq_metal_panel", (0, -(ap + 0.15), 0), (0.62, 0.26, 1.40))
    sn = P["th_plasma_sensors"] = Part("th_plasma_sensors")
    if lod == 0:
        for a in (math.pi / 6, 5 * math.pi / 6):
            for zz in (-0.7, 0.7):
                cyl(sn, "eq_metal_graphite", polz(ap, a, zz), polz(ap + 0.18, a, zz), 0.08, small)

    # --- column + pad, emergency coolant, lugs
    heat_column(P.setdefault("th_heat_pipe_column", Part("th_heat_pipe_column")), lod, seg, small, ap, d["pad_y0"],
                band="eq_th_steel")
    cooling_pad(P.setdefault("reactor_cooling_pad", Part("reactor_cooling_pad")), lod, d["pad_y0"], d["pad_t"], d["pad_half"],
                frame="eq_th_steel")
    ec = P["th_emergency_coolant"] = Part("th_emergency_coolant")
    if lod < 2:
        box(ec, "eq_metal_panel", (0, ap + 0.08, 1.30), (0.44, 0.16, 0.34))
        for s in (-1, 1):
            cyl(ec, "eq_copper", (0.12 * s, ap + 0.20, 0.45), (0.12 * s, ap + 0.20, 1.15), 0.05, small)
            cyl(ec, "eq_copper", (0.12 * s, ap + 0.16, 1.30), (0.12 * s, ap + 0.40, 1.30), 0.06, small)
    mt = P["th_mounts"] = Part("th_mounts")
    for a in (7 * math.pi / 6, 11 * math.pi / 6):
        for zz in (-1.8, 1.8):
            Rz = rot_z(a)
            box(mt, "eq_metal_band", apply(Rz, (ap + 0.14, 0, 0))[:2] + (zz,), (0.28, 0.46, 0.50), Rz)
            if lod < 2:
                lathe(mt, "eq_steel", crect(0.05, 0.11, -0.18, 0.18), small, I3, apply(Rz, (ap + 0.22, 0, 0))[:2] + (zz,))

    # --- close-inspection extras (asset LOD0): cryogenic manifold rings on both cones, a cryo line to every coil
    if close():
        hz = P["th_cryo_lines"] = Part("th_cryo_lines")
        for s in (-1, 1):
            zr = s * (kz - 0.12)
            tube(hz, "eq_steel", cyl_path(1.62, 0.0, 2 * math.pi, zr, zr, 48), 0.06, 10)
            for k in range(12):
                a = TH_FACET0 + k * math.pi / 6
                pipe_run(hz, "eq_steel", [polz(1.62, a, zr), polz(1.85, a, s * (cz + 0.05)), polz(2.12, a, s * 1.62)], 0.04, clamp_every=0.35, seg=8)
    return P


# --- smaller family members: the big unit scaled to the passport envelope, own pad side, part prefix renamed


def th_hits():
    d = TH
    R = d["cryo_r"]
    ap = R * math.cos(math.pi / 12)
    H = [hcyl("th_cryostat", (0, 0, -d["cone_z"]), (0, 0, d["cone_z"]), R)]
    for k in range(12):
        a = TH_FACET0 + k * math.pi / 6
        H.append(hbox("th_tf_coils", polz(R + 0.25, a, 0), (0.50, 0.30, 3.70), (0, 0, math.degrees(a))))
    H += [hcyl("th_flange_fore", (0, 0, -d["face_z"]), (0, 0, -d["face_z"] + 0.70), d["flange_r"]),
          hcyl("th_flange_aft", (0, 0, d["face_z"] - 0.70), (0, 0, d["face_z"]), d["flange_r"]),
          hbox("th_inverter", (ap + 0.17, 0, 0), (0.30, 0.62, 1.70))]
    for zs in (-1, 1):
        H.append(hbox("th_inverter", (ap + 0.17, 0, zs * 1.82), (0.16, 0.48, 1.94)))
    H += [hbox("th_fuel_feed", (-(ap + 0.17), 0, 0), (0.30, 0.62, 1.10)),
          hcyl("th_fuel_feed", (-(ap + 0.17), 0.31, 0), (-(ap + 0.17), 0.70, 0), 0.12),
          hbox("th_plasma_ctl", (0, -(ap + 0.15), 0), (0.62, 0.26, 1.40)),
          hbox("th_heat_pipe_column", (0, ap + 0.02, 0), (0.80, 0.16, 1.30)),
          hcyl("th_heat_pipe_column", (0, ap, 0), (0, d["pad_y0"], 0), 0.65),
          hbox("reactor_cooling_pad", (0, d["pad_y0"] + d["pad_t"] / 2 + 0.03, 0), (2 * d["pad_half"], d["pad_t"] + 0.06, 2 * d["pad_half"])),
          hbox("th_emergency_coolant", (0, ap + 0.22, 0.88), (0.44, 0.44, 1.0))]
    for a in (math.pi / 6, 5 * math.pi / 6):
        for zz in (-0.7, 0.7):
            H.append(hcyl("th_plasma_sensors", polz(ap, a, zz), polz(ap + 0.18, a, zz), 0.08))
    for a in (7 * math.pi / 6, 11 * math.pi / 6):
        for zz in (-1.8, 1.8):
            H.append(hbox("th_mounts", polz(ap + 0.14, a, zz), (0.28, 0.46, 0.50), (0, 0, math.degrees(a))))
    return H


TR26_S = 3.04 / 4.97        # TR-2.6V passport envelope / TR-10 envelope

ASSETS = {"tianhe_tr10_reactor": th_parts, "tianhe_tr26v_reactor": scaled_family(th_parts, TH, TR26_S, 7.78)}
HITS = {"tianhe_tr10_reactor": th_hits, "tianhe_tr26v_reactor": scaled_hits(th_hits, TH, TR26_S, 7.78)}
MODEL_ASSETS = {"th.reactor.tr10": "tianhe_tr10_reactor", "th.reactor.tr26v": "tianhe_tr26v_reactor"}
