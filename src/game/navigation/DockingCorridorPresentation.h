#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/GuidanceCorridor.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation
{

/*
    World-space presentation frames of a docking / route corridor (cockpit
    HUD, tunnel renderer). Route truth is fixed in the hub's local frame, so
    everything per gate that does not move with the hub is prepared ONCE per
    plan: local position, normalised forward, the orthonormal local gate
    orientation before the dock roll, the dock-roll weight, the cross-section
    (width / height / tolerances from the remaining distance), the
    recommended speed. A render frame then only places each gate:
    world centre = hub transform x local position, world orientation =
    hub rotation x dock roll (about the local forward, by the gate's weight)
    x local orientation. (Thousands of gates were rebuilt from scratch every
    frame for two routes before: ~7 ms per frame in the Debug build.)
*/
class DockingCorridorPresentation
{
public:
    struct RouteInput
    {
        const std::vector<planner::RouteGate>* gates = nullptr;
        const std::vector<glm::dvec3>* frameUp = nullptr;          // per gate, hub-local
        const std::vector<double>* dockPhaseWeights = nullptr;     // per gate, 0..1
        glm::dvec3 routeUpReference {0.0, 1.0, 0.0};
        double lateralToleranceMeters = 0.0;
        double verticalToleranceMeters = 0.0;
        double shipWidthMeters = 0.0;
        double shipHeightMeters = 0.0;
    };

    // Fills route.frames and route.hubLocalGatePositionsMeters for the given
    // render frame. planKey identifies the plan the input belongs to (any
    // change of the plan must change it).
    void present(
        GuidanceCorridor& route,
        const RouteInput& input,
        std::uint64_t planKey,
        const KinematicFrame& renderFrame,
        double renderTimeSeconds,
        double dockRollPhaseRad);

    void reset() { m_planKey = 0; m_gates.clear(); m_localPositions.clear(); }

private:
    struct Gate
    {
        glm::dvec3 localPosition {0.0};
        glm::dvec3 localForward {0.0, 0.0, -1.0};
        glm::dquat localOrientation {1.0, 0.0, 0.0, 0.0}; // before the dock roll
        double dockPhaseWeight = 1.0;
        double widthMeters = 0.0;
        double heightMeters = 0.0;
        double lateralToleranceMeters = 0.0;
        double verticalToleranceMeters = 0.0;
        double recommendedSpeedMps = 0.0;
        bool requiredVehiclePose = false;
    };

    void prepare(const RouteInput& input);

    std::uint64_t m_planKey = 0;
    std::vector<Gate> m_gates;
    std::vector<glm::dvec3> m_localPositions;
};

// Identity of a route's presentation input (plan serial + content
// fingerprint): changes whenever the gates, frame ups or weights change.
std::uint64_t dockingCorridorPlanKey(
    std::uint64_t serial,
    const DockingCorridorPresentation::RouteInput& input);

} // namespace game::navigation
