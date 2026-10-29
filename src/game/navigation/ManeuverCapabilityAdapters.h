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

// Path-normal acceleration a Newtonian ship provides in routine flight, from
// its vector main authority. How (RCS, nozzle vectoring, main with the hull
// turned) is decided by the pilot. Only a share is planned for curves so a
// curve always leaves authority for braking and corrections.
inline constexpr double NewtonianPreferredLateralShare = 0.5;

[[nodiscard]] inline double newtonianPreferredLateralAccelerationMps2(
    double vectorMainAccelerationMps2
) noexcept
{
    return NewtonianPreferredLateralShare *
        std::max(0.0, vectorMainAccelerationMps2);
}

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

        // Full aft-main remains available on straights and for
        // flip-and-burn. Path-normal acceleration is the ship's preferred
        // routine lateral capability; which actuator delivers it is the
        // pilot's choice, not the trajectory's.
        // Straight-line ramps (step J): the 20 % feedback reserve stays the
        // executor's catch-up headroom and the route profile applies its
        // planning share; the execution share is NOT stacked on top of both
        // for the longitudinal ramps (0.68 instead of 0.61 of the main).
        // Path-normal (curve) authority keeps it: curves keep their margin.
        const double longitudinal =
            game::navigation::newtonian::makeExecutionAuthority(
                params, 1.0).plannedVectorAccelerationMps2;
        out.forwardAccelerationMps2 = longitudinal;
        out.brakingAccelerationMps2 = longitudinal;
        out.lateralAccelerationMps2 =
            newtonianPreferredLateralAccelerationMps2(
                newtonian.plannedVectorAccelerationMps2
            );
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
    // Never manufacture thrust at zero installed authority. Geometry
    // planning must be able to reject an impossible manoeuvre instead of
    // accepting a fictitious 0.1 m/s2 on every missing axis.
    out.forwardAccelerationMps2 =
        std::max(0.0, (forward - reserve) * fraction);
    out.brakingAccelerationMps2 =
        std::max(0.0, (reverse - reserve) * fraction);
    out.lateralAccelerationMps2 =
        std::max(0.0, (lateral - reserve) * fraction);
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

    const auto facts = makeNavigationControlCapability(
        params, defaultLocalFlightControlLaw(), revision);
    const double forwardMain = facts.mainForwardMps2;
    const double reverseMain = facts.mainReverseMps2;
    const double manoeuvre = facts.rcsScalarMps2;

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
    out.maxAngularAccelerationRadPerSec2 = facts.angularScalarRadPerSec2;
    out.maxAngularSpeedRadPerSec = facts.maxAngularSpeedRadPerSec;
    return out;
}

} // namespace game::navigation
