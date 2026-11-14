# Elite tools for Blender (docs/BLENDER_ASSEMBLY.md, MESH_EDITOR_SPEC 10 / 16).
#
# Install: Edit > Preferences > Add-ons > Install from Disk... > this file. The panel: 3D view, N sidebar, tab "Elite".
# Headless (the mesh editor and checks use these):
#   blender -b -P elite_tools.py -- build-library <equipment root> <parts_library.blend> [<id substring>...]
#   blender -b <scene.blend> -P elite_tools.py -- export <out.gltf>
#
# The scheme (everything the mesh editor needs travels as glTF node extras = Blender custom properties):
#   hull / frame / plates  - ordinary meshes; the object name is the part key; optional elite_role, elite_kind
#                            (volume | sheet), elite_material (a global library id)
#   a module               - an Empty instancing a library collection (linked from parts_library.blend) with
#                            elite_part = "equipment_ship/<id>"; its meshes are NOT exported (the editor takes the
#                            module's own checked mesh from its card)
#   hit volume             - Empty CUBE, elite_hit = "<node>" (the part / module it belongs to), elite_shape = box |
#                            cylinder; box half size = the empty's scale; cylinder: radius = scale X, half height =
#                            scale Z (Blender Z = the cylinder axis)
#   zone                   - Empty CUBE, elite_zone = keep_out (red) | keep_in (blue) | prefer (green)
#   mount point            - Empty CUBE, elite_mount = "<structure>:<module>"
#   seam ("колбаска")      - Empty CUBE stretched along the joint (scale X = half length), elite_seam = "<a>:<b>"
#   buses                  - on the feeding object: elite_feeds = "service_power:ship_radar ship_laser_L; data:ship_radar"
#   LODs                   - "<part>__lod1", "<part>__lod2" (MESH_EDITOR_SPEC 9); damage states "<part>__damaged"
bl_info = {
    "name": "Elite tools",
    "author": "Elite",
    "version": (1, 0, 0),
    "blender": (4, 2, 0),
    "location": "View3D > Sidebar > Elite",
    "description": "Modules as library links, hit volumes, zones, mounts, seams, buses; glTF export for the Elite mesh editor",
    "category": "Import-Export",
}

import json
import os
import sys

import bpy
import mathutils

ZONE_COLOURS = {"keep_out": (0.9, 0.15, 0.12, 0.35), "keep_in": (0.15, 0.35, 0.95, 0.35), "prefer": (0.2, 0.8, 0.3, 0.35)}
BUSES = ["propulsion_power", "service_power", "coolant", "heat_rejection", "data", "fuel", "jump_charge"]


def _collection(name):
    c = bpy.data.collections.get(name)
    if not c:
        c = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(c)
    return c


def _empty(name, coll, display="CUBE", size=1.0):
    e = bpy.data.objects.new(name, None)
    e.empty_display_type = display
    e.empty_display_size = size
    coll.objects.link(e)
    return e


def _selection_box(objs):
    lo = mathutils.Vector((1e30, 1e30, 1e30))
    hi = mathutils.Vector((-1e30, -1e30, -1e30))
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ mathutils.Vector(c)
            lo = mathutils.Vector(map(min, lo, w))
            hi = mathutils.Vector(map(max, hi, w))
    return lo, hi


def add_hit(target, shape="box"):
    """a hit volume around an object (its world bounding box)"""
    lo, hi = _selection_box([target])
    e = _empty(f"hit_{target.name}", _collection("Хитвалумы"))
    e.location = (lo + hi) / 2
    e.scale = (hi - lo) / 2
    e["elite_hit"] = target.name
    e["elite_shape"] = shape
    return e


def add_zone(kind, centre, half):
    e = _empty(f"zone_{kind}", _collection("Зоны"))
    e.location = centre
    e.scale = half
    e["elite_zone"] = kind
    e.color = ZONE_COLOURS[kind]
    return e


def add_mount(structure, module, at):
    e = _empty(f"mount_{module}", _collection("Крепления"), size=1.0)
    e.location = at
    e.scale = (0.08, 0.08, 0.08)
    e["elite_mount"] = f"{structure}:{module}"
    return e


def add_seam(a, b, centre, half_length, rotation=(0, 0, 0)):
    e = _empty(f"seam_{a}_{b}", _collection("Швы"))
    e.location = centre
    e.rotation_euler = rotation
    e.scale = (half_length, 0.04, 0.04)
    e["elite_seam"] = f"{a}:{b}"
    return e


def place_part(library, ref, location=(0, 0, 0)):
    """link the module collection <id> from the library .blend and instance it; elite_part = ref"""
    cid = ref.split("/", 1)[1]
    coll = bpy.data.collections.get(cid)
    if not coll or not coll.library:
        with bpy.data.libraries.load(library, link=True) as (src, dst):
            dst.collections = [c for c in src.collections if c == cid]
        coll = bpy.data.collections.get(cid)
    if not coll:
        raise RuntimeError(f"no collection {cid} in {library}")
    e = bpy.data.objects.new(f"eq_{cid}", None)
    e.instance_type = "COLLECTION"
    e.instance_collection = coll
    e.location = location
    e["elite_part"] = ref
    _collection("Модули").objects.link(e)
    return e


def export_gltf(path):
    """glTF (separate .bin) with custom properties as extras; library instances become nodes with elite_part"""
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    # the exporter writes an instanced collection's meshes as children of the instance node; the editor skips them
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLTF_SEPARATE", export_extras=True, export_apply=False,
                              use_visible=False, export_yup=True)
    return path


