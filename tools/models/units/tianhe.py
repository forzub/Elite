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



# ================================================================ Tianhe school additions: drives, loop, radiator, RCS, APUs
# Form language (DESIGN_SCHOOLS.md): flat welded plates, polygonal sections, exposed functional parts, big handles and
# rungs, olive steel + red coil lacquer; interface = 12-sided flange with 24 large bolts (drives mate the TR-10 / TR-2.6V
# flanges: busbar boss into the r 0.60 pocket, 3 terminals on r 0.30, data plug at y = -1.20, all scaled with the flange).
register_materials({
    "eq_th_steel_dark": ((0.22, 0.25, 0.20), (0.20, 0.20, 0.20), 40),
    "eq_th_csic": ((0.11, 0.11, 0.12), (0.10, 0.10, 0.10), 20),
})
_ST, _DK, _CO = "eq_th_steel", "eq_th_steel_dark", "eq_th_coil"


def _ng(part, mat, prof, n, t=(0, 0, 0), ph=None, R=I3):
    """n-gon lathe about local Z (welded flat plates); default phase puts a facet centre at angle 0"""
    ph = math.pi / n if ph is None else ph
    lathe(part, mat, prof, n, R, t, ph, ph + 2 * math.pi, smooth=False)


def _yng(part, mat, prof, n, t=(0, 0, 0), ph=None):
    """n-gon lathe about world Y (facet centre at world angle 0 = +X)"""
    ph = math.pi / n if ph is None else ph
    ylathe(part, mat, prof, n, ph, ph + 2 * math.pi, t=t, smooth=False)


def _rbox(part, mat, a, r, z, size, ch=0.0):
    """box at polar angle a about Z: size = (radial, tangential, axial)"""
    box(part, mat, polz(r, a, z), size, rot_z(a), chamfer=ch)


def _ybox(part, mat, th, r, y, size, ch=0.0, t=(0, 0, 0)):
    """box at world angle th about Y (from +X towards +Z): size = (radial, height, tangential)"""
    q = polar(r, th, y)
    box(part, mat, (q[0] + t[0], q[1] + t[1], q[2] + t[2]), size, rot_y(-th), chamfer=ch)


def _poly(part, mat, pts, hint):
    n = facet_normal(pts) or hint
    if n[0] * hint[0] + n[1] * hint[1] + n[2] * hint[2] < 0:
        n = (-n[0], -n[1], -n[2])
    part.face(mat, [part.vert(p, n) for p in pts], n)


def _extrude(part, mat, A, B):
    """a convex prism between two matching polygons A and B (gussets, wedges)"""
    m = lambda Q: tuple(sum(p[k] for p in Q) / len(Q) for k in range(3))
    ca, cb, c = m(A), m(B), m(A + B)
    _poly(part, mat, A, tuple(ca[k] - cb[k] for k in range(3)))
    _poly(part, mat, B, tuple(cb[k] - ca[k] for k in range(3)))
    for i in range(len(A)):
        j = (i + 1) % len(A)
        q = [A[i], A[j], B[j], B[i]]
        mq = m(q)
        _poly(part, mat, q, tuple(mq[k] - c[k] for k in range(3)))


def _gusset(part, mat, a, r0, r1, z0, h, t):
    """triangular welded gusset in the (r, z) plane at angle a about Z (foot r0..r1 at z0, apex at r0, z0 + h)"""
    c, s = math.cos(a), math.sin(a)
    P = lambda r, z, w: (r * c - s * w, r * s + c * w, z)
    tri = [(r0, z0), (r1, z0), (r0, z0 + h)]
    _extrude(part, mat, [P(r, z, -t / 2) for r, z in tri], [P(r, z, t / 2) for r, z in tri])


def _handle(part, mat, p, out, along, L=0.40, h=0.10, r=0.025, seg=6):
    """big U-handle / rung: two legs out of the surface at p -/+ along*L/2, a bar between"""
    a = tuple(p[k] - along[k] * L / 2 for k in range(3))
    b = tuple(p[k] + along[k] * L / 2 for k in range(3))
    a1 = tuple(a[k] + out[k] * h for k in range(3))
    b1 = tuple(b[k] + out[k] * h for k in range(3))
    cyl(part, mat, a, a1, r, seg, caps=False)
    cyl(part, mat, b, b1, r, seg, caps=False)
    cyl(part, mat, tuple(a1[k] - along[k] * r for k in range(3)), tuple(b1[k] + along[k] * r for k in range(3)), r, seg)


# ---------------------------------------------------------------- TZ-11 / TZ-16V / TZ-28V field drives
# One Tianhe drive scheme (flange - welded polygonal housing - exposed red coil packs - polygonal pad), three roles:
# TZ-11 the standard 12-gon drum with a ladder of rungs and a finned backup exchanger; TZ-16V the forced 8-gon export
# drive: narrow waist wrapped in transverse cooling fins, double-row coil crown; TZ-28V the military unit: 12-gon
# housing under bolted armour tiles, coils behind a slotted armour hood, raised armour collar round the pad.
# The pad is an n-gon whose inscribed circle = nozzle_or_emitter_diameter_m (flats on X and Y fill the envelope).
TZ11 = dict(pf="tz11", style="std", n=12, s=1.0, fr=2.30, ft=0.35, nk=1.55, rb=2.05, zb0=0.85, zc0=5.25, zc1=6.15,
            zp0=6.15, zp1=6.60, D=5.23, cout=2.45)
TZ16 = dict(pf="tz16v", style="forced", n=8, s=0.6117, fr=1.407, ft=0.22, nk=0.92, rb=0.78, rb1=0.98, zb0=0.45,
            zc0=2.62, zc1=3.27, zp0=3.27, zp1=3.59, D=2.85, cout=1.40)
TZ28 = dict(pf="tz28v", style="armoured", n=12, s=1.0, fr=2.30, ft=0.40, nk=1.75, rb=2.95, zb0=1.50, zc0=7.75, zc1=8.60,
            zp0=8.60, zp1=8.94, D=7.09, cout=3.30, collar=0.08)


def _tz_layout(D):
    """field control unit (angle, base radius, size radial/tangential/axial, centre z) and sensor spots"""
    n, st, rb = D["n"], D["style"], D["rb"]
    ap = rb * math.cos(math.pi / n)
    if st == "std":
        fc = (0.0, ap, (0.46, 1.30, 2.10), 3.0)
        sp = ([(math.pi / 3, 1.6), (2 * math.pi / 3, 1.6), (math.pi / 3, 4.5), (2 * math.pi / 3, 4.5)], ap, (0.20, 0.26, 0.26))
    elif st == "forced":
        fc = (math.pi / 2, 1.00, (0.30, 0.60, 1.15), 1.35)
        sp = ([(5 * math.pi / 4, 0.62), (7 * math.pi / 4, 0.62), (math.pi / 4, 2.25)], rb * math.cos(math.pi / n), (0.12, 0.16, 0.16))
    else:
        fc = (0.0, ap + 0.20, (0.40, 1.50, 2.40), 4.6)
        sp = ([(math.pi / 3, 2.6), (2 * math.pi / 3, 2.6), (math.pi / 3, 6.2), (2 * math.pi / 3, 6.2)], ap + 0.19, (0.24, 0.40, 0.46))
    return ap, fc, sp


