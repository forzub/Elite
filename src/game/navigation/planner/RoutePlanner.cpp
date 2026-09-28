#include "src/game/navigation/planner/RoutePlannerApi.h"

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
    // entrance + outward * standoff. Keep that implementation detail private
    // to this adapter so callers do not depend on docking-specific geometry.
    legacy.entranceMeters =
        input.goalMeters -
        input.terminalOutward *
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
    out.initialTurnRadiusMeters = planned.initialTurnRadiusMeters;
    out.terminalApproachLengthMeters =
        planned.terminalApproachLengthMeters;
    out.terminalTurnRadiusMeters = planned.terminalTurnRadiusMeters;
    out.terminalArcRotationDegrees =
        planned.terminalArcRotationDegrees;
    out.terminalArcRouteable = planned.terminalArcRouteable;
    out.terminalArcTransitReady = planned.terminalArcTransitReady;
    return out;
}

} // namespace game::navigation::planner
