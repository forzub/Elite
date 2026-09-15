#include "src/game/navigation/RuckigTrajectorySolver.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

#include <ruckig/ruckig.hpp>

namespace game::navigation
{
namespace
{
constexpr double TimeEpsilon = 1.0e-9;
constexpr double ConstraintFloor = 1.0e-6;

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

std::array<double, 3> toArray(const glm::dvec3& value)
{
    return {value.x, value.y, value.z};
}

glm::dvec3 toVec3(const std::array<double, 3>& value)
{
    return {value[0], value[1], value[2]};
}

RuckigTrajectoryResult failure(
    const RuckigTrajectoryRequest& request,
    TrajectoryPredictionStatus status,
    const std::string& message
)
{
    RuckigTrajectoryResult out;
    out.prediction.status = status;
    out.prediction.systemId = request.systemId;
    out.prediction.message = message;
    return out;
}

bool validRequest(const RuckigTrajectoryRequest& request)
{
    return request.systemId >= 0 &&
        finite(request.startUniverseTimeSeconds) &&
        finite(request.horizonSeconds) &&
        finite(request.sampleIntervalSeconds) &&
        finite(request.validationStepSeconds) &&
        request.horizonSeconds > 0.0 &&
        request.sampleIntervalSeconds > 0.0 &&
        request.validationStepSeconds > 0.0 &&
        finite(request.initialState.positionMeters) &&
        finite(request.initialState.velocityMps) &&
        finite(request.initialState.accelerationMps2) &&
        finite(request.initialProperAccelerationMps2) &&
        finite(request.targetPositionMeters) &&
        finite(request.targetVelocityMps) &&
        finite(request.motionEnvelope.maxProperAccelerationMps2) &&
        finite(request.motionEnvelope.maxProperJerkMps3);
}

struct CoMovingFrame
{
    glm::dvec3 position0Meters {0.0};
    glm::dvec3 velocity0Mps {0.0};
    glm::dvec3 accelerationMps2 {0.0};

    glm::dvec3 positionAt(double t) const
    {
        return position0Meters + velocity0Mps * t +
            0.5 * accelerationMps2 * t * t;
    }

