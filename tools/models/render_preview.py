"""Blender preview of an equipment model (geometry + MTL colours, no textures).

usage: blender -b --factory-startup -P tools/models/render_preview.py -- <asset dir> <LOD> <out.png> [view] [hidden,parts] [mesh|hits]
view: pad (three-quarter from the pad side, default) | flange (from the reactor side) | side | top | under
A 1.8 m figure stands next to the unit for scale.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]
asset, lod, out = argv[0], int(argv[1]), argv[2]
view = argv[3] if len(argv) > 3 else "pad"
hide = set(argv[4].split(",")) if len(argv) > 4 else set()   # part names left out (e.g. the pad, to see the body)

bpy.ops.wm.read_factory_settings(use_empty=True)
folder = os.path.join(asset, f"LOD{lod}")
for f in sorted(os.listdir(folder)):
    if f.endswith(".obj"):
        bpy.ops.wm.obj_import(filepath=os.path.join(folder, f), forward_axis="NEGATIVE_Z", up_axis="Y")

# MTL Kd -> viewport colour (Workbench MATERIAL colour)
for m in bpy.data.materials:
    if m.use_nodes and m.node_tree:
        b = m.node_tree.nodes.get("Principled BSDF")
        if b:
            m.diffuse_color = tuple(b.inputs["Base Color"].default_value)
            m.roughness = 0.55
            m.metallic = 0.0

# mode "hits": draw the generated hit volumes (hit_volumes.json) instead of the mesh, one colour per part
mode = argv[5] if len(argv) > 5 else "mesh"
if mode == "hits":
    import colorsys
    import json
    from mathutils import Matrix
    for o in list(bpy.context.scene.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    doc = json.load(open(os.path.join(asset, "hit_volumes.json"), encoding="utf-8"))
    parts = sorted({v["part"] for v in doc["volumes"]})
    C = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))       # OBJ (x, y, z) -> Blender (x, -z, y)
    for v in doc["volumes"]:
        if v["part"] in hide:
            continue
        rx, ry, rz = (math.radians(a) for a in v["rotation_deg"])
        M = Matrix.Translation(v["position"]) @ Matrix.Rotation(rx, 4, "X") @ Matrix.Rotation(ry, 4, "Y") @ Matrix.Rotation(rz, 4, "Z")
        if v["shape"] == "box":
            bpy.ops.mesh.primitive_cube_add(size=2)
            S = Matrix.Diagonal((*v["half_size"], 1))
            A = Matrix.Identity(4)
        else:
            bpy.ops.mesh.primitive_cylinder_add(radius=1, depth=2, vertices=32)
            S = Matrix.Diagonal((v["radius"], v["half_height"], v["radius"], 1))
            A = Matrix.Rotation(math.radians(-90), 4, "X")                       # Blender cylinder Z -> local +Y
        o = bpy.context.object
        o.matrix_world = C @ M @ S @ A
        mat = bpy.data.materials.get(v["part"]) or bpy.data.materials.new(v["part"])
        h = parts.index(v["part"]) / max(1, len(parts))
        mat.diffuse_color = (*colorsys.hsv_to_rgb(h, 0.55, 0.85), 1)
        o.data.materials.append(mat)
for o in list(bpy.context.scene.objects):
    if o.name.split(".")[0] in hide:
        bpy.data.objects.remove(o, do_unlink=True)
objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
lo = Vector((min(min((o.matrix_world @ Vector(c))[i] for c in o.bound_box) for o in objs) for i in range(3)))
hi = Vector((max(max((o.matrix_world @ Vector(c))[i] for c in o.bound_box) for o in objs) for i in range(3)))
centre = (lo + hi) / 2
size = (hi - lo).length

# scale figure: 1.8 m, standing on the unit's lowest point, beside it
fig_mat = bpy.data.materials.new("figure")
fig_mat.diffuse_color = (0.85, 0.45, 0.15, 1)
x = hi.x + 1.2
bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=1.45, location=(x, centre.y, lo.z + 0.725))
body = bpy.context.object
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.14, location=(x, centre.y, lo.z + 1.62))
head = bpy.context.object
for o in (body, head):
    o.data.materials.append(fig_mat)

sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"
sh = sc.display.shading
sh.light = "STUDIO"
sh.color_type = "MATERIAL"
sh.show_cavity = True
sh.cavity_type = "BOTH"
sh.show_object_outline = False
sh.show_specular_highlight = True
sc.display.render_aa = "16"
sc.render.resolution_x, sc.render.resolution_y = 1600, 1100
sc.render.film_transparent = False
sc.world = bpy.data.worlds.new("w")
sh.background_type = "VIEWPORT"
sh.background_color = (0.11, 0.12, 0.13)

# the unit axis (OBJ +Z) is Blender -Y after the import; the pad end is at the most negative Y
dirs = {"pad": Vector((0.9, -1.25, 0.55)), "flange": Vector((-0.9, 1.25, 0.5)), "side": Vector((1.0, 0.0, 0.12)),
        "top": Vector((0.8, -1.0, 0.95)), "under": Vector((1.1, -0.9, -0.42))}
d = dirs.get(view, dirs["pad"]).normalized()
cam_data = bpy.data.cameras.new("cam")
cam_data.lens = 60
cam = bpy.data.objects.new("cam", cam_data)
sc.collection.objects.link(cam)
cam.location = centre + d * size * 1.75
direction = centre - cam.location
cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
sc.camera = cam
sc.render.filepath = out
bpy.ops.render.render(write_still=True)
print("rendered", out)
