#include "SharedShipPhysics.h"
#include "src/game/ship/ShipController.h"

namespace SharedShipPhysics
{
    void integrate(
        ShipTransform& transform,
        const ShipParams& params,
        const ShipControlState& control,
        const WorldParams& world,
        float dt)
    {
        transform.pitchInput      = control.pitchInput;
        transform.yawInput        = control.yawInput;
        transform.rollInput       = control.rollInput;
        transform.forwardInput    = control.forwardInput;
        transform.strafeInput     = control.strafeInput;
        transform.liftInput       = control.liftInput;
        transform.targetSpeedRate = control.targetSpeedRate;
        transform.cruiseActive    = control.cruiseActive;

        ShipController controller;
        controller.update(dt, params, transform, world);

        // После физики input обнуляется
        transform.pitchInput      = 0.0f;
        transform.yawInput        = 0.0f;
        transform.rollInput       = 0.0f;
        transform.forwardInput    = 0.0f;
        transform.strafeInput     = 0.0f;
        transform.liftInput       = 0.0f;
        transform.targetSpeedRate = 0.0f;
    }
}