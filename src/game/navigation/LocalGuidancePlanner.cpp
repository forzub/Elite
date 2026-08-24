#include "src/game/navigation/LocalGuidancePlanner.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

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

bool finiteQuat(const glm::dquat& q)
{
    return std::isfinite(q.w) && std::isfinite(q.x) &&
           std::isfinite(q.y) && std::isfinite(q.z) &&
           glm::length(q) > Epsilon;
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
        finiteQuat(request.actorOrientation) &&
        finiteVec(request.actorAngularVelocityWorldRadPerSecond) &&
        finiteVec(request.target.positionMeters) &&
        finiteVec(request.target.velocityMps) &&
        finiteVec(request.target.angularVelocityWorldRadPerSecond);
}

ResolvedHubSemanticAnchor anchorAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    const auto& samples = request.targetMotionSamples;
    if (samples.empty())
        return predictHubSemanticAnchorAt(request.target, universeTimeSeconds);

    if (universeTimeSeconds <= samples.front().epochUniverseTimeSeconds)
        return samples.front();
    if (universeTimeSeconds >= samples.back().epochUniverseTimeSeconds)
        return samples.back();

    const auto upper = std::upper_bound(
        samples.begin(),
        samples.end(),
        universeTimeSeconds,
        [](double time, const ResolvedHubSemanticAnchor& sample)
        {
            return time < sample.epochUniverseTimeSeconds;
        }
    );
    if (upper == samples.begin() || upper == samples.end())
        return request.target;

    const auto& b = *upper;
    const auto& a = *(upper - 1);
    const double span =
        b.epochUniverseTimeSeconds - a.epochUniverseTimeSeconds;
    if (span <= Epsilon)
        return b;

    const double t = std::clamp(
        (universeTimeSeconds - a.epochUniverseTimeSeconds) / span,
        0.0,
        1.0
    );

    ResolvedHubSemanticAnchor out = request.target;
    out.epochUniverseTimeSeconds = universeTimeSeconds;
    out.positionMeters = a.positionMeters +
        (b.positionMeters - a.positionMeters) * t;
    out.velocityMps = a.velocityMps +
        (b.velocityMps - a.velocityMps) * t;
    out.orientation = glm::normalize(glm::slerp(
        a.orientation,
        b.orientation,
        t
    ));
    out.angularVelocityWorldRadPerSecond =
        a.angularVelocityWorldRadPerSecond +
        (b.angularVelocityWorldRadPerSecond -
         a.angularVelocityWorldRadPerSecond) * t;
    out.hasRotationCenterKinematics = false;
    return out;
}

glm::dquat orientationAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    return anchorAt(request, universeTimeSeconds).orientation;
}

glm::dvec3 positionAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    return anchorAt(request, universeTimeSeconds).positionMeters;
}

glm::dvec3 velocityAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    return anchorAt(request, universeTimeSeconds).velocityMps;
}

glm::dvec3 outwardAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    return glm::normalize(
        orientationAt(request, universeTimeSeconds) *
        glm::dvec3(0.0, 0.0, -1.0)
    );
}

glm::dvec3 dockUpAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    glm::dvec3 up = glm::normalize(
        orientationAt(request, universeTimeSeconds) *
        glm::dvec3(0.0, 1.0, 0.0)
    );
    if (request.target.orientationPolicy == DockOrientationPolicy::Inverted)
        up = -up;
    return up;
}

glm::dquat orientationFromForwardUp(
    glm::dvec3 forward,
    glm::dvec3 up
)
{
    if (glm::length(forward) <= Epsilon)
        forward = glm::dvec3(0.0, 0.0, -1.0);
    forward = glm::normalize(forward);

    up -= forward * glm::dot(up, forward);
    if (glm::length(up) <= Epsilon)
    {
        up = std::abs(forward.y) < 0.9
            ? glm::dvec3(0.0, 1.0, 0.0)
            : glm::dvec3(1.0, 0.0, 0.0);
        up -= forward * glm::dot(up, forward);
    }
    up = glm::normalize(up);

    glm::dvec3 right = glm::cross(forward, up);
    if (glm::length(right) <= Epsilon)
        right = glm::dvec3(1.0, 0.0, 0.0);
    else
        right = glm::normalize(right);
    up = glm::normalize(glm::cross(right, forward));

    const glm::dmat3 basis(right, up, -forward);
    return glm::normalize(glm::quat_cast(basis));
}

