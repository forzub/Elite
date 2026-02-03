#pragma once
#include "GalaxyIds.h"
#include "GalaxyEnums.h"
#include <string>
#include <vector>
#include <glm/vec3.hpp>

struct StarSystem
{
    StarSystemId id;

    std::string name;

    glm::vec3 position;
    float distance;

    StarSystemKind kind;

    ActorId dominantActor;     // справочный
    std::string description;

    std::vector<NodeId> nodes;
    std::vector<std::string> tags;
};

