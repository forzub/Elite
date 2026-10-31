#pragma once

#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset
{

// BUILD step (offline, all LOD payloads resident, metric asset): for every
// Component-role LOD above 0 and every semantic node, measure the two-sided
// distance between the node's geometry in that LOD and in LOD0 (vertex, edge
// midpoint and centroid samples to the nearest triangle, both directions) and
// store it in RenderLod::nodeGeometricErrorM. A node missing from a LOD gets
// its LOD0 diagonal (the whole part is omitted). Values never decrease along
// the LOD chain. Returns a short human-readable summary.
std::string measureNodeLodErrors(ModelAsset& asset);

// Two-sided distance between two meshes in the same frame (the measurement
// above, for one geometry pair; the LOD generator uses it per geometry).
// -1 when either mesh is empty, 0 when identical.
float meshDeviation(const MeshLod& a, const MeshLod& b);

} // namespace elite::model_asset
