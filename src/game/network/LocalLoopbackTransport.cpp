#include "LocalLoopbackTransport.h"
#include "src/game/server/GameServer.h"
#include "src/game/network/ClientMessage.h"
#include <iostream>
#include <type_traits>
#include <utility>


LocalLoopbackTransport::LocalLoopbackTransport(GameServer* server)
    : m_server(server)
{
}



void LocalLoopbackTransport::sendInput(
    EntityId id,
    const ShipControlState& control)
{
    m_server->submitCommand(id, control);
}




bool LocalLoopbackTransport::receiveSnapshot(
    SimulationSnapshot& outSnapshot)
{
    if (m_incoming.empty())
        return false;

    outSnapshot = m_incoming.front();
    m_incoming.pop();
    return true;
}


void LocalLoopbackTransport::enqueueCurrentSnapshotImmediately()
{
    const SimulationSnapshot snap = m_server->snapshot();

    m_incoming.push(snap);
    m_hasLastQueuedSnapshot = true;
    m_lastQueuedSnapshotTick = snap.snapshotTick;
    m_lastQueuedServerTime = snap.serverTime;
}


void LocalLoopbackTransport::update(float dt)
{
    const SimulationSnapshot snap = m_server->snapshot();

    const bool snapshotChanged =
        !m_hasLastQueuedSnapshot ||
        snap.snapshotTick != m_lastQueuedSnapshotTick ||
        snap.serverTime != m_lastQueuedServerTime;

    // ВАЖНО:
    // Не кладём в latency buffer один и тот же snapshot много раз.
    // Иначе ClientWorldState получает дубли serverTime/snapshotTick,
    // а интерполяция вращения станции превращается в ступеньки.
    if (snapshotChanged)
    {
        m_latencyBuffer.push_back({
            snap,
            m_fakeLatency
        });

        m_hasLastQueuedSnapshot = true;
        m_lastQueuedSnapshotTick = snap.snapshotTick;
        m_lastQueuedServerTime = snap.serverTime;
    }

    for (auto& s : m_latencyBuffer)
    {
        s.delay -= dt;
    }

    while (!m_latencyBuffer.empty() &&
           m_latencyBuffer.front().delay <= 0.0f)
    {
        if ((rand() / static_cast<float>(RAND_MAX)) < m_packetLoss)
        {
            m_latencyBuffer.erase(m_latencyBuffer.begin());
            continue;
        }

        m_incoming.push(m_latencyBuffer.front().snapshot);
        m_latencyBuffer.erase(m_latencyBuffer.begin());
    }
}





void LocalLoopbackTransport::sendClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    m_server->receiveClientMessage(playerId, msg);
}


void LocalLoopbackTransport::sendMapRequest(
    const game::network::MapRequest& request)
{
    std::visit(
        [this](const auto& typedRequest)
        {
            using RequestT = std::decay_t<decltype(typedRequest)>;

            if constexpr (
                std::is_same_v<RequestT, game::network::GalaxyMapRequest>)
            {
                game::network::GalaxyMapResponse response;
                response.requestId = typedRequest.requestId;
                response.metadata = m_server->protocolMetadata();
                response.snapshot = m_server->buildGalaxyMapSnapshot();
                m_mapResponses.push(std::move(response));
            }
            else if constexpr (
                std::is_same_v<RequestT, game::network::SystemMapRequest>)
            {
                game::network::SystemMapResponse response;
                response.requestId = typedRequest.requestId;
                response.metadata = m_server->protocolMetadata();
                response.systemId = typedRequest.systemId;
                response.snapshot =
                    m_server->buildSystemMapSnapshot(typedRequest.systemId);
                m_mapResponses.push(std::move(response));
            }
            else if constexpr (
                std::is_same_v<RequestT, game::network::DetailMapRequest>)
            {
                game::network::DetailMapResponse response;
                response.requestId = typedRequest.requestId;
                response.metadata = m_server->protocolMetadata();
                response.target = typedRequest.target;
                response.snapshot = m_server->buildDetailMapSnapshot(
                    typedRequest.target);
                m_mapResponses.push(std::move(response));
            }
            else if constexpr (
                std::is_same_v<RequestT, game::network::HubMapRequest>)
            {
                game::network::HubMapResponse response;
                response.requestId = typedRequest.requestId;
                response.metadata = m_server->protocolMetadata();
                response.systemId = typedRequest.systemId;
                response.hubId = typedRequest.hubId;
                response.snapshot = m_server->buildHubMapSnapshot(
                    typedRequest.systemId,
                    typedRequest.hubId);
                m_mapResponses.push(std::move(response));
            }
        },
        request
    );
}


bool LocalLoopbackTransport::receiveMapResponse(
    game::network::MapResponse& outResponse)
{
    if (m_mapResponses.empty())
        return false;

    outResponse = std::move(m_mapResponses.front());
    m_mapResponses.pop();
    return true;
}

void LocalLoopbackTransport::requestStarAtlas()
{
    game::network::StarAtlasResponse response;
    response.metadata.catalogRevision = m_server->catalogRevision();
    response.atlas = m_server->starAtlas();
    m_starAtlasResponses.push(std::move(response));
}

bool LocalLoopbackTransport::receiveStarAtlas(
    game::network::StarAtlasResponse& outResponse)
{
    if (m_starAtlasResponses.empty())
        return false;

    outResponse = std::move(m_starAtlasResponses.front());
    m_starAtlasResponses.pop();
    return true;
}

void LocalLoopbackTransport::requestCelestialSnapshot()
{
    game::network::CelestialSnapshotResponse response;
    response.metadata = m_server->protocolMetadata();
    response.snapshot = m_server->celestialSnapshot();
    m_celestialResponses.push(std::move(response));
}

bool LocalLoopbackTransport::receiveCelestialSnapshot(
    game::network::CelestialSnapshotResponse& outResponse)
{
    if (m_celestialResponses.empty())
        return false;

    outResponse = std::move(m_celestialResponses.front());
    m_celestialResponses.pop();
    return true;
}
