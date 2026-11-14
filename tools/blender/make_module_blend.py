# The module's own .blend (docs/MESH_EDITOR_SPEC.md 17), written by the mesh editor after it built the shell:
#   blender -b --factory-startup -P make_module_blend.py -- <jobs.json>
# jobs.json: {"jobs": [{id, ref, title, source, blend, lods: [{name, files: [<link>.obj]}], links: [{id, parts, parent,
# joint}], materials: [library entries], hit_volumes: [...], mounts: [...]}]}
#
# One file per module, <output>/<category>/<id>/<id>.blend:
#   collection "<id>"        - the asset (Asset Browser; elite_part = "<category>/<id>"): the LOD0 shell, one object
#                              per link "<id>.<link>", and the empty "<id>.mount" at the origin = the mounting point;
#   collection "<id>.LOD<N>" - the other LOD shells (excluded from the view layer, not part of the asset);
#   collection "<id>.hits"   - hit volumes as CUBE empties (elite_hit = element, elite_shape box / cylinder), hidden.
# Materials are the global library's (assets/materials/material_library.json) as Principled BSDF, elite_material = id.
# The OBJ frame (+Y up, metres) becomes Blender's +Z up: (x, y, z) -> (x, -z, y), as Blender's own OBJ import does.
# Point Blender's asset library at <output> (Preferences -> File Paths -> Asset Libraries) to see every module.
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector


def conv(v):
    return (v[0], -v[2], v[1])


def read_obj(path):
    vs, ns, faces, fn, fm, names = [], [], [], [], [], []
    cur = 0
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("v "):
                x, y, z = map(float, line.split()[1:4])
                vs.append(conv((x, y, z)))
            elif line.startswith("vn "):
                x, y, z = map(float, line.split()[1:4])
                ns.append(conv((x, y, z)))
            elif line.startswith("usemtl "):
                name = line.split(None, 1)[1].strip()
                if name not in names:
                    names.append(name)
                cur = names.index(name)
            elif line.startswith("f "):
                idx, nidx = [], []
                for w in line.split()[1:]:
                    a = w.split("/")
                    idx.append(int(a[0]) - 1)
                    nidx.append(int(a[2]) - 1 if len(a) > 2 and a[2] else -1)
                faces.append(idx)
                fn.append(nidx)
                fm.append(cur)
    return vs, ns, faces, fn, fm, names


def library_material(entry_by_id, mid):
    m = bpy.data.materials.get(mid)
    if m:
        return m
    e = entry_by_id.get(mid, {})
    m = bpy.data.materials.new(mid)
    if not m.node_tree:
        m.use_nodes = True
    m.use_backface_culling = False   # shells are thin sheets drawn from both sides
    m["elite_material"] = mid
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    col = list(e.get("baseColor", [0.6, 0.62, 0.65]))[:3]
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*col, 1.0)
        bsdf.inputs["Metallic"].default_value = float(e.get("metallic", 0.1))
        bsdf.inputs["Roughness"].default_value = float(e.get("roughness", 0.65))
        em = list(e.get("emissive", [0, 0, 0]))[:3]
        if any(em):
            bsdf.inputs["Emission Color"].default_value = (*em, 1.0)
            bsdf.inputs["Emission Strength"].default_value = 1.0
    m.diffuse_color = (*col, 1.0)
    m.metallic = float(e.get("metallic", 0.1))
    m.roughness = float(e.get("roughness", 0.65))
    return m


def mesh_object(name, path, entry_by_id):
    vs, ns, faces, fn, fm, names = read_obj(path)
    me = bpy.data.meshes.new(name)
    me.from_pydata(vs, [], faces)
    for mid in names:
        me.materials.append(library_material(entry_by_id, mid))
    me.polygons.foreach_set("material_index", fm)
    me.update()
    if ns:
        me.shade_smooth()
        loops = []
        for f in fn:
            loops += [ns[i] if i >= 0 else (0.0, 0.0, 1.0) for i in f]
        me.normals_split_custom_set(loops)
    return bpy.data.objects.new(name, me)


def hit_matrix(h):
    """OBJ frame: T * Rx * Ry * Rz * S (cylinder: local Z of the empty = the volume's +Y axis); then +Y up -> +Z up"""
    p = h.get("position", [0, 0, 0])
    r = [math.radians(a) for a in h.get("rotation_deg", [0, 0, 0])]
    m = Matrix.Translation(Vector(p)) @ Matrix.Rotation(r[0], 4, "X") @ Matrix.Rotation(r[1], 4, "Y") @ Matrix.Rotation(r[2], 4, "Z")
    if h.get("shape") == "cylinder":
        rad, hh = float(h.get("radius", 0.5)), float(h.get("half_height", 0.5))
        m = m @ Matrix.Rotation(math.radians(-90), 4, "X") @ Matrix.Diagonal((rad, rad, hh, 1))
    else:
        s = h.get("half_size", [0.5, 0.5, 0.5])
        m = m @ Matrix.Diagonal((s[0], s[1], s[2], 1))
    return Matrix.Rotation(math.radians(90), 4, "X") @ m


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
    entry_by_id = {m["id"]: m for m in job.get("materials", [])}
    asset = child_collection(scene.collection, mid)
    asset["elite_part"] = job["ref"]
    links = {l["id"]: l for l in job.get("links", [])}
    for lod in job["lods"]:
        lname = lod["name"]
        coll = asset if lname == job["lods"][0]["name"] else child_collection(scene.collection, f"{mid}.{lname}")
        objs = {}
        for path in lod["files"]:
            link = os.path.splitext(os.path.basename(path))[0]
            o = mesh_object(f"{mid}.{link}" + ("" if coll is asset else f".{lname}"), path, entry_by_id)
            o["elite_link"] = link
            o["elite_lod"] = lname
            if links.get(link, {}).get("joint"):
                o["elite_joint"] = json.dumps(links[link]["joint"], ensure_ascii=False)
            coll.objects.link(o)
            objs[link] = o
        for link, o in objs.items():   # rig: the link hierarchy (pivots come with rig.json poses later)
            parent = links.get(link, {}).get("parent")
            if parent and parent in objs:
                o.parent = objs[parent]
        if coll is not asset:
            bpy.context.view_layer.layer_collection.children[coll.name].exclude = True
    mount = bpy.data.objects.new(f"{mid}.mount", None)
    mount.empty_display_type = "ARROWS"
    mount.empty_display_size = 0.5
    mount["elite_mount_point"] = True
    asset.objects.link(mount)
    hits = child_collection(scene.collection, f"{mid}.hits")
    for h in job.get("hit_volumes", []):
        e = bpy.data.objects.new(f"hit.{h.get('id', h.get('node', ''))}", None)
        e.empty_display_type = "CUBE"
        e.matrix_basis = hit_matrix(h)   # matrix_world is not decomposed for an object outside the evaluated scene
        e["elite_hit"] = h.get("node", h.get("part", ""))
        e["elite_shape"] = h.get("shape", "box")
        hits.objects.link(e)
    hits.hide_viewport = True
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
