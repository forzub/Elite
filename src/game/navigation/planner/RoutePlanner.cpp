#include "src/game/navigation/planner/RoutePlannerApi.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

#include "src/game/navigation/DockingAdvisoryPlanner.h"
#include "src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.h"

namespace game::navigation::planner
{

const char* routePlanFailureMessage(
    RoutePlanFailureCode code
) noexcept
{
    switch (code)
    {
        case RoutePlanFailureCode::None:
            return "Route is ready.";
        case RoutePlanFailureCode::InvalidRequest:
            return "Navigation request is invalid.";
        case RoutePlanFailureCode::InvalidWorldGeometry:
            return "Navigation world geometry is invalid or incomplete.";
        case RoutePlanFailureCode::HullDoesNotFit:
            return "The ship and required clearance do not fit through any available passage.";
        case RoutePlanFailureCode::GoalGeometricallyIsolated:
            return "The destination is geometrically isolated from the ship by impassable obstacles.";
        case RoutePlanFailureCode::UnavoidableCollision:
            return "Given the current state and hard vehicle limits, every reachable trajectory collides.";
        case RoutePlanFailureCode::PropulsionInsufficient:
            return "Available propulsion or control authority is insufficient to reach a safe route.";
        case RoutePlanFailureCode::NoCollisionFreeCandidate:
            return "The current route candidate is blocked; search another sector, radius, lead length or approach geometry.";
        case RoutePlanFailureCode::DynamicWindowUnavailable:
            return "No safe dynamic passage is available now; wait for a valid movement window and replan.";
        case RoutePlanFailureCode::BackendFailure:
        default:
            return "The current route candidate is not acceptable; refine route geometry or speed limits and try again.";
    }
}

namespace
{

RouteGate adaptGate(const game::navigation::DockingAdvisoryGate& gate)
{
    RouteGate out;
    out.positionMeters = gate.positionMeters;
    out.forward = gate.forward;
    out.speedMps = gate.speedMps;
    return out;
}


glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length = glm::length(value);
    return std::isfinite(length) && length > 1.0e-9
        ? value / length
        : fallback;
}

double finalizeCurveProgress(
    RouteCurveSegment& curve,
    double startProgressMeters
)
{
    curve.startProgressMeters = startProgressMeters;

    if (curve.kind == RouteCurveKind::Line)
    {
        const double length =
            glm::length(curve.endMeters - curve.startMeters);
        curve.endProgressMeters =
            startProgressMeters + std::max(0.0, length);
        curve.arcLengthKnots.clear();
        return std::max(0.0, length);
    }

    constexpr std::size_t Samples = 96;
    curve.arcLengthKnots.clear();
    curve.arcLengthKnots.reserve(Samples + 1);

    double localProgress = 0.0;
    glm::dvec3 previous = curve.positionAtParameter(0.0);

    for (std::size_t i = 0; i <= Samples; ++i)
    {
        const double t =
            static_cast<double>(i) /
            static_cast<double>(Samples);
        const glm::dvec3 point =
            curve.positionAtParameter(t);

        if (i > 0)
            localProgress += glm::length(point - previous);

        RouteCurveArcLengthKnot knot;
        knot.parameter01 = t;
        knot.localProgressMeters = localProgress;
        curve.arcLengthKnots.push_back(knot);
        previous = point;
    }

    curve.endProgressMeters =
        startProgressMeters + localProgress;
    return localProgress;
}

void appendLineCurve(
    std::vector<RouteCurveSegment>& curves,
    const glm::dvec3& start,
    const glm::dvec3& end,
    double maxSpeedMps,
    double& progressMeters
)
{
    const glm::dvec3 delta = end - start;
    const double length = glm::length(delta);
    if (!(std::isfinite(length) && length > 1.0e-6))
        return;

    RouteCurveSegment curve;
    curve.kind = RouteCurveKind::Line;
    curve.maxSpeedMps = maxSpeedMps;
    curve.startMeters = start;
    curve.endMeters = end;
    curve.startForward = delta / length;
    curve.endForward = curve.startForward;
    finalizeCurveProgress(curve, progressMeters);
    progressMeters = curve.endProgressMeters;
    curves.push_back(std::move(curve));
}

void appendBezierCurve(
    std::vector<RouteCurveSegment>& curves,
    const glm::dvec3& p0,
    const glm::dvec3& p1,
    const glm::dvec3& p2,
    const glm::dvec3& p3,
    double maxSpeedMps,
    double& progressMeters
)
{
    RouteCurveSegment curve;
    curve.kind = RouteCurveKind::CubicBezier;
    curve.maxSpeedMps = maxSpeedMps;
    curve.startMeters = p0;
    curve.bezierControl1Meters = p1;
    curve.bezierControl2Meters = p2;
    curve.endMeters = p3;
    curve.startForward =
        normalizedOr(p1 - p0, normalizedOr(p3 - p0, {0.0,0.0,-1.0}));
    curve.endForward =
        normalizedOr(p3 - p2, curve.startForward);
    finalizeCurveProgress(curve, progressMeters);
    progressMeters = curve.endProgressMeters;
    curves.push_back(std::move(curve));
}

struct CurveSample
{
    glm::dvec3 position {0.0};
    glm::dvec3 tangent {0.0, 0.0, -1.0};
    double curvaturePerMeter = 0.0;
};

CurveSample sampleCurves(
    const std::vector<RouteCurveSegment>& curves,
    double progressMeters
)
{
    CurveSample out;
    if (curves.empty())
        return out;

    const RouteCurveSegment* selected = &curves.back();
    for (const auto& curve : curves)
    {
        if (progressMeters <= curve.endProgressMeters + 1.0e-9)
        {
            selected = &curve;
            break;
        }
    }

    const double clamped =
        std::clamp(
            progressMeters,
            selected->startProgressMeters,
            selected->endProgressMeters
        );
    const double t = selected->parameterAtProgress(clamped);
    out.position = selected->positionAtParameter(t);
    out.tangent = selected->tangentAtProgress(clamped);
    out.curvaturePerMeter =
        selected->curvatureAtProgress(clamped);
    return out;
}

double speedLimitForCurvature(
    const RoutePlanRequest& request,
    double curvaturePerMeter
)
{
    double limit = std::max(0.0, request.maxSpeedMps);
    if (curvaturePerMeter > 1.0e-12 &&
        request.lateralMps2 > 0.0)
    {
        limit =
            std::min(
                limit,
                std::sqrt(
                    request.lateralMps2 /
                    curvaturePerMeter
                )
            );
    }

    if (curvaturePerMeter > 1.0e-12 &&
        request.maxAngularVelocityRadPerSecond > 0.0)
    {
        limit =
            std::min(
                limit,
                request.maxAngularVelocityRadPerSecond /
                    curvaturePerMeter
            );
    }

    return std::max(0.0, limit);
}

void appendGateAtProgress(
    std::vector<RouteGate>& gates,
    const std::vector<RouteCurveSegment>& curves,
    const RoutePlanRequest& request,
    double progressMeters,
    bool terminal
)
{
    const auto sample =
        sampleCurves(curves, progressMeters);

    RouteGate gate;
    gate.positionMeters = sample.position;
    gate.forward = sample.tangent;
    gate.speedMps =
        terminal
            ? std::clamp(
                  request.terminalSpeedMps,
                  0.0,
                  speedLimitForCurvature(
                      request,
                      sample.curvaturePerMeter
                  )
              )
            : speedLimitForCurvature(
                  request,
                  sample.curvaturePerMeter
              );

    if (!gates.empty() &&
        glm::length(
            gates.back().positionMeters -
            gate.positionMeters
        ) <= 1.0e-6)
    {
        gates.back() = gate;
        return;
    }

    gates.push_back(gate);
}

std::vector<RouteGate> buildPresentationGates(
    const std::vector<RouteCurveSegment>& curves,
    const RoutePlanRequest& request
)
{
    std::vector<RouteGate> gates;
    if (curves.empty())
        return gates;

    const double total = curves.back().endProgressMeters;
    appendGateAtProgress(
        gates,
        curves,
        request,
        0.0,
        total <= 1.0e-9
    );

    double progress = 0.0;
    while (progress < total - 1.0e-9)
    {
        const double remaining = total - progress;
        const bool terminalDense =
            request.terminalDenseDistanceMeters > 0.0 &&
            remaining <= request.terminalDenseDistanceMeters;

        const double requestedSpacing =
            terminalDense
                ? request.terminalGateSpacingMeters
                : request.gateSpacingMeters;
        const double spacing =
            std::max(1.0, requestedSpacing);

        progress = std::min(total, progress + spacing);
        appendGateAtProgress(
            gates,
            curves,
            request,
            progress,
            progress >= total - 1.0e-9
        );
    }

    return gates;
}

std::vector<RouteGate> buildExecutionGates(
    const std::vector<RouteCurveSegment>& curves,
    const RoutePlanRequest& request
)
{
    std::vector<RouteGate> gates;
    if (curves.empty())
        return gates;

    appendGateAtProgress(
        gates,
        curves,
        request,
        curves.front().startProgressMeters,
        false
    );

    for (const auto& curve : curves)
    {
        if (curve.kind == RouteCurveKind::CubicBezier &&
            curve.arcLengthKnots.size() >= 2)
        {
            for (std::size_t i = 1;
                 i < curve.arcLengthKnots.size();
                 ++i)
            {
                const double progress =
                    curve.startProgressMeters +
                    curve.arcLengthKnots[i].localProgressMeters;
                appendGateAtProgress(
                    gates,
                    curves,
                    request,
                    progress,
                    false
                );
            }
            continue;
        }

        const double length =
            curve.endProgressMeters -
            curve.startProgressMeters;
        const double spacing =
            std::max(
                1.0,
                request.terminalGateSpacingMeters > 0.0
                    ? request.terminalGateSpacingMeters
                    : request.gateSpacingMeters
            );
        const std::size_t pieces =
            std::max<std::size_t>(
                1,
                static_cast<std::size_t>(
                    std::ceil(length / spacing)
                )
            );
        for (std::size_t i = 1; i <= pieces; ++i)
        {
            const double progress =
                curve.startProgressMeters +
                length *
                    static_cast<double>(i) /
                    static_cast<double>(pieces);
            appendGateAtProgress(
                gates,
                curves,
                request,
                progress,
                false
            );
        }
    }

    if (!gates.empty())
    {
        const auto terminalSample =
            sampleCurves(
                curves,
                curves.back().endProgressMeters
            );
        gates.back().speedMps =
            std::clamp(
                request.terminalSpeedMps,
                0.0,
                speedLimitForCurvature(
                    request,
                    terminalSample.curvaturePerMeter
                )
            );
    }
    return gates;
}

RoutePlan planContinuousAssisted(
    const RoutePlanRequest& input
)
{
    using assisted::lab::ContinuousAssistedPathOptimizer;

    RoutePlan out;

    assisted::lab::Request request;
    request.start.positionMeters = input.startMeters;
    request.start.tangent =
        input.hasInitialForward
            ? input.initialForward
            : normalizedOr(
                  input.goalMeters - input.startMeters,
                  {0.0, 0.0, -1.0}
              );
    request.start.speedMps =
        std::max(0.0, input.initialSpeedMps);

    request.goal.positionMeters = input.goalMeters;
    request.goal.tangent =
        normalizedOr(
            -input.terminalOutward,
            normalizedOr(
                input.goalMeters - input.startMeters,
                request.start.tangent
            )
        );
    request.goal.speedMps =
        std::clamp(
            input.terminalSpeedMps,
            0.0,
            std::max(0.0, input.maxSpeedMps)
        );

    request.hullRadiusMeters =
        std::max(0.0, input.agentRadiusMeters);
    request.designSpeedMps =
        std::max(0.0, input.maxSpeedMps);
    request.acceleratingMps2 =
        std::max(0.0, input.acceleratingMps2);
    request.brakingMps2 =
        std::max(0.0, input.brakingMps2);
    request.maxLateralAccelerationMps2 =
        input.lateralMps2;
    request.maxAngularVelocityRadPerSecond =
        std::max(
            0.0,
            input.maxAngularVelocityRadPerSecond
        );
    request.maxAngularAccelerationRadPerSecond2 =
        std::max(
            0.0,
            input.maxAngularAccelerationRadPerSecond2
        );

    // Ordinary Assisted geometry is allowed to tighten and therefore lower
    // the local speed. maxSpeed is a cruise ceiling, not a global turn-radius
    // command. A non-zero comfort radius would still remain a true hard floor.
    request.minimumComfortRadiusMeters = 0.0;

    // HOT-REPLAN CONTRACT: an already moving craft continues
    // ballistically along its measured course before any curvature is allowed.
    // initialForwardLeadMeters is therefore authored geometry, not advisory
    // metadata. Optimize the curved path only from the END of this straight.
    request.protectedStartStraightMeters =
        std::max(
            0.0,
            std::max(
                input.initialProtectedStraightMeters,
                input.initialForwardLeadMeters
            )
        );
    request.protectedGoalStraightMeters =
        std::max(0.0, input.terminalApproachLengthMeters);
    request.obstacles = input.obstacles;

    // Production planning is asynchronous, but route validity is spatial:
    // elapsed wall-clock time does not expire the authored start. Keep the
    // optimizer budget bounded for runtime cost only; geometry remains
    // collision-sampled densely enough for the current obstacle representation.
    request.samplesPerBezier = 96;
    request.maxPatternIterations = 160;

    const auto optimized =
        ContinuousAssistedPathOptimizer::optimize(request);

    if (!optimized.valid)
    {
        out.disposition = RoutePlanDisposition::NeedsRefinement;
        out.failureCode =
            RoutePlanFailureCode::NoCollisionFreeCandidate;
        out.failure =
            "continuous Assisted optimizer failed: " +
            optimized.message;
        out.userMessage = out.failure;
        out.diagnosticSummary =
            "continuous-assisted invalid evals=" +
            std::to_string(
                optimized.diagnostics.objectiveEvaluations
            ) +
            " violation=" +
            std::to_string(
                optimized.diagnostics.maxConstraintViolation
            );
        return out;
    }

    const glm::dvec3 startTangent =
        normalizedOr(
            request.start.tangent,
            {0.0,0.0,-1.0}
        );
    const glm::dvec3 goalTangent =
        normalizedOr(
            request.goal.tangent,
            startTangent
        );

    const glm::dvec3 startCurve =
        input.startMeters +
        startTangent *
            request.protectedStartStraightMeters;
    const glm::dvec3 goalCurve =
        input.goalMeters -
        goalTangent *
            request.protectedGoalStraightMeters;

    double progress = 0.0;
    appendLineCurve(
        out.routeCurves,
        input.startMeters,
        startCurve,
        input.maxSpeedMps,
        progress
    );

    for (const auto& segment : optimized.segments)
    {
        if (segment.kind ==
            assisted::lab::OptimizedSegmentKind::Line)
        {
            appendLineCurve(
                out.routeCurves,
                segment.startMeters,
                segment.endMeters,
                input.maxSpeedMps,
                progress
            );
            continue;
        }

        appendBezierCurve(
            out.routeCurves,
            segment.startMeters,
            segment.control1Meters,
            segment.control2Meters,
            segment.endMeters,
            input.maxSpeedMps,
            progress
        );
    }

    appendLineCurve(
        out.routeCurves,
        goalCurve,
        input.goalMeters,
        input.maxSpeedMps,
        progress
    );

    if (out.routeCurves.empty())
    {
        out.disposition = RoutePlanDisposition::NeedsRefinement;
        out.failureCode = RoutePlanFailureCode::BackendFailure;
        out.failure =
            "continuous Assisted optimizer produced empty route geometry";
        out.userMessage = out.failure;
        out.diagnosticSummary = "continuous-assisted empty-geometry";
        return out;
    }

    out.gates =
        buildPresentationGates(out.routeCurves, input);
    out.executionGates =
        buildExecutionGates(out.routeCurves, input);

    if (out.gates.size() < 2 ||
        out.executionGates.size() < 2)
    {
        out.disposition = RoutePlanDisposition::NeedsRefinement;
        out.failureCode = RoutePlanFailureCode::BackendFailure;
        out.failure =
            "continuous Assisted route sampling produced too few gates";
        out.userMessage = out.failure;
        out.diagnosticSummary =
            "continuous-assisted insufficient-gates";
        return out;
    }

    out.disposition = RoutePlanDisposition::Ready;
    out.failureCode = RoutePlanFailureCode::None;
    out.failure.clear();
    out.userMessage = routePlanFailureMessage(out.failureCode);
    out.initialTurnPresent =
        optimized.diagnostics.maxCurvaturePerMeter >
            1.0e-12;

    const double representativeTurnRadius =
        std::isfinite(
            optimized.diagnostics.minimumRadiusMeters
        )
            ? optimized.diagnostics.minimumRadiusMeters
            : 0.0;

    out.initialTurnRadiusMeters =
        representativeTurnRadius;
    out.terminalTurnRadiusMeters =
        representativeTurnRadius;
    out.terminalTurnLateralRadiusMeters =
        representativeTurnRadius;
    out.terminalTurnSpeedMps = input.maxSpeedMps;
    out.terminalApproachLengthMeters =
        request.protectedGoalStraightMeters;

    out.diagnosticSummary =
        "continuous-assisted ready curves=" +
        std::to_string(out.routeCurves.size()) +
        " execution_gates=" +
        std::to_string(out.executionGates.size()) +
        " evals=" +
        std::to_string(
            optimized.diagnostics.objectiveEvaluations
        ) +
        " accepted_steps=" +
        std::to_string(
            optimized.diagnostics.acceptedSteps
        ) +
        " length_m=" +
        std::to_string(
            optimized.diagnostics.pathLengthMeters
        ) +
        " lower_bound_m=" +
        std::to_string(
            optimized.diagnostics.lowerBoundLengthMeters
        ) +
        " stretch=" +
        std::to_string(
            optimized.diagnostics.stretchRatio
        ) +
        " eta_s=" +
        std::to_string(
            optimized.diagnostics.estimatedTravelTimeSeconds
        ) +
        " terminal_speed_mps=" +
        std::to_string(
            request.goal.speedMps
        ) +
        " seeds=" +
        std::to_string(
            optimized.diagnostics.seedCount
        ) +
        " min_radius_m=" +
        std::to_string(
            optimized.diagnostics.minimumRadiusMeters
        ) +
        " peak_lat_util=" +
        std::to_string(
            optimized.diagnostics.maxLateralUtilization
        ) +
        " brake_energy=" +
        std::to_string(
            optimized.diagnostics.brakingDemandEnergy
        ) +
        " total_turn_rad=" +
        std::to_string(
            optimized.diagnostics.totalCurvatureRadians
        ) +
        " bend_energy=" +
        std::to_string(
            optimized.diagnostics.curvatureConcentrationEnergy
        ) +
        " peak_omega_util=" +
        std::to_string(
            optimized.diagnostics.maxAngularVelocityUtilization
        ) +
        " peak_alpha_util=" +
        std::to_string(
            optimized.diagnostics.maxAngularAccelerationUtilization
        ) +
        " objective=" +
        std::to_string(
            optimized.diagnostics.objective
        );

    return out;
}

} // namespace

