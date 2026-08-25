#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace world::navigation
{

enum class TrajectoryStatus : std::uint8_t
{
    Ready = 0,
    InvalidRequest,
    NoSafePath,
    InitialStateInfeasible,
    NumericalFailure
};

/*
    Time-parameterized motion in one caller-owned planning frame.

    position/velocity/acceleration are NOT implicitly world-space. frameId and
    the caller's planning epoch define the reference frame. A Hub trajectory is
    therefore naturally Hub-local and can later be resolved through the same
    predicted Hub frame at each sample time without baking orbital tangent
    motion into the route itself.
*/
struct TrajectorySample
{
    double universeTimeSeconds = 0.0;
    double timeOffsetSeconds = 0.0;

    double pathProgressMeters = 0.0;
    double sourcePathProgressMeters = 0.0;

    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 accelerationMps2 {0.0};

    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
    glm::dvec3 angularVelocityRadPerSecond {0.0};

    double speedMps = 0.0;
};

struct Trajectory
{
    TrajectoryStatus status = TrajectoryStatus::InvalidRequest;
    int systemId = -1;
    std::string frameId;
    std::string message;

    double startUniverseTimeSeconds = 0.0;
    double durationSeconds = 0.0;
    double lengthMeters = 0.0;

    std::vector<TrajectorySample> samples;

    bool ready() const noexcept
    {
        return status == TrajectoryStatus::Ready &&
            systemId >= 0 && !frameId.empty() && samples.size() >= 2 &&
            std::isfinite(startUniverseTimeSeconds) &&
            std::isfinite(durationSeconds) && durationSeconds >= 0.0;
    }
};

} // namespace world::navigation