def tz_parts(D, lod):
    small = small_for(lod)
    pf, n, s, st = D["pf"], D["n"], D["s"], D["style"]
    fr, ft, nk, rb = D["fr"], D["ft"], D["nk"], D["rb"]
    zc0, zc1, zp0, zp1 = D["zc0"], D["zc1"], D["zp0"], D["zp1"]
    ap, (fa, fr0, fsz, fz), (spots, sr0, ssz) = _tz_layout(D)
    fw = 2 * rb * math.sin(math.pi / n)                                   # facet width
    facets = [2 * math.pi * k / n for k in range(n)]
    verts = [math.pi / n + 2 * math.pi * k / n for k in range(n)]
    ss = max(s, 0.7)
    P = {}

    # --- flange: Tianhe 12-sided welded plate, 24 large bolts, gussets to the housing
    f = P[pf + "_flange"] = Part(pf + "_flange")
    _ng(f, _ST, [(0, 0), (fr, 0), (fr, ft), (nk, ft)], 12)
    if lod < 2:
        for k in range(12):
            _gusset(f, _ST, math.pi / 12 + k * math.pi / 6, nk - 0.02, fr - 0.14 * s, ft, 0.42 * s, 0.07 * ss)
    if lod == 0:
        for i in range(24):
            a = 2 * math.pi * (i + 0.5) / 24
            hex_bolt(f, "eq_steel", polz(fr * 0.915, a, ft), (0, 0, 1), 0.12 if s > 0.9 else 0.08, 0.08 * ss, washer=True)

    # --- busbar: power inlet boss into the reactor flange pocket, 3 terminals, data plug (SRU busbar)
    bb = P[pf + "_busbar"] = Part(pf + "_busbar")
    _ng(bb, _DK, [(0, -0.20 * s), (0.58 * s, -0.20 * s), (0.58 * s, 0.0)], 12)
    if lod < 2:
        for i in range(3):
            a = 2 * math.pi * i / 3 + math.pi / 2
            p = polz(0.30 * s, a, -0.20 * s)
            lathe(bb, "eq_insulator", [(0, -0.02 * s), (0.11 * s, -0.02 * s), (0.11 * s, 0.0)], small, I3, p)
            if lod == 0:
                cyl(bb, "eq_copper", (p[0], p[1], -0.22 * s), (p[0], p[1], -0.25 * s), 0.06 * s, small)
        box(bb, _DK, (0, -1.20 * s, -0.04 * s), (0.42 * s, 0.26 * s, 0.08 * s))

    # --- housing
    h = P[pf + "_housing"] = Part(pf + "_housing")
    rcar = D.get("rb1", rb)
    if st == "std":
        _ng(h, _ST, [(nk, ft), (nk, ft + 0.25), (rb, D["zb0"]), (rb, zc0)], n)
        belts = [(2.05, rb), (3.85, rb)]
    elif st == "forced":
        _ng(h, _ST, [(nk, ft), (rb, D["zb0"]), (rb, 2.05), (rcar, 2.40), (rcar, zc0)], n)
        belts = [(2.50, rcar)]
    else:
        _ng(h, _ST, [(nk, ft), (nk, ft + 0.30), (rb, D["zb0"]), (rb, zc0)], n)
        belts = []
    if lod < 2:
        for zz, rr in belts:
            _ng(h, _DK, crect(rr - 0.02, rr + 0.09 * ss, zz - 0.12 * ss, zz + 0.12 * ss), n)
    if lod == 0:
        if st != "forced":
            zs0 = D["zb0"] + 0.05
            for a in verts:                                               # longitudinal weld seams at the vertices
                _rbox(h, _DK, a, rb + 0.005, (zs0 + zc0) / 2, (0.05, 0.06, zc0 - zs0 - 0.05))
        if st == "std":                                                   # ladder of rungs on the top facet
            for i in range(7):
                zz = 1.30 + i * 0.52
                if min(abs(zz - 2.05), abs(zz - 3.85)) < 0.2:
                    continue
                _handle(h, "eq_steel", polz(ap, math.pi / 2, zz), (0, 1, 0), (1, 0, 0), 0.50, 0.13, 0.025, small)

    # --- coil ring: exposed red coil packs on a welded carrier (SRU coil)
    c = P[pf + "_coil_ring"] = Part(pf + "_coil_ring")
    _ng(c, _DK, crect(0.55 * rcar, rcar + 0.06 * s, zc0, zc1), n)
    rows = [(zc0 + 0.06 * s, zc1 - 0.06 * s)]
    if st == "forced":
        mid = (zc0 + zc1) / 2
        rows = [(zc0 + 0.04, mid - 0.02), (mid + 0.02, zc1 - 0.04)]
    fwc = 2 * rcar * math.sin(math.pi / n)
    w = fwc * (0.80 if st == "forced" else 0.74)
    dr = D["cout"] - rcar + 0.04
    for a in facets:
        for z0, z1 in rows:
            zm = (z0 + z1) / 2
            _rbox(c, _CO, a, D["cout"] - dr / 2, zm, (dr, w, z1 - z0), 0.04 * s if lod == 0 else 0.0)
            if lod == 0:
                _rbox(c, "eq_steel", a, D["cout"] + 0.012, zm, (0.025, w + 0.05, 0.12 * ss))          # clamp strap
                for side in (-1, 1):
                    q = apply(rot_z(a), (D["cout"] - dr / 2, side * (w / 2 + 0.02), 0))
                    box(c, "eq_steel", (q[0], q[1], zm), (dr * 0.9, 0.04, 0.12 * ss), rot_z(a))
        if lod == 0:                                                      # cable glands at the pack front faces
            q = polz(D["cout"] - dr * 0.4, a, zc0)
            cyl(c, "eq_insulator", q, (q[0], q[1], zc0 - 0.10 * s), 0.05 * ss, 8)
    if st == "forced" and lod < 2:                                       # forced: copper jumpers between the two rows
        for a in facets:
            _rbox(c, "eq_copper", a, D["cout"] + 0.005, (zc0 + zc1) / 2, (0.03, w * 0.35, 0.12))

    # --- pad = engine_hot_emitter_annulus: n-gon, replaceable trapezoid sections between radial ribs
    p = P["engine_hot_emitter_annulus"] = Part("engine_hot_emitter_annulus")
    rP = D["D"] / 2 / math.cos(math.pi / n)                                # inscribed circle = emitter diameter
    hub, rimw = 0.24 * rP, 0.085 * rP
    top = zp1 + D.get("collar", 0.0)
    _ng(p, _DK, [(0, zp0), (rP - rimw, zp0), (rP - rimw, zp0 + 0.06 * s), (0, zp0 + 0.06 * s)], n)
    _ng(p, _ST, crect(rP - rimw, rP, zp0, top, 0.02 if lod == 0 else 0.0), n)          # rim
    if lod < 2:
        g = 0.025 / rP
        for a in verts:
            lathe(p, "eq_pad_ceramic", crect(hub, rP - rimw, zp0 + 0.06 * s, zp1 - 0.03 * s), 1, I3, (0, 0, 0), a + g,
                  a + 2 * math.pi / n - g, smooth=False, cap=True)
            _rbox(p, _ST, a, (hub + rP - rimw) / 2 * math.cos(0), zp1 - 0.035 * s, (rP - rimw - hub, 0.09 * ss, 0.07 * s))
        _ng(p, _ST, [(0, zp0), (hub, zp0), (hub, zp1 + 0.03 * s), (0.6 * hub, zp1 + 0.05 * s), (0, zp1 + 0.05 * s)], n)
        if st == "armoured":
            _ng(p, _DK, crect(rP - rimw - 0.12, rP - rimw, zp1 - 0.03, top - 0.02), n)          # inner collar step
        if lod == 0:
            for a in verts:
                for k in (0.25, 0.55, 0.82):
                    hex_bolt(p, "eq_steel", polz(hub + k * (rP - rimw - hub), a, zp1 - 0.002), (0, 0, 1), 0.07, 0.04, washer=False)
            for a in facets:                                             # rim clamps, two per facet
                for dt in (-0.25, 0.25):
                    _rbox(p, "eq_steel", a + dt * 2 * math.pi / n, (rP - rimw / 2) * math.cos(math.pi / n), top + 0.015,
                          (rimw * 0.7, 0.14 * ss, 0.03))
    else:
        _ng(p, "eq_pad_ceramic", [(0, zp0 + 0.06 * s), (rP - rimw, zp0 + 0.06 * s), (rP - rimw, zp1 - 0.03 * s), (0, zp1 - 0.03 * s)], n)

    # --- field control unit (SRU field_ctl)
    fc = P[pf + "_field_ctl"] = Part(pf + "_field_ctl")
    _rbox(fc, _ST, fa, fr0 + fsz[0] / 2, fz, fsz, 0.03 if lod == 0 else 0.0)
    out = polz(1, fa, 0)
    if lod < 2:
        _rbox(fc, _DK, fa, fr0 + fsz[0] + 0.01, fz - fsz[2] * 0.08, (0.03, fsz[1] * 0.80, fsz[2] * 0.62))   # door
        for sgn in (-1, 1):                                             # conduits to the coil ring
            q = apply(rot_z(fa), (fr0 + 0.12 * ss, sgn * fsz[1] * 0.32, 0))
            cyl(fc, _DK, (q[0], q[1], fz + fsz[2] / 2), (q[0], q[1], zc0), 0.055 * ss, small)
    if lod == 0:
        for sgn in (-1, 1):                                             # big handles on the door
            q = apply(rot_z(fa), (fr0 + fsz[0] + 0.025, sgn * fsz[1] * 0.28, 0))
            _handle(fc, "eq_steel", (q[0], q[1], fz - fsz[2] * 0.08), out, (0, 0, 1), 0.36 * ss, 0.09, 0.022, small)
        for i in range(3):
            q = apply(rot_z(fa), (fr0 + fsz[0] + 0.03, -fsz[1] * 0.30 + 0.09 * i, 0))
            box(fc, "eq_status_amber", (q[0], q[1], fz + fsz[2] * 0.36), (0.012, 0.06, 0.06), rot_z(fa))
        if st == "armoured":                                            # bolted armour cover
            for sy in (-1, 1):
                for sz in (-1, 1):
                    q = apply(rot_z(fa), (fr0 + fsz[0] + 0.01, sy * fsz[1] * 0.45, 0))
                    hex_bolt(fc, "eq_steel", (q[0], q[1], fz + sz * fsz[2] * 0.45), out, 0.09, 0.05)

    # --- backup heat exchanger (SRU hx)
    hx = P[pf + "_hx"] = Part(pf + "_hx")
    if st == "std":                                                     # finned block on the -X facet
        a, z0, z1 = math.pi, 1.70, 4.10
        _rbox(hx, _DK, a, ap + 0.09, (z0 + z1) / 2, (0.18, 1.25, z1 - z0))
        if lod < 2:
            for i in range(11):
                _rbox(hx, _ST, a, ap + 0.33, z0 + 0.12 + i * (z1 - z0 - 0.24) / 10, (0.30, 1.20, 0.05))
            for yy in (-0.42, 0.42):                                    # coolant lines to the flange disconnect
                x0 = -(ap + 0.10)
                cyl(hx, "eq_steel", (x0, yy, z0), (x0, yy, 0.85), 0.075, small)
                lathe(hx, _ST, [(0, 0), (0.14, 0), (0.14, 0.05), (0, 0.05)], small, I3, (x0, yy, 0.80))
        else:
            _rbox(hx, _ST, a, ap + 0.33, (z0 + z1) / 2, (0.30, 1.20, z1 - z0 - 0.2))
    elif st == "forced":                                               # transverse fins round the waist
        zf = [0.78 + i * 0.16 for i in range(8)]
        if lod < 2:
            for zz in zf:
                _ng(hx, _ST, crect(rb - 0.01, rb + 0.30, zz, zz + 0.035), n)
            if lod == 0:
                for a in verts:                                         # tie rods through the fins
                    cyl(hx, "eq_steel", polz(rb + 0.24, a, zf[0] - 0.05), polz(rb + 0.24, a, zf[-1] + 0.09), 0.018, 6)
            for yy in (-0.24, 0.24):                                    # manifold pipes to the flange
                cyl(hx, "eq_steel", (-(rb + 0.15), yy, zf[0]), (-(rb + 0.15), yy, ft + 0.05), 0.045, small)
        else:
            _ng(hx, _ST, crect(rb, rb + 0.30, zf[0], zf[-1] + 0.035), n)
    else:                                                               # armoured louvred exchanger on -X
        a, z0, z1 = math.pi, 3.2, 6.0
        rr = ap + 0.19
        _rbox(hx, _DK, a, rr + 0.06, (z0 + z1) / 2, (0.12, 1.6, z1 - z0))
        for zz in (z0 - 0.05, z1 + 0.05):
            _rbox(hx, _ST, a, rr + 0.21, zz, (0.42, 1.7, 0.12), 0.03 if lod == 0 else 0.0)
        for sy in (-1, 1):
            q = apply(rot_z(a), (rr + 0.21, sy * 0.80, 0))
            box(hx, _ST, (q[0], q[1], (z0 + z1) / 2), (0.42, 0.10, z1 - z0), rot_z(a))
        if lod < 2:
            for i in range(9):                                          # slanted louvres
                q = apply(rot_z(a), (rr + 0.22, 0, 0))
                box(hx, _ST, (q[0], q[1], z0 + 0.20 + i * (z1 - z0 - 0.4) / 8), (0.34, 1.50, 0.05), mat_mul(rot_z(a), rot_y(-0.6)))

    # --- sensors (SRU sensor)
    sn = P[pf + "_sensors"] = Part(pf + "_sensors")
    for a, zz in spots:
        _rbox(sn, _ST if st == "armoured" else _DK, a, sr0 + ssz[0] / 2, zz, ssz, 0.05 if (lod == 0 and st == "armoured") else 0.0)
        if lod == 0:
            lathe(sn, "eq_insulator", [(0.05, 0), (0.04, 0.035), (0, 0.05)], 8, axis_to(polz(1, a, 0)), polz(sr0 + ssz[0], a, zz))

    # --- mounting lugs to the bay frame
    mt = P[pf + "_mounts"] = Part(pf + "_mounts")
    if st == "forced":                                                  # trunnion pins on X near the flange
        rr = nk * math.cos(math.pi / n)
        for sx in (-1, 1):
            a = 0.0 if sx > 0 else math.pi
            _rbox(mt, _ST, a, rr + 0.11, 0.50, (0.22, 0.36, 0.40))
            cyl(mt, "eq_steel", (sx * (rr + 0.22), 0, 0.50), (sx * (rr + 0.38), 0, 0.50), 0.11, small)
            if lod < 2:
                _gusset(mt, _ST, a, rr - 0.02, rr + 0.22, 0.70, 0.30, 0.06)
    else:
        rr = ap + (0.19 if st == "armoured" else 0.0)
        for a in (7 * math.pi / 6, 11 * math.pi / 6):
            for zz in ((1.4, 4.4) if st == "std" else (2.4, 6.6)):
                _rbox(mt, _ST, a, rr + 0.16, zz, (0.32, 0.52, 0.56))
                if lod < 2:
                    lathe(mt, "eq_steel", crect(0.06, 0.13, -0.30, 0.30), small, I3, polz(rr + 0.24, a, zz))
                    for dz in (-0.36, 0.36):
                        _rbox(mt, _ST, a, rr + 0.10, zz + dz, (0.20, 0.06, 0.16))

    # --- armour (TZ-28V): bolted armour tiles on the housing facets, slotted hood over the coil ring
    if st == "armoured":
        ar = P[pf + "_armour"] = Part(pf + "_armour")
        for a in facets:
            for z0, z1 in ((1.75, 4.55), (4.65, 7.55)):
                _rbox(ar, _ST, a, ap + 0.10, (z0 + z1) / 2, (0.20, fw * 0.96, z1 - z0 - 0.08), 0.05 if lod == 0 else 0.0)
                if lod == 0:
                    for sy in (-1, 1):
                        for zz in (z0 + 0.18, (z0 + z1) / 2, z1 - 0.18):
                            q = apply(rot_z(a), (ap + 0.20, sy * fw * 0.38, 0))
                            hex_bolt(ar, "eq_steel", (q[0], q[1], zz), polz(1, a, 0), 0.09, 0.05)
        for z0, z1 in ((zc0 - 0.02, zc0 + 0.12), (zc1 - 0.12, zc1 - 0.02)):        # hood rings
            _ng(ar, _ST, crect(rb - 0.02, D["cout"] + 0.10, z0, z1), n)
        for a in verts:                                                 # hood bars over the gaps between the packs
            _rbox(ar, _ST, a, (rb + D["cout"]) / 2 + 0.05, (zc0 + zc1) / 2, (D["cout"] - rb + 0.10, 0.34, zc1 - zc0 - 0.10))
        if lod < 2:
            for a in facets:                                            # one slat across every window
                _rbox(ar, _ST, a, D["cout"] + 0.06, (zc0 + zc1) / 2, (0.08, fwc * 0.8, 0.10))

    # --- close-inspection extras: harness ring with drops to the coil glands, coolant lines, armoured conduits
    if close():
        hz = P[pf + "_harness"] = Part(pf + "_harness")
        rr = rcar + 0.10 * ss + (0.20 if st == "armoured" else 0.0)
        zr = zc0 - (0.30 if st != "forced" else 0.20)
        harness(hz, cyl_path(rr, 0.0, 2 * math.pi, zr, zr, 48), 0.022 * ss, 3, ("eq_insulator", _DK), tie_every=4)
        for a in facets:
            g = polz(D["cout"] - dr * 0.4, a, zc0 - 0.10 * s)
            harness(hz, bezier(polz(rr, a, zr), polz(rr + 0.05, a, zr + 0.05), (g[0], g[1], zr + 0.10), g, 6),
                    0.015 * ss, 2, ("eq_insulator",), tie_every=10)
        q = apply(rot_z(fa), (fr0 + 0.10, 0.0, 0))
        harness(hz, bezier((q[0], q[1], fz + fsz[2] / 2), (q[0], q[1], zr - 0.3), polz(rr, fa + 0.2, zr - 0.2), polz(rr, fa + 0.3, zr), 10),
                0.03 * ss, 4, ("eq_insulator", _DK))
        if st == "std":
            for sy in (-1, 1):
                pipe_run(hz, "eq_steel", [(-(ap + 0.10), sy * 0.62, 4.10), (-(ap + 0.10), sy * 0.62, 4.6),
                                          polz(rb + 0.06, math.pi - sy * 0.45, 4.95)], 0.045, clamp_every=0.4, seg=8)
        if st == "armoured":                                            # conduits in armoured channels
            for a in (math.pi / 3 + 0.16, 2 * math.pi / 3 - 0.16):
                _rbox(hz, _DK, a, ap + 0.25, 4.4, (0.10, 0.16, 4.6))
    return P


