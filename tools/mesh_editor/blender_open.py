# The mesh editor's "Открыть в Blender" (docs/MESH_EDITOR_SPEC.md 6.1): an interactive Blender with one model's meshes.
#   blender -P blender_open.py -- <list.json>
# list.json: {"files": [obj or one gltf/glb], "select": "<part key>", "title": "..."}. Each OBJ becomes one object named
# after its file (the part key), so edits are saved back per part with File > Export > Wavefront (selection only).
import json
import os
import sys

import bpy


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if not args:
        return
    with open(args[0], encoding="utf-8") as f:
        job = json.load(f)
    # an empty scene
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    files = job.get("files", [])
    for path in files:
        ext = os.path.splitext(path)[1].lower()
        if ext in (".gltf", ".glb"):
            bpy.ops.import_scene.gltf(filepath=path)
            continue
        before = set(bpy.data.objects)
        bpy.ops.wm.obj_import(filepath=path)
        stem = os.path.splitext(os.path.basename(path))[0]
        if stem.endswith((".lod0", ".lod1", ".lod2", ".lod3")):
            stem = stem.rsplit(".", 1)[0]
        new = [o for o in bpy.data.objects if o not in before]
        for i, o in enumerate(new):
            o.name = stem if i == 0 else f"{stem}.{i}"
            o["elite_source"] = path   # where to export it back
    want = job.get("select") or ""
    bpy.ops.object.select_all(action="DESELECT")
    target = bpy.data.objects.get(want)
    if target:
        target.select_set(True)
        bpy.context.view_layer.objects.active = target
    # frame the selection (or everything) in every 3D view once the window is up
    def frame():
        for win in bpy.context.window_manager.windows:
            for area in win.screen.areas:
                if area.type == "VIEW_3D":
                    region = next((r for r in area.regions if r.type == "WINDOW"), None)
                    with bpy.context.temp_override(window=win, area=area, region=region):
                        if target:
                            bpy.ops.view3d.view_selected()
                        else:
                            bpy.ops.view3d.view_all()
        return None
    bpy.app.timers.register(frame, first_interval=0.5)
    print(f"[blender_open] {job.get('title', '')}: {len(files)} files, selected {want or '-'}")


main()
