#pragma once

// The assembly document of a model (docs/MESH_EDITOR_SPEC.md 16): <model folder>/assembly.json, schema
// elite.assembly.v1. It is what the steps after "Загрузка и проверка" edit and what "Сборка" turns into a package:
//   parts      - meshes (one per source file stem), with kind / material;
//   nodes      - the hierarchy: instances of parts (id, part, parent, role, position, rotation_deg) or links to library
//                models (ref "equipment_ship/<id>" - the module's own mesh, fixed once in its own card);
//   materials  - source material name -> global library id (assets/materials/material_library.json);
//   hit_volumes, zones (keep_out red / keep_in blue / prefer green), mounts, seams ("колбаски"), connections (buses),
//   damage {states, detachable}.
// Seeded when missing: from the mock-up's source/ship_asset.json, an equipment hit_volumes.json, a Blender glTF
// (extras elite_*), or automatically (hit volume per part, seams between sheets, mounts to the frame, buses by name).
// "Сборка" writes <output>/<category>/<id>/: geo/*.obj (the repaired meshes, links resolved from the library),
// ship_asset.json (the assembler's schema), <id>.elmodel + .lodN.elmesh (EliteShipAssetAssemble) and
// <id>.elassembly.json (links, connections, zones, mounts - what .elmodel v4 has no chunk for). Nothing is overwritten
// outside that folder.
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tools/mesh_editor/MeshEditorInspect.h"

namespace elite::mesh_editor
{
struct ModelCard;

class Assembly
{
public:
    // library models a part may be (category/id of a card with the part's key), or "" (asked per part key)
    using LinkResolver = std::function<std::string(const std::string& partKey)>;
    // the repaired groups of a library model ("equipment_ship/<id>")
    using LinkGeometry = std::function<std::vector<RepairedGroup>(const std::string& ref)>;

    static std::filesystem::path file(const ModelCard& card);
    // the saved document, or a seeded one ("seeded": true, not on disk until saved); parts' centre / size refreshed
    // seed: extra data for a fresh document (a Blender scene.json: nodes / volumes / zones...; "calibration" of the
    // shipped package this model replaces)
    static nlohmann::json load(const ModelCard& card, const std::vector<RepairedGroup>& groups, const LinkResolver& links,
                               const nlohmann::json& seed = nlohmann::json());
    static void save(const ModelCard& card, nlohmann::json doc);
    // the whole build; returns {ok, report, files[], output}
    static nlohmann::json build(const ModelCard& card, const nlohmann::json& doc, const std::vector<RepairedGroup>& groups,
                                const std::filesystem::path& outputRoot, const nlohmann::json& library, const LinkGeometry& linkGeometry);
    // the global material library
    static std::filesystem::path libraryFile(const std::filesystem::path& sourceTree);
    static nlohmann::json loadLibrary(const std::filesystem::path& sourceTree);
    static void saveLibrary(const std::filesystem::path& sourceTree, const nlohmann::json& lib);
    // the library id of a part's source material (the part's own choice, the document's map, the name itself, default)
    static std::string materialId(const nlohmann::json& doc, const nlohmann::json& library, const std::string& partKey,
                                  const std::string& source);
    // one OBJ of the given parts (merged; corner normals inside 35 deg; usemtl = library ids, mtl names taken as ids)
    static void writeMeshObj(const std::filesystem::path& file, const std::vector<const RepairedPart*>& parts,
                             const nlohmann::json& library);
};
} // namespace elite::mesh_editor