def tz_hits(D):
    def fn():
        pf, n, s, st = D["pf"], D["n"], D["s"], D["style"]
        rb, ft, zc0, zc1, zp0, zp1 = D["rb"], D["ft"], D["zc0"], D["zc1"], D["zp0"], D["zp1"]
        ap, (fa, fr0, fsz, fz), (spots, sr0, ssz) = _tz_layout(D)
        rcar = D.get("rb1", rb)
        H = [hcyl(pf + "_busbar", (0, 0, -0.20 * s), (0, 0, 0), 0.58 * s),
             hcyl(pf + "_flange", (0, 0, 0), (0, 0, ft), D["fr"] * 0.97)]
        if st == "forced":
            H += [hcyl(pf + "_housing", (0, 0, ft), (0, 0, 2.2), rb), hcyl(pf + "_housing", (0, 0, 2.2), (0, 0, zc0), rcar),
                  hcyl(pf + "_hx", (0, 0, 0.78), (0, 0, 1.94), rb + 0.28)]
        else:
            H.append(hcyl(pf + "_housing", (0, 0, ft), (0, 0, zc0), ap))
        H += [hcyl(pf + "_coil_ring", (0, 0, zc0), (0, 0, zc1), D["cout"]),
              hcyl("engine_hot_emitter_annulus", (0, 0, zp0), (0, 0, zp1 + D.get("collar", 0.0)), D["D"] / 2)]
        if st == "std":
            H.append(hbox(pf + "_hx", (-(ap + 0.24), 0, 2.9), (0.48, 1.25, 2.40)))
        elif st == "armoured":
            H += [hbox(pf + "_hx", (-(ap + 0.40), 0, 4.6), (0.42, 1.7, 3.0)),
                  hcyl(pf + "_armour", (0, 0, 1.75), (0, 0, 7.55), ap + 0.20)]
        H.append(hbox(pf + "_field_ctl", polz(fr0 + fsz[0] / 2, fa, fz), fsz, (0, 0, math.degrees(fa))))
        for a, zz in spots:
            H.append(hbox(pf + "_sensors", polz(sr0 + ssz[0] / 2, a, zz), ssz, (0, 0, math.degrees(a))))
        if st == "forced":
            rr = D["nk"] * math.cos(math.pi / n)
            for sx in (-1, 1):
                H.append(hbox(pf + "_mounts", (sx * (rr + 0.19), 0, 0.50), (0.38, 0.36, 0.40)))
        else:
            rr = ap + (0.19 if st == "armoured" else 0.0)
            for a in (7 * math.pi / 6, 11 * math.pi / 6):
                for zz in ((1.4, 4.4) if st == "std" else (2.4, 6.6)):
                    H.append(hbox(pf + "_mounts", polz(rr + 0.17, a, zz), (0.34, 0.52, 0.88), (0, 0, math.degrees(a))))
        return H
    return fn


