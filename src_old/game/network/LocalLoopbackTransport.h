#pragma once

#include "ITransport.h"
#include "src/game/network/ClientMessage.h"
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
    LocalLoopbackTransport(GameServer* server);

    void sendInput(
        EntityId id,
        const ShipControlState& control) override;

    bool receiveSnapshot(
        SimulationSnapshot& outSnapshot) override;

    void update(float dt) override;

    void sendClientMessage(
        EntityId playerId,
        const game::network::ClientMessage& msg) override;

private:
    GameServer* m_server;
    std::queue<SimulationSnapshot> m_incoming;
    std::vector<DelayedSnapshot> m_latencyBuffer;
    float m_fakeLatency = 0.1f; // 100ms
    float m_packetLoss = 0.1f;
};
