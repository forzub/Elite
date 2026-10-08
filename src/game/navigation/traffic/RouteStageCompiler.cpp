#include "src/game/navigation/traffic/RouteStageCompiler.h"

#include <cmath>

namespace game::navigation::traffic
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

void appendUnique(
    std::vector<glm::dvec3>& out,
    const glm::dvec3& point
)
{
    constexpr double epsilonMeters = 1.0e-6;
    if (!out.empty() &&
        glm::length(out.back() - point) <= epsilonMeters)
    {
        return;
    }
    out.push_back(point);
}

bool resolvePortalWorld(
    const std::string& portalId,
    const TrafficRouteGraph& graph,
    const HubSemanticAnchorCatalog& anchors,
    const RouteStageCompiler::ReferenceFrameMap& frames,
    glm::dvec3& position,
    std::string& failure
)
{
    const auto* portal = graph.portal(portalId);
    if (!portal)
    {
        failure = "compiled route references unknown portal: " + portalId;
        return false;
    }

    const auto* anchor =
        anchors.find(portal->hubModuleId, portal->semanticAnchorId);
    if (!anchor || !anchor->enabled)
    {
        failure =
            "traffic portal has no enabled semantic anchor: " + portalId;
        return false;
    }

    const auto frameIt = frames.find(portal->hubModuleId);
    if (frameIt == frames.end())
    {
        failure =
            "missing runtime reference frame for portal: " + portalId;
        return false;
    }

    position =
        frameIt->second.pointToWorld(anchor->localPositionMeters);
    if (!finite3(position))
    {
        failure =
            "traffic portal resolved to non-finite world position: " +
            portalId;
        return false;
    }

    return true;
}

bool compileVolumeCenterline(
    const TrafficRouteStage& stage,
    const NavigationVolumeCatalog& volumes,
    const RouteStageCompiler::ReferenceFrameMap& frames,
    std::vector<glm::dvec3>& out,
    std::string& failure
)
{
    const auto* volume = volumes.find(stage.volumeConstraint.volumeId);
    if (!volume)
    {
        failure =
            "traffic stage references unknown navigation volume: " +
            stage.volumeConstraint.volumeId;
        return false;
    }

    const auto frameIt = frames.find(volume->referenceFrameId);
    if (frameIt == frames.end())
    {
        failure =
            "missing runtime reference frame for navigation volume: " +
            volume->id;
        return false;
    }

    if (volume->kind !=
        world::navigation::NavigationVolumeKind::SweptCorridor)
    {
        failure =
            "volume transit currently requires swept-corridor geometry: " +
            volume->id;
        return false;
    }

    if (volume->sections.size() < 2)
    {
        failure =
            "volume transit corridor has fewer than two sections: " +
            volume->id;
        return false;
    }

    out.clear();
    out.reserve(volume->sections.size());
    for (const auto& section : volume->sections)
    {
        const glm::dvec3 world =
            frameIt->second.pointToWorld(section.centerLocalMeters);
        if (!finite3(world))
        {
            failure =
                "navigation volume section resolved non-finite: " +
                volume->id;
            return false;
        }
        appendUnique(out, world);
    }

    return true;
}

} // namespace

CompiledTrafficRoute RouteStageCompiler::compile(
    const ResolvedTrafficRoute& route,
    const TrafficRouteGraph& graph,
    const HubSemanticAnchorCatalog& anchors,
    const NavigationVolumeCatalog& volumes,
    const ReferenceFrameMap& referenceFrames
)
{
    CompiledTrafficRoute out;

    if (!route.valid)
    {
        out.failure =
            route.failure.empty()
                ? "cannot compile invalid traffic route"
                : route.failure;
        return out;
    }

    if (route.stages.empty())
    {
        out.failure = "resolved traffic route contains no stages";
        return out;
    }

    out.stages.reserve(route.stages.size());

    for (const auto& stage : route.stages)
    {
        CompiledTrafficStage compiled;
        compiled.kind = stage.kind;
        compiled.fromPortalId = stage.fromPortalId;
        compiled.toPortalId = stage.toPortalId;
        compiled.laneId = stage.laneId;
        compiled.volumeConstraint = stage.volumeConstraint;

        if (!stage.fromPortalId.empty())
        {
            if (!resolvePortalWorld(
                    stage.fromPortalId,
                    graph,
                    anchors,
                    referenceFrames,
                    compiled.fromWorldMeters,
                    out.failure))
            {
                return out;
            }
        }

        if (!stage.toPortalId.empty())
        {
            if (!resolvePortalWorld(
                    stage.toPortalId,
                    graph,
                    anchors,
                    referenceFrames,
                    compiled.toWorldMeters,
                    out.failure))
            {
                return out;
            }
        }

        if (stage.kind == TrafficRouteStageKind::VolumeTransit)
        {
            if (stage.volumeConstraint.policy !=
                    world::navigation::NavigationVolumePolicy::KeepInside &&
                stage.volumeConstraint.policy !=
                    world::navigation::NavigationVolumePolicy::PreferInside)
            {
                out.failure =
                    "volume transit lost inside-volume policy";
                return out;
            }

            if (!compileVolumeCenterline(
                    stage,
                    volumes,
                    referenceFrames,
                    compiled.requiredCenterlineMeters,
                    out.failure))
            {
                return out;
            }

            // Keep topology authoritative at the boundaries. The authored
            // corridor may contain the same points as its first/last section,
            // but the semantic portals still own entry/exit ordering.
            appendUnique(
                out.requiredViaPointsMeters,
                compiled.fromWorldMeters
            );
            for (const auto& point : compiled.requiredCenterlineMeters)
                appendUnique(out.requiredViaPointsMeters, point);
            appendUnique(
                out.requiredViaPointsMeters,
                compiled.toWorldMeters
            );
        }
        else if (!stage.toPortalId.empty())
        {
            appendUnique(
                out.requiredViaPointsMeters,
                compiled.toWorldMeters
            );
        }

        out.stages.push_back(std::move(compiled));
    }

    out.valid = true;
    return out;
}

} // namespace game::navigation::traffic
