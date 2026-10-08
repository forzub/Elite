#include "src/game/navigation/planner/RoutePlannerApi.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

#include "src/game/navigation/DockingAdvisoryPlanner.h"

namespace game::navigation::planner
{

const char* routePlanFailureMessage(
    RoutePlanFailureCode code
) noexcept
{
    switch (code)
    {
        case RoutePlanFailureCode::None:
            return "Route is ready.";
        case RoutePlanFailureCode::InvalidRequest:
            return "Navigation request is invalid.";
        case RoutePlanFailureCode::InvalidWorldGeometry:
            return "Navigation world geometry is invalid or incomplete.";
        case RoutePlanFailureCode::HullDoesNotFit:
            return "The ship and required clearance do not fit through any available passage.";
        case RoutePlanFailureCode::GoalGeometricallyIsolated:
            return "The destination is geometrically isolated from the ship by impassable obstacles.";
        case RoutePlanFailureCode::UnavoidableCollision:
            return "Given the current state and hard vehicle limits, every reachable trajectory collides.";
        case RoutePlanFailureCode::PropulsionInsufficient:
            return "Available propulsion or control authority is insufficient to reach a safe route.";
        case RoutePlanFailureCode::NoCollisionFreeCandidate:
            return "The current route candidate is blocked; search another sector, radius, lead length or approach geometry.";
        case RoutePlanFailureCode::DynamicWindowUnavailable:
            return "No safe dynamic passage is available now; wait for a valid movement window and replan.";
        case RoutePlanFailureCode::BackendFailure:
        default:
            return "The current route candidate is not acceptable; refine route geometry or speed limits and try again.";
    }
}

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
    legacy.requiredViaPointsMeters =
        input.requiredViaPointsMeters;
    legacy.mandatoryTangentStraights =
        input.mandatoryTangentStraights;
    legacy.obstacles = input.obstacles;

    const auto planned =
        game::navigation::DockingAdvisoryPlanner::plan(legacy);

    RoutePlan out;
    out.failure = planned.failure;
    out.diagnosticSummary =
        game::navigation::dockingAdvisoryPlanDiagnosticSummary(planned);

    if (planned.valid())
    {
        out.disposition = RoutePlanDisposition::Ready;
        out.failureCode = RoutePlanFailureCode::None;
        out.userMessage = routePlanFailureMessage(out.failureCode);
    }
    else
    {
        const std::string failureLower = [&]()
        {
            std::string value = planned.failure;
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                }
            );
            return value;
        }();

        if (failureLower.find("invalid") != std::string::npos ||
            failureLower.find("nan") != std::string::npos ||
            failureLower.find("non-finite") != std::string::npos)
        {
            out.disposition = RoutePlanDisposition::InvalidWorldData;
            out.failureCode = RoutePlanFailureCode::InvalidWorldGeometry;
            out.userMessage =
                routePlanFailureMessage(out.failureCode);
        }
        else if (failureLower.find("blocked") != std::string::npos ||
                 failureLower.find("obstruct") != std::string::npos ||
                 failureLower.find("collision-free") != std::string::npos ||
                 failureLower.find("route") != std::string::npos)
        {
            // One blocked candidate is not proof that the route is impossible.
            // Ask the planner/orchestrator for a different sector, radius or
            // approach geometry first.
            out.disposition = RoutePlanDisposition::NeedsRefinement;
            out.failureCode = RoutePlanFailureCode::NoCollisionFreeCandidate;
            out.userMessage =
                routePlanFailureMessage(out.failureCode);
        }
        else
        {
            out.disposition = RoutePlanDisposition::NeedsRefinement;
            out.failureCode = RoutePlanFailureCode::BackendFailure;
            out.userMessage =
                routePlanFailureMessage(out.failureCode);
        }
    }

    out.gates.reserve(planned.gates.size());
    for (const auto& gate : planned.gates)
        out.gates.push_back(adaptGate(gate));

    out.executionGates.reserve(planned.executionGates.size());
    for (const auto& gate : planned.executionGates)
        out.executionGates.push_back(adaptGate(gate));

    out.routeCurves = planned.routeCurves;

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
