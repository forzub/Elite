#pragma once

// Runtime assembly of a generated hub city (author 2026-11-18: all hubs are built «by id» - repeatable and
// recognizable). The city is its frozen layout (src/assets/data/navigation/hub_cities/<hub>.json, written by
// tools/models/hub_city_layout.py) + the shared module packages (assets/compiled/hub_modules/<folder>/<folder>.elmodel):
// the result is the same metric production ModelAsset the mesh editor's per-hub package had (nodes in the layout's
// order, one render node per placed module per LOD sharing the module's merged geometry, collision volumes
// "<node>.<folder>.<k>", a module-proxy LOD of one OBB-box mesh per node group, the "hlod" socket) - so the renderer,
// the city damage and the hit volumes work unchanged, without a 50-150 MB package per hub.

#include "src/model_asset/ModelAsset.h"

#include <string>

namespace elite::model_asset
{

// the frozen layout of hub `hubId`: assets/data/navigation/hub_cities/<hubId>.json (relative to the working dir)
std::string hubCityLayoutPath(const std::string& hubId);

// builds the city of `hubId` from its layout and the module packages under `moduleRoot` (assets/compiled/hub_modules).
// false (with *error) when the layout or a module package is missing / invalid. Module packages are cached per
// process (thread-safe); one city of ~13 000 nodes assembles in well under a second.
bool assembleHubCity(const std::string& hubId, const std::string& moduleRoot, ModelAsset& out, std::string* error = nullptr);

// the process-wide city asset of `hubId` (assembled once, shared by the server's hit volumes and the client's
// renderer); nullptr when it cannot be assembled
const ModelAsset* hubCityAsset(const std::string& hubId);

} // namespace elite::model_asset
