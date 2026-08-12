#include "src/game/debug/LocalDebugSessionControl.h"

#include <utility>

namespace game::debug
{
SimulationSnapshot LocalDebugSessionControl::snapshot() const
{
    // Return a value copy: application/debug code must never retain a reference
    // into memory that can later belong exclusively to a server worker thread.
    return m_snapshot;
}

std::uint64_t LocalDebugSessionControl::snapshotRevision() const
{
    return m_snapshotRevision;
}

std::uint64_t LocalDebugSessionControl::stateRevision() const
{
    return m_stateRevision;
}

void LocalDebugSessionControl::enqueue(DebugCommand command)
{
    m_commands.push(std::move(command));
}

void LocalDebugSessionControl::refreshSnapshot()
{
    DebugCommand command;
    command.type = DebugCommandType::RefreshSnapshot;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::refreshStructureSnapshot()
{
    DebugCommand command;
    command.type = DebugCommandType::RefreshStructureSnapshot;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::destroyShipModule(
    EntityId shipId,
    const std::string& moduleId)
{
    DebugCommand command;
    command.type = DebugCommandType::DestroyShipModule;
    command.shipId = shipId;
    command.itemId = moduleId;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::restoreShipModule(
    EntityId shipId,
    const std::string& moduleId)
{
    DebugCommand command;
    command.type = DebugCommandType::RestoreShipModule;
    command.shipId = shipId;
    command.itemId = moduleId;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::resetShipStructure(EntityId shipId)
{
    DebugCommand command;
    command.type = DebugCommandType::ResetShipStructure;
    command.shipId = shipId;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::resetAllShipStructures()
{
    DebugCommand command;
    command.type = DebugCommandType::ResetAllShipStructures;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::detachShipModule(
    EntityId shipId,
    const std::string& moduleId)
{
    DebugCommand command;
    command.type = DebugCommandType::DetachShipModule;
    command.shipId = shipId;
    command.itemId = moduleId;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::hangShipModule(
    EntityId shipId,
    const std::string& moduleId)
{
    DebugCommand command;
    command.type = DebugCommandType::HangShipModule;
    command.shipId = shipId;
    command.itemId = moduleId;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::reevaluateShipStructure(EntityId shipId)
{
    DebugCommand command;
    command.type = DebugCommandType::ReevaluateShipStructure;
    command.shipId = shipId;
    enqueue(std::move(command));
}

void LocalDebugSessionControl::setShipStructuralLinkHealth(
    EntityId shipId,
    const std::string& linkId,
    float health,
    bool destroyed)
{
    DebugCommand command;
    command.type = DebugCommandType::SetShipStructuralLinkHealth;
    command.shipId = shipId;
    command.itemId = linkId;
    command.health = health;
    command.destroyed = destroyed;
    enqueue(std::move(command));
}

bool LocalDebugSessionControl::fastUniverseTime() const
{
    return m_state.fastUniverseTime;
}

bool LocalDebugSessionControl::universeTimeSimulation() const
{
    return m_state.universeTimeSimulation;
}

double LocalDebugSessionControl::universeTimeScale() const
{
    return m_state.universeTimeScale;
}

double LocalDebugSessionControl::configuredUniverseTimeScale() const
{
    return m_state.configuredUniverseTimeScale;
}

void LocalDebugSessionControl::setUniverseTimeSimulation(
    bool enabled,
    double timeScale)
{
    DebugCommand command;
    command.type = DebugCommandType::SetUniverseTimeSimulation;
    command.enabled = enabled;
    command.timeScale = timeScale;
    enqueue(std::move(command));
}

bool LocalDebugSessionControl::receiveCommand(DebugCommand& outCommand)
{
    if (m_commands.empty())
        return false;

    outCommand = std::move(m_commands.front());
    m_commands.pop();
    return true;
}

void LocalDebugSessionControl::publishSnapshot(
    const SimulationSnapshot& snapshot)
{
    m_snapshot = snapshot;
    ++m_snapshotRevision;
}

void LocalDebugSessionControl::publishState(
    const DebugSessionState& state)
{
    m_state = state;
    ++m_stateRevision;
}
}
