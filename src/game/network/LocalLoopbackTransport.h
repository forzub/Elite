#pragma once

#include "ITransport.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/TimeSyncMessage.h"
#include <cstdint>
#include <queue>
#include <vector>

class GameServer;


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


class LocalLoopbackTransport : public ITransport
{
public:
    explicit LocalLoopbackTransport(GameServer& server);


    bool receiveSnapshot(
        SimulationSnapshot& outSnapshot) override;

    // Startup handshake for an in-process client. The initial authoritative
    // snapshot must be available before client-facing state is initialized;
    // normal runtime snapshots still pass through the latency buffer.
    void enqueueCurrentSnapshotImmediately();

    void update(float dt) override;

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

private:
    GameServer& m_server;
    std::queue<SimulationSnapshot> m_incoming;
    std::queue<game::network::MapResponse> m_mapResponses;
    std::queue<game::network::PresentationDataResponse> m_presentationResponses;
    std::queue<game::network::TimeSyncResponse> m_timeSyncResponses;
    std::vector<DelayedSnapshot> m_latencyBuffer;
    std::vector<DelayedTimeSyncRequest> m_timeSyncRequestBuffer;
    std::vector<DelayedTimeSyncResponse> m_timeSyncResponseBuffer;
    float m_fakeLatency = 0.1f; // 100ms
    float m_packetLoss = 0.0f;

    bool m_hasLastQueuedSnapshot = false;
    std::uint64_t m_lastQueuedSnapshotTick = 0;
    double m_lastQueuedServerTime = -1.0;
};
