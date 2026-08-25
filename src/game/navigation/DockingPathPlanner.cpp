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
    const double approachStandoff = std::max(10.0, request.approachStandoffMeters);
    const double alignmentStandoff = std::max(
        approachStandoff + 10.0,
        request.alignmentStandoffMeters
    );
    out.alignmentPointMeters = request.dockCenterMeters + outward *
        alignmentStandoff;
    out.approachPointMeters = request.dockCenterMeters + outward *
        approachStandoff;
    out.terminalPointMeters = request.dockCenterMeters - outward *
        std::max(0.0, request.terminalDepthMeters);

    world::navigation::GeometricPathRequest geometric;
    geometric.startMeters = out.startPointMeters;
    geometric.goalMeters = out.alignmentPointMeters;
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

    // From alignment onward the centreline is locked to the docking axis.
    // The outer alignment->approach leg must remain collision-free against the
    // target too; only the authored approach->terminal ingress may enter it.
    if (!world::navigation::segmentClearOfNavigationObstacles(
            out.alignmentPointMeters,
            out.approachPointMeters,
            request.obstacles,
            geometric.params.agentRadiusMeters,
            geometric.params.additionalClearanceMeters))
    {
        out.message = "docking alignment leg is blocked";
        return out;
    }
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
        glm::length(out.pointsMeters.back() - out.alignmentPointMeters) > 1.0e-7)
    {
        out.pointsMeters.push_back(out.alignmentPointMeters);
    }
    out.pointsMeters.push_back(out.approachPointMeters);
    out.pointsMeters.push_back(out.terminalPointMeters);

    // Compute from the path so this does not depend on how many bypass
    // nodes the generic planner inserted before the authored alignment point.
    out.alignmentSourceProgressMeters = 0.0;
    for (std::size_t i = 1; i + 2 < out.pointsMeters.size(); ++i)
    {
        out.alignmentSourceProgressMeters += glm::length(
            out.pointsMeters[i] - out.pointsMeters[i - 1]
        );
    }
    out.approachSourceProgressMeters =
        out.alignmentSourceProgressMeters + glm::length(
            out.approachPointMeters - out.alignmentPointMeters
        );
    out.terminalSourceProgressMeters =
        out.approachSourceProgressMeters + glm::length(
            out.terminalPointMeters - out.approachPointMeters
        );

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
