#pragma once

// The files of one module after its shell is built (docs/MESH_EDITOR_SPEC.md 17; author 2026-11-14: "держать запчасти
// отдельно … .elmodel - в этом формате мы собирались собирать все, что касается конкретного модуля, включая шарниры,
// хитвалюмы, поврежденные состояния и точки крепления. все в бинарном виде для быстрой загрузки"):
//   package <output>/<category>/<id>/<id>.elmodel + .lod<N>.elmesh - the game's / the editor's binary form:
//           one semantic node per rig link (parent, joint), the shell of every LOD (one render node per link,
//           two-sided), library materials, hit volumes (collision, on the link holding the element), the mount point
//           (socket "equipment_mount" at the origin); damaged states come with their models later;
//   glTF    <work>/<category>/<id>/blender/<id>.gltf + .bin - the exchange form Blender reads to write the module's
//           .blend (tools/blender/make_module_blend.py); the conventions of a Blender scene (BLENDER_ASSEMBLY.md):
//           mesh node "<link>__lod<N>", hit volume = node with extras elite_hit / elite_shape (scale = half size;
//           cylinder: x = radius, y = half height), "<id>.mount" with elite_mount_point; materials by library id.
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tools/mesh_editor/MeshEditorInspect.h"
#include "tools/mesh_editor/MeshEditorShell.h"

namespace elite::mesh_editor
{
struct ModuleShell
{
    std::string id, category, title;
    std::vector<RepairedGroup> lods;   // the shell per LOD (name "LOD<N>"), parts = links (key = link id, mtl = library ids)
    std::vector<ShellLink> links;
    nlohmann::json hitVolumes = nlohmann::json::array();
    nlohmann::json library;           // the global material library
};

class ModuleFiles
{
public:
    // true on success; report: the one-line summary or the error
    static bool writePackage(const ModuleShell& m, const std::filesystem::path& elmodel, std::string& report);
    static void writeGltf(const ModuleShell& m, const std::filesystem::path& gltf);
};
} // namespace elite::mesh_editor
