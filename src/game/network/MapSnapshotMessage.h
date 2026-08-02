#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/network/ProtocolMetadata.h"

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

struct DetailMapRequest
{
    std::uint64_t requestId = 0;
    world::celestial::DetailTarget target;
};

struct HubMapRequest
{
    std::uint64_t requestId = 0;
    int systemId = -1;
    std::string hubId;
};

using MapRequest = std::variant<
    GalaxyMapRequest,
    SystemMapRequest,
    DetailMapRequest,
    HubMapRequest
>;

struct GalaxyMapResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;
    world::celestial::GalaxyMapSnapshot snapshot;
};

struct SystemMapResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;
    int systemId = -1;
    world::celestial::SystemMapSnapshot snapshot;
};

struct DetailMapResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;
    world::celestial::DetailTarget target;
    world::celestial::DetailMapSnapshot snapshot;
};

struct HubMapResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;
    int systemId = -1;
    std::string hubId;
    world::celestial::HubMapSnapshot snapshot;
};

using MapResponse = std::variant<
    GalaxyMapResponse,
    SystemMapResponse,
    DetailMapResponse,
    HubMapResponse
>;
}
