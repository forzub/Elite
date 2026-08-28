#pragma once

#include <cstddef>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset
{

// Convert the shared-geometry v2/v3 representation into v4 independent render
// documents. Semantic Nodes remain shared gameplay structure; every render LOD
// receives its own geometry pool, hierarchy and instance bindings.
void buildIndependentRenderLodsFromLegacy(ModelAsset& asset);

std::size_t legacyRenderLodCount(const ModelAsset& asset);

} // namespace elite::model_asset
