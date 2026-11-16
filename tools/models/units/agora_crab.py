"""Agora Modular RC-3 "Granchio" - hull repair drone, a crab (author 2026-11-15: «можно еще сделать дрона похожего на
краба - плоский широкий корпус, ноги для перемещения по обшивке - руки как у папая для ремонта. глаза - две камеры. без
усов. с двигателем в задней части. можно какие-то мелкие двигатели присобачить в суставы лап - тогда будет летать как
квадрокоптер»). PROPOSAL (tools/models/briefs/new_parts_proposal.json), not in the canon until the author accepts it.

OBJ frame: feet on the hull at y = 0, front +Z, +X = the drone's left. A flat wide carapace on six walking legs with
magnetic pads (it walks sideways like a crab, too), two short-upper-arm / massive-forearm arms with hands for repairs (the
right forearm carries a welding torch), two camera eyes on stalks, the main field thruster at the back and four gimballed
field thrusters on the knees of the front and rear legs - it flies like a quadcopter to a drifting part and tows it home
(the repair job of src/world/modules/ObjectRepairJobRuntime: MovingToFragment -> CapturingFragment -> towing back).
Every moving part is its own OBJ part modelled in the rest pose; RC3_RIG + test clips go into intact/assembly/rig.json.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel (geometry, LOD policy, shared builders)
from units.new_equipment import (W_, T_, S_, B_, UPV, _add, _sub, _mul, _cross, _lerp, _pod, ball, frame_along, hex_limb,  # noqa: F401
                                 hobox, knuckle, obox, seg_lathe, euler_xyz, ray_pose)

# author 2026-11-16: «крабу надо корпус сделать повыше и руки - побольше и подлиннее ... что бы общая форма стремилась к
# крабьей»: a taller hull (0.29 .. 0.51) under a crab carapace - a wide-fronted hexagonal shell that overhangs the hips and
# domes up to 0.73 m in lofted tiers
RC3 = dict(body_y=0.40, body=(0.92, 0.22, 0.70))
RC3_SHELL = [(0.36, 0.43), (0.68, 0.22), (0.58, -0.20), (0.27, -0.40)]          # the left half (x, z), front to rear
RC3_SHELL = RC3_SHELL + [(-x, z) for x, z in reversed(RC3_SHELL)]
RC3_TIERS = ((0.0, 0.51), (0.0, 0.575), (0.11, 0.675), (0.24, 0.73))            # (inset, y): edge band, then the dome
# legs: key -> (side, hip z, foot (x, z)); 1 = front, 3 = rear
RC3_LEGS = {f"{'l' if s > 0 else 'r'}{i + 1}": (s, hz, (s * fx, fz))
            for s in (1, -1) for i, (hz, fx, fz) in enumerate(((0.22, 1.02, 0.66), (0.0, 1.16, 0.02), (-0.20, 1.00, -0.60)))}
RC3_POD_LEGS = ("l1", "r1", "l3", "r3")          # the legs with a knee thruster (four: a quadcopter)
RC3_EYES = {s: ((s * 0.15, 0.66, 0.30), (s * 0.17, 0.83, 0.37)) for s in (1, -1)}   # stalk base, camera centre


def loft(part, mat, tiers, poly, cap_top=True, cap_bottom=True):
    """a convex shell through (inset, y) tiers of one (x, z) polygon, flat facets"""
    rings = [[(x, y, z) for x, z in (inset(poly, dd) if dd > 0 else poly)] for dd, y in tiers]
    n = len(poly)
    cx, cz = sum(q[0] for q in poly) / n, sum(q[1] for q in poly) / n
    for a, b in zip(rings, rings[1:]):
        for i in range(n):
            q = [a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]]
            m = _lerp(q[0], q[2], 0.5)
            hint = (m[0] - cx, 0.25, m[2] - cz)
            fn = facet_normal(q) or (0.0, 1.0, 0.0)
            if fn[0] * hint[0] + fn[1] * hint[1] + fn[2] * hint[2] < 0:
                fn = _mul(fn, -1)
            part.face(mat, [part.vert(v, fn) for v in q], fn)
    if cap_top:
        part.face(mat, [part.vert(v, (0, 1, 0)) for v in rings[-1]], (0, 1, 0))
    if cap_bottom:
        part.face(mat, [part.vert(v, (0, -1, 0)) for v in rings[0]], (0, -1, 0))


def rc3_leg(key):
    """hip H (on the carapace side), coxa end C, knee K (up), ankle A, foot F, the leg's pitch axis"""
    s, hz, (fx, fz) = RC3_LEGS[key]
    H = (s * RC3["body"][0] / 2, RC3["body_y"], hz)
    u = norm((fx - H[0], 0.0, fz - H[2]))
    C = _add(H, _mul(u, 0.08))
    K = _add(_add(C, _mul(u, 0.42)), (0.0, 0.30, 0.0))
    A = (fx, 0.12, fz)
    F = (fx, 0.0, fz)
    return H, C, K, A, F, norm(_cross(u, UPV))


