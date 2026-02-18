#pragma once

#include <vector>
#include <unordered_map>

#include "game/ship/Ship.h"
#include "world/WorldParams.h"
#include "world/WorldSignal.h"
#include "world/Planet.h"
#include "world/InterferenceSource.h"
#include "game/equipment/signalNode/processing/WorldSignalTxSystem.h"
#include "game/simulation/NpcAiSystem.h"
#include "game/simulation/SimulationSnapshot.h"


class StateContext;

class GameSimulation
{
public:
    GameSimulation();

    
    void update(double dt);
    WorldParams& world();

    const std::vector<WorldSignal>& worldSignals() const;
    const std::vector<Planet>& planets() const;
    const std::vector<InterferenceSource>& interferenceSources() const;

    Ship* getShip(EntityId id);
    const Ship* getShip(EntityId id) const;

    Ship* playerShip();
    const Ship* playerShip() const;

    std::unordered_map<EntityId, Ship>& ships();
    EntityId spawnShip(
        ShipRole role,
        const ShipDescriptor& descriptor,
        glm::vec3 position,
        const ShipInitData& initData
    );

    void setPlayerControl(const ShipControlState& control);
    void applyControl(EntityId id, const ShipControlState& control);
    const SimulationSnapshot& snapshot() const;
    void setTick(uint32_t tick);
    EntityId playerId() const { return m_playerId; }
    

private:
    uint32_t                            m_nextEntityId = 1;

    std::unordered_map<EntityId, Ship>  m_ships;
    EntityId                            m_playerId;
    WorldParams                         m_world;

    std::vector<WorldSignal>            m_worldSignals;
    std::vector<Planet>                 m_planets;
    std::vector<InterferenceSource>     m_interferenceSources;
    ShipControlState                    m_playerControlState;
    NpcAiSystem                         m_npcAiSystem;
    SimulationSnapshot                  m_snapshot;
    
    
    EntityId generateEntityId();
};
