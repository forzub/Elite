#include <algorithm>
#include <cmath>
#include <utility>
#include "GameClient.h"
#include "src/game/client/ClientWorldState.h"
#include "src/game/network/ClientMessage.h"

GameClient::GameClient(ITransport& transport)
    : m_transport(transport)
    , m_maps(transport)
    , m_catalogs(transport)
{
    m_connectionState = ClientConnectionState::Connecting;
}

void GameClient::beginSynchronization()
{
    m_connectionError.clear();
    m_connectionState = ClientConnectionState::Synchronizing;
    m_maps.resetPendingRequests();
    m_catalogs.resetPendingRequests();
    m_pendingInputs.clear();
    m_predictionSuspended = false;
    m_accumulator = 0.0f;
    m_serverClock.reset();
    m_presentationClock.reset();
    m_universeTimeline.reset();
    m_timeSyncSequence = 0;
    m_nextTimeSyncLocalSeconds = 0.0;
    m_gameplayFramePrepared = false;
    m_preparedAcceptedSnapshot = false;

    requestStarAtlas();
    refreshConnectionState();
}

void GameClient::failSynchronization(std::string message)
{
    m_connectionError = std::move(message);
    m_connectionState = ClientConnectionState::Failed;
}

ClientConnectionState GameClient::connectionState() const
{
    return m_connectionState;
}

const std::string& GameClient::connectionError() const
{
    return m_connectionError;
}






void GameClient::submitInput(const ShipControlState& control)
{
    // Input sampling is frame-rate dependent; network commands and prediction
    // are emitted only on fixed client steps in updateGameplay().
    m_latestControl = control;
    m_hasLatestControl = true;
}



void GameClient::sendMessage(const game::network::ClientMessage& msg)
{
    m_transport.sendClientMessage(msg);
}


bool GameClient::requestGalaxyMapSnapshot(bool forceRefresh)
{
    return m_maps.requestGalaxy(forceRefresh);
}

bool GameClient::requestSystemMapSnapshot(int systemId, bool forceRefresh)
{
    return m_maps.requestSystem(systemId, forceRefresh);
}

bool GameClient::requestDetailMapSnapshot(
    const world::celestial::DetailTarget& target,
    bool forceRefresh)
{
    return m_maps.requestDetail(target, forceRefresh);
}

bool GameClient::requestHubMapSnapshot(
    int systemId,
    const std::string& hubId,
    bool forceRefresh)
{
    return m_maps.requestHub(systemId, hubId, forceRefresh);
}

game::client::ClientRequestStatus GameClient::galaxyMapRequestStatus() const
{
    return m_maps.galaxyStatus();
}

game::client::ClientRequestStatus GameClient::systemMapRequestStatus() const
{
    return m_maps.systemStatus();
}

game::client::ClientRequestStatus GameClient::detailMapRequestStatus() const
{
    return m_maps.detailStatus();
}

game::client::ClientRequestStatus GameClient::hubMapRequestStatus() const
{
    return m_maps.hubStatus();
}

const world::celestial::GalaxyMapSnapshot*
GameClient::galaxyMapSnapshot() const
{
    return m_maps.galaxy();
}

const world::celestial::SystemMapSnapshot*
GameClient::systemMapSnapshot(int systemId) const
{
    return m_maps.system(systemId);
}

const world::celestial::DetailMapSnapshot*
GameClient::detailMapSnapshot(
    const world::celestial::DetailTarget& target) const
{
    return m_maps.detail(target);
}

const world::celestial::HubMapSnapshot*
GameClient::hubMapSnapshot(
    int systemId,
    const std::string& hubId) const
{
    return m_maps.hub(systemId, hubId);
}

bool GameClient::hasSessionSnapshot() const
{
    return m_hasSessionSnapshot;
}

EntityId GameClient::playerId() const
{
    return m_playerId;
}

bool GameClient::hasGameplayCoreState() const
{
    if (!m_hasSessionSnapshot || !m_hasPlayerIdentity)
        return false;

    const auto& ships = m_world.ships();
    const auto it = ships.find(m_playerId.value);

    if (it == ships.end())
        return false;

    const ClientShipState& ship = it->second;
    return
        ship.descriptor != nullptr &&
        ship.assembly != nullptr;
}

void GameClient::refreshConnectionState()
{
    if (m_connectionState == ClientConnectionState::Failed ||
        m_connectionState == ClientConnectionState::Disconnected)
    {
        return;
    }

    if (hasGameplayCoreState() &&
        m_serverClock.synchronized() &&
        m_universeTimeline.synchronized() &&
        m_catalogs.hasStarAtlas() &&
        m_catalogs.hasCelestialSnapshot())
    {
        m_connectionState = ClientConnectionState::Ready;
    }
    else if (m_connectionState != ClientConnectionState::Connecting)
    {
        m_connectionState = ClientConnectionState::Synchronizing;
    }
}

