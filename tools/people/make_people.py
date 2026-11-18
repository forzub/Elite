"""People: body variants of the milo_002 astronaut with one shared humanoid skeleton, skin weights and baked actions.

blender -b --factory-startup -P tools/people/make_people.py -- [--only id,id] [--out <models root>] [--work <tmp dir>]
    [--reuse 1]   (reuse <work>/people_base.blend of an earlier run: the weights step takes a while)
    PEOPLE_SHAPE_DEBUG=1 in the environment prints the body model and every spread step of the audit

The source (D:/__elite/_data/objects/spacemans/milo_002/astronaut.glb, ASSET_LICENSES/models/astronauts/milo_002) is read
only. Steps:
  1. base: import, fit the skeleton (BONES, metres, Blender Z up, the face looks -Y, .L = +X), automatic weights plus
     fixes (rigid small parts, helmet -> head, backpack -> chest, boots -> foot, no cross-side weights), saved to
     <work>/people_base.blend;
  2. per person (PEOPLE): re-proportion by pose-bone scaling (inherit scale off, so a bone's scale moves its children
     but does not scale them), bake it into the mesh and the rest pose, add the belly / bust, scale to the height, put
     the soles on the floor;
  3. actions (ACTIONS): idle, walk, run, wave, carry_idle, carry_walk, tool_use, dance, jumping_jacks, jump, bow - FK
     for the torso, FK or leg IK (planted feet) for the legs, FK or two-bone IK for the arms; the socket bone prop.R
     (right palm) is animated too: in carry_* it sits in the middle of the box held in both hands (axes = the chest's),
     in tool_use it holds the tablet facing the eyes. Every baked frame goes through the clearance audit (the arms
     against the person's own torso, helmet and thighs; see "clearance audit"): a clip that brings an arm into the
     body is baked again with the arms spread a step wider / the elbows swung off the body; the table is printed and
     written to <work>/people_clearance.json, the per-clip result into the passport (animations[].clearance_cm);
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


def ik_chain(arm, upper, lower, target, pole):
    """two-bone IK: the end of `lower` to target (armature space), the middle joint bent toward pole"""
    update()
    pb = arm.pose.bones
    S = pb[upper].head.copy()
    a = pb[upper].length
    b = pb[lower].length
    d = max(1e-4, min((target - S).length, (a + b) * 0.999))
    dirv = (target - S).normalized()
    along = (a * a - b * b + d * d) / (2 * d)
    h = math.sqrt(max(0.0, a * a - along * along))
    perp = pole - dirv * pole.dot(dirv)
    perp = perp.normalized() if perp.length > 1e-6 else Vector((0, 0, -1))
    E = S + dirv * along + perp * h
    W = S + dirv * d
    aim(arm, upper, E - S)
    aim(arm, lower, W - E)


def ik_arm(arm, s, wrist, pole, hand_dir=None, hand_up=None):
    ik_chain(arm, "upper_arm" + s, "forearm" + s, wrist, pole)
    if hand_dir is not None:
        aim(arm, "hand" + s, hand_dir, hand_up)


def ik_leg(arm, s, ankle, toe_deg=0.0):
    """the ankle (foot head) to `ankle`, the knee forward (a little outward); the foot keeps its rest orientation
    (flat on the floor) turned by toe_deg about the armature X (+ = toes down)"""
    sx = 1.0 if s == ".L" else -1.0
    ik_chain(arm, "thigh" + s, "shin" + s, ankle, Vector((0.12 * sx, -1.0, 0.0)))
    pb = arm.pose.bones["foot" + s]
    rest = pb.bone.matrix_local.to_3x3()
    m = (Matrix.Rotation(math.radians(-toe_deg), 3, "X") @ rest).to_4x4()
    m.translation = pb.matrix.translation
    pb.matrix = m
    update()


# ----------------------------------------------------------------------------------------------- clearance audit
# Nothing in the rig itself keeps an arm out of the body (keyframed FK / IK poses only), so the generator checks every
# frame: the body is modelled from the person's own mesh - the torso (hips, spine, chest, neck, head) as stacked
# rounded slices (a superellipse cross-section per slice, the back capped so the backpack does not count), the limbs
# as capsules - and an arm segment (upper arm beyond the shoulder, forearm, hand) is tested against the torso slices,
# the head and both thighs. A clip that comes closer than CLEAR_MARGIN gets its arms spread a step wider and is baked
# again (pose functions read SPREAD), up to SPREAD_MAX steps.
TORSO = ("hips", "spine", "chest", "neck", "head")
ARM_SEGS = ("upper_arm", "forearm", "hand")
CLEAR_MARGIN = 0.005   # metres
SPREAD_MAX = 16
SLICES = 12            # torso slices from the crotch to the top of the neck
SUPER_N = 2.0          # the slice cross-section: |x/rx|^n + |z/rz|^n = 1 (2 = ellipse, higher = boxier)


def pct(xs, q):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, max(0, int(round(q / 100.0 * (len(xs) - 1)))))]


def side_frame(bone):
    """a frame for a torso bone: y along the bone, x toward the armature +X (the person's left), z = x cross y (the
    front, -Y); returned as the 3x3 that turns the bone's rest-local coordinates into it"""
    y = (bone.tail_local - bone.head_local).normalized()
    x = Vector((1, 0, 0))
    x = (x - y * x.dot(y)).normalized()
    z = x.cross(y)
    F = Matrix((x, y, z))   # rows: armature -> this frame
    return F @ bone.matrix_local.to_3x3()