glm::dquat actorOrientationAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    const double dt = universeTimeSeconds - request.startUniverseTimeSeconds;
    const glm::dvec3 omega = request.actorAngularVelocityWorldRadPerSecond;
    const double speed = glm::length(omega);
    if (speed <= Epsilon || std::abs(dt) <= Epsilon)
        return glm::normalize(request.actorOrientation);

    const glm::dquat delta = glm::angleAxis(speed * dt, omega / speed);
    return glm::normalize(delta * request.actorOrientation);
}

glm::dquat dockingOrientationAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    const glm::dvec3 inbound = -outwardAt(request, universeTimeSeconds);
    glm::dvec3 up = dockUpAt(request, universeTimeSeconds);

    if (request.target.orientationPolicy == DockOrientationPolicy::FreeRoll)
    {
        glm::dvec3 actorUp = glm::normalize(
            request.actorOrientation * glm::dvec3(0.0, 1.0, 0.0)
        );
        actorUp -= inbound * glm::dot(actorUp, inbound);
        if (glm::length(actorUp) > Epsilon)
            up = glm::normalize(actorUp);
    }

    return orientationFromForwardUp(inbound, up);
}

double effectiveShipSafetyRadius(const LocalGuidanceRequest& request)
{
    if (request.profile.vehicleEnvelope.valid)
        return request.profile.vehicleEnvelope.conservativeSafetyRadiusMeters();
    return std::max(0.0, request.profile.shipSafetyRadiusMeters);
}

double dockingStandoffMeters(const LocalGuidanceRequest& request)
{
    if (request.profile.dockingApproachStandoffMeters > 0.0)
        return request.profile.dockingApproachStandoffMeters;

    const auto& vehicle = request.profile.vehicleEnvelope;
    const double length = vehicle.valid ? vehicle.lengthMeters : 20.0;
    return std::max(
        30.0,
        length * 1.5 + std::max(0.0, request.target.requiredClearanceMeters)
    );
}

double dockingTerminalDepthMeters(const LocalGuidanceRequest& request)
{
    if (request.profile.dockingTerminalDepthMeters > 0.0)
        return request.profile.dockingTerminalDepthMeters;

    if (request.profile.vehicleEnvelope.valid)
    {
        return request.profile.vehicleEnvelope.terminalCenterDepthMeters(
            request.target.requiredClearanceMeters
        );
    }
    return std::max(5.0, request.target.requiredClearanceMeters);
}

double dockingEntrySpeedMps(const LocalGuidanceRequest& request)
{
    if (request.profile.recommendedSpeedMps > 0.0)
        return request.profile.recommendedSpeedMps;
    if (request.target.maxEntrySpeedMps > 0.0)
        return request.target.maxEntrySpeedMps;
    return 5.0;
}

glm::dvec3 dockingApproachPointAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    return positionAt(request, universeTimeSeconds) +
        outwardAt(request, universeTimeSeconds) *
        dockingStandoffMeters(request);
}

glm::dvec3 dockingTerminalPointAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    return positionAt(request, universeTimeSeconds) -
        outwardAt(request, universeTimeSeconds) *
        dockingTerminalDepthMeters(request);
}

glm::dvec3 dockingIngressVelocityAt(
    const LocalGuidanceRequest& request,
    double universeTimeSeconds
)
{
    return velocityAt(request, universeTimeSeconds) -
        outwardAt(request, universeTimeSeconds) *
        dockingEntrySpeedMps(request);
}