    glm::dvec3 velocityAt(double t) const
    {
        return velocity0Mps + accelerationMps2 * t;
    }
};

CoMovingFrame makeTerminalFrame(
    const RuckigTrajectoryRequest& request,
    const glm::dvec3& referenceGravityMps2
)
{
    const double total = request.horizonSeconds;
    CoMovingFrame frame;
    frame.accelerationMps2 = referenceGravityMps2;
    frame.velocity0Mps =
        request.targetVelocityMps - referenceGravityMps2 * total;
    frame.position0Meters =
        request.targetPositionMeters -
        frame.velocity0Mps * total -
        0.5 * referenceGravityMps2 * total * total;
    return frame;
}

double derivedAccelerationLimit(
    double relativeDistance,
    double relativeSpeed,
    double currentAcceleration,
    double targetAcceleration,
    double duration
)
{
    const double invT = 1.0 / std::max(duration, TimeEpsilon);
    return std::max({
        1.0,
        std::abs(currentAcceleration),
        std::abs(targetAcceleration),
        6.0 * std::abs(relativeDistance) * invT * invT +
            4.0 * std::abs(relativeSpeed) * invT
    });
}

struct SampledState
{
    WorldKinematicState state;
    glm::dvec3 properAccelerationMps2 {0.0};
    GravityFieldSample gravity;
};

SampledState sampleTrajectory(
    const RuckigTrajectoryRequest& request,
    const CoMovingFrame& frame,
    const ruckig::Trajectory<3>& trajectory,
    double timeOffsetSeconds
)
{
    std::array<double, 3> relativePosition {};
    std::array<double, 3> relativeVelocity {};
    std::array<double, 3> relativeAcceleration {};
    trajectory.at_time(
        timeOffsetSeconds,
        relativePosition,
        relativeVelocity,
        relativeAcceleration
    );

    SampledState out;
    out.state.positionMeters =
        frame.positionAt(timeOffsetSeconds) + toVec3(relativePosition);
    out.state.velocityMps =
        frame.velocityAt(timeOffsetSeconds) + toVec3(relativeVelocity);
    out.state.accelerationMps2 =
        frame.accelerationMps2 + toVec3(relativeAcceleration);
    out.gravity = GravityFieldSystem::sample(
        out.state.positionMeters,
        request.gravityBodies
    );
    out.properAccelerationMps2 =
        out.state.accelerationMps2 - out.gravity.accelerationMps2;
    return out;
}

TrajectoryPredictionSample makeOutputSample(
    const RuckigTrajectoryRequest& request,
    double timeOffsetSeconds,
    const SampledState& sampled,
    double cumulativeProperDeltaVMps
)
{
    TrajectoryPredictionSample out;
    out.universeTimeSeconds =
        request.startUniverseTimeSeconds + timeOffsetSeconds;
    out.timeOffsetSeconds = timeOffsetSeconds;
    out.state = sampled.state;
    out.properAccelerationMps2 = sampled.properAccelerationMps2;
    out.gravityAccelerationMps2 = sampled.gravity.accelerationMps2;
    out.properLoadGs =
        magnitude(sampled.properAccelerationMps2) / StandardGravityMps2;
    out.cumulativeProperDeltaVMps = cumulativeProperDeltaVMps;
    return out;
}

} // namespace

RuckigTrajectoryResult RuckigTrajectorySolver::solve(
    const RuckigTrajectoryRequest& request
)
{
    if (!validRequest(request))
    {
        return failure(
            request,
            TrajectoryPredictionStatus::InvalidRequest,
            "invalid Ruckig trajectory request"
        );
    }

    const auto gravityStart = GravityFieldSystem::sample(
        request.initialState.positionMeters,
        request.gravityBodies
    );
    const auto gravityTarget = GravityFieldSystem::sample(
        request.targetPositionMeters,
        request.gravityBodies
    );
    const glm::dvec3 referenceGravity =
        0.5 * (gravityStart.accelerationMps2 +
               gravityTarget.accelerationMps2);
    const CoMovingFrame frame = makeTerminalFrame(
        request,
        referenceGravity
    );

    const glm::dvec3 relativePosition0 =
        request.initialState.positionMeters - frame.position0Meters;
    const glm::dvec3 relativeVelocity0 =
        request.initialState.velocityMps - frame.velocity0Mps;
    const glm::dvec3 relativeAcceleration0 =
        request.initialProperAccelerationMps2 +
        gravityStart.accelerationMps2 - referenceGravity;
    const glm::dvec3 relativeTargetAcceleration =
        gravityTarget.accelerationMps2 - referenceGravity;

    ruckig::InputParameter<3> input;
    input.current_position = toArray(relativePosition0);
    input.current_velocity = toArray(relativeVelocity0);
    input.current_acceleration = toArray(relativeAcceleration0);
    input.target_position = {0.0, 0.0, 0.0};
    input.target_velocity = {0.0, 0.0, 0.0};
    input.target_acceleration = toArray(relativeTargetAcceleration);
    input.minimum_duration = request.horizonSeconds;
    input.control_interface = ruckig::ControlInterface::Position;
    input.synchronization = ruckig::Synchronization::Time;

    const double properAccelerationLimit =
        request.motionEnvelope.maxProperAccelerationMps2;
    const double properJerkLimit =
        request.motionEnvelope.maxProperJerkMps3;

    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        const double gravityVariation = std::max(
            std::abs(gravityStart.accelerationMps2[axis] - referenceGravity[axis]),
            std::abs(gravityTarget.accelerationMps2[axis] - referenceGravity[axis])
        );

        double accelerationLimit = properAccelerationLimit > 0.0
            ? properAccelerationLimit + gravityVariation
            : derivedAccelerationLimit(
                relativePosition0[axis],
                relativeVelocity0[axis],
                relativeAcceleration0[axis],
                relativeTargetAcceleration[axis],
                request.horizonSeconds
              );
        accelerationLimit = std::max({
            accelerationLimit,
            std::abs(relativeAcceleration0[axis]) + ConstraintFloor,
            std::abs(relativeTargetAcceleration[axis]) + ConstraintFloor,
            ConstraintFloor
        });

        double jerkLimit = properJerkLimit > 0.0
            ? properJerkLimit +
                2.0 * gravityVariation /
                    std::max(request.horizonSeconds, TimeEpsilon)
            : std::max(
                1.0,
                8.0 * accelerationLimit /
                    std::max(request.horizonSeconds, TimeEpsilon)
              );
        jerkLimit = std::max(jerkLimit, ConstraintFloor);

        const double distanceSpeed =
            2.0 * std::abs(relativePosition0[axis]) /
            std::max(request.horizonSeconds, TimeEpsilon);
        const double velocityLimit = std::max({
            1.0,
            std::abs(relativeVelocity0[axis]) + 1.0,
            distanceSpeed + 1.0,
            std::abs(relativeVelocity0[axis]) +
                accelerationLimit * request.horizonSeconds + 1.0
        });

        input.max_velocity[axis] = velocityLimit;
        input.max_acceleration[axis] = accelerationLimit;
        input.max_jerk[axis] = jerkLimit;
    }

