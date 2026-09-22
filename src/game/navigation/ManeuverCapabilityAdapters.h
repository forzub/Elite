#pragma once

#include <algorithm>
#include <cstdint>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation
{

[[nodiscard]] inline AcceptedManeuverProgram::CapabilitySnapshot
makeManeuverCapabilitySnapshot(
    const ShipParams& params,
    std::uint64_t revision
) noexcept
{
    AcceptedManeuverProgram::CapabilitySnapshot out;
    out.revision = revision;

    const double forwardMain =
        game::ship::forwardMainAccelerationLimitMps2(params);
    const double reverseMain =
        game::ship::reverseMainAccelerationLimitMps2(params);
    const double manoeuvre =
        game::ship::manoeuvreAccelerationLimitMps2(params);

    // These are instantaneous, attitude-preserving axis capabilities.
    // Flip-and-burn is not encoded as "reverse acceleration" because it
    // requires a separate attitude maneuver and time proof.
    out.maxForwardAccelerationMetersPerSec2 =
        std::max(forwardMain, manoeuvre);
    out.maxReverseAccelerationMetersPerSec2 =
        std::max(reverseMain, manoeuvre);
    out.maxLateralAccelerationMetersPerSec2 =
        manoeuvre;
    out.maxVerticalAccelerationMetersPerSec2 =
        manoeuvre;
    out.maxAngularAccelerationRadPerSec2 =
        game::ship::angularAccelerationLimitRadPerSec2(params);
    out.maxAngularSpeedRadPerSec =
        game::ship::maximumAngularSpeedRadPerSec(params);
    return out;
}

} // namespace game::navigation