def rc3_knee_pod(key):
    H, C, K, A, F, ax = rc3_leg(key)
    u = norm(_sub(K, C))
    uh = norm((u[0], 0.0, u[2]))
    return K, _add(_add(K, _mul(uh, 0.07)), (0.0, 0.03, 0.0)), ax


def rc3_arm(s):
    """shoulder S (front corner), elbow E, wrist W, hand tip T - the rest pose: forearms raised in front like a crab's"""
    return (s * 0.30, 0.42, 0.36), (s * 0.68, 0.36, 0.68), (s * 0.48, 0.60, 1.16), (s * 0.44, 0.57, 1.40)


RC3_PALM = 0.10


def rc3_hand(s):
    S, E, W, T = rc3_arm(s)
    dw = norm(_sub(T, W))
    nup = norm(_cross(dw, (1, 0, 0)))
    fingers = []
    for fid, x in (("f1", 0.035), ("f2", -0.035)):
        k0 = _add(_add(W, _mul(dw, RC3_PALM)), (x, 0.0, 0.0))
        k1 = _add(k0, _mul(dw, 0.08))
        k2 = _add(_add(k1, _mul(dw, 0.065)), _mul(nup, -0.02))
        fingers.append((fid, k0, k1, k2))
    k0 = _add(_add(_add(W, _mul(dw, 0.04)), (-s * 0.07, 0.0, 0.0)), _mul(nup, -0.014))
    k1 = _add(_add(k0, _mul(dw, 0.06)), (-s * 0.016, 0.0, 0.0))
    k2 = _add(_add(k1, _mul(dw, 0.05)), _add((s * 0.012, 0.0, 0.0), _mul(nup, -0.016)))
    fingers.append(("thumb", k0, k1, k2))
    return dw, nup, fingers