ASSETS.update({"tianhe_tz11_drive": lambda lod: tz_parts(TZ11, lod), "tianhe_tz16v_drive": lambda lod: tz_parts(TZ16, lod),
               "tianhe_tz28v_drive": lambda lod: tz_parts(TZ28, lod)})
HITS.update({"tianhe_tz11_drive": tz_hits(TZ11), "tianhe_tz16v_drive": tz_hits(TZ16), "tianhe_tz28v_drive": tz_hits(TZ28)})
MODEL_ASSETS.update({"th.drive.tz11": "tianhe_tz11_drive", "th.drive.tz16v": "tianhe_tz16v_drive", "th.drive.tz28": "tianhe_tz28v_drive"})


RX = axis_to((1, 0, 0))


def _Rbox(part, mat, R, t, a, r, z, size, ch=0.0):
    """box at polar (a, r, z) of a lathe frame (R, t): size = (radial, tangential, axial)"""
    q = apply(R, polz(r, a, z))
    box(part, mat, (q[0] + t[0], q[1] + t[1], q[2] + t[2]), size, mat_mul(R, rot_z(a)), chamfer=ch)


def _ygusset(part, mat, th, r0, r1, y0, h, t):
    """vertical triangular gusset at world angle th about Y (foot r0..r1 at y0, apex at r0, y0 + h)"""
    tx, tz = -math.sin(th), math.cos(th)
    P = lambda r, y, w: (r * math.cos(th) + tx * w, y, r * math.sin(th) + tz * w)
    tri = [(r0, y0), (r1, y0), (r0, y0 + h)]
    _extrude(part, mat, [P(r, y, -t / 2) for r, y in tri], [P(r, y, t / 2) for r, y in tri])


def _wheel(part, mat, c, r, lod):
    """red hand wheel about Y: rim ring and three spokes"""
    _yng(part, mat, crect(r - 0.03, r, -0.015, 0.015), 12 if lod < 2 else 8, t=c)
    if lod < 2:
        for k in range(3):
            th = k * math.pi / 3
            box(part, mat, c, (2 * r - 0.04, 0.02, 0.025), rot_y(th))


def _tflange(part, mat, R, t, r, th, lod, bolt=0.05):
    """small Tianhe 12-sided pipe / bus flange, axis = R local Z, face at t"""
    _ng(part, mat, [(0, 0), (r, 0), (r, th), (0, th)], 12, t=t, R=R)
    if lod == 0 and close():
        for i in range(8):
            q = apply(R, polz(r * 0.78, 2 * math.pi * (i + 0.5) / 8, th))
            hex_bolt(part, "eq_steel", (q[0] + t[0], q[1] + t[1], q[2] + t[2]), apply(R, (0, 0, 1)), bolt, bolt * 0.6)


# ---------------------------------------------------------------- KO-600 coolant loop skid (th.cool.loop600)
# Envelope 3.557 x 1.778 x 1.778 (X long), open welded skid on y = 0, everything exposed and hand-serviceable:
# 12-gon finned pump motor + volute (SRU pump), 8-gon expansion tank (SRU fluid), two headers with Tianhe 12-gon
# flanges at both X ends + risers (SRU pipes), three gate valves with big red hand wheels (SRU valves).
KO_HY, KO_HZ = 1.45, 0.35                                              # header height, header z (-: supply, +: return)


