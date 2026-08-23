#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace game::navigation
{

enum class HubSemanticAnchorKind : std::uint8_t
{
    DockingPort = 0,
    LandingPad,
    TransitGate,
    CargoAccess,
    ServiceAccess,
    AttackPoint,
    SensorArray,
    NavigationReference
};

struct HubSemanticAnchorDefinition
{
    std::string id;
    std::string hubModuleId;
    HubSemanticAnchorKind kind = HubSemanticAnchorKind::NavigationReference;

    glm::dvec3 localPositionMeters {0.0};
    glm::dvec3 localForward {0.0, 0.0, -1.0};
    glm::dvec3 localUp {0.0, 1.0, 0.0};

    glm::dvec3 extentMeters {0.0};
    double requiredClearanceMeters = 0.0;
    double maxEntrySpeedMps = 0.0;

    bool enabled = true;
};

/*
    Runtime-resolved semantic element. It is intentionally independent from
    mesh vertices and remains meaningful if the module OBJ is replaced.
*/
struct ResolvedHubSemanticAnchor
{
    std::string id;
    std::string hubModuleId;
    HubSemanticAnchorKind kind = HubSemanticAnchorKind::NavigationReference;

    int systemId = -1;
    double epochUniverseTimeSeconds = 0.0;

    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};

    glm::dvec3 extentMeters {0.0};
    double requiredClearanceMeters = 0.0;
    double maxEntrySpeedMps = 0.0;
    bool enabled = true;

    glm::dvec3 forward() const noexcept
    {
        return orientation * glm::dvec3(0.0, 0.0, -1.0);
    }

    glm::dvec3 up() const noexcept
    {
        return orientation * glm::dvec3(0.0, 1.0, 0.0);
    }
};

inline ResolvedHubSemanticAnchor resolveHubSemanticAnchor(
    const HubSemanticAnchorDefinition& definition,
    int systemId,
    double universeTimeSeconds,
    const glm::dvec3& objectPositionMeters,
    const glm::dvec3& objectVelocityMps,
    const glm::mat4& objectOrientation,
    const glm::dvec3& objectAngularVelocityWorldRadPerSecond
)
{
    ResolvedHubSemanticAnchor out;
    out.id = definition.id;
    out.hubModuleId = definition.hubModuleId;
    out.kind = definition.kind;
    out.systemId = systemId;
    out.epochUniverseTimeSeconds = universeTimeSeconds;
    out.extentMeters = definition.extentMeters;
    out.requiredClearanceMeters = definition.requiredClearanceMeters;
    out.maxEntrySpeedMps = definition.maxEntrySpeedMps;
    out.enabled = definition.enabled;
    out.angularVelocityWorldRadPerSecond =
        objectAngularVelocityWorldRadPerSecond;

    const glm::dmat3 objectBasis(objectOrientation);
    const glm::dvec3 worldOffset =
        objectBasis * definition.localPositionMeters;
    out.positionMeters = objectPositionMeters + worldOffset;
    out.velocityMps = objectVelocityMps +
        glm::cross(objectAngularVelocityWorldRadPerSecond, worldOffset);

    glm::dvec3 localForward = definition.localForward;
    glm::dvec3 localUp = definition.localUp;
    if (glm::length(localForward) <= 1.0e-9)
        localForward = glm::dvec3(0.0, 0.0, -1.0);
    if (glm::length(localUp) <= 1.0e-9)
        localUp = glm::dvec3(0.0, 1.0, 0.0);

    localForward = glm::normalize(localForward);
    localUp = glm::normalize(localUp);
    glm::dvec3 localRight = glm::cross(localForward, localUp);
    if (glm::length(localRight) <= 1.0e-9)
        localRight = glm::dvec3(1.0, 0.0, 0.0);
    else
        localRight = glm::normalize(localRight);
    localUp = glm::normalize(glm::cross(localRight, localForward));

    const glm::dvec3 worldForward =
        glm::normalize(objectBasis * localForward);
    const glm::dvec3 worldUpGuess =
        glm::normalize(objectBasis * localUp);
    glm::dvec3 worldRight = glm::cross(worldForward, worldUpGuess);
    if (glm::length(worldRight) <= 1.0e-9)
        worldRight = glm::dvec3(1.0, 0.0, 0.0);
    else
        worldRight = glm::normalize(worldRight);
    const glm::dvec3 worldUp =
        glm::normalize(glm::cross(worldRight, worldForward));

    const glm::dmat3 anchorBasis(
        worldRight,
        worldUp,
        -worldForward
    );
    out.orientation = glm::normalize(glm::quat_cast(anchorBasis));
    return out;
}

} // namespace game::navigation
