#pragma once

#include <vector>
#include <unordered_map>
#include <memory>

#include "game/ship/Ship.h"
#include "world/WorldParams.h"
#include "world/WorldSignal.h"
#include "world/Planet.h"
#include "world/InterferenceSource.h"
#include "world/WorldSignalTxSystem.h"
#include "game/simulation/NpcAiSystem.h"
#include "game/simulation/SimulationSnapshot.h"

#include "src/world/objects/StaticObject.h"
#include "src/world/types/ObjectType.h"


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




    
    EntityId spawnShip(
        ShipRole role,
        const ShipDescriptor& descriptor,
        glm::vec3 position,
        const ShipInitData& initData
    );

    EntityId spawnStation(
        ObjectType type,
        const glm::vec3& position
    );





    void setPlayerControl(const ShipControlState& control);
    void applyControl(EntityId id, const ShipControlState& control);
    const SimulationSnapshot& snapshot() const;
    void setTick(uint32_t tick);
    EntityId playerId() const { return m_playerId; }
    double serverTime() const { return m_serverTime; }
    std::unordered_map<EntityId, std::unique_ptr<Ship>>& ships();

private:
    uint32_t                            m_nextEntityId = 1;

    std::unordered_map<EntityId, std::unique_ptr<Ship>> m_ships;
    std::unordered_map<EntityId, StaticObject> m_staticObjects;

    EntityId                            m_playerId;
    WorldParams                         m_world;

    std::vector<WorldSignal>            m_worldSignals;
    std::vector<Planet>                 m_planets;
    std::vector<InterferenceSource>     m_interferenceSources;
    ShipControlState                    m_playerControlState;
    NpcAiSystem                         m_npcAiSystem;
    SimulationSnapshot                  m_snapshot;
    double                              m_serverTime = 0.0;
    
    EntityId generateEntityId();
};
