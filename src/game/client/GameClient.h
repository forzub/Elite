#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include "src/game/client/ClientWorldState.h"
#include "src/game/client/ClientCatalogService.h"
#include "src/game/client/ClientMapService.h"
#include "src/game/client/ClientServerClock.h"
#include "src/game/client/ClientPresentationClock.h"
#include "src/game/client/ClientUniverseTimeline.h"
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
    explicit GameClient(ITransport& transport);

    void submitInput(const ShipControlState& control);
    bool updateSynchronization(double wallDeltaSeconds);
    void prepareGameplayFrame(double wallDeltaSeconds);
    void updateGameplay(
        float simulationDt,
        float fixedDt,
        double wallDeltaSeconds
    );
    void update(
        float simulationDt,
        float fixedDt,
        double wallDeltaSeconds
    );

    const ClientWorldState& world() const;
    ClientWorldState& world();

    void sendMessage(const game::network::ClientMessage& msg);

    bool requestGalaxyMapSnapshot(bool forceRefresh = false);
    bool requestSystemMapSnapshot(int systemId, bool forceRefresh = false);
    bool requestDetailMapSnapshot(
        const world::celestial::DetailTarget& target,
        bool forceRefresh = false
    );
    bool requestHubMapSnapshot(
        int systemId,
        const std::string& hubId,
        bool forceRefresh = false
    );

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

    game::client::ClientRequestStatus galaxyMapRequestStatus() const;
    game::client::ClientRequestStatus systemMapRequestStatus() const;
    game::client::ClientRequestStatus detailMapRequestStatus() const;
    game::client::ClientRequestStatus hubMapRequestStatus() const;

    const game::network::SnapshotMetadata& lastSimulationMetadata() const;
    const game::network::SnapshotMetadata& galaxyMapMetadata() const;
    const game::network::SnapshotMetadata& systemMapMetadata() const;
    const game::network::SnapshotMetadata& detailMapMetadata() const;
    const game::network::SnapshotMetadata& hubMapMetadata() const;
    const game::network::CatalogMetadata& starAtlasMetadata() const;
    const game::network::SnapshotMetadata& celestialMetadata() const;

    std::uint64_t droppedPendingInputCount() const
    {
        return m_droppedPendingInputCount;
    }

    std::uint64_t predictionResyncCount() const
    {
        return m_predictionResyncCount;
    }

    bool predictionSuspended() const
    {
        return m_predictionSuspended;
    }

    void beginSynchronization();
    void failSynchronization(std::string message);
    ClientConnectionState connectionState() const;
    const std::string& connectionError() const;

    bool hasSessionSnapshot() const;
    EntityId playerId() const;
    bool readyForGameplay() const;
    const game::simulation::ClientSessionSnapshot&
        sessionSnapshot() const;
    const world::celestial::PlayerNavigationState&
        playerNavigation() const;

    double estimatedServerTimeSeconds() const;
    double renderServerTimeSeconds() const;
    double universeTimeSeconds() const;
    double renderUniverseTimeSeconds() const;

    bool requestStarAtlas();
    bool resolveCelestialSnapshot(bool forceRefresh = false);
    const world::celestial::StarAtlasDatabase* starAtlas() const;
    const world::celestial::CelestialSystemSnapshot*
        celestialSnapshot() const;


private:
    bool hasGameplayCoreState() const;
    void refreshConnectionState();
    void replayPendingInputs(const WorldParams& world, float fixedDt);
    void sendAndPredictFixedStep(const WorldParams& world, float fixedDt);
    void updateTimeSynchronization(double wallDeltaSeconds);
    void sendTimeSyncRequestIfDue();

private:
    ITransport&                     m_transport;
    ClientConnectionState           m_connectionState =
        ClientConnectionState::Disconnected;
    std::string                     m_connectionError;
    EntityId                        m_playerId {0};
    bool                            m_hasPlayerIdentity = false;

    ClientWorldState                m_world;

    struct TimedInput
    {
        std::uint64_t               controlTick = 0;
        ShipControlState            control;
    };

    static constexpr std::size_t MaxPendingInputs = 240;

    std::deque<TimedInput>          m_pendingInputs;
    std::uint64_t                   m_droppedPendingInputCount = 0;
    std::uint64_t                   m_predictionResyncCount = 0;
    bool                            m_predictionSuspended = false;
    ShipControlState                m_latestControl;
    bool                            m_hasLatestControl = false;
    float                           m_accumulator = 0.0f;
    std::uint64_t                   m_clientTick = 0;
    std::uint64_t                   m_lastAcceptedSnapshotTick = 0;
    bool                            m_hasAcceptedSnapshot = false;
    game::network::SnapshotMetadata m_lastSimulationMetadata;

    // Construction order is intentional: System-map presentation depends on
    // the client's local catalog/runtime and must never query server-owned
    // static celestial definitions.
    game::client::ClientCatalogService m_catalogs;
    game::client::ClientMapService m_maps;
    game::client::ClientServerClock m_serverClock;
    game::client::ClientPresentationClock m_presentationClock;
    game::client::ClientUniverseTimeline m_universeTimeline;

    std::uint64_t m_timeSyncSequence = 0;
    double m_nextTimeSyncLocalSeconds = 0.0;

    static constexpr double StartupTimeSyncIntervalSeconds = 0.050;
    static constexpr double SteadyTimeSyncIntervalSeconds = 2.0;
    static constexpr double RenderInterpolationDelaySeconds = 0.200;

    bool m_hasSessionSnapshot = false;
    game::simulation::ClientSessionSnapshot m_sessionSnapshot;

    bool m_gameplayFramePrepared = false;
    bool m_preparedAcceptedSnapshot = false;

};
