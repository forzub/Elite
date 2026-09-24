#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation
{
struct DockingAdvisoryRequest
{
    glm::dvec3 startMeters {0.0};
    glm::dvec3 entranceMeters {0.0};
    glm::dvec3 outward {0.0, 0.0, 1.0};
    double standoffMeters = 300.0;
    double hullRadiusMeters = 0.0;
    double maxSpeedMps = 100.0;
    double brakingMps2 = 5.0;
    double lateralMps2 = 3.0;
    double gateSpacingMeters = 350.0;
    std::vector<world::navigation::NavigationObstacle> obstacles;
};
struct DockingAdvisoryGate
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};
    double speedMps = 0.0;
};
struct DockingAdvisoryPlan
{
    std::string failure;
    std::vector<DockingAdvisoryGate> gates;
    bool valid() const noexcept { return failure.empty() && gates.size() >= 2; }
};
class DockingAdvisoryPlanner
{
public:
    static DockingAdvisoryPlan plan(const DockingAdvisoryRequest& input);
};
} // namespace game::navigation
