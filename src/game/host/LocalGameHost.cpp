#include "src/game/host/LocalGameHost.h"

#include "src/game/network/ITransport.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/server/GameServer.h"

namespace game::host
{
LocalGameHost::LocalGameHost()
    : m_server(std::make_unique<GameServer>())
    , m_transport(std::make_unique<LocalLoopbackTransport>(m_server.get()))
{
    // Publish an initial authoritative snapshot before the client is created.
    m_server->update(0.0);

    m_runner = std::make_unique<server::ServerRunner>(
        *m_server,
        *m_transport
    );

    // Startup handshake is delivered immediately. Artificial latency applies
    // only after the client has consumed this initial authoritative state.
    m_transport->enqueueCurrentSnapshotImmediately();
}

LocalGameHost::~LocalGameHost() = default;

EntityId LocalGameHost::playerId() const
{
    return m_server->playerId();
}

ITransport& LocalGameHost::transport()
{
    return *m_transport;
}

const ITransport& LocalGameHost::transport() const
{
    return *m_transport;
}

server::ServerAdvanceResult LocalGameHost::advance(double elapsedSeconds)
{
    return m_runner->advance(elapsedSeconds);
}

double LocalGameHost::fixedStepSeconds() const
{
    return m_runner->fixedStepSeconds();
}

void LocalGameHost::configureWorld(float linearDrag, float maxSafeDecel)
{
    m_server->world().linearDrag = linearDrag;
    m_server->world().maxSafeDecel = maxSafeDecel;
}

const SimulationSnapshot& LocalGameHost::debugSnapshot() const
{
    return m_server->snapshot();
}

void LocalGameHost::debugRefreshSnapshot()
{
    m_server->debugRefreshSnapshot();
}

bool LocalGameHost::debugDestroyShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugDestroyShipModule(shipId, moduleId);
}

bool LocalGameHost::debugRestoreShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugRestoreShipModule(shipId, moduleId);
}

bool LocalGameHost::debugResetShipStructure(EntityId shipId)
{
    return m_server->debugResetShipStructure(shipId);
}

void LocalGameHost::debugResetAllShipStructures()
{
    m_server->debugResetAllShipStructures();
}

bool LocalGameHost::debugDetachShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugDetachShipModule(shipId, moduleId);
}

bool LocalGameHost::debugHangShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugHangShipModule(shipId, moduleId);
}

bool LocalGameHost::debugReevaluateShipStructure(EntityId shipId)
{
    return m_server->debugReevaluateShipStructure(shipId);
}

bool LocalGameHost::debugSetShipStructuralLinkHealth(
    EntityId shipId,
    const std::string& linkId,
    float health,
    bool destroyed)
{
    return m_server->debugSetShipStructuralLinkHealth(
        shipId, linkId, health, destroyed);
}

bool LocalGameHost::debugFastUniverseTime() const
{
    return m_server->debugFastUniverseTime();
}

void LocalGameHost::setDebugUniverseTimeSimulation(bool enabled, double timeScale)
{
    m_server->setDebugUniverseTimeSimulation(enabled, timeScale);
}

const world::celestial::StarAtlasDatabase& LocalGameHost::starAtlas() const
{
    return m_server->starAtlas();
}

const world::celestial::CelestialSystemSnapshot& LocalGameHost::celestialSnapshot() const
{
    return m_server->celestialSnapshot();
}
}