bool GameClient::readyForGameplay() const
{
    return m_connectionState == ClientConnectionState::Ready;
}


const game::simulation::ClientSessionSnapshot&
GameClient::sessionSnapshot() const
{
    return m_sessionSnapshot;
}


const world::celestial::PlayerNavigationState&
GameClient::playerNavigation() const
{
    return m_sessionSnapshot.playerNavigation;
}


double GameClient::estimatedServerTimeSeconds() const
{
    return m_serverClock.synchronized()
        ? m_serverClock.estimatedServerTimeSeconds()
        : m_lastSimulationMetadata.serverTimeSeconds;
}

double GameClient::renderServerTimeSeconds() const
{
    if (m_presentationClock.ready())
        return m_presentationClock.renderTimeSeconds();

    return std::max(
        0.0,
        estimatedServerTimeSeconds() -
            RenderInterpolationDelaySeconds
    );
}

double GameClient::universeTimeSeconds() const
{
    return m_universeTimeline.synchronized()
        ? m_universeTimeline.timeAtServerTime(
            estimatedServerTimeSeconds()
          )
        : m_sessionSnapshot.universeTimeSeconds;
}

double GameClient::renderUniverseTimeSeconds() const
{
    return m_universeTimeline.synchronized()
        ? m_universeTimeline.timeAtServerTime(
            renderServerTimeSeconds()
          )
        : m_sessionSnapshot.universeTimeSeconds;
}


bool GameClient::requestStarAtlas()
{
    const bool ready = m_catalogs.requestStarAtlas();
    refreshConnectionState();
    return ready;
}

bool GameClient::resolveCelestialSnapshot(bool forceRefresh)
{
    if (!m_hasSessionSnapshot ||
        !m_universeTimeline.synchronized())
    {
        refreshConnectionState();
        return false;
    }

    const bool ready =
        m_catalogs.resolveCelestialSnapshot(
            m_sessionSnapshot.playerNavigation.currentSystemId,
            renderUniverseTimeSeconds(),
            m_lastSimulationMetadata,
            forceRefresh
        );

    refreshConnectionState();
    return ready;
}

const world::celestial::StarAtlasDatabase* GameClient::starAtlas() const
{
    return m_catalogs.starAtlas();
}

const world::celestial::CelestialSystemSnapshot*
GameClient::celestialSnapshot() const
{
    return m_catalogs.celestialSnapshot();
}

void GameClient::updateTimeSynchronization(double wallDeltaSeconds)
{
    const double safeWallDelta =
        std::isfinite(wallDeltaSeconds)
            ? std::max(0.0, wallDeltaSeconds)
            : 0.0;

    m_serverClock.advance(safeWallDelta);

    game::network::TimeSyncResponse response;
    while (m_transport.receiveTimeSyncResponse(response))
    {
        m_serverClock.addSyncSample(
            response.clientSendTimeSeconds,
            m_serverClock.localTimeSeconds(),
            response.serverReceiveTimeSeconds
        );
    }

    sendTimeSyncRequestIfDue();
}

void GameClient::sendTimeSyncRequestIfDue()
{
    const double localNow = m_serverClock.localTimeSeconds();

    if (localNow + 1.0e-12 < m_nextTimeSyncLocalSeconds)
        return;

    game::network::TimeSyncRequest request;
    request.sequence = ++m_timeSyncSequence;
    request.clientSendTimeSeconds = localNow;
    m_transport.sendTimeSyncRequest(request);

    const double interval =
        m_serverClock.synchronized()
            ? SteadyTimeSyncIntervalSeconds
            : StartupTimeSyncIntervalSeconds;

    m_nextTimeSyncLocalSeconds = localNow + interval;
}