    ruckig::Ruckig<3> ruckig;
    ruckig::Trajectory<3> trajectory;
    const ruckig::Result ruckigResult = ruckig.calculate(input, trajectory);
    if (ruckigResult < 0)
    {
        std::ostringstream message;
        message << "Ruckig failed with code "
                << static_cast<int>(ruckigResult);
        return failure(
            request,
            ruckigResult == ruckig::ErrorInvalidInput
                ? TrajectoryPredictionStatus::InvalidRequest
                : TrajectoryPredictionStatus::NumericalFailure,
            message.str()
        );
    }

    RuckigTrajectoryResult result;
    result.prediction.systemId = request.systemId;
    result.solverDiagnostics.ruckigDurationSeconds =
        trajectory.get_duration();

    const double durationTolerance = std::max(
        1.0e-6,
        request.horizonSeconds * 1.0e-8
    );
    if (trajectory.get_duration() >
        request.horizonSeconds + durationTolerance)
    {
        result.prediction.status = TrajectoryPredictionStatus::NumericalFailure;
        result.prediction.message =
            "Ruckig trajectory cannot satisfy requested leg duration";
        return result;
    }

    const double sampleInterval = std::min(
        request.sampleIntervalSeconds,
        request.horizonSeconds
    );
    const double validationStep = std::min(
        request.validationStepSeconds,
        request.horizonSeconds
    );

    double time = 0.0;
    double nextOutputTime = sampleInterval;
    double cumulativeProperDeltaV = 0.0;
    double travelledDistance = 0.0;
    SampledState previous = sampleTrajectory(request, frame, trajectory, 0.0);

    if (!finite(previous.state.positionMeters) ||
        !finite(previous.state.velocityMps) ||
        !finite(previous.state.accelerationMps2) ||
        !finite(previous.properAccelerationMps2))
    {
        return failure(
            request,
            TrajectoryPredictionStatus::NumericalFailure,
            "Ruckig produced non-finite initial state"
        );
    }

    result.prediction.samples.push_back(
        makeOutputSample(request, 0.0, previous, 0.0)
    );
    result.prediction.diagnostics.maxSpeedMps =
        magnitude(previous.state.velocityMps);
    result.prediction.diagnostics.maxRequestedProperAccelerationMps2 =
        magnitude(previous.properAccelerationMps2);
    result.prediction.diagnostics.maxAppliedProperAccelerationMps2 =
        magnitude(previous.properAccelerationMps2);
    result.prediction.diagnostics.maxProperLoadGs =
        magnitude(previous.properAccelerationMps2) / StandardGravityMps2;

    const double accelerationTolerance = properAccelerationLimit > 0.0
        ? std::max(1.0e-6, properAccelerationLimit * 1.0e-6)
        : 0.0;
    const double jerkTolerance = properJerkLimit > 0.0
        ? std::max(1.0e-6, properJerkLimit * 1.0e-6)
        : 0.0;

