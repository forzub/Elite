#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <glm/glm.hpp>

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
#include "src/world/orbits/OrbitalMotion.h"
#include "src/world/hubs/OrbitalHubRuntime.h"
#include "src/game/navigation/ReferenceFrame.h"
#include "src/game/navigation/HubNavigationFrame.h"

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/OrbitalCorridorSystem.h"


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
        const glm::dvec3& positionMeters,
        const ShipInitData& initData,
        const glm::mat4& orientation = glm::mat4(1.0f)
    );

    EntityId spawnStation(
        ObjectType type,
        const glm::dvec3& positionMeters,
        const glm::mat4& orientation = glm::mat4(1.0f)
    );

    bool setStaticObjectOrbitalMotion(
        EntityId id,
        const world::orbits::OrbitalMotion& motion
    );

    bool registerOrbitalHub(
        const world::hubs::OrbitalHubRuntime& hub
    );

    bool attachStaticObjectToHub(
        EntityId objectId,
        const std::string& hubId,
        const std::string& hubModuleId,
        const glm::dvec3& localOffsetMeters,
        const glm::dvec3& localRotationDeg = glm::dvec3(0.0),
        bool inheritHubOrientation = true
    );

    void updateStaticObjectOrbitParentParameters(
        const std::string& parentBodyId,
        double parentRadiusMeters,
        double parentGravitationalParameterM3s2,
        bool forceKeplerPeriod
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
    const std::unordered_map<EntityId, StaticObject>& staticObjects() const;
    bool setStaticObjectMapInfo(
        EntityId id,
        const std::string& name,
        const std::string& owner,
        int systemId = -1,
        const std::string& parentBodyId = {},
        const std::string& hubId = {},
        const std::string& hubModuleId = {}
    );

    void setCelestialBodyWorldPositionsAu(
        const std::unordered_map<std::string, glm::dvec3>& positionsAu
    );
    void setCelestialBodyKinematicStateAu(
        const std::unordered_map<std::string, glm::dvec3>& currentPositionsAu,
        const std::unordered_map<std::string, glm::dvec3>& previousPositionsAu,
        double sampleDtSeconds
    );
    void setOrbitalUniverseTimeSeconds(double t);

    void buildInitialScene();

    bool resolveCelestialBodyMeters(
        const std::string& bodyId,
        glm::dvec3& outCenterMeters,
        double& outRadiusMeters
    ) const;

    /*
        Возвращает абсолютную мировую скорость центра
        небесного тела из текущего серверного kinematic cache.
    */
    bool resolveCelestialBodyVelocityMetersPerSecond(
        const std::string& bodyId,
        glm::dvec3& outVelocityMetersPerSecond
    ) const;

    game::navigation::ResolvedFrameState resolveReferenceFrame(
        const game::navigation::ReferenceFrame& frame
    ) const;

    bool placeShipInReferenceFrame(
        EntityId shipId,
        const game::navigation::ReferenceFrame& frame
    );

    void updateShipReferenceFrames(double dt);

    const game::navigation::HubNavigationFrame* hubNavigationFrame(
        const std::string& hubId
    ) const;


    const std::unordered_map<std::string, world::hubs::OrbitalHubRuntime>&
    orbitalHubs() const
    {
        return m_orbitalHubs;
    }

    void rebuildHubNavigationFrames(double dt);
    void prepareReferenceFramesForSpawn();

private:
    EntityId generateEntityId();
    void markShipGraphDirty(EntityId id);

    game::promo::PromoFlybyScenario m_promoFlybyScenario;
    void updatePromoPlayerTracking(float dt);

    glm::mat4 makePromoLookOrientation(
        const glm::vec3& forward,
        const glm::vec3& upHint
    ) const;

    glm::vec3 promoWingCenterAtTime(float time) const;

    struct ShipReferenceBinding
    {
        game::navigation::ReferenceFrame frame;

        // true только для docked/attached-состояний.
        // Для свободного корабля рядом с хабом должно быть false.
        bool lockPositionToFrame = false;
    };


    void debugLogServerNavState(double dt);
    void debugLogPlayerMotion(double dt);
    void debugLogHubPlayerChain(double dt);

    void rebuildNavigationGravityContext();
    void updateDynamicNavigationContext(double dt);


private:
    uint32_t                            m_nextEntityId = 1;

    std::unordered_map<EntityId, std::unique_ptr<Ship>> m_ships;
    std::unordered_map<EntityId, StaticObject> m_staticObjects;
    std::unordered_map<std::string, world::hubs::OrbitalHubRuntime> m_orbitalHubs;

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
    
    
    std::unordered_map<EntityId, ShipReferenceBinding> m_shipReferenceBindings;

    std::unordered_map<std::string, glm::dvec3> m_previousHubPositionMeters;
    std::unordered_map<std::string, glm::dvec3> m_hubVelocityMetersPerSecond;

    
    
    std::unordered_map<std::string, glm::dvec3> m_celestialBodyPositionsAu;
    std::unordered_map<std::string, glm::dvec3> m_previousCelestialBodyPositionsMeters;
    std::unordered_map<std::string, glm::dvec3> m_celestialBodyVelocitiesMetersPerSecond;
    double m_orbitalUniverseTimeSeconds = 0.0;
    std::unordered_map<std::string, game::navigation::HubNavigationFrame>
        m_hubNavigationFrames;


    std::vector<game::navigation::GravityBody> m_gravityBodies;
    std::vector<game::navigation::OrbitalCorridor> m_orbitalCorridors;


};
