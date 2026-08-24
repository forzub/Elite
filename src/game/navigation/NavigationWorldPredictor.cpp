#include "src/game/navigation/NavigationWorldPredictor.h"

#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/HubCoMovingFrame.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/navigation/ReplicatedHubFrame.h"

namespace game::navigation
{
namespace
{
bool finiteVec(const glm::dvec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}
}

KinematicFrame NavigationWorldPredictor::predictHubFrameAt(
    const HubPredictionSource& source,
    double targetUniverseTimeSeconds
)
{
    KinematicFrame invalid;
    invalid.systemId = source.systemId;
    invalid.frameId = source.hubId;

    if (source.systemId < 0 || source.hubId.empty() ||
        !std::isfinite(source.sourceUniverseTimeSeconds) ||
        !std::isfinite(targetUniverseTimeSeconds) ||
        !finiteVec(source.positionMeters) ||
        !finiteVec(source.velocityMps))
    {
        return invalid;
    }

    const KinematicFrame replicatedFrame =
        makeReplicatedHubKinematicFrame(
            source.systemId,
            source.hubId,
            source.positionMeters,
            source.velocityMps,
            source.angularVelocityWorldRadPerSecond,
            source.orientation
        );
    if (!replicatedFrame.valid)
        return invalid;

    if (!source.orbitalMotion.enabled)
    {
        KinematicFrame out = replicatedFrame;
        const double dt =
            targetUniverseTimeSeconds - source.sourceUniverseTimeSeconds;
        out.originMeters += out.linearVelocityMps * dt;
        return out;
    }

    // OrbitalMotion is already the canonical curve definition. Do not infer
    // the future orbital angle by integrating the numerically differentiated
    // source velocity: doing so introduces a small but systematic phase error
    // even though period/phase are known exactly. Recover only the parent's
    // translational velocity from the replicated total velocity, advance the
    // parent linearly over this short horizon, and evaluate the same orbital
    // model used by the authoritative simulation at the planning epoch.
    const glm::dvec3 sourceLocalOrbitVelocity =
        world::orbits::computeOrbitVelocityMetersPerSecond(
            source.orbitalMotion,
            source.sourceUniverseTimeSeconds
        );
    const glm::dvec3 parentVelocity =
        source.velocityMps - sourceLocalOrbitVelocity;

    const double dt =
        targetUniverseTimeSeconds - source.sourceUniverseTimeSeconds;
    const glm::dvec3 targetParentPosition =
        source.orbitalMotion.centerMeters + parentVelocity * dt;

    world::orbits::OrbitalMotion targetMotion = source.orbitalMotion;
    targetMotion.centerMeters = targetParentPosition;

    const glm::dvec3 targetPosition =
        world::orbits::computeOrbitPositionMeters(
            targetMotion,
            targetUniverseTimeSeconds
        );
    const glm::dvec3 targetLocalOrbitVelocity =
        world::orbits::computeOrbitVelocityMetersPerSecond(
            targetMotion,
            targetUniverseTimeSeconds
        );
    const glm::dvec3 targetVelocity =
        parentVelocity + targetLocalOrbitVelocity;

    const glm::dvec3 relativePosition =
        targetPosition - targetParentPosition;
    const double radiusMeters = glm::length(relativePosition);
    if (!std::isfinite(radiusMeters) || radiusMeters <= 1.0)
        return invalid;

    const glm::dvec3 radial = normalizedHubFrameAxis(
        relativePosition,
        glm::dvec3(replicatedFrame.localToWorldBasis[1])
    );

    glm::dvec3 tangentialVelocity =
        targetLocalOrbitVelocity -
        radial * glm::dot(targetLocalOrbitVelocity, radial);
    const glm::dvec3 prograde = normalizedHubFrameAxis(
        tangentialVelocity,
        glm::dvec3(replicatedFrame.localToWorldBasis[0])
    );
    const glm::dvec3 normal = normalizedHubFrameAxis(
        glm::cross(prograde, radial),
        glm::dvec3(replicatedFrame.localToWorldBasis[2])
    );
    const glm::dvec3 orthogonalPrograde = normalizedHubFrameAxis(
        glm::cross(radial, normal),
        prograde
    );

    const double tangentialSpeedMps = glm::length(tangentialVelocity);
    const double angularSpeedRadPerSecond =
        tangentialSpeedMps / radiusMeters;
    const glm::dvec3 angularVelocity =
        -normal * angularSpeedRadPerSecond;

    KinematicFrame out;
    out.systemId = source.systemId;
    out.frameId = source.hubId;
    out.originMeters = targetPosition;
    out.linearVelocityMps = targetVelocity;
    out.linearAccelerationMps2 = glm::cross(
        angularVelocity,
        glm::cross(angularVelocity, relativePosition)
    );
    out.angularVelocityWorldRadPerSecond = angularVelocity;
    out.localToWorldBasis = glm::dmat3(
        orthogonalPrograde,
        radial,
        normal
    );
    out.valid =
        finiteVec(out.originMeters) &&
        finiteVec(out.linearVelocityMps) &&
        finiteVec(out.angularVelocityWorldRadPerSecond);
    return out;
}

WorldKinematicState NavigationWorldPredictor::predictConstantVelocity(
    const WorldKinematicState& source,
    double deltaSeconds
)
{
    WorldKinematicState out = source;
    if (!std::isfinite(deltaSeconds))
        return out;

    out.positionMeters += source.velocityMps * deltaSeconds;
    return out;
}

WorldKinematicState
NavigationWorldPredictor::predictHubLocalConstantVelocity(
    const KinematicFrame& targetFrame,
    const glm::dvec3& sourceLocalPositionMeters,
    const glm::dvec3& sourceLocalVelocityMps,
    double deltaGameplaySeconds
)
{
    if (!targetFrame.valid || !std::isfinite(deltaGameplaySeconds))
        return {};

    LocalKinematicState local;
    local.positionMeters =
        sourceLocalPositionMeters +
        sourceLocalVelocityMps * deltaGameplaySeconds;
    local.velocityMps = sourceLocalVelocityMps;
    return localToWorldKinematics(targetFrame, local);
}

HubAttachedKinematicState
NavigationWorldPredictor::resolveHubAttachmentAt(
    const KinematicFrame& hubFrame,
    double universeTimeSeconds,
    const glm::dvec3& localOffsetMeters,
    const glm::dvec3& localRotationDeg,
    const glm::dvec3& localAngularVelocityDegPerSecond
)
{
    HubAttachedKinematicState out;
    if (!hubFrame.valid || !std::isfinite(universeTimeSeconds))
        return out;

    const glm::dvec3 prograde(hubFrame.localToWorldBasis[0]);
    const glm::dvec3 radial(hubFrame.localToWorldBasis[1]);
    const glm::dvec3 normal(hubFrame.localToWorldBasis[2]);

    out.positionMeters = hubVisualLocalToWorldPosition(
        hubFrame.originMeters,
        prograde,
        radial,
        normal,
        localOffsetMeters
    );

    const glm::dvec3 localRotationAtEpoch =
        localRotationDeg +
        localAngularVelocityDegPerSecond * universeTimeSeconds;
    out.orientation = hubAttachedVisualOrientation(
        prograde,
        radial,
        normal,
        localRotationAtEpoch
    );

    const glm::dvec3 worldOffset =
        out.positionMeters - hubFrame.originMeters;
    out.velocityMps =
        hubFrame.linearVelocityMps +
        glm::cross(
            hubFrame.angularVelocityWorldRadPerSecond,
            worldOffset
        );

    const glm::dvec3 localAngularVelocityWorld =
        hubVisualLocalToWorldVector(
            prograde,
            radial,
            normal,
            glm::radians(localAngularVelocityDegPerSecond)
        );
    out.angularVelocityWorldRadPerSecond =
        hubFrame.angularVelocityWorldRadPerSecond +
        localAngularVelocityWorld;
    out.valid = finiteVec(out.positionMeters) && finiteVec(out.velocityMps);
    return out;
}

} // namespace game::navigation
