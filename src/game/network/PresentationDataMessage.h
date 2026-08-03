#pragma once

#include <cstdint>
#include <variant>

#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialTypes.h"

namespace game::network
{
struct StarAtlasRequest
{
    std::uint64_t requestId = 0;
};

struct CelestialSnapshotRequest
{
    std::uint64_t requestId = 0;
};

using PresentationDataRequest = std::variant<
    StarAtlasRequest,
    CelestialSnapshotRequest
>;

struct StarAtlasResponse
{
    std::uint64_t requestId = 0;
    CatalogMetadata metadata;
    world::celestial::StarAtlasDatabase atlas;
};

struct CelestialSnapshotResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;
    world::celestial::CelestialSystemSnapshot snapshot;
};

using PresentationDataResponse = std::variant<
    StarAtlasResponse,
    CelestialSnapshotResponse
>;
}