def ko_parts(lod):
    small = small_for(lod)
    L = 3.557
    hx = L / 2
    P = {}
    sk = P["ko600_skid"] = Part("ko600_skid")
    for sz in (-1, 1):                                                  # two welded I-beams along X
        z = sz * 0.76
        box(sk, _DK, (0, 0.015, z), (L, 0.03, 0.24))
        box(sk, _DK, (0, 0.11, z), (L, 0.16, 0.035))
        box(sk, _DK, (0, 0.205, z), (L, 0.03, 0.24))
    for x in (-1.62, -0.55, 0.55, 1.62):
        box(sk, _DK, (x, 0.11, 0), (0.16, 0.16, 1.28))
    for x in (-1.70, 1.40):                                             # portal frames carrying the headers
        for sz in (-1, 1):
            box(sk, _ST, (x, (0.22 + 1.34) / 2, sz * 0.60), (0.10, 1.12, 0.10))
        box(sk, _ST, (x, 1.29, 0), (0.12, 0.10, 1.30))
    if lod < 2:
        box(sk, _ST, (-0.85, 0.23, -0.35), (1.55, 0.02, 0.86))                     # pump deck plate
        for x in (-1.70, 1.40):
            for sz in (-1, 1):
                box(sk, _ST, (x, 0.32, sz * 0.52), (0.04, 0.18, 0.16))
    if lod == 0:
        for x in (-hx + 0.12, hx - 0.12):                               # lifting eyes on the beam ends
            for sz in (-1, 1):
                lathe(sk, "eq_steel", crect(0.04, 0.08, -0.02, 0.02), small, RX, (x, 0.30, sz * 0.76))
                box(sk, "eq_steel", (x, 0.25, sz * 0.76), (0.04, 0.06, 0.10))
        for x in (-1.2, 0.0, 1.2):                                      # foot bolts on the bottom flanges
            for sz in (-1, 1):
                for dz in (-0.08, 0.08):
                    hex_bolt(sk, "eq_steel", (x, 0.03, sz * 0.76 + dz), (0, 1, 0), 0.07, 0.04, washer=True)

    # --- pump: finned 12-gon motor, coupling guard, volute (SRU pump)
    pu = P["ko600_pump"] = Part("ko600_pump")
    mc = (-1.55, 0.60, -0.35)
    _ng(pu, _ST, [(0, 0), (0.30, 0), (0.36, 0.05), (0.36, 0.88), (0.30, 0.93), (0, 0.93)], 12, t=mc, R=RX)
    if lod < 2:
        for k in range(12):
            if k in (8, 9, 10):                                         # no fins over the feet
                continue
            _Rbox(pu, _DK, RX, mc, math.pi / 12 + k * math.pi / 6, 0.40, 0.47, (0.09, 0.03, 0.78))
    for x in (-1.42, -0.78):
        box(pu, _DK, (x, 0.28, -0.35), (0.12, 0.10, 0.62))
    box(pu, _DK, (-0.56, 0.60, -0.35), (0.14, 0.44, 0.44))                          # coupling guard
    _ng(pu, _ST, [(0, 0), (0.38, 0), (0.38, 0.34), (0, 0.34)], 12, t=(-0.50, 0.62, -0.35), R=RX)   # volute
    if lod < 2:
        _ng(pu, _CO, crect(0.0, 0.10, 0.34, 0.40), 8, t=(-0.50, 0.62, -0.35), R=RX)              # inlet eye cap
        box(pu, "eq_status_amber" if lod == 0 else _DK, (-1.10, 0.97, -0.35), (0.20, 0.02, 0.12))  # terminal box lid
        box(pu, _DK, (-1.10, 0.93, -0.35), (0.26, 0.08, 0.20))
    if lod == 0:
        _handle(pu, "eq_steel", (-1.10, 0.97, -0.35), (0, 1, 0), (0, 0, 1), 0.14, 0.05, 0.015, 6)
        for i in range(8):
            q = apply(RX, polz(0.33, 2 * math.pi * (i + 0.5) / 8, 0.34))
            hex_bolt(pu, "eq_steel", (q[0] - 0.50, q[1] + 0.62, q[2] - 0.35), (1, 0, 0), 0.07, 0.04)

    # --- expansion tank on saddles (SRU fluid: coolant charge, fill cap, sight glass)
    fl = P["ko600_fluid"] = Part("ko600_fluid")
    tc = (-1.40, 0.70, 0.42)
    _ng(fl, _ST, [(0, 0), (0.22, 0), (0.40, 0.18), (0.40, 2.0), (0.22, 2.18), (0, 2.18)], 8, t=tc, R=RX)
    for x in (-0.95, 0.35):
        box(fl, _DK, (x, 0.28, 0.42), (0.16, 0.14, 0.66))
        if lod < 2:
            _ng(fl, _DK, crect(0.39, 0.45, -0.05, 0.05), 8, t=(x, 0.70, 0.42), R=RX)
    if lod < 2:
        cyl(fl, _ST, (-0.6, 1.08, 0.42), (-0.6, 1.20, 0.42), 0.08, small)
        cyl(fl, _CO, (-0.6, 1.20, 0.42), (-0.6, 1.25, 0.42), 0.10, small)          # fill cap
    if lod == 0:
        box(fl, "eq_status_amber", (-0.4, 0.70, 0.42 + 0.375), (0.9, 0.07, 0.02))     # sight glass
        for x in (-0.86, 0.06):
            box(fl, "eq_steel", (x, 0.70, 0.42 + 0.385), (0.04, 0.12, 0.03))

    # --- pipes: two headers with flanges at both ends, discharge and suction risers, tank line (SRU pipes)
    pp = P["ko600_pipes"] = Part("ko600_pipes")
    for sz in (-1, 1):
        cyl(pp, _ST, (-hx + 0.06, KO_HY, sz * KO_HZ), (hx - 0.06, KO_HY, sz * KO_HZ), 0.11, small)
        _tflange(pp, _ST, RX, (hx - 0.06, KO_HY, sz * KO_HZ), 0.22, 0.06, lod)
        _tflange(pp, _ST, rot_y(-math.pi / 2), (-hx + 0.06, KO_HY, sz * KO_HZ), 0.22, 0.06, lod)
    cyl(pp, _ST, (-0.32, 0.98, -KO_HZ), (-0.32, KO_HY, -KO_HZ), 0.10, small)          # discharge riser
    sx = 0.30
    for a, b in (((-0.14, 0.62, -KO_HZ), (sx, 0.62, -KO_HZ)), ((sx, 0.62, -KO_HZ), (sx, 1.15, -KO_HZ)),
                 ((sx, 1.15, -KO_HZ), (sx, 1.15, KO_HZ)), ((sx, 1.15, KO_HZ), (sx, KO_HY, KO_HZ))):
        cyl(pp, _ST, a, b, 0.09, small)                                 # suction line from the return header
    cyl(pp, _ST, (0.62, 1.04, 0.40), (0.62, KO_HY, 0.40), 0.06, small)             # tank line
    if lod < 2:
        for c in ((sx, 0.62, -KO_HZ), (sx, 1.15, -KO_HZ), (sx, 1.15, KO_HZ)):     # welded elbows (blocks)
            box(pp, _ST, c, (0.20, 0.20, 0.20))

    # --- valves: gate valves with flanged bodies and big red hand wheels (SRU valves)
    vv = P["ko600_valves"] = Part("ko600_valves")
    for x, sz in ((0.70, -1), (1.10, -1), (-0.95, 1)):
        z = sz * KO_HZ
        box(vv, _ST, (x, KO_HY, z), (0.26, 0.30, 0.30))
        for dx in (-0.15, 0.15):
            _ng(vv, _ST, crect(0.0, 0.17, -0.025, 0.025), 12, t=(x + dx, KO_HY, z), R=RX)
        cyl(vv, _DK, (x, KO_HY + 0.15, z), (x, 1.68, z), 0.05, small)
        _wheel(vv, _CO, (x, 1.72, z), 0.16, lod)
    return P


def ko_hits():
    hx = 3.557 / 2
    H = [hbox("ko600_skid", (0, 0.11, sz * 0.76), (3.557, 0.22, 0.24)) for sz in (-1, 1)]
    H += [hbox("ko600_skid", (x, 0.78, 0), (0.12, 1.12, 1.30)) for x in (-1.70, 1.40)]
    H += [hcyl("ko600_pump", (-1.55, 0.60, -0.35), (-0.62, 0.60, -0.35), 0.40),
          hcyl("ko600_pump", (-0.62, 0.62, -0.35), (-0.16, 0.62, -0.35), 0.38),
          hcyl("ko600_fluid", (-1.40, 0.70, 0.42), (0.78, 0.70, 0.42), 0.40)]
    for sz in (-1, 1):
        H.append(hcyl("ko600_pipes", (-hx, KO_HY, sz * KO_HZ), (hx, KO_HY, sz * KO_HZ), 0.13))
    H += [hbox("ko600_pipes", (-0.32, 1.22, -KO_HZ), (0.20, 0.48, 0.20)),
          hbox("ko600_pipes", (0.30, 0.90, 0.0), (0.20, 0.60, 0.90))]
    for x, sz in ((0.70, -1), (1.10, -1), (-0.95, 1)):
        H.append(hbox("ko600_valves", (x, 1.55, sz * KO_HZ), (0.34, 0.40, 0.34)))
    return H


