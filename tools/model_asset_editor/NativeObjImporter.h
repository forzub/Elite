#pragma once

#include <filesystem>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::editor
{

// Native editor-side OBJ importer. It deliberately does not use the runtime
// ObjLoader: authored corner normals/UV/material ids and source polygon ids must
// survive compilation into .elmodel.
bool importObjNative(
    const std::filesystem::path& path,
    ModelAsset& asset,
    MeshLod& out,
    std::string* error = nullptr
);

} // namespace elite::model_asset::editor
