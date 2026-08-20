#pragma once

#include <cstdint>
#include <string>

#include "src/game/client/ClientRequestStatus.h"
#include "src/game/network/ITransport.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/CelestialTypes.h"

class ClientWorldState;

namespace game::client
{
class ClientCatalogService;

class ClientMapService
{
public:
    ClientMapService(
        ITransport& transport,
        const ClientCatalogService& catalogs,
        const ::ClientWorldState& world
    );

    void update(float dt);
    void pumpResponses();
    void resetPendingRequests();
    void setUniverseTimelineRevision(std::uint64_t revision);

    // Galaxy remains an RPC because the response still carries server-owned
    // jurisdiction/world-knowledge overlays not present in SimulationSnapshot.
    bool requestGalaxy(bool forceRefresh = false);

    // System/Detail/Hub are endpoint-local presentation composition. The epoch
    // is the metadata of an already accepted SimulationSnapshot; no map RPC or
    // second server-time handshake is allowed on this path.
    bool composeSystem(
        int systemId,
        const game::network::SnapshotMetadata& sourceMetadata,
        double universeTimeScale,
        const std::string& universeDate
    );
    bool composeDetail(
        const world::celestial::DetailTarget& target,
        const game::network::SnapshotMetadata& sourceMetadata
    );
    bool composeHub(
        int systemId,
        const std::string& hubId,
        const game::network::SnapshotMetadata& sourceMetadata
    );

    ClientRequestStatus galaxyStatus() const;

    const game::network::SnapshotMetadata& galaxyMetadata() const;
    const game::network::SnapshotMetadata& systemMetadata() const;
    const game::network::SnapshotMetadata& detailMetadata() const;
    const game::network::SnapshotMetadata& hubMetadata() const;

    const world::celestial::GalaxyMapSnapshot* galaxy() const;
    const world::celestial::SystemMapSnapshot* system(int systemId) const;
    const world::celestial::DetailMapSnapshot* detail(
        const world::celestial::DetailTarget& target) const;
    const world::celestial::HubMapSnapshot* hub(
        int systemId,
        const std::string& hubId) const;

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
    void fail(RequestState& state);
    void cancel(RequestState& state);
    bool advanceTimeout(RequestState& state, float dt);
    void sendGalaxyRequest();
    bool acceptsTimeline(const game::network::SnapshotMetadata& metadata) const;

private:
    ITransport& m_transport;
    const ClientCatalogService& m_catalogs;
    const ::ClientWorldState& m_world;
    std::uint64_t m_nextRequestId = 1;
    std::uint64_t m_universeTimelineRevision = 0;

    RequestState m_galaxyRequest;

    bool m_hasGalaxy = false;
    bool m_hasSystem = false;
    bool m_hasDetail = false;
    bool m_hasHub = false;
    int m_systemSnapshotId = -1;
    int m_hubSnapshotSystemId = -1;
    std::string m_hubSnapshotId;
    world::celestial::DetailTarget m_detailSnapshotTarget;

    game::network::SnapshotMetadata m_galaxyMetadata;
    game::network::SnapshotMetadata m_systemMetadata;
    game::network::SnapshotMetadata m_detailMetadata;
    game::network::SnapshotMetadata m_hubMetadata;

    world::celestial::GalaxyMapSnapshot m_galaxySnapshot;
    world::celestial::SystemMapSnapshot m_systemSnapshot;
    world::celestial::DetailMapSnapshot m_detailSnapshot;
    world::celestial::HubMapSnapshot m_hubSnapshot;
};
}
