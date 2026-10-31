#pragma once

#include <string>
#include <vector>

#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
    /*
        Immutable System-map input produced before scene rendering.

        Authoritative snapshot data stays untouched. Bodies in this structure
        contain presentation-time orbit and rotation phases, while systemScale
        maps astronomical units into the local cartographic coordinate space.
    */
    struct SystemMapPresentation
    {
        int systemId = -1;
        double timeSeconds = 0.0;
        float systemScale = 1.0f;

        std::vector<world::celestial::SystemMapBody> bodies;
        // Every moon of the system and its planet, before visibility
        // filtering (presentation audits: a presented planet must present
        // all its moons).
        std::vector<std::pair<std::string, std::string>> moonParents;
    };

    std::string systemMapObjectStableKey(
        const world::celestial::SystemMapObject& object
    );
}
