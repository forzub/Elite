#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include "src/game/client/ClientWorldState.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/simulation/SimulationSnapshot.h"
// #include "src/game/SpaceState.h"
#include "src/scene/EntityID.h"
#include "src/game/network/ITransport.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/MapSnapshotMessage.h"

class GameClient
{
public:
    GameClient(ITransport* transport, EntityId playerId);

    void submitInput(const ShipControlState& control);
    void update(float dt,
            float fixedDt);

    const ClientWorldState& world() const;
    ClientWorldState& world();

    void sendMessage(const game::network::ClientMessage& msg);

    bool requestGalaxyMapSnapshot();
    bool requestSystemMapSnapshot(int systemId);
    bool requestDetailMapSnapshot(
        const world::celestial::DetailTarget& target);
    bool requestHubMapSnapshot(
        int systemId,
        const std::string& hubId);

    const world::celestial::GalaxyMapSnapshot*
        galaxyMapSnapshot() const;

    const world::celestial::SystemMapSnapshot*
        systemMapSnapshot(int systemId) const;
    const world::celestial::DetailMapSnapshot*
        detailMapSnapshot(
            const world::celestial::DetailTarget& target) const;
    const world::celestial::HubMapSnapshot*
        hubMapSnapshot(
            int systemId,
            const std::string& hubId) const;

    bool hasSessionSnapshot() const;
    bool readyForGameplay() const;
    const game::simulation::ClientSessionSnapshot&
        sessionSnapshot() const;
    const world::celestial::PlayerNavigationState&
        playerNavigation() const;


private:
    void receiveMapResponses();
    void reconcile(const SimulationSnapshot& snapshot,
               const WorldParams& world,
               float fixedDt);
    void replayPendingInputs(const WorldParams& world, float fixedDt);

private:
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
    std::uint64_t                    m_lastDetailMapRequestId = 0;
    std::uint64_t                    m_lastHubMapRequestId = 0;

    bool                             m_hasGalaxyMapSnapshot = false;
    bool                             m_hasSystemMapSnapshot = false;
    bool                             m_hasDetailMapSnapshot = false;
    bool                             m_hasHubMapSnapshot = false;
    int                              m_systemMapSnapshotId = -1;
    int                              m_hubMapSnapshotSystemId = -1;
    std::string                      m_hubMapSnapshotHubId;
    world::celestial::DetailTarget   m_detailMapSnapshotTarget;

    world::celestial::GalaxyMapSnapshot m_galaxyMapSnapshot;
    world::celestial::SystemMapSnapshot m_systemMapSnapshot;
    world::celestial::DetailMapSnapshot m_detailMapSnapshot;
    world::celestial::HubMapSnapshot m_hubMapSnapshot;

    bool m_hasSessionSnapshot = false;
    game::simulation::ClientSessionSnapshot m_sessionSnapshot;
};
