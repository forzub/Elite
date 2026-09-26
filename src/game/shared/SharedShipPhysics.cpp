#include "SharedShipPhysics.h"

#include <algorithm>
#include <cmath>

#include "src/game/ship/ShipController.h"
#include "src/game/navigation/LocalFlightControlStateMachine.h"

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
    evaluateControl(transform, params, control, world, dt);
    propagateOrientation(transform, dt);
}

void evaluateControl(
    ShipTransform& transform,
    const ShipParams& params,
    const ShipControlState& control,
    const WorldParams& world,
    float dt)
{
    auto& motion = transform.motion;

    if (control.localControlLawCommandValid)
    {
        double assistedEntryForwardSpeedMps = motion.forwardSpeedMps;
        if (motion.travelFrame.valid)
        {
            const glm::dvec3 relativeWorldVelocity =
                motion.travelFrame.localToWorldVector(
                    motion.localVelocityMps
                );
            const glm::dvec3 forward(transform.forward());
            const double forwardLength = glm::length(forward);
            if (forwardLength > 1.0e-12)
            {
                assistedEntryForwardSpeedMps = glm::dot(
                    relativeWorldVelocity,
                    forward / forwardLength
                );
            }
        }

        game::navigation::LocalFlightControlStateMachine::transition(
            motion,
            control.requestedLocalControlLaw,
            assistedEntryForwardSpeedMps
        );
    }

    if (control.velocityAlignmentCommand !=
            game::navigation::VelocityAlignmentMode::None)
    {
        (void)game::navigation::LocalFlightControlStateMachine::
            requestVelocityAlignment(
                motion,
                control.velocityAlignmentCommand
            );
    }

    if (control.assistedMaxSpeedCommand)
    {
        (void)game::navigation::LocalFlightControlStateMachine::
            requestAssistedMaximumSpeed(
                motion,
                static_cast<double>(params.maxCombatSpeed)
            );
    }

    // Direct pilot attitude input always wins over an alignment autopilot.
    // A valid navigation acceleration demand also owns attitude for this
    // control sample unless the player supplies material manual attitude input.
    if (hasManualAttitudeInput(control) ||
        control.navigationAccelerationDemandValid)
    {
        (void)game::navigation::LocalFlightControlStateMachine::
            cancelVelocityAlignment(motion);
    }

    // A fresh longitudinal command cancels autobrake only if that command has
    // meaning in the active law. Newtonian '-' remains a no-op.
    const bool manualLongitudinalOverride =
        game::navigation::LocalFlightControlStateMachine::
            longitudinalInputCancelsBrake(
                motion.localControlLaw,
                static_cast<double>(control.targetSpeedRate),
                control.assistedMaxSpeedCommand
            );

    if (manualLongitudinalOverride &&
        motion.velocityAlignmentMode ==
            game::navigation::VelocityAlignmentMode::BrakeToStop)
    {
        (void)game::navigation::LocalFlightControlStateMachine::
            cancelVelocityAlignment(motion);
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
    const bool useNavigationAngularDemand =
        control.navigationAccelerationDemandValid &&
        !hasManualAttitudeInput(control);

    if (useNavigationAngularDemand)
    {
        controller.updateControlRates(
            dt,
            params,
            transform,
            world,
            control.navigationAngularAccelerationDemandSystemRadPerSec2
        );
    }
    else
    {
        controller.updateControlRates(dt, params, transform, world);
    }

    // input state is one-control-evaluation transient. Kinematic orientation
    // propagation below does not need to retain these commands.
    transform.pitchInput      = 0.0f;
    transform.yawInput        = 0.0f;
    transform.rollInput       = 0.0f;
    transform.forwardInput    = 0.0f;
    transform.strafeInput     = 0.0f;
    transform.liftInput       = 0.0f;
    transform.targetSpeedRate = 0.0f;
}

void propagateOrientation(
    ShipTransform& transform,
    float dt
)
{
    ShipController controller;
    controller.propagateOrientation(dt, transform);
}
} // namespace SharedShipPhysics
