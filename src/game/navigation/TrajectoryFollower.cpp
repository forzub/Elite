#include "TrajectoryFollower.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{
namespace
{

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double lengthSquared = glm::dot(value, value);
    if (!finite(lengthSquared) || lengthSquared <= kEpsilon)
        return fallback;

    return value / std::sqrt(lengthSquared);
}

NavigationRuntimeControlBridge::Vec3d toBridge(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

bool validSegment(const AcceptedShortSegment& segment) noexcept
{
    return
        segment.valid &&
        segment.revision != 0 &&
        finite(segment.acceptedAtUniverseTimeSeconds) &&
        finite(segment.validUntilUniverseTimeSeconds) &&
        segment.validUntilUniverseTimeSeconds >=
            segment.acceptedAtUniverseTimeSeconds &&
        finite(segment.startPositionMapMeters) &&
        finite(segment.targetPositionMapMeters) &&
        finite(segment.targetVelocityMapMetersPerSecond) &&
        finite(segment.fixedLinearAccelerationMapMps2) &&
        finite(segment.velocityResponsePerSecond) &&
        segment.velocityResponsePerSecond >= 0.0 &&
        finite(segment.desiredForwardMap) &&
        finite(segment.angularDampingPerSecond) &&
        segment.angularDampingPerSecond >= 0.0 &&
        finite(segment.orientationResponsePerSecond2) &&
        segment.orientationResponsePerSecond2 >= 0.0 &&
        finite(segment.maximumAngularAccelerationRadPerSec2) &&
        segment.maximumAngularAccelerationRadPerSec2 >= 0.0 &&
        finite(segment.completionRadiusMeters) &&
        segment.completionRadiusMeters >= 0.0 &&
        finite(segment.trackingEnvelopeRadiusMeters) &&
        segment.trackingEnvelopeRadiusMeters >= 0.0 &&
        finite(segment.hazardUrgency01);
}

bool validAgent(const TrajectoryFollower::AgentState& agent) noexcept
{
    return
        finite(agent.positionMapMeters) &&
        finite(agent.velocityMapMetersPerSecond) &&
        finite(agent.forwardMap) &&
        finite(agent.rightMap) &&
        finite(agent.upMap) &&
        finite(agent.pitchRateRadPerSec) &&
        finite(agent.yawRateRadPerSec) &&
        finite(agent.rollRateRadPerSec);
}

double crossTrackError(
    const AcceptedShortSegment& segment,
    const glm::dvec3& position
) noexcept
{
    const glm::dvec3 path =
        segment.targetPositionMapMeters -
        segment.startPositionMapMeters;
    const double pathLengthSquared = glm::dot(path, path);

    if (!finite(pathLengthSquared) || pathLengthSquared <= kEpsilon)
        return glm::length(position - segment.startPositionMapMeters);

    const double t = std::clamp(
        glm::dot(
            position - segment.startPositionMapMeters,
            path
        ) / pathLengthSquared,
        0.0,
        1.0
    );

    const glm::dvec3 closest =
        segment.startPositionMapMeters + path * t;
    return glm::length(position - closest);
}

glm::dvec3 angularVelocityMap(
    const TrajectoryFollower::AgentState& agent
) noexcept
{
    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    return
        right * agent.pitchRateRadPerSec +
        up * agent.yawRateRadPerSec +
        forward * agent.rollRateRadPerSec;
}

glm::dvec3 angularDemand(
    const AcceptedShortSegment& segment,
    const TrajectoryFollower::AgentState& agent
) noexcept
{
    const glm::dvec3 currentAngularVelocity =
        angularVelocityMap(agent);

    if (!segment.alignForward)
    {
        return
            -currentAngularVelocity *
            segment.angularDampingPerSecond;
    }

    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 desired = normalizedOr(
        segment.desiredForwardMap,
        forward
    );

    const glm::dvec3 cross = glm::cross(forward, desired);
    const double sinAngle = glm::length(cross);
    const double cosAngle = std::clamp(
        glm::dot(forward, desired),
        -1.0,
        1.0
    );
    const double angle = std::atan2(sinAngle, cosAngle);

    glm::dvec3 axis(0.0);
    if (sinAngle > kEpsilon)
    {
        axis = cross / sinAngle;
    }
    else if (cosAngle < 0.0)
    {
        axis = normalizedOr(
            agent.upMap,
            glm::dvec3(0.0, 1.0, 0.0)
        );
    }

    glm::dvec3 demand =
        axis *
            (segment.orientationResponsePerSecond2 * angle) -
        currentAngularVelocity *
            segment.angularDampingPerSecond;

    const double magnitude = glm::length(demand);
    if (segment.maximumAngularAccelerationRadPerSec2 > 0.0 &&
        finite(magnitude) &&
        magnitude >
            segment.maximumAngularAccelerationRadPerSec2 &&
        magnitude > kEpsilon)
    {
        demand *=
            segment.maximumAngularAccelerationRadPerSec2 /
            magnitude;
    }

    return demand;
}

} // namespace

TrajectoryFollower::Result TrajectoryFollower::follow(
    const AcceptedShortSegment& segment,
    const AgentState& agent
) noexcept
{
    Result result;

    if (!validSegment(segment) || !validAgent(agent))
        return result;

    result.remainingDistanceMeters =
        glm::length(
            segment.targetPositionMapMeters -
            agent.positionMapMeters
        );
    result.crossTrackErrorMeters =
        crossTrackError(segment, agent.positionMapMeters);

    if (!finite(result.remainingDistanceMeters) ||
        !finite(result.crossTrackErrorMeters))
    {
        return Result {};
    }

    result.trackingErrorExceeded =
        segment.trackingEnvelopeRadiusMeters > 0.0 &&
        result.crossTrackErrorMeters >
            segment.trackingEnvelopeRadiusMeters;

    result.intent.revision = segment.goalRevision;
    result.intent.targetRevision = segment.revision;
    result.intent.emergency = segment.emergency;
    result.intent.hazardUrgency01 =
        std::clamp(segment.hazardUrgency01, 0.0, 1.0);

    glm::dvec3 linearDemand(0.0);
    if (segment.linearMode ==
        AcceptedShortSegment::LinearMode::FixedAcceleration)
    {
        linearDemand =
            segment.fixedLinearAccelerationMapMps2;
    }
    else
    {
        linearDemand =
            (segment.targetVelocityMapMetersPerSecond -
             agent.velocityMapMetersPerSecond) *
            segment.velocityResponsePerSecond;
    }

    const glm::dvec3 rotationDemand =
        angularDemand(segment, agent);

    if (!finite(linearDemand) || !finite(rotationDemand))
        return Result {};

    result.intent.idealLinearAccelerationDemandMapMps2 =
        toBridge(linearDemand);
    result.intent.idealAngularAccelerationDemandMapRadPerSec2 =
        toBridge(rotationDemand);

    result.status =
        segment.completionTriggersReplan &&
        result.remainingDistanceMeters <=
            segment.completionRadiusMeters
            ? Status::Complete
            : Status::Following;

    return result;
}

} // namespace game::navigation
