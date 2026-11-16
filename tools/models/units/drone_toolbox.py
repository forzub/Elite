"""Standard repair-drone toolbox std.drone.toolbox and its saddle (author 2026-11-16: «надо его дополнить, что бы у него
был еще отдельный ящик с инструментами, он его может брать а может и не брать. можно взваливать на себя сверху. наверное
это будет отдельный объект» + «то же самое у богомола и вообще у любого дрона - ремонтника»). PROPOSAL
(tools/models/briefs/new_parts_proposal.json).

One open standard for every repair drone, whoever makes it:
- the box: 0.52 x 0.20 x 0.32 m (with the lid), its own object (model std_drone_toolbox), signal-yellow so it is found
  drifting in space; a hinged lid, a fold-down top handle the drone's hands grip, a tray of tools inside;
- the saddle: four twist-lock castings at the TB_LOCKS pitch (0.28 x 0.22 m) on the drone's back; the box's four
  graphite lock plates underneath sit on them. A drone carries the saddle in its own palette (tb_saddle) and declares
  the "saddle" attach point in its rig.json; the box declares "base" (sits on a saddle) and "handle" (the grip).

OBJ frame of the box: its base on y = 0 (the castings' tops), centred, long side along X, front (latches) +Z.
"""
import math

from eqlib import *  # noqa: F401,F403 - the unit kernel

register_materials({
    "eq_toolbox_yellow": ((0.86, 0.66, 0.12), (0.30, 0.28, 0.20), 40),
    "eq_toolbox_rubber": ((0.10, 0.10, 0.11), (0.05, 0.05, 0.05), 10),
})

TB_SIZE = (0.52, 0.16, 0.32)                     # the body (x, wall height, z); the lid adds 0.035
TB_LID = 0.035
TB_LOCKS = [(sx * 0.14, sz * 0.11) for sx in (-1, 1) for sz in (-1, 1)]   # lock pitch (x, z) in the box frame
TB_HANDLE_Y = 0.255                              # the grip bar's centre (handle up)
TB_PLATE = 0.008                                 # lock plates under the floor: the base point is their underside
Y_, K_, R_ = "eq_toolbox_yellow", "eq_metal_graphite", "eq_toolbox_rubber"


def _hinge(lid_y, d):
    return (0.0, lid_y, -d / 2)


