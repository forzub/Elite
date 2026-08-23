#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

namespace game::radar
{

struct RadarTrackId
{
    std::uint64_t value = 0;

    explicit operator bool() const noexcept { return value != 0; }

    friend bool operator==(RadarTrackId a, RadarTrackId b) noexcept
    {
        return a.value == b.value;
    }

    friend bool operator!=(RadarTrackId a, RadarTrackId b) noexcept
    {
        return !(a == b);
    }
};

struct RadarTrackReport
{
    RadarTrackId trackId {};

    // Sensor-local Cartesian state at measuredAtUniverseTimeSeconds.
    glm::dvec3 relativePositionMeters {0.0};
    glm::dvec3 relativeVelocityMps {0.0};

    double rangeMeters = 0.0;
    double positionUncertaintyMeters = 0.0;
    double velocityUncertaintyMps = 0.0;
    double confidence = 0.0;
};

struct RadarScanReport
{
    std::uint64_t scanSequence = 0;

    // The physical state belongs to measuredAt. availableAt models processing
    // latency and is the earliest universe epoch at which consumers may use it.
    double measuredAtUniverseTimeSeconds = 0.0;
    double availableAtUniverseTimeSeconds = 0.0;

    std::vector<RadarTrackReport> tracks;
};

} // namespace game::radar
