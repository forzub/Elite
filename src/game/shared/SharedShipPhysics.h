#pragma once

#include <glm/glm.hpp>
#include <algorithm>

#include <iostream>

#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/world/WorldParams.h"

#include "src/game/ship/ShipController.h"

namespace SharedShipPhysics{
    void integrate(
        ShipTransform& transform,
        const ShipParams& params,
        const ShipControlState& control,
        const WorldParams& world,
        float dt);
   
}
// class SharedShipPhysics
// {
// public:

//     static void integrate(
//         ShipTransform& transform,
//         const ShipControlState& control,
//         const WorldParams& world,
//         float dt)
//     {
        

//         // === INPUT → transform ===
//         transform.pitchInput   = control.pitchInput;
//         transform.yawInput     = control.yawInput;
//         transform.rollInput    = control.rollInput;
//         transform.forwardInput = control.forwardInput;
//         transform.strafeInput  = control.strafeInput;
//         transform.liftInput    = control.liftInput;
//         transform.cruiseActive = control.cruiseActive;
//         transform.targetSpeedRate = control.targetSpeedRate;

//         const float accel = 100.0f;

//         // === TARGET SPEED LOGIC ===
//         transform.targetSpeed += control.targetSpeedRate * 50.0f * dt;
//         transform.targetSpeed =
//             glm::clamp(transform.targetSpeed, 0.0f, 500.0f);

//         if (transform.forwardVelocity < transform.targetSpeed)
//             transform.forwardVelocity += accel * dt;
//         else if (transform.forwardVelocity > transform.targetSpeed)
//             transform.forwardVelocity -= accel * dt;

//         // === MANOEUVRE THRUSTERS ===
//         transform.forwardVelocity +=
//             transform.forwardInput * accel * dt;

//         // drag
//         transform.forwardVelocity *=
//             (1.0f - world.linearDrag * dt);

//         // integrate position
//         transform.position +=
//             transform.forward() *
//             transform.forwardVelocity *
//             dt;

        
//     }
// };
