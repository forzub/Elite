#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

namespace game::navigation
{

enum class ReferenceFrameType
{
    InertialWorld,
    CelestialBody,
    OrbitalHub,
    HubModule
};

struct ReferenceFrame
{
    ReferenceFrameType type =
        ReferenceFrameType::InertialWorld;

    std::string bodyId;
    std::string hubId;
    std::string moduleId;

    glm::dvec3 localOffsetMeters {0.0};
};

struct ResolvedFrameState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMetersPerSecond {0.0};
    glm::mat4 orientation {1.0f};

    bool valid = false;
};

} // namespace game::navigation