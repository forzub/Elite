#include <iostream>
#include <type_traits>
#include <utility>
#include "GameClient.h"
#include "src/game/client/ClientWorldState.h"
#include "src/game/network/ClientMessage.h"

GameClient::GameClient(ITransport& transport, EntityId playerId)
    : m_transport(transport)
    , m_playerId(playerId)
    , m_maps(transport)
    , m_catalogs(transport)
{
    m_connectionState = ClientConnectionState::Connecting;
}

void GameClient::beginSynchronization()
{
    m_connectionError.clear();
    m_connectionState = ClientConnectionState::Synchronizing;

    requestStarAtlas();
    requestCelestialSnapshot();
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
    m_transport.sendClientMessage(m_playerId, msg);
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

bool GameClient::hasGameplayCoreState() const
{
    if (!m_hasSessionSnapshot)
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


bool GameClient::requestStarAtlas()
{
    const bool ready = m_catalogs.requestStarAtlas();
    refreshConnectionState();
    return ready;
}

bool GameClient::requestCelestialSnapshot()
{
    const bool ready = m_catalogs.requestCelestialSnapshot();
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

bool GameClient::updateSynchronization()
{
    m_maps.pumpResponses();
    m_catalogs.pumpResponses();

    if (!m_catalogs.hasStarAtlas())
        m_catalogs.requestStarAtlas();
    if (!m_catalogs.hasCelestialSnapshot())
        m_catalogs.requestCelestialSnapshot();

    bool acceptedSnapshot = false;

    SimulationSnapshot snapshot;
    while (m_transport.receiveSnapshot(snapshot))
    {
        if (m_hasAcceptedSnapshot &&
            snapshot.metadata.serverTick <= m_lastAcceptedSnapshotTick)
        {
            continue;
        }

        acceptedSnapshot = true;
        m_lastAcceptedSnapshotTick = snapshot.metadata.serverTick;
        m_lastSimulationMetadata = snapshot.metadata;
        m_hasAcceptedSnapshot = true;
        m_sessionSnapshot = snapshot.session;
        m_hasSessionSnapshot = true;

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
    }

    refreshConnectionState();
    return acceptedSnapshot;
}

void GameClient::updateGameplay(float dt, float fixedDt)
{
    const bool acceptedSnapshot = updateSynchronization();

    if (!readyForGameplay())
    {
        m_accumulator = 0.0f;
        m_world.update(dt);
        return;
    }

    if (acceptedSnapshot)
    {
        replayPendingInputs(
            m_sessionSnapshot.predictionWorldParams,
            fixedDt
        );
    }

    m_accumulator += dt;
    const WorldParams& predictionWorld =
        m_sessionSnapshot.predictionWorldParams;

    while (m_accumulator >= fixedDt)
    {
        sendAndPredictFixedStep(predictionWorld, fixedDt);
        m_accumulator -= fixedDt;
    }

    m_world.update(dt);
}

void GameClient::update(float dt, float fixedDt)
{
    if (readyForGameplay())
        updateGameplay(dt, fixedDt);
    else
    {
        (void)updateSynchronization();
        m_accumulator = 0.0f;
        m_world.update(dt);
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

    TimedInput step;
    step.controlTick = control.controlTick;
    step.control = control;
    m_pendingInputs.push_back(step);

    game::network::ClientMessage msg;
    msg.clientTick = control.controlTick;
    msg.payload = control;
    m_transport.sendClientMessage(m_playerId, msg);

    m_world.predict(
        m_playerId,
        control,
        world,
        fixedDt
    );
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


