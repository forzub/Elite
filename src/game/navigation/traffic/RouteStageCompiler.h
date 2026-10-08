#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/HubSemanticAnchorCatalog.h"
#include "src/game/navigation/NavigationVolumeCatalog.h"
#include "src/game/navigation/traffic/TrafficRouteGraph.h"

namespace game::navigation::traffic
{

struct NavigationReferenceFramePose
{
    glm::dvec3 originWorldMeters {0.0};
    glm::dmat3 localToWorldBasis {1.0};

    [[nodiscard]] glm::dvec3 pointToWorld(
        const glm::dvec3& localPoint
    ) const noexcept
    {
        return originWorldMeters + localToWorldBasis * localPoint;
    }

    [[nodiscard]] glm::dvec3 directionToWorld(
        const glm::dvec3& localDirection
    ) const noexcept
    {
        return localToWorldBasis * localDirection;
    }
};

struct CompiledTrafficStage
{
    TrafficRouteStageKind kind = TrafficRouteStageKind::FreeSpace;
    std::string fromPortalId;
    std::string toPortalId;
    std::string laneId;

    glm::dvec3 fromWorldMeters {0.0};
    glm::dvec3 toWorldMeters {0.0};

    world::navigation::NavigationVolumeConstraint volumeConstraint {};

    // Ordered authored centerline stations that geometric planning must visit
    // for a VolumeTransit stage. For a straight cylinder this is entry->exit;
    // for a bent tunnel/canyon it includes every swept-corridor section.
    std::vector<glm::dvec3> requiredCenterlineMeters;
};

struct CompiledTrafficRoute
{
    bool valid = false;
    std::string failure;

    std::vector<CompiledTrafficStage> stages;

    // Direct adapter for RoutePlanRequest::requiredViaPointsMeters.
    // It contains traffic portals and authored volume centerline stations in
    // semantic order, with adjacent duplicates removed.
    std::vector<glm::dvec3> requiredViaPointsMeters;
};

class RouteStageCompiler final
{
public:
    using ReferenceFrameMap =
        std::unordered_map<std::string, NavigationReferenceFramePose>;

    static CompiledTrafficRoute compile(
        const ResolvedTrafficRoute& route,
        const TrafficRouteGraph& graph,
        const HubSemanticAnchorCatalog& anchors,
        const NavigationVolumeCatalog& volumes,
        const ReferenceFrameMap& referenceFrames
    );
};

} // namespace game::navigation::traffic
