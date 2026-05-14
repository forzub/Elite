#pragma once

#include <string>
#include <nlohmann/json.hpp>

class SpaceState;

nlohmann::json buildVolumeViewerPreviewForType(
    const SpaceState& state,
    const std::string& shipTypeId
);