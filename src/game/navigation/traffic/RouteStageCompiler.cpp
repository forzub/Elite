#include "src/game/navigation/traffic/RouteStageCompiler.h"

#include <cmath>
#include <utility>

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
    glm::dvec3* crossingForwardWorld,
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

    if (crossingForwardWorld)
    {
        *crossingForwardWorld =
            frameIt->second.directionToWorld(
                portal->crossingForwardLocal
            );
        if (!finite3(*crossingForwardWorld) ||
            glm::length(*crossingForwardWorld) <= 1.0e-9)
        {
            failure =
                "traffic portal resolved invalid crossing direction: " +
                portalId;
            return false;
        }
        *crossingForwardWorld =
            glm::normalize(*crossingForwardWorld);
    }

    return true;
}

bool compileVolumeGeometry(
    const TrafficRouteStage& stage,
    const NavigationVolumeCatalog& volumes,
    const RouteStageCompiler::ReferenceFrameMap& frames,
    std::vector<CompiledNavigationVolumeSection>& sections,
    std::vector<glm::dvec3>& centerline,
    double& requiredClearanceMeters,
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

    sections.clear();
    centerline.clear();
    sections.reserve(volume->sections.size());
    centerline.reserve(volume->sections.size());
    requiredClearanceMeters = volume->requiredClearanceMeters;

    for (const auto& section : volume->sections)
    {
        CompiledNavigationVolumeSection resolved;
        resolved.centerWorldMeters =
            frameIt->second.pointToWorld(section.centerLocalMeters);
        resolved.forwardWorld =
            frameIt->second.directionToWorld(section.forwardLocal);
        resolved.upWorld =
            frameIt->second.directionToWorld(section.upLocal);
        resolved.crossSection = section.crossSection;
        resolved.radiusMeters = section.radiusMeters;
        resolved.halfWidthMeters = section.halfWidthMeters;
        resolved.halfHeightMeters = section.halfHeightMeters;

        if (!finite3(resolved.centerWorldMeters) ||
            !finite3(resolved.forwardWorld) ||
            !finite3(resolved.upWorld) ||
            glm::length(resolved.forwardWorld) <= 1.0e-9 ||
            glm::length(resolved.upWorld) <= 1.0e-9)
        {
            failure =
                "navigation volume section resolved non-finite: " +
                volume->id;
            return false;
        }

        resolved.forwardWorld = glm::normalize(resolved.forwardWorld);
        resolved.upWorld = glm::normalize(resolved.upWorld);

        sections.push_back(resolved);
        appendUnique(centerline, resolved.centerWorldMeters);
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
                    nullptr,
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
                    nullptr,
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

            if (!compileVolumeGeometry(
                    stage,
                    volumes,
                    referenceFrames,
                    compiled.volumeSections,
                    compiled.requiredCenterlineMeters,
                    compiled.volumeRequiredClearanceMeters,
                    out.failure))
            {
                return out;
            }

            // Keep topology authoritative at the boundaries. The authored
            // corridor may contain the same points as its first/last section,
            // but the semantic portals still own entry/exit ordering.
            const auto* entryPortal =
                graph.portal(stage.fromPortalId);
            if (!entryPortal)
            {
                out.failure =
                    "volume transit lost entry portal definition";
                return out;
            }

            if (entryPortal->inboundConnection.kind ==
                PortalConnectionKind::MandatoryTangentStraight)
            {
                glm::dvec3 portalPosition(0.0);
                glm::dvec3 crossingForward(0.0);
                if (!resolvePortalWorld(
                        stage.fromPortalId,
                        graph,
                        anchors,
                        referenceFrames,
                        portalPosition,
                        &crossingForward,
                        out.failure))
                {
                    return out;
                }

                CompiledMandatoryTangentStraight straight;
                straight.portalId = stage.fromPortalId;
                straight.inbound = true;
                straight.endWorldMeters = portalPosition;
                straight.forwardWorld = crossingForward;
                straight.lengthMeters =
                    entryPortal->inboundConnection.mandatoryStraightMeters;
                straight.startWorldMeters =
                    portalPosition -
                    crossingForward * straight.lengthMeters;

                out.mandatoryTangentStraights.push_back(straight);
                appendUnique(
                    out.requiredViaPointsMeters,
                    straight.startWorldMeters
                );
            }

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

            const auto* exitPortal =
                graph.portal(stage.toPortalId);
            if (!exitPortal)
            {
                out.failure =
                    "volume transit lost exit portal definition";
                return out;
            }

            if (exitPortal->outboundConnection.kind ==
                PortalConnectionKind::MandatoryTangentStraight)
            {
                glm::dvec3 portalPosition(0.0);
                glm::dvec3 crossingForward(0.0);
                if (!resolvePortalWorld(
                        stage.toPortalId,
                        graph,
                        anchors,
                        referenceFrames,
                        portalPosition,
                        &crossingForward,
                        out.failure))
                {
                    return out;
                }

                CompiledMandatoryTangentStraight straight;
                straight.portalId = stage.toPortalId;
                straight.inbound = false;
                straight.startWorldMeters = portalPosition;
                straight.forwardWorld = crossingForward;
                straight.lengthMeters =
                    exitPortal->outboundConnection.mandatoryStraightMeters;
                straight.endWorldMeters =
                    portalPosition +
                    crossingForward * straight.lengthMeters;

                out.mandatoryTangentStraights.push_back(straight);
                appendUnique(
                    out.requiredViaPointsMeters,
                    straight.endWorldMeters
                );
            }
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
