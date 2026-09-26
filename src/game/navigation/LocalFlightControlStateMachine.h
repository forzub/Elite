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
        motion.assistedStabilizationAccelerationMps2 = glm::dvec3(0.0);

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

    static bool requestVelocityAlignment(
        DynamicMotionState& motion,
        VelocityAlignmentMode requested
    ) noexcept
    {
        if (requested == VelocityAlignmentMode::None)
            return cancelVelocityAlignment(motion);

        // HOME/INSERT are Newtonian vector-orientation actions. END is valid in
        // both laws; Assisted interprets it as a zero-VREL state.
        if (requested != VelocityAlignmentMode::BrakeToStop &&
            motion.localControlLaw != LocalFlightControlLaw::Newtonian)
        {
            return false;
        }

        if (motion.velocityAlignmentMode == requested)
            return false;

        motion.velocityAlignmentMode = requested;
        if (requested == VelocityAlignmentMode::BrakeToStop)
        {
            motion.targetForwardSpeedMps = 0.0;
            motion.assistedTargetSpeedHold = false;
            motion.assistedThrottleTrimWasActive = false;
        }
        return true;
    }

    static bool cancelVelocityAlignment(
        DynamicMotionState& motion
    ) noexcept
    {
        if (motion.velocityAlignmentMode == VelocityAlignmentMode::None)
            return false;
        motion.velocityAlignmentMode = VelocityAlignmentMode::None;
        return true;
    }

    static bool completeVelocityAlignment(
        DynamicMotionState& motion
    ) noexcept
    {
        if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop)
            motion.targetForwardSpeedMps = 0.0;
        return cancelVelocityAlignment(motion);
    }

    static bool updateAssistedLongitudinalTarget(
        DynamicMotionState& motion,
        double trimInput,
        double targetSpeedChangeRateMps2,
        double dtSeconds,
        double reachedForwardSpeedMps,
        double maximumSpeedMps
    ) noexcept
    {
        if (motion.localControlLaw != LocalFlightControlLaw::Assisted)
            return false;

        const double maxSpeed = std::max(
            0.0,
            std::isfinite(maximumSpeedMps) ? maximumSpeedMps : 0.0
        );

        if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop)
        {
            motion.targetForwardSpeedMps = 0.0;
            motion.assistedTargetSpeedHold = false;
            motion.assistedThrottleTrimWasActive = false;
            return true;
        }

        const bool trimActive =
            std::isfinite(trimInput) && std::abs(trimInput) > 1.0e-6;

        if (trimActive)
        {
            motion.assistedTargetSpeedHold = false;
            motion.assistedThrottleTrimWasActive = true;
            motion.targetForwardSpeedMps +=
                trimInput *
                std::max(
                    0.0,
                    std::isfinite(targetSpeedChangeRateMps2)
                        ? targetSpeedChangeRateMps2
                        : 0.0
                ) *
                std::max(
                    0.0,
                    std::isfinite(dtSeconds) ? dtSeconds : 0.0
                );
        }
        else if (motion.assistedThrottleTrimWasActive)
        {
            // Capture exactly once on the +/- release edge. Manual body-axis
            // RCS and external impulses never rewrite the longitudinal setpoint.
            motion.targetForwardSpeedMps = std::max(
                0.0,
                std::isfinite(reachedForwardSpeedMps)
                    ? reachedForwardSpeedMps
                    : 0.0
            );
            motion.assistedThrottleTrimWasActive = false;
        }

        motion.targetForwardSpeedMps = std::clamp(
            motion.targetForwardSpeedMps,
            0.0,
            maxSpeed
        );
        return true;
    }

    static bool requestAssistedMaximumSpeed(
        DynamicMotionState& motion,
        double maximumSpeedMps
    ) noexcept
    {
        if (motion.localControlLaw != LocalFlightControlLaw::Assisted ||
            !std::isfinite(maximumSpeedMps))
        {
            return false;
        }

        motion.targetForwardSpeedMps = std::max(0.0, maximumSpeedMps);
        motion.assistedTargetSpeedHold = true;
        motion.assistedThrottleTrimWasActive = false;
        cancelVelocityAlignment(motion);
        return true;
    }

    static constexpr bool longitudinalInputCancelsBrake(
        LocalFlightControlLaw law,
        double targetSpeedRate,
        bool assistedMaxSpeedCommand
    ) noexcept
    {
        return law == LocalFlightControlLaw::Newtonian
            ? targetSpeedRate > 0.001
            : std::abs(targetSpeedRate) > 0.001 ||
                assistedMaxSpeedCommand;
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
