"""People: body variants of the milo_002 astronaut with one shared humanoid skeleton, skin weights and baked actions.

blender -b --factory-startup -P tools/people/make_people.py -- [--only id,id] [--out <models root>] [--work <tmp dir>]

The source (D:/__elite/_data/objects/spacemans/milo_002/astronaut.glb, ASSET_LICENSES/models/astronauts/milo_002) is read
only. Steps:
  1. base: import, fit the skeleton (BONES, metres, Blender Z up, the face looks -Y, .L = +X), automatic weights plus
     fixes (rigid small parts, helmet -> head, backpack -> chest, boots -> foot, no cross-side weights), saved to
     <work>/people_base.blend;
  2. per person (PEOPLE): re-proportion by pose-bone scaling (inherit scale off, so a bone's scale moves its children
     but does not scale them), bake it into the mesh and the rest pose, add the belly / bust, scale to the height, put
     the soles on the floor;
  3. actions (ACTIONS): idle, walk, run, wave, carry_idle, carry_walk, tool_use - FK for legs and torso, two-bone IK
     for the arms; the socket bone prop.R (right palm) is animated too: in carry_* it sits in the middle of the box
     held in both hands (axes = the chest's), in tool_use it holds the tablet facing the eyes;
  4. export <out>/<id>/intact/assembly/<id>.gltf (+ .bin, textures/) and <out>/<id>/model.json;
  5. props: prop_box, prop_tool -> <out>/../objects/<id>/ (origin = the socket point, glTF Y up).

Socket convention (game): attach the prop's root to the glTF node "prop.R" (its world transform); the prop's own glTF
axes are used as is. In rest / idle / walk the node is in the right palm with the hand's axes."""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

SRC = "D:/__elite/_data/objects/spacemans/milo_002/astronaut.glb"
FPS = 30

# name: (head, tail, parent, deform); left side only, the right is mirrored (x -> -x)
BONES = {
    "root": ((0, 0, 0), (0, 0, 0.25), None, False),
    "hips": ((0, -0.02, 0.80), (0, -0.02, 0.97), "root", True),
    "spine": ((0, -0.02, 0.97), (0, -0.01, 1.15), "hips", True),
    "chest": ((0, -0.01, 1.15), (0, -0.01, 1.38), "spine", True),
    "neck": ((0, -0.01, 1.38), (0, -0.02, 1.50), "chest", True),
    "head": ((0, -0.02, 1.50), (0, -0.02, 1.90), "neck", True),
    "shoulder.L": ((0.04, -0.02, 1.36), (0.20, -0.02, 1.33), "chest", True),
    "upper_arm.L": ((0.20, -0.02, 1.33), (0.43, -0.07, 1.18), "shoulder.L", True),
    "forearm.L": ((0.43, -0.07, 1.18), (0.58, -0.17, 1.08), "upper_arm.L", True),
    "hand.L": ((0.58, -0.17, 1.08), (0.76, -0.22, 0.98), "forearm.L", True),
    "prop.L": ((0.65, -0.20, 1.04), (0.74, -0.225, 0.99), "hand.L", False),
    "thigh.L": ((0.11, -0.02, 0.80), (0.16, -0.02, 0.43), "hips", True),
    "shin.L": ((0.16, -0.02, 0.43), (0.20, -0.03, 0.15), "thigh.L", True),
    "foot.L": ((0.20, -0.03, 0.15), (0.22, -0.19, 0.04), "shin.L", True),
}
CONNECTED = {"spine", "chest", "neck", "head", "upper_arm.L", "forearm.L", "hand.L", "shin.L", "foot.L"}


def mirror_name(n):
    return n[:-2] + ".R" if n.endswith(".L") else n


def all_bones():
    out = {}
    for n, (h, t, p, d) in BONES.items():
        out[n] = (Vector(h), Vector(t), p, d)
        if n.endswith(".L"):
            out[mirror_name(n)] = (Vector((-h[0], h[1], h[2])), Vector((-t[0], t[1], t[2])), mirror_name(p) if p else None, d)
    return out


# ---------------------------------------------------------------------------------------------------------------- people
# per-part scales in the source's space (1 = the astronaut), then the whole is scaled to "height" (the helmet top)
#   sh: shoulder span, chest_w/d, waist_w/d (spine), hip_w/d: width / depth, torso_l: spine + chest length,
#   arm_t/arm_l, thigh_t/shin_t/leg_l: limb thickness / length, head: helmet, neck_t, hand, belly/bust/butt: metres
BASE = dict(sh=1, chest_w=1, chest_d=1, waist_w=1, waist_d=1, hip_w=1, hip_d=1, torso_l=1, arm_t=1, arm_l=1.08,
            thigh_t=1, shin_t=1, leg_l=1.22, head=0.86, neck_t=1, hand=1, foot=1, belly=0, bust=0, butt=0)


def person(pid, title, height, **kw):
    p = dict(BASE)
    p.update(kw)
    p.update(id=pid, title=title, height=height)
    return p


FEMALE = dict(sh=0.86, chest_w=0.9, chest_d=0.95, waist_w=0.84, waist_d=0.9, hip_w=1.12, hip_d=1.05, arm_t=0.86,
              thigh_t=1.02, shin_t=0.9, leg_l=1.26, head=0.82, neck_t=0.85, hand=0.88, foot=0.9, bust=0.035, butt=0.02)


