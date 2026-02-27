#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "src/game/equipment/types/RadarVisualProfile.h"

struct RadarMountPoint
{
    glm::vec2 normalizedPosition;   // 0..1 внутри кабины
    glm::vec2 normalizedSize;       // 0..1

    double maxSupportedMountSize;

    std::vector<game::RadarVisualProfile> allowedProfiles;
};