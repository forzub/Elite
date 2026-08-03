#pragma once

#include "ITransport.h"
#include "src/game/network/ClientMessage.h"
#include <cstdint>
#include <queue>

class GameServer;


struct DelayedSnapshot
{
    SimulationSnapshot snapshot;
    float delay;
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

private:
    GameServer& m_server;
    std::queue<SimulationSnapshot> m_incoming;
    std::queue<game::network::MapResponse> m_mapResponses;
    std::queue<game::network::PresentationDataResponse> m_presentationResponses;
    std::vector<DelayedSnapshot> m_latencyBuffer;
    float m_fakeLatency = 0.1f; // 100ms
    float m_packetLoss = 0.0f;

    bool m_hasLastQueuedSnapshot = false;
    std::uint64_t m_lastQueuedSnapshotTick = 0;
    double m_lastQueuedServerTime = -1.0;
};
