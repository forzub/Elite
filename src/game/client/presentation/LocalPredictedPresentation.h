#pragma once

#include <algorithm>
#include <vector>

#include "src/game/client/ClientHubTacticalPrediction.h"
#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/world/WorldParams.h"

namespace game::client::presentation
{

/*
    Build a presentation-only sample between two fixed prediction ticks.

    Rendering can run faster than fixed prediction ticks. Copy the latest
    server-based kinematic prediction and extrapolate only that copy by the
    accumulator remainder, using the SAME rates/velocity as the fixed path.
    No client physics or engine allocator is ever invoked.
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

    // Presentation copy only. No attitude-control or motor-force replay;
    // the server has already solved both in its authoritative snapshot.
    (void)shipParams;
    (void)control;
    (void)world;
    (void)game::client::advanceReplicatedHubKinematics(
        target,referenceFrame,dt);

    return target;
}


inline ShipTransform sampleLocalPredictedPresentationTarget(
    const ShipTransform& fixedPredictedTransform,
    const game::simulation::ShipReferenceFrameSnapshot& referenceFrame,
    const ShipParams& shipParams,
    const ShipControlState& control,
    const WorldParams& world,
    game::ship::plant::PhysicalControlModelId model,
    const std::vector<game::ship::plant::EngineSlotState>& slotState,
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

    (void)shipParams;
    (void)control;
    (void)world;
    (void)model;
    (void)slotState;
    (void)game::client::advanceReplicatedHubKinematics(
        target,referenceFrame,dt);


    return target;
}

} // namespace game::client::presentation
