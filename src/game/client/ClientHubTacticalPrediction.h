#pragma once

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"

namespace game::client
{

inline bool canPredictHubTacticalMotion(
    const ShipTransform& transform,
    const game::simulation::ShipReferenceFrameSnapshot& frame
) noexcept
{
    return
        transform.motion.mode == game::navigation::MotionMode::HubTactical &&
        frame.valid &&
        transform.motion.systemId >= 0 &&
        transform.motion.systemId == frame.systemId &&
        transform.motion.travelFrame.valid &&
        !transform.motion.travelFrame.frameId.empty() &&
        transform.motion.travelFrame.frameId ==
            (frame.frameId.empty() ? frame.hubId : frame.frameId);
}

inline game::navigation::KinematicFrame
travelFrameForPrediction(
    const game::simulation::ShipReferenceFrameSnapshot& source
)
{
    return source.kinematicFrame();
}

inline bool predictHubTacticalMotion(
    ShipTransform& transform,
    const game::simulation::ShipReferenceFrameSnapshot& frameSnapshot,
    const ShipParams& params,
    const ShipControlState& control,
    float dt
)
{
    if (!canPredictHubTacticalMotion(transform, frameSnapshot))
        return false;

    const game::navigation::KinematicFrame frame =
        travelFrameForPrediction(frameSnapshot);

    transform.motion.travelFrame = frame;
    transform.motion.matchedToReferenceFrame =
        frameSnapshot.matchedToReferenceFrame;

    game::navigation::DynamicMotionSystem::applyLocalFrameInput(
        transform.motion,
        frame,
        params,
        dt,
        control.targetSpeedRate,
        control.cruiseActive,
        control.forwardInput,
        control.liftInput,
        control.strafeInput,
        transform.forward(),
        transform.right(),
        transform.up()
    );

    game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
        transform.motion,
        transform.worldPosition,
        frame,
        params,
        static_cast<double>(dt)
    );

    transform.referenceVelocityMetersPerSecond =
        transform.motion.referenceVelocityMps;

    return true;
}

} // namespace game::client
