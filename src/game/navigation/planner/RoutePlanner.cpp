#include "src/game/navigation/planner/RoutePlannerApi.h"

#include <cmath>
#include <utility>

#include "src/game/navigation/DockingAdvisoryPlanner.h"

namespace game::navigation::planner
{
namespace
{

RouteGate adaptGate(const game::navigation::DockingAdvisoryGate& gate)
{
    RouteGate out;
    out.positionMeters = gate.positionMeters;
    out.forward = gate.forward;
    out.speedMps = gate.speedMps;
    return out;
}

} // namespace

RoutePlan RoutePlanner::plan(const RoutePlanRequest& input)
{
    game::navigation::DockingAdvisoryRequest legacy;
    legacy.startMeters = input.startMeters;
    legacy.outward = input.terminalOutward;
    legacy.standoffMeters = input.terminalReferenceDistanceMeters;

    // The current backend represents the generic endpoint as
    // entrance + normalized(outward) * standoff. Keep that implementation
    // detail private so callers only own the exact generic goal point.
    const double outwardLength = glm::length(input.terminalOutward);
    const glm::dvec3 terminalOutward =
        std::isfinite(outwardLength) && outwardLength > 1.0e-12
            ? input.terminalOutward / outwardLength
            : input.terminalOutward;
    legacy.entranceMeters =
        input.goalMeters -
        terminalOutward *
            input.terminalReferenceDistanceMeters;

    legacy.hullRadiusMeters = input.agentRadiusMeters;
    legacy.maxSpeedMps = input.maxSpeedMps;
    legacy.acceleratingMps2 = input.acceleratingMps2;
    legacy.brakingMps2 = input.brakingMps2;
    legacy.lateralMps2 = input.lateralMps2;
    legacy.maxAngularVelocityRadPerSecond =
        input.maxAngularVelocityRadPerSecond;
    legacy.maxAngularAccelerationRadPerSecond2 =
        input.maxAngularAccelerationRadPerSecond2;
    legacy.initialSpeedMps = input.initialSpeedMps;
    legacy.roundTurns = input.roundTurns;
    legacy.hasInitialForward = input.hasInitialForward;
    legacy.initialForward = input.initialForward;
    legacy.initialForwardLeadMeters = input.initialForwardLeadMeters;
    legacy.gateSpacingMeters = input.gateSpacingMeters;
    legacy.terminalGateSpacingMeters = input.terminalGateSpacingMeters;
    legacy.terminalDenseDistanceMeters = input.terminalDenseDistanceMeters;
    legacy.terminalApproachLengthMeters =
        input.terminalApproachLengthMeters;
    legacy.terminalTurnSegmentFraction =
        input.terminalTurnSegmentFraction;
    legacy.deriveTerminalTurnRadiusFromVehicle =
        input.deriveTerminalTurnRadiusFromVehicle;
    legacy.preferredTerminalTurnRadiusMeters =
        input.preferredTerminalTurnRadiusMeters;
    legacy.obstacles = input.obstacles;

    const auto planned =
        game::navigation::DockingAdvisoryPlanner::plan(legacy);

    RoutePlan out;
    out.failure = planned.failure;
    out.diagnosticSummary =
        game::navigation::dockingAdvisoryPlanDiagnosticSummary(planned);

    out.gates.reserve(planned.gates.size());
    for (const auto& gate : planned.gates)
        out.gates.push_back(adaptGate(gate));

    out.executionGates.reserve(planned.executionGates.size());
    for (const auto& gate : planned.executionGates)
        out.executionGates.push_back(adaptGate(gate));

    out.terminalDetourUsed = planned.terminalDetourUsed;
    out.terminalTurnRadiusRelaxed =
        planned.terminalTurnRadiusRelaxed;
    out.terminalTurnRequestedRadiusMeters =
        planned.terminalTurnRequestedRadiusMeters;
    out.terminalTurnSpeedMps = planned.terminalTurnSpeedMps;
    out.terminalTurnLateralRadiusMeters =
        planned.terminalTurnLateralRadiusMeters;
    out.terminalTurnAngularRadiusMeters =
        planned.terminalTurnAngularRadiusMeters;
    out.terminalTurnAngularRampMeters =
        planned.terminalTurnAngularRampMeters;
    out.terminalArcCandidatesTested =
        planned.terminalArcCandidatesTested;
    out.terminalArcRouteable = planned.terminalArcRouteable;
    out.terminalArcRouteRejected =
        planned.terminalArcRouteRejected;
    out.terminalArcTransitReady = planned.terminalArcTransitReady;
    out.terminalArcTransitRejected =
        planned.terminalArcTransitRejected;
    out.terminalArcCollisionRejected =
        planned.terminalArcCollisionRejected;
    out.terminalArcAcceptedCandidates =
        planned.terminalArcAcceptedCandidates;
    out.terminalArcAxisPassesTested =
        planned.terminalArcAxisPassesTested;
    out.terminalArcLastRejection =
        planned.terminalArcLastRejection;
    out.terminalArcDominantBlockerId =
        planned.terminalArcDominantBlockerId;
    out.terminalArcDominantBlockerHits =
        planned.terminalArcDominantBlockerHits;
    out.terminalArcDominantBlockerCenterMeters =
        planned.terminalArcDominantBlockerCenterMeters;
    out.terminalArcDominantBlockerHalfExtentsMeters =
        planned.terminalArcDominantBlockerHalfExtentsMeters;
    out.terminalArcDominantBlockerRadiusMeters =
        planned.terminalArcDominantBlockerRadiusMeters;
    out.terminalArcRotationDegrees =
        planned.terminalArcRotationDegrees;
    out.initialTurnPresent = planned.initialTurnPresent;
    out.initialTurnRadiusMeters = planned.initialTurnRadiusMeters;
    out.terminalApproachShortened =
        planned.terminalApproachShortened;
    out.terminalApproachExtended =
        planned.terminalApproachExtended;
    out.terminalApproachLengthMeters =
        planned.terminalApproachLengthMeters;
    out.terminalTurnRadiusMeters = planned.terminalTurnRadiusMeters;
    return out;
}

} // namespace game::navigation::planner
