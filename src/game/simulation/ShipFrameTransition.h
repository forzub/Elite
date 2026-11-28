#pragma once

// THE ONE FRAME CHANGE FOR AN EXISTING SHIP (core stabilization D-1, 2026-11-28).
//
// A change of reference frame changes only the frame the motion is DESCRIBED in. The physical state is untouched:
// world position, world velocity, attitude and the angular rates are the same before and after. Quantities that are
// defined RELATIVE to the frame are re-based to what the ship already does in the new frame, so no regulator acts on a
// stale setpoint:
// - the relative body speeds (forward / strafe / lift);
// - the Assisted speed setpoint (hold off, setpoint = the current forward speed, the same rule as the Ctrl+J switches);
// - the desired velocities.
// The velocity-alignment mode and the control law are kept.
//
// A failed transition (invalid frame, another system, a non-finite state) changes NOTHING.
//
// Spawning a NEW ship into a frame is a different operation: GameSimulation::placeShipInReferenceFrame.

#include <cmath>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/TravelFrameSystem.h"
#include "src/game/ship/core/ShipTransform.h"

namespace game::simulation
{

inline bool isFiniteVector(const glm::dvec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// The ship's world kinematics as they stand. Inside a valid travel frame the local state is authoritative
// (TravelFrameSystem); otherwise the world fields are.
inline game::navigation::WorldKinematicState currentWorldKinematics(const ShipTransform& tr)
{
    game::navigation::WorldKinematicState world;
    const auto& motion = tr.motion;
    if (motion.travelFrame.valid)
    {
        world.positionMeters = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
        world.velocityMps = motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
    }
    else
    {
        world.positionMeters = tr.fullWorldMeters();
        world.velocityMps = motion.worldVelocityMps;
    }
    return world;
}

// THE ONE RULE for code that sets a ship's WORLD pose itself (scripted traffic, shells, the dock station): the world
// fields are written and the local description is kept consistent with them - inside a valid travel frame through the
// frame's full kinematics (its velocity at the point and its turn, not just its axes); without a frame the local
// velocity mirrors the world one (the convention of a frameless ship).
inline void setWorldKinematics(ShipTransform& tr, const glm::dvec3& positionMeters, const glm::dvec3& velocityMps)
{
    tr.setWorldPositionMeters(positionMeters);
    tr.motion.worldVelocityMps = velocityMps;
    if (tr.motion.travelFrame.valid)
    {
        tr.motion.localPositionMeters = tr.motion.travelFrame.worldToLocalPosition(positionMeters);
        tr.motion.localVelocityMps = tr.motion.travelFrame.worldToLocalVelocity(positionMeters, velocityMps);
    }
    else
    {
        tr.motion.localVelocityMps = velocityMps;
    }
}

struct FrameTransitionRequest
{
    game::navigation::KinematicFrame destination;
    std::string ownedFrameId;        // "ship_travel_<id>"
    std::string referenceFrameId;    // the reference the travel frame is matched to (the hub id)
    std::string hubId;
    // when set, the world state to carry over instead of the ship's current one (a materialized ship whose pose comes
    // from its shell, or a caller that measured the state at another point of the tick)
    const game::navigation::WorldKinematicState* worldOverride = nullptr;
};

inline bool transferShipIntoFrame(ShipTransform& tr, const FrameTransitionRequest& request)
{
    const auto& dest = request.destination;
    if (!dest.valid || request.ownedFrameId.empty() || request.referenceFrameId.empty())
        return false;
    // a frame change never moves a ship between systems (a jump does that)
    if (tr.motion.systemId >= 0 && dest.systemId != tr.motion.systemId)
        return false;

    const game::navigation::WorldKinematicState world =
        request.worldOverride ? *request.worldOverride : currentWorldKinematics(tr);
    if (!isFiniteVector(world.positionMeters) || !isFiniteVector(world.velocityMps))
        return false;
    const game::navigation::LocalKinematicState local = game::navigation::worldToLocalKinematics(dest, world);
    if (!isFiniteVector(local.positionMeters) || !isFiniteVector(local.velocityMps))
        return false;

    // --- commit (nothing above changed the ship) ---
    auto& motion = tr.motion;
    if (!game::navigation::TravelFrameSystem::matchToReference(motion, dest, request.ownedFrameId,
                                                               request.referenceFrameId))
        return false;
    motion.hubId = request.hubId;
    motion.mode = game::navigation::MotionMode::HubTactical;
    motion.localPositionMeters = local.positionMeters;
    motion.localVelocityMps = local.velocityMps;
    tr.setWorldPositionMeters(world.positionMeters);
    motion.worldVelocityMps = world.velocityMps;
    motion.referenceVelocityMps = dest.localToWorldVelocity(local.positionMeters, glm::dvec3(0.0));

    // frame-relative quantities: what the ship already does, measured in the new frame
    const glm::dvec3 relative = dest.localToWorldVector(local.velocityMps);
    const glm::dvec3 f(tr.forward());
    const glm::dvec3 r(tr.right());
    const glm::dvec3 u(tr.up());
    motion.forwardSpeedMps = glm::dot(relative, f);
    motion.strafeSpeedMps = glm::dot(relative, r);
    motion.liftSpeedMps = glm::dot(relative, u);
    motion.assistedTargetSpeedHold = false;
    motion.assistedThrottleTrimWasActive = false;
    motion.targetForwardSpeedMps = motion.forwardSpeedMps;
    motion.desiredTacticalVelocityMps = relative;
    motion.desiredRelativeVelocityMps = relative;
    motion.pendingReferenceVelocityMatch = false;
    return true;
}

} // namespace game::simulation
