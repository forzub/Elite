#include "src/game/navigation/planner/RouteEntryBinder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "src/game/navigation/OrdinaryRoutePolicy.h"

namespace game::navigation::planner
{
namespace
{

bool samePoint(const glm::dvec3& a, const glm::dvec3& b)
{
    return glm::length(a - b) <= 1.0e-4;
}

void appendUniqueGate(
    std::vector<RouteGate>& dst,
    const RouteGate& gate
)
{
    if (!dst.empty() &&
        samePoint(dst.back().positionMeters, gate.positionMeters))
    {
        dst.back().forward = gate.forward;

        // At an internal splice, two positive values are both speed ceilings,
        // so the stricter one wins. Preserve a positive handoff if one side is
        // a legacy zero-terminal artifact; HOLD/final zero constraints are not
        // used as rebound anchors.
        if (dst.back().speedMps > 1.0e-6 &&
            gate.speedMps > 1.0e-6)
        {
            dst.back().speedMps =
                std::min(dst.back().speedMps, gate.speedMps);
        }
        else
        {
            dst.back().speedMps =
                std::max(dst.back().speedMps, gate.speedMps);
        }
        return;
    }
    dst.push_back(gate);
}

std::size_t nearestGateIndex(
    const std::vector<RouteGate>& gates,
    const glm::dvec3& point
)
{
    if (gates.empty())
        return 0;

    std::size_t best = 0;
    double bestDistance2 = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < gates.size(); ++i)
    {
        const glm::dvec3 delta = gates[i].positionMeters - point;
        const double d2 = glm::dot(delta, delta);
        if (d2 < bestDistance2)
        {
            bestDistance2 = d2;
            best = i;
        }
    }
    return best;
}

void appendShiftedCurves(
    std::vector<RouteCurveSegment>& dst,
    const std::vector<RouteCurveSegment>& src
)
{
    const double offset =
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

double minimumSampledRadiusMeters(
    const std::vector<RouteCurveSegment>& curves
) noexcept
{
    double maxCurvature = 0.0;
    constexpr int SamplesPerCurve = 48;

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
    const std::vector<RouteCurveSegment>& curves
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
            std::max(maximum, std::abs(after - before));
    }
    return maximum;
}

struct ExecutionStress
{
    double maxLateralUtilization = 0.0;
    double maxAngularVelocityUtilization = 0.0;
    double maxAngularAccelerationUtilization = 0.0;
};

ExecutionStress executionStress(
    const std::vector<RouteGate>& gates,
    const RoutePlanRequest& base
) noexcept
{
    ExecutionStress out;
    if (gates.size() < 3)
        return out;

    std::vector<double> curvature(gates.size(), 0.0);
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
        const double across =
            glm::length(
                gates[i + 1].positionMeters -
                gates[i - 1].positionMeters
            );
        const double denom = la * lb * across;
        if (denom > 1.0e-9)
        {
            curvature[i] =
                2.0 * glm::length(glm::cross(a, b)) / denom;
        }
    }

    for (std::size_t i = 0; i < gates.size(); ++i)
    {
        const double v = std::max(0.0, gates[i].speedMps);
        const double kappa = curvature[i];

        if (base.lateralMps2 > 1.0e-9)
        {
            out.maxLateralUtilization =
                std::max(
                    out.maxLateralUtilization,
                    v * v * kappa / base.lateralMps2
                );
        }

        if (base.maxAngularVelocityRadPerSecond > 1.0e-9)
        {
            out.maxAngularVelocityUtilization =
                std::max(
                    out.maxAngularVelocityUtilization,
                    v * kappa /
                        base.maxAngularVelocityRadPerSecond
                );
        }
    }

