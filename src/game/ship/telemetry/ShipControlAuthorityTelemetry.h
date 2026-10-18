#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPresetEngineManeuvers.h"

namespace game::ship::telemetry
{
// Read-only PHYSICAL control-authority telemetry, separate from BITE failure
// diagnostics and from actual-red-thrust HUD samples. The autopilot sees the
// same precomputed six-axis limits that the Assisted motor executor uses.
// Basis entries are sign-specific, and include active cooling/impact damage.
struct ShipControlAuthorityTelemetry
{
    bool valid = false;
    game::ship::plant::PhysicalControlModelId model =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    std::uint64_t sourceActuatorRevision = 0;
    std::array<double,6> positive {};
    std::array<double,6> negative {};
    // Indices: body X (strafe), body Y (lift), body Z (reverse positive),
    // angular pitch, yaw, roll. Magnitudes always nonnegative.
};

[[nodiscard]] inline ShipControlAuthorityTelemetry
readAssistedControlAuthority(
    const game::ship::plant::AssistedManeuverPrograms& motorPrograms,
    std::uint64_t actuatorRevision
) noexcept
{
    ShipControlAuthorityTelemetry out;
    if(!motorPrograms.valid) return out;
    out.valid=true;
    out.model=motorPrograms.model;
    out.sourceActuatorRevision=actuatorRevision;
    for(std::size_t axis=0;axis<6;++axis)
    {
        out.positive[axis]=std::max(
            0.0,motorPrograms.programs[2*axis].achievable
        );
        out.negative[axis]=std::max(
            0.0,motorPrograms.programs[2*axis+1].achievable
        );
    }
    return out;
}

[[nodiscard]] inline ShipParams applyAuthorityToControlParams(
    ShipParams p, const ShipControlAuthorityTelemetry& telemetry
) noexcept
{
    if(!telemetry.valid ||
       game::ship::plant::familyOf(telemetry.model)!=
           game::ship::plant::PhysicalControlFamily::Assisted)
        return p;
    // Conservative plan/controller limits: NO value may exceed physical
    // accepted motor-basis capability. Existing ShipParams API is kept as
    // an adapter until navigation directly takes the telemetry structure.
    p.forwardMainEngineAccelerationMps2=static_cast<float>(std::min(
        std::max(0.0,static_cast<double>(
            p.forwardMainEngineAccelerationMps2)),
        telemetry.negative[2] // body -Z is forward
    ));
    p.reverseMainEngineAccelerationMps2=static_cast<float>(std::min(
        std::max(0.0,static_cast<double>(
            p.reverseMainEngineAccelerationMps2)),
        telemetry.positive[2] // body +Z is reverse
    ));
    p.strafeAccel=static_cast<float>(std::min({
        std::max(0.0,static_cast<double>(p.strafeAccel)),
        telemetry.positive[0],telemetry.negative[0],
        telemetry.positive[1],telemetry.negative[1]
    }));
    p.angularAccel=static_cast<float>(std::min({
        std::max(0.0,static_cast<double>(p.angularAccel)),
        telemetry.positive[3],telemetry.negative[3],
        telemetry.positive[4],telemetry.negative[4],
        telemetry.positive[5],telemetry.negative[5]
    }));
    return p;
}
} // namespace game::ship::telemetry
