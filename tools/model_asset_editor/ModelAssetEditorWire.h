#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::editor::wire
{

// Editor transport only. This is NOT the .elmodel/.elmesh v4 disk format.
// Small commands and metadata stay on the JSON control plane; bulk mesh arrays
// use this binary frame and are reassembled into the existing browser payload
// object before the old viewport/application handler sees them.
constexpr std::uint32_t WireVersion = 1u;

std::vector<std::uint8_t> encodeLodGeometryPayload(
    std::uint32_t transferId,
    std::uint32_t lodIndex,
    const RenderLod& lod,
    const std::map<std::string, MeshLod>* rawSnapshots = nullptr);

} // namespace elite::model_asset::editor::wire