def body_shape(arm, body):
    """the person's body for the audit, from its rest mesh:
    torso -> {"slices": {bone: [(y0, y1, cx, rx, cz, rz)]}, "frames": {bone: 3x3}} in side_frame coordinates (from the
    bone head): every vertex of the trunk (no arm weight; the legs only above the hip joints - the hand passes the
    pelvis there) cut into SLICES horizontal slices, each with its width, front and back (the back capped: the
    backpack); the head (helmet) one slice; limbs -> (cx, cz, r, y0, y1) capsules in the bone's rest frame"""
    w = weights_of(body)
    B = arm.data.bones
    arm_names = {n + s for n in ("shoulder",) + ARM_SEGS for s in (".L", ".R")}
    leg_names = {n + s for n in ("thigh", "shin", "foot") for s in (".L", ".R")}
    hip_z = B["thigh.L"].head_local.z
    trunk, head, limbs = [], [], {}
    for v in body.data.vertices:
        d = w[v.index]
        if not d:
            continue
        armw = sum(x for k, x in d.items() if k in arm_names)
        legw = sum(x for k, x in d.items() if k in leg_names)
        n, x = max(d.items(), key=lambda kv: kv[1])
        if x >= 0.5 and n not in TORSO and (n in arm_names or n in leg_names):
            limbs.setdefault(n, []).append(v.co.copy())
        if armw >= 0.5:
            continue
        if d.get("head", 0) >= 0.5:
            head.append(v.co.copy())
        elif legw < 0.5 or v.co.z >= hip_z:
            trunk.append(v.co.copy())
    chain = ("hips", "spine", "chest", "neck")
    frames = {n: side_frame(B[n]) for n in chain + ("head",)}
    slices = {n: [] for n in chain}
    shape_head = []

    def cut(n, pts, a, b):
        bone = B[n]
        F = frames[n] @ bone.matrix_local.to_3x3().transposed()   # armature -> side frame
        loc = [F @ (p - bone.head_local) for p in pts]
        # extremes, not the bulk: the backpack has far more vertices than the front of the suit
        x_lo, x_hi = pct([p.x for p in loc], 0.5), pct([p.x for p in loc], 99.5)
        z_lo, z_hi = pct([p.z for p in loc], 0.5), pct([p.z for p in loc], 99.5)
        z_lo = min(max(z_lo, -0.7 * (x_hi - x_lo) / 2), z_hi - 0.02)   # the back without the backpack
        ya, yb = (F @ (Vector((0, 0, a)) - bone.head_local)).y, (F @ (Vector((0, 0, b)) - bone.head_local)).y
        if n == "head":   # the helmet: an ellipsoid (a, b = its bottom and top along the bone)
            ylo, yhi = min(ya, yb), max(ya, yb)
            shape_head[:] = [((x_lo + x_hi) / 2, (ylo + yhi) / 2, (z_lo + z_hi) / 2, (x_hi - x_lo) / 2, (yhi - ylo) / 2,
                              (z_hi - z_lo) / 2)]
            return
        slices[n].append((min(ya, yb), max(ya, yb), (x_lo + x_hi) / 2, max(0.01, (x_hi - x_lo) / 2),
                          (z_lo + z_hi) / 2, max(0.01, (z_hi - z_lo) / 2)))

    zs = [p.z for p in trunk]
    # up to the armpits: above them are the collar and the shoulder pads, which the shoulder joint deforms (the helmet
    # is its own ellipsoid)
    z0, z1 = pct(zs, 1), min(pct(zs, 99), B["upper_arm.L"].head_local.z - 0.04)
    for i in range(SLICES):
        a, b = z0 + (z1 - z0) * i / SLICES, z0 + (z1 - z0) * (i + 1) / SLICES
        ps = [p for p in trunk if a <= p.z <= b]
        if len(ps) < 12:
            continue
        zm = (a + b) / 2
        n = next((c for c in chain if zm < B[c].tail_local.z), "neck")
        cut(n, ps, a, b)
    if len(head) >= 12:
        hz = [p.z for p in head]
        cut("head", head, pct(hz, 1), pct(hz, 99))
    shape = {"slices": {n: v for n, v in slices.items() if v}, "frames": frames}
    if shape_head:
        shape["helmet"] = shape_head[0]
    for n, pts in limbs.items():
        if len(pts) < 20:
            continue
        inv = B[n].matrix_local.inverted()
        loc = [inv @ p for p in pts]
        ys = [p.y for p in loc]
        x_lo, x_hi = pct([p.x for p in loc], 12), pct([p.x for p in loc], 88)   # the sleeve, not the cuffs / pads
        z_lo, z_hi = pct([p.z for p in loc], 12), pct([p.z for p in loc], 88)
        shape[n] = ((x_lo + x_hi) / 2, (z_lo + z_hi) / 2, ((x_hi - x_lo) + (z_hi - z_lo)) / 4, pct(ys, 2), pct(ys, 98))
    if os.environ.get("PEOPLE_SHAPE_DEBUG"):
        for n, v in shape["slices"].items():
            print("[shape]", n, [tuple(round(x, 3) for x in t) for t in v])
        for n, v in shape.items():
            if n not in ("slices", "frames", "helmet", "rest"):
                print("[shape]", n, tuple(round(x, 3) for x in v))
    return shape


