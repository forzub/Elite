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
    double gateSpacingMeters = 500.0;

    // Manual guidance stays sparse in open transit but becomes denser on the
    // final station approach so a curved turn is presented as a usable tunnel
    // rather than a few long chords.
    double terminalGateSpacingMeters = 250.0;
    double terminalDenseDistanceMeters = 2000.0;

    // Optional manual-guidance geometry. Zero keeps the historical minimum
    // final-axis length. terminalTurnSegmentFraction controls how much of each
    // adjacent segment a circular fillet may consume.
    double terminalApproachLengthMeters = 0.0;
    double terminalTurnSegmentFraction = 0.40;

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
