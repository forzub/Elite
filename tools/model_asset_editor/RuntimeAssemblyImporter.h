#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "src/model_asset/ModelAsset.h"
#include "src/world/types/ObjectType.h"

namespace elite::model_asset::editor
{

struct ImportProgress
{
    std::string stage;
    std::size_t completed = 0;
    std::size_t total = 0;
    std::filesystem::path path;
};

using ImportProgressCallback = std::function<void(const ImportProgress&)>;

struct SourceAdditionalMesh
{
    std::filesystem::path file;
    std::string runtimePath;
};

// Scans only the immediate LOD<N> directory/directories that contain the known
// source meshes. Any sibling OBJ that is not one of the registered intact source
// meshes is returned as an additional authored mesh. File/folder names carry no
// semantic identity; the editor assigns a persistent authoring id separately.
std::vector<SourceAdditionalMesh> discoverAdditionalLodMeshes(
    const std::filesystem::path& sourceRoot,
    const std::vector<std::string>& knownRuntimePaths,
    std::vector<std::string>* warnings = nullptr);

// Returns every OBJ path declared by the runtime assembly for one LOD. This is
// used as the exclusion set when scanning for extra authored meshes, so source
// files removed from the edited geometry pool after instance consolidation do
// not accidentally reappear as damage variants.
std::vector<std::string> runtimeAssemblyLodSourcePaths(
    ObjectType typeId,
    std::size_t lodIndex);

// Transitional assembly importer: hierarchy still comes from the current
// descriptor registry, but mesh data is parsed directly from source OBJ files
// by the editor-side native importer. Runtime ObjLoader/MeshData processing is
// deliberately bypassed so normals/UV/material/topology survive.
bool importRuntimeAssembly(
    const std::filesystem::path& sourceRoot,
    ObjectType typeId,
    const std::string& assetId,
    const std::string& displayName,
    ModelAsset& out,
    std::string* error = nullptr,
    std::string* warning = nullptr,
    ImportProgressCallback progress = {}
);

} // namespace elite::model_asset::editor
