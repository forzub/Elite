"""Mesh editor card preview (docs/MESH_EDITOR_SPEC.md 5): a three-quarter Workbench render of a model folder.

usage: blender -b --factory-startup -P tools/mesh_editor/preview_thumbnail.py -- <model folder> <out.png>
       blender -b --factory-startup -P tools/mesh_editor/preview_thumbnail.py -- --list <file>   (lines: folder<TAB>out.png)
The folder: LOD0/*.obj (the source layout), else *.gltf / *.glb in it.
"""
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:]


def render(folder, out):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    lod0 = os.path.join(folder, "LOD0")
    if os.path.isdir(lod0):
        for root, _, files in os.walk(lod0):
            if os.path.basename(root) == "variants":
                continue
            for f in sorted(files):
                if f.endswith(".obj"):
                    bpy.ops.wm.obj_import(filepath=os.path.join(root, f), forward_axis="NEGATIVE_Z", up_axis="Y")
    else:
        for f in sorted(os.listdir(folder)):
            if f.endswith(".gltf") or f.endswith(".glb"):
                bpy.ops.import_scene.gltf(filepath=os.path.join(folder, f))
                break

    for m in bpy.data.materials:
        if m.use_nodes and m.node_tree:
            b = m.node_tree.nodes.get("Principled BSDF")
            if b:
                m.diffuse_color = tuple(b.inputs["Base Color"].default_value)

    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not meshes:
        return
    lo = Vector((1e30, 1e30, 1e30))
    hi = Vector((-1e30, -1e30, -1e30))
    for o in meshes:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
    centre = (lo + hi) / 2
    size = max((hi - lo).length, 1e-3)

    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light = "STUDIO"
    sh.color_type = "MATERIAL"
    sh.show_cavity = True
    sh.cavity_type = "BOTH"
    sc.display.render_aa = "8"
    sc.render.resolution_x, sc.render.resolution_y = 480, 340
    sh.background_type = "VIEWPORT"
    sh.background_color = (0.09, 0.10, 0.12)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.lens = 50
    cam = bpy.data.objects.new("cam", cam_data)
    sc.collection.objects.link(cam)
    d = Vector((1.0, -1.3, 0.75)).normalized()
    cam.location = centre + d * size * 1.45
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam_data.clip_end = size * 10
    sc.camera = cam
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)



if argv[0] == "--list":
    for line in open(argv[1], encoding="utf-8"):
        parts = line.rstrip("\n").split("\t")
        if len(parts) == 2:
            try:
                render(parts[0], parts[1])
            except Exception as e:  # one bad model must not stop the batch
                print("preview failed", parts[0], e)
else:
    render(argv[0], argv[1])
