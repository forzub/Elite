#include "ShipInstance.h"

void ShipInstance::update(
    float dt,
    const WorldParams& world
)
{
    // -------------------------------------------------
    // 1. Передаём intent → физическое состояние
    // -------------------------------------------------
    transform.cruiseActive      = control.cruiseActive;
    transform.pitchInput        = control.pitchInput;
    transform.yawInput          = control.yawInput;
    transform.rollInput         = control.rollInput;
    transform.targetSpeedRate   = control.targetSpeedRate;
    transform.strafeInput       = control.strafeInput;
    transform.liftInput         = control.liftInput;
    transform.forwardInput      = control.forwardInput;

    // -------------------------------------------------
    // 2. Физика корабля
    // -------------------------------------------------
    controller.update(
        dt,
        desc->physics,
        transform,
        world
    );
}