def torso_sdf(slices, q):
    """signed distance (approx.) of a side-frame point q to the stacked rounded slices"""
    best = 1e9
    for (a, b, cx, rx, cz, rz) in slices:
        ox, oz = q.x - cx, q.z - cz
        r = math.hypot(ox, oz)
        rho = ((abs(ox) / rx) ** SUPER_N + (abs(oz) / rz) ** SUPER_N) ** (1.0 / SUPER_N)
        radial = r * (1.0 - 1.0 / rho) if rho > 1e-6 else -min(rx, rz)
        axial = max(a - q.y, q.y - b)
        d = math.hypot(radial, axial) if radial > 0 and axial > 0 else max(radial, axial)
        best = min(best, d)
    return best


def ellipsoid_sdf(e, q):
    cx, cy, cz, rx, ry, rz = e
    p = Vector((q.x - cx, q.y - cy, q.z - cz))
    k0 = Vector((p.x / rx, p.y / ry, p.z / rz)).length
    k1 = Vector((p.x / rx ** 2, p.y / ry ** 2, p.z / rz ** 2)).length
    return k0 * (k0 - 1.0) / k1 if k1 > 1e-9 else -min(rx, ry, rz)


def limb_sdf(cap, q):
    cx, cz, r, y0, y1 = cap
    t = max(y0, min(y1, q.y))
    return math.sqrt((q.x - cx) ** 2 + (q.y - t) ** 2 + (q.z - cz) ** 2) - r


def clearance(arm, shape, raw=False, side=None):
    """the smallest gap (metres, < 0 = inside) between the arms and the body at the current pose, and what it is.
    A sample already inside in the rest pose (the armpit of a thick arm on a wide chest) counts only for what it sinks
    deeper than there (shape["rest"], the raw gaps of the rest pose: raw=True returns them)"""
    update()
    pb = arm.pose.bones
    bodies = []
    for n, sl in shape["slices"].items():
        if n == "neck":   # the collar / shoulder pads round the joint: the shoulder deforms there, not a collision
            continue
        bodies.append((n, shape["frames"][n] @ pb[n].matrix.inverted().to_3x3(), pb[n].matrix.translation, sl, torso_sdf))
    if "helmet" in shape:
        bodies.append(("head", shape["frames"]["head"] @ pb["head"].matrix.inverted().to_3x3(), pb["head"].matrix.translation,
                       shape["helmet"], ellipsoid_sdf))
    for n in ("thigh.L", "thigh.R"):
        if n in shape:
            bodies.append((n, pb[n].matrix.inverted(), None, shape[n], limb_sdf))
    rest = shape.get("rest")
    out = []
    worst = (1e9, "")
    for s in (".L", ".R"):
        for seg in ARM_SEGS:
            n = seg + s
            if n not in shape:
                continue
            cx, cz, r, y0, y1 = shape[n]
            m = pb[n].matrix
            start = y0 + (y1 - y0) * (0.4 if seg == "upper_arm" else 0.0)
            for i in range(7):
                q = m @ Vector((cx, start + (y1 - start) * i / 6, cz))
                for t, M, origin, data, fn in bodies:
                    if seg == "upper_arm" and i < 3 and t in ("hips", "spine", "chest"):   # the shoulder region
                        continue
                    d = fn(data, M @ (q - origin) if origin is not None else M @ q) - r
                    if rest is not None:
                        d -= min(0.0, rest[len(out)])
                    out.append(d)
                    if d < worst[0] and (side is None or s == side):
                        worst = (d, f"{n}/{t}")
    return out if raw else worst


CUR_SHAPE = {}   # the person's body while its clips are baked (ik_arm_safe)


def ik_arm_safe(arm, s, wrist, pole, hand_dir=None, hand_up=None):
    """ik_arm, and when the arm then comes closer to the body than CLEAR_MARGIN, the elbow is swung about the
    shoulder-wrist line (the same wrist, another pole: up to +-120 deg in 15 deg steps) to the swing that keeps the arm
    farthest from the body - how a person lifts the elbow off the belly"""
    ik_arm(arm, s, wrist, pole, hand_dir, hand_up)
    shape = CUR_SHAPE.get("shape") if SPREAD["k"] else None   # spread 0 = the clip as designed (the "before")
    if not shape:
        return
    c0 = clearance(arm, shape, side=s)[0]
    if c0 >= CLEAR_MARGIN:
        return
    S = arm.pose.bones["upper_arm" + s].head.copy()
    axis = (wrist - S).normalized()
    best = (c0, 0)
    for deg in [x for k in range(1, 9) for x in (15 * k, -15 * k)]:
        q = Quaternion(axis, math.radians(deg))
        ik_arm(arm, s, wrist, q @ pole, hand_dir, hand_up)
        c = clearance(arm, shape, side=s)[0]
        if c > best[0] + 1e-4:
            best = (c, deg)
        if c >= CLEAR_MARGIN and abs(deg) <= abs(best[1]):
            best = (c, deg)
            break
    ik_arm(arm, s, wrist, Quaternion(axis, math.radians(best[1])) @ pole, hand_dir, hand_up)


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


SPREAD = {"k": 0, "alt": 0}   # the clip's spread step while it is baked (see the clearance audit)
AUDIT = {}          # clip -> {"before": (gap, pair), "after": (gap, pair), "spread": k}


