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
    void integrate(
        ShipTransform& transform,
        const ShipParams& params,
        const ShipControlState& control,
        const WorldParams& world,
        float dt
    );
}