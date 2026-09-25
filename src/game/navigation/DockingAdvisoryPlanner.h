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

    // Preferred human-flyable radius, not a task-failure threshold. Planner
    // must first try another geometric route that preserves it; only after the
    // reroute search is exhausted may it tighten the terminal arc as far as
    // collision-free geometry allows.
    double preferredTerminalTurnRadiusMeters = 0.0;

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

    // Diagnostics for route-selection policy. A detour means Planner changed
    // coarse geometry before conceding turn radius. relaxed means no route
    // preserving the preferred radius was found and the widest feasible local
    // terminal arc was accepted instead.
    bool terminalDetourUsed = false;
    bool terminalTurnRadiusRelaxed = false;
    double terminalTurnRadiusMeters = 0.0;

    // terminalApproachLengthMeters is the actually accepted collision-free
    // straight docking-axis lead. The request value is preferred geometry, not
    // a hard semantic requirement; only the minimum ingress immediately in
    // front of the port is mandatory.
    bool terminalApproachShortened = false;
    double terminalApproachLengthMeters = 0.0;

    bool valid() const noexcept { return failure.empty() && gates.size() >= 2; }
};
class DockingAdvisoryPlanner
{
public:
    static DockingAdvisoryPlan plan(const DockingAdvisoryRequest& input);
};
} // namespace game::navigation