def tb_parts(lod):
    w, h, d = TB_SIZE
    t = 0.012
    seg, small = seg_for(lod), small_for(lod)
    ch = (lambda c: c) if lod == 0 else (lambda c: 0.0)
    P = {}

    def part(name):
        return P.setdefault(name, Part(name))

    bd = part("tb_body")
    # a hollow shell: floor + four walls
    box(bd, Y_, (0, t / 2, 0), (w, t, d), chamfer=ch(0.004))
    for s in (-1, 1):
        box(bd, Y_, (0, h / 2, s * (d / 2 - t / 2)), (w, h, t), chamfer=ch(0.003))
        box(bd, Y_, (s * (w / 2 - t / 2), h / 2, 0), (t, h, d - 2 * t), chamfer=ch(0.003))
    if lod < 3:
        for sx in (-1, 1):
            for sz in (-1, 1):                                                           # rubber corner bumpers
                box(bd, R_, (sx * (w / 2 - 0.012), h / 2, sz * (d / 2 - 0.012)), (0.036, h + 0.004, 0.036), chamfer=ch(0.006))
        for lx, lz in TB_LOCKS:                                                          # lock plates underneath
            box(bd, K_, (lx, -TB_PLATE / 2, lz), (0.075, TB_PLATE, 0.075))
            if lod == 0:
                cyl(bd, "eq_steel", (lx, -0.0085, lz), (lx, -0.0075, lz), 0.024, small)
        for sx in (-1, 1):                                                               # end grips (a hand can take it by the side)
            cyl(bd, "eq_steel", (sx * (w / 2 + 0.03), 0.10, -0.07), (sx * (w / 2 + 0.03), 0.10, 0.07), 0.011, 6)
            for sz in (-1, 1):
                cyl(bd, "eq_steel", (sx * w / 2, 0.10, sz * 0.07), (sx * (w / 2 + 0.03), 0.10, sz * 0.07), 0.009, 6)
            box(bd, K_, (sx * 0.15, h - 0.02, d / 2 + 0.006), (0.045, 0.035, 0.012), chamfer=ch(0.003))   # latch keepers
        box(bd, "eq_status_amber", (0.0, 0.11, d / 2 + 0.003), (0.10, 0.03, 0.004))                      # ID / beacon strip
        for sx in (-1, 1):                                                               # hinge knuckles on the body
            cyl(bd, K_, (sx * 0.20 - 0.03, h, -d / 2), (sx * 0.20 + 0.03, h, -d / 2), 0.012, 8)
    # the foam insert with the tools in their cut-outs (author 2026-11-16: «начинка - какой-то хлам. можно ли сделать,
    # что бы дроид вытягивал из ящика например - кувалду или другие инструменты»): every tool is its own part - a drone's
    # clip takes it out by an event (MESH_EDITOR_SPEC 18.18)
    fm = part("tb_foam")
    box(fm, R_, (0, 0.041, 0), (w - 2 * t - 0.004, 0.058, d - 2 * t - 0.004))
    if lod < 2:
        box(fm, "eq_agora_rm2_brass", (-0.17, 0.076, -0.137), (0.12, 0.012, 0.016), chamfer=ch(0.002))   # spare torch tips
    for key, fn in TB_TOOL_BUILD.items():
        fn(part(key), lod, small, ch)
    # the lid on a rear hinge
    ld = part("tb_lid")
    box(ld, Y_, (0, h + TB_LID / 2, 0), (w + 0.006, TB_LID, d + 0.006), chamfer=ch(0.008))
    if lod < 2:
        for x in (-0.12, 0.12):
            box(ld, Y_, (x, h + TB_LID + 0.006, 0), (0.03, 0.012, d - 0.06), chamfer=ch(0.003))      # ribs
        for sx in (-1, 1):
            box(ld, "eq_steel", (sx * 0.15, h + 0.004, d / 2 + 0.014), (0.035, 0.05, 0.01))           # latches
        cyl(ld, K_, (-0.03, h, -d / 2), (0.03, h, -d / 2), 0.012, 8)                     # lid hinge knuckle
        for x in (-0.14, 0.14):
            box(ld, K_, (x, h + TB_LID + 0.004, 0.0), (0.05, 0.008, 0.04))               # handle brackets
    # the fold-down handle (up = grip)
    hd = part("tb_handle")
    y0 = h + TB_LID + 0.008
    for x in (-0.14, 0.14):
        cyl(hd, "eq_steel", (x, y0, 0), (x, TB_HANDLE_Y, 0), 0.010, 6)
    cyl(hd, R_, (-0.15, TB_HANDLE_Y, 0), (0.15, TB_HANDLE_Y, 0), 0.016, 8 if lod < 2 else 6)
    return P


def _hammer(pt, lod, small, ch):
    """sledgehammer: steel handle with a yellow grip, a square steel head across the handle"""
    y, z = 0.09, 0.075
    cyl(pt, "eq_steel", (-0.225, y, z), (0.15, y, z), 0.013, 8 if lod < 2 else 6)
    cyl(pt, Y_, (-0.225, y, z), (-0.08, y, z), 0.017, 8 if lod < 2 else 6)                              # grip sleeve
    box(pt, "eq_steel", (0.18, y, z), (0.06, 0.06, 0.13), chamfer=ch(0.006))                            # head
    if lod < 2:
        for sz in (-1, 1):
            box(pt, K_, (0.18, y, z + sz * 0.066), (0.052, 0.052, 0.004))                               # striking faces


def _wrench(pt, lod, small, ch):
    """adjustable wrench: flat handle, head with a fixed and a sliding jaw, the worm"""
    y, z = 0.078, -0.02
    box(pt, "eq_steel", (-0.065, y, z), (0.29, 0.012, 0.034), chamfer=ch(0.003))                        # handle
    box(pt, R_, (-0.15, y, z), (0.11, 0.016, 0.038), chamfer=ch(0.003))                                 # rubber grip
    box(pt, "eq_steel", (0.115, y, z - 0.004), (0.075, 0.016, 0.07), chamfer=ch(0.004))                 # head
    box(pt, "eq_steel", (0.165, y, z - 0.03), (0.03, 0.016, 0.02))                                      # fixed jaw
    box(pt, "eq_steel", (0.165, y, z + 0.022), (0.03, 0.016, 0.02))                                     # sliding jaw
    if lod < 2:
        cyl(pt, "eq_agora_rm2_brass", (0.095, y, z + 0.02), (0.135, y, z + 0.02), 0.008, 6)             # worm


