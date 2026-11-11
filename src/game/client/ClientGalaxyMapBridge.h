#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::client
{
/*
    Map presentation of an interstellar-medium object (structure or
    background cell); null for compact objects. ClientGalaxyMapBridge.cpp.
*/
std::shared_ptr<const world::celestial::GalaxyMapMediumVolume> galaxyMapMediumVolume(
    const world::celestial::GalaxyObjectDefinition& source);

/*
    Galaxy Map presentation proxies of the distinct medium structures (outer
    voxel shells, LODs, far blobs) in one vertex set; proxyOfObject[i] = proxy
    index of objects[i] or -1. ClientGalaxyMapBridge.cpp.
*/
std::shared_ptr<const world::celestial::GalaxyMapMediumAtlas> buildGalaxyMapMediumAtlas(
    const std::vector<world::celestial::GalaxyObjectDefinition>& objects,
    std::vector<int>& proxyOfObject);

// Map info card rows of a medium structure / icy field (empty otherwise).
std::vector<std::pair<std::string, std::string>> galaxyMapMediumInfo(
    const world::celestial::GalaxyObjectDefinition& source);

/*
    The static object layer of the Galaxy map (catalog objects with their medium proxies) - deterministic
    from the local StarAtlas, ~0.7 s to build (the medium shells). Built once per catalog and shared:
    prewarmGalaxyMapCatalogObjectLayer starts it on a worker as soon as the atlas is loaded, the first
    map response then only waits for what is left. ClientGalaxyMapBridge.cpp.
*/
struct GalaxyMapCatalogObjectLayer
{
    std::vector<world::celestial::GalaxyMapObject> objects;
    std::shared_ptr<const world::celestial::GalaxyMapMediumAtlas> mediumAtlas;
};

std::shared_ptr<const GalaxyMapCatalogObjectLayer> galaxyMapCatalogObjectLayer(
    const std::vector<world::celestial::GalaxyObjectDefinition>& objects);

void prewarmGalaxyMapCatalogObjectLayer(
    const std::vector<world::celestial::GalaxyObjectDefinition>& objects);

// the catalog goes (its owner is destroyed): waits for a running build and forgets the layer
void releaseGalaxyMapCatalogObjectLayer();

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

    const auto layer = galaxyMapCatalogObjectLayer(objects);
    snapshot.objects = layer->objects;
    snapshot.mediumAtlas = layer->mediumAtlas;
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
