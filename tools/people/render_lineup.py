"""Contact sheet of the people built by make_people.py (reads the per-person .blend files it leaves in the work dir).

blender -b --factory-startup -P tools/people/render_lineup.py -- [--work D:/tmp/people] [--png D:/tmp/people_lineup.png]

Top row: every person in the idle pose, left to right in PEOPLE order. Bottom row: one person per action at a telling
frame, with the props (prop_box / prop_tool) put on the socket bone prop.R by a Copy Transforms constraint (the
socket's head = the glTF node)."""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
opt = dict(zip([a.lstrip("-") for a in argv[0::2]], argv[1::2]))
work = opt.get("work", "D:/tmp/people")
png = opt.get("png", "D:/tmp/people_lineup.png")
ids = opt.get("ids", "").split(",") if opt.get("ids") else None

src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "make_people.py"), encoding="utf-8").read()
PEOPLE_IDS = [l.split('"')[1] for l in src.splitlines() if l.strip().startswith('person("person_')]
ids = ids or [i for i in PEOPLE_IDS if os.path.exists(os.path.join(work, i + ".blend"))]
ROW2 = [("person_m_athletic", "walk", 9), ("person_f_athletic", "run", 6), ("person_child_small", "wave", 36),
        ("person_m_fat", "carry_idle", 1), ("person_m_muscular", "carry_walk", 10), ("person_f_normal", "tool_use", 20),
        ("person_m_obese", "walk", 25), ("person_child_teen", "run", 16)]
ROW2 = [r for r in ROW2 if r[0] in ids]

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.fps = 30


def load(pid, x, y, action, frame, z=0.0):
    with bpy.data.libraries.load(os.path.join(work, pid + ".blend")) as (src_data, dst):
        dst.objects = ["skeleton", "body"]
    arm, body = dst.objects
    for o in (arm, body):
        sc.collection.objects.link(o)
    arm.location = (x, y, z)
    act = None
    for tr in arm.animation_data.nla_tracks:
        for st in tr.strips:
            if st.action and st.action.name.split(".")[0] == action:
                act = st.action
        tr.mute = True
    arm.animation_data.action = act
    if act and act.slots and not arm.animation_data.action_slot:
        arm.animation_data.action_slot = act.slots[0]
    return arm, body


def prop(kind, arm):
    if kind == "box":
        bpy.ops.mesh.primitive_cube_add(size=1)
        o = bpy.context.object
        o.scale = (0.40, 0.30, 0.30)
        col = (0.55, 0.42, 0.12, 1)
    else:
        bpy.ops.mesh.primitive_cube_add(size=1)
        o = bpy.context.object
        o.scale = (0.24, 0.16, 0.012)
        col = (0.1, 0.45, 0.6, 1)
    bpy.ops.object.transform_apply(scale=True)
    o.color = col
    mat = bpy.data.materials.new(kind)
    mat.diffuse_color = col
    o.data.materials.append(mat)
    c = o.constraints.new("COPY_TRANSFORMS")
    c.target = arm
    c.subtarget = "prop.R"
    return o


ROW2_Z = 2.5
frames = {}
x = 0.0
for pid in ids:
    arm, body = load(pid, x, 0, "idle", 1)
    frames[arm.name] = 1
    x += 1.15
width = max(x, 1.15 * len(ROW2))
x2 = 0.0
gap2 = width / max(1, len(ROW2))
row2 = []
for pid, action, fr in ROW2:
    arm, body = load(pid, x2 + gap2 / 2 - 0.55, 0, action, fr, ROW2_Z)
    if action.startswith("carry"):
        prop("box", arm)
    if action == "tool_use":
        prop("tool", arm)
    row2.append((arm, fr))
    x2 += gap2
# per-object frames: offset each action by its NLA-free time via the action's frame range (one scene frame for all):
# shift keys by retiming - simplest is a time offset modifier-free trick: bake each to the wanted frame by evaluating
sc.frame_set(1)
for arm, fr in row2:
    act = arm.animation_data.action
    if act:
        # move the whole action so the wanted frame lands on scene frame 1
        for layer in act.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for fc in bag.fcurves:
                        for kp in fc.keyframe_points:
                            kp.co.x -= fr - 1
                            kp.handle_left.x -= fr - 1
                            kp.handle_right.x -= fr - 1
sc.frame_set(1)

sc.render.engine = "BLENDER_WORKBENCH"
sc.display.shading.light = "STUDIO"
sc.display.shading.color_type = "TEXTURE"
sc.display.shading.show_shadows = True
sc.render.film_transparent = False
sc.world = bpy.data.worlds.new("w")
sc.world.color = (0.18, 0.19, 0.21)
sc.render.resolution_x = 2600
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
sc.collection.objects.link(cam)
sc.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = max(width + 0.4, 4.8)
sc.render.resolution_y = int(2600 * 4.8 / (width + 0.4))
cx = width / 2 - 0.575
yaw = math.radians(15)
d = 30
cam.location = (cx + d * math.sin(yaw), -d * math.cos(yaw), 2.3)
cam.rotation_euler = (math.radians(90), 0, yaw)
for z in (0.0, ROW2_Z):
    bpy.ops.mesh.primitive_plane_add(size=1, location=(cx, 0, z - 0.002))
    bpy.context.object.scale = (width + 4, 2.4, 1)
    bpy.context.object.color = (0.3, 0.31, 0.33, 1)
sc.display.shading.color_type = "OBJECT"
sc.display.shading.color_type = "TEXTURE"
sc.render.filepath = png
bpy.ops.render.render(write_still=True)
print("[lineup]", png, ids)
