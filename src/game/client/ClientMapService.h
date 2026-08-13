#pragma once

#include <cstdint>
#include <optional>
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

    bool requestGalaxy(bool forceRefresh = false);
    bool requestSystem(int systemId, bool forceRefresh = false);
    bool requestDetail(
        const world::celestial::DetailTarget& target,
        bool forceRefresh = false);
    bool requestHub(
        int systemId,
        const std::string& hubId,
        bool forceRefresh = false);

    ClientRequestStatus galaxyStatus() const;
    ClientRequestStatus systemStatus() const;
    ClientRequestStatus detailStatus() const;
    ClientRequestStatus hubStatus() const;

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
    void sendSystemRequest();
    void sendDetailRequest();
    void sendHubRequest();
    bool acceptsTimeline(
        const game::network::SnapshotMetadata& metadata
    ) const;

    enum class SystemResponseResult
    {
        Ready,
        AwaitingSimulationHistory,
        RetryFreshResponse,
        Failed
    };

    SystemResponseResult tryCompleteSystemResponse(
        game::network::SystemMapResponse& response
    );
    void retrySystemRequestOrFail();

    enum class DetailResponseResult
    {
        Ready,
        AwaitingSimulationHistory,
        RetryFreshResponse,
        Failed
    };

    DetailResponseResult tryCompleteDetailResponse(
        game::network::DetailMapResponse& response
    );
    void retryDetailRequestOrFail();

    enum class HubResponseResult
    {
        Ready,
        AwaitingSimulationHistory,
        RetryFreshResponse,
        Failed
    };

    HubResponseResult tryCompleteHubResponse(
        game::network::HubMapResponse& response
    );
    void retryHubRequestOrFail();

private:
    ITransport& m_transport;
    const ClientCatalogService& m_catalogs;
    const ::ClientWorldState& m_world;
    std::uint64_t m_nextRequestId = 1;
    std::uint64_t m_universeTimelineRevision = 0;

    RequestState m_galaxyRequest;
    RequestState m_systemRequest;
    RequestState m_detailRequest;
    RequestState m_hubRequest;

    int m_requestedSystemId = -1;
    world::celestial::DetailTarget m_requestedDetailTarget;
    int m_requestedHubSystemId = -1;
    std::string m_requestedHubId;
    std::optional<game::network::SystemMapResponse>
        m_deferredSystemResponse;
    std::optional<game::network::DetailMapResponse>
        m_deferredDetailResponse;
    std::optional<game::network::HubMapResponse>
        m_deferredHubResponse;

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
