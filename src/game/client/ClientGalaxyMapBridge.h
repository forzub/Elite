#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::client
{
inline void rebuildGalaxyMapCatalogLayer(
    world::celestial::GalaxyMapSnapshot& snapshot,
    const std::vector<world::celestial::StarSystemSummary>& systems,
    const std::vector<world::celestial::GalaxyObjectDefinition>& objects
)
{
    /*
        The authoritative reply owns world-state overlays (currently the
        jurisdiction attached to a physical system) and the response epoch.
        Static system/object catalog fields are deterministic local data and
        are reconstructed from the client's StarAtlas instead of being copied
        through every Galaxy-map response.
    */
    std::unordered_map<int, std::string> jurisdictionBySystem;
    jurisdictionBySystem.reserve(snapshot.systems.size());

    for (const auto& overlay : snapshot.systems)
    {
        if (overlay.id >= 0 && !overlay.jurisdiction.empty())
        {
            jurisdictionBySystem[overlay.id] = overlay.jurisdiction;
        }
    }

    snapshot.systems.clear();
    snapshot.systems.reserve(systems.size());

    for (const auto& source : systems)
    {
        world::celestial::GalaxyMapSystem item;
        item.id = source.id;
        item.name = source.name;
        item.starType = source.starType;
        item.starsCount = source.starsCount;
        item.positionLy = source.positionLy;

        const auto jurisdictionIt =
            jurisdictionBySystem.find(source.id);
        item.jurisdiction =
            jurisdictionIt != jurisdictionBySystem.end()
                ? jurisdictionIt->second
                : "Unregistered";

        snapshot.systems.push_back(std::move(item));
    }

    snapshot.objects.clear();
    snapshot.objects.reserve(objects.size());

    for (const auto& source : objects)
    {
        world::celestial::GalaxyMapObject item;
        item.id = source.id;
        item.name = source.name;
        item.objectType = source.objectType;
        item.positionLy = source.positionLy;
        item.description = source.description;
        item.tags = source.tags;
        snapshot.objects.push_back(std::move(item));
    }
}

inline void rebuildGalaxyMapCatalogLayer(
    world::celestial::GalaxyMapSnapshot& snapshot,
    const world::celestial::StarAtlasDatabase& atlas
)
{
    rebuildGalaxyMapCatalogLayer(
        snapshot,
        atlas.systems(),
        atlas.objects()
    );
}
}
