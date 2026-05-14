#pragma once

#include <deque>
#include "src/game/client/ClientWorldState.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/simulation/SimulationSnapshot.h"
// #include "src/game/SpaceState.h"
#include "src/scene/EntityID.h"
#include "src/game/network/ITransport.h"
#include "src/game/network/ClientMessage.h"

class GameServer;

class GameClient
{
public:
    // GameClient(GameServer* server, EntityId playerId);
    GameClient(ITransport* transport, EntityId playerId);

    void submitInput(const ShipControlState& control);
    void update(float dt,
            const WorldParams& world,
            float fixedDt);

    const ClientWorldState& world() const;
    ClientWorldState& world();
    
    void sendMessage(const game::network::ClientMessage& msg);



private:
    void reconcile(const SimulationSnapshot& snapshot,
               const WorldParams& world,
               float fixedDt);
    void replayPendingInputs(const WorldParams& world, float fixedDt);

private:
    // GameServer* m_server;
    ITransport*                     m_transport;
    EntityId                        m_playerId;

    ClientWorldState                m_world;

    struct TimedInput
    {
        uint32_t                    controlTick;
        ShipControlState            control;
    };

    std::deque<TimedInput>          m_pendingInputs;
    float                           m_accumulator = 0.0f;
    uint32_t                        m_clientTick = 0;
};
