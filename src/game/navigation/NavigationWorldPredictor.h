#pragma once

#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/KinematicFrame.h"
#include "src/world/orbits/OrbitalMotion.h"

namespace game::navigation
{

/*
    Kinematic prediction boundary used before path/trajectory planning.

    This layer owns "bring this authoritative state to planning epoch T".
    It is deliberately independent from client presentation/interpolation and
    from route-search policy. Client/server adapters provide authoritative seed
    state; planners consume only the resolved result.
*/
struct HubPredictionSource
{
    int systemId = -1;
    std::string hubId;
    double sourceUniverseTimeSeconds = 0.0;

    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
    glm::mat4 orientation {1.0f};
    world::orbits::OrbitalMotion orbitalMotion;
};

struct HubAttachedKinematicState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::mat4 orientation {1.0f};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
    bool valid = false;
};

class NavigationWorldPredictor
{
public:
    static KinematicFrame predictHubFrameAt(
        const HubPredictionSource& source,
        double targetUniverseTimeSeconds
    );

    static WorldKinematicState predictConstantVelocity(
        const WorldKinematicState& source,
        double deltaSeconds
    );

    static WorldKinematicState predictHubLocalConstantVelocity(
        const KinematicFrame& targetFrame,
        const glm::dvec3& sourceLocalPositionMeters,
        const glm::dvec3& sourceLocalVelocityMps,
        double deltaGameplaySeconds
    );

    static HubAttachedKinematicState resolveHubAttachmentAt(
        const KinematicFrame& hubFrame,
        double universeTimeSeconds,
        const glm::dvec3& localOffsetMeters,
        const glm::dvec3& localRotationDeg,
        const glm::dvec3& localAngularVelocityDegPerSecond
    );
};

} // namespace game::navigation
