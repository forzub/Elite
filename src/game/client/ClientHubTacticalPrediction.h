#pragma once

#include <cmath>
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

// No client should synthesize engine wrench from slot-only coefficients.
// Advance the last replicated kinematic state
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

// Legacy prediction entry points are retained for source compatibility,
// but deliberately have NO local engine/actuator physics. Both use the same
// authoritative-state kinematic extrapolator as active ClientWorldState.
inline bool predictHubTacticalMotion(
    ShipTransform& transform,
    const game::simulation::ShipReferenceFrameSnapshot& frameSnapshot,
    const ShipParams& params,
    const ShipControlState& control,
    float dt
)
{
    (void)params;
    (void)control;
    return advanceReplicatedHubKinematics(transform,frameSnapshot,dt);
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
    (void)params;
    (void)control;
    (void)model;
    (void)slotState;
    return advanceReplicatedHubKinematics(transform,frameSnapshot,dt);
}

} // namespace game::client
