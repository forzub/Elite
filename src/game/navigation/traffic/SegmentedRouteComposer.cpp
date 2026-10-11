#include "src/game/navigation/traffic/SegmentedRouteComposer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <utility>
#include <vector>

#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/OrdinaryRoutePolicy.h"
#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"

namespace game::navigation::traffic
{
namespace
{

bool samePoint(const glm::dvec3& a, const glm::dvec3& b)
{
    return glm::length(a - b) <= 1.0e-6;
}

void appendUniqueGate(
    std::vector<planner::RouteGate>& dst,
    const planner::RouteGate& gate
)
{
    if (!dst.empty() &&
        samePoint(dst.back().positionMeters, gate.positionMeters))
    {
        dst.back().forward = gate.forward;
        dst.back().speedMps = std::max(dst.back().speedMps, gate.speedMps);
        return;
    }
    dst.push_back(gate);
}

void appendShiftedCurves(
    std::vector<planner::RouteCurveSegment>& dst,
    const std::vector<planner::RouteCurveSegment>& src
)
{
    double offset =
        dst.empty() ? 0.0 : dst.back().endProgressMeters;

    for (auto curve : src)
    {
        const double localStart = curve.startProgressMeters;
        const double localEnd = curve.endProgressMeters;
        curve.startProgressMeters = offset + localStart;
        curve.endProgressMeters = offset + localEnd;
        dst.push_back(std::move(curve));
    }
}

void appendPlanSamples(
    planner::RoutePlan& out,
    const planner::RoutePlan& leg,
    bool virtualTerminal,
    double fallbackSpeedMps
)
{
    appendShiftedCurves(out.routeCurves, leg.routeCurves);

    for (std::size_t i = 0; i < leg.executionGates.size(); ++i)
    {
        auto gate = leg.executionGates[i];
        if (virtualTerminal &&
            i + 1 == leg.executionGates.size() &&
            gate.speedMps <= 1.0e-6)
        {
            gate.speedMps = fallbackSpeedMps;
        }
        appendUniqueGate(out.executionGates, gate);
    }

    for (std::size_t i = 0; i < leg.gates.size(); ++i)
    {
        auto gate = leg.gates[i];
        if (virtualTerminal &&
            i + 1 == leg.gates.size() &&
            gate.speedMps <= 1.0e-6)
        {
            gate.speedMps = fallbackSpeedMps;
        }
        appendUniqueGate(out.gates, gate);
    }
}

double routeEndProgress(
    const planner::RoutePlan& plan
) noexcept
{
    return
        plan.routeCurves.empty()
            ? 0.0
            : plan.routeCurves.back().endProgressMeters;
}

void appendStageSpan(
    planner::RoutePlan& out,
    const std::string& id,
    planner::RouteStageKind kind,
    double startProgressMeters,
    double endProgressMeters,
    planner::RouteStageFramePolicy framePolicy,
    const glm::dvec3& frameForward,
    const glm::dvec3& frameUp,
    const std::string& sourceId,
    const std::string& volumeId,
    bool hardContainment
)
{
    if (endProgressMeters <= startProgressMeters + 1.0e-6)
        return;

    planner::RouteStageSpan span;
    span.id = id;
    span.kind = kind;
    span.startProgressMeters = startProgressMeters;
    span.endProgressMeters = endProgressMeters;
    span.framePolicy = framePolicy;
    span.refreshPolicy =
        planner::RouteStageRefreshPolicy::FrozenAtPlanning;
    span.frameForward = frameForward;
    span.frameUp = frameUp;
    span.sourceId = sourceId;
    span.volumeId = volumeId;
    span.hardContainment = hardContainment;
    out.stages.push_back(std::move(span));
}

double polylineLength(
    const std::vector<glm::dvec3>& points
) noexcept
{
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i)
        total += glm::length(points[i] - points[i - 1]);
    return total;
}

void appendMandatoryBlock(
    planner::RoutePlan& out,
    const CompiledTrafficStage& stage,
    const glm::dvec3& blockStart,
    const glm::dvec3& blockEnd,
    const planner::RoutePlanRequest& base
)
{
    std::vector<glm::dvec3> points;
    const auto appendPoint =
        [&](const glm::dvec3& point)
        {
            if (points.empty() || !samePoint(points.back(), point))
                points.push_back(point);
        };

    // The semantic boundary block owns the complete authored passage:
    // inbound tangent straight -> volume -> outbound tangent straight.
    // Free-space planners connect only to the two OUTER boundary states.
    appendPoint(blockStart);
    appendPoint(stage.fromWorldMeters);
    for (const auto& point : stage.requiredCenterlineMeters)
        appendPoint(point);
    appendPoint(stage.toWorldMeters);
    appendPoint(blockEnd);

    if (points.size() < 2)
        return;

    const double stageSpeed =
        game::navigation::OrdinaryRoutePolicy::
            stageSpeedLimitMps(
                base.maxSpeedMps,
                stage.volumeConstraint.maxSpeedMps
            );

    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const glm::dvec3 delta = points[i] - points[i - 1];
        const double length = glm::length(delta);
        if (length <= 1.0e-9)
            continue;

        const glm::dvec3 forward = delta / length;

        planner::RouteCurveSegment curve;
        curve.kind = planner::RouteCurveKind::Line;
        curve.startProgressMeters =
            out.routeCurves.empty()
                ? 0.0
                : out.routeCurves.back().endProgressMeters;
        curve.endProgressMeters =
            curve.startProgressMeters + length;
        curve.maxSpeedMps = stageSpeed;
        curve.startMeters = points[i - 1];
        curve.endMeters = points[i];
        curve.startForward = forward;
        curve.endForward = forward;
        out.routeCurves.push_back(std::move(curve));

        const double spacing =
            std::max(
                1.0,
                std::min(10.0, base.gateSpacingMeters)
            );
        const int subdivisions =
            std::max(1, static_cast<int>(std::ceil(length / spacing)));

        for (int j = 1; j <= subdivisions; ++j)
        {
            const double t =
                static_cast<double>(j) /
                static_cast<double>(subdivisions);
            planner::RouteGate gate;
            gate.positionMeters =
                j == subdivisions
                    ? points[i]
                    : glm::mix(points[i - 1], points[i], t);
            gate.forward = forward;
            gate.speedMps = stageSpeed;
            appendUniqueGate(out.executionGates, gate);
        }

        planner::RouteGate sparse;
        sparse.positionMeters = points[i];
        sparse.forward = forward;
        sparse.speedMps = stageSpeed;
        appendUniqueGate(out.gates, sparse);
    }
}

