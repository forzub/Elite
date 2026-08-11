#pragma once

#include "ITransport.h"
#include "IServerTransport.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/TimeSyncMessage.h"
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

struct DelayedSnapshot
{
    SimulationSnapshot snapshot;
    float delay;
};

struct DelayedTimeSyncRequest
{
    game::network::TimeSyncRequest request;
    float delay = 0.0f;
};

struct DelayedTimeSyncResponse
{
    game::network::TimeSyncResponse response;
    float delay = 0.0f;
};


/*
    In-process transport link exposing distinct client and server interfaces.

    It intentionally owns no GameServer reference. Client calls can only enqueue
    protocol messages; ServerRunner consumes them through IServerTransport and
    publishes responses back through the same message seam. The queues are still
    single-threaded at this stage; thread-safe ownership comes after this logical
    boundary is regression-protected.
*/
class LocalLoopbackTransport final : public ITransport, public IServerTransport
{
public:
    LocalLoopbackTransport() = default;

    // ----- client endpoint (ITransport) -----
    bool receiveSnapshot(
        SimulationSnapshot& outSnapshot) override;

    void sendClientMessage(
        EntityId playerId,
        const game::network::ClientMessage& msg) override;

    void sendMapRequest(
        const game::network::MapRequest& request) override;

    bool receiveMapResponse(
        game::network::MapResponse& outResponse) override;

    void sendPresentationDataRequest(
        const game::network::PresentationDataRequest& request) override;

    bool receivePresentationDataResponse(
        game::network::PresentationDataResponse& outResponse) override;

    void sendTimeSyncRequest(
        const game::network::TimeSyncRequest& request) override;

    bool receiveTimeSyncResponse(
        game::network::TimeSyncResponse& outResponse) override;

    // ----- server endpoint (IServerTransport) -----
    void update(float dt) override;

    bool receiveClientMessage(
        EntityId& outPlayerId,
        game::network::ClientMessage& outMessage) override;

    bool receiveMapRequest(
        game::network::MapRequest& outRequest) override;

    bool receivePresentationDataRequest(
        game::network::PresentationDataRequest& outRequest) override;

    bool receiveTimeSyncRequest(
        game::network::TimeSyncRequest& outRequest) override;

    void publishSnapshot(
        const SimulationSnapshot& snapshot) override;

    void publishSnapshotImmediately(
        const SimulationSnapshot& snapshot) override;

    void sendMapResponse(
        game::network::MapResponse response) override;

    void sendPresentationDataResponse(
        game::network::PresentationDataResponse response) override;

    void sendTimeSyncResponse(
        game::network::TimeSyncResponse response) override;

private:
    std::queue<SimulationSnapshot> m_incoming;
    std::queue<std::pair<EntityId, game::network::ClientMessage>> m_clientMessages;
    std::queue<game::network::MapRequest> m_mapRequests;
    std::queue<game::network::PresentationDataRequest> m_presentationRequests;
    std::queue<game::network::TimeSyncRequest> m_serverTimeSyncRequests;

    std::queue<game::network::MapResponse> m_mapResponses;
    std::queue<game::network::PresentationDataResponse> m_presentationResponses;
    std::queue<game::network::TimeSyncResponse> m_timeSyncResponses;

    std::vector<DelayedSnapshot> m_latencyBuffer;
    std::vector<DelayedTimeSyncRequest> m_timeSyncRequestBuffer;
    std::vector<DelayedTimeSyncResponse> m_timeSyncResponseBuffer;
    float m_fakeLatency = 0.1f; // 100ms per simulated leg
    float m_packetLoss = 0.0f;

    bool m_hasLastQueuedSnapshot = false;
    std::uint64_t m_lastQueuedSnapshotTick = 0;
    double m_lastQueuedServerTime = -1.0;
};