bool GameClient::updateSynchronization(double wallDeltaSeconds)
{
    const float serviceDt = static_cast<float>(
        std::max(0.0, wallDeltaSeconds)
    );

    game::network::SessionWelcome welcome;
    while (m_transport.receiveSessionWelcome(welcome))
    {
        if (welcome.controlledEntityId.value == 0)
        {
            failSynchronization(
                "Server session welcome has no controlled entity"
            );
            return false;
        }

        if (m_hasPlayerIdentity &&
            welcome.controlledEntityId.value != m_playerId.value)
        {
            // Control transfer/respawn needs an explicit protocol transition:
            // prediction history and SpaceState ownership are keyed by this id.
            failSynchronization(
                "Controlled entity changed without a session transition"
            );
            return false;
        }

        m_playerId = welcome.controlledEntityId;
        m_hasPlayerIdentity = true;
    }

    /*
        Simulation metadata establishes the active universe-timeline branch.
        Consume it before map responses so a response can never be accepted
        against the previous branch merely because the network queues happened
        to be pumped in the opposite order.
    */
    updateTimeSynchronization(wallDeltaSeconds);
    m_catalogs.update(serviceDt);

    if (!m_catalogs.hasStarAtlas())
        m_catalogs.requestStarAtlas();

    if (m_catalogs.starAtlasStatus() ==
        game::client::ClientRequestStatus::TimedOut)
    {
        failSynchronization(
            "Timed out while synchronizing the world catalog"
        );
        return false;
    }

    bool acceptedSnapshot = false;

    SimulationSnapshot snapshot;
    while (m_transport.receiveSnapshot(snapshot))
    {
        if (snapshot.metadata.universeTimelineRevision !=
            snapshot.session.universeTimelineRevision)
        {
            failSynchronization(
                "Simulation snapshot timeline revision mismatch"
            );
            return false;
        }

        if (m_hasAcceptedSnapshot &&
            snapshot.metadata.serverTick <= m_lastAcceptedSnapshotTick)
        {
            continue;
        }

        const bool timelineRevisionChanged =
            m_hasSessionSnapshot &&
            snapshot.session.universeTimelineRevision !=
                m_sessionSnapshot.universeTimelineRevision;

        const bool playerSystemChanged =
            m_hasSessionSnapshot &&
            snapshot.session.playerNavigation.currentSystemId !=
                m_sessionSnapshot.playerNavigation.currentSystemId;

        acceptedSnapshot = true;
        m_lastAcceptedSnapshotTick = snapshot.metadata.serverTick;
        m_lastSimulationMetadata = snapshot.metadata;
        m_hasAcceptedSnapshot = true;
        m_sessionSnapshot = snapshot.session;
        m_hasSessionSnapshot = true;

        m_maps.setUniverseTimelineRevision(
            snapshot.session.universeTimelineRevision
        );

        if (timelineRevisionChanged || playerSystemChanged)
        {
            // Prediction history belongs to one universe-time branch and one
            // system-local coordinate domain. Rewind and inter-system transfer
            // are both hard reconciliation boundaries.
            m_pendingInputs.clear();
            m_predictionSuspended = false;
            m_accumulator = 0.0f;
            ++m_predictionResyncCount;
        }

        m_universeTimeline.synchronize(
            snapshot.metadata.serverTimeSeconds,
            snapshot.session.universeTimeSeconds,
            snapshot.session.universeTimeScale,
            snapshot.session.universeTimelineRevision
        );

        m_world.applySnapshot(snapshot);

        std::uint64_t acknowledgedControlTick = 0;
        for (const auto& ship : snapshot.ships)
        {
            if (ship.id == m_playerId)
            {
                acknowledgedControlTick = ship.acknowledgedControlTick;
                break;
            }
        }

        while (!m_pendingInputs.empty() &&
               m_pendingInputs.front().controlTick <= acknowledgedControlTick)
        {
            m_pendingInputs.pop_front();
        }

        if (m_predictionSuspended)
        {
            m_pendingInputs.clear();
            m_predictionSuspended = false;
        }
    }

    /*
        Keep one buffered presentation playhead. The server-clock estimator
        answers "what time is it on the server now?"; rendering needs a
        different question: "what delayed authoritative time can the current
        snapshot history represent smoothly?". A long frame/server hitch may
        separate those two by seconds, so the render playhead is constrained
        by the newest accepted snapshot instead of silently degenerating into
        latest-snapshot hold.
    */
    m_presentationClock.update(
        wallDeltaSeconds,
        estimatedServerTimeSeconds(),
        m_hasAcceptedSnapshot,
        m_lastSimulationMetadata.serverTimeSeconds
    );

    /*
        Map responses are branch-tagged too. Pump them only after the newest
        simulation snapshot has selected the active revision.
    */
    m_maps.update(serviceDt);

    (void)resolveCelestialSnapshot(acceptedSnapshot);
    refreshConnectionState();
    return acceptedSnapshot;
}

void GameClient::prepareGameplayFrame(double wallDeltaSeconds)
{
    m_preparedAcceptedSnapshot =
        updateSynchronization(wallDeltaSeconds);
    m_gameplayFramePrepared = true;
}

