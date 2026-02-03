#pragma once
#include "GalaxyIds.h"

struct StarRoute
{
    RouteId id;

    StarSystemId a;
    StarSystemId b;

    float distance;
    bool restricted;
    float dangerLevel;

    std::string description;
    std::vector<std::string> tags;
};

