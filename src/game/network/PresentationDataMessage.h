#pragma once

#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialTypes.h"

namespace game::network
{
struct StarAtlasResponse
{
    CatalogMetadata metadata;
    world::celestial::StarAtlasDatabase atlas;
};

struct CelestialSnapshotResponse
{
    SnapshotMetadata metadata;
    world::celestial::CelestialSystemSnapshot snapshot;
};
}