# ---------------------------------------------------------------- RS-50 radiator section (th.rad.panel50)
# 5 x 10 m face (= 50 m2 radiating area), 0.15 m thick, on the hull skin: outward +Y, mounting face y = 0. Welded
# olive rib grid holding eight flat C-SiC plates (SRU panel); manifold pipe along the -X long edge with a feeder per
# plate row (SRU manifold); three hinge knuckles and the deploy drive on the same edge (SRU hinge).
def rs_parts(lod):
    small = small_for(lod)
    P = {}
    x0, x1, zh = -2.20, 2.50, 4.98
    xm = (x0 + x1) / 2
    pn = P["rs50_panel"] = Part("rs50_panel")
    rows = [-zh + k * (2 * zh) / 4 for k in range(5)]
    if lod < 2:
        box(pn, _DK, ((x0 + x1) / 2, 0.025, 0), (x1 - x0, 0.02, 2 * zh))                # back sheet
        for (a, b) in ((x0 + 0.05, xm - 0.05), (xm + 0.05, x1 - 0.05)):
            for z0, z1 in zip(rows, rows[1:]):
                box(pn, "eq_th_csic", ((a + b) / 2, 0.06, (z0 + z1) / 2), (b - a, 0.05, z1 - z0 - 0.10))
                if lod == 0:                                            # heat-pipe ridges across each plate
                    for i in range(7):
                        zz = z0 + 0.25 + i * (z1 - z0 - 0.5) / 6
                        cyl(pn, _DK, (a + 0.06, 0.088, zz), (b - 0.06, 0.088, zz), 0.012, 6, caps=False)
        for x in (x0 + 0.04, xm, x1 - 0.04):
            box(pn, _ST, (x, 0.075, 0), (0.09, 0.11, 2 * zh), chamfer=0.02 if lod == 0 else 0.0)
        for z in rows:
            box(pn, _ST, ((x0 + x1) / 2, 0.075, z), (x1 - x0, 0.105, 0.09))
        if lod == 0:
            for z in rows:
                for i in range(12):
                    x = x0 + 0.2 + i * (x1 - x0 - 0.4) / 11
                    hex_bolt(pn, "eq_steel", (x, 0.128, z), (0, 1, 0), 0.06, 0.02, washer=False)
    else:
        box(pn, "eq_th_csic", ((x0 + x1) / 2, 0.06, 0), (x1 - x0, 0.07, 2 * zh))
        for x in (x0 + 0.04, x1 - 0.04):
            box(pn, _ST, (x, 0.075, 0), (0.09, 0.11, 2 * zh))

    mf = P["rs50_manifold"] = Part("rs50_manifold")
    xp = -2.33
    cyl(mf, _ST, (xp, 0.075, -zh), (xp, 0.075, zh - 0.15), 0.065, small)
    for sz in (-0.25, 0.25):                                            # inlet / outlet stubs at the -Z end
        _ng(mf, _ST, crect(0.0, 0.07, -0.04, 0.0), 12, t=(xp + sz * 0.0, 0.075, -zh), R=I3)
    if lod < 2:
        for z0, z1 in zip(rows, rows[1:]):
            zz = (z0 + z1) / 2
            box(mf, _DK, (xp, 0.075, zz), (0.15, 0.14, 0.24))
            cyl(mf, _ST, (xp + 0.07, 0.075, zz), (x0 + 0.05, 0.075, zz), 0.035, small)
            if lod == 0:
                box(mf, _CO, (xp, 0.146, zz), (0.10, 0.004, 0.10))       # red isolation tag plate
        box(mf, _DK, (xp, 0.075, zh - 0.12), (0.14, 0.14, 0.12))

    hg = P["rs50_hinge"] = Part("rs50_hinge")
    for zz in (-3.4, 0.0, 3.4):
        box(hg, _ST, (-2.30, 0.02, zz), (0.30, 0.04, 0.50))
        cyl(hg, "eq_steel", (-2.45, 0.07, zz - 0.25), (-2.45, 0.07, zz + 0.25), 0.065, small)
        if lod < 2:
            for dz in (-0.12, 0.12):
                box(hg, _DK, (-2.42, 0.07, zz + dz), (0.10, 0.10, 0.08))
    box(hg, _DK, (-2.40, 0.065, -4.45), (0.16, 0.13, 0.70))              # deploy drive
    if lod < 2:
        cyl(hg, _ST, (-2.40, 0.065, -4.10), (-2.40, 0.065, -3.65), 0.04, small)
    if lod == 0:
        for dz in (-0.25, 0.25):
            hex_bolt(hg, "eq_steel", (-2.40, 0.13, -4.45 + dz), (0, 1, 0), 0.06, 0.02, washer=False)
    if close():
        hz = P["rs50_harness"] = Part("rs50_harness")
        harness(hz, [(-2.40, 0.10, -4.10), (-2.38, 0.12, -3.2), (-2.36, 0.13, 0.0), (-2.36, 0.13, 3.0)], 0.012, 2,
                ("eq_insulator", _DK), tie_every=1)
    return P


def rs_hits():
    return [hbox("rs50_panel", (0.15, 0.065, 0), (4.70, 0.11, 9.96)),
            hcyl("rs50_manifold", (-2.33, 0.075, -4.98), (-2.33, 0.075, 4.98), 0.075),
            hbox("rs50_hinge", (-2.40, 0.065, -4.45), (0.16, 0.13, 0.70))] + \
           [hcyl("rs50_hinge", (-2.45, 0.07, zz - 0.25), (-2.45, 0.07, zz + 0.25), 0.07) for zz in (-3.4, 0.0, 3.4)]


# ---------------------------------------------------------------- RCS-4T heavy RCS cluster (std.rcs.cluster_h)
# 1.817 m cube on the hull skin (outward +Y): Tianhe 12-gon base flange with 24 bolts, welded 8-gon column and head,
# four field nozzles on +-X / +-Z: red 8-gon coil collars (SRU coil) round ceramic emitter discs (SRU emitter);
# the pulse switch box on a diagonal facet (SRU driver). No bells: field nozzles, like the drives.
RC_Y = 1.29                                                             # nozzle axis height


def rcs_parts(lod):
    small = small_for(lod)
    P = {}
    b = P["rcsh_body"] = Part("rcsh_body")
    rfl = 0.90 / math.cos(math.pi / 12)
    _yng(b, _ST, [(0, 0), (rfl, 0), (rfl, 0.10), (0.46, 0.10)], 12)
    _yng(b, _ST, [(0.455, 0.10), (0.455, 0.95), (0.58, 1.02), (0.58, 1.55), (0.48, 1.62), (0, 1.62)], 8)
    if lod < 2:
        for k in range(8):
            _ygusset(b, _ST, math.pi / 8 + k * math.pi / 4, 0.44, 0.80, 0.10, 0.42, 0.05)
        _yng(b, _DK, crect(0.57, 0.64, 1.02, 1.10), 8)
        lathe(b, "eq_steel", crect(0.05, 0.09, -0.025, 0.025), small, RX, (0, 1.70, 0))           # lifting eye
        box(b, _ST, (0, 1.64, 0), (0.06, 0.06, 0.16))
    if lod == 0:
        for i in range(24):
            hex_bolt(b, "eq_steel", polar(0.80, 2 * math.pi * (i + 0.5) / 24, 0.10), (0, 1, 0), 0.07, 0.045, washer=True)
    co = P["rcsh_coils"] = Part("rcsh_coils")
    em = P["rcsh_emitters"] = Part("rcsh_emitters")
    for th in (0.0, math.pi / 2, math.pi, 3 * math.pi / 2):
        R = axis_to(polar(1, th, 0))
        c = (0, RC_Y, 0)
        _ng(co, _CO, crect(0.16, 0.30, 0.58, 0.75, 0.015 if lod == 0 else 0.0), 8, t=c, R=R)
        _ng(em, _ST, crect(0.24, 0.29, 0.75, 0.87), 8, t=c, R=R)                       # emitter clamp ring
        _ng(em, "eq_pad_ceramic", [(0, 0.75), (0.24, 0.75), (0.24, 0.85), (0, 0.85)], 8, t=c, R=R)
        if lod < 2:
            _ng(em, _ST, [(0, 0.85), (0.07, 0.85), (0.07, 0.88), (0, 0.88)], 8, t=c, R=R)  # hub
        if lod == 0:
            for k in range(8):                                          # coil clamp straps
                _Rbox(co, "eq_steel", R, c, 2 * math.pi * k / 8, 0.305, 0.665, (0.02, 0.06, 0.20))
            for k in range(4):
                q = apply(R, polz(0.265, 2 * math.pi * k / 4 + math.pi / 4, 0.87))
                hex_bolt(em, "eq_steel", (q[0] + c[0], q[1] + c[1], q[2] + c[2]), polar(1, th, 0), 0.04, 0.02, washer=False)
    dv = P["rcsh_driver"] = Part("rcsh_driver")
    th = math.pi / 4
    _ybox(dv, _ST, th, 0.42 + 0.16, 0.55, (0.32, 0.62, 0.44), 0.02 if lod == 0 else 0.0)
    if lod < 2:
        _ybox(dv, _DK, th, 0.42 + 0.33, 0.55, (0.02, 0.50, 0.34))                       # door
        for sgn in (-1, 1):                                             # conduits up to the head
            q = polar(0.62, th + sgn * 0.25, 0)
            cyl(dv, _DK, (q[0], 0.86, q[2]), (q[0], 1.02, q[2]), 0.035, small)
    if lod == 0:
        q = polar(0.76, th, 0.55)
        _handle(dv, "eq_steel", q, polar(1, th, 0), (0, 1, 0), 0.28, 0.07, 0.018, small)
        for i in range(2):
            q = polar(0.765, th + 0.20, 0.72 - 0.06 * i)
            box(dv, "eq_status_amber", q, (0.012, 0.035, 0.05), rot_y(-th))
    if close():
        hz = P["rcsh_harness"] = Part("rcsh_harness")
        for th2 in (0.0, math.pi / 2, math.pi, 3 * math.pi / 2):
            g = polar(0.50, th2 + 0.35, 1.10)
            harness(hz, bezier(polar(0.62, th + (0.25 if th2 in (0.0, 3 * math.pi / 2) else -0.25), 1.02), polar(0.66, th, 1.15),
                               (g[0] * 1.2, 1.10, g[2] * 1.2), polar(0.62, th2 + 0.42, RC_Y - 0.12), 10), 0.015, 2,
                    ("eq_insulator", _DK), tie_every=5)
    return P


