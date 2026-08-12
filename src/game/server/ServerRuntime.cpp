#include "src/game/server/ServerRuntime.h"

#include "src/game/network/IServerTransport.h"
#include "src/game/server/GameServer.h"

namespace game::server
{
ServerRuntime::ServerRuntime(
    const WorldParams& worldParams,
    IServerTransport& transport
)
    : m_server(std::make_unique<GameServer>())
{
    // Bootstrap configuration belongs to the authoritative runtime. The host
    // must not mutate GameServer/world state directly, because that access
    // cannot survive a later server-thread boundary.
    m_server->world() = worldParams;

    // Preserve the established startup semantics: apply configured world
    // parameters before the first host-visible authoritative publication.
    m_server->update(0.0);

    m_runner = std::make_unique<ServerRunner>(
        *m_server,
        transport
    );

    // Session authority is bootstrap metadata, not recurring replicated state.
    // The client learns which entity it controls from the server endpoint and
    // never selects that EntityId in its command packets.
    game::network::SessionWelcome welcome;
    welcome.controlledEntityId = m_server->playerId();
    transport.publishSessionWelcomeImmediately(welcome);

    // The first authoritative snapshot is bootstrap data, not a
    // latency-simulated gameplay packet.
    transport.publishSnapshotImmediately(m_server->snapshot());
}

ServerRuntime::~ServerRuntime() = default;

ServerAdvanceResult ServerRuntime::advance(double elapsedSeconds)
{
    return m_runner->advance(elapsedSeconds);
}

double ServerRuntime::fixedStepSeconds() const
{
    return m_runner->fixedStepSeconds();
}

const SimulationSnapshot& ServerRuntime::snapshot() const
{
    return m_server->snapshot();
}

void ServerRuntime::refreshSnapshot()
{
    m_server->debugRefreshSnapshot();
}

bool ServerRuntime::destroyShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugDestroyShipModule(shipId, moduleId);
}

bool ServerRuntime::restoreShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugRestoreShipModule(shipId, moduleId);
}

bool ServerRuntime::resetShipStructure(EntityId shipId)
{
    return m_server->debugResetShipStructure(shipId);
}

void ServerRuntime::resetAllShipStructures()
{
    m_server->debugResetAllShipStructures();
}

bool ServerRuntime::detachShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugDetachShipModule(shipId, moduleId);
}

bool ServerRuntime::hangShipModule(EntityId shipId, const std::string& moduleId)
{
    return m_server->debugHangShipModule(shipId, moduleId);
}

bool ServerRuntime::reevaluateShipStructure(EntityId shipId)
{
    return m_server->debugReevaluateShipStructure(shipId);
}

bool ServerRuntime::setShipStructuralLinkHealth(
    EntityId shipId,
    const std::string& linkId,
    float health,
    bool destroyed)
{
    return m_server->debugSetShipStructuralLinkHealth(
        shipId,
        linkId,
        health,
        destroyed
    );
}

bool ServerRuntime::fastUniverseTime() const
{
    return m_server->debugFastUniverseTime();
}

bool ServerRuntime::universeTimeSimulation() const
{
    return m_server->debugUniverseTimeSimulation();
}

double ServerRuntime::universeTimeScale() const
{
    return m_server->debugUniverseTimeScale();
}

double ServerRuntime::configuredUniverseTimeScale() const
{
    return m_server->debugUniverseTimeConfiguredScale();
}

void ServerRuntime::setUniverseTimeSimulation(bool enabled, double timeScale)
{
    m_server->setDebugUniverseTimeSimulation(enabled, timeScale);
}
}
