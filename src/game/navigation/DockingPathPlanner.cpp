#include "src/game/navigation/DockingPathPlanner.h"

#include <algorithm>
#include <cmath>

#include "src/world/navigation/GeometricPathPlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;

bool finite3(const glm::dvec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double length2 = glm::dot(value, value);
    if (!std::isfinite(length2) || length2 <= Epsilon)
        return fallback;
    return value / std::sqrt(length2);
}
}

DockingPathPlan DockingPathPlanner::plan(const DockingPathRequest& request)
{
    DockingPathPlan out;
    if (!finite3(request.startPositionMeters) ||
        !finite3(request.dockCenterMeters) ||
        !finite3(request.dockOutward))
    {
        out.message = "non-finite docking path input";
        return out;
    }

    const glm::dvec3 outward = normalizedOr(
        request.dockOutward,
        glm::dvec3(0.0, 0.0, 1.0)
    );
    const glm::dvec3 inbound = -outward;

    out.startPointMeters = request.startPositionMeters;
    out.approachPointMeters = request.dockCenterMeters + outward *
        std::max(10.0, request.approachStandoffMeters);
    out.terminalPointMeters = request.dockCenterMeters - outward *
        std::max(0.0, request.terminalDepthMeters);

    world::navigation::GeometricPathRequest geometric;
    geometric.startMeters = out.startPointMeters;
    geometric.goalMeters = out.approachPointMeters;
    geometric.obstacles = request.obstacles;
    geometric.params.agentRadiusMeters =
        std::max(0.0, request.vehicleSafetyRadiusMeters);
    geometric.params.supportMarginMeters = std::max(
        2.0,
        geometric.params.agentRadiusMeters * 0.10
    );
    geometric.params.sphereRadialSamples = 20;
    geometric.params.capsuleRadialSamples = 16;
    geometric.params.maxConsideredObstacles = 48;

    const auto transit = world::navigation::GeometricPathPlanner::plan(geometric);
    if (!transit.valid)
    {
        out.message = transit.message;
        return out;
    }

    // The target module remains solid during transit. Only the authored final
    // ingress may enter that specific obstacle; all other obstacles remain
    // collision constraints on the terminal segment.
    if (!world::navigation::segmentClearOfNavigationObstacles(
            out.approachPointMeters,
            out.terminalPointMeters,
            request.obstacles,
            geometric.params.agentRadiusMeters,
            geometric.params.additionalClearanceMeters,
            request.targetObstacleId))
    {
        out.message = "docking ingress is blocked by non-target obstacle";
        return out;
    }

    out.pointsMeters = transit.pointsMeters;
    if (out.pointsMeters.empty() ||
        glm::length(out.pointsMeters.back() - out.approachPointMeters) > 1.0e-7)
    {
        out.pointsMeters.push_back(out.approachPointMeters);
    }
    out.pointsMeters.push_back(out.terminalPointMeters);

    const glm::dvec3 finalLeg = normalizedOr(
        out.terminalPointMeters - out.approachPointMeters,
        inbound
    );
    if (glm::dot(finalLeg, inbound) < 1.0 - 1.0e-9)
    {
        out.pointsMeters.clear();
        out.message = "docking ingress is not normal to entrance plane";
        return out;
    }

    out.valid = out.pointsMeters.size() >= 3;
    out.obstacleDetourUsed = transit.obstacleDetourUsed;
    out.message = out.valid
        ? (out.obstacleDetourUsed
            ? "docking geometric path with obstacle detour"
            : "docking geometric path")
        : "docking path has too few points";
    return out;
}

} // namespace game::navigation