def cycle(arm, name, seconds, pose_fn, step=2, shape=None, alts=1):
    """bake the clip; with a shape, every keyed frame is audited and the clip is baked again with SPREAD k = 1, 2, ..
    (and, for a clip with alts > 1, with its alternative remedy SPREAD alt = 1, ..) until the arms keep CLEAR_MARGIN;
    when nothing gets there, the best variant is kept"""
    n = max(2, round(seconds * FPS))

    def bake(k, alt):
        SPREAD["k"], SPREAD["alt"] = k, alt
        kr = Keyer(arm, name)
        worst = (1e9, "")
        for fr in list(range(0, n, step)) + [n]:
            rest_pose(arm)
            pose_fn(fr / n, arm.pose.bones)
            if shape:
                c = clearance(arm, shape)
                if c[0] < worst[0]:
                    worst = (c[0], c[1] + f"@{fr / FPS:.2f}s")
            kr.key(fr + 1)
        if os.environ.get("PEOPLE_SHAPE_DEBUG"):
            print("[spread]", name, alt, k, round(worst[0] * 100, 1), worst[1])
        return kr, worst

    def drop(kr):
        arm.animation_data.action = None
        bpy.data.actions.remove(kr.act)

    kr, before = bake(0, 0)
    best = (before[0], 0, 0)
    if shape and before[0] < CLEAR_MARGIN:
        drop(kr)
        kr = None
        for alt in range(alts):
            for k in range(1, SPREAD_MAX + 1):
                kr, worst = bake(k, alt)
                if worst[0] > best[0]:
                    best = (worst[0], k, alt)
                if worst[0] >= CLEAR_MARGIN:
                    break
                drop(kr)
                kr = None
            if kr:
                break
        if not kr:
            kr, worst = bake(best[1], best[2])
    kr.done()
    after = before if best[1] == 0 and best[2] == 0 else worst
    AUDIT[name] = {"before": before, "after": after, "spread": best[1], "alt": best[2]}
    SPREAD["k"], SPREAD["alt"] = 0, 0


def keyed(p, ks):
    """value at p of smoothstep-eased keys [(p, value), ...]"""
    if p <= ks[0][0]:
        return ks[0][1]
    for (t0, a0), (t1, a1) in zip(ks, ks[1:]):
        if p <= t1:
            u = (p - t0) / (t1 - t0) if t1 > t0 else 1.0
            u = u * u * (3 - 2 * u)
            return a0 + (a1 - a0) * u
    return ks[-1][1]


