# A worked example of the Blender scheme (docs/BLENDER_ASSEMBLY.md) and its check, headless:
#   blender -b --factory-startup -P tools/blender/make_demo_assembly.py -- <assets_source/models> <out folder>
# Builds parts_library.blend from two equipment models, a scene with the game Cobra's frames and plates as the hull,
# the two modules as library links, hit volumes, three zones, a mount point, a seam and buses, saves
# <out>/blender_demo.blend and exports <out>/blender_demo.gltf for the mesh editor.
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import elite_tools as et  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
models, out = args[0], args[1]
os.makedirs(out, exist_ok=True)
lib = os.path.join(out, "parts_library.blend")
et.build_library(os.path.join(models, "equipment"), lib, ("cobretty_fl2_laser", "xinzhi_n2_radar"))

bpy.ops.wm.read_factory_settings(use_empty=True)
hull = et._collection("Корпус")
cobra = os.path.join(models, "ships", "cobrettymk1", "intact", "assembly", "LOD0")   # MESH_EDITOR_SPEC 18
keep = [f for f in sorted(os.listdir(cobra)) if f.endswith(".obj") and ("frame" in f or "panel" in f or "cockpit" in f)]
parts = {}
for f in keep:
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=os.path.join(cobra, f))
    for o in set(bpy.data.objects) - before:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        hull.objects.link(o)
        o.name = os.path.splitext(f)[0]
        parts[o.name] = o
root = parts["ship_frame_CF"]
for name, o in parts.items():
    if o is not root:
        o.parent = root
        o.matrix_parent_inverse = root.matrix_world.inverted()
    et.add_hit(o)
parts["ship_cockpit"]["elite_kind"] = "volume"
parts["ship_panel_top_CF"]["elite_material"] = "eq_metal_panel"

# two modules from the library, placed where the game Cobra has its laser and radar (Blender Z up)
laser = et.place_part(lib, "equipment_ship/cobretty_fl2_laser", (3.2, -4.0, 0.0))
radar = et.place_part(lib, "equipment_ship/xinzhi_n2_radar", (0.0, 1.0, 1.6))
for m in (laser, radar):
    m.parent = root
    m.matrix_parent_inverse = root.matrix_world.inverted()
et.add_mount("ship_frame_CF", laser.name, (3.2, -4.0, 0.3))
et.add_seam("ship_panel_top_CF", "ship_panel_top_CB", (0.0, 0.0, 1.2), 1.4)
et.add_zone("keep_out", (0, 0, 0), (14, 14, 5))
et.add_zone("keep_in", (0, -40, 0), (3, 25, 3))
et.add_zone("prefer", (0, -90, 0), (20, 20, 10))
root["elite_feeds"] = f"service_power:{laser.name} {radar.name}; data:{radar.name}"
bpy.context.scene["elite_library"] = lib
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "blender_demo.blend"))
et.export_gltf(os.path.join(out, "blender_demo.gltf"))
print(f"[demo] {len(parts)} hull parts, 2 modules, {len(bpy.data.objects)} objects -> {out}")
