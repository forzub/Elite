#pragma once

#include <unordered_map>
#include <deque>


#include "src/game/simulation/GameSimulation.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/network/ClientMessage.h"
#include "src/scene/EntityID.h"
#include "src/game/network/ClientShipCommand.h"

#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialSystemRuntime.h"
#include "src/world/time/UniverseClock.h"
#include "src/world/celestial/SystemMapTypes.h"

class GameServer
{
public:
    GameServer();
    
    void update(double dt);

    void submitCommand(EntityId shipId, const ShipControlState& control);

    const SimulationSnapshot& snapshot() const;

    EntityId playerId() const;

    WorldParams& world();

    bool debugDestroyShipModule(EntityId shipId, const std::string& moduleId);
    bool debugRestoreShipModule(EntityId shipId, const std::string& moduleId);
    bool debugResetShipStructure(EntityId shipId);
    void debugResetAllShipStructures();

    bool debugDetachShipModule(EntityId id, const std::string& moduleId);
    bool debugReattachShipModule(EntityId id, const std::string& moduleId);
    bool startShipRepairJob(EntityId id, const std::string& moduleId);

    bool ejectShipCockpitCapsule(EntityId id);
    bool debugHangShipModule(EntityId id, const std::string& moduleId);
    bool debugReevaluateShipStructure(EntityId id);

    void receiveClientMessage(EntityId playerId, const game::network::ClientMessage& msg);
    bool startBestRepairJobForFirstMissingSlot(EntityId targetShipId);

    bool startBestRepairJobForMissingSlot(
        EntityId targetShipId,
        const std::string& targetModuleId
    );

    bool debugSetShipStructuralLinkHealth(
        EntityId id,
        const std::string& linkId,
        float health,
        bool destroyed
    );

    void debugRefreshSnapshot();

    const world::celestial::StarAtlasDatabase& starAtlas() const
    {
        return m_starAtlas;
    }

    const world::celestial::CelestialSystemSnapshot& celestialSnapshot() const
    {
        return m_celestialRuntime.snapshot();
    }

    const world::time::UniverseClock& universeClock() const
    {
        return m_universeClock;
    }

    world::time::UniverseClock& universeClock()
    {
        return m_universeClock;
    }

    const world::celestial::PlayerNavigationState& playerNavigation() const
    {
        return m_playerNavigation;
    }

    world::celestial::GalaxyMapSnapshot buildGalaxyMapSnapshot() const;

    world::celestial::SystemMapSnapshot buildSystemMapSnapshot(
        int systemId
    ) const;

    world::celestial::PlanetMapSnapshot buildPlanetMapSnapshot(
        int systemId,
        const std::string& planetBodyId
    ) const;


    world::celestial::HubMapSnapshot buildHubMapSnapshot(
        int systemId,
        const std::string& hubId
    ) const;


    void setDebugFastUniverseTime(bool enabled);
    bool debugFastUniverseTime() const;
    double debugUniverseTimeScale() const;

private:

    GameSimulation                                                          m_simulation;
    
    
    std::unordered_map<uint32_t, std::deque<ShipControlState>> m_pendingCommands;
    std::unordered_map<uint32_t, std::deque<ClientShipCommand>> m_pendingClientShipCommands;
    uint32_t m_serverTick = 0;
    world::time::UniverseClock m_universeClock;
    uint32_t m_snapshotInterval = 3;
    SimulationSnapshot m_lastSnapshot;

    world::celestial::StarAtlasDatabase      m_starAtlas;
    world::celestial::CelestialSystemRuntime m_celestialRuntime;
    world::celestial::PlayerNavigationState  m_playerNavigation;

    bool m_debugFastUniverseTime = false;
    double m_debugFastUniverseTimeScale = 10000.0;
    int m_debugFastUniverseTimeTraceFrames = 0;


    void applyCelestialOrbitParentParameters();
};