def make_actions(arm, body, P):
    D = dims(arm)
    # arms hang lower on a slim body, stay out from a wide one (the source holds them ~33 deg below the horizontal)
    down = max(18.0, min(48.0, 46.0 - 34.0 * (max(P["waist_w"], P["chest_w"], P["arm_t"], P["hip_w"] * 0.9) - 1.0)))
    leg = D["leg"]
    sc = leg / 0.66   # the source's leg is ~0.66 m
    shape = body_shape(arm, body)
    AUDIT.clear()
    rest_pose(arm)
    shape["rest"] = clearance(arm, shape, raw=True)
    CUR_SHAPE["shape"] = shape
    c = clearance(arm, shape)
    AUDIT["(rest)"] = {"before": c, "after": c, "spread": 0}
    B = arm.data.bones
    ankle = {s: B["foot" + s].head_local.copy() for s in (".L", ".R")}

    def sp():
        return 4.0 * SPREAD["k"]

    def abd(deg):
        """arm abduction (degrees below the rest line, ~33 deg under the horizontal; -33 = horizontal, -123 = straight
        up) moved away from the body by the audit's spread: a hanging arm goes out, a raised one comes down from the
        head - never past the horizontal"""
        return deg - sp() if deg > -33.0 else min(-33.0, deg + sp())

    def arm_fk(pb, s, a, flex, elbow):
        """a: abduction (see abd, already spread), flex: + = back / - = forward, elbow: - = bent"""
        q = qa((1, 0, 0), flex) @ qa((0, 1, 0), a)
        f = qa((1, 0, 0), elbow)
        set_rot(pb["upper_arm" + s], q if s == ".L" else mir(q))
        set_rot(pb["forearm" + s], f if s == ".L" else mir(f))

    def hips_move(pb, off, rot=None):
        """the pelvis (and all above it) moved by off (armature space, metres), turned by rot (armature space)"""
        pb["hips"].location = pb["hips"].bone.matrix_local.to_3x3().transposed() @ off
        if rot is not None:
            set_rot(pb["hips"], rot)

    def plant(dl=Vector(), dr=Vector(), toe_l=0.0, toe_r=0.0):
        """the feet by IK at their rest place + offsets (a planted foot stays where it stands whatever the pelvis does)"""
        ik_leg(arm, ".L", ankle[".L"] + dl, toe_l)
        ik_leg(arm, ".R", ankle[".R"] + dr, toe_r)

    def idle(p, pb):
        br = math.sin(2 * math.pi * p)
        set_rot(pb["chest"], qa((1, 0, 0), -1.2 * br))
        set_rot(pb["head"], qa((0, 0, 1), 6 * math.sin(2 * math.pi * p + 0.6)) @ qa((1, 0, 0), 1.5 * br))
        arms_down(pb, abd(down) + 1.5 * br)
        pb["hips"].location = (0, 0.004 * sc * br, 0)

    def walk(p, pb):
        legs(pb, p, 20, 10, 48)
        c = math.cos(2 * math.pi * p)
        pb["hips"].location = (0, -0.02 * sc * (0.5 + 0.5 * math.cos(4 * math.pi * p)), 0)
        set_rot(pb["hips"], qa((0, 0, 1), -5 * c))
        set_rot(pb["chest"], qa((0, 0, 1), 7 * c) @ qa((1, 0, 0), 2))
        set_rot(pb["upper_arm.L"], qa((1, 0, 0), 16 * c) @ qa((0, 1, 0), abd(down)))
        set_rot(pb["upper_arm.R"], mir(qa((1, 0, 0), -16 * c) @ qa((0, 1, 0), abd(down))))
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
        set_rot(pb["upper_arm.L"], qa((1, 0, 0), 40 * c) @ qa((0, 1, 0), abd(down)))
        set_rot(pb["upper_arm.R"], mir(qa((1, 0, 0), -40 * c) @ qa((0, 1, 0), abd(down))))
        set_rot(pb["forearm.L"], qa((1, 0, 0), -70 - 15 * c))
        set_rot(pb["forearm.R"], mir(qa((1, 0, 0), -70 + 15 * c)))

    def wave(p, pb):
        set_rot(pb["upper_arm.L"], qa((1, 0, 0), -6) @ qa((0, 1, 0), abd(down)))
        set_rot(pb["forearm.L"], qa((1, 0, 0), -14))
        set_rot(pb["head"], qa((0, 0, 1), -8) @ qa((0, 1, 0), 4))
        set_rot(pb["chest"], qa((0, 1, 0), -3))
        rise = min(1.0, p / 0.18) if p < 0.5 else min(1.0, (1.0 - p) / 0.18)
        rise = rise * rise * (3 - 2 * rise)
        sw = math.sin(2 * math.pi * 4 * p) * rise
        S = D["sh_R"]
        L = D["armlen"]
        out = 0.07 * SPREAD["k"]
        lo = S + Vector((-0.12 - out, -0.08, -0.85)).normalized() * L * 0.95
        up = S + Vector((-0.45 - 0.08 * SPREAD["k"], -0.25, 0.75 - 0.03 * SPREAD["k"])).normalized() * L * 0.93 + Vector((-0.05 * sw * L, 0, 0))
        W = lo.lerp(up, rise)
        hand_dir = Vector((0, -0.25, -1)).lerp(Vector((-0.35 * sw, -0.1, 1)), rise)
        update()
        ik_arm_safe(arm, ".R", W, Vector((-1 - 0.3 * SPREAD["k"], 0.3, -0.6)), hand_dir, Vector((0, -1, 0)))

    torso_front = front_y(body, D["hips"].z, D["chest"].z + 0.1 * sc, 0.25 * sc)
    box = Vector((0.40, 0.30, 0.30))   # prop_box: width (x), depth (y), height (z), metres

    def carry_pose(pb):
        """both hands on the box's sides; the audit's spread moves the box forward and the elbows out"""
        k = SPREAD["k"]
        update()
        C = Vector((0.0, torso_front - box.y / 2 - 0.03 - (0.01 if not SPREAD["alt"] else 0.03) * k,
                   (D["spine"].z + D["chest"].z) / 2 + 0.02 * sc + (0.025 if not SPREAD["alt"] else 0.005) * k))
        for s, sx in ((".L", 1), (".R", -1)):
            W = C + Vector((sx * (box.x / 2 + 0.02), 0.02 + 0.01 * k, -0.02))
            ik_arm_safe(arm, s, W, Vector((sx * (1.0 + 0.6 * k), 0.6 / (1 + k), -0.8 / (1 + 0.2 * k))), Vector((0.0, -1, -0.15)), Vector((-sx * 1.0, 0, 0)))
        update()
        m = Matrix.Translation(C)
        pb["prop.R"].matrix = m   # the box's centre, its axes = the armature's (upright, facing the front)
        update()
        return {n: (pb[n].rotation_quaternion.copy(), pb[n].location.copy()) for n in
                ("upper_arm.L", "forearm.L", "hand.L", "upper_arm.R", "forearm.R", "hand.R", "prop.R")}

    carry_cache = {}

    def carry(pb):
        key = (SPREAD["k"], SPREAD["alt"])
        if key not in carry_cache:
            rest_pose(arm)
            carry_cache[key] = carry_pose(pb)
            rest_pose(arm)
        return carry_cache[key]

    def put(pb, saved):
        for n, (q, l) in saved.items():
            pb[n].rotation_quaternion = q
            pb[n].location = l

    def carry_idle(p, pb):
        saved = carry(pb)
        br = math.sin(2 * math.pi * p)
        set_rot(pb["spine"], qa((1, 0, 0), -3 - 0.8 * br))
        pb["hips"].location = (0, 0.004 * sc * br, 0)
        put(pb, saved)

    def carry_walk(p, pb):
        saved = carry(pb)
        legs(pb, p, 16, 8, 42)
        c = math.cos(2 * math.pi * p)
        pb["hips"].location = (0, -0.018 * sc * (0.5 + 0.5 * math.cos(4 * math.pi * p)), 0)
        set_rot(pb["hips"], qa((0, 0, 1), -4 * c))
        set_rot(pb["spine"], qa((1, 0, 0), -3) @ qa((0, 0, 1), 3 * c))
        put(pb, saved)

    def tool_use(p, pb):
        k = SPREAD["k"]
        set_rot(pb["head"], qa((1, 0, 0), 14 + 2 * math.sin(2 * math.pi * p)))
        set_rot(pb["neck"], qa((1, 0, 0), 6))
        update()
        C = D["chest"].lerp(D["chest_top"], 0.25)
        # with the spread the tablet comes to the middle and both hands hold it by its sides (g: 0 -> 1 over 4 steps)
        g = min(1.0, k / 4.0)
        T = Vector((-0.05 * sc * (1.0 - g), torso_front - 0.26 * sc - (0.01 if not SPREAD["alt"] else 0.02) * k,
                    C.z + 0.01 * k))   # tablet centre
        Wr = T + Vector((-0.11 - 0.02 * g, 0.03, -0.03))
        # within 85 % of the reach: a straight arm cannot take its elbow off the flank (the tablet comes nearer)
        S = D["sh_R"]
        lat = (Wr.x - S.x) ** 2 + (Wr.z - S.z) ** 2
        reach = 0.85 * D["armlen"]
        if not SPREAD["alt"] and (Wr - S).length > reach and reach * reach > lat:
            dy = S.y - math.sqrt(reach * reach - lat) - Wr.y
            T.y += dy
            Wr.y += dy
        ik_arm_safe(arm, ".R", Wr, Vector((-1 - 0.6 * k, 0.5 / (1 + k), -0.8 / (1 + 0.2 * k))), Vector((0.25, -1, 0.1)), Vector((0, 0, 1)))
        update()
        pal = pb["prop.R"]
        rot = Matrix.Rotation(math.radians(-40), 4, "X")   # screen (+Z of the tablet) toward the eyes
        pal.matrix = Matrix.Translation(T) @ rot
        update()
        n = (rot.to_3x3() @ Vector((0, 0, 1)))
        tap = 0.5 + 0.5 * math.cos(2 * math.pi * 3 * p)
        Wl = T + Vector((0.06 + 0.03 * math.sin(2 * math.pi * 2 * p), 0.0, 0.0)) + n * (0.10 + 0.04 * tap) + Vector((0.04, 0.02, -0.03))
        # a wide body: the left hand cannot reach across it to tap - with the spread it takes the tablet's left edge
        # instead (both hands on the tablet, the left thumb taps: a small twitch over the screen)
        if g > 0:
            grip = T + Vector((0.13, 0.03, -0.03)) + n * (0.015 * tap)
            Wl = Wl.lerp(grip, g)
        ik_arm_safe(arm, ".L", Wl, Vector((1 + 0.6 * k, 0.5 / (1 + k), -0.8)), Vector((-0.6, -0.8, -0.3)), Vector((0, 0, 1)))

    # ---- the new clips (2026-11-18): whole-body moves with the feet planted by IK (or in the air with the pelvis)
    def dance(p, pb):
        """4 s, 8 beats (120 bpm): step-touch to the left and to the right, a bounce on every beat, the pelvis and the
        chest twisting against each other; beats 1-4 the bent arms pump in turn, beats 5-8 both arms up, swaying"""
        half = int(4 * p) % 2                 # 0: the left foot steps out, 1: the right one
        ph = (4 * p) % 1.0
        side = 1.0 if half == 0 else -1.0
        out = math.sin(math.pi * ph) ** 2    # 0 home .. 1 out .. 0 home
        lift = 0.035 * sc * abs(math.sin(2 * math.pi * ph))
        bounce = -0.012 * sc - 0.03 * sc * (0.5 + 0.5 * math.cos(2 * math.pi * 8 * p))
        twist = math.sin(2 * math.pi * 2 * p)
        hips_move(pb, Vector((side * 0.06 * sc * out, 0.0, bounce)),
                  qa((0, 0, 1), 9 * twist) @ qa((0, 1, 0), side * 4 * out))
        set_rot(pb["spine"], qa((0, 0, 1), -5 * twist) @ qa((0, 1, 0), -side * 3 * out))
        set_rot(pb["chest"], qa((0, 0, 1), -9 * twist) @ qa((1, 0, 0), 3))
        set_rot(pb["head"], qa((1, 0, 0), 5 * math.cos(2 * math.pi * 8 * p)) @ qa((0, 0, 1), 8 * twist))
        # arms: the up-section weight (beats 5..8), eased over half a beat at both ends
        if p < 0.0625:
            wu = 0.5 - 0.5 * keyed(p / 0.0625, [(0, 0), (1, 1)])
        elif p < 0.4375:
            wu = 0.0
        elif p < 0.5625:
            wu = keyed((p - 0.4375) / 0.125, [(0, 0), (1, 1)])
        elif p < 0.9375:
            wu = 1.0
        else:
            wu = 1.0 - 0.5 * keyed((p - 0.9375) / 0.0625, [(0, 0), (1, 1)])
        sway = math.sin(2 * math.pi * 4 * p)
        for s, sg in ((".L", 1.0), (".R", -1.0)):
            a = math.sin(2 * math.pi * 4 * p + (0.0 if s == ".L" else math.pi))   # pump: one arm up per beat
            # the spread also opens the elbows: a bent forearm swings the hand up to the (big) helmet
            ease = max(0.2, 1.0 - 0.05 * SPREAD["k"])
            pump = (down - 30.0, (-35.0 - 30.0 * a) * ease, (-95.0 + 15.0 * a) * ease)
            up = (-100.0 - sg * 14.0 * sway, -10.0, -25.0 * ease)
            if SPREAD["alt"]:   # low arms: swinging at the sides, the "up" half only out to the sides
                pump = (down - 15.0, -20.0 - 25.0 * a, -35.0 + 10.0 * a)
                up = (-50.0 - sg * 10.0 * sway, -5.0, -10.0)
            arm_fk(pb, s, abd(pump[0] + (up[0] - pump[0]) * wu), pump[1] + (up[1] - pump[1]) * wu,
                   pump[2] + (up[2] - pump[2]) * wu)
            set_rot(pb["hand" + s], qa((1, 0, 0), 20 * wu * math.sin(2 * math.pi * 8 * p)) if s == ".L"
                    else mir(qa((1, 0, 0), 20 * wu * math.sin(2 * math.pi * 8 * p))))
        step = Vector((side * 0.16 * sc * out, 0.0, lift))
        plant(step if half == 0 else Vector(), step if half == 1 else Vector(),
              toe_l=12 * lift / (0.035 * sc) if half == 0 else 0.0, toe_r=12 * lift / (0.035 * sc) if half == 1 else 0.0)

    def jumping_jacks(p, pb):
        """1 s: hop out (feet apart, arms up over the head), hop in (feet together, arms down); two short flights"""
        o = 0.5 - 0.5 * math.cos(2 * math.pi * p)
        s2 = abs(math.sin(2 * math.pi * p))
        hop = 0.07 * sc * max(0.0, (s2 - 0.35) / 0.65) ** 1.5
        absorb = 0.045 * sc * (1.0 - s2) ** 2
        wide = 0.20 * sc * o
        sag = leg - math.sqrt(max(0.0, leg * leg - wide * wide))
        hips_move(pb, Vector((0, 0, hop - absorb - sag)))
        set_rot(pb["head"], qa((1, 0, 0), -3 * o))
        for s, sg in ((".L", 1.0), (".R", -1.0)):
            arm_fk(pb, s, abd(down + ((-112.0 if not SPREAD["alt"] else -78.0) - down) * o), -6.0, (-12.0 - 10.0 * o) * max(0.1, 1.0 - 0.08 * SPREAD["k"]))
            f = qa((1, 0, 0), (0 if SPREAD["alt"] else 25) * math.sin(2 * math.pi * 2 * p) * o)   # the hands flap at the top
            set_rot(pb["hand" + s], f if s == ".L" else mir(f))
        toe = 25 * hop / (0.07 * sc)
        plant(Vector((wide, 0, hop)), Vector((-wide, 0, hop)), toe, toe)

    T_JUMP = 1.6
    t_take, t_land = 0.46, 0.80
    flight = (t_land - t_take) * T_JUMP
    H_JUMP = 9.81 * flight * flight / 8.0   # the peak of a ballistic flight of that time (g = 9.81)

    def jump(p, pb):
        """1.6 s: stand, counter-movement squat with the arms swung back, push-off onto the toes swinging the arms up,
        a ballistic flight (knees tucked a little), landing with a squat, standing up"""
        crouch = 0.30 * leg
        toes = 0.05 * sc
        if p < t_take:
            hip = keyed(p, [(0.0, 0.0), (0.08, 0.0), (0.36, -crouch), (t_take, toes)])
            feet = keyed(p, [(0.0, 0.0), (0.40, 0.0), (t_take, toes)])
            toe = keyed(p, [(0.0, 0.0), (0.40, 0.0), (t_take, 35.0)])
        elif p < t_land:
            u = (p - t_take) / (t_land - t_take)
            h = 4.0 * H_JUMP * u * (1.0 - u)
            base = toes * (1.0 - u)
            hip = base + h
            tuck = 0.0 if SPREAD["alt"] else 0.12 * leg / (1.0 + 0.15 * SPREAD["k"])
            feet = max(0.0, base + h - tuck * math.sin(math.pi * u))   # the knee tuck
            toe = 35.0 * (1.0 - u) + 10.0 * math.sin(math.pi * u)
        else:
            hip = keyed(p, [(t_land, 0.0), (0.88, -0.5 * crouch), (1.0, 0.0)])
            feet, toe = 0.0, 0.0
        cr = max(0.0, -hip / crouch)
        hips_move(pb, Vector((0, 0.30 * cr * crouch, hip)), qa((1, 0, 0), 12 * cr))
        set_rot(pb["spine"], qa((1, 0, 0), 14 * cr))
        set_rot(pb["chest"], qa((1, 0, 0), 8 * cr))
        set_rot(pb["head"], qa((1, 0, 0), -18 * cr))
        flex = keyed(p, [(0.0, -8.0), (0.08, -8.0), (0.34, 50.0), (t_take, -95.0), (0.62, -105.0), (t_land, -70.0), (0.9, -40.0), (1.0, -8.0)])
        out = keyed(p, [(0.0, down), (0.34, down), (0.5, down - 25.0), (t_land, down - 20.0), (1.0, down)])
        elbow = keyed(p, [(0.0, -14.0), (0.34, -8.0), (0.5, -30.0), (t_land, -35.0), (1.0, -14.0)])
        if SPREAD["alt"]:
            flex = max(flex, -90.0)
        for s in (".L", ".R"):   # the spread moves out the arms hanging by the body, not the ones swung up
            arm_fk(pb, s, out - sp() * (1.0 if flex >= 0 else max(0.4, 1.0 + flex / 100.0)), flex, elbow)
        plant(Vector((0, 0, feet)), Vector((0, 0, feet)), toe, toe)

    def bow(p, pb):
        """2.6 s: a bow from the hips (about 40 deg in all), the arms along the body, held, straightened up"""
        w = keyed(p, [(0.0, 0.0), (0.15, 0.0), (0.40, 1.0), (0.62, 1.0), (0.88, 0.0), (1.0, 0.0)])
        hips_move(pb, Vector((0, 0.07 * sc * w, -0.01 * sc * w)), qa((1, 0, 0), 16 * w))
        set_rot(pb["spine"], qa((1, 0, 0), 12 * w))
        set_rot(pb["chest"], qa((1, 0, 0), 10 * w))
        set_rot(pb["neck"], qa((1, 0, 0), 5 * w))
        set_rot(pb["head"], qa((1, 0, 0), 10 * w))
        for s in (".L", ".R"):
            arm_fk(pb, s, abd(down), 26 * w - 4, -10 - 6 * w)
        plant()

    cycle(arm, "idle", 3.0, idle, step=3, shape=shape)
    cycle(arm, "walk", 1.1, walk, shape=shape)
    cycle(arm, "run", 0.7, run, step=1, shape=shape)
    cycle(arm, "wave", 2.4, wave, step=1, shape=shape)
    cycle(arm, "carry_idle", 3.0, carry_idle, step=3, shape=shape, alts=2)
    cycle(arm, "carry_walk", 1.2, carry_walk, shape=shape, alts=2)
    cycle(arm, "tool_use", 3.0, tool_use, step=2, shape=shape, alts=2)
    cycle(arm, "dance", 4.0, dance, step=2, shape=shape, alts=2)
    cycle(arm, "jumping_jacks", 1.0, jumping_jacks, step=1, shape=shape, alts=2)
    cycle(arm, "jump", T_JUMP, jump, step=1, shape=shape, alts=2)
    cycle(arm, "bow", 2.6, bow, step=2, shape=shape)
    rest_pose(arm)
    return box


