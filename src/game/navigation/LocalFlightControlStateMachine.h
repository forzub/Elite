#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/navigation/DynamicMotionState.h"

namespace game::navigation
{

// Single transition owner for the persistent local-flight control law.
// Input/UI code may request a law, but only this state machine defines what
// entering/leaving that law does to the persistent DynamicMotionState.
class LocalFlightControlStateMachine
{
public:
    static constexpr LocalFlightControlLaw defaultLaw() noexcept
    {
        return defaultLocalFlightControlLaw();
    }

    static constexpr LocalFlightControlLaw next(
        LocalFlightControlLaw current
    ) noexcept
    {
        return current == LocalFlightControlLaw::Assisted
            ? LocalFlightControlLaw::Newtonian
            : LocalFlightControlLaw::Assisted;
    }

    static bool transition(
        DynamicMotionState& motion,
        LocalFlightControlLaw requested,
        double assistedEntryForwardSpeedMps
    ) noexcept
    {
        if (requested != LocalFlightControlLaw::Assisted &&
            requested != LocalFlightControlLaw::Newtonian)
        {
            return false;
        }

        if (motion.localControlLaw == requested)
            return false;

        motion.localControlLaw = requested;

        // A law transition terminates any doctrine-specific persistent action.
        // Physical velocity/orientation are never rewritten.
        motion.velocityAlignmentMode = VelocityAlignmentMode::None;
        motion.assistedTargetSpeedHold = false;
        motion.assistedThrottleTrimWasActive = false;

        // Assisted owns a FORWARD-speed setpoint, not total |VREL|. Capturing
        // total speed here used to reinterpret sideways Newtonian drift as a
        // new commanded forward speed and made the two laws feel misleadingly
        // similar after a mode switch.
        if (requested == LocalFlightControlLaw::Assisted)
        {
            motion.targetForwardSpeedMps =
                std::max(
                    0.0,
                    std::isfinite(assistedEntryForwardSpeedMps)
                        ? assistedEntryForwardSpeedMps
                        : 0.0
                );
        }

        return true;
    }

    static bool transition(
        DynamicMotionState& motion,
        LocalFlightControlLaw requested
    ) noexcept
    {
        return transition(
            motion,
            requested,
            motion.forwardSpeedMps
        );
    }

    static constexpr bool usesAssistedVelocityController(
        LocalFlightControlLaw law
    ) noexcept
    {
        return law == LocalFlightControlLaw::Assisted;
    }

    static constexpr bool neutralAngularDampingEnabled(
        LocalFlightControlLaw law,
        VelocityAlignmentMode alignmentMode
    ) noexcept
    {
        // Assisted flight automatically arrests pilot-induced angular motion.
        // Newtonian flight preserves angular inertia unless a persistent
        // alignment/autobrake action explicitly owns attitude.
        return law == LocalFlightControlLaw::Assisted ||
            alignmentMode != VelocityAlignmentMode::None;
    }

    static constexpr bool velocityAlignmentOwnsAttitude(
        LocalFlightControlLaw law,
        VelocityAlignmentMode mode,
        bool forwardMainAvailable,
        bool reverseMainAvailable
    ) noexcept
    {
        if (mode == VelocityAlignmentMode::BrakeToStop)
        {
            // Healthy Assisted braking uses fore/reverse main directly and
            // does not rotate the hull. Only loss of reverse-main authority
            // promotes aft-main flip-and-burn to an Assisted fallback.
            if (law == LocalFlightControlLaw::Assisted)
                return !reverseMainAvailable && forwardMainAvailable;

            return forwardMainAvailable || reverseMainAvailable;
        }

        return law == LocalFlightControlLaw::Newtonian &&
            (mode == VelocityAlignmentMode::ForwardToVelocity ||
             mode == VelocityAlignmentMode::BackwardToVelocity);
    }
};

} // namespace game::navigation
