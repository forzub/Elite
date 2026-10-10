#include "src/game/navigation/traffic/TrafficRoutePlanAdapter.h"

#include <algorithm>
#include <utility>

#include "src/game/navigation/RouteFrameField.h"

namespace game::navigation::traffic
{

TrafficRoutePlanAdapter::Result
TrafficRoutePlanAdapter::buildStageContract(
    const CompiledTrafficRoute& trafficRoute,
    const planner::RoutePlan& geometricPlan,
    const glm::dvec3& terminalUpReference
)
{
    Result out;

    if (!trafficRoute.valid)
    {
        out.failure = trafficRoute.failure.empty()
            ? "traffic route is invalid"
            : trafficRoute.failure;
        return out;
    }
    if (!geometricPlan.valid() || geometricPlan.routeCurves.empty())
    {
        out.failure = "geometric route plan is not ready";
        return out;
    }

    const auto routeProgressAt =
        [&](const glm::dvec3& point, double& progressMeters)
        {
            return RouteFrameField::progressAtPoint(
                geometricPlan.routeCurves,
                point,
                progressMeters
            );
        };

    const auto appendStage =
        [&](std::string id,
            planner::RouteStageKind kind,
            double startProgressMeters,
            double endProgressMeters,
            planner::RouteStageFramePolicy framePolicy,
            planner::RouteStageRefreshPolicy refreshPolicy,
            const glm::dvec3& frameForward,
            const glm::dvec3& frameUp,
            std::string sourceId,
            std::string volumeId,
            bool hardContainment)
        {
            if (!(endProgressMeters > startProgressMeters + 1.0e-6))
                return;

            planner::RouteStageSpan span;
            span.id = std::move(id);
            span.kind = kind;
            span.startProgressMeters = startProgressMeters;
            span.endProgressMeters = endProgressMeters;
            span.framePolicy = framePolicy;
            span.refreshPolicy = refreshPolicy;
            span.frameForward = frameForward;
            span.frameUp = frameUp;
            span.sourceId = std::move(sourceId);
            span.volumeId = std::move(volumeId);
            span.hardContainment = hardContainment;
            out.stages.push_back(std::move(span));
        };

    for (const auto& stage : trafficRoute.stages)
    {
        if (stage.kind != TrafficRouteStageKind::VolumeTransit ||
            stage.volumeSections.empty())
        {
            continue;
        }

        double entryProgress = 0.0;
        double exitProgress = 0.0;
        if (!routeProgressAt(stage.fromWorldMeters, entryProgress) ||
            !routeProgressAt(stage.toWorldMeters, exitProgress))
        {
            out.failure =
                "traffic stage boundary is absent from geometric route";
            return out;
        }

        const auto& firstSection = stage.volumeSections.front();
        const auto& lastSection = stage.volumeSections.back();

        double captureStartProgress = entryProgress;
        double releaseEndProgress = exitProgress;

        for (const auto& straight : trafficRoute.mandatoryTangentStraights)
        {
            double p = 0.0;
            if (straight.inbound &&
                straight.portalId == stage.fromPortalId &&
                routeProgressAt(straight.startWorldMeters, p))
            {
                captureStartProgress =
                    std::min(captureStartProgress, p);
            }

            if (!straight.inbound &&
                straight.portalId == stage.toPortalId &&
                routeProgressAt(straight.endWorldMeters, p))
            {
                releaseEndProgress =
                    std::max(releaseEndProgress, p);
            }
        }

        appendStage(
            "capture:" + stage.fromPortalId,
            planner::RouteStageKind::VolumeEntryCapture,
            captureStartProgress,
            entryProgress,
            planner::RouteStageFramePolicy::NavigationFrame,
            planner::RouteStageRefreshPolicy::FrozenAtPlanning,
            firstSection.forwardWorld,
            firstSection.upWorld,
            stage.fromPortalId,
            stage.volumeConstraint.volumeId,
            false
        );

        appendStage(
            "volume:" + stage.laneId,
            planner::RouteStageKind::VolumeTransit,
            entryProgress,
            exitProgress,
            planner::RouteStageFramePolicy::NavigationFrame,
            planner::RouteStageRefreshPolicy::FrozenAtPlanning,
            firstSection.forwardWorld,
            firstSection.upWorld,
            stage.laneId,
            stage.volumeConstraint.volumeId,
            stage.volumeConstraint.policy ==
                world::navigation::NavigationVolumePolicy::KeepInside
        );

        appendStage(
            "release:" + stage.toPortalId,
            planner::RouteStageKind::VolumeExit,
            exitProgress,
            releaseEndProgress,
            planner::RouteStageFramePolicy::NavigationFrame,
            planner::RouteStageRefreshPolicy::FrozenAtPlanning,
            lastSection.forwardWorld,
            lastSection.upWorld,
            stage.toPortalId,
            stage.volumeConstraint.volumeId,
            false
        );
    }

    std::sort(
        out.stages.begin(),
        out.stages.end(),
        [](const auto& a, const auto& b)
        {
            return a.startProgressMeters < b.startProgressMeters;
        }
    );

    const double routeStart =
        geometricPlan.routeCurves.front().startProgressMeters;
    const double routeEnd =
        geometricPlan.routeCurves.back().endProgressMeters;

    std::vector<planner::RouteStageSpan> complete;
    complete.reserve(out.stages.size() + 2);

    double cursor = routeStart;
    for (const auto& explicitStage : out.stages)
    {
        if (explicitStage.startProgressMeters > cursor + 1.0e-6)
        {
            planner::RouteStageSpan freeStage;
            freeStage.id =
                complete.empty() ? "free:initial" : "free:between";
            freeStage.kind = planner::RouteStageKind::FreeTransit;
            freeStage.startProgressMeters = cursor;
            freeStage.endProgressMeters =
                explicitStage.startProgressMeters;

            // Free approach is allowed to prepare the orientation required by
            // the next constrained stage, but it does not gain its semantic
            // containment authority.
            freeStage.framePolicy = explicitStage.framePolicy;
            freeStage.refreshPolicy = explicitStage.refreshPolicy;
            freeStage.frameForward = explicitStage.frameForward;
            freeStage.frameUp = explicitStage.frameUp;
            freeStage.sourceId = explicitStage.sourceId;
            complete.push_back(std::move(freeStage));
        }

        complete.push_back(explicitStage);
        cursor = std::max(cursor, explicitStage.endProgressMeters);
    }

    if (routeEnd > cursor + 1.0e-6)
    {
        planner::RouteStageSpan terminal;
        terminal.id = "terminal";
        terminal.kind = planner::RouteStageKind::TerminalApproach;
        terminal.startProgressMeters = cursor;
        terminal.endProgressMeters = routeEnd;
        terminal.framePolicy =
            planner::RouteStageFramePolicy::LiveDockFrame;
        terminal.refreshPolicy =
            planner::RouteStageRefreshPolicy::LiveDuringStage;
        terminal.frameForward =
            geometricPlan.routeCurves.back().endForward;
        terminal.frameUp = terminalUpReference;
        terminal.sourceId = "terminal";
        complete.push_back(std::move(terminal));
    }

    out.stages = std::move(complete);

    glm::dvec3 previousOwnedUp = terminalUpReference;
    bool havePreviousOwnedUp = false;

    for (const auto& stage : out.stages)
    {
        if (stage.framePolicy ==
            planner::RouteStageFramePolicy::Transported)
        {
            continue;
        }

        planner::RouteFrameAnchor begin;
        begin.progressMeters = stage.startProgressMeters;

        planner::RouteFrameAnchor end;
        end.progressMeters = stage.endProgressMeters;

        if (stage.framePolicy ==
            planner::RouteStageFramePolicy::LiveDockFrame)
        {
            begin.upReference =
                havePreviousOwnedUp
                    ? previousOwnedUp
                    : stage.frameUp;
            begin.dynamicRollPhaseWeight = 0.0;

            end.upReference = stage.frameUp;
            end.dynamicRollPhaseWeight = 1.0;
        }
        else
        {
            begin.upReference = stage.frameUp;
            begin.dynamicRollPhaseWeight = 0.0;
            end = begin;
            end.progressMeters = stage.endProgressMeters;

            previousOwnedUp = stage.frameUp;
            havePreviousOwnedUp = true;
        }

        out.frameAnchors.push_back(begin);
        out.frameAnchors.push_back(end);
    }

    std::sort(
        out.frameAnchors.begin(),
        out.frameAnchors.end(),
        [](const auto& a, const auto& b)
        {
            return a.progressMeters < b.progressMeters;
        }
    );

    out.valid = true;
    return out;
}

} // namespace game::navigation::traffic
