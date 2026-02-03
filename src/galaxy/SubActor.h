#pragma once
#include "GalaxyIds.h"
#include <string>

struct SubActor
{
    SubActorId id;
    ActorId    parent;

    std::string name;          // «Свободная Карма», «Клан Красных»
    std::string description;
    std::vector<std::string> tags;
};
