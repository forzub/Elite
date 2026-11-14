# The module's own .blend (docs/MESH_EDITOR_SPEC.md 17), written by the mesh editor after it built the shell:
#   blender -b --factory-startup -P make_module_blend.py -- <jobs.json>
# jobs.json: {"jobs": [{id, ref, title, source, blend, gltf}]}. The glTF (tools/mesh_editor/MeshEditorModule.h) is the
# exchange form: mesh nodes "<link>__lod<N>" (extras elite_link, elite_lod, elite_joint), hit volumes (elite_hit,
# elite_shape), "<id>.mount" (elite_mount_point), materials named by library id (extras elite_material). The game and
# the editor read the module's .elmodel; this file is for assembling ships in Blender.
#
# One file per module, <output>/<category>/<id>/<id>.blend:
#   collection "<id>"        - the asset (Asset Browser; elite_part = "<category>/<id>"): the LOD0 shell, one object
#                              per link "<id>.<link>", and the empty "<id>.mount" at the origin = the mounting point;
#   collection "<id>.LOD<N>" - the other LOD shells (excluded from the view layer, not part of the asset);
#   collection "<id>.hits"   - hit volumes as CUBE empties (elite_hit = element, elite_shape box / cylinder), hidden.
# Blender's glTF importer turns +Y up into +Z up. Point Blender's asset library at <output> (Preferences -> File
# Paths -> Asset Libraries) to see every module.
import json
import os
import sys

import bpy


def child_collection(parent, name):
    c = bpy.data.collections.new(name)
    parent.children.link(c)
    return c


def make(job):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0   # no <id>.blend1 beside the file
    scene = bpy.context.scene
    mid = job["id"]
    scene["elite_module"] = job["ref"]
    scene["elite_source"] = job.get("source", "")
    bpy.ops.import_scene.gltf(filepath=job["gltf"])
    imported = list(scene.collection.objects)
    for c in list(scene.collection.children):   # the importer may put objects in a collection of its own
        imported += list(c.all_objects)
    asset = child_collection(scene.collection, mid)
    asset["elite_part"] = job["ref"]
    hits = child_collection(scene.collection, f"{mid}.hits")
    lod_colls = {}
    first_lod = None
    for o in imported:
        lod = o.get("elite_lod")
        if o.type == "MESH" and lod:
            link = o["elite_link"]
            if first_lod is None or lod < first_lod:
                first_lod = lod
            lod_colls.setdefault(lod, []).append(o)
            o.data.name = o.name = f"{mid}.{link}.{lod}"
        elif "elite_hit" in o:
            o.empty_display_type = "CUBE"
            o.empty_display_size = 1.0
        elif o.get("elite_mount_point"):
            o.empty_display_type = "ARROWS"
            o.empty_display_size = 0.5
            o.name = f"{mid}.mount"
    for o in imported:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        if o.type == "MESH" and o.get("elite_lod"):
            lod = o["elite_lod"]
            if lod == first_lod:
                o.name = f"{mid}.{o['elite_link']}"
                asset.objects.link(o)
            else:
                name = f"{mid}.{lod}"
                coll = bpy.data.collections.get(name) or child_collection(scene.collection, name)
                coll.objects.link(o)
        elif "elite_hit" in o:
            hits.objects.link(o)
        else:
            asset.objects.link(o)
    for c in list(scene.collection.children):   # an empty importer collection
        if c not in (asset, hits) and not c.name.startswith(f"{mid}.LOD") and not c.all_objects:
            bpy.data.collections.remove(c)
    for c in scene.collection.children:
        if c.name.startswith(f"{mid}.LOD"):
            bpy.context.view_layer.layer_collection.children[c.name].exclude = True
    hits.hide_viewport = True
    for m in bpy.data.materials:   # shells are thin sheets drawn from both sides
        m.use_backface_culling = False
    asset.asset_mark()
    asset.asset_data.description = job.get("title", mid)
    for tag in job["ref"].split("/"):
        asset.asset_data.tags.new(tag, skip_if_exists=True)
    try:
        asset.asset_generate_preview()
    except Exception as ex:   # no preview without a GPU context: the browser shows the name
        print(f"[make_module_blend] {mid}: no preview ({ex})")
    bpy.context.view_layer.update()   # world matrices stored in the file
    os.makedirs(os.path.dirname(job["blend"]), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=job["blend"], compress=True)
    print(f"[make_module_blend] {mid}: {job['blend']}")


def main():
    jobs = json.load(open(sys.argv[sys.argv.index("--") + 1], encoding="utf-8"))["jobs"]
    bad = 0
    for job in jobs:
        try:
            make(job)
        except Exception as ex:
            bad += 1
            print(f"[make_module_blend] {job.get('id')}: FAILED {ex}")
    print(f"[make_module_blend] {len(jobs) - bad} of {len(jobs)} written")
    sys.exit(1 if bad else 0)


main()
