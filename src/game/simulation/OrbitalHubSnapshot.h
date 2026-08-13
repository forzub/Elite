#pragma once

#include <string>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "src/world/coordinates/WorldPosition.h"
#include "src/world/orbits/OrbitalMotion.h"

namespace game::simulation
{

/*
    Ordinary authoritative replication state for a composite orbital hub.

    A hub is not a presentation/map object and is not represented by one of its
    child station EntityIds. The server publishes the hub identity and runtime
    state once through the normal SimulationSnapshot stream; clients may then
    compose System/Detail/Hub presentation from that state without a parallel
    map-specific infrastructure channel.
*/
struct OrbitalHubSnapshot
{
    std::string id;
    std::string name;
    std::string owner;

    int systemId = -1;
    std::string parentBodyId;

    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
    glm::mat4 orientation {1.0f};
    std::string primeModuleId;
    world::orbits::OrbitalMotion motion;
};

} // namespace game::simulation
