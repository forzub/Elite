#pragma once

// A Blender glTF as a mesh editor source (docs/BLENDER_ASSEMBLY.md, MESH_EDITOR_SPEC 16.2). The scene is turned into
// the editor's working form once per change of the .gltf / .bin:
//   <work>/gltf/LOD<N>/<part>.obj  - every mesh node in model space (glTF Y up, metres), materials by name; a node
//                                    "<part>__lod<N>" goes to LOD<N>; nodes under a library link are not exported
//   <work>/gltf/scene.json         - what the assembly document is seeded with: nodes (parts, links with elite_part),
//                                    hit volumes, zones, mounts, seams, buses (elite_feeds), kinds, materials, roles
// Only .gltf with external / embedded buffers (not .glb) for now.
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace elite::mesh_editor
{
struct ModelCard;

class GltfSource
{
public:
    // the .gltf of a model folder, or empty
    static std::filesystem::path find(const std::filesystem::path& folder);
    // converts when stale; returns the folder with LOD<N>/ (empty path + error on failure)
    static std::filesystem::path convert(const ModelCard& card, const std::filesystem::path& workDir, std::string& error);
    static nlohmann::json scene(const std::filesystem::path& workDir);
};
} // namespace elite::mesh_editor
