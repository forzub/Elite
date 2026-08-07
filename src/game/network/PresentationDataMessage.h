#pragma once

#include <cstdint>
#include <variant>

#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/StarAtlasDatabase.h"

namespace game::network
{
struct StarAtlasRequest
{
    std::uint64_t requestId = 0;
};

using PresentationDataRequest = std::variant<
    StarAtlasRequest
>;

struct StarAtlasResponse
{
    std::uint64_t requestId = 0;
    CatalogMetadata metadata;
    world::celestial::StarAtlasDatabase atlas;
};

using PresentationDataResponse = std::variant<
    StarAtlasResponse
>;
}