def _bar(pt, lod, small, ch):
    """crowbar: hex bar, a bent claw at one end, a chisel at the other"""
    y, z = 0.08, -0.105
    cyl(pt, K_, (-0.205, y, z), (0.165, y, z), 0.011, 6)
    cyl(pt, K_, (0.165, y, z), (0.205, y + 0.012, z - 0.02), 0.010, 6)                                  # claw bend
    box(pt, K_, (0.212, y + 0.016, z - 0.026), (0.016, 0.02, 0.02))
    box(pt, "eq_steel", (-0.215, y, z), (0.02, 0.006, 0.024))                                           # chisel


# tools in the box: part -> (title, builder, grip point, handle axis) in the box frame (rest = lying in the foam)
TB_TOOLS = {"tb_tool_hammer": ("кувалда", _hammer, (0.02, 0.09, 0.075), (1, 0, 0)),
            "tb_tool_wrench": ("разводной ключ", _wrench, (-0.04, 0.078, -0.02), (1, 0, 0)),
            "tb_tool_bar": ("лом-гвоздодёр", _bar, (0.0, 0.08, -0.105), (1, 0, 0))}
TB_TOOL_BUILD = {k: v[1] for k, v in TB_TOOLS.items()}


def tb_hits():
    w, h, d = TB_SIZE
    return [hbox("tb_body", (0, h / 2, 0), (w + 0.01, h, d + 0.01)),
            hbox("tb_lid", (0, h + TB_LID / 2 + 0.004, 0), (w + 0.01, TB_LID + 0.008, d + 0.01)),
            hbox("tb_handle", (0, (h + TB_LID + TB_HANDLE_Y) / 2, 0), (0.33, TB_HANDLE_Y - h - TB_LID + 0.02, 0.04)),
            hbox("tb_tool_hammer", (-0.02, 0.09, 0.075), (0.43, 0.035, 0.035)),
            hbox("tb_tool_hammer", (0.18, 0.09, 0.075), (0.065, 0.065, 0.135)),
            hbox("tb_tool_wrench", (0.0, 0.078, -0.022), (0.42, 0.02, 0.075)),
            hbox("tb_tool_bar", (0.0, 0.085, -0.11), (0.44, 0.04, 0.04))]


def _j(part, parent, pivot, axis, rng, joint, **kw):
    return {"part": part, "parent": parent, "pivot_m": [round(x, 4) for x in pivot], "axis": [round(x, 4) for x in norm(axis)],
            "range_deg": list(rng), "joint": joint, **kw}


TB_ATTACH = [
    {"id": "base", "part": "tb_body", "point_m": [0.0, -TB_PLATE, 0.0], "up": [0, 1, 0], "forward": [0, 0, 1],
     "fits": "std.drone.toolbox.saddle", "note": "the underside of the four lock plates (centre); they sit on the saddle castings, the twist-lock cones enter their sockets"},
    {"id": "handle", "part": "tb_handle", "point_m": [0.0, TB_HANDLE_Y, 0.0], "up": [0, 1, 0], "forward": [0, 0, 1],
     "fits": "grip", "note": "the grip bar (handle up); a drone's hand closes here"}] + [
    {"id": "grip_" + k[len("tb_tool_"):], "part": k, "point_m": list(v[2]), "axis": list(v[3]), "fits": "grip",
     "note": f"{v[0]}: where a hand closes on it (axis = the handle, towards the working end)"} for k, v in TB_TOOLS.items()]


