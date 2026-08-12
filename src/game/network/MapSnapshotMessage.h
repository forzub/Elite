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

    // Stage 3C protocol seam: systems contain only authoritative world-state
    // overlays (currently id + jurisdiction). Static system/object catalog
    // fields are reconstructed from the client's local StarAtlas. The
    // snapshot still carries authoritative universe time/date for the map.
    world::celestial::GalaxyMapSnapshot snapshot;
};

struct SystemMapResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;
    int systemId = -1;

    // Stage 3B protocol seam: the authoritative server publishes map-specific
    // infrastructure/hub metadata and the map epoch, but leaves deterministic
    // celestial bodies and ordinary replicated ships to the client. Bodies are
    // rebuilt from the local catalog/runtime at snapshot.universeTimeSeconds;
    // ships are sampled from normal SimulationSnapshot history at the exact
    // response metadata.serverTimeSeconds. This prevents System-map requests
    // from becoming a second replication channel for moving ships.
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
