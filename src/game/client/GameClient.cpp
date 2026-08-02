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
}






void GameClient::submitInput(const ShipControlState& control)
{

    ShipControlState c = control;

    m_clientTick++;
    c.controlTick = m_clientTick;

    m_pendingInputs.push_back({ m_clientTick, c });

    game::network::ClientMessage msg;
    msg.clientTick = m_clientTick;
    msg.type = game::network::ClientMessageType::ControlInput;
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

bool GameClient::readyForGameplay() const
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

    world::celestial::StarAtlasDatabase atlas;
    while (m_transport->receiveStarAtlas(atlas))
    {
        m_starAtlas = std::move(atlas);
        m_hasStarAtlas = true;
    }

    return m_hasStarAtlas;
}

bool GameClient::requestCelestialSnapshot()
{
    m_transport->requestCelestialSnapshot();

    world::celestial::CelestialSystemSnapshot snapshot;
    while (m_transport->receiveCelestialSnapshot(snapshot))
    {
        m_celestialSnapshot = std::move(snapshot);
        m_hasCelestialSnapshot = true;
    }

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
                    if (typedResponse.requestId <
                        m_lastGalaxyMapRequestId)
                    {
                        return;
                    }

                    m_galaxyMapSnapshot =
                        std::move(typedResponse.snapshot);
                    m_hasGalaxyMapSnapshot = true;
                }
                else if constexpr (
                    std::is_same_v<
                        ResponseT,
                        game::network::SystemMapResponse>)
                {
                    if (typedResponse.requestId <
                        m_lastSystemMapRequestId)
                    {
                        return;
                    }

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
                    if (typedResponse.requestId < m_lastDetailMapRequestId)
                        return;
                    m_detailMapSnapshot = std::move(typedResponse.snapshot);
                    m_detailMapSnapshotTarget = typedResponse.target;
                    m_hasDetailMapSnapshot = true;
                }
                else if constexpr (
                    std::is_same_v<ResponseT,
                        game::network::HubMapResponse>)
                {
                    if (typedResponse.requestId < m_lastHubMapRequestId)
                        return;
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
        m_sessionSnapshot = snapshot.session;
        m_hasSessionSnapshot = true;
        m_world.applySnapshot(snapshot);

        while (!m_pendingInputs.empty() &&
               m_pendingInputs.front().controlTick <= snapshot.snapshotTick)
        {
            m_pendingInputs.pop_front();
        }
    }

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

    m_world.update(dt);
}






void GameClient::reconcile(
    const SimulationSnapshot& snapshot,
    const WorldParams& world,
    float fixedDt)
{
    // 1️⃣ Удаляем подтверждённые инпуты
    while (!m_pendingInputs.empty() &&
           m_pendingInputs.front().controlTick <= snapshot.snapshotTick)
    {
        m_pendingInputs.pop_front();
    }

    // Authoritative ship state. Reference-frame local coordinates are
    // the source of truth when the server supplies a valid frame.
    const ShipSnapshot* authoritativeShip = nullptr;

    for (const auto& s : snapshot.ships)
    {
        if (s.id == m_playerId)
        {
            authoritativeShip = &s;
            break;
        }
    }

    if (!authoritativeShip)
        return;

    // 3️⃣ Найти текущий клиентский корабль
    const auto& ships = m_world.ships();
    auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;

    const auto& clientShip = it->second;

    // 4️⃣ Посчитать ошибку


    double error = 0.0;

    const bool sameFrame =
        clientShip.referenceFrame.valid &&
        authoritativeShip->referenceFrame.valid &&
        clientShip.referenceFrame.type == authoritativeShip->referenceFrame.type &&
        clientShip.referenceFrame.bodyId == authoritativeShip->referenceFrame.bodyId &&
        clientShip.referenceFrame.hubId == authoritativeShip->referenceFrame.hubId &&
        clientShip.referenceFrame.moduleId == authoritativeShip->referenceFrame.moduleId;

    if (sameFrame)
    {
        error = glm::length(
            authoritativeShip->referenceFrame.localPositionMeters -
            clientShip.referenceFrame.localPositionMeters
        );
    }
    else
    {
        error = glm::length(
            world::coordinates::relativeMeters(
                authoritativeShip->transform.worldPosition,
                clientShip.transform.worldPosition
            )
        );
    }

    if (error > 0.01)
    {
        m_world.applySoftCorrection(
            m_playerId,
            *authoritativeShip
        );
    }


    // 6️⃣ Переигрываем неподтверждённые инпуты
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