def tb_rig():
    w, h, d = TB_SIZE
    J = [_j("tb_body", None, (0, 0, 0), (0, 1, 0), (-180, 180), "root (box frame)"),
         _j("tb_foam", "tb_body", (0, 0, 0), (0, 1, 0), (0, 0), "fixed (foam insert)"),
         _j("tb_lid", "tb_body", _hinge(h, d), (-1, 0, 0), (0, 110), "lid hinge (+ = open)"),
         _j("tb_handle", "tb_lid", (0, h + TB_LID + 0.008, 0), (1, 0, 0), (-90, 90), "handle fold (0 = up)")]
    for k, v in TB_TOOLS.items():
        J.append(_j(k, "tb_body", v[2], (0, 1, 0), (0, 0), f"{v[0]}: lifts out of the foam (metres)", type="slide", range_m=[0, 0.35]))
    out = {"tb_handle": [[0, 0], [0.5, 90], [8.5, 90], [9.0, 0]], "tb_lid": [[0, 0], [0.5, 0], [1.2, 105], [7.8, 105], [8.5, 0], [9.0, 0]]}
    for i, k in enumerate(TB_TOOLS):
        t0 = 1.4 + i * 2.2
        out[k] = [[0, 0], [t0, 0], [t0 + 0.6, 0.3], [t0 + 1.4, 0.3], [t0 + 2.0, 0], [9.0, 0]]
    return {"joints": J,
            "clips": [{"id": "lid", "title": "Крышка: открыть и закрыть", "duration_s": 4.0, "loop": True,
                       "keys": {"tb_handle": [[0, 0], [0.8, 90], [3.2, 90], [4.0, 0]],
                                "tb_lid": [[0, 0], [0.8, 0], [1.8, 105], [2.6, 105], [3.4, 0], [4.0, 0]]}},
                      {"id": "tools", "title": "Инструменты: кувалда, ключ, лом — достать и положить", "duration_s": 9.0,
                       "loop": True, "keys": out}],
            "attach": TB_ATTACH}


def tb_saddle(part, c, lod, rotated=False, plate=None, cast="eq_agora_rm2_brass", plate_size=None):
    """the saddle on a drone's back: (optional) plate + four twist-lock castings whose tops are at c[1]; rotated = the
    box rides with its long side along the drone's Z. Returns the "saddle" attach point."""
    th = 0.035
    for lx, lz in TB_LOCKS:
        x, z = (lz, lx) if rotated else (lx, lz)
        box(part, cast, (c[0] + x, c[1] - th / 2, c[2] + z), (0.05, th, 0.05), chamfer=0.006 if lod == 0 else 0.0)
        if lod == 0:
            cyl(part, "eq_steel", (c[0] + x, c[1], c[2] + z), (c[0] + x, c[1] + 0.008, c[2] + z), 0.012, 6)   # twist-lock cone
    if plate:
        ps = plate_size or ((0.34, 0.3) if not rotated else (0.3, 0.34))
        box(part, plate, (c[0], c[1] - th - 0.008, c[2]), (ps[0], 0.016, ps[1]), chamfer=0.004 if lod < 2 else 0.0)
    return {"point_m": [round(v, 4) for v in c], "up": [0, 1, 0], "forward": [-1, 0, 0] if rotated else [0, 0, 1],
            "fits": "std.drone.toolbox.saddle", "note": "std.drone.toolbox rides here (box forward = this forward)"}


ASSETS = {"std_drone_toolbox": tb_parts}
HITS = {"std_drone_toolbox": tb_hits}
RIGS = {"std_drone_toolbox": tb_rig}
MODEL_ASSETS = {"std.drone.toolbox": "std_drone_toolbox"}


def carry_set(drone, short, saddle):
    """SETS entry "<drone>_toolbox_set": the drone with std.drone.toolbox locked on its saddle (`saddle` = the drone's
    "saddle" attach point: the box's base goes to point_m, its +Z to forward)"""
    f = saddle["forward"]
    yaw = math.degrees(math.atan2(f[0], f[2]))
    return {drone + "_toolbox_set": {
        "title": f"{short} + ящик с инструментами",
        "description": "Набор: ремонтный дрон и стандартный ящик std.drone.toolbox на его седле, по ссылкам (ящик - отдельный объект, дрон может и не брать его).",
        "nodes": [(drone, drone, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), "equipment", None),
                  ("toolbox", "std_drone_toolbox", _add3(saddle["point_m"], (0.0, TB_PLATE, 0.0)), (0.0, yaw, 0.0), "toolbox", drone)]}}


def _add3(a, b):
    return tuple(round(x + y, 4) for x, y in zip(a, b))


# ---------------------------------------------------------------- rig kinematics (the viewer's poseWorld in Python)
# used to pose a drone's arm for its tool clips: the hand's grip point goes to the tool's grip in the box on its back
def _rot(axis, deg):
    x, y, z = norm(axis)
    a = math.radians(deg)
    c, s, C = math.cos(a), math.sin(a), 1 - math.cos(a)
    return [[c + x * x * C, x * y * C - z * s, x * z * C + y * s],
            [y * x * C + z * s, c + y * y * C, y * z * C - x * s],
            [z * x * C - y * s, z * y * C + x * s, c + z * z * C]]


