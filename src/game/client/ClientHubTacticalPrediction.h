#pragma once

#include <vector>

#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"

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

// Damaged Assisted prediction must not synthesize engine wrench from
// slot-only coefficients. Advance the last replicated kinematic state
// (pose/rates/velocity), without solving thrust or re-running controls.
// Shared by fixed client ticks AND the fractional render sample.
inline bool advanceReplicatedHubKinematics(
    ShipTransform& transform,
    const game::simulation::ShipReferenceFrameSnapshot& frameSnapshot,
    float dt
)
{
    if (!canPredictHubTacticalMotion(transform,frameSnapshot) ||
        !std::isfinite(dt) || dt<=0.0f)
        return false;

    game::ship::control::ShipControlExecutionGateway::
        propagateOrientation(transform,dt);

    auto& motion=transform.motion;
    motion.localPositionMeters+=
        motion.localVelocityMps*static_cast<double>(dt);

    const auto frame=travelFrameForPrediction(frameSnapshot);
    motion.travelFrame=frame;
    transform.setWorldPositionMeters(
        frame.localToWorldPosition(motion.localPositionMeters));
    motion.worldVelocityMps=frame.localToWorldVelocity(
        motion.localPositionMeters,motion.localVelocityMps);
    motion.referenceVelocityMps=frame.localToWorldVelocity(
        motion.localPositionMeters,glm::dvec3(0.0));
    transform.referenceVelocityMetersPerSecond=
        motion.referenceVelocityMps;
    return true;
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

    (void)game::ship::control::ShipControlExecutionGateway::
        evaluateTranslationControl(
            transform,
            params,
            control,
            dt
        );

    const bool propagated =
        game::ship::control::ShipControlExecutionGateway::
            propagateHubTacticalTranslation(
                transform,
                params,
                static_cast<double>(dt)
            );

    if (propagated)
    {
        transform.referenceVelocityMetersPerSecond =
            transform.motion.referenceVelocityMps;
    }

    return propagated;
}


inline bool predictHubTacticalMotion(
    ShipTransform& transform,
    const game::simulation::ShipReferenceFrameSnapshot& frameSnapshot,
    const ShipParams& params,
    const ShipControlState& control,
    game::ship::plant::PhysicalControlModelId model,
    const std::vector<game::ship::plant::EngineSlotState>& slotState,
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

    (void)game::ship::control::ShipControlExecutionGateway::
        evaluateTranslationControl(
            transform,
            params,
            control,
            model,
            slotState,
            dt
        );

    const bool propagated =
        game::ship::control::ShipControlExecutionGateway::
            propagateHubTacticalTranslation(
                transform,
                params,
                static_cast<double>(dt)
            );

    if (propagated)
    {
        transform.referenceVelocityMetersPerSecond =
            transform.motion.referenceVelocityMps;
    }

    return propagated;
}

} // namespace game::client
