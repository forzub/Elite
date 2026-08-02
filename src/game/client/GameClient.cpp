#include <iostream>
#include <type_traits>
#include <utility>
#include "GameClient.h"
#include "src/game/client/ClientWorldState.h"
#include "src/game/network/ClientMessage.h"

GameClient::GameClient(ITransport* transport, EntityId playerId)
    : m_transport(transport)
    , m_playerId(playerId)
{
    m_connectionState = m_transport
        ? ClientConnectionState::Connecting
        : ClientConnectionState::Disconnected;
}

void GameClient::beginSynchronization()
{
    if (!m_transport)
    {
        failSynchronization("Client transport is not available");
        return;
    }

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

    ShipControlState c = control;

    m_clientTick++;
    c.controlTick = m_clientTick;

    m_pendingInputs.push_back({ m_clientTick, c });

    game::network::ClientMessage msg;
    msg.clientTick = m_clientTick;
    msg.payload = c;

    m_transport->sendClientMessage(m_playerId, msg);
    // m_transport->sendInput(m_playerId, c);

}



void GameClient::sendMessage(const game::network::ClientMessage& msg)
{
    m_transport->sendClientMessage(m_playerId, msg);
}


bool GameClient::requestGalaxyMapSnapshot()
{
    game::network::GalaxyMapRequest request;
    request.requestId = m_nextMapRequestId++;
    m_lastGalaxyMapRequestId = request.requestId;

    m_transport->sendMapRequest(request);
    receiveMapResponses();

    return m_hasGalaxyMapSnapshot;
}


bool GameClient::requestSystemMapSnapshot(int systemId)
{
    game::network::SystemMapRequest request;
    request.requestId = m_nextMapRequestId++;
    request.systemId = systemId;
    m_lastSystemMapRequestId = request.requestId;
    m_requestedSystemMapId = systemId;

    m_transport->sendMapRequest(request);
    receiveMapResponses();

    return
        m_hasSystemMapSnapshot &&
        m_systemMapSnapshotId == systemId;
}


bool GameClient::requestDetailMapSnapshot(
    const world::celestial::DetailTarget& target)
{
    if (!target.valid())
        return false;

    game::network::DetailMapRequest request;
    request.requestId = m_nextMapRequestId++;
    request.target = target;
    m_lastDetailMapRequestId = request.requestId;
    m_requestedDetailMapTarget = target;
    m_transport->sendMapRequest(request);
    receiveMapResponses();

    return m_hasDetailMapSnapshot &&
        m_detailMapSnapshotTarget == target;
}

bool GameClient::requestHubMapSnapshot(
    int systemId,
    const std::string& hubId)
{
    if (systemId < 0 || hubId.empty())
        return false;

    game::network::HubMapRequest request;
    request.requestId = m_nextMapRequestId++;
    request.systemId = systemId;
    request.hubId = hubId;
    m_lastHubMapRequestId = request.requestId;
    m_requestedHubMapSystemId = systemId;
    m_requestedHubMapHubId = hubId;
    m_transport->sendMapRequest(request);
    receiveMapResponses();

    return m_hasHubMapSnapshot &&
        m_hubMapSnapshotSystemId == systemId &&
        m_hubMapSnapshotHubId == hubId;
}


const world::celestial::GalaxyMapSnapshot*
GameClient::galaxyMapSnapshot() const
{
    return m_hasGalaxyMapSnapshot
        ? &m_galaxyMapSnapshot
        : nullptr;
}


const world::celestial::SystemMapSnapshot*
GameClient::systemMapSnapshot(int systemId) const
{
    if (!m_hasSystemMapSnapshot ||
        m_systemMapSnapshotId != systemId)
    {
        return nullptr;
    }

    return &m_systemMapSnapshot;
}


const world::celestial::DetailMapSnapshot*
GameClient::detailMapSnapshot(
    const world::celestial::DetailTarget& target) const
{
    if (!m_hasDetailMapSnapshot ||
        m_detailMapSnapshotTarget != target)
        return nullptr;
    return &m_detailMapSnapshot;
}

const world::celestial::HubMapSnapshot*
GameClient::hubMapSnapshot(
    int systemId,
    const std::string& hubId) const
{
    if (!m_hasHubMapSnapshot ||
        m_hubMapSnapshotSystemId != systemId ||
        m_hubMapSnapshotHubId != hubId)
        return nullptr;
    return &m_hubMapSnapshot;
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
        m_hasStarAtlas &&
        m_hasCelestialSnapshot)
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
    m_transport->requestStarAtlas();

    game::network::StarAtlasResponse response;
    while (m_transport->receiveStarAtlas(response))
    {
        if (m_hasStarAtlas &&
            response.metadata.catalogRevision < m_starAtlasRevision)
        {
            continue;
        }

        m_starAtlasRevision = response.metadata.catalogRevision;
        m_starAtlas = std::move(response.atlas);
        m_hasStarAtlas = true;
    }

    refreshConnectionState();
    return m_hasStarAtlas;
}