def _mv(R, v):
    return tuple(sum(R[i][k] * v[k] for k in range(3)) for i in range(3))


def _mm(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def fk(joints, angles):
    """world(part) -> (R, t): a rest-frame point p of the part is at R p + t (parents first, like the viewer)"""
    by = {j["part"]: j for j in joints}
    cache = {}
    I = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]

    def world(part, depth=0):
        if part in cache:
            return cache[part]
        j = by.get(part)
        R, t = I, (0.0, 0.0, 0.0)
        if j and depth < 64:
            if j.get("parent") and j["parent"] != part:
                R, t = world(j["parent"], depth + 1)
            v = angles.get(part, 0)
            if v and j.get("type") == "slide":
                t = tuple(a + b for a, b in zip(t, _mv(R, tuple(c * v for c in norm(j["axis"])))))
            elif v:
                L = _rot(j["axis"], v)
                p = j["pivot_m"]
                tl = tuple(a - b for a, b in zip(p, _mv(L, p)))
                R, t = _mm(R, L), tuple(a + b for a, b in zip(_mv(R, tl), t))
        cache[part] = (R, t)
        return R, t
    return world


def at(world, part, p):
    R, t = world(part)
    return tuple(a + b for a, b in zip(_mv(R, p), t))


def ik(joints, base, chain, bounds, hand, grip, target, up_local=None, up=(0.0, 1.0, 0.0), w_up=0.0, seeds=(), avoid=None):
    """angles of the `chain` joints that bring the hand's grip point (rest frame) to `target` (coordinate descent from
    the base pose, the seeds and a few seeded random starts; w_up > 0 also turns the hand's up_local towards `up`)"""
    import random
    rng = random.Random(7)

    def cost(x):
        a = dict(base)
        a.update(zip(chain, x))
        w = fk(joints, a)
        q = at(w, hand, grip)
        c = 20.0 * sum((u - v) ** 2 for u, v in zip(q, target))
        if w_up and up_local:
            wu = _mv(w(hand)[0], up_local)
            c += w_up * (1.0 - sum(u * v for u, v in zip(norm(wu), up)))
        if avoid:                                  # keep the forearm out of the box: depth of its samples inside, squared
            lo, hi = avoid["box"]
            for q in (at(w, avoid["part"], p) for p in avoid["pts"]):
                d = min(min(q[i] - lo[i], hi[i] - q[i]) for i in range(3))
                if d > 0:
                    c += 4.0 * d * d
        return c

    starts = [[base.get(c, 0.0) for c in chain]] + [list(s) for s in seeds]
    starts += [[rng.uniform(*bounds[c]) for c in chain] for _ in range(6)]
    best, bc = None, 1e9
    for x in starts:
        x = [min(max(v, bounds[c][0]), bounds[c][1]) for v, c in zip(x, chain)]
        c0, step = cost(x), 24.0
        while step > 0.05:
            better = False
            for i, c in enumerate(chain):
                for d in (step, -step):
                    y = list(x)
                    y[i] = min(max(y[i] + d, bounds[c][0]), bounds[c][1])
                    cy = cost(y)
                    if cy < c0 - 1e-12:
                        x, c0, better = y, cy, True
            if not better:
                step /= 2
        if c0 < bc:
            best, bc = x, c0
    a = dict(zip(chain, [round(v, 1) for v in best]))
    a0 = dict(base)
    a0.update(a)
    err = math.dist(at(fk(joints, a0), hand, grip), target)
    return a, err


def prop_point(joints, prop, p_box):
    """a box-frame point of the prop placed on the drone (prop: {part, point_m, rotation_deg}), drone at rest"""
    R = _rot((0, 1, 0), prop["rotation_deg"][1])
    q = tuple(a + b for a, b in zip(_mv(R, p_box), prop["point_m"]))
    return at(fk(joints, {}), prop["part"], q)


