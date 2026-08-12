#pragma once

#include <cstdint>
#include <queue>
#include <utility>

#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/debug/IServerDebugChannel.h"

namespace game::debug
{
/*
    In-process debug link with separate client/tool and server endpoints.

    The application side only sees IDebugSessionControl. ServerRuntime only sees
    IServerDebugChannel. No method returns or stores a reference to authoritative
    server memory; snapshots crossing the seam are copied value objects.
*/
class LocalDebugSessionControl final
    : public IDebugSessionControl
    , public IServerDebugChannel
{
public:
    LocalDebugSessionControl() = default;

    // ----- application/debug-tool endpoint (IDebugSessionControl) -----
    SimulationSnapshot snapshot() const override;
    std::uint64_t snapshotRevision() const override;
    std::uint64_t stateRevision() const override;

    void refreshSnapshot() override;
    void refreshStructureSnapshot() override;
    void destroyShipModule(EntityId shipId, const std::string& moduleId) override;
    void restoreShipModule(EntityId shipId, const std::string& moduleId) override;
    void resetShipStructure(EntityId shipId) override;
    void resetAllShipStructures() override;
    void detachShipModule(EntityId shipId, const std::string& moduleId) override;
    void hangShipModule(EntityId shipId, const std::string& moduleId) override;
    void reevaluateShipStructure(EntityId shipId) override;
    void setShipStructuralLinkHealth(
        EntityId shipId,
        const std::string& linkId,
        float health,
        bool destroyed
    ) override;

    bool fastUniverseTime() const override;
    bool universeTimeSimulation() const override;
    double universeTimeScale() const override;
    double configuredUniverseTimeScale() const override;
    void setUniverseTimeSimulation(bool enabled, double timeScale) override;

    // ----- authoritative endpoint (IServerDebugChannel) -----
    bool receiveCommand(DebugCommand& outCommand) override;
    void publishSnapshot(const SimulationSnapshot& snapshot) override;
    void publishState(const DebugSessionState& state) override;

private:
    void enqueue(DebugCommand command);

    std::queue<DebugCommand> m_commands;
    SimulationSnapshot m_snapshot;
    DebugSessionState m_state;
    std::uint64_t m_snapshotRevision = 0;
    std::uint64_t m_stateRevision = 0;
};
}