def build_library(equipment_root, out_blend, only=()):
    """one collection per equipment model (its LOD1 = the 20-30 m look), marked as an asset"""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    n = 0
    for mid in sorted(os.listdir(equipment_root)):
        if only and not any(o in mid for o in only):
            continue
        lod = os.path.join(equipment_root, mid, "LOD1")
        if not os.path.isdir(lod):
            lod = os.path.join(equipment_root, mid, "LOD0")
        if not os.path.isdir(lod):
            continue
        coll = bpy.data.collections.new(mid)
        bpy.context.scene.collection.children.link(coll)
        for f in sorted(os.listdir(lod)):
            if not f.endswith(".obj"):
                continue
            before = set(bpy.data.objects)
            bpy.ops.wm.obj_import(filepath=os.path.join(lod, f))
            for o in set(bpy.data.objects) - before:
                for c in list(o.users_collection):
                    c.objects.unlink(o)
                coll.objects.link(o)
        coll.asset_mark()
        coll["elite_part"] = f"equipment_ship/{mid}"
        n += 1
    bpy.ops.wm.save_as_mainfile(filepath=out_blend)
    print(f"[elite_tools] library: {n} modules -> {out_blend}")


# ------------------------------------------------------------------ the panel
class ELITE_OT_hit(bpy.types.Operator):
    bl_idname = "elite.add_hit"
    bl_label = "Хитвалум по выделенному"
    shape: bpy.props.EnumProperty(items=[("box", "коробка", ""), ("cylinder", "цилиндр", "")])

    def execute(self, context):
        for o in context.selected_objects:
            add_hit(o, self.shape)
        return {"FINISHED"}


class ELITE_OT_zone(bpy.types.Operator):
    bl_idname = "elite.add_zone"
    bl_label = "Зона"
    kind: bpy.props.EnumProperty(items=[("keep_out", "красная: не входить", ""), ("keep_in", "синяя: коридор", ""),
                                        ("prefer", "зелёная: желательно", "")])

    def execute(self, context):
        sel = context.selected_objects
        if sel:
            lo, hi = _selection_box(sel)
            add_zone(self.kind, (lo + hi) / 2, (hi - lo) / 2 * 1.1)
        else:
            add_zone(self.kind, context.scene.cursor.location, (5, 5, 5))
        return {"FINISHED"}


class ELITE_OT_mount(bpy.types.Operator):
    bl_idname = "elite.add_mount"
    bl_label = "Точка крепления (структура, затем модуль)"

    def execute(self, context):
        sel = context.selected_objects
        if len(sel) != 2:
            self.report({"ERROR"}, "выделите конструкцию и модуль (модуль — активный)")
            return {"CANCELLED"}
        module = context.active_object
        structure = sel[0] if sel[1] == module else sel[1]
        add_mount(structure.name, module.name, context.scene.cursor.location)
        return {"FINISHED"}


class ELITE_OT_seam(bpy.types.Operator):
    bl_idname = "elite.add_seam"
    bl_label = "Шов между двумя выделенными"

    def execute(self, context):
        sel = context.selected_objects
        if len(sel) != 2:
            self.report({"ERROR"}, "выделите две детали")
            return {"CANCELLED"}
        add_seam(sel[0].name, sel[1].name, context.scene.cursor.location, 0.5)
        return {"FINISHED"}


class ELITE_OT_part(bpy.types.Operator):
    bl_idname = "elite.place_part"
    bl_label = "Поставить модуль"
    ref: bpy.props.StringProperty(name="модуль", default="equipment_ship/")

    def execute(self, context):
        lib = context.scene.get("elite_library", "")
        if not lib:
            self.report({"ERROR"}, "укажите библиотеку: свойство сцены elite_library (путь к parts_library.blend)")
            return {"CANCELLED"}
        place_part(lib, self.ref, context.scene.cursor.location)
        return {"FINISHED"}

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)


class ELITE_OT_export(bpy.types.Operator):
    bl_idname = "elite.export"
    bl_label = "Экспорт для Elite (glTF)"

    def execute(self, context):
        blend = bpy.data.filepath
        if not blend:
            self.report({"ERROR"}, "сохраните сцену: glTF ляжет рядом, под её именем")
            return {"CANCELLED"}
        path = os.path.splitext(blend)[0] + ".gltf"
        export_gltf(path)
        self.report({"INFO"}, f"записано {path}")
        return {"FINISHED"}


class ELITE_PT_panel(bpy.types.Panel):
    bl_label = "Elite"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Elite"

    def draw(self, context):
        col = self.layout.column(align=True)
        col.operator("elite.place_part")
        col.operator("elite.add_hit").shape = "box"
        col.operator("elite.add_hit", text="Хитвалум-цилиндр").shape = "cylinder"
        for k, t in (("keep_out", "Красная зона"), ("keep_in", "Синяя зона"), ("prefer", "Зелёная зона")):
            col.operator("elite.add_zone", text=t).kind = k
        col.operator("elite.add_mount")
        col.operator("elite.add_seam")
        col.separator()
        col.operator("elite.export")


CLASSES = (ELITE_OT_hit, ELITE_OT_zone, ELITE_OT_mount, ELITE_OT_seam, ELITE_OT_part, ELITE_OT_export, ELITE_PT_panel)


def register():
    for c in CLASSES:
        bpy.utils.register_class(c)


def unregister():
    for c in reversed(CLASSES):
        bpy.utils.unregister_class(c)


def _cli():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if not args:
        return False
    if args[0] == "build-library":
        build_library(args[1], args[2], tuple(args[3:]))
    elif args[0] == "export":
        print(f"[elite_tools] exported {export_gltf(args[1])}")
    else:
        return False
    return True


if __name__ == "__main__":
    if not _cli():
        register()