def fem(**kw):
    p = dict(FEMALE)
    p.update(kw)
    return p


PEOPLE = [
    person("person_m_normal", "Мужчина, обычный, 1,78 м", 1.78),
    person("person_m_tall", "Мужчина, высокий, 2,00 м", 2.00, leg_l=1.3, arm_l=1.12, torso_l=1.05, head=0.82),
    person("person_m_thin", "Мужчина, худой, 1,82 м", 1.82, sh=0.9, chest_w=0.8, chest_d=0.8, waist_w=0.72, waist_d=0.75,
           hip_w=0.84, hip_d=0.84, arm_t=0.7, thigh_t=0.72, shin_t=0.74, leg_l=1.28, neck_t=0.85, head=0.84),
    person("person_m_athletic", "Мужчина, атлетичный, 1,85 м", 1.85, sh=1.14, chest_w=1.1, chest_d=1.06, waist_w=0.92,
           waist_d=0.94, hip_w=0.98, arm_t=1.1, thigh_t=1.08, shin_t=1.05, leg_l=1.25, neck_t=1.08),
    person("person_m_muscular", "Мужчина, накачанный, 1,88 м", 1.88, sh=1.45, chest_w=1.36, chest_d=1.25, waist_w=1.05,
           waist_d=1.05, hip_w=1.04, arm_t=1.55, thigh_t=1.32, shin_t=1.14, leg_l=1.22, neck_t=1.3, hand=1.08, head=0.84),
    person("person_m_fat", "Мужчина, полный, 1,75 м", 1.75, sh=1.08, chest_w=1.2, chest_d=1.25, waist_w=1.42, waist_d=1.45,
           hip_w=1.3, hip_d=1.28, arm_t=1.22, thigh_t=1.32, shin_t=1.12, leg_l=1.16, neck_t=1.2, belly=0.1, butt=0.03),
    person("person_m_obese", "Мужчина, с ожирением, 1,72 м", 1.72, sh=1.12, chest_w=1.38, chest_d=1.45, waist_w=1.78,
           waist_d=1.8, hip_w=1.62, hip_d=1.5, arm_t=1.45, thigh_t=1.62, shin_t=1.3, leg_l=1.1, neck_t=1.4, head=0.88,
           belly=0.2, butt=0.05, bust=0.03),
    person("person_f_normal", "Женщина, обычная, 1,66 м", 1.66, **fem()),
    person("person_f_thin", "Женщина, худая, 1,70 м", 1.70, **fem(sh=0.82, chest_w=0.82, chest_d=0.85, waist_w=0.74,
           waist_d=0.8, hip_w=1.0, hip_d=0.95, arm_t=0.74, thigh_t=0.84, shin_t=0.78, leg_l=1.3, bust=0.025, neck_t=0.78)),
    person("person_f_athletic", "Женщина, атлетичная, 1,74 м", 1.74, **fem(sh=0.95, chest_w=0.98, waist_w=0.84, arm_t=0.96,
           thigh_t=1.1, shin_t=1.0, leg_l=1.28, neck_t=0.92)),
    person("person_f_fat", "Женщина, полная, 1,60 м", 1.60, **fem(sh=0.94, chest_w=1.15, chest_d=1.2, waist_w=1.32,
           waist_d=1.35, hip_w=1.5, hip_d=1.35, arm_t=1.15, thigh_t=1.42, shin_t=1.08, leg_l=1.18, belly=0.06, bust=0.05,
           butt=0.04, neck_t=1.0)),
    person("person_child_teen", "Подросток, 1,45 м", 1.45, sh=0.9, chest_w=0.9, chest_d=0.9, waist_w=0.88, waist_d=0.9,
           hip_w=0.9, hip_d=0.9, arm_t=0.82, thigh_t=0.85, shin_t=0.82, leg_l=1.2, arm_l=1.06, head=0.92, neck_t=0.82,
           hand=0.9),
    person("person_child_small", "Ребёнок, 1,10 м", 1.10, sh=0.94, chest_w=0.98, chest_d=1.0, waist_w=1.02, waist_d=1.04,
           hip_w=0.96, hip_d=1.0, arm_t=0.9, thigh_t=0.95, shin_t=0.92, leg_l=0.98, arm_l=0.98, torso_l=0.92, head=1.0,
           neck_t=0.9, hand=0.92, belly=0.02),
]


# ------------------------------------------------------------------------------------------------------------- helpers
def objs():
    return bpy.context.view_layer.objects


def activate(o, mode="OBJECT"):
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    for x in bpy.context.view_layer.objects:
        x.select_set(False)
    o.select_set(True)
    objs().active = o
    if mode != "OBJECT":
        bpy.ops.object.mode_set(mode=mode)


def update():
    bpy.context.view_layer.update()


def loose_parts(me):
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    seen = set()
    parts = []
    for v in bm.verts:
        if v.index in seen:
            continue
        stack = [v]
        seen.add(v.index)
        comp = []
        while stack:
            a = stack.pop()
            comp.append(a.index)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in seen:
                    seen.add(b.index)
                    stack.append(b)
        parts.append(comp)
    bm.free()
    return parts


