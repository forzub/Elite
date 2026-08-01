#pragma once

#include <string>

#include <glm/glm.hpp>

#include "src/game/system_map/LocalMapFrameData.h"

namespace game::system_map
{

struct DetailMapPresentation
{
    bool valid = false;
    bool sceneIsSpatialVolume = false;
    double minimumZoom = 0.15;
    double maxRadiusMeters = 1.0;
    double scale = 1.0;
    glm::dvec2 centerPx {0.0};

    std::string selectedHubId;
    std::string selectedHubParentBodyId;

    DetailMapFrameData frame;
};

struct HubMapPresentation
{
    bool valid = false;
    int systemId = -1;
    std::string hubId;
    double scale = 1.0;
    glm::dvec2 centerPx {0.0};

    HubMapFrameData frame;
};

} // namespace game::system_map
