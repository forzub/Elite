#include <cstdio>
#include "src/game/navigation/NavigationGeometryTolerance.h"
#include "src/game/navigation/traffic/SegmentedRouteComposer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/OrdinaryRoutePolicy.h"
#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"
#include "src/game/navigation/traffic/NarrowZoneEnvelope.h"

namespace game::navigation::traffic
{
namespace
{
// Pure route code owns no clock (gate check_navigation_route_boundaries): the diagnostic planning
// times come from a clock the orchestration installs (0 when none).
SegmentedRouteDiagnosticClockMs g_diagnosticClock = nullptr;
double diagnosticNowMs() { return g_diagnosticClock ? g_diagnosticClock() : 0.0; }
}

void setSegmentedRouteDiagnosticClock(SegmentedRouteDiagnosticClockMs clock) { g_diagnosticClock = clock; }

namespace
{

bool samePoint(const glm::dvec3& a, const glm::dvec3& b)
{
    return glm::length(a - b) <= game::navigation::geometry_tolerance::ConstructedPointMeters;
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

double minimumSampledRadiusMeters(
    const std::vector<planner::RouteCurveSegment>& curves
) noexcept
{
    double maxCurvature = 0.0;
    constexpr int SamplesPerCurve = 32;

    for (const auto& curve : curves)
    {
        for (int i = 0; i <= SamplesPerCurve; ++i)
        {
            const double u =
                static_cast<double>(i) /
                static_cast<double>(SamplesPerCurve);
            const double progress =
                curve.startProgressMeters +
                (curve.endProgressMeters -
                 curve.startProgressMeters) * u;
            maxCurvature =
                std::max(
                    maxCurvature,
                    curve.curvatureAtProgress(progress)
                );
        }
    }

    return
        maxCurvature > 1.0e-12
            ? 1.0 / maxCurvature
            : std::numeric_limits<double>::infinity();
}

double maximumJoinCurvatureJump(
    const std::vector<planner::RouteCurveSegment>& curves
) noexcept
{
    double maximum = 0.0;
    for (std::size_t i = 1; i < curves.size(); ++i)
    {
        const double before =
            curves[i - 1].curvatureAtProgress(
                curves[i - 1].endProgressMeters
            );
        const double after =
            curves[i].curvatureAtProgress(
                curves[i].startProgressMeters
            );
        maximum =
            std::max(
                maximum,
                std::abs(after - before)
            );
    }
    return maximum;
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

            if (base.maxAngularVelocityRadPerSecond > 1.0e-9)
            {
                gates[i].speedMps = std::min(
                    gates[i].speedMps,
                    base.maxAngularVelocityRadPerSecond / curvature
                );
            }
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

// a free leg (to / from / between the lanes) among a city's buildings: tight turns, a small obstacle search
// 40: the final leg to a pad's approach point was feasible only at <= 45 m/s - every faster attempt (150 / 82 / 60) cost
// a whole 20 s composition before the slower retry (live 2026-11-22: 72-100 s a pad route)
static constexpr double FreeLegInCityMaxSpeedMps = 40.0;

// A free leg sees only the obstacles near it: those whose bounding sphere comes within a margin of the leg's
// straight segment (the margin: half the leg length, at least 1.5 km - room for its turns). A generated settlement
// has thousands of hit volumes; a short connector between two lanes must not search all of them.
static planner::RoutePlanRequest withLocalObstacles(const planner::RoutePlanRequest& leg)
{
    planner::RoutePlanRequest out = leg;
    const glm::dvec3 a = leg.startMeters, b = leg.goalMeters;
    const glm::dvec3 ab = b - a;
    const double len2 = glm::dot(ab, ab);
    // the room the leg's turns need (3 turn radii at its top speed) + a share of its length; a free leg in a city is
    // slow (FreeLegInCityMaxSpeedMps) - a 1.5 km margin took all ~3500 city volumes into every search: 30-55 s a leg,
    // 80-100 s a pad route (author 2026-11-22: «он ужасно долго считает маршрут»). The accepted route is checked against
    // ALL the obstacles afterwards ([RouteCheck]): an obstacle missed here fails the route, it is never flown into.
    const double turnRadius = leg.maxSpeedMps * leg.maxSpeedMps / std::max(1.0, leg.lateralMps2);
    const double margin = std::max(300.0, 3.0 * turnRadius) + std::min(0.25 * std::sqrt(len2), 800.0);
    out.obstacles.clear();
    for (const auto& o : leg.obstacles)
    {
        const double t = len2 > 1.0e-9 ? std::clamp(glm::dot(o.centerMeters - a, ab) / len2, 0.0, 1.0) : 0.0;
        const double d = glm::length(o.centerMeters - (a + ab * t));
        if (d <= margin + o.conservativeRadiusMeters() + o.requiredClearanceMeters)
            out.obstacles.push_back(o);
    }
    return out;
}

static planner::RoutePlan composeOnce(
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

    planner::RoutePlan out;
    out.disposition = planner::RoutePlanDisposition::Ready;
    out.failureCode = planner::RoutePlanFailureCode::None;
    out.userMessage = planner::routePlanFailureMessage(out.failureCode);

    // Route planning remains pure. Keep performance/geometry diagnostics
    // inside the returned route value, not global logging or side effects.
    std::string freeLegDiagnostics;
    double freeLegTotalMs = 0.0;

    planner::RoutePlanRequest legRequest = terminalRequest;
    legRequest.requiredViaPointsMeters.clear();
    legRequest.mandatoryTangentStraights.clear();
    // off the lanes among a city's buildings (thousands of volumes) the free legs are flown slowly: tight turns, a
    // small search (the lanes keep their own speeds)
    // ... only a leg that starts among them: the approach from open space keeps its speed (capped from 10 km out it
    // crawled at 40 m/s for minutes, live 2026-11-22)
    const auto startsInCity = [](const planner::RoutePlanRequest& r) {
        if (r.obstacles.size() <= 500)
            return false;
        for (const auto& o : r.obstacles)
            if (glm::length(o.centerMeters - r.startMeters) < 1500.0 + o.conservativeRadiusMeters())
                return true;
        return false;
    };
    if (startsInCity(legRequest))
        legRequest.maxSpeedMps = std::min(legRequest.maxSpeedMps, FreeLegInCityMaxSpeedMps);

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
        // A stage whose centreline turns (a hub lane chain through pre-written turns) enters and leaves along its
        // boundary straights, not along from -> to.
        const glm::dvec3 entryForward =
            inboundBoundary && glm::length(inboundBoundary->forwardWorld) > 1.0e-9
                ? glm::normalize(inboundBoundary->forwardWorld)
                : traversalForward;
        const glm::dvec3 exitForward =
            outboundBoundary && glm::length(outboundBoundary->forwardWorld) > 1.0e-9
                ? glm::normalize(outboundBoundary->forwardWorld)
                : traversalForward;

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
        legRequest.terminalOutward = -entryForward;
        legRequest.initialSpeedMps = std::min(currentSpeed, legRequest.maxSpeedMps);
        legRequest.terminalSpeedMps = handoffSpeed;
        legRequest.hasInitialForward = currentHasForward;
        legRequest.initialForward = currentForward;
        legRequest.initialForwardLeadMeters =
            currentHasForward
                ? (legRequest.assistedCompactStartLead
                  ? game::navigation::OrdinaryRoutePolicy::
                      requiredAssistedStartLeadMeters(
                          currentSpeed,
                          legRequest.maxSpeedMps,
                          legRequest.brakingMps2,
                          legRequest.lateralMps2,
                          legRequest.maxAngularVelocityRadPerSecond,
                          legRequest.vehicleLengthMeters
                      )
                  : game::navigation::OrdinaryRoutePolicy::
                      requiredStartLeadMeters(
                          currentSpeed,
                          legRequest.maxSpeedMps,
                          legRequest.brakingMps2,
                          legRequest.lateralMps2,
                          legRequest.maxAngularVelocityRadPerSecond,
                          legRequest.vehicleLengthMeters
                      ))
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

        const double inboundStarted = diagnosticNowMs();
        const auto inbound =
            planner::RoutePlanner::plan(withLocalObstacles(legRequest));
        const double inboundMs = diagnosticNowMs() - inboundStarted;
        freeLegTotalMs += inboundMs;
        freeLegDiagnostics +=
            " inbound_ms=" + std::to_string(inboundMs) +
            " inbound_start_hard_m=" +
            std::to_string(
                std::max(legRequest.initialForwardLeadMeters,
                         legRequest.initialProtectedStraightMeters)
            ) +
            " inbound_terminal_hard_m=" +
            std::to_string(legRequest.terminalApproachLengthMeters) +
            " inbound_detail={ " + inbound.diagnosticSummary + " }";
        if (!inbound.valid())
        {
            // the failed leg's geometry and the planner's own diagnostics (live 2026-11-17: a hub lane join failing
            // with only "did not reach a feasible path" in the log)
            const auto v = [](const glm::dvec3& p) {
                return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + "," + std::to_string(p.z) + ")";
            };
            planner::RoutePlan failed = failedComposite(inbound, "segmented inbound leg failed: ");
            failed.diagnosticSummary =
                "inbound_leg{ start=" + v(legRequest.startMeters) + " goal=" + v(legRequest.goalMeters) +
                " terminal_outward=" + v(legRequest.terminalOutward) +
                " initial_forward=" + v(legRequest.initialForward) +
                " has_forward=" + std::to_string(legRequest.hasInitialForward) +
                " lead_m=" + std::to_string(legRequest.initialForwardLeadMeters) +
                " terminal_approach_m=" + std::to_string(legRequest.terminalApproachLengthMeters) +
                " terminal_ref_m=" + std::to_string(legRequest.terminalReferenceDistanceMeters) +
                " max_mps=" + std::to_string(legRequest.maxSpeedMps) +
                " terminal_mps=" + std::to_string(legRequest.terminalSpeedMps) +
                " lat=" + std::to_string(legRequest.lateralMps2) +
                " brake=" + std::to_string(legRequest.brakingMps2) +
                " terminal_turn_r=" + std::to_string(legRequest.preferredTerminalTurnRadiusMeters) +
                " derive_turn_r=" + std::to_string(legRequest.deriveTerminalTurnRadiusFromVehicle) +
                " obstacles=" + std::to_string(withLocalObstacles(legRequest).obstacles.size()) +
                " } planner{ " + inbound.diagnosticSummary + " }";
            return failed;
        }

        appendPlanSamples(
            out,
            inbound,
            true,
            handoffSpeed
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
            ) > game::navigation::geometry_tolerance::ComposedStageLengthMeters)
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

        if (stage.volumeConstraint.policy ==
            world::navigation::NavigationVolumePolicy::KeepInside)
        {
            // Step 3: the hull, not its centre of mass, must stay inside.
            // A volume only the aligned hull box fits in is a narrow zone:
            // validated with the box's own insets and handed to execution
            // as a fixed-attitude stretch.
            ShipHullBox hull;
            hull.halfWidthMeters = 0.5 * terminalRequest.vehicleWidthMeters;
            hull.halfHeightMeters = 0.5 * terminalRequest.vehicleHeightMeters;
            hull.halfLengthMeters = 0.5 * terminalRequest.vehicleLengthMeters;
            const VolumeFit fit = analyseVolumeFit(
                stage, hull, terminalRequest.agentRadiusMeters,
                terminalRequest.narrowZoneAttitudeToleranceRad);
            const bool narrow = !fit.sphereFits && fit.alignedFits;
            if (!fit.sphereFits && !fit.alignedFits)
            {
                planner::RoutePlan invalid = out;
                invalid.disposition =
                    planner::RoutePlanDisposition::InvalidWorldData;
                invalid.failureCode =
                    planner::RoutePlanFailureCode::InvalidWorldGeometry;
                invalid.failure =
                    "the ship does not fit the mandatory volume in any "
                    "attitude";
                invalid.userMessage = invalid.failure;
                return invalid;
            }
            const auto containment =
                narrow
                    ? RouteVolumeContainmentValidator::
                          validateKeepInsideRouteIntervalInsets(
                              stage,
                              out.routeCurves,
                              stage.fromWorldMeters,
                              stage.toWorldMeters,
                              fit.insetRightMeters,
                              fit.insetUpMeters)
                    : RouteVolumeContainmentValidator::
                          validateKeepInsideRouteInterval(
                              stage,
                              out.routeCurves,
                              stage.fromWorldMeters,
                              stage.toWorldMeters,
                              terminalRequest.agentRadiusMeters
                          );
            if (narrow && containment.valid)
            {
                planner::RouteNarrowZone zone;
                RouteVolumeContainmentValidator::progressOnRoute(
                    out.routeCurves, stage.fromWorldMeters,
                    zone.startProgressMeters);
                RouteVolumeContainmentValidator::progressOnRoute(
                    out.routeCurves, stage.toWorldMeters,
                    zone.endProgressMeters);
                zone.axisWorld = fit.axisWorld;
                zone.upWorld = fit.upWorld;
                zone.lateralToleranceRightMeters = fit.marginRightMeters;
                zone.lateralToleranceUpMeters = fit.marginUpMeters;
                zone.maxSpeedMps = handoffSpeed;
                const RotationFit rotation = analyseRotationFit(
                    fit, hull,
                    terminalRequest.narrowZoneAttitudeToleranceRad,
                    std::min(fit.marginRightMeters, fit.marginUpMeters));
                zone.safeFlipAxisWorld = rotation.flipAxisWorld;
                zone.flipMarginMeters = rotation.flipMarginMeters;
                zone.rollSafe = rotation.rollSafe;
                out.narrowZones.push_back(zone);
            }

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

        currentStart = blockEnd;
        currentForward = exitForward;
        currentHasForward = true;
        currentSpeed = handoffSpeed;
    }

    // a pad route ends on its last lane: no final free leg (it threaded the city lattice and failed / crashed, live
    // 2026-11-22) - the route stops at the exit; the pad service flies on at transit height
    if (terminalRequest.endAtLaneExit)
    {
        freeLegDiagnostics += " final=lane_exit(pad service)";
        recomputeGlobalSpeeds(out, terminalRequest);
        out.diagnosticSummary += freeLegDiagnostics;
        return out;
    }

    planner::RoutePlanRequest finalRequest = terminalRequest;
    finalRequest.startMeters = currentStart;
    // in a city the final free leg is slow: it starts at its own limit - the lane brakes to it before the exit (the
    // route speed profile); started at the lane speed (100-150) it was infeasible at every retry (live 2026-11-22)
    finalRequest.initialSpeedMps = finalRequest.obstacles.size() > 500 ? std::min(currentSpeed, FreeLegInCityMaxSpeedMps) : currentSpeed;
    finalRequest.terminalSpeedMps = 0.0;
    finalRequest.hasInitialForward = currentHasForward;
    finalRequest.initialForward = currentForward;

    // Every free leg starts from a real moving state. Recompute its
    // straight lead from exit speed and vehicle dynamics exactly as at the
    // original ship start.
    finalRequest.initialForwardLeadMeters =
        currentHasForward
            ? (finalRequest.assistedCompactStartLead
              ? game::navigation::OrdinaryRoutePolicy::
                  requiredAssistedStartLeadMeters(
                      currentSpeed,
                      finalRequest.maxSpeedMps,
                      finalRequest.brakingMps2,
                      finalRequest.lateralMps2,
                      finalRequest.maxAngularVelocityRadPerSecond,
                      finalRequest.vehicleLengthMeters
                  )
              : game::navigation::OrdinaryRoutePolicy::
                  requiredStartLeadMeters(
                      currentSpeed,
                      finalRequest.maxSpeedMps,
                      finalRequest.brakingMps2,
                      finalRequest.lateralMps2,
                      finalRequest.maxAngularVelocityRadPerSecond,
                      finalRequest.vehicleLengthMeters
                  ))
            : 0.0;

    // The authored mandatory release already guarantees that the whole craft
    // clears the tunnel. Keep only a short straight beyond that boundary, but
    // retain the full dynamic lead as TURN BUDGET so the first curve does not
    // collapse to a small radius.
    finalRequest.initialProtectedStraightMeters =
        currentHasForward ? SegmentedRouteComposer::ReleaseProtectedStraightMeters : 0.0;

    // Assisted final free leg is geometrically independent of the initial
    // high-speed ballistic ship entry. The authoritative BLUE release is
    // already an exact hard stage. Keep only its 150m exit straight, not the
    // entire manoeuvre budget as a second mandatory straight.
    finalRequest.assistedUseExactProtectedStart =
        finalRequest.assistedCompactStartLead &&
        finalRequest.useContinuousAssistedOptimizer;
    if (finalRequest.assistedUseExactProtectedStart)
    {
        // The actual required physical arrival speed at the docking HOLD
        // point is zero. The continuous optimizer already computes the
        // feasible braking profile over the WHOLE terminal leg. Therefore
        // maxSpeed^2/(2*braking) must not become an arbitrarily long *hard*
        // final tangent. Retain the geometric minimum for docking approach.
        finalRequest.terminalApproachLengthMeters =
            game::navigation::OrdinaryRoutePolicy::
                requiredDockingAxisMinimumMeters(
                    finalRequest.terminalReferenceDistanceMeters,
                    finalRequest.vehicleLengthMeters
                );
    }

    finalRequest.requiredViaPointsMeters.clear();
    finalRequest.mandatoryTangentStraights.clear();

    const double terminalLegStarted = diagnosticNowMs();
    const auto finalLeg =
        planner::RoutePlanner::plan(withLocalObstacles([&] {
            auto capped = finalRequest;
            if (capped.obstacles.size() > 500)
                capped.maxSpeedMps = std::min(capped.maxSpeedMps, FreeLegInCityMaxSpeedMps);
            return capped;
        }()));
    const double finalLegMs = diagnosticNowMs() - terminalLegStarted;
    freeLegTotalMs += finalLegMs;
    freeLegDiagnostics +=
        " final_ms=" + std::to_string(finalLegMs) +
        " final_start_hard_m=" +
        std::to_string(
            finalRequest.assistedUseExactProtectedStart
                ? finalRequest.initialProtectedStraightMeters
                : std::max(finalRequest.initialProtectedStraightMeters,
                           finalRequest.initialForwardLeadMeters)
        ) +
        " final_lead_budget_m=" +
        std::to_string(finalRequest.initialForwardLeadMeters) +
        " final_goal_hard_m=" +
        std::to_string(finalRequest.terminalApproachLengthMeters) +
        " final_exit_speed_mps=" +
        std::to_string(currentSpeed) +
        " final_detail={ " + finalLeg.diagnosticSummary + " }";
    if (!finalLeg.valid())
        return failedComposite(
            finalLeg,
            "segmented final leg failed: "
        );

    appendPlanSamples(
        out,
        finalLeg,
        false,
        0.0
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
        " compiled_traffic_stages=" +
        std::to_string(trafficRoute.stages.size()) +
        " total_length_m=" +
        std::to_string(routeEndProgress(out)) +
        " direct_distance_m=" +
        std::to_string(glm::length(
            terminalRequest.goalMeters - terminalRequest.startMeters
        )) +
        " min_radius_m=" +
        std::to_string(
            minimumSampledRadiusMeters(out.routeCurves)
        ) +
        " max_join_dk=" +
        std::to_string(
            maximumJoinCurvatureJump(out.routeCurves)
        ) +
        " free_leg_total_ms=" +
        std::to_string(freeLegTotalMs) +
        freeLegDiagnostics;

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

planner::RoutePlan SegmentedRouteComposer::plan(
    const planner::RoutePlanRequest& terminalRequest,
    const CompiledTrafficRoute& trafficRoute
)
{
    auto out = composeOnce(terminalRequest, trafficRoute);
    // Assisted: a route the derated craft cannot fly at its cruise speed is
    // planned again SLOWER (cruise and, once below the zone limits, the
    // mandatory-zone hand-off speeds), not refused - reaching the dock
    // matters more than the time. Only a dynamically infeasible plan is
    // retried; invalid world data or no authority stays a refusal.
    // Healthy plans that succeed at full speed are unchanged.
    constexpr double SlowerSpeedScales[] = {0.75, 0.55, 0.40, 0.30, 0.20};
    if (!terminalRequest.useContinuousAssistedOptimizer)
        return out;
    for (const double scale : SlowerSpeedScales)
    {
        if (out.valid() ||
            out.disposition != planner::RoutePlanDisposition::NeedsRefinement)
            break;
        // why the attempt failed (each attempt costs a whole composition - 20 s a pad route, live 2026-11-22)
        std::fprintf(stderr, "[RouteRetry] attempt at speed x%.2f failed: %s\n", scale == SlowerSpeedScales[0] ? 1.0 : scale,
                     out.failure.substr(0, 600).c_str());
        if (!out.diagnosticSummary.empty())
            std::fprintf(stderr, "[RouteRetry]   detail: %s\n", out.diagnosticSummary.substr(0, 900).c_str());
        auto slower = terminalRequest;
        slower.maxSpeedMps = terminalRequest.maxSpeedMps * scale;
        auto retry = composeOnce(slower, trafficRoute);
        retry.diagnosticSummary =
            "slower_retry_speed_scale=" + std::to_string(scale) + " " +
            retry.diagnosticSummary;
        out = std::move(retry);
    }
    return out;
}

} // namespace game::navigation::traffic
