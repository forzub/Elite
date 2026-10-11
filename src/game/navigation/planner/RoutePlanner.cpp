#include "src/game/navigation/planner/RoutePlannerApi.h"

#include <algorithm>
#include <cmath>

#include "src/game/navigation/planner/assisted/AssistedRoutePlannerApi.h"

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
            return "No collision-free route satisfies the hard Assisted geometry constraints.";
        case RoutePlanFailureCode::DynamicWindowUnavailable:
            return "No safe dynamic passage is available now; wait for a valid movement window and replan.";
        case RoutePlanFailureCode::BackendFailure:
        default:
            return "The route planner backend could not produce an accepted route.";
    }
}

RoutePlan RoutePlanner::plan(
    const RoutePlanRequest& input
)
{
    // PRODUCTION BACKEND:
    // The historical DockingAdvisoryPlanner remains in the repository as a
    // regression oracle / recovery implementation, but production RoutePlanner
    // no longer delegates to it.
    //
    // Traffic topology must already have been segmented outside this module.
    // A free-space planner does not accept semantic via-points or mandatory
    // traffic straights.
    if (!input.requiredViaPointsMeters.empty() ||
        !input.mandatoryTangentStraights.empty())
    {
        RoutePlan out;
        out.disposition =
            RoutePlanDisposition::InvalidWorldData;
        out.failureCode =
            RoutePlanFailureCode::InvalidRequest;
        out.failure =
            "Assisted RoutePlanner received semantic traffic constraints; "
            "segment the route before free-space planning";
        out.userMessage = out.failure;
        return out;
    }

    assisted::Request request;

    request.start.positionMeters =
        input.startMeters;
    request.start.forward =
        input.hasInitialForward
            ? input.initialForward
            : (
                glm::length(
                    input.goalMeters -
                    input.startMeters
                ) > 1.0e-9
                    ? glm::normalize(
                          input.goalMeters -
                          input.startMeters
                      )
                    : glm::dvec3(0.0, 0.0, -1.0)
              );
    request.start.speedMps =
        std::max(0.0, input.initialSpeedMps);

    const double outwardLength =
        glm::length(input.terminalOutward);
    const glm::dvec3 outward =
        std::isfinite(outwardLength) &&
        outwardLength > 1.0e-9
            ? input.terminalOutward /
                outwardLength
            : glm::dvec3(0.0, 0.0, 1.0);

    request.goal.positionMeters =
        input.goalMeters;

    // terminalOutward points FROM the terminal into approach space. Travel
    // direction at the goal is therefore the opposite vector.
    request.goal.forward =
        -outward;
    request.goal.speedMps =
        std::max(0.0, input.terminalSpeedMps);

    request.vehicle.radiusMeters =
        input.agentRadiusMeters;
    request.vehicle.lengthMeters =
        input.vehicleLengthMeters;
    request.vehicle.maxSpeedMps =
        input.maxSpeedMps;
    request.vehicle.acceleratingMps2 =
        input.acceleratingMps2;
    request.vehicle.brakingMps2 =
        input.brakingMps2;
    request.vehicle.lateralMps2 =
        input.lateralMps2;
    request.vehicle.maxAngularVelocityRadPerSecond =
        input.maxAngularVelocityRadPerSecond;
    request.vehicle.maxAngularAccelerationRadPerSecond2 =
        input.maxAngularAccelerationRadPerSecond2;

    request.geometry.protectedDepartureStraightMeters =
        input.initialProtectedStraightMeters > 1.0e-6
            ? input.initialProtectedStraightMeters
            : 150.0;

    request.geometry.protectedArrivalStraightMeters =
        std::max(
            0.0,
            input.terminalApproachLengthMeters
        );

    // If the caller has no authored terminal straight, keep only the generic
    // terminal scale. This is still a physical boundary distance, not a
    // docking-specific magic number.
    if (request.geometry.protectedArrivalStraightMeters <= 1.0e-6)
    {
        request.geometry.protectedArrivalStraightMeters =
            std::max(
                150.0,
                input.terminalReferenceDistanceMeters
            );
    }

    request.geometry.minimumComfortRadiusMeters =
        std::max(
            150.0,
            30.0 * input.agentRadiusMeters
        );
    request.geometry.designSpeedFraction = 0.80;
    request.geometry.denseSampleSpacingMeters =
        std::min(
            10.0,
            std::max(1.0, input.gateSpacingMeters)
        );
    request.geometry.sparseGateSpacingMeters =
        std::max(
            request.geometry.denseSampleSpacingMeters,
            input.gateSpacingMeters
        );

    request.obstacles =
        input.obstacles;

    return assisted::AssistedRoutePlanner::plan(
        request
    );
}

} // namespace game::navigation::planner
