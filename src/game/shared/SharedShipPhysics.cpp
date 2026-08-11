#include "SharedShipPhysics.h"

#include <cmath>

#include "src/game/ship/ShipController.h"

namespace SharedShipPhysics
{
namespace
{
bool hasManualAttitudeInput(const ShipControlState& control)
{
    return
        std::abs(control.pitchInput) > 0.001f ||
        std::abs(control.yawInput) > 0.001f ||
        std::abs(control.rollInput) > 0.001f;
}
}

void integrate(
    ShipTransform& transform,
    const ShipParams& params,
    const ShipControlState& control,
    const WorldParams& world,
    float dt)
{
    auto& motion = transform.motion;

    if (control.localControlLawCommandValid)
    {
        const auto oldLaw = motion.localControlLaw;
        motion.localControlLaw = control.requestedLocalControlLaw;
        motion.velocityAlignmentMode =
            game::navigation::VelocityAlignmentMode::None;

        // Enter Assisted without changing physical velocity. The target is
        // initialized from the current local speed so the controller does not
        // create an artificial braking/acceleration impulse on mode switch.
        if (oldLaw != motion.localControlLaw &&
            motion.localControlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted)
        {
            motion.targetForwardSpeedMps =
                glm::length(motion.localVelocityMps);
        }
    }

    if (control.velocityAlignmentCommand !=
            game::navigation::VelocityAlignmentMode::None)
    {
        // HOME/INSERT are Newtonian vector-orientation tools. END is valid in
        // both laws: Newtonian aligns tail-to-velocity and brakes; Assisted
        // uses its velocity controller to bring VREL to zero.
        if (control.velocityAlignmentCommand ==
                game::navigation::VelocityAlignmentMode::BrakeToStop ||
            motion.localControlLaw ==
                game::navigation::LocalFlightControlLaw::Newtonian)
        {
            motion.velocityAlignmentMode =
                control.velocityAlignmentCommand;
        }
    }

    // Direct pilot attitude input always wins over an alignment autopilot.
    if (hasManualAttitudeInput(control))
    {
        motion.velocityAlignmentMode =
            game::navigation::VelocityAlignmentMode::None;
    }

    // A fresh +/- command cancels an existing autobrake action. The pilot has
    // explicitly taken longitudinal control again.
    if (std::abs(control.targetSpeedRate) > 0.001f &&
        motion.velocityAlignmentMode ==
            game::navigation::VelocityAlignmentMode::BrakeToStop)
    {
        motion.velocityAlignmentMode =
            game::navigation::VelocityAlignmentMode::None;
    }

    // control -> transform input state
    transform.pitchInput      = control.pitchInput;
    transform.yawInput        = control.yawInput;
    transform.rollInput       = control.rollInput;
    transform.forwardInput    = control.forwardInput;
    transform.strafeInput     = control.strafeInput;
    transform.liftInput       = control.liftInput;
    transform.targetSpeedRate = control.targetSpeedRate;
    transform.cruiseActive    = control.cruiseActive;
    transform.jumpActive      = control.jumpActive;

    ShipController controller;
    controller.update(dt, params, transform, world);

    // input state is one-frame transient
    transform.pitchInput      = 0.0f;
    transform.yawInput        = 0.0f;
    transform.rollInput       = 0.0f;
    transform.forwardInput    = 0.0f;
    transform.strafeInput     = 0.0f;
    transform.liftInput       = 0.0f;
    transform.targetSpeedRate = 0.0f;
}
} // namespace SharedShipPhysics
