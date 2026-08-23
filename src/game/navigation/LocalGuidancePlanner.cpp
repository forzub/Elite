#include "src/game/navigation/LocalGuidancePlanner.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>
#include <utility>

namespace game::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;

bool finiteVec(const glm::dvec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool validRequest(const LocalGuidanceRequest& request)
{
    return request.systemId >= 0 &&
        request.target.enabled &&
        request.target.systemId == request.systemId &&
        request.profile.horizonSeconds > 0.0 &&
        request.profile.frameIntervalSeconds > 0.0 &&
        request.profile.predictorIntegrationStepSeconds > 0.0 &&
        finiteVec(request.actorState.positionMeters) &&
        finiteVec(request.actorState.velocityMps) &&
        finiteVec(request.actorProperAccelerationMps2) &&
        finiteVec(request.target.positionMeters) &&
        finiteVec(request.target.velocityMps) &&
        finiteVec(request.target.angularVelocityWorldRadPerSecond);
}

glm::dquat orientationAt(
    const ResolvedHubSemanticAnchor& target,
    double universeTimeSeconds
)
{
    const double dt = universeTimeSeconds - target.epochUniverseTimeSeconds;
    const glm::dvec3 omega = target.angularVelocityWorldRadPerSecond;
    const double speed = glm::length(omega);
    if (speed <= Epsilon || std::abs(dt) <= Epsilon)
        return glm::normalize(target.orientation);

    const glm::dquat delta = glm::angleAxis(speed * dt, omega / speed);
    return glm::normalize(delta * target.orientation);
}

glm::dvec3 positionAt(
    const ResolvedHubSemanticAnchor& target,
    double universeTimeSeconds
)
{
    const double dt = universeTimeSeconds - target.epochUniverseTimeSeconds;
    return target.positionMeters + target.velocityMps * dt;
}

/*
    Two equal-duration constant-acceleration legs that match position and
    terminal velocity in the no-gravity/no-envelope ideal case. Predictor then
    applies real gravity plus acceleration/jerk envelopes. This is intentionally
    a deterministic first candidate, not the final route solver.
*/
void makeTwoLegAccelerationProgram(
    const LocalGuidanceRequest& request,
    const glm::dvec3& targetPosition,
    const glm::dvec3& targetVelocity,
    TrajectoryPredictionRequest& out
)
{
    const double total = request.profile.horizonSeconds;
    const double half = total * 0.5;
    const glm::dvec3 displacement =
        targetPosition - request.actorState.positionMeters;

    const glm::dvec3 velocityDelta =
        targetVelocity - request.actorState.velocityMps;
    const glm::dvec3 D = velocityDelta / half;
    const glm::dvec3 S =
        displacement - request.actorState.velocityMps * total;

    const glm::dvec3 first = S / (half * half) - 0.5 * D;
    const glm::dvec3 second = D - first;

    const double switchBlend = std::min(0.05, total * 0.01);
    const double switchBefore = std::max(0.0, half - switchBlend);
    const double endBefore = std::max(half, total - switchBlend);

    out.initialProperAccelerationMps2 = request.actorProperAccelerationMps2;
    out.accelerationProgram = {
        {0.0, first},
        {switchBefore, first},
        {half, second},
        {endBefore, second},
        {total, glm::dvec3(0.0)}
    };
}

TrajectoryPredictionResult predictLeg(
    const LocalGuidanceRequest& request,
    const WorldKinematicState& initialState,
    const glm::dvec3& initialProperAccelerationMps2,
    double startUniverseTimeSeconds,
    double durationSeconds,
    const glm::dvec3& targetPosition,
    const glm::dvec3& targetVelocity
)
{
    LocalGuidanceRequest leg = request;
    leg.startUniverseTimeSeconds = startUniverseTimeSeconds;
    leg.actorState = initialState;
    leg.actorProperAccelerationMps2 = initialProperAccelerationMps2;
    leg.profile.horizonSeconds = durationSeconds;

    TrajectoryPredictionRequest predictionRequest;
    predictionRequest.systemId = request.systemId;
    predictionRequest.startUniverseTimeSeconds = startUniverseTimeSeconds;
    predictionRequest.initialState = initialState;
    predictionRequest.gravityBodies = request.environment.gravityBodies;
    predictionRequest.motionEnvelope = request.profile.motionEnvelope;
    predictionRequest.horizonSeconds = durationSeconds;
    predictionRequest.sampleIntervalSeconds = std::min(
        request.profile.frameIntervalSeconds,
        durationSeconds
    );
    predictionRequest.maxIntegrationStepSeconds =
        request.profile.predictorIntegrationStepSeconds;

    makeTwoLegAccelerationProgram(
        leg,
        targetPosition,
        targetVelocity,
        predictionRequest
    );
    return TrajectoryPredictor::predict(predictionRequest);
}

TrajectoryPredictionResult concatenatePredictions(
    const TrajectoryPredictionResult& first,
    const TrajectoryPredictionResult& second
)
{
    TrajectoryPredictionResult out;
    if (!first.ok() || !second.ok() || first.samples.empty() || second.samples.empty())
    {
        out.status = TrajectoryPredictionStatus::NumericalFailure;
        out.message = "cannot concatenate failed local guidance legs";
        return out;
    }

    out.status = TrajectoryPredictionStatus::Ok;
    out.systemId = first.systemId;
    out.message = "multi-leg local guidance prediction";
    out.samples.reserve(first.samples.size() + second.samples.size() - 1);
    out.samples.insert(out.samples.end(), first.samples.begin(), first.samples.end());
    const double offsetBase = first.samples.back().timeOffsetSeconds;
    const double deltaVBase = first.samples.back().cumulativeProperDeltaVMps;
    for (auto it = second.samples.begin() + 1; it != second.samples.end(); ++it)
    {
        auto sample = *it;
        sample.timeOffsetSeconds += offsetBase;
        sample.cumulativeProperDeltaVMps += deltaVBase;
        out.samples.push_back(std::move(sample));
    }

    out.diagnostics.integrationSteps =
        first.diagnostics.integrationSteps + second.diagnostics.integrationSteps;
    out.diagnostics.maxSpeedMps = std::max(
        first.diagnostics.maxSpeedMps,
        second.diagnostics.maxSpeedMps
    );
    out.diagnostics.maxRequestedProperAccelerationMps2 = std::max(
        first.diagnostics.maxRequestedProperAccelerationMps2,
        second.diagnostics.maxRequestedProperAccelerationMps2
    );
    out.diagnostics.maxAppliedProperAccelerationMps2 = std::max(
        first.diagnostics.maxAppliedProperAccelerationMps2,
        second.diagnostics.maxAppliedProperAccelerationMps2
    );
    out.diagnostics.maxProperLoadGs = std::max(
        first.diagnostics.maxProperLoadGs,
        second.diagnostics.maxProperLoadGs
    );
    out.diagnostics.maxAppliedProperJerkMps3 = std::max(
        first.diagnostics.maxAppliedProperJerkMps3,
        second.diagnostics.maxAppliedProperJerkMps3
    );
    out.diagnostics.totalProperDeltaVMps =
        first.diagnostics.totalProperDeltaVMps +
        second.diagnostics.totalProperDeltaVMps;
    out.diagnostics.travelledDistanceMeters =
        first.diagnostics.travelledDistanceMeters +
        second.diagnostics.travelledDistanceMeters;
    out.diagnostics.accelerationClamped =
        first.diagnostics.accelerationClamped || second.diagnostics.accelerationClamped;
    out.diagnostics.jerkClamped =
        first.diagnostics.jerkClamped || second.diagnostics.jerkClamped;
    return out;
}

bool trySimpleLateralDetour(
    const LocalGuidanceRequest& request,
    const TrajectorySafetyReport& directSafety,
    const glm::dvec3& targetEndPosition,
    const glm::dvec3& targetEndVelocity,
    TrajectoryPredictionResult& outPrediction,
    TrajectorySafetyReport& outSafety
)
{
    if (directSafety.conflicts.empty())
        return false;

    const TrajectoryConflict& conflict = directSafety.conflicts.front();
    const double total = request.profile.horizonSeconds;
    const double conflictOffset =
        conflict.universeTimeSeconds - request.startUniverseTimeSeconds;
    const double firstDuration = std::clamp(
        conflictOffset,
        total * 0.30,
        total * 0.65
    );
    const double secondDuration = total - firstDuration;
    if (firstDuration <= 0.5 || secondDuration <= 0.5)
        return false;

    glm::dvec3 pathDirection =
        targetEndPosition - request.actorState.positionMeters;
    if (glm::length(pathDirection) <= Epsilon)
        return false;
    pathDirection = glm::normalize(pathDirection);

    glm::dvec3 lateral =
        conflict.shipPositionMeters - conflict.hazardPositionMeters;
    lateral -= pathDirection * glm::dot(lateral, pathDirection);

    if (glm::length(lateral) <= Epsilon)
    {
        const glm::dquat pose = orientationAt(
            request.target,
            conflict.universeTimeSeconds
        );
        glm::dvec3 up = pose * glm::dvec3(0.0, 1.0, 0.0);
        lateral = glm::cross(pathDirection, up);
    }
    if (glm::length(lateral) <= Epsilon)
        lateral = glm::cross(pathDirection, glm::dvec3(0.0, 0.0, 1.0));
    if (glm::length(lateral) <= Epsilon)
        lateral = glm::cross(pathDirection, glm::dvec3(0.0, 1.0, 0.0));
    if (glm::length(lateral) <= Epsilon)
        return false;
    lateral = glm::normalize(lateral);

    const double detourClearance = std::max(
        25.0,
        conflict.requiredSeparationMeters * 1.35 + 20.0
    );

    for (double sign : {1.0, -1.0})
    {
        const glm::dvec3 detourPoint =
            conflict.hazardPositionMeters + lateral * (detourClearance * sign);
        glm::dvec3 secondDirection = targetEndPosition - detourPoint;
        if (glm::length(secondDirection) <= Epsilon)
            secondDirection = pathDirection;
        else
            secondDirection = glm::normalize(secondDirection);

        const double secondDistance = glm::length(targetEndPosition - detourPoint);
        const glm::dvec3 detourVelocity =
            secondDirection * (secondDistance / secondDuration);

        const auto first = predictLeg(
            request,
            request.actorState,
            request.actorProperAccelerationMps2,
            request.startUniverseTimeSeconds,
            firstDuration,
            detourPoint,
            detourVelocity
        );
        if (!first.ok() || first.samples.empty())
            continue;

        const auto& join = first.samples.back();
        const auto second = predictLeg(
            request,
            join.state,
            join.properAccelerationMps2,
            join.universeTimeSeconds,
            secondDuration,
            targetEndPosition,
            targetEndVelocity
        );
        if (!second.ok() || second.samples.empty())
            continue;

        auto combined = concatenatePredictions(first, second);
        if (!combined.ok())
            continue;

        auto safety = TrajectorySafetyEvaluator::evaluate(
            combined,
            request.environment,
            request.profile.shipSafetyRadiusMeters
        );
        if (!safety.safe)
            continue;

        outPrediction = std::move(combined);
        outSafety = std::move(safety);
        return true;
    }

    return false;
}

glm::dquat corridorOrientation(
    const glm::dvec3& pathTangent,
    const glm::dquat& targetOrientation,
    double targetBlend
)
{
    glm::dvec3 forward = pathTangent;
    if (glm::length(forward) <= Epsilon)
        forward = targetOrientation * glm::dvec3(0.0, 0.0, -1.0);
    forward = glm::normalize(forward);

    glm::dvec3 targetUp = targetOrientation * glm::dvec3(0.0, 1.0, 0.0);
    glm::dvec3 fallbackUp(0.0, 1.0, 0.0);
    glm::dvec3 up = glm::mix(fallbackUp, targetUp, std::clamp(targetBlend, 0.0, 1.0));
    if (glm::length(glm::cross(forward, up)) <= Epsilon)
        up = glm::dvec3(1.0, 0.0, 0.0);

    glm::dvec3 right = glm::normalize(glm::cross(forward, up));
    up = glm::normalize(glm::cross(right, forward));
    const glm::dmat3 basis(right, up, -forward);
    return glm::normalize(glm::quat_cast(basis));
}

} // namespace

