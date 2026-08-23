#pragma once

#include <unordered_map>
#include <deque>
#include <cstddef>
#include <cstdint>
#include <vector>


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
#include "src/game/server/ServerSessionRegistry.h"
#include "src/game/server/PlayerRegistry.h"
#include "src/game/server/ShipInstanceRegistry.h"
#include "src/game/server/ControlRegistry.h"
#include "src/game/server/ShipOwnershipRegistry.h"
#include "src/game/server/ReplicationInterestPolicy.h"
#include "src/game/server/ReplicationPublicationPolicy.h"
#include "src/world/celestial/SystemMapTypes.h"

struct ServerQueueDiagnostics
{
    std::uint64_t staleControlCommands = 0;
    std::uint64_t droppedShipCommands = 0;
    std::uint64_t droppedMapRequests = 0;
    std::uint64_t droppedMapResponses = 0;
    std::uint64_t rejectedSessionMessages = 0;
};

class GameServer
{
public:
    explicit GameServer(std::size_t bootstrapPlayerSlotCount = 1);

    void update(double dt);

    void submitCommand(EntityId shipId, const ShipControlState& control);

    const SimulationSnapshot& snapshot() const;
    bool copySnapshotForSession(
        game::network::ServerSessionId sessionId,
        SimulationSnapshot& outSnapshot
    ) const;

    // Full field-retained baseline used for initial connection hydration.
    bool copyHydratedSnapshotForSession(
        game::network::ServerSessionId sessionId,
        SimulationSnapshot& outSnapshot
    ) const;

    // Compose one sparse per-session packet from runner-owned cadence/lifecycle
    // selection. Hydration ids are sourced from canonical retained server state.
    bool copySparseSnapshotForSession(
        game::network::ServerSessionId sessionId,
        const game::server::ReplicationPublicationSelection& selection,
        SimulationSnapshot& outSnapshot
    ) const;

    std::vector<game::navigation::OwnedNavigationAsset>
    ownedNavigationAssetsForSession(
        game::network::ServerSessionId sessionId
    ) const;

    game::server::ShipReplicationInterestPlan
    shipReplicationInterestPlanForSession(
        game::network::ServerSessionId sessionId
    ) const;

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

    game::network::ServerSessionId createPlayerSession(
        PlayerId playerId
    );
    bool disconnectPlayerSession(game::network::ServerSessionId sessionId);
    PlayerId playerForSession(
        game::network::ServerSessionId sessionId
    ) const noexcept;
    EntityId controlledEntityForSession(
        game::network::ServerSessionId sessionId
    ) const noexcept;
    ShipInstanceId controlledShipInstanceForSession(
        game::network::ServerSessionId sessionId
    ) const noexcept;
    std::size_t connectedPlayerSessionCount() const noexcept;
    std::vector<PlayerId> playerIdentities() const;
    PlayerId primaryPlayerIdentity() const noexcept
    {
        return m_primaryPlayerId;
    }

    void receiveClientMessage(
        game::network::ServerSessionId sessionId,
        const game::network::ClientMessage& msg
    );
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

    bool enqueueMapRequest(
        game::network::ServerSessionId sessionId,
        const game::network::MapRequest& request
    );
    bool popMapResponse(
        game::network::ServerSessionId& outSessionId,
        game::network::MapResponse& outResponse
    );

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
            m_simulation.activeCelestialSystemId()
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

    bool navigationStateForSession(
        game::network::ServerSessionId sessionId,
        world::celestial::PlayerNavigationState& outNavigation
    ) const;

    world::celestial::GalaxyMapSnapshot buildGalaxyMapSnapshot() const;
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
    struct PendingSessionMapRequest
    {
        game::network::ServerSessionId sessionId {};
        game::network::MapRequest request;
    };

    struct CompletedSessionMapResponse
    {
        game::network::ServerSessionId sessionId {};
        game::network::MapResponse response;
    };

    void processPendingMapRequests();
    void queueMapResponse(
        game::network::ServerSessionId sessionId,
        game::network::MapResponse response
    );

    void populateClientSessionSnapshot(
        SimulationSnapshot& snapshot
    ) const;
    void resetSessionControlState(
        EntityId controlledEntityId,
        const char* reason
    );
    world::celestial::PlayerNavigationState navigationStateForEntity(
        EntityId entityId
    ) const;

    mutable game::diagnostics::ServerDiagnostics m_diagnostics;
    GameSimulation m_simulation;
    game::server::PlayerRegistry m_players;
    game::server::ShipInstanceRegistry m_shipInstances;
    game::server::ControlRegistry m_controls;
    game::server::ShipOwnershipRegistry m_shipOwnership;
    game::server::ServerSessionRegistry m_sessions;
    PlayerId m_primaryPlayerId {};
    game::server::ReplicationInterestPolicy m_replicationInterestPolicy;


    static constexpr std::size_t MaxShipCommandsPerShip = 32;
    static constexpr std::size_t MaxPendingMapRequests = 64;
    static constexpr std::size_t MaxCompletedMapResponses = 64;

    ServerQueueDiagnostics m_queueDiagnostics;

    std::unordered_map<uint32_t, game::server::FixedStepControlQueue>
        m_controlStreams;
    std::unordered_map<uint32_t, std::deque<ClientShipCommand>> m_pendingClientShipCommands;
    std::deque<PendingSessionMapRequest> m_pendingMapRequests;
    std::deque<CompletedSessionMapResponse> m_completedMapResponses;
    std::uint64_t m_serverTick = 0;
    std::uint64_t m_universeTimelineRevision = 1;
    world::time::UniverseClock m_universeClock;
    double m_lastUniverseTimeSeconds = 0.0;
    int m_appliedSimulationContextSystemId = -1;
    uint32_t m_snapshotInterval = 3;
    SimulationSnapshot m_lastSnapshot;

    // Field-retained canonical source for late-join/re-entry hydration. Ordinary
    // published snapshots intentionally omit heavy graph fields when unchanged.
    SimulationSnapshot m_canonicalReplicationSnapshot;

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
    double m_systemMembershipRadiusAu = 100.0;

    bool m_debugFastUniverseTime = false;
    double m_debugFastUniverseTimeScale = 10000.0;
    int m_debugFastUniverseTimeTraceFrames = 0;


    const world::celestial::CelestialSystemSnapshot*
    celestialSnapshotForSystem(int systemId) const;

    int resolveSingleActiveSimulationSystemId() const;
    void applyCelestialOrbitParentParameters(int systemId);
};
