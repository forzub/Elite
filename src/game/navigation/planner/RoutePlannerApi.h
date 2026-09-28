#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation::planner
{

struct RoutePlanRequest
{
    glm::dvec3 startMeters {0.0};

    // Generic terminal-approach contract. goalMeters is the route endpoint.
    // terminalOutward points from the terminal structure into approach space.
    // terminalReferenceDistanceMeters is a scale used by the current backend
    // to size the protected terminal approach.
    glm::dvec3 goalMeters {0.0};
    glm::dvec3 terminalOutward {0.0, 0.0, 1.0};
    double terminalReferenceDistanceMeters = 300.0;

    double agentRadiusMeters = 0.0;
    double maxSpeedMps = 100.0;
    double acceleratingMps2 = 5.0;
    double brakingMps2 = 5.0;
    double lateralMps2 = 3.0;
    double maxAngularVelocityRadPerSecond = 0.0;
    double maxAngularAccelerationRadPerSecond2 = 0.0;

    bool roundTurns = true;

    bool hasInitialForward = false;
    glm::dvec3 initialForward {0.0, 0.0, -1.0};
    double initialForwardLeadMeters = 0.0;

    double gateSpacingMeters = 500.0;
    double terminalGateSpacingMeters = 250.0;
    double terminalDenseDistanceMeters = 2000.0;
    double terminalApproachLengthMeters = 0.0;
    double terminalTurnSegmentFraction = 0.40;
    bool deriveTerminalTurnRadiusFromVehicle = false;
    double preferredTerminalTurnRadiusMeters = 0.0;

    std::vector<world::navigation::NavigationObstacle> obstacles;
};

struct RouteGate
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};
    double speedMps = 0.0;
};

struct RoutePlan
{
    std::string failure;
    std::string diagnosticSummary;

    // Presentation and execution are two samplings of the same route geometry.
    std::vector<RouteGate> gates;
    std::vector<RouteGate> executionGates;

    bool terminalDetourUsed = false;

    // Planner diagnostics are carried through the public value API so
    // orchestration can explain a decision without depending on the private
    // docking-route backend type.
    bool terminalTurnRadiusRelaxed = false;
    double terminalTurnRequestedRadiusMeters = 0.0;
    double terminalTurnSpeedMps = 0.0;
    double terminalTurnLateralRadiusMeters = 0.0;
    double terminalTurnAngularRadiusMeters = 0.0;
    double terminalTurnAngularRampMeters = 0.0;

    int terminalArcCandidatesTested = 0;
    int terminalArcRouteable = 0;
    int terminalArcRouteRejected = 0;
    int terminalArcTransitReady = 0;
    int terminalArcTransitRejected = 0;
    int terminalArcCollisionRejected = 0;
    int terminalArcAcceptedCandidates = 0;
    int terminalArcAxisPassesTested = 0;
    std::string terminalArcLastRejection;

    std::string terminalArcDominantBlockerId;
    int terminalArcDominantBlockerHits = 0;
    glm::dvec3 terminalArcDominantBlockerCenterMeters {0.0};
    glm::dvec3 terminalArcDominantBlockerHalfExtentsMeters {0.0};
    double terminalArcDominantBlockerRadiusMeters = 0.0;

    double terminalArcRotationDegrees = 0.0;

    bool initialTurnPresent = false;
    double initialTurnRadiusMeters = 0.0;

    bool terminalApproachShortened = false;
    bool terminalApproachExtended = false;
    double terminalApproachLengthMeters = 0.0;
    double terminalTurnRadiusMeters = 0.0;

    [[nodiscard]] bool valid() const noexcept
    {
        return failure.empty() &&
            gates.size() >= 2 &&
            executionGates.size() >= 2;
    }
};

class RoutePlanner final
{
public:
    [[nodiscard]] static RoutePlan plan(const RoutePlanRequest& request);
};

} // namespace game::navigation::planner
