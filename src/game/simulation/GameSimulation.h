#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
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

#include "game/promo/PromoFlybyScenario.h"



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

    bool debugDestroyShipModule(EntityId id, const std::string& moduleId);
    bool debugRestoreShipModule(EntityId id, const std::string& moduleId);
    bool debugResetShipStructure(EntityId id);
    void debugResetAllShipStructures();

    bool debugDetachShipModule(EntityId id, const std::string& moduleId);
    bool debugReattachShipModule(EntityId id, const std::string& moduleId);
    bool startShipRepairJob(EntityId id, const std::string& moduleId);

    bool ejectShipCockpitCapsule(EntityId id);
    bool debugHangShipModule(EntityId id, const std::string& moduleId);
    bool debugReevaluateShipStructure(EntityId id);

    bool startBestRepairJobForMissingSlot(
        EntityId targetShipId,
        const std::string& targetModuleId
    );

    bool debugSetShipStructuralLinkHealth(
        EntityId id,
        const std::string& linkId,
        float health,
        bool destroyed
    );

    
    EntityId spawnShip(
        ShipRole role,
        const ShipDescriptor& descriptor,
        glm::vec3 position,
        const ShipInitData& initData,
        const glm::mat4& orientation = glm::mat4(1.0f)
    );

    EntityId spawnStation(
        ObjectType type,
        const glm::vec3& position,
        const glm::mat4& orientation = glm::mat4(1.0f)
    );


    bool startBestRepairJobForFirstMissingSlot(EntityId targetShipId);


    void setPlayerControl(const ShipControlState& control);
    void applyControl(EntityId id, const ShipControlState& control);
    const SimulationSnapshot& snapshot() const;
    void debugForceFullShipGraphPayload();
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

    // Snapshot graph payload control.
    // Heavy structural data is sent only on first sight / explicit dirty events.
    std::unordered_set<EntityId>        m_initializedShipGraphIds;
    std::unordered_map<EntityId, int>   m_shipGraphPayloadFramesRemaining;
    std::unordered_set<EntityId>        m_shipsWithDetachedFragmentPayload;
    std::unordered_set<EntityId>        m_shipsWithRepairJobPayload;

    double                              m_serverTime = 0.0;
    
    EntityId generateEntityId();
    void markShipGraphDirty(EntityId id);

    game::promo::PromoFlybyScenario m_promoFlybyScenario;
    void updatePromoPlayerTracking(float dt);

    glm::mat4 makePromoLookOrientation(
        const glm::vec3& forward,
        const glm::vec3& upHint
    ) const;

    glm::vec3 promoWingCenterAtTime(float time) const;
  
};
