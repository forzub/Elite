#include "src/game/navigation/TrajectoryPredictor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::navigation
{
namespace
{

constexpr double TimeEpsilon = 1.0e-9;
constexpr double VectorEpsilon = 1.0e-12;

bool finite(double value)
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value)
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

double magnitude(const glm::dvec3& value)
{
    return std::sqrt(glm::dot(value, value));
}

glm::dvec3 clampMagnitude(
    const glm::dvec3& value,
    double limit,
    bool& clamped
)
{
    if (limit <= 0.0)
        return value;

    const double length = magnitude(value);
    if (length <= limit || length <= VectorEpsilon)
        return value;

    clamped = true;
    return value * (limit / length);
}

std::vector<TrajectoryAccelerationKey> canonicalProgram(
    const TrajectoryPredictionRequest& request
)
{
    std::vector<TrajectoryAccelerationKey> program;
    program.reserve(request.accelerationProgram.size());

    for (const TrajectoryAccelerationKey& key : request.accelerationProgram)
    {
        if (!finite(key.timeOffsetSeconds) ||
            !finite(key.properAccelerationMps2))
        {
            continue;
        }

        TrajectoryAccelerationKey canonical = key;
        canonical.timeOffsetSeconds =
            std::max(0.0, canonical.timeOffsetSeconds);
        program.push_back(canonical);
    }

    std::stable_sort(
        program.begin(),
        program.end(),
        [](const TrajectoryAccelerationKey& a,
           const TrajectoryAccelerationKey& b)
        {
            return a.timeOffsetSeconds < b.timeOffsetSeconds;
        }
    );

    // Duplicate timestamps describe one target. Keep the last authored value
    // so a solver can replace a key without first erasing the old one.
    std::vector<TrajectoryAccelerationKey> unique;
    unique.reserve(program.size());
    for (const TrajectoryAccelerationKey& key : program)
    {
        if (!unique.empty() &&
            std::abs(unique.back().timeOffsetSeconds -
                     key.timeOffsetSeconds) <= TimeEpsilon)
        {
            unique.back() = key;
        }
        else
        {
            unique.push_back(key);
        }
    }

    return unique;
}

glm::dvec3 requestedProperAcceleration(
    const std::vector<TrajectoryAccelerationKey>& program,
    const glm::dvec3& initialProperAccelerationMps2,
    double timeOffsetSeconds
)
{
    if (program.empty())
        return initialProperAccelerationMps2;

    const double t = std::max(0.0, timeOffsetSeconds);

    if (t <= program.front().timeOffsetSeconds)
    {
        const double end = program.front().timeOffsetSeconds;
        if (end <= TimeEpsilon)
            return program.front().properAccelerationMps2;

        const double u = std::clamp(t / end, 0.0, 1.0);
        return initialProperAccelerationMps2 +
            (program.front().properAccelerationMps2 -
             initialProperAccelerationMps2) * u;
    }

    for (std::size_t i = 1; i < program.size(); ++i)
    {
        if (t > program[i].timeOffsetSeconds)
            continue;

        const TrajectoryAccelerationKey& a = program[i - 1];
        const TrajectoryAccelerationKey& b = program[i];
        const double span = b.timeOffsetSeconds - a.timeOffsetSeconds;
        if (span <= TimeEpsilon)
            return b.properAccelerationMps2;

        const double u = std::clamp(
            (t - a.timeOffsetSeconds) / span,
            0.0,
            1.0
        );
        return a.properAccelerationMps2 +
            (b.properAccelerationMps2 - a.properAccelerationMps2) * u;
    }

    return program.back().properAccelerationMps2;
}

glm::dvec3 applyJerkEnvelope(
    const glm::dvec3& current,
    const glm::dvec3& target,
    double maxJerkMps3,
    double dt,
    bool& clamped
)
{
    if (maxJerkMps3 <= 0.0 || dt <= 0.0)
        return target;

    const glm::dvec3 delta = target - current;
    const double deltaLength = magnitude(delta);
    const double maxDelta = maxJerkMps3 * dt;

    if (deltaLength <= maxDelta || deltaLength <= VectorEpsilon)
        return target;

    clamped = true;
    return current + delta * (maxDelta / deltaLength);
}

GravityFieldSample gravityAt(
    const glm::dvec3& positionMeters,
    const std::vector<GravityBody>& bodies
)
{
    return GravityFieldSystem::sample(positionMeters, bodies);
}

bool validRequest(const TrajectoryPredictionRequest& request)
{
    return finite(request.startUniverseTimeSeconds) &&
        finite(request.horizonSeconds) &&
        finite(request.sampleIntervalSeconds) &&
        finite(request.maxIntegrationStepSeconds) &&
        request.horizonSeconds >= 0.0 &&
        request.sampleIntervalSeconds > 0.0 &&
        request.maxIntegrationStepSeconds > 0.0 &&
        finite(request.initialState.positionMeters) &&
        finite(request.initialState.velocityMps) &&
        finite(request.initialState.accelerationMps2) &&
        finite(request.initialProperAccelerationMps2) &&
        finite(request.motionEnvelope.maxProperAccelerationMps2) &&
        finite(request.motionEnvelope.maxProperJerkMps3);
}

TrajectoryPredictionSample makeSample(
    const TrajectoryPredictionRequest& request,
    double timeOffsetSeconds,
    const WorldKinematicState& state,
    const glm::dvec3& properAccelerationMps2,
    const GravityFieldSample& gravity,
    double cumulativeProperDeltaVMps
)
{
    TrajectoryPredictionSample sample;
    sample.universeTimeSeconds =
        request.startUniverseTimeSeconds + timeOffsetSeconds;
    sample.timeOffsetSeconds = timeOffsetSeconds;
    sample.state = state;
    sample.properAccelerationMps2 = properAccelerationMps2;
    sample.gravityAccelerationMps2 = gravity.accelerationMps2;
    sample.properLoadGs =
        magnitude(properAccelerationMps2) / StandardGravityMps2;
    sample.cumulativeProperDeltaVMps = cumulativeProperDeltaVMps;
    return sample;
}

} // namespace

