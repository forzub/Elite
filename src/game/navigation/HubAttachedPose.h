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
#include "src/game/simulation/ObjectSnapshot.h"
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

// a hub-attached object whose WHOLE pose follows from its hub (the server need not re-send it): attached with the hub's
// orientation, no part turning on its own (an assembly angle accumulates on the server - not derivable yet), no loose
// fragments flying about it
inline bool hubAttachedObjectDerivable(const ::ObjectSnapshot& o)
{
    if (!o.hubAttachment.valid || !o.hubAttachment.inheritHubOrientation)
        return false;
    for (const auto& m : o.graph.assemblyModules)
        if (m.rotationAngleRad != 0.0f)
            return false;
    return o.graph.detachedFragments.empty();
}

// the object re-posed from its hub's row at `universeTimeSeconds`
inline bool reposeHubAttachedObject(::ObjectSnapshot& o, const game::simulation::OrbitalHubSnapshot& hub,
                                    double universeTimeSeconds)
{
    const auto pose = poseHubAttached(hub, o.hubAttachment, universeTimeSeconds);
    if (!pose.valid)
        return false;
    o.worldPosition = world::coordinates::makeWorldPositionFromMeters(pose.worldMeters);
    o.orientation = pose.orientation;
    o.linearVelocityMps = pose.linearVelocityMps;
    o.angularVelocityWorldRadPerSecond = pose.angularVelocityWorldRadPerSecond;
    return true;
}

} // namespace game::navigation
