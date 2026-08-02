#pragma once

#include <cstdint>
#include <variant>

#include "src/world/celestial/SystemMapTypes.h"

namespace game::network
{
struct GalaxyMapRequest
{
    std::uint64_t requestId = 0;
};

struct SystemMapRequest
{
    std::uint64_t requestId = 0;
    int systemId = -1;
};

using MapRequest = std::variant<
    GalaxyMapRequest,
    SystemMapRequest
>;

struct GalaxyMapResponse
{
    std::uint64_t requestId = 0;
    world::celestial::GalaxyMapSnapshot snapshot;
};

struct SystemMapResponse
{
    std::uint64_t requestId = 0;
    int systemId = -1;
    world::celestial::SystemMapSnapshot snapshot;
};

using MapResponse = std::variant<
    GalaxyMapResponse,
    SystemMapResponse
>;
}
