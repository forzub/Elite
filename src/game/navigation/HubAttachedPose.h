#pragma once

// ONE law for the pose of a hub-attached static object (a station module, a city, a dock): it follows from the hub's
// frame (position, velocity, axes, spin) + the module's fixed attachment (offset, epoch-0 rotation, own spin) + the
// universe time. The server computes it every tick (GameSimulation.cpp static objects); the CLIENT computes the same
// from the replicated hub row for the objects the server no longer re-sends every snapshot (author 2026-11-24: «зачем
// нам пересылать объекты хабов, если можно хранить те же данные на клиенте»).

#include <glm/glm.hpp>

#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/navigation/ReplicatedHubFrame.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"
#include "src/game/simulation/OrbitalHubSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

struct HubAttachedPose
{
    bool valid = false;
    glm::dvec3 worldMeters {0.0};
    glm::dvec3 linearVelocityMps {0.0};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
    glm::mat4 orientation {1.0f};
};

// the hub's axes (prograde, radial, normal), position, velocity and spin -> the attached object's pose
inline HubAttachedPose poseHubAttached(const glm::dvec3& hubWorldMeters, const glm::dvec3& hubVelocityMps,
                                       const glm::dvec3& prograde, const glm::dvec3& radial, const glm::dvec3& normal,
                                       const glm::dvec3& hubAngularVelocityWorldRadPerSecond,
                                       const game::simulation::HubAttachmentSnapshot& a, double universeTimeSeconds)
{
    HubAttachedPose out;
    if (!a.valid)
        return out;
    const glm::dvec3 rotatedOffset =
        a.inheritHubOrientation ? hubVisualLocalToWorldVector(prograde, radial, normal, a.localOffsetMeters) : a.localOffsetMeters;
    out.worldMeters = hubWorldMeters + rotatedOffset;
    out.linearVelocityMps = hubVelocityMps + glm::cross(hubAngularVelocityWorldRadPerSecond, rotatedOffset);
    const glm::dvec3 rotationDeg = a.localRotationDeg + a.localAngularVelocityDegPerSecond * universeTimeSeconds;
    out.angularVelocityWorldRadPerSecond =
        hubAngularVelocityWorldRadPerSecond +
        hubAttachedAngularVelocityWorld(prograde, radial, normal, rotationDeg, a.localAngularVelocityDegPerSecond);
    if (a.inheritHubOrientation)
        out.orientation = hubAttachedVisualOrientation(prograde, radial, normal, rotationDeg);
    out.valid = true;
    return out;
}

// the same from a replicated hub row (the client)
inline HubAttachedPose poseHubAttached(const game::simulation::OrbitalHubSnapshot& hub,
                                       const game::simulation::HubAttachmentSnapshot& a, double universeTimeSeconds)
{
    const auto frame = makeReplicatedHubKinematicFrame(hub.systemId, hub.id, world::coordinates::fullMeters(hub.worldPosition),
                                                       hub.worldVelocityMps, hub.angularVelocityWorldRadPerSecond,
                                                       hub.orientation);
    if (!frame.valid)
        return {};
    return poseHubAttached(frame.originMeters, frame.linearVelocityMps, frame.localToWorldBasis[0], frame.localToWorldBasis[1],
                           frame.localToWorldBasis[2], frame.angularVelocityWorldRadPerSecond, a, universeTimeSeconds);
}

} // namespace game::navigation