/*
    Two equal-duration constant-acceleration legs that match position and
    terminal velocity in the no-gravity/no-envelope ideal case. Predictor then
    applies real gravity plus acceleration/jerk envelopes. This remains a
    deterministic candidate generator, not an autopilot.
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
    // Solve the endpoint in a co-moving translational frame whose velocity is
    // the requested terminal velocity. Algebraically this is equivalent to
    // the old absolute formula, but it never subtracts two ~orbital-scale
    // world displacements to discover a kilometre-scale local manoeuvre.
    const glm::dvec3 targetLinearStart =
        targetPosition - targetVelocity * total;
    const glm::dvec3 relativePosition0 =
        request.actorState.positionMeters - targetLinearStart;
    const glm::dvec3 relativeVelocity0 =
        request.actorState.velocityMps - targetVelocity;

    const glm::dvec3 D = -relativeVelocity0 / half;
    const glm::dvec3 S =
        -relativePosition0 - relativeVelocity0 * total;

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

TrajectoryPredictionResult predictLegOnce(
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
    // Shooting correction closes the endpoint after real gravity and the
    // acceleration/jerk envelope have been applied by TrajectoryPredictor.
    // This is still only a candidate generator: it never moves the ship.
    glm::dvec3 commandPosition = targetPosition;
    glm::dvec3 commandVelocity = targetVelocity;

    TrajectoryPredictionResult best;
    double bestScore = std::numeric_limits<double>::infinity();

    constexpr int MaxIterations = 6;
    constexpr double PositionToleranceMeters = 0.25;
    constexpr double VelocityToleranceMps = 0.10;

    for (int iteration = 0; iteration < MaxIterations; ++iteration)
    {
        auto candidate = predictLegOnce(
            request,
            initialState,
            initialProperAccelerationMps2,
            startUniverseTimeSeconds,
            durationSeconds,
            commandPosition,
            commandVelocity
        );
        if (!candidate.ok() || candidate.samples.empty())
            return candidate;

        const auto& end = candidate.samples.back().state;
        const glm::dvec3 positionError = targetPosition - end.positionMeters;
        const glm::dvec3 velocityError = targetVelocity - end.velocityMps;
        const double positionErrorMeters = glm::length(positionError);
        const double velocityErrorMps = glm::length(velocityError);
        const double score = positionErrorMeters + velocityErrorMps * durationSeconds;

        if (score < bestScore)
        {
            bestScore = score;
            best = candidate;
        }

        if (positionErrorMeters <= PositionToleranceMeters &&
            velocityErrorMps <= VelocityToleranceMps)
        {
            return candidate;
        }

        // Endpoint response is close to linear for this short local candidate.
        // Bias the authored endpoint by the observed miss and let the shared
        // predictor apply gravity/envelopes again. If the envelope saturates,
        // the best physically achieved candidate is returned and terminal
        // validation below rejects it instead of snapping the corridor.
        commandPosition += positionError;
        commandVelocity += velocityError;
    }

    return best;
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

TrajectoryPredictionResult predictDockingCandidate(
    const LocalGuidanceRequest& request
)
{
    const double total = request.profile.horizonSeconds;
    const double ingressDuration = std::clamp(total * 0.30, 2.0, 6.0);
    const double approachDuration = total - ingressDuration;
    if (approachDuration <= 0.5)
    {
        TrajectoryPredictionResult failed;
        failed.status = TrajectoryPredictionStatus::InvalidRequest;
        failed.message = "docking horizon too short for approach + ingress";
        return failed;
    }

    const double approachTime =
        request.startUniverseTimeSeconds + approachDuration;
    const double endTime =
        request.startUniverseTimeSeconds + total;

    const auto first = predictLeg(
        request,
        request.actorState,
        request.actorProperAccelerationMps2,
        request.startUniverseTimeSeconds,
        approachDuration,
        dockingApproachPointAt(request, approachTime),
        dockingIngressVelocityAt(request, approachTime)
    );
    if (!first.ok() || first.samples.empty())
        return first;

    const auto& join = first.samples.back();
    const auto second = predictLeg(
        request,
        join.state,
        join.properAccelerationMps2,
        join.universeTimeSeconds,
        ingressDuration,
        dockingTerminalPointAt(request, endTime),
        dockingIngressVelocityAt(request, endTime)
    );
    if (!second.ok() || second.samples.empty())
        return second;

    return concatenatePredictions(first, second);
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
        glm::dvec3 up = request.actorOrientation * glm::dvec3(0.0, 1.0, 0.0);
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
            effectiveShipSafetyRadius(request)
        );
        if (!safety.safe)
            continue;

        outPrediction = std::move(combined);
        outSafety = std::move(safety);
        return true;
    }

    return false;
}

bool tryDockingLateralDetour(
    const LocalGuidanceRequest& request,
    const TrajectorySafetyReport& directSafety,
    TrajectoryPredictionResult& outPrediction,
    TrajectorySafetyReport& outSafety
)
{
    if (directSafety.conflicts.empty())
        return false;

    const double total = request.profile.horizonSeconds;
    const double ingressDuration = std::clamp(total * 0.30, 2.0, 6.0);
    const double preIngress = total - ingressDuration;
    if (preIngress <= 2.0)
        return false;

    const TrajectoryConflict& conflict = directSafety.conflicts.front();
    const double detourDuration = preIngress * 0.48;
    const double approachDuration = preIngress - detourDuration;
    const double detourTime = request.startUniverseTimeSeconds + detourDuration;
    const double approachTime = request.startUniverseTimeSeconds + preIngress;
    const double endTime = request.startUniverseTimeSeconds + total;

    glm::dvec3 pathDirection =
        dockingApproachPointAt(request, approachTime) -
        request.actorState.positionMeters;
    if (glm::length(pathDirection) <= Epsilon)
        return false;
    pathDirection = glm::normalize(pathDirection);

    glm::dvec3 lateral =
        conflict.shipPositionMeters - conflict.hazardPositionMeters;
    lateral -= pathDirection * glm::dot(lateral, pathDirection);
    if (glm::length(lateral) <= Epsilon)
    {
        const glm::dvec3 dockUp = dockUpAt(request, detourTime);
        lateral = glm::cross(pathDirection, dockUp);
    }
    if (glm::length(lateral) <= Epsilon)
        return false;
    lateral = glm::normalize(lateral);

    const double detourClearance = std::max(
        30.0,
        conflict.requiredSeparationMeters * 1.35 +
            effectiveShipSafetyRadius(request)
    );

    for (double sign : {1.0, -1.0})
    {
        const glm::dvec3 detourPoint =
            conflict.hazardPositionMeters + lateral * detourClearance * sign;
        glm::dvec3 towardApproach =
            dockingApproachPointAt(request, approachTime) - detourPoint;
        if (glm::length(towardApproach) <= Epsilon)
            continue;
        towardApproach = glm::normalize(towardApproach);

        const auto first = predictLeg(
            request,
            request.actorState,
            request.actorProperAccelerationMps2,
            request.startUniverseTimeSeconds,
            detourDuration,
            detourPoint,
            towardApproach * std::max(5.0, dockingEntrySpeedMps(request))
        );
        if (!first.ok() || first.samples.empty())
            continue;

        const auto& firstJoin = first.samples.back();
        const auto second = predictLeg(
            request,
            firstJoin.state,
            firstJoin.properAccelerationMps2,
            firstJoin.universeTimeSeconds,
            approachDuration,
            dockingApproachPointAt(request, approachTime),
            dockingIngressVelocityAt(request, approachTime)
        );
        if (!second.ok() || second.samples.empty())
            continue;

        auto firstTwo = concatenatePredictions(first, second);
        if (!firstTwo.ok() || firstTwo.samples.empty())
            continue;

        const auto& secondJoin = firstTwo.samples.back();
        const auto ingress = predictLeg(
            request,
            secondJoin.state,
            secondJoin.properAccelerationMps2,
            secondJoin.universeTimeSeconds,
            ingressDuration,
            dockingTerminalPointAt(request, endTime),
            dockingIngressVelocityAt(request, endTime)
        );
        if (!ingress.ok() || ingress.samples.empty())
            continue;

        auto combined = concatenatePredictions(firstTwo, ingress);
        if (!combined.ok())
            continue;

        auto safety = TrajectorySafetyEvaluator::evaluate(
            combined,
            request.environment,
            effectiveShipSafetyRadius(request)
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
    const glm::dvec3& upHint
)
{
    return orientationFromForwardUp(pathTangent, upHint);
}

double smoothStep(double edge0, double edge1, double x)
{
    if (edge1 <= edge0)
        return x >= edge1 ? 1.0 : 0.0;
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

DockingTerminalStateReport evaluateDockingTerminalState(
    const LocalGuidanceRequest& request,
    const TrajectoryPredictionResult& prediction
)
{
    DockingTerminalStateReport report;
    if (prediction.samples.empty())
        return report;

    report.evaluated = true;
    const auto& end = prediction.samples.back();
    const double endTime = end.universeTimeSeconds;
    report.requiredPositionMeters = dockingTerminalPointAt(request, endTime);
    report.requiredVelocityMps = dockingIngressVelocityAt(request, endTime);

    const glm::dvec3 worldPositionError =
        end.state.positionMeters - report.requiredPositionMeters;
    const glm::dvec3 worldVelocityError =
        end.state.velocityMps - report.requiredVelocityMps;

    const glm::dquat terminalPose = dockingOrientationAt(request, endTime);
    const glm::dvec3 right = glm::normalize(
        terminalPose * glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = glm::normalize(
        terminalPose * glm::dvec3(0.0, 1.0, 0.0)
    );
    const glm::dvec3 inbound = glm::normalize(
        terminalPose * glm::dvec3(0.0, 0.0, -1.0)
    );

    report.positionErrorDockMeters = {
        glm::dot(worldPositionError, right),
        glm::dot(worldPositionError, up),
        glm::dot(worldPositionError, inbound)
    };
    report.velocityErrorDockMps = {
        glm::dot(worldVelocityError, right),
        glm::dot(worldVelocityError, up),
        glm::dot(worldVelocityError, inbound)
    };

    report.positionErrorMeters = glm::length(worldPositionError);
    report.velocityErrorMps = glm::length(worldVelocityError);
    report.positionToleranceMeters = std::clamp(
        std::max(0.5, request.target.requiredClearanceMeters * 0.15),
        0.5,
        5.0
    );
    report.velocityToleranceMps = std::clamp(
        std::max(0.25, request.target.maxEntrySpeedMps * 0.10),
        0.25,
        2.0
    );
    report.matched =
        report.positionErrorMeters <= report.positionToleranceMeters &&
        report.velocityErrorMps <= report.velocityToleranceMps;
    return report;
}

GuidanceCorridor buildCorridor(
    const LocalGuidanceRequest& request,
    const TrajectoryPredictionResult& prediction,
    const TrajectorySafetyReport& safety,
    GuidancePurpose purpose,
    GuidanceSource source,
    bool noSafePrimarySolution
)
{
    GuidanceCorridor corridor;
    corridor.id = request.corridorId.empty()
        ? ("local:" + request.target.hubModuleId + ":" + request.target.id)
        : request.corridorId;
    corridor.systemId = request.systemId;
    corridor.source = source;
    corridor.purpose = purpose;
    corridor.generatedAtUniverseTimeSeconds = request.startUniverseTimeSeconds;
    corridor.validUntilUniverseTimeSeconds = prediction.samples.empty()
        ? request.startUniverseTimeSeconds
        : prediction.samples.back().universeTimeSeconds;
    corridor.confidence = safety.safe ? 1.0 : 0.25;
    corridor.priority = purpose == GuidancePurpose::EmergencyEscape ? 100 : 50;
    corridor.advisoryOnly = true;
    corridor.noSafePrimarySolution = noSafePrimarySolution;

    const auto& vehicle = request.profile.vehicleEnvelope;
    const bool vehiclePose = vehicle.valid &&
        (purpose == GuidancePurpose::Docking ||
         purpose == GuidancePurpose::EmergencyEscape);

    const double anchorWidth = std::max(
        request.target.extentMeters.x,
        request.target.extentMeters.y
    );
    const double clearance = std::max(
        request.target.requiredClearanceMeters,
        vehicle.valid ? vehicle.hullClearanceMeters : 0.0
    );
    const double vehicleWidth = vehicle.valid ? vehicle.widthMeters : 0.0;
    const double vehicleHeight = vehicle.valid ? vehicle.heightMeters : 0.0;
    const double defaultWidth = vehiclePose
        ? std::max(8.0, vehicleWidth + 2.0 * clearance)
        : std::max(20.0, anchorWidth + 2.0 * clearance);
    const double defaultHeight = vehiclePose
        ? std::max(8.0, vehicleHeight + 2.0 * clearance)
        : std::max(20.0, anchorWidth + 2.0 * clearance);
    const double baseWidth = request.profile.corridorWidthMeters > 0.0
        ? request.profile.corridorWidthMeters
        : defaultWidth;
    const double baseHeight = request.profile.corridorHeightMeters > 0.0
        ? request.profile.corridorHeightMeters
        : defaultHeight;

    const double duration = std::max(
        Epsilon,
        corridor.validUntilUniverseTimeSeconds -
            request.startUniverseTimeSeconds
    );

    corridor.frames.reserve(prediction.samples.size());
    for (std::size_t i = 0; i < prediction.samples.size(); ++i)
    {
        const auto& sample = prediction.samples[i];
        const double u = std::clamp(
            (sample.universeTimeSeconds - request.startUniverseTimeSeconds) /
                duration,
            0.0,
            1.0
        );

        glm::dvec3 tangent = sample.state.velocityMps;
        if (i + 1 < prediction.samples.size())
        {
            tangent = prediction.samples[i + 1].state.positionMeters -
                sample.state.positionMeters;
        }
        else if (i > 0)
        {
            tangent = sample.state.positionMeters -
                prediction.samples[i - 1].state.positionMeters;
        }
        if (glm::length(tangent) <= Epsilon)
            tangent = request.actorOrientation * glm::dvec3(0.0, 0.0, -1.0);

        const glm::dquat freeActorPose = actorOrientationAt(
            request,
            sample.universeTimeSeconds
        );
        const glm::dvec3 actorUp =
            freeActorPose * glm::dvec3(0.0, 1.0, 0.0);
        glm::dquat frameOrientation = corridorOrientation(tangent, actorUp);

        if (purpose == GuidancePurpose::Docking)
        {
            const glm::dquat terminalPose = dockingOrientationAt(
                request,
                sample.universeTimeSeconds
            );
            const double terminalBlend = smoothStep(0.40, 1.0, u);
            frameOrientation = glm::normalize(glm::slerp(
                frameOrientation,
                terminalPose,
                terminalBlend
            ));
        }

        if (i == 0 && vehiclePose)
            frameOrientation = glm::normalize(request.actorOrientation);

        GuidanceFrame frame;
        frame.universeTimeSeconds = sample.universeTimeSeconds;
        frame.centerMeters = sample.state.positionMeters;
        frame.orientation = frameOrientation;
        frame.requiredVehiclePose = vehiclePose;

        const double broadening = 1.0 + (1.0 - u) * 0.75;
        frame.widthMeters = baseWidth * broadening;
        frame.heightMeters = baseHeight * broadening;
        frame.lateralToleranceMeters = frame.widthMeters * 0.45;
        frame.verticalToleranceMeters = frame.heightMeters * 0.45;
        frame.recommendedSpeedMps = request.profile.recommendedSpeedMps > 0.0
            ? request.profile.recommendedSpeedMps
            : request.target.maxEntrySpeedMps;
        frame.maxClosureRateMps = request.profile.maxClosureRateMps > 0.0
            ? request.profile.maxClosureRateMps
            : request.target.maxEntrySpeedMps;
        corridor.frames.push_back(std::move(frame));
    }

    if (!corridor.frames.empty() && purpose == GuidancePurpose::Docking)
    {
        const auto terminal = evaluateDockingTerminalState(request, prediction);
        corridor.hasTerminalTarget = terminal.evaluated;
        corridor.terminalTargetMeters = terminal.requiredPositionMeters;
        corridor.terminalPositionErrorMeters = terminal.positionErrorMeters;

        // Never move the last frame onto the dock by presentation fiat. The
        // final frame is the physical predictor sample. A docking corridor is
        // published only after terminal validation accepts that sample.
        auto& last = corridor.frames.back();
        last.widthMeters = baseWidth;
        last.heightMeters = baseHeight;
        last.requiredVehiclePose = true;
    }

    return corridor;
}

bool tryEmergencyEscape(
    const LocalGuidanceRequest& request,
    const TrajectorySafetyReport& primarySafety,
    TrajectoryPredictionResult& outPrediction,
    TrajectorySafetyReport& outSafety
)
{
    const double duration = std::clamp(
        request.profile.horizonSeconds * 0.35,
        3.0,
        8.0
    );
    const double shipRadius = effectiveShipSafetyRadius(request);
    const double requestedDistance = request.profile.emergencyEscapeDistanceMeters;
    const double escapeDistance = requestedDistance > 0.0
        ? requestedDistance
        : std::max({
            120.0,
            shipRadius * 4.0,
            glm::length(request.actorState.velocityMps) * duration + 60.0
        });

    const glm::dvec3 actorForward = glm::normalize(
        request.actorOrientation * glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 actorRight = glm::normalize(
        request.actorOrientation * glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 actorUp = glm::normalize(
        request.actorOrientation * glm::dvec3(0.0, 1.0, 0.0)
    );
    const glm::dvec3 dockOutward = outwardAt(
        request,
        request.startUniverseTimeSeconds
    );

    glm::dvec3 awayFromConflict = dockOutward;
    if (!primarySafety.conflicts.empty())
    {
        awayFromConflict = request.actorState.positionMeters -
            primarySafety.conflicts.front().hazardPositionMeters;
        if (glm::length(awayFromConflict) <= Epsilon)
            awayFromConflict = dockOutward;
        else
            awayFromConflict = glm::normalize(awayFromConflict);
    }

    const std::array<glm::dvec3, 8> directions = {
        awayFromConflict,
        dockOutward,
        actorRight,
        -actorRight,
        actorUp,
        -actorUp,
        -actorForward,
        actorForward
    };

    for (glm::dvec3 direction : directions)
    {
        if (glm::length(direction) <= Epsilon)
            continue;
        direction = glm::normalize(direction);

        const glm::dvec3 endPosition =
            request.actorState.positionMeters + direction * escapeDistance;
        const double targetSpeed = std::clamp(
            escapeDistance / duration,
            10.0,
            80.0
        );
        const glm::dvec3 endVelocity = direction * targetSpeed;

        auto candidate = predictLeg(
            request,
            request.actorState,
            request.actorProperAccelerationMps2,
            request.startUniverseTimeSeconds,
            duration,
            endPosition,
            endVelocity
        );
        if (!candidate.ok() || candidate.samples.empty())
            continue;

        auto safety = TrajectorySafetyEvaluator::evaluate(
            candidate,
            request.environment,
            shipRadius
        );
        if (!safety.safe)
            continue;

        outPrediction = std::move(candidate);
        outSafety = std::move(safety);
        return true;
    }

    return false;
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
    const bool docking =
        request.profile.purpose == GuidancePurpose::Docking &&
        request.target.kind == HubSemanticAnchorKind::DockingPort;

    const glm::dvec3 targetEndPosition = docking
        ? dockingTerminalPointAt(request, endTime)
        : positionAt(request, endTime);
    const glm::dvec3 targetEndVelocity = docking
        ? dockingIngressVelocityAt(request, endTime)
        : velocityAt(request, endTime);

    out.prediction = docking
        ? predictDockingCandidate(request)
        : predictLeg(
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
        effectiveShipSafetyRadius(request)
    );

    if (!out.safety.safe)
    {
        TrajectoryPredictionResult detourPrediction;
        TrajectorySafetyReport detourSafety;
        const bool detourReady = docking
            ? tryDockingLateralDetour(
                request,
                out.safety,
                detourPrediction,
                detourSafety
              )
            : trySimpleLateralDetour(
                request,
                out.safety,
                targetEndPosition,
                targetEndVelocity,
                detourPrediction,
                detourSafety
              );

        if (detourReady)
        {
            out.prediction = std::move(detourPrediction);
            out.safety = std::move(detourSafety);
            out.detourUsed = true;
        }
    }

    if (out.safety.safe && docking)
    {
        out.terminal = evaluateDockingTerminalState(request, out.prediction);
        if (!out.terminal.matched)
        {
            out.status = LocalGuidanceStatus::NoTerminalSolution;
            out.message = "docking prediction does not satisfy terminal state";
            return out;
        }
    }

    if (out.safety.safe)
    {
        out.corridor = buildCorridor(
            request,
            out.prediction,
            out.safety,
            request.profile.purpose,
            GuidanceSource::LocalPlanner,
            false
        );
        out.status = LocalGuidanceStatus::Ready;
        out.message = docking
            ? (out.detourUsed
                ? "6-DOF docking detour guidance ready"
                : "6-DOF docking guidance ready")
            : (out.detourUsed
                ? "local guidance detour candidate ready"
                : "direct local guidance candidate ready");
        return out;
    }

    TrajectoryPredictionResult escapePrediction;
    TrajectorySafetyReport escapeSafety;
    if (tryEmergencyEscape(
            request,
            out.safety,
            escapePrediction,
            escapeSafety))
    {
        out.prediction = std::move(escapePrediction);
        out.safety = std::move(escapeSafety);
        out.corridor = buildCorridor(
            request,
            out.prediction,
            out.safety,
            GuidancePurpose::EmergencyEscape,
            GuidanceSource::EmergencyControl,
            true
        );
        out.emergencyEscapeUsed = true;
        out.status = LocalGuidanceStatus::EmergencyEscapeReady;
        out.message = "no safe primary solution; emergency escape corridor ready";
        return out;
    }

    out.status = LocalGuidanceStatus::Blocked;
    out.message = "no safe guidance solution";
    return out;
}

} // namespace game::navigation
