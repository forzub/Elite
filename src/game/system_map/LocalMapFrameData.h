#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::system_map
{

struct DetailHubScreenPoint
{
    std::string hubId;
    std::string parentBodyId;
    std::string name;
    glm::vec2 screen {0.0f};
    float depth = 0.0f;
    bool visible = false;
    float screenRadiusPx = 12.0f;
};

struct DetailMapFrameData
{
    std::vector<DetailHubScreenPoint> hubScreenPoints;
};

struct HubMapPickable
{
    glm::dvec3 localCenterMeters {0.0};
    glm::dvec2 screenCenterPx {0.0};
    double screenRadiusPx = 16.0;
    int priority = 0;
    std::string label;
};

struct HubMapFrameData
{
    double scale = 1.0;
    glm::dvec2 centerPx {0.0};
    std::vector<HubMapPickable> pickables;
};

} // namespace game::system_map