def rcs_hits():
    H = [hcyl("rcsh_body", (0, 0, 0), (0, 0.10, 0), 0.90), hcyl("rcsh_body", (0, 0.10, 0), (0, 1.02, 0), 0.44),
         hcyl("rcsh_body", (0, 1.02, 0), (0, 1.62, 0), 0.56)]
    for th in (0.0, math.pi / 2, math.pi, 3 * math.pi / 2):
        d = polar(1, th, 0)
        if abs(d[2]) < 0.5:
            H.append(hcyl("rcsh_coils", (0.58 * d[0], RC_Y, 0), (0.75 * d[0], RC_Y, 0), 0.30))
            H.append(hcyl("rcsh_emitters", (0.75 * d[0], RC_Y, 0), (0.88 * d[0], RC_Y, 0), 0.29))
        else:
            H.append(hcyl("rcsh_coils", (0, RC_Y, 0.58 * d[2]), (0, RC_Y, 0.75 * d[2]), 0.30))
            H.append(hcyl("rcsh_emitters", (0, RC_Y, 0.75 * d[2]), (0, RC_Y, 0.88 * d[2]), 0.29))
    H.append(hbox("rcsh_driver", polar(0.58, math.pi / 4, 0.55), (0.32, 0.62, 0.44), (0, -45.0, 0)))
    return H


# ---------------------------------------------------------------- SR-150 / SR-600 service reactors (th.apu.s150 / s600)
# Cube envelopes 1.896 / 2.735 m. A squat welded n-gon vessel standing on a skid with exposed red poloidal hoops,
# racks of hand-pulled energy cells on +-X (SRU cells), a 12-gon pulse starter drum on -Z (SRU starter), the bus
# regulator cabinet on +Z with a Tianhe 12-gon bus outlet (SRU regulator), sensor heads on the shoulders (SRU sensor),
# a short heat-pipe column and a square cooling pad as the top face. The passport has no cooling_pad_side_m: the pad
# side = the envelope width (waste heat ~200 / 760 MW -> ~57 / 100 MW/m2, same order as the ER-5 / TR-10 pads).
# SR-600 is not a scaled SR-150: 12-gon vessel, four hoops, three-tier cell racks, twin starter drums.
S150 = dict(pf="s150", E=1.896, n=8, hoops=3, tiers=2, starters=1)
S600 = dict(pf="s600", E=2.735, n=12, hoops=4, tiers=3, starters=2)


def _sr_dims(D):
    E = D["E"]
    h = E / 2
    rv = 0.27 * E
    yv0, yv1 = -h + 0.075 * E, h - 0.26 * E
    return E, h, rv, yv0, yv1


