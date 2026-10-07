#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"

namespace game::navigation
{

// The tactical Hub basis is X=prograde, Y=radial, Z=normal. Attachment data
// uses the visual basis X=normal, Y=radial, Z=-prograde.
inline glm::dvec3 hubVisualToTacticalVector(const glm::dvec3& visual)
{
    return {-visual.z, visual.y, visual.x};
}

struct DockingAdvisoryLocalPort
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    bool valid = false;
};

// Resolve exactly the authored Hub attachment and semantic port at one
// universe epoch. No world position, orbital tangent or render clock enters
// the docking decision. World projection belongs to the presentation adapter.
inline DockingAdvisoryLocalPort resolveDockingAdvisoryLocalPortAt(
    const simulation::HubAttachmentSnapshot& attachment,
    const HubSemanticAnchorDefinition& definition,
    double universeTimeSeconds
)
{
    DockingAdvisoryLocalPort out;
    if (!attachment.valid || !attachment.inheritHubOrientation ||
        attachment.hubId.empty() ||
        attachment.systemId < 0 ||
        definition.hubModuleId != attachment.moduleId ||
        !std::isfinite(universeTimeSeconds))
        return out;

    const glm::dvec3 angles = attachment.localRotationDeg +
        attachment.localAngularVelocityDegPerSecond * universeTimeSeconds;
    if (!std::isfinite(angles.x) || !std::isfinite(angles.y) ||
        !std::isfinite(angles.z))
        return out;
    const glm::dmat3 rotation(
        glm::mat3(hubLocalEulerDegToMatrix(angles))
    );
    const glm::dvec3 visualPosition = attachment.localOffsetMeters +
        rotation * definition.localPositionMeters;
    const glm::dvec3 visualForward = rotation * definition.localForward;
    const glm::dvec3 visualUp = rotation * definition.localUp;
    if (!std::isfinite(visualPosition.x) ||
        !std::isfinite(visualPosition.y) ||
        !std::isfinite(visualPosition.z) ||
        !std::isfinite(visualForward.x) ||
        !std::isfinite(visualForward.y) ||
        !std::isfinite(visualForward.z) ||
        !std::isfinite(visualUp.x) ||
        !std::isfinite(visualUp.y) ||
        !std::isfinite(visualUp.z) ||
        glm::length(visualForward) < 1.0e-9 ||
        glm::length(visualUp) < 1.0e-9)
        return out;

    out.positionMeters = hubVisualToTacticalVector(visualPosition);
    out.forward = glm::normalize(
        hubVisualToTacticalVector(visualForward)
    );
    const auto up = hubVisualToTacticalVector(visualUp);
    const auto orthogonalUp = up - out.forward * glm::dot(up, out.forward);
    if (glm::length(orthogonalUp) < 1.0e-9)
        return out;
    out.up = glm::normalize(orthogonalUp);
    out.valid = true;
    return out;
}


inline double dockingAdvisorySignedRollRateRadPerSec(
    const simulation::HubAttachmentSnapshot& attachment,
    const HubSemanticAnchorDefinition& definition,
    double universeTimeSeconds,
    double sampleSeconds = 0.01
)
{
    const double dt =
        std::max(1.0e-4, std::abs(sampleSeconds));
    const auto now =
        resolveDockingAdvisoryLocalPortAt(
            attachment,
            definition,
            universeTimeSeconds
        );
    const auto next =
        resolveDockingAdvisoryLocalPortAt(
            attachment,
            definition,
            universeTimeSeconds + dt
        );
    if (!now.valid || !next.valid)
        return 0.0;

    const glm::dvec3 axis =
        glm::normalize(now.forward);
    glm::dvec3 up0 =
        now.up - axis * glm::dot(now.up, axis);
    glm::dvec3 up1 =
        next.up - axis * glm::dot(next.up, axis);
    if (glm::length(up0) <= 1.0e-9 ||
        glm::length(up1) <= 1.0e-9)
    {
        return 0.0;
    }

    up0 = glm::normalize(up0);
    up1 = glm::normalize(up1);

    const double signedAngle =
        std::atan2(
            glm::dot(axis, glm::cross(up0, up1)),
            std::clamp(glm::dot(up0, up1), -1.0, 1.0)
        );
    return std::isfinite(signedAngle)
        ? signedAngle / dt
        : 0.0;
}

} // namespace game::navigation