void recomputeGlobalSpeeds(
    planner::RoutePlan& out,
    const planner::RoutePlanRequest& base
)
{
    auto& gates = out.executionGates;
    if (gates.size() < 2)
        return;

    const double cruise =
        std::max(0.5, 0.8 * base.maxSpeedMps);

    for (std::size_t i = 0; i < gates.size(); ++i)
    {
        if (!std::isfinite(gates[i].speedMps) ||
            gates[i].speedMps <= 1.0e-6)
        {
            gates[i].speedMps = cruise;
        }
        else
        {
            gates[i].speedMps =
                std::min(gates[i].speedMps, cruise);
        }
    }

    // Only the actual final destination is a zero-speed boundary.
    gates.back().speedMps = 0.0;

    for (std::size_t i = 1; i + 1 < gates.size(); ++i)
    {
        const glm::dvec3 a =
            gates[i].positionMeters -
            gates[i - 1].positionMeters;
        const glm::dvec3 b =
            gates[i + 1].positionMeters -
            gates[i].positionMeters;
        const double la = glm::length(a);
        const double lb = glm::length(b);
        const double across = glm::length(
            gates[i + 1].positionMeters -
            gates[i - 1].positionMeters
        );
        const double denom = la * lb * across;
        if (denom <= 1.0e-9)
            continue;

        const double curvature =
            2.0 * glm::length(glm::cross(a, b)) / denom;
        if (curvature > 1.0e-9)
        {
            gates[i].speedMps = std::min(
                gates[i].speedMps,
                std::sqrt(base.lateralMps2 / curvature)
            );
        }
    }

    for (std::size_t i = gates.size() - 1; i > 0; --i)
    {
        const double ds = glm::length(
            gates[i].positionMeters -
            gates[i - 1].positionMeters
        );
        gates[i - 1].speedMps = std::min(
            gates[i - 1].speedMps,
            std::sqrt(
                gates[i].speedMps * gates[i].speedMps +
                2.0 * base.brakingMps2 * ds
            )
        );
    }

    double reachable =
        std::max(0.0, base.initialSpeedMps);
    for (std::size_t i = 1; i < gates.size(); ++i)
    {
        const double ds = glm::length(
            gates[i].positionMeters -
            gates[i - 1].positionMeters
        );
        reachable = std::sqrt(
            reachable * reachable +
            2.0 * base.acceleratingMps2 * ds
        );
        gates[i].speedMps =
            std::min(gates[i].speedMps, reachable);
        reachable = gates[i].speedMps;
    }

    // Sparse HUD gates are only a view of the execution route. Transfer the
    // nearest dense speed ceiling instead of independently inventing another
    // profile.
    for (auto& sparse : out.gates)
    {
        double bestDistance =
            std::numeric_limits<double>::infinity();
        double bestSpeed = cruise;
        for (const auto& dense : gates)
        {
            const double distance =
                glm::length(
                    dense.positionMeters -
                    sparse.positionMeters
                );
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestSpeed = dense.speedMps;
            }
        }
        sparse.speedMps = bestSpeed;
    }
}