RoutePlan RoutePlanner::plan(const RoutePlanRequest& input)
{
    if (input.useContinuousAssistedOptimizer &&
        input.roundTurns &&
        input.requiredViaPointsMeters.empty() &&
        input.mandatoryTangentStraights.empty())
    {
        return planContinuousAssisted(input);
    }

    game::navigation::DockingAdvisoryRequest legacy;
    legacy.startMeters = input.startMeters;
    legacy.outward = input.terminalOutward;
    legacy.standoffMeters = input.terminalReferenceDistanceMeters;

    // The current backend represents the generic endpoint as
    // entrance + normalized(outward) * standoff. Keep that implementation
    // detail private so callers only own the exact generic goal point.
    const double outwardLength = glm::length(input.terminalOutward);
    const glm::dvec3 terminalOutward =
        std::isfinite(outwardLength) && outwardLength > 1.0e-12
            ? input.terminalOutward / outwardLength
            : input.terminalOutward;
    legacy.entranceMeters =
        input.goalMeters -
        terminalOutward *
            input.terminalReferenceDistanceMeters;

    legacy.hullRadiusMeters = input.agentRadiusMeters;
    legacy.maxSpeedMps = input.maxSpeedMps;
    legacy.acceleratingMps2 = input.acceleratingMps2;
    legacy.brakingMps2 = input.brakingMps2;
    legacy.lateralMps2 = input.lateralMps2;
    legacy.maxAngularVelocityRadPerSecond =
        input.maxAngularVelocityRadPerSecond;
    legacy.maxAngularAccelerationRadPerSecond2 =
        input.maxAngularAccelerationRadPerSecond2;
    legacy.initialSpeedMps = input.initialSpeedMps;
    legacy.roundTurns = input.roundTurns;
    legacy.hasInitialForward = input.hasInitialForward;
    legacy.initialForward = input.initialForward;
    legacy.initialForwardLeadMeters = input.initialForwardLeadMeters;
    legacy.initialProtectedStraightMeters =
        input.initialProtectedStraightMeters;
    legacy.gateSpacingMeters = input.gateSpacingMeters;
    legacy.terminalGateSpacingMeters = input.terminalGateSpacingMeters;
    legacy.terminalDenseDistanceMeters = input.terminalDenseDistanceMeters;
    legacy.terminalApproachLengthMeters =
        input.terminalApproachLengthMeters;
    legacy.terminalTurnSegmentFraction =
        input.terminalTurnSegmentFraction;
    legacy.deriveTerminalTurnRadiusFromVehicle =
        input.deriveTerminalTurnRadiusFromVehicle;
    legacy.preferredTerminalTurnRadiusMeters =
        input.preferredTerminalTurnRadiusMeters;
    legacy.requiredViaPointsMeters =
        input.requiredViaPointsMeters;
    legacy.mandatoryTangentStraights =
        input.mandatoryTangentStraights;
    legacy.obstacles = input.obstacles;

    const auto planned =
        game::navigation::DockingAdvisoryPlanner::plan(legacy);

    RoutePlan out;
    out.failure = planned.failure;
    out.diagnosticSummary =
        game::navigation::dockingAdvisoryPlanDiagnosticSummary(planned);

    if (planned.valid())
    {
        out.disposition = RoutePlanDisposition::Ready;
        out.failureCode = RoutePlanFailureCode::None;
        out.userMessage = routePlanFailureMessage(out.failureCode);
    }
    else
    {
        const std::string failureLower = [&]()
        {
            std::string value = planned.failure;
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                }
            );
            return value;
        }();

        if (failureLower.find("invalid") != std::string::npos ||
            failureLower.find("nan") != std::string::npos ||
            failureLower.find("non-finite") != std::string::npos)
        {
            out.disposition = RoutePlanDisposition::InvalidWorldData;
            out.failureCode = RoutePlanFailureCode::InvalidWorldGeometry;
            out.userMessage =
                routePlanFailureMessage(out.failureCode);
        }
        else if (failureLower.find("blocked") != std::string::npos ||
                 failureLower.find("obstruct") != std::string::npos ||
                 failureLower.find("collision-free") != std::string::npos ||
                 failureLower.find("route") != std::string::npos)
        {
            // One blocked candidate is not proof that the route is impossible.
            // Ask the planner/orchestrator for a different sector, radius or
            // approach geometry first.
            out.disposition = RoutePlanDisposition::NeedsRefinement;
            out.failureCode = RoutePlanFailureCode::NoCollisionFreeCandidate;
            out.userMessage =
                routePlanFailureMessage(out.failureCode);
        }
        else
        {
            out.disposition = RoutePlanDisposition::NeedsRefinement;
            out.failureCode = RoutePlanFailureCode::BackendFailure;
            out.userMessage =
                routePlanFailureMessage(out.failureCode);
        }
    }

    out.gates.reserve(planned.gates.size());
    for (const auto& gate : planned.gates)
        out.gates.push_back(adaptGate(gate));

    out.executionGates.reserve(planned.executionGates.size());
    for (const auto& gate : planned.executionGates)
        out.executionGates.push_back(adaptGate(gate));

    out.routeCurves = planned.routeCurves;

    out.terminalDetourUsed = planned.terminalDetourUsed;
    out.terminalTurnRadiusRelaxed =
        planned.terminalTurnRadiusRelaxed;
    out.terminalTurnRequestedRadiusMeters =
        planned.terminalTurnRequestedRadiusMeters;
    out.terminalTurnSpeedMps = planned.terminalTurnSpeedMps;
    out.terminalTurnLateralRadiusMeters =
        planned.terminalTurnLateralRadiusMeters;
    out.terminalTurnAngularRadiusMeters =
        planned.terminalTurnAngularRadiusMeters;
    out.terminalTurnAngularRampMeters =
        planned.terminalTurnAngularRampMeters;
    out.terminalArcCandidatesTested =
        planned.terminalArcCandidatesTested;
    out.terminalArcRouteable = planned.terminalArcRouteable;
    out.terminalArcRouteRejected =
        planned.terminalArcRouteRejected;
    out.terminalArcTransitReady = planned.terminalArcTransitReady;
    out.terminalArcTransitRejected =
        planned.terminalArcTransitRejected;
    out.terminalArcCollisionRejected =
        planned.terminalArcCollisionRejected;
    out.terminalArcAcceptedCandidates =
        planned.terminalArcAcceptedCandidates;
    out.terminalArcAxisPassesTested =
        planned.terminalArcAxisPassesTested;
    out.terminalArcLastRejection =
        planned.terminalArcLastRejection;
    out.terminalArcDominantBlockerId =
        planned.terminalArcDominantBlockerId;
    out.terminalArcDominantBlockerHits =
        planned.terminalArcDominantBlockerHits;
    out.terminalArcDominantBlockerCenterMeters =
        planned.terminalArcDominantBlockerCenterMeters;
    out.terminalArcDominantBlockerHalfExtentsMeters =
        planned.terminalArcDominantBlockerHalfExtentsMeters;
    out.terminalArcDominantBlockerRadiusMeters =
        planned.terminalArcDominantBlockerRadiusMeters;
    out.terminalArcRotationDegrees =
        planned.terminalArcRotationDegrees;
    out.initialTurnPresent = planned.initialTurnPresent;
    out.initialTurnRadiusMeters = planned.initialTurnRadiusMeters;
    out.terminalApproachShortened =
        planned.terminalApproachShortened;
    out.terminalApproachExtended =
        planned.terminalApproachExtended;
    out.terminalApproachLengthMeters =
        planned.terminalApproachLengthMeters;
    out.terminalTurnRadiusMeters = planned.terminalTurnRadiusMeters;
    return out;
}

} // namespace game::navigation::planner
