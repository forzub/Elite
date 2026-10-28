#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"

namespace game::navigation::traffic
{

struct RouteVolumeContainmentResult
{
    bool valid = false;
    std::string failure;

    std::size_t curveIndex = 0;
    double parameter01 = 0.0;
    glm::dvec3 offendingPointMeters {0.0};
    double requiredInsetMeters = 0.0;
};

class RouteVolumeContainmentValidator final
{
public:
    // Validate complete route primitives that belong to one compiled
    // VolumeTransit stage. KeepInside is hard failure. PreferInside may use
    // the same geometry query later as a routing cost, but is not rejected
    // here.
    static RouteVolumeContainmentResult validateKeepInside(
        const CompiledTrafficStage& stage,
        const std::vector<planner::RouteCurveSegment>& curves,
        double agentRadiusMeters
    );

    // Validate only the canonical route interval between semantic volume
    // boundaries. Boundary points may lie inside a longer Line primitive.
    static RouteVolumeContainmentResult validateKeepInsideRouteInterval(
        const CompiledTrafficStage& stage,
        const std::vector<planner::RouteCurveSegment>& curves,
        const glm::dvec3& entryWorldMeters,
        const glm::dvec3& exitWorldMeters,
        double agentRadiusMeters
    );

    static bool pointInsideErodedVolume(
        const CompiledTrafficStage& stage,
        const glm::dvec3& pointWorldMeters,
        double agentRadiusMeters,
        double additionalInsetMeters = 0.0
    ) noexcept;

    // Orientation-aware variant (Step 3): explicit insets to the section's
    // right / up (aligned hull box extent + required clearance).
    static RouteVolumeContainmentResult validateKeepInsideRouteIntervalInsets(
        const CompiledTrafficStage& stage,
        const std::vector<planner::RouteCurveSegment>& curves,
        const glm::dvec3& entryWorldMeters,
        const glm::dvec3& exitWorldMeters,
        double insetRightMeters,
        double insetUpMeters
    );

    static bool pointInsideErodedVolumeInsets(
        const CompiledTrafficStage& stage,
        const glm::dvec3& pointWorldMeters,
        double insetRightMeters,
        double insetUpMeters
    ) noexcept;

    // Route progress of a point that lies on the route curves.
    static bool progressOnRoute(
        const std::vector<planner::RouteCurveSegment>& curves,
        const glm::dvec3& pointWorldMeters,
        double& progressMeters
    );
};

} // namespace game::navigation::traffic
