#pragma once

#include <cstdint>

#include "src/game/network/ITransport.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialTypes.h"

namespace game::client
{
class ClientCatalogService
{
public:
    explicit ClientCatalogService(ITransport& transport);

    void pumpResponses();

    bool requestStarAtlas();
    bool requestCelestialSnapshot();

    bool hasStarAtlas() const;
    bool hasCelestialSnapshot() const;

    const world::celestial::StarAtlasDatabase* starAtlas() const;
    const world::celestial::CelestialSystemSnapshot* celestialSnapshot() const;

    const game::network::CatalogMetadata& starAtlasMetadata() const;
    const game::network::SnapshotMetadata& celestialMetadata() const;

private:
    ITransport& m_transport;

    std::uint64_t m_nextRequestId = 1;
    std::uint64_t m_starAtlasRequestId = 0;
    std::uint64_t m_celestialRequestId = 0;

    bool m_starAtlasResponseReady = false;
    bool m_celestialResponseReady = false;
    bool m_hasStarAtlas = false;
    bool m_hasCelestialSnapshot = false;

    game::network::CatalogMetadata m_starAtlasMetadata;
    game::network::SnapshotMetadata m_celestialSnapshotMetadata;
    world::celestial::StarAtlasDatabase m_starAtlas;
    world::celestial::CelestialSystemSnapshot m_celestialSnapshot;
};
}