    if (base.maxAngularAccelerationRadPerSecond2 > 1.0e-9)
    {
        for (std::size_t i = 0; i + 1 < gates.size(); ++i)
        {
            const double ds =
                glm::length(
                    gates[i + 1].positionMeters -
                    gates[i].positionMeters
                );
            const double sumSpeed =
                std::max(
                    1.0e-6,
                    gates[i].speedMps +
                    gates[i + 1].speedMps
                );
            const double dt = 2.0 * ds / sumSpeed;
            if (dt <= 1.0e-9)
                continue;

            const double omega0 =
                gates[i].speedMps * curvature[i];
            const double omega1 =
                gates[i + 1].speedMps * curvature[i + 1];

            out.maxAngularAccelerationUtilization =
                std::max(
                    out.maxAngularAccelerationUtilization,
                    std::abs(omega1 - omega0) /
                        dt /
                        base.maxAngularAccelerationRadPerSecond2
                );
        }
    }

    return out;
}

void recomputeGlobalSpeeds(
    RoutePlan& out,
    const RoutePlanRequest& base
)
{
    auto& gates = out.executionGates;
    if (gates.size() < 2)
        return;

    const double cruise =
        std::max(0.5, 0.8 * base.maxSpeedMps);

    for (auto& gate : gates)
    {
        if (!std::isfinite(gate.speedMps) ||
            gate.speedMps <= 1.0e-6)
        {
            gate.speedMps = cruise;
        }
        else
        {
            gate.speedMps = std::min(gate.speedMps, cruise);
        }
    }

    gates.back().speedMps = 0.0;

    std::vector<double> curvature(gates.size(), 0.0);

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

        curvature[i] =
            2.0 * glm::length(glm::cross(a, b)) / denom;
        const double kappa = curvature[i];
        if (kappa > 1.0e-9)
        {
            if (base.lateralMps2 > 1.0e-9)
            {
                gates[i].speedMps = std::min(
                    gates[i].speedMps,
                    std::sqrt(base.lateralMps2 / kappa)
                );
            }

            if (base.maxAngularVelocityRadPerSecond > 1.0e-9)
            {
                gates[i].speedMps = std::min(
                    gates[i].speedMps,
                    base.maxAngularVelocityRadPerSecond /
                        kappa
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

    if (base.maxAngularAccelerationRadPerSecond2 > 1.0e-9)
    {
        constexpr int AngularProfileIterations = 10;
        constexpr double AngularReserve = 0.995;

        for (int iteration = 0;
             iteration < AngularProfileIterations;
             ++iteration)
        {
            bool adjusted = false;

            for (std::size_t i = 0; i + 1 < gates.size(); ++i)
            {
                const double ds =
                    glm::length(
                        gates[i + 1].positionMeters -
                        gates[i].positionMeters
                    );
                if (ds <= 1.0e-9)
                    continue;

                const double sumSpeed =
                    gates[i].speedMps +
                    gates[i + 1].speedMps;
                if (sumSpeed <= 1.0e-6)
                    continue;

                const double dt = 2.0 * ds / sumSpeed;
                if (dt <= 1.0e-9)
                    continue;

                const double omega0 =
                    gates[i].speedMps * curvature[i];
                const double omega1 =
                    gates[i + 1].speedMps * curvature[i + 1];
                const double demandedAlpha =
                    std::abs(omega1 - omega0) / dt;

                if (demandedAlpha <=
                    base.maxAngularAccelerationRadPerSecond2 *
                        (1.0 + 1.0e-6))
                {
                    continue;
                }

                const double scale =
                    AngularReserve *
                    std::sqrt(
                        base.maxAngularAccelerationRadPerSecond2 /
                        demandedAlpha
                    );

                gates[i].speedMps *= scale;
                gates[i + 1].speedMps *= scale;
                adjusted = true;
            }

            if (!adjusted)
                break;

            for (std::size_t i = gates.size() - 1; i > 0; --i)
            {
                const double ds =
                    glm::length(
                        gates[i].positionMeters -
                        gates[i - 1].positionMeters
                    );
                gates[i - 1].speedMps =
                    std::min(
                        gates[i - 1].speedMps,
                        std::sqrt(
                            gates[i].speedMps *
                                gates[i].speedMps +
                            2.0 * base.brakingMps2 * ds
                        )
                    );
            }

            double reachableAfterAngular =
                std::max(0.0, base.initialSpeedMps);
            for (std::size_t i = 1; i < gates.size(); ++i)
            {
                const double ds =
                    glm::length(
                        gates[i].positionMeters -
                        gates[i - 1].positionMeters
                    );
                reachableAfterAngular =
                    std::sqrt(
                        reachableAfterAngular *
                            reachableAfterAngular +
                        2.0 * base.acceleratingMps2 * ds
                    );
                gates[i].speedMps =
                    std::min(
                        gates[i].speedMps,
                        reachableAfterAngular
                    );
                reachableAfterAngular =
                    gates[i].speedMps;
            }
        }
    }

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

} // namespace

LateBoundRouteBackbone RouteEntryBinder::extractBackbone(
    const RoutePlan& source,
    bool hasPreferredAnchor,
    const glm::dvec3& preferredAnchor,
    const glm::dvec3& preferredForward
)
{
    LateBoundRouteBackbone out;

    if (!source.valid() || source.routeCurves.size() < 2)
    {
        out.failure = "late entry requires at least two route curves";
        return out;
    }

    std::size_t curveIndex = source.routeCurves.size() - 1;

    if (hasPreferredAnchor)
    {
        bool found = false;
        for (std::size_t i = 1; i < source.routeCurves.size(); ++i)
        {
            if (samePoint(
                    source.routeCurves[i].startMeters,
                    preferredAnchor
                ))
            {
                curveIndex = i;
                found = true;
                break;
            }
        }
        if (!found)
        {
            out.failure =
                "late entry could not locate semantic backbone anchor";
            return out;
        }
    }

    const auto& firstTailCurve = source.routeCurves[curveIndex];
    const double oldOffset = firstTailCurve.startProgressMeters;

    out.tail = source;
    out.tail.routeCurves.assign(
        source.routeCurves.begin() +
            static_cast<std::ptrdiff_t>(curveIndex),
        source.routeCurves.end()
    );
    for (auto& curve : out.tail.routeCurves)
    {
        curve.startProgressMeters -= oldOffset;
        curve.endProgressMeters -= oldOffset;
    }

    out.anchorMeters = firstTailCurve.startMeters;
    out.anchorForward =
        hasPreferredAnchor &&
        glm::length(preferredForward) > 1.0e-9
            ? glm::normalize(preferredForward)
            : firstTailCurve.startForward;

    const std::size_t denseStart =
        nearestGateIndex(source.executionGates, out.anchorMeters);
    out.tail.executionGates.assign(
        source.executionGates.begin() +
            static_cast<std::ptrdiff_t>(denseStart),
        source.executionGates.end()
    );

    const std::size_t sparseStart =
        nearestGateIndex(source.gates, out.anchorMeters);
    out.tail.gates.assign(
        source.gates.begin() +
            static_cast<std::ptrdiff_t>(sparseStart),
        source.gates.end()
    );

    if (out.tail.executionGates.empty() ||
        !samePoint(
            out.tail.executionGates.front().positionMeters,
            out.anchorMeters
        ))
    {
        RouteGate anchor;
        anchor.positionMeters = out.anchorMeters;
        anchor.forward = out.anchorForward;
        anchor.speedMps = firstTailCurve.maxSpeedMps;
        out.tail.executionGates.insert(
            out.tail.executionGates.begin(),
            anchor
        );
    }

    if (out.tail.gates.empty() ||
        !samePoint(out.tail.gates.front().positionMeters, out.anchorMeters))
    {
        RouteGate anchor;
        anchor.positionMeters = out.anchorMeters;
        anchor.forward = out.anchorForward;
        anchor.speedMps = firstTailCurve.maxSpeedMps;
        out.tail.gates.insert(out.tail.gates.begin(), anchor);
    }

    out.handoffSpeedMps =
        !out.tail.executionGates.empty() &&
        out.tail.executionGates.front().speedMps > 1.0e-6
            ? out.tail.executionGates.front().speedMps
            : std::max(0.0, firstTailCurve.maxSpeedMps);

    std::vector<RouteFrameAnchor> shiftedAnchors;
    for (auto anchor : source.routeFrameAnchors)
    {
        if (anchor.progressMeters + 1.0e-6 < oldOffset)
            continue;
        anchor.progressMeters =
            std::max(0.0, anchor.progressMeters - oldOffset);
        shiftedAnchors.push_back(anchor);
    }
    out.tail.routeFrameAnchors = std::move(shiftedAnchors);

    std::vector<RouteStageSpan> shiftedStages;
    for (auto stage : source.stages)
    {
        if (stage.endProgressMeters <= oldOffset + 1.0e-6)
            continue;
        stage.startProgressMeters =
            std::max(0.0, stage.startProgressMeters - oldOffset);
        stage.endProgressMeters =
            std::max(0.0, stage.endProgressMeters - oldOffset);
        shiftedStages.push_back(std::move(stage));
    }
    out.tail.stages = std::move(shiftedStages);

    if (out.tail.routeCurves.empty() ||
        out.tail.executionGates.size() < 2 ||
        out.tail.gates.size() < 2)
    {
        out.failure = "late entry backbone tail is incomplete";
        return out;
    }

    out.valid = true;
    return out;
}

RoutePlan RouteEntryBinder::bind(
    const RoutePlanRequest& liveRequest,
    const LateBoundRouteBackbone& backbone
)
{
    RoutePlan invalid;
    if (!backbone.valid)
    {
        invalid.failure =
            backbone.failure.empty()
                ? "late entry backbone is invalid"
                : backbone.failure;
        invalid.userMessage = invalid.failure;
        return invalid;
    }

    RoutePlanRequest entryRequest = liveRequest;
    entryRequest.goalMeters = backbone.anchorMeters;
    entryRequest.terminalOutward = -backbone.anchorForward;
    entryRequest.terminalReferenceDistanceMeters =
        std::min(
            std::max(0.0, liveRequest.terminalReferenceDistanceMeters),
            200.0
        );

    // The connector must arrive at the immutable tail with the real handoff
    // speed and tangent. The tail already owns its protected semantic straight
    // (for BLUE this is the authored capture axis). Do not prepend a second
    // braking straight derived from route max speed: the continuous speed
    // profile already brakes wherever the actual geometry requires it.
    entryRequest.terminalSpeedMps =
        std::clamp(
            backbone.handoffSpeedMps,
            0.0,
            std::max(0.0, liveRequest.maxSpeedMps)
        );
    entryRequest.terminalApproachLengthMeters = 0.0;
    entryRequest.requiredViaPointsMeters.clear();
    entryRequest.mandatoryTangentStraights.clear();

    auto entry = RoutePlanner::plan(entryRequest);
    if (!entry.valid())
    {
        entry.failure =
            "late entry connector failed: " + entry.failure;
        entry.userMessage = entry.failure;
        return entry;
    }

    if (!entry.executionGates.empty())
        entry.executionGates.back().speedMps = backbone.handoffSpeedMps;
    if (!entry.gates.empty())
        entry.gates.back().speedMps = backbone.handoffSpeedMps;

    RoutePlan out = backbone.tail;
    out.disposition = RoutePlanDisposition::Ready;
    out.failureCode = RoutePlanFailureCode::None;
    out.failure.clear();
    out.userMessage = routePlanFailureMessage(out.failureCode);

    out.routeCurves.clear();
    appendShiftedCurves(out.routeCurves, entry.routeCurves);
    appendShiftedCurves(out.routeCurves, backbone.tail.routeCurves);

    out.executionGates.clear();
    for (const auto& gate : entry.executionGates)
        appendUniqueGate(out.executionGates, gate);
    for (const auto& gate : backbone.tail.executionGates)
        appendUniqueGate(out.executionGates, gate);

    out.gates.clear();
    for (const auto& gate : entry.gates)
        appendUniqueGate(out.gates, gate);
    for (const auto& gate : backbone.tail.gates)
        appendUniqueGate(out.gates, gate);

    const double entryLength =
        entry.routeCurves.empty()
            ? 0.0
            : entry.routeCurves.back().endProgressMeters;

    out.routeFrameAnchors = entry.routeFrameAnchors;
    for (auto anchor : backbone.tail.routeFrameAnchors)
    {
        anchor.progressMeters += entryLength;
        out.routeFrameAnchors.push_back(anchor);
    }

    out.stages = entry.stages;
    for (auto stage : backbone.tail.stages)
    {
        stage.startProgressMeters += entryLength;
        stage.endProgressMeters += entryLength;
        out.stages.push_back(std::move(stage));
    }

    out.initialTurnPresent = entry.initialTurnPresent;
    out.initialTurnRadiusMeters = entry.initialTurnRadiusMeters;

    recomputeGlobalSpeeds(out, liveRequest);

    const auto stress =
        executionStress(out.executionGates, liveRequest);

    out.diagnosticSummary =
        "late-entry rebound entry_m=" +
        std::to_string(entryLength) +
        " tail_m=" +
        std::to_string(
            backbone.tail.routeCurves.empty()
                ? 0.0
                : backbone.tail.routeCurves.back().endProgressMeters
        ) +
        " handoff_speed_mps=" +
        std::to_string(entryRequest.terminalSpeedMps) +
        " approach_m=" +
        std::to_string(entryRequest.terminalApproachLengthMeters) +
        " final_min_radius_m=" +
        std::to_string(
            minimumSampledRadiusMeters(out.routeCurves)
        ) +
        " final_max_join_dk=" +
        std::to_string(
            maximumJoinCurvatureJump(out.routeCurves)
        ) +
        " final_peak_lat_util=" +
        std::to_string(stress.maxLateralUtilization) +
        " final_peak_omega_util=" +
        std::to_string(stress.maxAngularVelocityUtilization) +
        " final_peak_alpha_util=" +
        std::to_string(stress.maxAngularAccelerationUtilization) +
        " | entry={" +
        entry.diagnosticSummary +
        "} | tail={" +
        backbone.tail.diagnosticSummary +
        "}";

    if (out.routeCurves.empty() ||
        out.executionGates.size() < 2 ||
        out.gates.size() < 2)
    {
        out.disposition = RoutePlanDisposition::NeedsRefinement;
        out.failureCode = RoutePlanFailureCode::BackendFailure;
        out.failure = "late entry splice produced incomplete route";
        out.userMessage = out.failure;
    }

    return out;
}

BallisticPrefixStatus RouteEntryBinder::inspectBallisticPrefix(
    const RoutePlan& plan,
    const glm::dvec3& livePositionMeters,
    double allowedCrossTrackMeters
) noexcept
{
    BallisticPrefixStatus out;

    if (!plan.valid() ||
        plan.routeCurves.empty() ||
        plan.routeCurves.front().kind != RouteCurveKind::Line)
    {
        return out;
    }

    const auto& line = plan.routeCurves.front();
    const glm::dvec3 delta = line.endMeters - line.startMeters;
    const double length = glm::length(delta);
    if (!(std::isfinite(length) && length > 1.0e-6))
        return out;

    const glm::dvec3 forward = delta / length;
    const glm::dvec3 relative =
        livePositionMeters - line.startMeters;
    const double along = glm::dot(relative, forward);
    const glm::dvec3 cross =
        relative - forward * along;
    const double crossTrack = glm::length(cross);
    const double allowed =
        std::max(0.0, allowedCrossTrackMeters);

    out.alongMeters = along;
    out.crossTrackMeters = crossTrack;
    out.prefixLengthMeters = length;
    out.remainingMeters = length - along;

    if (crossTrack > allowed)
    {
        out.disposition = BallisticPrefixDisposition::OffAxis;
        return out;
    }

    if (along < 0.0)
    {
        out.disposition = BallisticPrefixDisposition::BeforeStart;
        return out;
    }

    if (along <= length)
    {
        out.disposition = BallisticPrefixDisposition::OnPrefix;
        out.remainingMeters = std::max(0.0, out.remainingMeters);
        return out;
    }

    out.disposition = BallisticPrefixDisposition::PastPrefix;
    out.remainingMeters = 0.0;
    return out;
}

} // namespace game::navigation::planner
