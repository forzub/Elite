#include "src/game/navigation/NominalRoutePlanner.h"

#include <cmath>

#include "src/world/navigation/GeometricPathPlanner.h"

namespace game::navigation
{
namespace
{

bool finite3(const glm::dvec3& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

void appendLeg(
    NominalRoutePlanner::Plan& out,
    const world::navigation::GeometricPathResult& leg
)
{
    if (leg.pointsMeters.empty())
        return;

    if (out.pointsMapMeters.empty())
    {
        out.pointsMapMeters = leg.pointsMeters;
    }
    else
    {
        // Leg N starts at the exact terminal point of leg N-1.
        for (std::size_t i = 1; i < leg.pointsMeters.size(); ++i)
            out.pointsMapMeters.push_back(leg.pointsMeters[i]);
    }

    out.lengthMeters += leg.lengthMeters;
    out.staticDetourUsed =
        out.staticDetourUsed || leg.obstacleDetourUsed;
}

} // namespace

NominalRoutePlanner::Plan NominalRoutePlanner::plan(
    const Request& request
)
{
    Plan out;
    out.goalRevision = request.goalRevision;
    out.staticWorldRevision = request.staticWorldRevision;

    if (!finite3(request.startMapMeters) ||
        !finite3(request.goalMapMeters))
    {
        out.message = "nominal route endpoint is non-finite";
        return out;
    }

    for (const glm::dvec3& waypoint :
         request.requiredWaypointsMapMeters)
    {
        if (!finite3(waypoint))
        {
            out.message = "nominal route waypoint is non-finite";
            return out;
        }
    }

    std::vector<glm::dvec3> terminals =
        request.requiredWaypointsMapMeters;
    terminals.push_back(request.goalMapMeters);

    glm::dvec3 legStart = request.startMapMeters;

    for (const glm::dvec3& legGoal : terminals)
    {
        world::navigation::GeometricPathRequest geometric;
        geometric.startMeters = legStart;
        geometric.goalMeters = legGoal;
        geometric.obstacles = request.staticObstacles;

        geometric.params.agentRadiusMeters =
            std::max(
                0.0,
                request.navigationEnvelopeRadiusMeters
            );
        geometric.params.additionalClearanceMeters =
            std::max(
                0.0,
                request.additionalRouteClearanceMeters
            );

        // Correctness must not depend on an arbitrary obstacle-count cap.
        // The shared/static world reduction layer is responsible for scaling.
        geometric.params.maxConsideredObstacles = 0;
        geometric.params.allowStartEscape = false;
        geometric.params.allowGoalEscape = false;
        geometric.params.simplifyLineOfSight = true;

        const auto leg =
            world::navigation::GeometricPathPlanner::plan(geometric);

        if (!leg.valid)
        {
            out.message =
                "nominal static route failed: " + leg.message;
            out.pointsMapMeters.clear();
            out.lengthMeters = 0.0;
            out.staticDetourUsed = false;
            return out;
        }

        appendLeg(out, leg);
        legStart = legGoal;
    }

    out.valid = out.pointsMapMeters.size() >= 2;
    out.message =
        out.valid
            ? (
                out.staticDetourUsed
                    ? "nominal static route with detour"
                    : "nominal static route direct"
              )
            : "nominal static route has too few points";
    return out;
}

NominalRoutePlanner::InvalidationReason
NominalRoutePlanner::invalidationReason(
    const Plan& plan,
    const ValidityQuery& current
) noexcept
{
    (void)current.dynamicWorldRevision;

    if (!plan.valid)
        return InvalidationReason::InvalidPlan;

    if (plan.goalRevision != current.goalRevision)
        return InvalidationReason::GoalChanged;

    if (plan.staticWorldRevision != current.staticWorldRevision)
        return InvalidationReason::StaticWorldChanged;

    return InvalidationReason::None;
}

} // namespace game::navigation