void GameClient::updateGameplay(
    float simulationDt,
    float fixedDt,
    double wallDeltaSeconds
)
{
    bool acceptedSnapshot = false;

    if (m_gameplayFramePrepared)
    {
        acceptedSnapshot = m_preparedAcceptedSnapshot;
    }
    else
    {
        /*
            Compatibility fallback for non-SpaceState callers. Production
            gameplay prepares synchronization before input so branch changes
            cannot land between map picking and rendering.
        */
        acceptedSnapshot =
            updateSynchronization(wallDeltaSeconds);
    }

    m_gameplayFramePrepared = false;
    m_preparedAcceptedSnapshot = false;

    if (!readyForGameplay())
    {
        m_accumulator = 0.0f;
        m_world.clearLocalPredictedPresentation();
        m_world.update(
            simulationDt,
            false,
            renderServerTimeSeconds()
        );
        return;
    }

    const bool trajectoryDebugMode =
        m_sessionSnapshot.universeTimeSimulation;

    if (trajectoryDebugMode)
    {
        m_pendingInputs.clear();
        m_predictionSuspended = false;
        m_accumulator = 0.0f;
    }
    else if (acceptedSnapshot)
    {
        replayPendingInputs(
            m_sessionSnapshot.predictionWorldParams,
            fixedDt
        );
    }

    m_world.clearLocalPredictedPresentation();

    if (!trajectoryDebugMode)
    {
        m_accumulator += simulationDt;
        const WorldParams& predictionWorld =
            m_sessionSnapshot.predictionWorldParams;

        while (m_accumulator >= fixedDt)
        {
            sendAndPredictFixedStep(predictionWorld, fixedDt);
            m_accumulator -= fixedDt;
        }

        if (m_hasLatestControl && !m_predictionSuspended)
        {
            m_world.prepareLocalPredictedPresentation(
                m_playerId,
                m_latestControl,
                predictionWorld,
                m_accumulator,
                fixedDt
            );
        }
    }

    m_world.update(
        simulationDt,
        trajectoryDebugMode,
        renderServerTimeSeconds()
    );
}

void GameClient::update(
    float simulationDt,
    float fixedDt,
    double wallDeltaSeconds
)
{
    if (readyForGameplay())
    {
        updateGameplay(
            simulationDt,
            fixedDt,
            wallDeltaSeconds
        );
    }
    else
    {
        if (!m_gameplayFramePrepared)
            (void)updateSynchronization(wallDeltaSeconds);

        m_gameplayFramePrepared = false;
        m_preparedAcceptedSnapshot = false;
        m_accumulator = 0.0f;
        m_world.clearLocalPredictedPresentation();
        m_world.update(
            simulationDt,
            false,
            renderServerTimeSeconds()
        );
    }
}

void GameClient::sendAndPredictFixedStep(
    const WorldParams& world,
    float fixedDt)
{
    if (!m_hasLatestControl)
        return;

    ShipControlState control = m_latestControl;
    control.controlTick = ++m_clientTick;

    bool predictThisStep = !m_predictionSuspended;
    if (predictThisStep && m_pendingInputs.size() >= MaxPendingInputs)
    {
        // Silently dropping only the oldest inputs leaves a non-contiguous
        // replay history and produces invalid reconciliation. Fall back to
        // authoritative rendering until the next accepted snapshot instead.
        m_droppedPendingInputCount += m_pendingInputs.size();
        m_pendingInputs.clear();
        m_predictionSuspended = true;
        ++m_predictionResyncCount;
        predictThisStep = false;
    }

    if (predictThisStep)
    {
        TimedInput step;
        step.controlTick = control.controlTick;
        step.control = control;
        m_pendingInputs.push_back(step);
    }

    game::network::ClientMessage msg;
    msg.clientTick = control.controlTick;
    msg.payload = control;
    m_transport.sendClientMessage(msg);

    if (predictThisStep)
    {
        m_world.predict(
            m_playerId,
            control,
            world,
            fixedDt
        );
    }
}

void GameClient::replayPendingInputs(
    const WorldParams& world,
    float fixedDt)
{
    for (const auto& input : m_pendingInputs)
    {
        m_world.predict(
            m_playerId,
            input.control,
            world,
            fixedDt
        );
    }
}






const game::network::SnapshotMetadata&
GameClient::lastSimulationMetadata() const
{
    return m_lastSimulationMetadata;
}

const game::network::SnapshotMetadata&
GameClient::galaxyMapMetadata() const
{
    return m_maps.galaxyMetadata();
}

const game::network::SnapshotMetadata&
GameClient::systemMapMetadata() const
{
    return m_maps.systemMetadata();
}

const game::network::SnapshotMetadata&
GameClient::detailMapMetadata() const
{
    return m_maps.detailMetadata();
}

const game::network::SnapshotMetadata&
GameClient::hubMapMetadata() const
{
    return m_maps.hubMetadata();
}

const game::network::CatalogMetadata&
GameClient::starAtlasMetadata() const
{
    return m_catalogs.starAtlasMetadata();
}

const game::network::SnapshotMetadata&
GameClient::celestialMetadata() const
{
    return m_catalogs.celestialMetadata();
}

const ClientWorldState& GameClient::world() const
{
    return m_world;
}

ClientWorldState& GameClient::world()
{
    return m_world;
}


