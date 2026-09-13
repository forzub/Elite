#pragma once

#include <cstddef>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::binary
{

bool validateRenderLod(const RenderLod& lod, std::size_t semanticNodeCount, std::string* error = nullptr);
bool validateSemanticAsset(const ModelAsset& asset, std::string* error = nullptr);

} // namespace elite::model_asset::binary