def take_clip(joints, prop, arm, tool, work, cid, title):
    """a clip where the drone opens the box on its back, takes `tool` out with the arm's hand, works with it (`work` =
    [(dt, target point) | (dt, "wrist", degrees)]) and puts it back (MESH_EDITOR_SPEC 18.18). arm = {chain, bounds, hand,
    grip (rest point between the fingers), up (the hand's back normal, rest), fingers {part: closed angle}, wrist}.
    Returns (clip, used angles per chain joint, worst IK miss in metres)."""
    pid = prop["id"]
    gp = prop_point(joints, prop, TB_TOOLS[tool][2])
    chain, bounds, hand = arm["chain"], arm["bounds"], arm["hand"]
    avoid = None
    if arm.get("avoid"):                           # the box in the drone frame (rest), grown by the forearm's radius
        part, pts, m = arm["avoid"]
        cs = [prop_point(joints, prop, (sx * TB_SIZE[0] / 2, y, sz * TB_SIZE[2] / 2))
              for sx in (-1, 1) for sz in (-1, 1) for y in (0.0, TB_SIZE[1] + TB_LID)]
        avoid = {"part": part, "pts": pts, "box": (tuple(min(c[i] for c in cs) - m for i in range(3)),
                                                    tuple(max(c[i] for c in cs) + m for i in range(3)))}
    solve = lambda tgt, w_up, seeds=(): ik(joints, {}, chain, bounds, hand, arm["grip"], tgt, arm["up"], (0, 1, 0), w_up, seeds, avoid)
    above, e1 = solve(_add3(gp, (0, 0.16, 0)), 0.002)
    grip, e2 = solve(gp, 0.002, [[above[c] for c in chain]])
    lift, e3 = solve(_add3(gp, (0, 0.34, 0)), 0.0, [[grip[c] for c in chain]])
    poses, errs = [(1.6, above), (2.2, grip), (2.5, grip), (3.1, lift)], [e1, e2, e3]
    t, last = 3.1, lift
    for dt, *what in work:
        t += dt
        if what[0] == "wrist":
            a = dict(last)
            a[arm["wrist"]] = round(a.get(arm["wrist"], 0.0) + what[1], 1)
        else:
            a, e = solve(what[0], 0.0, [[last[c] for c in chain]])
            errs.append(e)
            last = a
        poses.append((t, a))
    t += 0.9
    poses += [(t, lift), (t + 0.6, above), (t + 1.1, grip), (t + 1.6, above), (t + 2.4, {c: 0.0 for c in chain})]
    stow, T = t + 1.15, t + 3.4
    keys = {c: [[0, 0.0]] + [[round(tt, 2), p.get(c, 0.0)] for tt, p in poses] + [[round(T, 2), 0.0]] for c in chain}
    for f, closed in arm["fingers"].items():
        keys[f] = [[0, 0], [1.6, -5], [2.2, -5], [2.5, closed], [round(stow, 2), closed], [round(stow + 0.3, 2), -5],
                   [round(t + 2.4, 2), 0], [round(T, 2), 0]]
    keys[f"{pid}/tb_handle"] = [[0, 0], [0.5, 90], [round(T - 0.5, 2), 90], [round(T, 2), 0]]
    keys[f"{pid}/tb_lid"] = [[0, 0], [0.5, 0], [1.2, 105], [round(t + 2.4, 2), 105], [round(T - 0.5, 2), 0], [round(T, 2), 0]]
    clip = {"id": cid, "title": title, "duration_s": round(T, 2), "loop": True, "props": [pid], "keys": keys,
            "events": [{"t": 2.5, "take": f"{pid}/{tool}", "hand": hand}, {"t": round(stow, 2), "stow": f"{pid}/{tool}"}]}
    used = {c: [min(p.get(c, 0.0) for _, p in poses + [(0, {})]), max(p.get(c, 0.0) for _, p in poses + [(0, {})])] for c in chain}
    clip["ik_miss_m"] = [round(e, 3) for e in errs]
    return clip, used, max(errs)


def widen(joints, used):
    """grow the suggested ranges of the joints to the angles a generated clip uses"""
    for j in joints:
        if j["part"] in used:
            lo, hi = used[j["part"]]
            j["range_deg"] = [min(j["range_deg"][0], math.floor(lo)), max(j["range_deg"][1], math.ceil(hi))]


def toolbox_prop(drone_part, saddle):
    """the rig "props" entry of a drone that carries std.drone.toolbox on its saddle"""
    f = saddle["forward"]
    return {"id": "toolbox", "title": "ящик", "ref": "equipment_ship/std_drone_toolbox", "part": drone_part,
            "point_m": list(_add3(saddle["point_m"], (0.0, TB_PLATE, 0.0))),
            "rotation_deg": [0.0, round(math.degrees(math.atan2(f[0], f[2])), 4), 0.0], "attach": "saddle", "fits": "base"}