    while (time < request.horizonSeconds - TimeEpsilon)
    {
        const double remainingToEnd = request.horizonSeconds - time;
        const double remainingToOutput = nextOutputTime - time;
        double dt = std::min(validationStep, remainingToEnd);
        if (remainingToOutput > TimeEpsilon)
            dt = std::min(dt, remainingToOutput);

        if (dt <= TimeEpsilon)
        {
            nextOutputTime = std::min(
                nextOutputTime + sampleInterval,
                request.horizonSeconds
            );
            continue;
        }

        const double nextTime = time + dt;
        const SampledState current = sampleTrajectory(
            request,
            frame,
            trajectory,
            nextTime
        );
        if (!finite(current.state.positionMeters) ||
            !finite(current.state.velocityMps) ||
            !finite(current.state.accelerationMps2) ||
            !finite(current.properAccelerationMps2))
        {
            result.prediction.status = TrajectoryPredictionStatus::NumericalFailure;
            result.prediction.message =
                "Ruckig produced non-finite trajectory state";
            return result;
        }

        const double previousProper =
            magnitude(previous.properAccelerationMps2);
        const double currentProper =
            magnitude(current.properAccelerationMps2);
        const double currentJerk = magnitude(
            current.properAccelerationMps2 -
            previous.properAccelerationMps2
        ) / dt;

        cumulativeProperDeltaV +=
            0.5 * (previousProper + currentProper) * dt;
        travelledDistance += 0.5 * (
            magnitude(previous.state.velocityMps) +
            magnitude(current.state.velocityMps)
        ) * dt;

        ++result.solverDiagnostics.validationSteps;
        ++result.prediction.diagnostics.integrationSteps;
        result.prediction.diagnostics.maxSpeedMps = std::max(
            result.prediction.diagnostics.maxSpeedMps,
            magnitude(current.state.velocityMps)
        );
        result.prediction.diagnostics.maxRequestedProperAccelerationMps2 =
            std::max(
                result.prediction.diagnostics.maxRequestedProperAccelerationMps2,
                currentProper
            );
        result.prediction.diagnostics.maxAppliedProperAccelerationMps2 =
            std::max(
                result.prediction.diagnostics.maxAppliedProperAccelerationMps2,
                currentProper
            );
        result.prediction.diagnostics.maxProperLoadGs = std::max(
            result.prediction.diagnostics.maxProperLoadGs,
            currentProper / StandardGravityMps2
        );
        result.prediction.diagnostics.maxAppliedProperJerkMps3 = std::max(
            result.prediction.diagnostics.maxAppliedProperJerkMps3,
            currentJerk
        );

        if (properAccelerationLimit > 0.0 &&
            currentProper > properAccelerationLimit + accelerationTolerance)
        {
            result.solverDiagnostics.accelerationLimitExceeded = true;
        }
        if (properJerkLimit > 0.0 &&
            currentJerk > properJerkLimit + jerkTolerance)
        {
            result.solverDiagnostics.jerkLimitExceeded = true;
        }

        time = nextTime;
        previous = current;
        const bool reachedEnd =
            time >= request.horizonSeconds - TimeEpsilon;
        const bool reachedOutput =
            time >= nextOutputTime - TimeEpsilon;
        if (reachedOutput || reachedEnd)
        {
            result.prediction.samples.push_back(
                makeOutputSample(
                    request,
                    time,
                    current,
                    cumulativeProperDeltaV
                )
            );
            if (!reachedEnd)
            {
                nextOutputTime = std::min(
                    nextOutputTime + sampleInterval,
                    request.horizonSeconds
                );
            }
        }
    }

    result.prediction.diagnostics.totalProperDeltaVMps =
        cumulativeProperDeltaV;
    result.prediction.diagnostics.travelledDistanceMeters =
        travelledDistance;

    if (result.solverDiagnostics.accelerationLimitExceeded)
    {
        result.prediction.status = TrajectoryPredictionStatus::NumericalFailure;
        result.prediction.message =
            "Ruckig candidate exceeds Elite proper-acceleration envelope";
        return result;
    }
    if (result.solverDiagnostics.jerkLimitExceeded)
    {
        result.prediction.status = TrajectoryPredictionStatus::NumericalFailure;
        result.prediction.message =
            "Ruckig candidate exceeds Elite proper-jerk envelope";
        return result;
    }
    if (result.prediction.samples.empty())
    {
        result.prediction.status = TrajectoryPredictionStatus::NumericalFailure;
        result.prediction.message = "Ruckig produced no trajectory samples";
        return result;
    }

    const auto& end = result.prediction.samples.back().state;
    const double positionError = magnitude(
        end.positionMeters - request.targetPositionMeters
    );
    const double velocityError = magnitude(
        end.velocityMps - request.targetVelocityMps
    );
    if (positionError > 1.0e-4 || velocityError > 1.0e-5)
    {
        result.prediction.status = TrajectoryPredictionStatus::NumericalFailure;
        result.prediction.message =
            "Ruckig candidate did not reproduce requested terminal state";
        return result;
    }

    result.prediction.status = TrajectoryPredictionStatus::Ok;
    result.prediction.message = "Ruckig local trajectory ready";
    return result;
}

} // namespace game::navigation
