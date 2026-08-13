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

    // Stage 3E protocol seam: this response is an authoritative map-epoch
    // anchor, not a second world-state channel. Deterministic celestial/static
    // catalog fields are rebuilt from the endpoint-local StarAtlas at
    // snapshot.universeTimeSeconds. production hubs/static infrastructure and
    // ordinary replicated ships are sampled from ordinary SimulationSnapshot history
    // at response metadata.serverTimeSeconds and converted to map
    // presentation on the client. Only explicit diagnostic presentation probes
    // may remain in snapshot.objects while this migration is incremental.
    world::celestial::SystemMapSnapshot snapshot;
};

struct DetailMapResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;
    world::celestial::DetailTarget target;

    // Stage 3F protocol seam: Details is now client-composed. The server only
    // acknowledges the semantic target at an authoritative server/universe
    // epoch. Celestial state comes from the endpoint-local StarAtlas/runtime;
    // ships, hubs and infrastructure come from ordinary SimulationSnapshot
    // history sampled at metadata.serverTimeSeconds.
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
