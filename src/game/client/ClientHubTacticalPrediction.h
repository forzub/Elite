#pragma once

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/game/ship/core/ShipControlState.h"
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
        !transform.motion.hubId.empty() &&
        transform.motion.hubId == frame.hubId;
}

inline game::navigation::HubNavigationFrame
hubNavigationFrameForPrediction(
    const game::simulation::ShipReferenceFrameSnapshot& source
)
{
    game::navigation::HubNavigationFrame frame;
    frame.systemId = source.systemId;
    frame.hubId = source.hubId;
    frame.parentBodyId = source.bodyId;
    frame.primeModuleId = source.moduleId;
    frame.originMeters = source.originMeters;
    frame.velocityMetersPerSecond = source.velocityMetersPerSecond;
    frame.angularVelocityWorldRadPerSecond =
        source.angularVelocityWorldRadPerSecond;
    frame.radialAxis = source.radialAxis;
    frame.progradeAxis = source.progradeAxis;
    frame.normalAxis = source.normalAxis;
    frame.valid = source.valid;
    return frame;
}

inline bool predictHubTacticalMotion(
    ShipTransform& transform,
    const game::simulation::ShipReferenceFrameSnapshot& frameSnapshot,
    const ShipControlState& control,
    float dt
)
{
    if (!canPredictHubTacticalMotion(transform, frameSnapshot))
        return false;

    const game::navigation::HubNavigationFrame frame =
        hubNavigationFrameForPrediction(frameSnapshot);

    game::navigation::DynamicMotionSystem::applyHubTacticalInput(
        transform.motion,
        frame,
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

    game::navigation::DynamicMotionSystem::updateHubTactical(
        transform.motion,
        transform.worldPosition,
        frame,
        static_cast<double>(dt)
    );

    transform.referenceVelocityMetersPerSecond =
        transform.motion.referenceVelocityMps;

    return true;
}

} // namespace game::client
