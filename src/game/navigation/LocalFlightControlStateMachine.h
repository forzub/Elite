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
        return LocalFlightControlLaw::Assisted;
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
        LocalFlightControlLaw requested
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

        // Assisted enters with the currently achieved VREL magnitude as its
        // target. It therefore begins from continuity instead of producing a
        // synthetic acceleration/braking impulse on the transition frame.
        if (requested == LocalFlightControlLaw::Assisted)
        {
            motion.targetForwardSpeedMps =
                std::max(0.0, glm::length(motion.localVelocityMps));
        }

        return true;
    }

    static constexpr bool usesAssistedVelocityController(
        LocalFlightControlLaw law
    ) noexcept
    {
        return law == LocalFlightControlLaw::Assisted;
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