def rc3_parts(lod):
    d = RC3
    seg, small = seg_for(lod), small_for(lod)
    ks = small
    ch = (lambda c: c) if lod == 0 else (lambda c: 0.0)
    P = {}

    def part(name):
        return P.setdefault(name, Part(name))

    # --- the hull and the crab carapace: white hull, terracotta dome over a sea-blue edge band, skirts, belly plate,
    #     bay castings on the dome, the main thruster in the back
    by = d["body_y"]
    bw, bh, bl = d["body"]
    top = RC3_TIERS[-1][1]
    bd = part("rc3_body")
    box(bd, W_, (0, by, 0), (bw, bh, bl), chamfer=0.06 if lod < 2 else 0.0)
    loft(bd, S_, RC3_TIERS[:2], RC3_SHELL, cap_top=False)
    loft(bd, T_, RC3_TIERS[1:], RC3_SHELL, cap_bottom=False)
    if lod < 2:
        for s in (-1, 1):
            box(bd, S_, (s * (bw / 2 + 0.004), by - 0.03, 0), (0.012, 0.10, bl - 0.22), chamfer=0.0)        # side skirts
        box(bd, S_, (0, by - bh / 2 - 0.006, 0), (bw - 0.3, 0.012, bl - 0.2))                                # belly plate
        for z in (-0.12, 0.10):                                                                            # carapace seams
            box(bd, "eq_metal_band", (0, top + 0.002, z), (0.40, 0.004, 0.012))
        for sx in (-1, 1):                                                                                 # bay castings
            for z in (-0.12, 0.14):
                box(bd, B_, (sx * 0.16, top + 0.02, z), (0.06, 0.04, 0.06), chamfer=ch(0.006))
    if lod == 0:
        for sx in (-1, 1):
            for sz in (-1, 1):
                hex_bolt(bd, B_, (sx * 0.26, top + 0.002, sz * 0.04 + 0.01), (0, 1, 0), 0.018, 0.007)
        box(bd, "eq_status_amber", (0.0, top + 0.002, 0.20), (0.06, 0.004, 0.02))
        cyl(bd, B_, (0, by - bh / 2, -0.15), (0, by - bh / 2 - 0.02, -0.15), 0.03, ks)                    # charge / data socket
    # the main field thruster in the back (emitter aft, under the carapace's rear edge)
    p0, p1 = (0, by - 0.02, -bl / 2 + 0.02), (0, by - 0.02, -bl / 2 - 1.0)
    seg_lathe(bd, W_, p0, p1, [(0.0, 0.0), (0.11, 0.0), (0.125, 0.02), (0.125, 0.08), (0.0, 0.08)], small * 2 if lod == 0 else small)
    seg_lathe(bd, "eq_pad_ceramic", p0, p1, [(0.108, 0.081), (0.0, 0.081)], small * 2 if lod == 0 else small)
    if lod < 2:
        seg_lathe(bd, "eq_field_coil", p0, p1, crect(0.125, 0.14, 0.03, 0.06), small * 2)
    for key in RC3_LEGS:                                                                                   # hip bosses
        H = rc3_leg(key)[0]
        cyl(bd, S_, _add(H, (-RC3_LEGS[key][0] * 0.03, 0, 0)), _add(H, (RC3_LEGS[key][0] * 0.02, 0, 0)), 0.055, ks)

    # --- two camera eyes on stalks (no feelers): a pan joint at the stalk base
    es = small * 2 if lod == 0 else small
    for s, side in ((1, "l"), (-1, "r")):
        ey = part(f"rc3_eye_{side}")
        b0, ec = RC3_EYES[s]
        cyl(ey, S_, b0, _add(ec, (0, -0.03, -0.01)), 0.024, 8)
        box(ey, W_, ec, (0.09, 0.08, 0.09), chamfer=0.02 if lod < 2 else 0.0)
        de = norm((0.0, -0.08, 1.0))
        c0 = _add(ec, (0, 0, 0.04))
        seg_lathe(ey, W_, c0, _add(c0, de), [(0.0, 0.0), (0.038, 0.0), (0.038, 0.035), (0.034, 0.045)], es)
        if lod < 2:
            seg_lathe(ey, "eq_agora_rm2_lens", c0, _add(c0, de), [(0.034, 0.045), (0.026, 0.052), (0.018, 0.057)], es)
            seg_lathe(ey, "eq_agora_rm2_iris", c0, _add(c0, de), [(0.018, 0.057), (0.0, 0.06)], es)
            seg_lathe(ey, B_, c0, _add(c0, de), crect(0.038, 0.043, 0.02, 0.03), es)                    # focus ring
        else:
            seg_lathe(ey, "eq_agora_rm2_iris", c0, _add(c0, de), [(0.034, 0.045), (0.0, 0.055)], es)

    # --- six legs: coxa, femur (up), tibia with the magnetic pad; knee thrusters on the front and rear pairs
    for key in RC3_LEGS:
        H, C, K, A, F, ax = rc3_leg(key)
        cx_ = part(f"rc3_leg_{key}_coxa")
        if lod == 0:
            knuckle(cx_, B_, H, UPV, 0.045, 0.10, ks, cap_mat=S_)
        hex_limb(cx_, S_, H, C, 0.042, 0.040)
        fm = part(f"rc3_leg_{key}_femur")
        if lod == 0:
            knuckle(fm, B_, C, ax, 0.042, 0.10, ks, cap_mat=W_)
        hex_limb(fm, W_, C, K, 0.044, 0.034, a0=math.pi / 6)
        tb = part(f"rc3_leg_{key}_tibia")
        if lod == 0:
            knuckle(tb, B_, K, ax, 0.036, 0.09, ks, cap_mat=T_)
        hex_limb(tb, T_, K, A, 0.032, 0.022, a0=math.pi / 6)
        cyl(tb, S_, A, _add(F, (0, 0.045, 0)), 0.02, 6)
        ylathe(tb, "eq_metal_band", [(0.0, 0.0), (0.08, 0.0), (0.08, 0.028), (0.058, 0.045), (0.0, 0.045)], small * 2 if lod == 0 else small, t=F)
        if lod == 0:
            ylathe(tb, "eq_field_coil", crect(0.028, 0.045, 0.045, 0.052), small * 2, t=F)                 # magnet coil
        if key in RC3_POD_LEGS:
            kp = part(f"rc3_leg_{key}_thruster")
            k0, kc, kax = rc3_knee_pod(key)
            _pod(kp, k0, kc, (0.0, -1.0, 0.0), small, lod)
            if lod == 0:
                knuckle(kp, B_, kc, kax, 0.018, 0.09, ks)

    # --- arms "like Popeye's" (author): a thin upper arm, a massive forearm, a hand with two fingers and a thumb;
    #     the right forearm carries a welding torch
    for s, side in ((1, "l"), (-1, "r")):
        S, E, W, T = rc3_arm(s)
        sh = part(f"rc3_arm_{side}_shoulder")
        ball(sh, W_, S, 0.07, ks)
        if lod == 0:
            knuckle(sh, B_, S, (1, 0, 0), 0.05, 0.15, ks, cap_mat=T_)
        up_ = part(f"rc3_arm_{side}_upper")
        hex_limb(up_, S_, S, E, 0.048, 0.042)
        fo = part(f"rc3_arm_{side}_fore")
        if lod == 0:
            knuckle(fo, B_, E, (1, 0, 0), 0.056, 0.13, ks, cap_mat=S_)
        L = math.dist(E, W)
        seg_lathe(fo, W_, E, W, [(0.0, 0.0), (0.06, 0.0), (0.12, 0.25 * L), (0.145, 0.5 * L), (0.125, 0.75 * L),
                                 (0.08, 0.95 * L), (0.065, L), (0.0, L)], small * 2 if lod == 0 else small)
        if lod < 2:
            seg_lathe(fo, T_, E, W, crect(0.141, 0.15, 0.42 * L, 0.58 * L), small * 2)                       # terracotta band
            seg_lathe(fo, B_, E, W, crect(0.083, 0.09, 0.9 * L, 0.95 * L), small * 2)                        # wrist ring
        if s < 0:                                                                                          # welding torch
            fd = norm(_sub(W, E))
            t0 = _add(_lerp(E, W, 0.55), (-0.145, 0.03, 0.0))
            t1 = _add(t0, _mul(fd, 0.30))
            cyl(fo, S_, _lerp(E, W, 0.5), t0, 0.015, 6)
            seg_lathe(fo, W_, t0, t1, [(0.0, 0.0), (0.025, 0.0), (0.025, 0.2), (0.0, 0.2)], ks)
            seg_lathe(fo, "eq_copper", t0, t1, [(0.0, 0.2), (0.018, 0.2), (0.008, 0.27), (0.0, 0.28)], ks)
        dw, nup, fingers = rc3_hand(s)
        hn = part(f"rc3_hand_{side}")
        if lod == 0:
            knuckle(hn, B_, W, (1, 0, 0), 0.035, 0.09, ks, cap_mat=W_)
        obox(hn, W_, W, _add(W, _mul(dw, RC3_PALM)), 0.12, 0.05, up=nup, chamfer=ch(0.008))
        for fid, k0, k1, k2 in fingers:
            fg = part(f"rc3_hand_{side}_{fid}")
            r = 0.02
            hex_limb(fg, T_ if fid == "thumb" else W_, k0, k1, r, r * 0.9)
            hex_limb(fg, W_ if fid == "thumb" else T_, k1, k2, r * 0.9, r * 0.75)
            if lod < 2:
                ball(fg, "eq_metal_band", k2, r * 0.8, 6)
    return P


