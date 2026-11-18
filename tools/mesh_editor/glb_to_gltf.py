"""Blender: a .glb -> a .gltf with a separate .bin and textures (the mesh editor reads .gltf, MeshEditorGltf.h).
blender -b --factory-startup -P tools/mesh_editor/glb_to_gltf.py -- <in.glb> <out.gltf>
The scene is imported as is (meshes, materials, skin / rig, animations) and exported unchanged."""
import os
import sys

import bpy

src, dst = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
os.makedirs(os.path.dirname(dst), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=dst, export_format="GLTF_SEPARATE", export_texture_dir="textures",
                          export_animations=True, export_skins=True, export_apply=False)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
print(f"[glb_to_gltf] {src} -> {dst}: {len(meshes)} meshes, {sum(len(o.data.polygons) for o in meshes)} faces, "
      f"{len([o for o in bpy.data.objects if o.type == 'ARMATURE'])} armatures, {len(bpy.data.actions)} actions")
