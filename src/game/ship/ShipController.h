#pragma once

#include "game/ship/core/ShipParams.h"
#include "game/ship/core/ShipTransform.h"
#include "world/WorldParams.h"

class ShipController
{
public:
    // Backward-compatible full fixed-step path used by client prediction and
    // other callers that require the established motion semantics.
    void update(
        float dt,
        const ShipParams& params,
        ShipTransform& ship,
        const WorldParams& world
    );

    // Stage 4B separates expensive control/rate evaluation from cheap
    // kinematic orientation propagation. Active mode still calls both every
    // fixed tick; Prewarm/Coarse may hold the last angular rates between
    // control evaluations while orientation remains continuously propagated.
    void updateControlRates(
        float dt,
        const ShipParams& params,
        ShipTransform& ship,
        const WorldParams& world
    );

    void propagateOrientation(
        float dt,
        ShipTransform& ship
    );
};