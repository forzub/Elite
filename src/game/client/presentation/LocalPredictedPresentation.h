#pragma once

#include <algorithm>

#include "src/game/client/ClientHubTacticalPrediction.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/world/WorldParams.h"

namespace game::client::presentation
{

/*
    Build a presentation-only sample between two fixed prediction ticks.

    Client prediction remains authoritative only at fixed simulation ticks.
    Rendering, however, may run faster than that fixed rate. Rendering the
    latest fixed predicted transform directly therefore feeds the camera a
    staircase (50 Hz in the current runtime) and then asks an exponential
    smoother to hide it.

    Instead, copy the latest fixed predicted state and advance only the copy by
    the accumulator remainder. At the next fixed tick the fixed state advances
    by the same deterministic equations and the remainder wraps, so this target
    is continuous without mutating prediction/reconciliation history.
*/
inline ShipTransform sampleLocalPredictedPresentationTarget(
    const ShipTransform& fixedPredictedTransform,
    const game::simulation::ShipReferenceFrameSnapshot& referenceFrame,
    const ShipParams& shipParams,
    const ShipControlState& control,
    const WorldParams& world,
    float fractionalStepSeconds,
    float fixedStepSeconds
)
{
    ShipTransform target = fixedPredictedTransform;

    const float maxFraction =
        std::max(0.0f, fixedStepSeconds);

    const float dt =
        std::clamp(
            fractionalStepSeconds,
            0.0f,
            maxFraction
        );

    if (dt <= 0.0f)
        return target;

    // Attitude uses exactly the same deterministic controller as fixed-step
    // prediction; this mutates only the presentation copy.
    SharedShipPhysics::integrate(
        target,
        shipParams,
        control,
        world,
        dt
    );

    // Hub-local translation must use the same DynamicMotionSystem equations as
    // both server simulation and fixed client prediction.
    (void)game::client::predictHubTacticalMotion(
        target,
        referenceFrame,
        control,
        dt
    );

    return target;
}

} // namespace game::client::presentation
