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
    const double omegaRate=glm::length(motion.deckOmegaLocal);
    if(motion.deckActive && omegaRate>1.0e-9)
    {
        // over / on a TURNING deck (a ring city; DeckFrame.h): the ship goes round with the deck - its place relative
        // to the deck's axis turned by omega dt, plus its own velocity relative to the deck (also turned). A straight
        // line here left the deck's arc by omega^2 r t^2 / 2 (~5 cm at a ring's rim in 0.1 s) and every snapshot
        // pulled it back: the jitter of a ship standing on a rim pad
        const double h=static_cast<double>(dt);
        const glm::dvec3 axis=motion.deckOmegaLocal/omegaRate;
        const glm::dvec3 r=motion.localPositionMeters-motion.deckAxisPointLocal;
        const glm::dvec3 u=motion.localVelocityMps-glm::cross(motion.deckOmegaLocal,r);
        const auto turn=[&](const glm::dvec3& x){
            const double a=omegaRate*h, c=std::cos(a), s=std::sin(a);
            return x*c+glm::cross(axis,x)*s+axis*glm::dot(axis,x)*(1.0-c);
        };
        const glm::dvec3 r1=turn(r)+turn(u)*h;
        motion.localPositionMeters=motion.deckAxisPointLocal+r1;
        motion.localVelocityMps=glm::cross(motion.deckOmegaLocal,r1)+turn(u);
    }
    else
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
    transform.motion.referenceVelocityMps=
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
