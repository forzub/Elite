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
#include "src/game/network/ProtocolMetadata.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialTypes.h"

enum class ClientConnectionState
{
    Disconnected,
    Connecting,
    Synchronizing,
    Ready,
    Failed
};

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

    void beginSynchronization();
    void failSynchronization(std::string message);
    ClientConnectionState connectionState() const;
    const std::string& connectionError() const;

    bool hasSessionSnapshot() const;
    bool readyForGameplay() const;
    const game::simulation::ClientSessionSnapshot&
        sessionSnapshot() const;
    const world::celestial::PlayerNavigationState&
        playerNavigation() const;

    bool requestStarAtlas();
    bool requestCelestialSnapshot();
    const world::celestial::StarAtlasDatabase* starAtlas() const;
    const world::celestial::CelestialSystemSnapshot*
        celestialSnapshot() const;


private:
    bool hasGameplayCoreState() const;
    void refreshConnectionState();
    void receiveMapResponses();
    void replayPendingInputs(const WorldParams& world, float fixedDt);

private:
    ITransport*                     m_transport;
    ClientConnectionState           m_connectionState =
        ClientConnectionState::Disconnected;
    std::string                     m_connectionError;
    EntityId                        m_playerId;

    ClientWorldState                m_world;

    struct TimedInput
    {
        std::uint64_t               controlTick = 0;
        ShipControlState            control;
    };

    std::deque<TimedInput>          m_pendingInputs;
    float                           m_accumulator = 0.0f;
    std::uint64_t                   m_clientTick = 0;
    std::uint64_t                   m_lastAcceptedSnapshotTick = 0;
    bool                            m_hasAcceptedSnapshot = false;

    std::uint64_t                    m_nextMapRequestId = 1;
    std::uint64_t                    m_lastGalaxyMapRequestId = 0;
    std::uint64_t                    m_lastSystemMapRequestId = 0;
    std::uint64_t                    m_lastDetailMapRequestId = 0;
    std::uint64_t                    m_lastHubMapRequestId = 0;
    int                              m_requestedSystemMapId = -1;
    world::celestial::DetailTarget   m_requestedDetailMapTarget;
    int                              m_requestedHubMapSystemId = -1;
    std::string                      m_requestedHubMapHubId;

    bool                             m_hasGalaxyMapSnapshot = false;
    bool                             m_hasSystemMapSnapshot = false;
    bool                             m_hasDetailMapSnapshot = false;
    bool                             m_hasHubMapSnapshot = false;
    int                              m_systemMapSnapshotId = -1;
    int                              m_hubMapSnapshotSystemId = -1;
    std::string                      m_hubMapSnapshotHubId;
    world::celestial::DetailTarget   m_detailMapSnapshotTarget;


    game::network::SnapshotMetadata m_lastGalaxyMapMetadata;
    game::network::SnapshotMetadata m_lastSystemMapMetadata;
    game::network::SnapshotMetadata m_lastDetailMapMetadata;
    game::network::SnapshotMetadata m_lastHubMapMetadata;

    world::celestial::GalaxyMapSnapshot m_galaxyMapSnapshot;
    world::celestial::SystemMapSnapshot m_systemMapSnapshot;
    world::celestial::DetailMapSnapshot m_detailMapSnapshot;
    world::celestial::HubMapSnapshot m_hubMapSnapshot;

    bool m_hasSessionSnapshot = false;
    game::simulation::ClientSessionSnapshot m_sessionSnapshot;

    bool m_hasStarAtlas = false;
    bool m_hasCelestialSnapshot = false;
    std::uint64_t m_starAtlasRevision = 0;
    game::network::SnapshotMetadata m_celestialSnapshotMetadata;
    world::celestial::StarAtlasDatabase m_starAtlas;
    world::celestial::CelestialSystemSnapshot m_celestialSnapshot;
};