def rc3_hits():
    d = RC3
    by = d["body_y"]
    bw, bh, bl = d["body"]
    H = [hbox("rc3_body", (0, by, 0), (bw, bh, bl)),
         hbox("rc3_body", (0, 0.543, 0.03), (1.24, 0.065, 0.62)),                     # carapace edge band
         hbox("rc3_body", (0, 0.543, 0.0), (0.66, 0.065, 0.82)),
         hbox("rc3_body", (0, 0.645, 0.01), (0.98, 0.14, 0.56)),                      # the dome
         hcyl("rc3_body", (0, by - 0.02, -bl / 2), (0, by - 0.02, -bl / 2 - 0.09), 0.14)]
    for s, side in ((1, "l"), (-1, "r")):
        b0, ec = RC3_EYES[s]
        H += [hobox(f"rc3_eye_{side}", b0, ec, 0.05, 0.05), hbox(f"rc3_eye_{side}", _add(ec, (0, 0, 0.02)), (0.10, 0.09, 0.14))]
    for key in RC3_LEGS:
        Hp, C, K, A, F, ax = rc3_leg(key)
        H += [hobox(f"rc3_leg_{key}_coxa", Hp, C, 0.09, 0.09, ext=0.03),
              hobox(f"rc3_leg_{key}_femur", C, K, 0.10, 0.10, ext=0.02),
              hobox(f"rc3_leg_{key}_tibia", K, A, 0.07, 0.07, ext=0.02),
              hcyl(f"rc3_leg_{key}_tibia", F, _add(F, (0, 0.045, 0)), 0.08)]
        if key in RC3_POD_LEGS:
            H.append(hbox(f"rc3_leg_{key}_thruster", _add(rc3_knee_pod(key)[1], (0, -0.02, 0)), (0.16, 0.12, 0.16)))
    for s, side in ((1, "l"), (-1, "r")):
        S, E, W, T = rc3_arm(s)
        dw, nup, fingers = rc3_hand(s)
        H += [hobox(f"rc3_arm_{side}_shoulder", _sub(S, (0.06, 0, 0)), _add(S, (0.06, 0, 0)), 0.11, 0.11),
              hobox(f"rc3_arm_{side}_upper", S, E, 0.10, 0.10),
              hobox(f"rc3_arm_{side}_fore", E, W, 0.29, 0.29),
              hobox(f"rc3_hand_{side}", W, _add(W, _mul(dw, RC3_PALM)), 0.13, 0.06, up=nup)]
        for fid, k0, k1, k2 in fingers:
            H.append(hobox(f"rc3_hand_{side}_{fid}", k0, k2, 0.045, 0.045, ext=0.01))
    return H


