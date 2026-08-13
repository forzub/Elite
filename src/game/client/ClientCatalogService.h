#pragma once

#include <string>

#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/CelestialRuntimeRegistry.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialTypes.h"

namespace game::client
{
class ClientCatalogService
{
public:
    ClientCatalogService() = default;

    // Static catalog ownership is local. No request/response protocol exists
    // for StarAtlas; client and server load the same packaged asset domain.
    bool loadLocalStarAtlas();
    void resetRuntimeState();

    bool validateServerStarAtlas(
        const game::network::CatalogMetadata& serverMetadata,
        std::string* errorMessage = nullptr
    ) const;

    bool resolveCelestialSnapshot(
        int systemId,
        double universeTimeSeconds,
        const game::network::SnapshotMetadata& sourceMetadata,
        bool forceRefresh = false
    );

    bool hasStarAtlas() const;
    bool hasCelestialSnapshot() const;

    const world::celestial::StarAtlasDatabase* starAtlas() const;
    const world::celestial::CelestialSystemSnapshot* celestialSnapshot() const;

    // Map/detail presentation may resolve any catalog system at an explicit
    // universe epoch without replacing the primary player-system cache.
    const world::celestial::CelestialSystemSnapshot* resolveCelestialSystem(
        int systemId,
        double universeTimeSeconds
    ) const;

    game::network::CatalogMetadata localStarAtlasMetadata() const;
    const game::network::SnapshotMetadata& celestialMetadata() const;

private:
    bool m_hasStarAtlas = false;
    bool m_hasCelestialSnapshot = false;

    game::network::SnapshotMetadata m_celestialSnapshotMetadata;
    world::celestial::StarAtlasDatabase m_starAtlas;
    world::celestial::CelestialRuntimeRegistry m_celestialRuntimes;
    world::celestial::CelestialSystemSnapshot m_celestialSnapshot;
};
}
