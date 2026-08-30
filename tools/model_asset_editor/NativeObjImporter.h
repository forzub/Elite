#pragma once

#include <filesystem>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::editor
{

// Native editor-side RAW OBJ decoder. It deliberately does not use the runtime
// ObjLoader because corner UV/material/polygon authoring data is needed while
// decoding. The returned MeshLod is not a working editor mesh yet: every load/
// import path must immediately cross ModelAssetEditorSession's canonical SOURCE
// boundary before the mesh is exposed to UI or later wizard stages.
bool importObjNative(
    const std::filesystem::path& path,
    ModelAsset& asset,
    MeshLod& out,
    std::string* error = nullptr
);

} // namespace elite::model_asset::editor