def _j(part, parent, pivot, axis, rng, joint):
    return {"part": part, "parent": parent, "pivot_m": [round(x, 4) for x in pivot], "axis": [round(x, 4) for x in norm(axis)],
            "range_deg": list(rng), "joint": joint}


def rc3_rig():
    d = RC3
    G = [_j("rc3_body", None, (0, d["body_y"], 0), UPV, (-180, 180), "root (body frame)")]
    for s, side in ((1, "l"), (-1, "r")):
        b0, ec = RC3_EYES[s]
        G.append(_j(f"rc3_eye_{side}", "rc3_body", b0, _sub(ec, b0), (-70, 70), "eye pan about its stalk"))
    for key in RC3_LEGS:
        Hp, C, K, A, F, ax = rc3_leg(key)
        G += [_j(f"rc3_leg_{key}_coxa", "rc3_body", Hp, UPV, (-45, 45), "hip yaw"),
              _j(f"rc3_leg_{key}_femur", f"rc3_leg_{key}_coxa", C, ax, (-45, 75), "hip pitch (+ = leg up, - = flight ray)"),
              _j(f"rc3_leg_{key}_tibia", f"rc3_leg_{key}_femur", K, ax, (-120, 130), "knee (straightens into the flight ray)")]
        if key in RC3_POD_LEGS:
            G.append(_j(f"rc3_leg_{key}_thruster", f"rc3_leg_{key}_tibia", rc3_knee_pod(key)[1], ax, (-120, 90), "knee thruster gimbal"))
    for s, side in ((1, "l"), (-1, "r")):
        S, E, W, T = rc3_arm(s)
        dw, nup, fingers = rc3_hand(s)
        G += [_j(f"rc3_arm_{side}_shoulder", "rc3_body", S, UPV, (-40, 40), "shoulder yaw"),
              _j(f"rc3_arm_{side}_upper", f"rc3_arm_{side}_shoulder", S, (1, 0, 0), (-60, 50), "shoulder pitch (- = arm up / forward)"),
              _j(f"rc3_arm_{side}_fore", f"rc3_arm_{side}_upper", E, (1, 0, 0), (-60, 100), "elbow"),
              _j(f"rc3_hand_{side}", f"rc3_arm_{side}_fore", W, (1, 0, 0), (-60, 60), "wrist pitch")]
        for fid, k0, k1, k2 in fingers:
            if fid == "thumb":
                G.append(_j(f"rc3_hand_{side}_thumb", f"rc3_hand_{side}", k0, _mul(nup, s), (-10, 60), "thumb (+ = close)"))
            else:
                G.append(_j(f"rc3_hand_{side}_{fid}", f"rc3_hand_{side}", k0, (1, 0, 0), (-10, 90), "finger curl (+ = grip)"))
    return G