TrajectoryPredictionResult TrajectoryPredictor::predict(
    const TrajectoryPredictionRequest& request
)
{
    TrajectoryPredictionResult result;
    result.systemId = request.systemId;

    if (!validRequest(request))
    {
        result.status = TrajectoryPredictionStatus::InvalidRequest;
        result.message = "invalid trajectory prediction request";
        return result;
    }

    const std::vector<TrajectoryAccelerationKey> program =
        canonicalProgram(request);

    WorldKinematicState state = request.initialState;

    bool initialAccelClamped = false;
    glm::dvec3 properAcceleration = clampMagnitude(
        request.initialProperAccelerationMps2,
        request.motionEnvelope.maxProperAccelerationMps2,
        initialAccelClamped
    );
    result.diagnostics.accelerationClamped = initialAccelClamped;

    GravityFieldSample gravity = gravityAt(
        state.positionMeters,
        request.gravityBodies
    );
    state.accelerationMps2 =
        gravity.accelerationMps2 + properAcceleration;

    if (!finite(state.accelerationMps2))
    {
        result.status = TrajectoryPredictionStatus::NumericalFailure;
        result.message = "non-finite initial trajectory acceleration";
        return result;
    }

    result.samples.push_back(
        makeSample(
            request,
            0.0,
            state,
            properAcceleration,
            gravity,
            0.0
        )
    );

    result.diagnostics.maxSpeedMps = magnitude(state.velocityMps);
    result.diagnostics.maxRequestedProperAccelerationMps2 =
        magnitude(request.initialProperAccelerationMps2);
    result.diagnostics.maxAppliedProperAccelerationMps2 =
        magnitude(properAcceleration);
    result.diagnostics.maxProperLoadGs =
        result.diagnostics.maxAppliedProperAccelerationMps2 /
        StandardGravityMps2;

    if (request.horizonSeconds <= TimeEpsilon)
    {
        result.status = TrajectoryPredictionStatus::Ok;
        result.message = "ok";
        return result;
    }

    double time = 0.0;
    double nextSampleTime =
        std::min(request.sampleIntervalSeconds, request.horizonSeconds);
    double cumulativeProperDeltaV = 0.0;
    double travelledDistance = 0.0;

    while (time < request.horizonSeconds - TimeEpsilon)
    {
        const double remainingToSample =
            std::max(0.0, nextSampleTime - time);
        const double remainingToEnd =
            std::max(0.0, request.horizonSeconds - time);

        double dt = std::min(
            request.maxIntegrationStepSeconds,
            remainingToEnd
        );
        if (remainingToSample > TimeEpsilon)
            dt = std::min(dt, remainingToSample);

        if (dt <= TimeEpsilon)
        {
            // We are exactly on a sample boundary. Append once and advance.
            gravity = gravityAt(state.positionMeters, request.gravityBodies);
            state.accelerationMps2 =
                gravity.accelerationMps2 + properAcceleration;
            result.samples.push_back(
                makeSample(
                    request,
                    time,
                    state,
                    properAcceleration,
                    gravity,
                    cumulativeProperDeltaV
                )
            );

            if (nextSampleTime >= request.horizonSeconds - TimeEpsilon)
                break;

            nextSampleTime = std::min(
                nextSampleTime + request.sampleIntervalSeconds,
                request.horizonSeconds
            );
            continue;
        }

        const double targetTime = time + dt;
        const glm::dvec3 rawRequested = requestedProperAcceleration(
            program,
            request.initialProperAccelerationMps2,
            targetTime
        );
        result.diagnostics.maxRequestedProperAccelerationMps2 = std::max(
            result.diagnostics.maxRequestedProperAccelerationMps2,
            magnitude(rawRequested)
        );

        bool accelClamped = false;
        const glm::dvec3 requested = clampMagnitude(
            rawRequested,
            request.motionEnvelope.maxProperAccelerationMps2,
            accelClamped
        );
        result.diagnostics.accelerationClamped =
            result.diagnostics.accelerationClamped || accelClamped;

        bool jerkClamped = false;
        const glm::dvec3 nextProperAcceleration = applyJerkEnvelope(
            properAcceleration,
            requested,
            request.motionEnvelope.maxProperJerkMps3,
            dt,
            jerkClamped
        );
        result.diagnostics.jerkClamped =
            result.diagnostics.jerkClamped || jerkClamped;

        const double appliedJerk =
            magnitude(nextProperAcceleration - properAcceleration) / dt;
        result.diagnostics.maxAppliedProperJerkMps3 = std::max(
            result.diagnostics.maxAppliedProperJerkMps3,
            appliedJerk
        );

        const GravityFieldSample gravityStart = gravityAt(
            state.positionMeters,
            request.gravityBodies
        );
        const glm::dvec3 properMid =
            0.5 * (properAcceleration + nextProperAcceleration);
        const glm::dvec3 positionAcceleration =
            gravityStart.accelerationMps2 + properMid;

        const glm::dvec3 previousVelocity = state.velocityMps;

        const glm::dvec3 nextPosition =
            state.positionMeters +
            state.velocityMps * dt +
            0.5 * positionAcceleration * dt * dt;

        const GravityFieldSample gravityEnd = gravityAt(
            nextPosition,
            request.gravityBodies
        );

        const glm::dvec3 accelerationStart =
            gravityStart.accelerationMps2 + properAcceleration;
        const glm::dvec3 accelerationEnd =
            gravityEnd.accelerationMps2 + nextProperAcceleration;

        const glm::dvec3 nextVelocity =
            state.velocityMps +
            0.5 * (accelerationStart + accelerationEnd) * dt;

        if (!finite(nextPosition) ||
            !finite(nextVelocity) ||
            !finite(accelerationEnd))
        {
            result.status = TrajectoryPredictionStatus::NumericalFailure;
            result.message = "non-finite trajectory state";
            result.diagnostics.totalProperDeltaVMps =
                cumulativeProperDeltaV;
            result.diagnostics.travelledDistanceMeters =
                travelledDistance;
            return result;
        }

        state.positionMeters = nextPosition;
        state.velocityMps = nextVelocity;
        state.accelerationMps2 = accelerationEnd;
        properAcceleration = nextProperAcceleration;
        gravity = gravityEnd;
        time = targetTime;

        cumulativeProperDeltaV += magnitude(properMid) * dt;
        travelledDistance +=
            0.5 * (magnitude(previousVelocity) +
                   magnitude(nextVelocity)) * dt;

        result.diagnostics.integrationSteps += 1;
        result.diagnostics.maxSpeedMps = std::max(
            result.diagnostics.maxSpeedMps,
            magnitude(state.velocityMps)
        );
        result.diagnostics.maxAppliedProperAccelerationMps2 = std::max(
            result.diagnostics.maxAppliedProperAccelerationMps2,
            magnitude(properAcceleration)
        );
        result.diagnostics.maxProperLoadGs = std::max(
            result.diagnostics.maxProperLoadGs,
            magnitude(properAcceleration) / StandardGravityMps2
        );

        const bool reachedSample =
            time >= nextSampleTime - TimeEpsilon;
        const bool reachedEnd =
            time >= request.horizonSeconds - TimeEpsilon;

        if (reachedSample || reachedEnd)
        {
            result.samples.push_back(
                makeSample(
                    request,
                    time,
                    state,
                    properAcceleration,
                    gravity,
                    cumulativeProperDeltaV
                )
            );

            if (reachedEnd)
                break;

            nextSampleTime = std::min(
                nextSampleTime + request.sampleIntervalSeconds,
                request.horizonSeconds
            );
        }

    }

    result.diagnostics.totalProperDeltaVMps = cumulativeProperDeltaV;
    result.diagnostics.travelledDistanceMeters = travelledDistance;
    result.status = TrajectoryPredictionStatus::Ok;
    result.message = "ok";
    return result;
}

} // namespace game::navigation
