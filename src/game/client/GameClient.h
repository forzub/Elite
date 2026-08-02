#pragma once

#include <cstdint>
#include <deque>
#include "src/game/client/ClientWorldState.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/simulation/SimulationSnapshot.h"
// #include "src/game/SpaceState.h"
#include "src/scene/EntityID.h"
#include "src/game/network/ITransport.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/MapSnapshotMessage.h"

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

    bool requestGalaxyMapSnapshot();
    bool requestSystemMapSnapshot(int systemId);

    const world::celestial::GalaxyMapSnapshot*
        galaxyMapSnapshot() const;

    const world::celestial::SystemMapSnapshot*
        systemMapSnapshot(int systemId) const;


private:
    void receiveMapResponses();
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

    std::uint64_t                    m_nextMapRequestId = 1;
    std::uint64_t                    m_lastGalaxyMapRequestId = 0;
    std::uint64_t                    m_lastSystemMapRequestId = 0;

    bool                             m_hasGalaxyMapSnapshot = false;
    bool                             m_hasSystemMapSnapshot = false;
    int                              m_systemMapSnapshotId = -1;

    world::celestial::GalaxyMapSnapshot m_galaxyMapSnapshot;
    world::celestial::SystemMapSnapshot m_systemMapSnapshot;
};