planner::RoutePlan failedComposite(
    const planner::RoutePlan& leg,
    const char* prefix
)
{
    planner::RoutePlan out = leg;
    out.failure =
        std::string(prefix) +
        (leg.failure.empty()
             ? std::string("unknown planner failure")
             : leg.failure);
    out.userMessage = out.failure;
    return out;
}

} // namespace

planner::RoutePlan SegmentedRouteComposer::plan(
    const planner::RoutePlanRequest& terminalRequest,
    const CompiledTrafficRoute& trafficRoute
)
{
    if (!trafficRoute.valid)
    {
        planner::RoutePlan out;
        out.disposition =
            planner::RoutePlanDisposition::InvalidWorldData;
        out.failureCode =
            planner::RoutePlanFailureCode::InvalidWorldGeometry;
        out.failure =
            trafficRoute.failure.empty()
                ? "segmented route received invalid traffic route"
                : trafficRoute.failure;
        out.userMessage = out.failure;
        return out;
    }

    std::vector<const CompiledTrafficStage*> mandatoryStages;
    for (const auto& stage : trafficRoute.stages)
    {
        if (stage.kind != TrafficRouteStageKind::VolumeTransit)
            continue;
        if (stage.volumeConstraint.policy !=
                world::navigation::NavigationVolumePolicy::KeepInside &&
            stage.volumeConstraint.policy !=
                world::navigation::NavigationVolumePolicy::PreferInside)
        {
            continue;
        }
        mandatoryStages.push_back(&stage);
    }

    if (mandatoryStages.empty())
        return planner::RoutePlanner::plan(terminalRequest);

    std::cerr
        << "[SegmentedRoute]"
        << " free_legs=" << (mandatoryStages.size() + 1)
        << " mandatory_segments=" << mandatoryStages.size()
        << std::endl;

    planner::RoutePlan out;
    out.disposition = planner::RoutePlanDisposition::Ready;
    out.failureCode = planner::RoutePlanFailureCode::None;
    out.userMessage = planner::routePlanFailureMessage(out.failureCode);

    planner::RoutePlanRequest legRequest = terminalRequest;
    legRequest.requiredViaPointsMeters.clear();
    legRequest.mandatoryTangentStraights.clear();

    glm::dvec3 currentStart = terminalRequest.startMeters;
    bool currentHasForward = terminalRequest.hasInitialForward;
    glm::dvec3 currentForward = terminalRequest.initialForward;
    double currentSpeed = terminalRequest.initialSpeedMps;

    const auto inboundBoundaryFor =
        [&](const CompiledTrafficStage& stage)
            -> const CompiledMandatoryTangentStraight*
        {
            for (const auto& straight :
                 trafficRoute.mandatoryTangentStraights)
            {
                if (straight.inbound &&
                    straight.portalId == stage.fromPortalId)
                {
                    return &straight;
                }
            }
            return nullptr;
        };

    const auto outboundBoundaryFor =
        [&](const CompiledTrafficStage& stage)
            -> const CompiledMandatoryTangentStraight*
        {
            for (const auto& straight :
                 trafficRoute.mandatoryTangentStraights)
            {
                if (!straight.inbound &&
                    straight.portalId == stage.toPortalId)
                {
                    return &straight;
                }
            }
            return nullptr;
        };

    for (std::size_t stageIndex = 0;
         stageIndex < mandatoryStages.size();
         ++stageIndex)
    {
        const auto& stage = *mandatoryStages[stageIndex];
        if (stage.volumeSections.empty())
        {
            planner::RoutePlan invalid;
            invalid.disposition =
                planner::RoutePlanDisposition::InvalidWorldData;
            invalid.failureCode =
                planner::RoutePlanFailureCode::InvalidWorldGeometry;
            invalid.failure =
                "mandatory traffic volume has no resolved sections";
            invalid.userMessage = invalid.failure;
            return invalid;
        }

        glm::dvec3 traversalForward =
            stage.toWorldMeters - stage.fromWorldMeters;
        if (glm::length(traversalForward) <= 1.0e-9)
            traversalForward =
                stage.volumeSections.front().forwardWorld;
        if (glm::length(traversalForward) <= 1.0e-9)
        {
            planner::RoutePlan invalid;
            invalid.disposition =
                planner::RoutePlanDisposition::InvalidWorldData;
            invalid.failureCode =
                planner::RoutePlanFailureCode::InvalidWorldGeometry;
            invalid.failure =
                "mandatory traffic traversal direction is degenerate";
            invalid.userMessage = invalid.failure;
            return invalid;
        }
        traversalForward = glm::normalize(traversalForward);

        const auto* inboundBoundary =
            inboundBoundaryFor(stage);
        const auto* outboundBoundary =
            outboundBoundaryFor(stage);

        const glm::dvec3 blockStart =
            inboundBoundary
                ? inboundBoundary->startWorldMeters
                : stage.fromWorldMeters;
        const glm::dvec3 blockEnd =
            outboundBoundary
                ? outboundBoundary->endWorldMeters
                : stage.toWorldMeters;

        const double handoffSpeed =
            game::navigation::OrdinaryRoutePolicy::
                stageSpeedLimitMps(
                    terminalRequest.maxSpeedMps,
                    stage.volumeConstraint.maxSpeedMps
                );

        legRequest.startMeters = currentStart;
        legRequest.goalMeters = blockStart;
        legRequest.terminalOutward = -traversalForward;
        legRequest.initialSpeedMps = currentSpeed;
        legRequest.hasInitialForward = currentHasForward;
        legRequest.initialForward = currentForward;
        legRequest.initialForwardLeadMeters =
            currentHasForward
                ? game::navigation::OrdinaryRoutePolicy::
                      requiredStartLeadMeters(
                          currentSpeed,
                          legRequest.maxSpeedMps,
                          legRequest.brakingMps2,
                          legRequest.lateralMps2,
                          legRequest.maxAngularVelocityRadPerSecond,
                          legRequest.vehicleLengthMeters
                      )
                : 0.0;
        legRequest.requiredViaPointsMeters.clear();
        legRequest.mandatoryTangentStraights.clear();

        // This is a virtual terminal boundary. Size the final straight
        // from the speed reduction required before entering the mandatory
        // stage; do not inherit any docking-specific constant.
        legRequest.terminalReferenceDistanceMeters =
            std::min(
                terminalRequest.terminalReferenceDistanceMeters,
                200.0
            );
        legRequest.terminalApproachLengthMeters =
            game::navigation::OrdinaryRoutePolicy::
                requiredTerminalApproachMeters(
                    terminalRequest.maxSpeedMps,
                    handoffSpeed,
                    terminalRequest.brakingMps2,
                    legRequest.terminalReferenceDistanceMeters,
                    terminalRequest.vehicleLengthMeters
                );

        std::cerr
            << "[SegmentedLeg]"
            << " index=" << stageIndex
            << " kind=free-to-boundary"
            << " from=("
            << legRequest.startMeters.x << ","
            << legRequest.startMeters.y << ","
            << legRequest.startMeters.z << ")"
            << " to=("
            << legRequest.goalMeters.x << ","
            << legRequest.goalMeters.y << ","
            << legRequest.goalMeters.z << ")"
            << " boundary_to_entry_m="
            << glm::length(stage.fromWorldMeters - blockStart)
            << " start_speed_mps=" << currentSpeed
            << " start_lead_m="
            << legRequest.initialForwardLeadMeters
            << " boundary_speed_mps=" << handoffSpeed
            << " terminal_approach_m="
            << legRequest.terminalApproachLengthMeters
            << std::endl;

        const auto inbound =
            planner::RoutePlanner::plan(legRequest);
        if (!inbound.valid())
            return failedComposite(
                inbound,
                "segmented inbound leg failed: "
            );

        std::cerr
            << "[SegmentedLegPlan]"
            << " index=" << stageIndex
            << " route_m="
            << (inbound.routeCurves.empty()
                    ? 0.0
                    : inbound.routeCurves.back().endProgressMeters)
            << " curves=" << inbound.routeCurves.size()
            << " execution_gates=" << inbound.executionGates.size()
            << " diagnostics=\"" << inbound.diagnosticSummary << "\""
            << std::endl;

        const double freeStageStart =
            routeEndProgress(out);

        appendPlanSamples(
            out,
            inbound,
            true,
            handoffSpeed
        );

        const double freeStageEnd =
            routeEndProgress(out);

        appendStageSpan(
            out,
            "free:" + std::to_string(stageIndex),
            planner::RouteStageKind::FreeTransit,
            freeStageStart,
            freeStageEnd,
            planner::RouteStageFramePolicy::Transported,
            currentForward,
            glm::dvec3(0.0, 1.0, 0.0),
            {},
            {},
            false
        );

        const double mandatoryStart =
            routeEndProgress(out);

        appendMandatoryBlock(
            out,
            stage,
            blockStart,
            blockEnd,
            terminalRequest
        );

        const glm::dvec3 stageUp =
            !stage.volumeSections.empty() &&
            glm::length(stage.volumeSections.front().upWorld) > 1.0e-9
                ? glm::normalize(stage.volumeSections.front().upWorld)
                : glm::dvec3(0.0, 1.0, 0.0);

        std::vector<glm::dvec3> volumePoints;
        const auto appendVolumePoint =
            [&](const glm::dvec3& point)
            {
                if (volumePoints.empty() ||
                    !samePoint(volumePoints.back(), point))
                {
                    volumePoints.push_back(point);
                }
            };

        appendVolumePoint(stage.fromWorldMeters);
        for (const auto& point : stage.requiredCenterlineMeters)
            appendVolumePoint(point);
        appendVolumePoint(stage.toWorldMeters);

        const double entryLength =
            glm::length(stage.fromWorldMeters - blockStart);
        const double volumeLength =
            polylineLength(volumePoints);
        const double exitLength =
            glm::length(blockEnd - stage.toWorldMeters);

        const double entryEnd =
            mandatoryStart + entryLength;
        const double volumeEnd =
            entryEnd + volumeLength;
        const double expectedMandatoryEnd =
            volumeEnd + exitLength;
        const double mandatoryEnd =
            routeEndProgress(out);

        if (std::abs(
                mandatoryEnd - expectedMandatoryEnd
            ) > 1.0e-4)
        {
            planner::RoutePlan invalid = out;
            invalid.disposition =
                planner::RoutePlanDisposition::InvalidWorldData;
            invalid.failureCode =
                planner::RoutePlanFailureCode::InvalidWorldGeometry;
            invalid.failure =
                "segmented mandatory stage progress accounting mismatch";
            invalid.userMessage = invalid.failure;
            return invalid;
        }

        const std::string stageSource =
            stage.laneId.empty()
                ? stage.volumeConstraint.volumeId
                : stage.laneId;

        appendStageSpan(
            out,
            "volume-entry:" + stageSource,
            planner::RouteStageKind::VolumeEntryCapture,
            mandatoryStart,
            entryEnd,
            planner::RouteStageFramePolicy::NavigationFrame,
            traversalForward,
            stageUp,
            stage.fromPortalId,
            stage.volumeConstraint.volumeId,
            false
        );

        appendStageSpan(
            out,
            "volume-transit:" + stageSource,
            planner::RouteStageKind::VolumeTransit,
            entryEnd,
            volumeEnd,
            planner::RouteStageFramePolicy::NavigationFrame,
            traversalForward,
            stageUp,
            stageSource,
            stage.volumeConstraint.volumeId,
            stage.volumeConstraint.policy ==
                world::navigation::NavigationVolumePolicy::KeepInside
        );

        appendStageSpan(
            out,
            "volume-exit:" + stageSource,
            planner::RouteStageKind::VolumeExit,
            volumeEnd,
            mandatoryEnd,
            planner::RouteStageFramePolicy::NavigationFrame,
            traversalForward,
            stageUp,
            stage.toPortalId,
            stage.volumeConstraint.volumeId,
            false
        );

        if (stage.volumeConstraint.policy ==
            world::navigation::NavigationVolumePolicy::KeepInside)
        {
            const auto containment =
                RouteVolumeContainmentValidator::
                    validateKeepInsideRouteInterval(
                        stage,
                        out.routeCurves,
                        stage.fromWorldMeters,
                        stage.toWorldMeters,
                        terminalRequest.agentRadiusMeters
                    );

            if (!containment.valid)
            {
                planner::RoutePlan invalid = out;
                invalid.disposition =
                    planner::RoutePlanDisposition::InvalidWorldData;
                invalid.failureCode =
                    planner::RoutePlanFailureCode::InvalidWorldGeometry;
                invalid.failure =
                    "segmented mandatory BLUE containment failed: " +
                    containment.failure;
                invalid.userMessage = invalid.failure;
                return invalid;
            }
        }

        std::cerr
            << "[SegmentedMandatory]"
            << " index=" << stageIndex
            << " volume=" << stage.volumeConstraint.volumeId
            << " block_from=("
            << blockStart.x << ","
            << blockStart.y << ","
            << blockStart.z << ")"
            << " entry=("
            << stage.fromWorldMeters.x << ","
            << stage.fromWorldMeters.y << ","
            << stage.fromWorldMeters.z << ")"
            << " exit=("
            << stage.toWorldMeters.x << ","
            << stage.toWorldMeters.y << ","
            << stage.toWorldMeters.z << ")"
            << " block_to=("
            << blockEnd.x << ","
            << blockEnd.y << ","
            << blockEnd.z << ")"
            << std::endl;

        currentStart = blockEnd;
        currentForward = traversalForward;
        currentHasForward = true;
        currentSpeed = handoffSpeed;
    }

    planner::RoutePlanRequest finalRequest = terminalRequest;
    finalRequest.startMeters = currentStart;
    finalRequest.initialSpeedMps = currentSpeed;
    finalRequest.hasInitialForward = currentHasForward;
    finalRequest.initialForward = currentForward;

    // Every free leg starts from a real moving state. Recompute its
    // straight lead from exit speed and vehicle dynamics exactly as at the
    // original ship start.
    finalRequest.initialForwardLeadMeters =
        currentHasForward
            ? game::navigation::OrdinaryRoutePolicy::
                  requiredStartLeadMeters(
                      currentSpeed,
                      finalRequest.maxSpeedMps,
                      finalRequest.brakingMps2,
                      finalRequest.lateralMps2,
                      finalRequest.maxAngularVelocityRadPerSecond,
                      finalRequest.vehicleLengthMeters
                  )
            : 0.0;

    // The authored mandatory release already guarantees that the whole craft
    // clears the tunnel. Keep only a short straight beyond that boundary, but
    // retain the full dynamic lead as TURN BUDGET so the first curve does not
    // collapse to a small radius.
    finalRequest.initialProtectedStraightMeters =
        currentHasForward ? 150.0 : 0.0;

    finalRequest.requiredViaPointsMeters.clear();
    finalRequest.mandatoryTangentStraights.clear();

    std::cerr
        << "[SegmentedLeg]"
        << " index=" << mandatoryStages.size()
        << " kind=free-to-terminal"
        << " from=("
        << finalRequest.startMeters.x << ","
        << finalRequest.startMeters.y << ","
        << finalRequest.startMeters.z << ")"
        << " to=("
        << finalRequest.goalMeters.x << ","
        << finalRequest.goalMeters.y << ","
        << finalRequest.goalMeters.z << ")"
        << " start_speed_mps=" << currentSpeed
        << " start_lead_m="
        << finalRequest.initialForwardLeadMeters
        << " protected_straight_m="
        << finalRequest.initialProtectedStraightMeters
        << " terminal_approach_m="
        << finalRequest.terminalApproachLengthMeters
        << std::endl;

    const auto finalLeg =
        planner::RoutePlanner::plan(finalRequest);
    if (!finalLeg.valid())
        return failedComposite(
            finalLeg,
            "segmented final leg failed: "
        );

    std::cerr
        << "[SegmentedLegPlan]"
        << " index=" << mandatoryStages.size()
        << " route_m="
        << (finalLeg.routeCurves.empty()
                ? 0.0
                : finalLeg.routeCurves.back().endProgressMeters)
        << " curves=" << finalLeg.routeCurves.size()
        << " execution_gates=" << finalLeg.executionGates.size()
        << " diagnostics=\"" << finalLeg.diagnosticSummary << "\""
        << std::endl;

    const double finalFreeStart =
        routeEndProgress(out);

    appendPlanSamples(
        out,
        finalLeg,
        false,
        0.0
    );

    appendStageSpan(
        out,
        "free:terminal",
        planner::RouteStageKind::FreeTransit,
        finalFreeStart,
        routeEndProgress(out),
        planner::RouteStageFramePolicy::Transported,
        currentForward,
        glm::dvec3(0.0, 1.0, 0.0),
        {},
        {},
        false
    );

    recomputeGlobalSpeeds(out, terminalRequest);

    // Terminal diagnostics belong to the real terminal leg. Initial-turn
    // diagnostics belong to the first free leg and are already represented in
    // the composite geometry; these fields are advisory only.
    out.terminalDetourUsed = finalLeg.terminalDetourUsed;
    out.terminalTurnRadiusRelaxed =
        finalLeg.terminalTurnRadiusRelaxed;
    out.terminalTurnRequestedRadiusMeters =
        finalLeg.terminalTurnRequestedRadiusMeters;
    out.terminalTurnSpeedMps =
        finalLeg.terminalTurnSpeedMps;
    out.terminalTurnLateralRadiusMeters =
        finalLeg.terminalTurnLateralRadiusMeters;
    out.terminalTurnAngularRadiusMeters =
        finalLeg.terminalTurnAngularRadiusMeters;
    out.terminalTurnAngularRampMeters =
        finalLeg.terminalTurnAngularRampMeters;
    out.terminalArcCandidatesTested =
        finalLeg.terminalArcCandidatesTested;
    out.terminalArcRouteable =
        finalLeg.terminalArcRouteable;
    out.terminalArcRouteRejected =
        finalLeg.terminalArcRouteRejected;
    out.terminalArcTransitReady =
        finalLeg.terminalArcTransitReady;
    out.terminalArcTransitRejected =
        finalLeg.terminalArcTransitRejected;
    out.terminalArcCollisionRejected =
        finalLeg.terminalArcCollisionRejected;
    out.terminalArcAcceptedCandidates =
        finalLeg.terminalArcAcceptedCandidates;
    out.terminalArcAxisPassesTested =
        finalLeg.terminalArcAxisPassesTested;
    out.terminalArcLastRejection =
        finalLeg.terminalArcLastRejection;
    out.terminalArcDominantBlockerId =
        finalLeg.terminalArcDominantBlockerId;
    out.terminalArcDominantBlockerHits =
        finalLeg.terminalArcDominantBlockerHits;
    out.terminalArcDominantBlockerCenterMeters =
        finalLeg.terminalArcDominantBlockerCenterMeters;
    out.terminalArcDominantBlockerHalfExtentsMeters =
        finalLeg.terminalArcDominantBlockerHalfExtentsMeters;
    out.terminalArcDominantBlockerRadiusMeters =
        finalLeg.terminalArcDominantBlockerRadiusMeters;
    out.terminalArcRotationDegrees =
        finalLeg.terminalArcRotationDegrees;
    out.terminalApproachShortened =
        finalLeg.terminalApproachShortened;
    out.terminalApproachExtended =
        finalLeg.terminalApproachExtended;
    out.terminalApproachLengthMeters =
        finalLeg.terminalApproachLengthMeters;
    out.terminalTurnRadiusMeters =
        finalLeg.terminalTurnRadiusMeters;

    out.diagnosticSummary =
        "segmented route: free legs=" +
        std::to_string(mandatoryStages.size() + 1) +
        " mandatory volumes=" +
        std::to_string(mandatoryStages.size()) +
        " semantic stages=" +
        std::to_string(out.stages.size());

    if (out.routeCurves.empty() ||
        out.executionGates.size() < 2 ||
        out.gates.size() < 2)
    {
        out.disposition =
            planner::RoutePlanDisposition::NeedsRefinement;
        out.failureCode =
            planner::RoutePlanFailureCode::BackendFailure;
        out.failure =
            "segmented route composition produced incomplete geometry";
        out.userMessage = out.failure;
    }

    return out;
}

} // namespace game::navigation::traffic
