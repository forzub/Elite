#pragma once

#include <cstdint>
#include <variant>

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/network/ProtocolMetadata.h"

namespace game::network
{
struct GalaxyMapRequest
{
    std::uint64_t requestId = 0;
};

// Only Galaxy still needs a dedicated map RPC because the response carries
// server-owned jurisdiction/world-knowledge overlays that are not part of the
// ordinary SimulationSnapshot stream yet. System/Detail/Hub presentation is
// composed directly from the client's latest accepted SimulationSnapshot epoch
// plus endpoint-local static/celestial data.
using MapRequest = std::variant<GalaxyMapRequest>;

struct GalaxyMapResponse
{
    std::uint64_t requestId = 0;
    SnapshotMetadata metadata;

    // Systems contain only authoritative world-state overlays (currently id +
    // jurisdiction). Static catalog fields are reconstructed from the client's
    // local StarAtlas.
    world::celestial::GalaxyMapSnapshot snapshot;
};

using MapResponse = std::variant<GalaxyMapResponse>;
}
