#include "src/game/host/LocalGameHost.h"

#include "src/game/network/ITransport.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/server/GameServer.h"

namespace game::host
{
LocalGameHost::LocalGameHost(const WorldParams& worldParams)
    : m_server(std::make_unique<GameServer>())
    , m_transport(std::make_unique<LocalLoopbackTransport>())
{
    // Session configuration is applied before the first authoritative tick.
    m_server->world() = worldParams;

    // Publish an initial authoritative snapshot before the client is created.
    m_server->update(0.0);

    m_runner = std::make_unique<server::ServerRunner>(
        *m_server,
        *m_transport
    );

    // Startup handshake is delivered immediately. Artificial latency applies
    // only after the client has consumed this initial authoritative state.
    m_transport->publishSnapshotImmediately(m_server->snapshot());
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

const SimulationSnapshot& LocalGameHost::snapshot() const
{
    return m_server->snapshot();
}

void LocalGameHost::refreshSnapshot()
{
    m_server->debugRefreshSnapshot();
}

bool LocalGameHost::destroyShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugDestroyShipModule(shipId, moduleId);
}

bool LocalGameHost::restoreShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugRestoreShipModule(shipId, moduleId);
}

bool LocalGameHost::resetShipStructure(EntityId shipId)
{
    return m_server->debugResetShipStructure(shipId);
}

void LocalGameHost::resetAllShipStructures()
{
    m_server->debugResetAllShipStructures();
}

bool LocalGameHost::detachShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugDetachShipModule(shipId, moduleId);
}

bool LocalGameHost::hangShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugHangShipModule(shipId, moduleId);
}

bool LocalGameHost::reevaluateShipStructure(EntityId shipId)
{
    return m_server->debugReevaluateShipStructure(shipId);
}

bool LocalGameHost::setShipStructuralLinkHealth(
    EntityId shipId,
    const std::string& linkId,
    float health,
    bool destroyed)
{
    return m_server->debugSetShipStructuralLinkHealth(
        shipId, linkId, health, destroyed);
}

bool LocalGameHost::fastUniverseTime() const
{
    return m_server->debugFastUniverseTime();
}

bool LocalGameHost::universeTimeSimulation() const
{
    return m_server->debugUniverseTimeSimulation();
}

double LocalGameHost::universeTimeScale() const
{
    return m_server->debugUniverseTimeScale();
}

double LocalGameHost::configuredUniverseTimeScale() const
{
    return m_server->debugUniverseTimeConfiguredScale();
}

void LocalGameHost::setUniverseTimeSimulation(bool enabled, double timeScale)
{
    m_server->setDebugUniverseTimeSimulation(enabled, timeScale);
}

}
