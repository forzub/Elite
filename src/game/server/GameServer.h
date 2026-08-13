#pragma once

#include <unordered_map>
#include <deque>
#include <cstddef>
#include <cstdint>


#include "src/game/simulation/GameSimulation.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/game/network/MapSnapshotMessage.h"
#include "src/scene/EntityID.h"
#include "src/game/network/ClientShipCommand.h"
#include "src/game/diagnostics/ServerDiagnostics.h"

#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/CelestialRuntimeRegistry.h"
#include "src/world/time/UniverseClock.h"
#include "src/game/server/ServerTimeContext.h"
#include "src/game/server/FixedStepControlQueue.h"
#include "src/world/celestial/SystemMapTypes.h"

struct ServerQueueDiagnostics
{
    std::uint64_t staleControlCommands = 0;
    std::uint64_t droppedShipCommands = 0;
    std::uint64_t droppedMapRequests = 0;
    std::uint64_t droppedMapResponses = 0;
};

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


    game::network::SnapshotMetadata protocolMetadata() const
    {
        game::network::SnapshotMetadata metadata;
        metadata.serverTick = m_serverTick;
        metadata.serverTimeSeconds = m_simulation.serverTime();
        metadata.universeTimeSeconds = m_universeClock.timeSeconds();
        metadata.universeTimelineRevision = m_universeTimelineRevision;
        return metadata;
    }

    void enqueueMapRequest(const game::network::MapRequest& request);
    bool popMapResponse(game::network::MapResponse& outResponse);

    const ServerQueueDiagnostics& queueDiagnostics() const
    {
        return m_queueDiagnostics;
    }

    const world::celestial::StarAtlasDatabase& starAtlas() const
    {
        return m_starAtlas;
    }

    const world::celestial::CelestialSystemSnapshot& celestialSnapshot() const
    {
        static const world::celestial::CelestialSystemSnapshot empty;
        const auto* snapshot = celestialSnapshotForSystem(
            m_playerNavigation.currentSystemId
        );
        return snapshot ? *snapshot : empty;
    }

    double serverTimeSeconds() const
    {
        return m_simulation.serverTime();
    }

    const world::time::UniverseClock& universeClock() const
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


    void setDiagnosticsSettings(
        const game::diagnostics::ServerDiagnosticsSettings& settings
    );

    const game::diagnostics::ServerDiagnosticsSettings&
    diagnosticsSettings() const;

    void resetDiagnosticsCapture();

    void setDebugFastUniverseTime(bool enabled);
    bool debugFastUniverseTime() const;

    void setDebugUniverseTimeSimulation(
        bool enabled,
        double timeScale
    );

    bool debugUniverseTimeSimulation() const;
    double debugUniverseTimeScale() const;
    double debugUniverseTimeConfiguredScale() const;

private:
    void processPendingMapRequests();

    void populateClientSessionSnapshot(
        SimulationSnapshot& snapshot
    ) const;

    mutable game::diagnostics::ServerDiagnostics m_diagnostics;
    GameSimulation m_simulation;


    static constexpr std::size_t MaxShipCommandsPerShip = 32;
    static constexpr std::size_t MaxPendingMapRequests = 64;
    static constexpr std::size_t MaxCompletedMapResponses = 64;

    ServerQueueDiagnostics m_queueDiagnostics;

    std::unordered_map<uint32_t, game::server::FixedStepControlQueue>
        m_controlStreams;
    std::unordered_map<uint32_t, std::deque<ClientShipCommand>> m_pendingClientShipCommands;
    std::deque<game::network::MapRequest> m_pendingMapRequests;
    std::deque<game::network::MapResponse> m_completedMapResponses;
    std::uint64_t m_serverTick = 0;
    std::uint64_t m_universeTimelineRevision = 1;
    world::time::UniverseClock m_universeClock;
    double m_lastUniverseTimeSeconds = 0.0;
    int m_appliedSimulationContextSystemId = -1;
    uint32_t m_snapshotInterval = 3;
    SimulationSnapshot m_lastSnapshot;
    bool m_forceSnapshotPublication = false;

    // Enabling accelerated universe time is a two-phase transition. The
    // request is recorded by the debug API, then the trajectory seed is
    // captured at the last complete authoritative epoch before the clock is
    // advanced by the accelerated delta.
    bool m_pendingUniverseTrajectoryDiagnosticEntry = false;
    double m_pendingUniverseTrajectoryDiagnosticEpochSeconds = 0.0;

    world::celestial::StarAtlasDatabase       m_starAtlas;
    // Mutable/authored map facts are server world state.  They deliberately
    // do not live in StarAtlas (physical catalog) and are never inferred by
    // the client from numeric system IDs.
    std::unordered_map<int, std::string> m_systemJurisdictions;
    world::celestial::CelestialRuntimeRegistry m_celestialRuntimes;
    world::celestial::PlayerNavigationState  m_playerNavigation;
    double m_systemMembershipRadiusAu = 100.0;

    bool m_debugFastUniverseTime = false;
    double m_debugFastUniverseTimeScale = 10000.0;
    int m_debugFastUniverseTimeTraceFrames = 0;


    const world::celestial::CelestialSystemSnapshot*
    celestialSnapshotForSystem(int systemId) const;

    void synchronizePlayerSystemMembership();
    void applyCelestialOrbitParentParameters();
};