LocalGuidanceResult LocalGuidancePlanner::plan(
    const LocalGuidanceRequest& request
)
{
    LocalGuidanceResult out;
    if (!validRequest(request))
    {
        out.message = "invalid local guidance request";
        return out;
    }

    const double endTime =
        request.startUniverseTimeSeconds + request.profile.horizonSeconds;
    const glm::dvec3 targetEndPosition = positionAt(request.target, endTime);
    const glm::dvec3 targetEndVelocity = request.target.velocityMps;

    out.prediction = predictLeg(
        request,
        request.actorState,
        request.actorProperAccelerationMps2,
        request.startUniverseTimeSeconds,
        request.profile.horizonSeconds,
        targetEndPosition,
        targetEndVelocity
    );
    if (!out.prediction.ok() || out.prediction.samples.empty())
    {
        out.status = LocalGuidanceStatus::PredictionFailure;
        out.message = out.prediction.message.empty()
            ? "trajectory prediction failed"
            : out.prediction.message;
        return out;
    }

    out.safety = TrajectorySafetyEvaluator::evaluate(
        out.prediction,
        request.environment,
        request.profile.shipSafetyRadiusMeters
    );

    if (!out.safety.safe)
    {
        TrajectoryPredictionResult detourPrediction;
        TrajectorySafetyReport detourSafety;
        if (trySimpleLateralDetour(
                request,
                out.safety,
                targetEndPosition,
                targetEndVelocity,
                detourPrediction,
                detourSafety))
        {
            out.prediction = std::move(detourPrediction);
            out.safety = std::move(detourSafety);
            out.detourUsed = true;
        }
    }

    GuidanceCorridor corridor;
    corridor.id = request.corridorId.empty()
        ? ("local:" + request.target.hubModuleId + ":" + request.target.id)
        : request.corridorId;
    corridor.systemId = request.systemId;
    corridor.source = GuidanceSource::LocalPlanner;
    corridor.purpose = request.profile.purpose;
    corridor.generatedAtUniverseTimeSeconds =
        request.startUniverseTimeSeconds;
    corridor.validUntilUniverseTimeSeconds = endTime;
    corridor.confidence = out.safety.safe ? 1.0 : 0.35;
    corridor.priority = 50;
    corridor.advisoryOnly = true;

    const double anchorWidth = std::max(
        request.target.extentMeters.x,
        request.target.extentMeters.y
    );
    const double defaultSize = std::max(
        20.0,
        anchorWidth + 2.0 * std::max(
            0.0,
            request.target.requiredClearanceMeters
        )
    );
    const double baseWidth = request.profile.corridorWidthMeters > 0.0
        ? request.profile.corridorWidthMeters
        : defaultSize;
    const double baseHeight = request.profile.corridorHeightMeters > 0.0
        ? request.profile.corridorHeightMeters
        : defaultSize;

    corridor.frames.reserve(out.prediction.samples.size());
    for (std::size_t i = 0; i < out.prediction.samples.size(); ++i)
    {
        const auto& sample = out.prediction.samples[i];
        const double u = request.profile.horizonSeconds > Epsilon
            ? std::clamp(
                sample.timeOffsetSeconds / request.profile.horizonSeconds,
                0.0,
                1.0
              )
            : 1.0;

        glm::dvec3 tangent = sample.state.velocityMps;
        if (i + 1 < out.prediction.samples.size())
        {
            tangent =
                out.prediction.samples[i + 1].state.positionMeters -
                sample.state.positionMeters;
        }

        const glm::dquat targetPose = orientationAt(
            request.target,
            sample.universeTimeSeconds
        );

        GuidanceFrame frame;
        frame.universeTimeSeconds = sample.universeTimeSeconds;
        frame.centerMeters = sample.state.positionMeters;
        frame.orientation = corridorOrientation(
            tangent,
            targetPose,
            u * u
        );

        // Broad early frames become tighter near the semantic gate. This is a
        // presentation/safety corridor, independent from mesh geometry.
        const double broadening = 1.0 + (1.0 - u) * 0.75;
        frame.widthMeters = baseWidth * broadening;
        frame.heightMeters = baseHeight * broadening;
        frame.lateralToleranceMeters = frame.widthMeters * 0.45;
        frame.verticalToleranceMeters = frame.heightMeters * 0.45;
        frame.recommendedSpeedMps =
            request.profile.recommendedSpeedMps > 0.0
                ? request.profile.recommendedSpeedMps
                : request.target.maxEntrySpeedMps;
        frame.maxClosureRateMps = request.profile.maxClosureRateMps > 0.0
            ? request.profile.maxClosureRateMps
            : request.target.maxEntrySpeedMps;
        corridor.frames.push_back(std::move(frame));
    }

    // Anchor the final visual gate to the predicted moving semantic element.
    // The physical prediction remains untouched and available for diagnostics.
    if (!corridor.frames.empty())
    {
        auto& last = corridor.frames.back();
        last.centerMeters = targetEndPosition;
        last.orientation = orientationAt(request.target, endTime);
        last.widthMeters = baseWidth;
        last.heightMeters = baseHeight;
    }

    out.corridor = std::move(corridor);
    out.status = out.safety.safe
        ? LocalGuidanceStatus::Ready
        : LocalGuidanceStatus::Blocked;
    out.message = out.safety.safe
        ? (out.detourUsed
            ? "local guidance detour candidate ready"
            : "direct local guidance candidate ready")
        : "local guidance candidates conflict with known hazard";
    return out;
}

} // namespace game::navigation
