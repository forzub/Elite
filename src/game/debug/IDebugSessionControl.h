#pragma once

#include <cstdint>
#include <string>

#include "src/game/simulation/SimulationSnapshot.h"
#include "src/scene/EntityID.h"

namespace game::debug
{
/*
    Application-side debug/control facade.

    Commands are requests to the authoritative runtime, not synchronous calls
    into GameServer. The snapshot is a copied debug value object; revision
    counters let tools wait for a requested authoritative refresh without
    retaining server-owned memory across a future thread/process boundary.
*/
class IDebugSessionControl
{
public:
    virtual ~IDebugSessionControl() = default;

    virtual SimulationSnapshot snapshot() const = 0;
    virtual std::uint64_t snapshotRevision() const = 0;
    virtual std::uint64_t stateRevision() const = 0;

    // Lightweight refresh waits for the next normal authoritative publication.
    // Structural debug explicitly opts into the heavier full module/link graph.
    virtual void refreshSnapshot() = 0;
    virtual void refreshStructureSnapshot() = 0;
    virtual void destroyShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual void restoreShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual void resetShipStructure(EntityId shipId) = 0;
    virtual void resetAllShipStructures() = 0;
    virtual void detachShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual void hangShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual void reevaluateShipStructure(EntityId shipId) = 0;
    virtual void setShipStructuralLinkHealth(
        EntityId shipId,
        const std::string& linkId,
        float health,
        bool destroyed
    ) = 0;

    virtual bool fastUniverseTime() const = 0;
    virtual bool universeTimeSimulation() const = 0;
    virtual double universeTimeScale() const = 0;
    virtual double configuredUniverseTimeScale() const = 0;
    virtual void setUniverseTimeSimulation(bool enabled, double timeScale) = 0;
};
}