# id: (title for the editor and the passport, loops, the prop on prop.R)
ACTIONS = {
    "idle": ("стоит, дышит (3 с, петля)", True, None),
    "walk": ("шаг на месте (1,1 с, петля)", True, None),
    "run": ("бег на месте (0,7 с, петля)", True, None),
    "wave": ("машет правой рукой (2,4 с)", False, None),
    "carry_idle": ("держит ящик двумя руками (prop.R = центр ящика), стоит (3 с, петля)", True, "prop_box"),
    "carry_walk": ("несёт ящик, шаг на месте (1,2 с, петля)", True, "prop_box"),
    "tool_use": ("планшет в правой руке (prop.R), левая тычет в экран (3 с, петля)", True, "prop_tool"),
    "dance": ("танцует: шаг в сторону с пружинкой, руки качают, потом руки вверх (4 с, петля)", True, None),
    "jumping_jacks": ("машет руками и ногами: прыжки «ноги врозь — руки вверх» (1 с, петля)", True, None),
    "jump": ("прыжок на месте: присед, толчок, полёт, приземление (1,6 с)", False, None),
    "bow": ("поклон (2,6 с)", False, None),
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
        # per clip: loops or plays once, the prop shown on prop.R, the clearance audit (the smallest gap between the
        # arms and the body over the clip, cm; spread = how many steps the arms were moved out to get it)
        "animations": [dict({"id": k, "title": t, "loop": loop}, **({"prop": prop} if prop else {}),
                            **({"clearance_cm": round(AUDIT[k]["after"][0] * 100, 1), "spread": AUDIT[k]["spread"], "alt": AUDIT[k].get("alt", 0)}
                               if k in AUDIT else {}))
                       for k, (t, loop, prop) in ACTIONS.items()],
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


def print_audit(audit, path):
    """the clearance table: per clip and person the smallest gap before the fix -> after it (cm, < 0 = an arm inside
    the body), the spread steps used; the worst pair and time are in the JSON"""
    if not audit:
        return
    with open(path, "w", encoding="utf-8") as f:
        json.dump(audit, f, ensure_ascii=False, indent=1)
    people = list(audit)
    clips = list(next(iter(audit.values())))
    short = {pid: pid.replace("person_", "") for pid in people}
    print("[people] clearance audit, cm: before -> after (spread steps; a = the alternative remedy)")
    print("clip".ljust(14) + "".join(short[p][:12].rjust(16) for p in people))
    for c in clips:
        row = c.ljust(14)
        for pid in people:
            a = audit[pid].get(c)
            row += (f"{a['before_cm']:.1f}>{a['after_cm']:.1f}" + (f"({a['spread']}{'a' if a.get('alt') else ''})" if a["spread"] else "")).rjust(16) if a else " " * 16
        print(row)
    bad = [(pid, c, a) for pid in people for c, a in audit[pid].items() if a["after_cm"] < 0 and c != "(rest)"]
    print(f"[people] clips with an arm inside the body after the fix: {len(bad)}" +
          "".join(f"\n  {pid} {c}: {a['after_cm']} cm at {a['after_at']}" for pid, c, a in bad))
    print(f"[people] audit -> {path}")


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
    if not (opt.get("reuse") and os.path.isfile(base)):   # --reuse 1: the base of an earlier run (weights take a while)
        build_base()
        bpy.context.scene.render.fps = FPS
        bpy.ops.wm.save_as_mainfile(filepath=base)
    audit = {}
    for P in PEOPLE:
        if only and P["id"] not in only:
            continue
        bpy.ops.wm.open_mainfile(filepath=base)
        arm, body = bpy.data.objects["skeleton"], bpy.data.objects["body"]
        s = reshape(arm, body, P)
        make_actions(arm, body, P)
        audit[P["id"]] = {k: {"before_cm": round(v["before"][0] * 100, 1), "before_at": v["before"][1],
                              "after_cm": round(v["after"][0] * 100, 1), "after_at": v["after"][1], "spread": v["spread"], "alt": v.get("alt", 0)}
                          for k, v in AUDIT.items()}
        export_person(arm, body, P, out, s)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, P["id"] + ".blend"))
    print_audit(audit, os.path.join(work, "people_clearance.json"))
    if not only or "props" in only:
        make_props(os.path.join(os.path.dirname(out.rstrip("/\\")), "objects"))


if __name__ == "__main__":
    main()
