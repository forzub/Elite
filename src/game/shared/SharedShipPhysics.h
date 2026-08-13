// #pragma once

// #include <glm/glm.hpp>
// #include <algorithm>

// #include <iostream>

// #include "src/game/ship/core/ShipTransform.h"
// #include "src/game/ship/core/ShipControlState.h"
// #include "src/world/WorldParams.h"

// #include "src/game/ship/ShipController.h"

// namespace SharedShipPhysics{
//     void integrate(
//         ShipTransform& transform,
//         const ShipParams& params,
//         const ShipControlState& control,
//         const WorldParams& world,
//         float dt);
   
// }



#pragma once

#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/WorldParams.h"

namespace SharedShipPhysics
{
    // Established full fixed-step wrapper. Client prediction keeps using this
    // path so Stage 4B changes only server-side activation cost.
    void integrate(
        ShipTransform& transform,
        const ShipParams& params,
        const ShipControlState& control,
        const WorldParams& world,
        float dt
    );

    // Expensive control/rate evaluation may run at a lower cadence for
    // Prewarm/Coarse server entities.
    void evaluateControl(
        ShipTransform& transform,
        const ShipParams& params,
        const ShipControlState& control,
        const WorldParams& world,
        float dt
    );

    // Cheap orientation kinematics stays fixed-step so snapshots remain
    // continuous even while the control solver is decimated.
    void propagateOrientation(
        ShipTransform& transform,
        float dt
    );
}