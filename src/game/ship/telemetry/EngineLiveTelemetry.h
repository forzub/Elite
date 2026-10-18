#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

namespace game::ship::telemetry
{
// Runtime instrumentation, deliberately separate from BITE diagnostics.
// These samples come ONLY from physically executed Engine outputs. No UI or
// client code is allowed to reconstruct thrust from movement or pilot input.
struct EngineLiveTelemetry
{
    static constexpr std::size_t Capacity = 16;
    bool valid = false;
    std::uint8_t count = 0;
    std::uint64_t controlTick = 0;
    std::array<double, Capacity> thrust01 {};
    std::array<double, Capacity> available01 {};
    std::array<double, Capacity> nozzleYawRad {};
    std::array<double, Capacity> nozzlePitchRad {};
    std::array<glm::dvec3, Capacity> forceBodyNewtons {};
    std::array<double, Capacity> electricalMW {};
    std::array<double, Capacity> wasteHeatMW {};

    void reset(std::uint64_t tick) noexcept
    {
        *this = {};
        controlTick = tick;
    }
};
} // namespace game::ship::telemetry
