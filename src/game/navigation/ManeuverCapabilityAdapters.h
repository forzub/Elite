#pragma once

#include <algorithm>
#include <cstdint>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/newtonian/NewtonianExecutionAuthority.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation
{

struct ManeuverExecutionAuthority
{
    double forwardAccelerationMps2 = 0.0;
    double brakingAccelerationMps2 = 0.0;
    double lateralAccelerationMps2 = 0.0;
    double feedbackReserveMps2 = 0.0;
};

[[nodiscard]] inline ManeuverExecutionAuthority
makeManeuverExecutionAuthority(
    const ShipParams& params,
    LocalFlightControlLaw law,
    double executionFraction = 0.90
) noexcept
{
    const double fraction =
        std::clamp(executionFraction, 0.0, 1.0);

    if (law == LocalFlightControlLaw::Newtonian)
    {
        const auto newtonian =
            game::navigation::newtonian::makeExecutionAuthority(
                params,
                fraction
            );

        ManeuverExecutionAuthority out;
        out.feedbackReserveMps2 =
            newtonian.feedbackReserveMps2;

        // The route acceleration vector is supplied by the aft main after the
        // hull/TVC solver orients the thrust axis. "Lateral" here is therefore
        // path-normal acceleration authority, not a body-axis RCS channel.
        out.forwardAccelerationMps2 =
            newtonian.plannedVectorAccelerationMps2;
        out.brakingAccelerationMps2 =
            newtonian.plannedVectorAccelerationMps2;
        out.lateralAccelerationMps2 =
            newtonian.plannedVectorAccelerationMps2;
        return out;
    }

    // Assisted keeps its existing independent longitudinal-main and automatic
    // lateral-stabilization envelopes. Nothing in the Newtonian authority
    // calculation is allowed to alter this path.
    const double forward =
        std::max(
            0.0,
            game::ship::forwardMainAccelerationLimitMps2(params)
        );
    const double reverse =
        std::max(
            0.0,
            game::ship::reverseMainAccelerationLimitMps2(params)
        );
    const double lateral =
        std::max(
            0.0,
            game::ship::
                assistedLateralStabilizationAccelerationLimitMps2(params)
        );

    const double reserve =
        DockingAutomaticRecoveryPolicy::linearFeedbackReserveMps2(
            forward,
            reverse,
            lateral
        );

    ManeuverExecutionAuthority out;
    out.feedbackReserveMps2 = reserve;
    out.forwardAccelerationMps2 =
        std::max(0.1, (forward - reserve) * fraction);
    out.brakingAccelerationMps2 =
        std::max(0.1, (reverse - reserve) * fraction);
    out.lateralAccelerationMps2 =
        std::max(0.1, (lateral - reserve) * fraction);
    return out;
}

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
    out.maxForwardMainAccelerationMetersPerSec2 = forwardMain;
    out.maxReverseMainAccelerationMetersPerSec2 = reverseMain;
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
