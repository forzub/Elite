#pragma once

#include <cstdint>

#include "src/game/client/ClientRequestStatus.h"
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

    void update(float dt);
    void pumpResponses();
    void resetPendingRequests();

    bool requestStarAtlas();
    bool requestCelestialSnapshot();

    ClientRequestStatus starAtlasStatus() const;
    ClientRequestStatus celestialStatus() const;

    bool hasStarAtlas() const;
    bool hasCelestialSnapshot() const;

    const world::celestial::StarAtlasDatabase* starAtlas() const;
    const world::celestial::CelestialSystemSnapshot* celestialSnapshot() const;

    const game::network::CatalogMetadata& starAtlasMetadata() const;
    const game::network::SnapshotMetadata& celestialMetadata() const;

private:
    struct RequestState
    {
        ClientRequestStatus status = ClientRequestStatus::Idle;
        std::uint64_t requestId = 0;
        float elapsedSeconds = 0.0f;
        int attempts = 0;
    };

    static constexpr float RequestTimeoutSeconds = 2.0f;
    static constexpr int MaxRequestAttempts = 3;

    std::uint64_t nextRequestId();
    void begin(RequestState& state, std::uint64_t requestId);
    void complete(RequestState& state);
    void cancel(RequestState& state);
    bool advanceTimeout(RequestState& state, float dt);
    void sendStarAtlasRequest();
    void sendCelestialRequest();

private:
    ITransport& m_transport;
    std::uint64_t m_nextRequestId = 1;
    RequestState m_starAtlasRequest;
    RequestState m_celestialRequest;

    bool m_hasStarAtlas = false;
    bool m_hasCelestialSnapshot = false;

    game::network::CatalogMetadata m_starAtlasMetadata;
    game::network::SnapshotMetadata m_celestialSnapshotMetadata;
    world::celestial::StarAtlasDatabase m_starAtlas;
    world::celestial::CelestialSystemSnapshot m_celestialSnapshot;
};
}