def sr_parts(D, lod):
    small = small_for(lod)
    pf, n = D["pf"], D["n"]
    E, h, rv, yv0, yv1 = _sr_dims(D)
    k = E / 1.896
    ap = rv * math.cos(math.pi / n)
    P = {}
    sk = P[pf + "_skid"] = Part(pf + "_skid")
    for sz in (-1, 1):
        box(sk, _DK, (0, -h + 0.035 * E, sz * 0.36 * E), (E - 0.02, 0.07 * E, 0.09 * E))
    for sx in (-1, 1):
        box(sk, _DK, (sx * 0.30 * E, -h + 0.035 * E, 0), (0.08 * E, 0.06 * E, 0.66 * E))
    if lod == 0:
        for sx in (-1, 1):
            for sz in (-1, 1):
                lathe(sk, "eq_steel", crect(0.03 * k, 0.06 * k, -0.015, 0.015), small, RX, (sx * (h - 0.06 * k), -h + 0.10 * E, sz * 0.36 * E))

    v = P[pf + "_vessel"] = Part(pf + "_vessel")
    _yng(v, _ST, [(0, yv0), (rv * 0.7, yv0), (rv, yv0 + 0.07 * E), (rv, yv1 - 0.07 * E), (rv * 0.62, yv1), (0, yv1)], n)
    if lod < 2:
        for th in [2 * math.pi * i / 6 + math.pi / 6 for i in range(6)]:
            _ygusset(v, _ST, th, rv * 0.7, rv + 0.06 * E, yv0 - 0.005, 0.10 * E, 0.035 * k)
    nh = D["hoops"]
    for i in range(nh):                                                 # exposed red poloidal hoops
        yy = yv0 + 0.12 * E + i * (yv1 - yv0 - 0.30 * E) / (nh - 1)
        _yng(v, _CO, crect(rv - 0.01, rv + 0.055 * E, yy, yy + 0.05 * E, 0.01 if lod == 0 else 0.0), n)
    if lod == 0:
        for i in range(n):                                              # vertical weld seams
            th = math.pi / n + 2 * math.pi * i / n
            _ybox(v, _DK, th, rv + 0.003, (yv0 + yv1) / 2, (0.02, yv1 - yv0 - 0.16 * E, 0.025))

    # --- cells (+-X racks): frame and hand-pulled cell modules with big handles
    ce = P[pf + "_cells"] = Part(pf + "_cells")
    x0, x1 = ap + 0.015 * E, h - 0.045 * E
    cy0, cy1 = yv0 + 0.02 * E, yv1 - 0.17 * E
    nz, ny = 3, D["tiers"]
    for sx in (-1, 1):
        box(ce, _DK, (sx * (x0 + 0.02 * E), (cy0 + cy1) / 2, 0), (0.04 * E, cy1 - cy0, 0.70 * E))      # back plate
        for zz in (-0.35 * E, 0.35 * E):
            box(ce, _DK, (sx * (x0 + x1) / 2, (cy0 + cy1) / 2, zz), (x1 - x0, cy1 - cy0, 0.03 * E))     # side posts
        if lod < 2:
            for j in range(ny):
                for i in range(nz):
                    cz = -0.335 * E + (i + 0.5) * 0.67 * E / nz
                    cyc = cy0 + (j + 0.5) * (cy1 - cy0) / ny
                    dz, dy = 0.67 * E / nz - 0.035 * E, (cy1 - cy0) / ny - 0.03 * E
                    box(ce, _ST, (sx * (x0 + x1 + 0.02 * E) / 2, cyc, cz), (x1 - x0 - 0.04 * E, dy, dz), chamfer=0.012 if lod == 0 else 0.0)
                    if lod == 0:
                        _handle(ce, "eq_steel", (sx * x1, cyc, cz), (sx, 0, 0), (0, 0, 1), dz * 0.55, 0.05 * k, 0.012 * k, 6)
                        box(ce, _CO, (sx * (x1 + 0.002), cyc + dy * 0.36, cz), (0.004, dy * 0.12, dz * 0.8))
        else:
            box(ce, _ST, (sx * (x0 + x1 + 0.02 * E) / 2, (cy0 + cy1) / 2, 0), (x1 - x0 - 0.04 * E, cy1 - cy0, 0.66 * E))

    # --- starter: 12-gon pulse capacitor drum(s) along X on the -Z side
    stt = P[pf + "_starter"] = Part(pf + "_starter")
    rd = 0.10 * E
    zc = -(h - 0.02 * E - rd)
    drums = [-0.30 * E] if D["starters"] == 1 else [-0.36 * E, -0.12 * E]
    for yy in drums:
        _ng(stt, _ST, [(0, 0), (rd * 0.8, 0), (rd, 0.03 * E), (rd, 0.52 * E), (rd * 0.8, 0.55 * E), (0, 0.55 * E)], 12,
            t=(-0.275 * E, yy, zc), R=RX)
        if lod < 2:
            for xx in (-0.17 * E, 0.17 * E):
                _ng(stt, _CO, crect(rd - 0.005, rd + 0.02 * E, -0.02 * E, 0.02 * E), 12, t=(xx, yy, zc), R=RX)
            box(stt, _DK, (0, yy, zc + rd * 0.5), (0.40 * E, 0.08 * E, rd * 1.2))            # cradle to the vessel
        if lod == 0:
            for xx in (-0.2 * E, 0.2 * E):
                cyl(stt, "eq_insulator", (xx, yy + rd * 0.9, zc), (xx, yy + rd + 0.04 * E, zc), 0.02 * k, 8)

    # --- regulator cabinet on +Z with the Tianhe 12-gon bus outlet, door, big handles, lamps
    rg = P[pf + "_regulator"] = Part(pf + "_regulator")
    z0, z1 = ap + 0.01 * E, h - 0.035 * E
    box(rg, _ST, (0, -0.20 * E, (z0 + z1) / 2), (0.40 * E, 0.42 * E, z1 - z0), chamfer=0.015 if lod == 0 else 0.0)
    _tflange(rg, _ST, I3, (0.10 * E, -0.30 * E, z1), 0.085 * E, 0.035 * E, lod, 0.04)
    if lod < 2:
        box(rg, _DK, (-0.06 * E, -0.16 * E, z1 + 0.006), (0.20 * E, 0.26 * E, 0.012))      # door
    if lod == 0:
        _handle(rg, "eq_steel", (-0.13 * E, -0.16 * E, z1 + 0.012), (0, 0, 1), (0, 1, 0), 0.16 * E, 0.05 * k, 0.014 * k, 6)
        for i in range(3):
            box(rg, "eq_status_amber", (0.12 * E, -0.04 * E - 0.04 * E * i, z1 + 0.005), (0.04 * E, 0.025 * E, 0.01))

    # --- sensors on the upper shoulder
    sn = P[pf + "_sensors"] = Part(pf + "_sensors")
    for th in (math.pi / 4, 3 * math.pi / 4, 5 * math.pi / 4, 7 * math.pi / 4):
        _ybox(sn, _DK, th, rv * 0.80 + 0.04 * E, yv1 - 0.035 * E, (0.08 * E, 0.07 * E, 0.08 * E))
        if lod == 0:
            cyl(sn, "eq_insulator", polar(rv * 0.80 + 0.04 * E, th, yv1), polar(rv * 0.80 + 0.04 * E, th, yv1 + 0.03 * E), 0.022 * k, 8)

    # --- heat column and square pad (top face of the envelope)
    hc = P[pf + "_heat_pipe_column"] = Part(pf + "_heat_pipe_column")
    py0, pt = h - 0.075 * E, 0.075 * E
    _yng(hc, _ST, [(0.15 * E, yv1 - 0.01), (0.15 * E, py0), (0, py0)], n)
    if lod < 2:
        _yng(hc, _DK, crect(0.145 * E, 0.19 * E, yv1, yv1 + 0.03 * E), n)
    pd = P["reactor_cooling_pad"] = Part("reactor_cooling_pad")
    L = h
    bw = 0.06 * E
    if lod < 2:
        box(pd, _DK, (0, py0 + 0.012, 0), (E - 0.04, 0.024, E - 0.04))
        for s in (-1, 1):
            box(pd, _ST, (s * (L - bw / 2), py0 + pt / 2, 0), (bw, pt, E))
            box(pd, _ST, (0, py0 + pt / 2, s * (L - bw / 2)), (E - 2 * bw, pt, bw))
        for th in (0.0, math.pi / 2):
            box(pd, _ST, (0, py0 + pt / 2 - 0.004, 0), (E - 2 * bw, pt - 0.008, 0.045 * E), rot_y(th))
        for qx in (-1, 1):
            for qz in (-1, 1):
                prism(pd, "eq_pad_ceramic", inset([(0.0, 0.0), (qx * L, 0.0), (qx * L, qz * L), (0.0, qz * L)][::(1 if qx * qz > 0 else -1)], 0.045 * E),
                      py0 + 0.024, py0 + pt - 0.01)
        box(pd, _ST, (0, py0 + pt / 2 + 0.006, 0), (0.16 * E, pt + 0.012, 0.16 * E))
        if lod == 0:
            for s in (-1, 1):
                for i in range(7):
                    q = -L + 0.15 * E + i * (E - 0.30 * E) / 6
                    hex_bolt(pd, "eq_steel", (s * (L - bw / 2), py0 + pt, q), (0, 1, 0), 0.04 * k, 0.02, washer=False)
                    hex_bolt(pd, "eq_steel", (q, py0 + pt, s * (L - bw / 2)), (0, 1, 0), 0.04 * k, 0.02, washer=False)
    else:
        box(pd, "eq_pad_ceramic", (0, py0 + pt / 2, 0), (E, pt, E))

    if close():                                                         # bus cables cells -> regulator, starter leads
        hz = P[pf + "_harness"] = Part(pf + "_harness")
        for sx in (-1, 1):
            harness(hz, bezier((sx * x0, cy1 + 0.02 * E, 0.30 * E), (sx * x0, cy1 + 0.05 * E, 0.42 * E),
                               (sx * 0.24 * E, 0.10 * E, z0 + 0.02), (sx * 0.20 * E, 0.12 * E, z0 + 0.04), 10), 0.02 * k, 3,
                    ("eq_insulator", _DK), tie_every=3)
        for yy in drums:
            harness(hz, bezier((0.2 * E, yy + rd + 0.04 * E, zc), (0.2 * E, yy + rd + 0.12 * E, zc), polar(rv + 0.06 * E, -math.pi / 2 + 0.6, yv0 + 0.25 * E),
                               polar(rv + 0.02 * E, -math.pi / 2 + 0.4, yv0 + 0.30 * E), 10), 0.016 * k, 2, ("eq_insulator",), tie_every=4)
    return P


def sr_hits(D):
    def fn():
        pf = D["pf"]
        E, h, rv, yv0, yv1 = _sr_dims(D)
        ap = rv * math.cos(math.pi / D["n"])
        x0, x1 = ap + 0.015 * E, h - 0.045 * E
        rd = 0.10 * E
        zc = -(h - 0.02 * E - rd)
        z0, z1 = ap + 0.01 * E, h - 0.035 * E
        py0 = h - 0.075 * E
        H = [hbox(pf + "_skid", (0, -h + 0.035 * E, 0), (E, 0.07 * E, 0.81 * E)),
             hcyl(pf + "_vessel", (0, yv0, 0), (0, yv1, 0), ap + 0.03 * E)]
        for sx in (-1, 1):
            H.append(hbox(pf + "_cells", (sx * (x0 + x1) / 2, (yv0 + yv1 - 0.15 * E) / 2, 0), (x1 - x0, yv1 - yv0 - 0.19 * E, 0.73 * E)))
        for yy in ([-0.30 * E] if D["starters"] == 1 else [-0.36 * E, -0.12 * E]):
            H.append(hcyl(pf + "_starter", (-0.275 * E, yy, zc), (0.275 * E, yy, zc), rd))
        H += [hbox(pf + "_regulator", (0, -0.20 * E, (z0 + z1) / 2 + 0.01), (0.40 * E, 0.42 * E, z1 - z0 + 0.04)),
              hcyl(pf + "_heat_pipe_column", (0, yv1, 0), (0, py0, 0), 0.19 * E),
              hbox("reactor_cooling_pad", (0, py0 + 0.0375 * E, 0), (E, 0.075 * E, E))]
        for th in (math.pi / 4, 3 * math.pi / 4, 5 * math.pi / 4, 7 * math.pi / 4):
            H.append(hbox(pf + "_sensors", polar(rv * 0.80 + 0.04 * E, th, yv1 - 0.02 * E), (0.08 * E, 0.10 * E, 0.08 * E),
                          (0, -math.degrees(th), 0)))
        return H
    return fn


ASSETS.update({"tianhe_ko600_coolant_loop": ko_parts, "tianhe_rs50_radiator": rs_parts, "tianhe_cluster_h_rcs": rcs_parts,
               "tianhe_s150_reactor": lambda lod: sr_parts(S150, lod), "tianhe_s600_reactor": lambda lod: sr_parts(S600, lod)})
HITS.update({"tianhe_ko600_coolant_loop": ko_hits, "tianhe_rs50_radiator": rs_hits, "tianhe_cluster_h_rcs": rcs_hits,
             "tianhe_s150_reactor": sr_hits(S150), "tianhe_s600_reactor": sr_hits(S600)})
MODEL_ASSETS.update({"th.cool.loop600": "tianhe_ko600_coolant_loop", "th.rad.panel50": "tianhe_rs50_radiator",
                     "std.rcs.cluster_h": "tianhe_cluster_h_rcs", "th.apu.s150": "tianhe_s150_reactor", "th.apu.s600": "tianhe_s600_reactor"})
