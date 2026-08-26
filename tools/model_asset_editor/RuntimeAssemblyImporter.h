#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>

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
