#pragma once

#include <filesystem>
#include <string>

#include "src/model_asset/ModelAsset.h"
#include "src/world/types/ObjectType.h"

namespace elite::model_asset::editor
{

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
    std::string* error = nullptr
);

} // namespace elite::model_asset::editor
