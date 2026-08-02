#pragma once

#include <string>

#include "src/game/simulation/SimulationSnapshot.h"
#include "src/scene/EntityID.h"

namespace game::debug
{
class IDebugSessionControl
{
public:
    virtual ~IDebugSessionControl() = default;

    virtual const SimulationSnapshot& snapshot() const = 0;
    virtual void refreshSnapshot() = 0;
    virtual bool destroyShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual bool restoreShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual bool resetShipStructure(EntityId shipId) = 0;
    virtual void resetAllShipStructures() = 0;
    virtual bool detachShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual bool hangShipModule(EntityId shipId, const std::string& moduleId) = 0;
    virtual bool reevaluateShipStructure(EntityId shipId) = 0;
    virtual bool setShipStructuralLinkHealth(
        EntityId shipId,
        const std::string& linkId,
        float health,
        bool destroyed
    ) = 0;
    virtual bool fastUniverseTime() const = 0;
    virtual void setUniverseTimeSimulation(bool enabled, double timeScale) = 0;
};
}