def _loop_keys(pts, T, phase):
    """[(u 0..1, value)] of one cycle, shifted by phase, as loop keys over T seconds"""
    keys = sorted([[((u + phase) % 1.0) * T, v] for u, v in pts])
    if keys[0][0] > 0:
        a, b = keys[-1], keys[0]
        keys.insert(0, [0.0, a[1] + (b[1] - a[1]) * (T - a[0]) / (T - a[0] + b[0])])
    keys.append([T, keys[0][1]])
    return keys


def rc3_clips():
    C = []
    # 1. sideways walk (tripod gait: l1 + r2 + l3 / r1 + l2 + r3), the legs swing about their hips
    T, k = 1.4, {}
    for key in RC3_LEGS:
        s = RC3_LEGS[key][0]
        phase = 0.0 if key in ("l1", "r2", "l3") else 0.5
        k[f"rc3_leg_{key}_coxa"] = _loop_keys([(0.0, 10), (0.25, 0), (0.5, -10), (1.0, 10)], T, phase)
        k[f"rc3_leg_{key}_femur"] = _loop_keys([(0.0, 0), (0.25, 22), (0.5, 0), (1.0, 0)], T, phase)
        k[f"rc3_leg_{key}_tibia"] = _loop_keys([(0.0, 0), (0.25, -16), (0.5, 0), (1.0, 0)], T, phase)
    C.append({"id": "walk", "title": "Шаг (треногой)", "duration_s": T, "loop": True, "keys": k})
    # 2. repair: the forearms work in turns, hands grip and let go
    k = {}
    for side, ph in (("l", 0.0), ("r", 0.5)):
        k[f"rc3_arm_{side}_upper"] = _loop_keys([(0.0, -10), (0.3, -22), (0.5, -10), (1.0, -10)], 2.0, ph)
        k[f"rc3_arm_{side}_fore"] = _loop_keys([(0.0, 25), (0.3, -2), (0.5, 25), (1.0, 25)], 2.0, ph)
        for f in ("f1", "f2"):
            k[f"rc3_hand_{side}_{f}"] = _loop_keys([(0.0, 60), (0.3, 0), (0.5, 60), (1.0, 60)], 2.0, ph)
        k[f"rc3_hand_{side}_thumb"] = _loop_keys([(0.0, 40), (0.3, 0), (0.5, 40), (1.0, 40)], 2.0, ph)
    C.append({"id": "repair", "title": "Ремонт: руки работают по очереди", "duration_s": 2.0, "loop": True, "keys": k})
    # 3. flight (author 2026-11-16: «для полета ноги надо складывать так, что бы вид был как у квадрокоптера - вытягивать
    #    в лучи под 45 град»): the four pod legs straighten into horizontal rays at 45 degrees, emitters down; the middle
    #    pair tucks up under the shell; the arms fold in; the knee thrusters swing to steer
    k = {}
    for key in RC3_LEGS:
        s = RC3_LEGS[key][0]
        if key in RC3_POD_LEGS:
            Hp, Cp, K, A, F, ax = rc3_leg(key)
            yaw, fm, tb, lv = ray_pose(Hp, Cp, K, A, (s, 0.0, 1.0 if key[1] == "1" else -1.0))
            sw = 1 if key in ("l1", "r3") else -1
            k[f"rc3_leg_{key}_coxa"] = [[0, 0], [1.2, yaw], [4.8, yaw], [6.0, 0]]
            k[f"rc3_leg_{key}_femur"] = [[0, 0], [1.2, fm], [4.8, fm], [6.0, 0]]
            k[f"rc3_leg_{key}_tibia"] = [[0, 0], [1.2, tb], [4.8, tb], [6.0, 0]]
            k[f"rc3_leg_{key}_thruster"] = [[0, 0], [1.2, lv], [2.2, lv + sw * 25], [3.2, lv - sw * 25], [4.2, lv + sw * 25],
                                            [4.8, lv], [6.0, 0]]
        else:
            k[f"rc3_leg_{key}_femur"] = [[0, 0], [1.2, 30], [4.8, 30], [6.0, 0]]
            k[f"rc3_leg_{key}_tibia"] = [[0, 0], [1.2, -100], [4.8, -100], [6.0, 0]]
    for side in ("l", "r"):
        k[f"rc3_arm_{side}_upper"] = [[0, 0], [1.2, 20], [4.8, 20], [6.0, 0]]
        k[f"rc3_arm_{side}_fore"] = [[0, 0], [1.2, -25], [4.8, -25], [6.0, 0]]
    C.append({"id": "fly", "title": "Полёт: ноги лучами под 45° (квадрокоптер), двигатели в коленях рулят", "duration_s": 6.0,
              "loop": True, "keys": k})
    # 4. capture and tow: reach forward, grip the part, pull it in
    k = {}
    for side in ("l", "r"):
        k[f"rc3_arm_{side}_upper"] = [[0, 0], [1.2, -40], [2.0, -40], [3.2, -5], [4.6, -5], [5.6, 0]]
        k[f"rc3_arm_{side}_fore"] = [[0, 0], [1.2, 60], [2.0, 60], [3.2, 10], [4.6, 10], [5.6, 0]]
        for f in ("f1", "f2"):
            k[f"rc3_hand_{side}_{f}"] = [[0, 0], [1.2, -5], [1.8, 75], [4.6, 75], [5.6, 0]]
        k[f"rc3_hand_{side}_thumb"] = [[0, 0], [1.2, -5], [1.8, 50], [4.6, 50], [5.6, 0]]
    C.append({"id": "capture", "title": "Захват детали и буксировка", "duration_s": 6.0, "loop": True, "keys": k})
    # 5. looking around with the eyes
    C.append({"id": "look", "title": "Осмотр: глаза", "duration_s": 4.0, "loop": True, "keys": {
        "rc3_eye_l": [[0, 0], [1.0, 45], [2.0, 0], [3.0, -30], [4.0, 0]],
        "rc3_eye_r": [[0, 0], [1.0, 30], [2.0, 0], [3.0, -45], [4.0, 0]]}})
    return C


RC3_RIG = rc3_rig()
ASSETS = {"agora_rc3_drone": rc3_parts}
HITS = {"agora_rc3_drone": rc3_hits}
RIGS = {"agora_rc3_drone": lambda: {"joints": RC3_RIG, "clips": rc3_clips()}}
MODEL_ASSETS = {"agora.drone.rc3": "agora_rc3_drone"}