bool GameClient::requestCelestialSnapshot()
{
    m_transport->requestCelestialSnapshot();

    game::network::CelestialSnapshotResponse response;
    while (m_transport->receiveCelestialSnapshot(response))
    {
        if (m_hasCelestialSnapshot &&
            response.metadata.serverTick <
                m_celestialSnapshotMetadata.serverTick)
        {
            continue;
        }

        m_celestialSnapshotMetadata = response.metadata;
        m_celestialSnapshot = std::move(response.snapshot);
        m_hasCelestialSnapshot = true;
    }

    refreshConnectionState();
    return m_hasCelestialSnapshot;
}

const world::celestial::StarAtlasDatabase* GameClient::starAtlas() const
{
    return m_hasStarAtlas ? &m_starAtlas : nullptr;
}

const world::celestial::CelestialSystemSnapshot*
GameClient::celestialSnapshot() const
{
    return m_hasCelestialSnapshot ? &m_celestialSnapshot : nullptr;
}


void GameClient::receiveMapResponses()
{
    game::network::MapResponse response;

    while (m_transport->receiveMapResponse(response))
    {
        std::visit(
            [this](auto&& typedResponse)
            {
                using ResponseT =
                    std::decay_t<decltype(typedResponse)>;

                if constexpr (
                    std::is_same_v<
                        ResponseT,
                        game::network::GalaxyMapResponse>)
                {
                    if (typedResponse.requestId !=
                        m_lastGalaxyMapRequestId)
                    {
                        return;
                    }

                    m_lastGalaxyMapMetadata = typedResponse.metadata;
                    m_galaxyMapSnapshot =
                        std::move(typedResponse.snapshot);
                    m_hasGalaxyMapSnapshot = true;
                }
                else if constexpr (
                    std::is_same_v<
                        ResponseT,
                        game::network::SystemMapResponse>)
                {
                    if (typedResponse.requestId !=
                        m_lastSystemMapRequestId ||
                        typedResponse.systemId != m_requestedSystemMapId)
                    {
                        return;
                    }

                    m_lastSystemMapMetadata = typedResponse.metadata;
                    m_systemMapSnapshot =
                        std::move(typedResponse.snapshot);
                    m_systemMapSnapshotId =
                        typedResponse.systemId;
                    m_hasSystemMapSnapshot = true;
                }
                else if constexpr (
                    std::is_same_v<ResponseT,
                        game::network::DetailMapResponse>)
                {
                    if (typedResponse.requestId != m_lastDetailMapRequestId ||
                        typedResponse.target != m_requestedDetailMapTarget)
                        return;
                    m_lastDetailMapMetadata = typedResponse.metadata;
                    m_detailMapSnapshot = std::move(typedResponse.snapshot);
                    m_detailMapSnapshotTarget = typedResponse.target;
                    m_hasDetailMapSnapshot = true;
                }
                else if constexpr (
                    std::is_same_v<ResponseT,
                        game::network::HubMapResponse>)
                {
                    if (typedResponse.requestId != m_lastHubMapRequestId ||
                        typedResponse.systemId != m_requestedHubMapSystemId ||
                        typedResponse.hubId != m_requestedHubMapHubId)
                        return;
                    m_lastHubMapMetadata = typedResponse.metadata;
                    m_hubMapSnapshot = std::move(typedResponse.snapshot);
                    m_hubMapSnapshotSystemId = typedResponse.systemId;
                    m_hubMapSnapshotHubId = std::move(typedResponse.hubId);
                    m_hasHubMapSnapshot = true;
                }
            },
            std::move(response)
        );
    }
}







void GameClient::update(
    float dt,
    float fixedDt)
{
    m_accumulator += dt;

    receiveMapResponses();

    SimulationSnapshot snapshot;

    while (m_transport->receiveSnapshot(snapshot))
    {
        if (m_hasAcceptedSnapshot &&
            snapshot.metadata.serverTick <= m_lastAcceptedSnapshotTick)
        {
            continue;
        }

        m_lastAcceptedSnapshotTick = snapshot.metadata.serverTick;
        m_hasAcceptedSnapshot = true;

        m_sessionSnapshot = snapshot.session;
        m_hasSessionSnapshot = true;

        // Every accepted snapshot is a complete authoritative baseline.
        // Apply it once, discard acknowledged controls, then replay only
        // commands that the server has not processed yet.
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

        replayPendingInputs(
            m_sessionSnapshot.predictionWorldParams,
            fixedDt
        );
    }

    refreshConnectionState();

    if (readyForGameplay())
    {
        const WorldParams& predictionWorld =
            m_sessionSnapshot.predictionWorldParams;

        while (m_accumulator >= fixedDt)
        {
            if (!m_pendingInputs.empty())
            {
                const auto& last = m_pendingInputs.back();

                m_world.predict(
                    m_playerId,
                    last.control,
                    predictionWorld,
                    fixedDt
                );
            }

            m_accumulator -= fixedDt;
        }
    }
    else
    {
        // Do not accumulate a prediction debt while startup data is incomplete.
        m_accumulator = 0.0f;
    }

    m_world.update(dt);
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






const ClientWorldState& GameClient::world() const
{
    return m_world;
}

ClientWorldState& GameClient::world()
{
    return m_world;
}