def weights_of(body):
    names = [g.name for g in body.vertex_groups]
    w = [dict() for _ in body.data.vertices]
    for v in body.data.vertices:
        for g in v.groups:
            if g.weight > 1e-4:
                w[v.index][names[g.group]] = g.weight
    return w


def set_weights(body, w):
    for g in body.vertex_groups:
        g.remove(list(range(len(body.data.vertices))))
    groups = {g.name: g for g in body.vertex_groups}
    for i, d in enumerate(w):
        items = sorted(d.items(), key=lambda kv: -kv[1])[:4]
        s = sum(x for _, x in items)
        for n, x in items:
            if x / s > 1e-3:
                if n not in groups:
                    groups[n] = body.vertex_groups.new(name=n)
                groups[n].add([i], x / s, "REPLACE")


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (a + ab * t - p).length


# ---------------------------------------------------------------------------------------------------------------- base
def build_base():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=SRC)
    body = [o for o in bpy.data.objects if o.type == "MESH"][0]
    body.name = body.data.name = "body"
    body.data.transform(body.matrix_world)   # the importer turns the Y-up data by the object; bake it, Z up from here
    body.matrix_world = Matrix.Identity(4)
    bones = all_bones()
    arm_data = bpy.data.armatures.new("skeleton")
    arm = bpy.data.objects.new("skeleton", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    activate(arm, "EDIT")
    for n, (h, t, p, d) in bones.items():
        eb = arm_data.edit_bones.new(n)
        eb.head, eb.tail = h, t
        eb.use_deform = d
    for n, (h, t, p, d) in bones.items():
        eb = arm_data.edit_bones[n]
        if p:
            eb.parent = arm_data.edit_bones[p]
            eb.use_connect = n.replace(".R", ".L") in CONNECTED
        y = (eb.tail - eb.head).normalized()
        eb.align_roll(Vector((0, 0, 1)) if abs(y.y) > 0.7 else Vector((0, -1, 0)))
    bpy.ops.object.mode_set(mode="OBJECT")
    # automatic (bone heat) weights fail on the source itself (197 loose, open parts): solve them on a watertight voxel
    # proxy and carry them over to the nearest proxy face
    proxy = body.copy()
    proxy.data = body.data.copy()
    proxy.name = "weights_proxy"
    bpy.context.scene.collection.objects.link(proxy)
    rm = proxy.modifiers.new("remesh", "REMESH")
    rm.mode = "VOXEL"
    rm.voxel_size = 0.014
    activate(proxy)
    bpy.ops.object.modifier_apply(modifier=rm.name)
    for x in objs():
        x.select_set(False)
    proxy.select_set(True)
    arm.select_set(True)
    objs().active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    print(f"[people] proxy: {len(proxy.data.vertices)} verts, {len(proxy.vertex_groups)} groups")
    for x in objs():
        x.select_set(False)
    body.select_set(True)
    arm.select_set(True)
    objs().active = arm
    bpy.ops.object.parent_set(type="ARMATURE_NAME")   # the modifier and empty groups
    activate(body)
    dt = body.modifiers.new("weights", "DATA_TRANSFER")
    dt.object = proxy
    dt.use_vert_data = True
    dt.data_types_verts = {"VGROUP_WEIGHTS"}
    dt.vert_mapping = "POLYINTERP_NEAREST"
    dt.layers_vgroup_select_src = "ALL"
    dt.layers_vgroup_select_dst = "NAME"
    bpy.ops.object.modifier_move_to_index(modifier=dt.name, index=0)
    bpy.ops.object.modifier_apply(modifier=dt.name)
    bpy.data.objects.remove(proxy)
    fix_weights(body, arm)
    return arm, body


def fix_weights(body, arm):
    bones = all_bones()
    deform = [n for n, b in bones.items() if b[3]]
    w = weights_of(body)
    vs = [v.co.copy() for v in body.data.vertices]
    empty = sum(1 for d in w if not d)
    # nearest-bone fallback for what the heat solver left empty, and no weights across the middle
    for i, d in enumerate(w):
        x = vs[i].x
        for n in list(d):
            if (n.endswith(".R") and x > 0.04) or (n.endswith(".L") and x < -0.04):
                del d[n]
        if not d or sum(d.values()) < 1e-3:
            cand = [n for n in deform if not ((n.endswith(".R") and x > 0) or (n.endswith(".L") and x < 0))]
            ds = sorted((seg_dist(vs[i], bones[n][0], bones[n][1]), n) for n in cand)[:2]
            w[i] = {n: 1.0 / max(dd, 1e-3) ** 3 for dd, n in ds}
    # rigid parts: helmet, backpack, boots, gloves, small plates / buttons follow one bone mix
    side = lambda x: ".L" if x > 0 else ".R"
    rigid = 0
    for part in loose_parts(body.data):
        pts = [vs[i] for i in part]
        lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
        hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
        c = (lo + hi) / 2
        fixed = None
        if lo.z > 1.45:
            fixed = {"head": 1.0}
        elif lo.y > 0.12 and lo.z > 0.9 and hi.z < 1.5 and abs(c.x) < 0.25:
            fixed = {"chest": 0.75, "spine": 0.25} if c.z > 1.12 else {"spine": 0.6, "chest": 0.4}
        elif hi.z < 0.26 and abs(c.x) > 0.05:
            fixed = {"foot" + side(c.x): 1.0}
        elif lo.x > 0.52 or hi.x < -0.52:
            fixed = {"hand" + side(c.x): 1.0}
        elif (hi - lo).length < 0.2:
            acc = {}
            for i in part:
                for n, x in w[i].items():
                    acc[n] = acc.get(n, 0) + x
            fixed = acc
        if fixed:
            rigid += 1
            for i in part:
                w[i] = dict(fixed)
    set_weights(body, w)
    print(f"[people] weights: {empty} verts without heat weights, {rigid} rigid parts")


# ------------------------------------------------------------------------------------------------------------- reshape
def reshape(arm, body, P):
    activate(arm, "POSE")
    for b in arm.data.bones:
        b.inherit_scale = "NONE"
    pb = arm.pose.bones
    tl = P["torso_l"]

    def sc(n, x, y, z):
        pb[n].scale = (x, y, z)

    sc("hips", P["hip_w"], 1.0, P["hip_d"])
    sc("spine", P["waist_w"], tl, P["waist_d"])
    sc("chest", P["chest_w"], tl, P["chest_d"])
    sc("neck", P["neck_t"], 1.0, P["neck_t"])
    sc("head", P["head"], P["head"], P["head"])
    for s in (".L", ".R"):
        sc("shoulder" + s, P["arm_t"] ** 0.5, P["sh"], P["arm_t"] ** 0.5)
        sc("upper_arm" + s, P["arm_t"], P["arm_l"], P["arm_t"])
        sc("forearm" + s, P["arm_t"] ** 0.9, P["arm_l"], P["arm_t"] ** 0.9)
        sc("hand" + s, P["hand"], P["hand"], P["hand"])
        sc("thigh" + s, P["thigh_t"], P["leg_l"], P["thigh_t"])
        sc("shin" + s, P["shin_t"], P["leg_l"], P["shin_t"])
        sc("foot" + s, P["foot"], P["foot"], P["foot"])
    update()
    bpy.ops.object.mode_set(mode="OBJECT")
    activate(body)
    mod = [m for m in body.modifiers if m.type == "ARMATURE"][0]
    bpy.ops.object.modifier_apply(modifier=mod.name)
    activate(arm, "POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    for b in arm.data.bones:
        b.inherit_scale = "FULL"
    m = body.modifiers.new("Armature", "ARMATURE")
    m.object = arm
    bulges(arm, body, P)
    # height and floor
    zs = [v.co.z for v in body.data.vertices]
    s = P["height"] / (max(zs) - min(zs))
    lift = -min(zs)
    body.data.transform(Matrix.Scale(s, 4) @ Matrix.Translation((0, 0, lift)))
    activate(arm, "EDIT")
    for eb in arm.data.edit_bones:
        if eb.name == "root":
            continue
        eb.head = (eb.head + Vector((0, 0, lift))) * s
        eb.tail = (eb.tail + Vector((0, 0, lift))) * s
    bpy.ops.object.mode_set(mode="OBJECT")
    body.data.update()
    return s


def bulges(arm, body, P):
    """belly (front, below the chest panel), bust (front of the chest), butt (back of the hips) - smooth bumps masked
    by the torso weights, so the arms and legs nearby stay as they are"""
    B = arm.data.bones
    spine = (B["spine"].head_local + B["spine"].tail_local) / 2
    chest = B["chest"].head_local.lerp(B["chest"].tail_local, 0.45)
    hips = B["hips"].head_local
    w = weights_of(body)
    hw = P["waist_w"] * 0.2
    for v in body.data.vertices:
        d = w[v.index]
        mask = d.get("spine", 0) + d.get("chest", 0) + d.get("hips", 0)
        if mask < 0.05:
            continue
        p = v.co
        dy = 0.0
        if P["belly"] and p.y < spine.y:
            u = ((p.x / (hw * 1.25)) ** 2 + ((p.z - (spine.z - 0.03)) / 0.2) ** 2)
            front = min(1.0, (spine.y - p.y) / 0.12)
            dy -= P["belly"] * math.exp(-1.6 * u) * front
            p.x *= 1 + 0.6 * P["belly"] * math.exp(-1.6 * u)
        if P["bust"] and p.y < chest.y:
            for sx in (0.1, -0.1):
                u = ((p.x - sx * P["chest_w"]) / 0.09) ** 2 + ((p.z - chest.z) / 0.09) ** 2
                dy -= P["bust"] * math.exp(-u) * min(1.0, (chest.y - p.y) / 0.12)
        if P["butt"] and p.y > hips.y and p.z < hips.z + 0.12:
            u = (abs(p.x) - 0.1) ** 2 / 0.1 ** 2 + ((p.z - (hips.z - 0.05)) / 0.12) ** 2
            dy += P["butt"] * math.exp(-u) * min(1.0, (p.y - hips.y) / 0.1)
        p.y += dy * mask
    body.data.update()


# ------------------------------------------------------------------------------------------------------------- posing
def qa(axis, deg):
    return Quaternion(Vector(axis), math.radians(deg))


def mir(q):
    """the same rotation for the other side (mirror x): axis (x, y, z) -> (x, -y, -z)"""
    return Quaternion((q.w, q.x, -q.y, -q.z))


def set_rot(pb, q_arm):
    r = pb.bone.matrix_local.to_quaternion()
    pb.rotation_quaternion = r.inverted() @ q_arm @ r


def rest_pose(arm):
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = Quaternion()
        pb.location = (0, 0, 0)
        pb.scale = (1, 1, 1)


def aim(arm, name, direction, up_hint=None):
    """turn bone so its Y axis points along direction (armature space), keeping its twist as little changed as possible"""
    update()
    pb = arm.pose.bones[name]
    m = pb.matrix.copy()
    y = m.to_3x3().col[1].normalized()
    q = y.rotation_difference(direction.normalized())
    rot = (q.to_matrix() @ m.to_3x3())
    if up_hint is not None:
        # spin about the new Y so that Z leans to up_hint
        yy = rot.col[1].normalized()
        z = rot.col[2].normalized()
        h = (up_hint - yy * up_hint.dot(yy)).normalized()
        ang = math.atan2(yy.dot(z.cross(h)), z.dot(h))
        rot = Matrix.Rotation(ang, 3, yy) @ rot
    new = rot.to_4x4()
    new.translation = m.translation
    pb.matrix = new
    update()


def ik_arm(arm, s, wrist, pole, hand_dir=None, hand_up=None):
    update()
    pb = arm.pose.bones
    S = pb["upper_arm" + s].head.copy()
    a = pb["upper_arm" + s].length
    b = pb["forearm" + s].length
    d = max(1e-4, min((wrist - S).length, (a + b) * 0.999))
    dirv = (wrist - S).normalized()
    along = (a * a - b * b + d * d) / (2 * d)
    h = math.sqrt(max(0.0, a * a - along * along))
    perp = pole - dirv * pole.dot(dirv)
    perp = perp.normalized() if perp.length > 1e-6 else Vector((0, 0, -1))
    E = S + dirv * along + perp * h
    W = S + dirv * d
    aim(arm, "upper_arm" + s, E - S)
    aim(arm, "forearm" + s, W - E)
    if hand_dir is not None:
        aim(arm, "hand" + s, hand_dir, hand_up)


class Keyer:
    """keys every pose bone (rotation, location) of the armature at a frame; quaternions kept on one hemisphere"""

    def __init__(self, arm, name):
        self.arm = arm
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        arm.animation_data_create()
        arm.animation_data.action = act
        self.act = act
        self.prev = {}

    def key(self, frame):
        update()
        for pb in self.arm.pose.bones:
            q = pb.rotation_quaternion.copy()
            if pb.name in self.prev and self.prev[pb.name].dot(q) < 0:
                q.negate()
                pb.rotation_quaternion = q
            self.prev[pb.name] = q
            pb.keyframe_insert("rotation_quaternion", frame=frame, group=pb.name)
            pb.keyframe_insert("location", frame=frame, group=pb.name)

    def done(self):
        tr = self.arm.animation_data.nla_tracks.new()
        tr.name = self.act.name
        tr.strips.new(self.act.name, int(self.act.frame_range[0]), self.act)
        tr.mute = True
        self.arm.animation_data.action = None


def dims(arm):
    B = arm.data.bones
    return dict(
        leg=(B["thigh.L"].length + B["shin.L"].length),
        armlen=(B["upper_arm.L"].length + B["forearm.L"].length),
        chest=B["chest"].head_local.copy(), chest_top=B["chest"].tail_local.copy(),
        spine=B["spine"].head_local.copy(), hips=B["hips"].head_local.copy(),
        sh_L=B["upper_arm.L"].head_local.copy(), sh_R=B["upper_arm.R"].head_local.copy(),
    )


def front_y(body, z0, z1, half_w):
    ys = [v.co.y for v in body.data.vertices if z0 <= v.co.z <= z1 and abs(v.co.x) < half_w]
    return min(ys) if ys else -0.2


def bump(p, c, w):
    d = (p - c + 0.5) % 1.0 - 0.5
    return math.exp(-(d / w) ** 2)


def arms_down(pb, deg=36, fwd=-8):
    set_rot(pb["upper_arm.L"], qa((1, 0, 0), fwd) @ qa((0, 1, 0), deg))
    set_rot(pb["upper_arm.R"], mir(qa((1, 0, 0), fwd) @ qa((0, 1, 0), deg)))
    set_rot(pb["forearm.L"], qa((1, 0, 0), -14))
    set_rot(pb["forearm.R"], mir(qa((1, 0, 0), -14)))


def legs(pb, p, amp_t, knee_stance, knee_swing, lean=0.0):
    for s, ph in ((".L", p), (".R", (p + 0.5) % 1.0)):
        t = -amp_t * math.cos(2 * math.pi * ph) + lean
        k = 4 + knee_stance * bump(ph, 0.1, 0.09) + knee_swing * bump(ph, 0.72, 0.16)
        f = -(t + k) * 0.8 + 10 * bump(ph, 0.5, 0.08)
        q = lambda d: qa((1, 0, 0), d) if s == ".L" else mir(qa((1, 0, 0), d))
        set_rot(pb["thigh" + s], q(t))
        set_rot(pb["shin" + s], q(k))
        set_rot(pb["foot" + s], q(f))


def cycle(arm, name, seconds, pose_fn, step=2):
    n = max(2, round(seconds * FPS))
    k = Keyer(arm, name)
    for fr in list(range(0, n, step)) + [n]:
        rest_pose(arm)
        pose_fn(fr / n, arm.pose.bones)
        k.key(fr + 1)
    k.done()


def make_actions(arm, body, P):
    D = dims(arm)
    # arms hang lower on a slim body, stay out from a wide one (the source holds them ~33 deg below the horizontal)
    down = max(18.0, min(48.0, 46.0 - 34.0 * (max(P["waist_w"], P["chest_w"], P["arm_t"], P["hip_w"] * 0.9) - 1.0)))
    leg = D["leg"]
    sc = leg / 0.66   # the source's leg is ~0.66 m

    def idle(p, pb):
        br = math.sin(2 * math.pi * p)
        set_rot(pb["chest"], qa((1, 0, 0), -1.2 * br))
        set_rot(pb["head"], qa((0, 0, 1), 6 * math.sin(2 * math.pi * p + 0.6)) @ qa((1, 0, 0), 1.5 * br))
        arms_down(pb, down + 1.5 * br)
        pb["hips"].location = (0, 0.004 * sc * br, 0)

    def walk(p, pb):
        legs(pb, p, 20, 10, 48)
        c = math.cos(2 * math.pi * p)
        pb["hips"].location = (0, -0.02 * sc * (0.5 + 0.5 * math.cos(4 * math.pi * p)), 0)
        set_rot(pb["hips"], qa((0, 0, 1), -5 * c))
        set_rot(pb["chest"], qa((0, 0, 1), 7 * c) @ qa((1, 0, 0), 2))
        set_rot(pb["upper_arm.L"], qa((1, 0, 0), 16 * c) @ qa((0, 1, 0), down))
        set_rot(pb["upper_arm.R"], mir(qa((1, 0, 0), -16 * c) @ qa((0, 1, 0), down)))
        set_rot(pb["forearm.L"], qa((1, 0, 0), -18 - 8 * max(0, -c)))
        set_rot(pb["forearm.R"], mir(qa((1, 0, 0), -18 - 8 * max(0, c))))

    def run(p, pb):
        legs(pb, p, 34, 22, 95, lean=-6)
        c = math.cos(2 * math.pi * p)
        pb["hips"].location = (0, 0.035 * sc * math.cos(4 * math.pi * p) - 0.03 * sc, 0)
        set_rot(pb["hips"], qa((0, 0, 1), -8 * c))
        set_rot(pb["spine"], qa((1, 0, 0), 10))
        set_rot(pb["chest"], qa((0, 0, 1), 12 * c))
        set_rot(pb["head"], qa((1, 0, 0), -8))
        set_rot(pb["upper_arm.L"], qa((1, 0, 0), 40 * c) @ qa((0, 1, 0), down))
        set_rot(pb["upper_arm.R"], mir(qa((1, 0, 0), -40 * c) @ qa((0, 1, 0), down)))
        set_rot(pb["forearm.L"], qa((1, 0, 0), -70 - 15 * c))
        set_rot(pb["forearm.R"], mir(qa((1, 0, 0), -70 + 15 * c)))

    def wave(p, pb):
        set_rot(pb["upper_arm.L"], qa((1, 0, 0), -6) @ qa((0, 1, 0), down))
        set_rot(pb["forearm.L"], qa((1, 0, 0), -14))
        set_rot(pb["head"], qa((0, 0, 1), -8) @ qa((0, 1, 0), 4))
        set_rot(pb["chest"], qa((0, 1, 0), -3))
        rise = min(1.0, p / 0.18) if p < 0.5 else min(1.0, (1.0 - p) / 0.18)
        rise = rise * rise * (3 - 2 * rise)
        sw = math.sin(2 * math.pi * 4 * p) * rise
        S = D["sh_R"]
        L = D["armlen"]
        lo = S + Vector((-0.12, -0.08, -0.85)).normalized() * L * 0.95
        up = S + Vector((-0.45, -0.25, 0.75)).normalized() * L * 0.93 + Vector((-0.05 * sw * L, 0, 0))
        W = lo.lerp(up, rise)
        hand_dir = Vector((0, -0.25, -1)).lerp(Vector((-0.35 * sw, -0.1, 1)), rise)
        update()
        ik_arm(arm, ".R", W, Vector((-1, 0.3, -0.6)), hand_dir, Vector((0, -1, 0)))

    torso_front = front_y(body, D["hips"].z, D["chest"].z + 0.1 * sc, 0.25 * sc)
    box = Vector((0.40, 0.30, 0.30))   # prop_box: width (x), depth (y), height (z), metres

    def carry_pose(pb):
        update()
        C = Vector((0.0, torso_front - box.y / 2 - 0.03, (D["spine"].z + D["chest"].z) / 2 + 0.02 * sc))
        for s, sx in ((".L", 1), (".R", -1)):
            W = C + Vector((sx * (box.x / 2 + 0.02), 0.02, -0.02))
            ik_arm(arm, s, W, Vector((sx * 1.0, 0.6, -0.8)), Vector((0.0, -1, -0.15)), Vector((-sx * 1.0, 0, 0)))
        update()
        m = Matrix.Translation(C)
        pb["prop.R"].matrix = m   # the box's centre, its axes = the armature's (upright, facing the front)
        update()
        return {n: (pb[n].rotation_quaternion.copy(), pb[n].location.copy()) for n in
                ("upper_arm.L", "forearm.L", "hand.L", "upper_arm.R", "forearm.R", "hand.R", "prop.R")}

    rest_pose(arm)
    carry = carry_pose(arm.pose.bones)

    def put(pb, saved):
        for n, (q, l) in saved.items():
            pb[n].rotation_quaternion = q
            pb[n].location = l

    def carry_idle(p, pb):
        br = math.sin(2 * math.pi * p)
        set_rot(pb["spine"], qa((1, 0, 0), -3 - 0.8 * br))
        pb["hips"].location = (0, 0.004 * sc * br, 0)
        put(pb, carry)

    def carry_walk(p, pb):
        legs(pb, p, 16, 8, 42)
        c = math.cos(2 * math.pi * p)
        pb["hips"].location = (0, -0.018 * sc * (0.5 + 0.5 * math.cos(4 * math.pi * p)), 0)
        set_rot(pb["hips"], qa((0, 0, 1), -4 * c))
        set_rot(pb["spine"], qa((1, 0, 0), -3) @ qa((0, 0, 1), 3 * c))
        put(pb, carry)

    def tool_use(p, pb):
        set_rot(pb["head"], qa((1, 0, 0), 14 + 2 * math.sin(2 * math.pi * p)))
        set_rot(pb["neck"], qa((1, 0, 0), 6))
        update()
        C = D["chest"].lerp(D["chest_top"], 0.25)
        T = Vector((-0.05 * sc, torso_front - 0.26 * sc, C.z))   # tablet centre
        Wr = T + Vector((-0.11, 0.03, -0.03))
        ik_arm(arm, ".R", Wr, Vector((-1, 0.5, -0.8)), Vector((0.25, -1, 0.1)), Vector((0, 0, 1)))
        update()
        pal = pb["prop.R"]
        rot = Matrix.Rotation(math.radians(-40), 4, "X")   # screen (+Z of the tablet) toward the eyes
        pal.matrix = Matrix.Translation(T) @ rot
        update()
        n = (rot.to_3x3() @ Vector((0, 0, 1)))
        tap = 0.5 + 0.5 * math.cos(2 * math.pi * 3 * p)
        Wl = T + Vector((0.06 + 0.03 * math.sin(2 * math.pi * 2 * p), 0.0, 0.0)) + n * (0.10 + 0.04 * tap) + Vector((0.04, 0.02, -0.03))
        ik_arm(arm, ".L", Wl, Vector((1, 0.5, -0.8)), Vector((-0.6, -0.8, -0.3)), Vector((0, 0, 1)))

    cycle(arm, "idle", 3.0, idle, step=3)
    cycle(arm, "walk", 1.1, walk)
    cycle(arm, "run", 0.7, run, step=1)
    cycle(arm, "wave", 2.4, wave, step=1)
    cycle(arm, "carry_idle", 3.0, carry_idle, step=3)
    cycle(arm, "carry_walk", 1.2, carry_walk)
    cycle(arm, "tool_use", 3.0, tool_use, step=2)
    rest_pose(arm)
    return box


ACTIONS = {
    "idle": "стоит, дышит (3 с, петля)",
    "walk": "шаг на месте (1,1 с, петля)",
    "run": "бег на месте (0,7 с, петля)",
    "wave": "машет правой рукой (2,4 с)",
    "carry_idle": "держит ящик двумя руками (prop.R = центр ящика), стоит (3 с, петля)",
    "carry_walk": "несёт ящик, шаг на месте (1,2 с, петля)",
    "tool_use": "планшет в правой руке (prop.R), левая тычет в экран (3 с, петля)",
}


# -------------------------------------------------------------------------------------------------------------- export
def export_person(arm, body, P, out_root, s):
    pid = P["id"]
    root = os.path.join(out_root, pid)
    dst = os.path.join(root, "intact", "assembly", pid + ".gltf")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    for x in objs():
        x.select_set(x in (arm, body))
    objs().active = arm
    bpy.ops.export_scene.gltf(filepath=dst, export_format="GLTF_SEPARATE", export_texture_dir="textures", use_selection=True,
                              export_animations=True, export_animation_mode="ACTIONS", export_skins=True,
                              export_def_bones=False, export_apply=False, export_yup=True)
    zs = [v.co.z for v in body.data.vertices]
    passport = {
        "schema": "elite.model.v1",
        "id": pid,
        "title": P["title"],
        "kind": "person",
        "description": (f"Человек: «{P['title']}» - тело космонавта milo_002 (CGTrader «Cartoon astronaut», albertisharf, "
                        "Royalty Free, без ИИ-обучения; лицензия ASSET_LICENSES/models/astronauts/milo_002), "
                        "перепропорционировано по костям; общий скелет людей, веса кожи, действия в glTF. "
                        "Собрано tools/people/make_people.py, правка - там же (параметры PEOPLE)."),
        "height_m": round(max(zs) - min(zs), 3),
        "skeleton": {
            "bones": [b.name for b in arm.data.bones],
            "deform": [b.name for b in arm.data.bones if b.use_deform],
            "sockets": {"prop.R": "правая ладонь; в carry_* - центр ящика, в tool_use - центр планшета",
                        "prop.L": "левая ладонь"},
            "frame": "glTF: +Y вверх, лицо смотрит в +Z, левая рука (.L) в +X; корень (root) на полу",
        },
        "animations": [{"id": k, "title": v} for k, v in ACTIONS.items()],
        "props": ["prop_box", "prop_tool"],
        "variants": [{"id": "intact", "title": "Целый", "inGame": "shell"}],
    }
    with open(os.path.join(root, "model.json"), "w", encoding="utf-8") as f:
        json.dump(passport, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print(f"[people] {pid}: {passport['height_m']} m (scale {s:.3f}) -> {dst}")


def make_props(objects_root):
    for pid, title, kind in (("prop_box", "Ящик для переноски (реквизит людей)", "box"),
                             ("prop_tool", "Планшет (реквизит людей)", "tool")):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        mat = bpy.data.materials.new(pid)
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes["Principled BSDF"]
        if kind == "box":
            bpy.ops.mesh.primitive_cube_add(size=1)
            o = bpy.context.object
            o.scale = (0.40, 0.30, 0.30)
            bpy.ops.object.transform_apply(scale=True)
            bev = o.modifiers.new("bevel", "BEVEL")
            bev.width = 0.012
            bev.segments = 2
            bsdf.inputs["Base Color"].default_value = (0.55, 0.42, 0.12, 1)
            bsdf.inputs["Roughness"].default_value = 0.6
            note = "0,40 x 0,30 x 0,30 м; начало координат - центр ящика (= сокет prop.R в carry_idle / carry_walk)"
        else:
            bpy.ops.mesh.primitive_cube_add(size=1)
            o = bpy.context.object
            o.scale = (0.24, 0.16, 0.012)
            bpy.ops.object.transform_apply(scale=True)
            bev = o.modifiers.new("bevel", "BEVEL")
            bev.width = 0.004
            bev.segments = 2
            bsdf.inputs["Base Color"].default_value = (0.08, 0.09, 0.1, 1)
            bsdf.inputs["Metallic"].default_value = 0.5
            bpy.ops.mesh.primitive_plane_add(size=1, location=(0, 0, 0.0065))
            scr = bpy.context.object
            scr.scale = (0.21, 0.13, 1)
            bpy.ops.object.transform_apply(scale=True)
            sm = bpy.data.materials.new(pid + "_screen")
            sm.use_nodes = True
            sb = sm.node_tree.nodes["Principled BSDF"]
            sb.inputs["Base Color"].default_value = (0.1, 0.45, 0.6, 1)
            sb.inputs["Emission Color"].default_value = (0.1, 0.45, 0.6, 1)
            sb.inputs["Emission Strength"].default_value = 1.5
            scr.data.materials.append(sm)
            scr.name = pid + "_screen"
            note = "0,24 x 0,16 x 0,012 м; начало координат - центр, экран смотрит в +Z Blender (+Y... см. glTF: экран вверх) = сокет prop.R в tool_use"
        o.name = o.data.name = pid
        o.data.materials.append(mat)
        root = os.path.join(objects_root, pid)
        dst = os.path.join(root, "intact", "assembly", pid + ".gltf")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        bpy.ops.export_scene.gltf(filepath=dst, export_format="GLTF_SEPARATE", export_apply=True, export_yup=True)
        with open(os.path.join(root, "model.json"), "w", encoding="utf-8") as f:
            json.dump({"schema": "elite.model.v1", "id": pid, "title": title, "kind": "prop",
                       "description": f"Реквизит для анимаций людей (tools/people/make_people.py). {note}.",
                       "variants": [{"id": "intact", "title": "Целый", "inGame": "shell"}]}, f, ensure_ascii=False, indent=1)
            f.write("\n")
        print(f"[people] prop {pid} -> {dst}")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opt = {}
    i = 0
    while i < len(argv):
        opt[argv[i].lstrip("-")] = argv[i + 1] if i + 1 < len(argv) else ""
        i += 2
    out = opt.get("out", "D:/__elite/assets_source/models/people")
    work = opt.get("work", "D:/tmp/people")
    only = set(opt["only"].split(",")) if opt.get("only") else None
    os.makedirs(work, exist_ok=True)
    base = os.path.join(work, "people_base.blend")
    build_base()
    bpy.context.scene.render.fps = FPS
    bpy.ops.wm.save_as_mainfile(filepath=base)
    for P in PEOPLE:
        if only and P["id"] not in only:
            continue
        bpy.ops.wm.open_mainfile(filepath=base)
        arm, body = bpy.data.objects["skeleton"], bpy.data.objects["body"]
        s = reshape(arm, body, P)
        make_actions(arm, body, P)
        export_person(arm, body, P, out, s)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, P["id"] + ".blend"))
    if not only or "props" in only:
        make_props(os.path.join(os.path.dirname(out.rstrip("/\\")), "objects"))


main()
