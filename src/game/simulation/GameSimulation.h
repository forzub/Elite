#pragma once
#include <cstdint>

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
#include "src/game/simulation/UniverseDiagnosticTrajectorySession.h"

#include "src/world/objects/StaticObject.h"
#include "src/world/types/ObjectType.h"

#include "game/promo/PromoFlybyScenario.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/world/hubs/OrbitalHubRuntime.h"
#include "src/game/navigation/ReferenceFrame.h"
#include "src/game/navigation/HubNavigationFrame.h"
#include "src/game/diagnostics/ServerDiagnostics.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/diagnostics/ActivationCadenceLab.h"
#include "src/game/server/ServerTimeContext.h"
#include "src/game/world_state/InitialWorldState.h"
#include "src/game/server/ServerTimelineClock.h"
#include "src/game/simulation/activation/ActivationPlanner.h"
#include "src/game/simulation/activation/ActivationExecutionPolicy.h"

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/OrbitalCorridorSystem.h"


class StateContext;

class GameSimulation
{
public:
    explicit GameSimulation(
        game::diagnostics::ServerDiagnostics& diagnostics
    );

    GameSimulation(const GameSimulation&) = delete;
    GameSimulation& operator=(const GameSimulation&) = delete;
    GameSimulation(GameSimulation&&) = delete;
    GameSimulation& operator=(GameSimulation&&) = delete;

    
    void update(const game::server::ServerTimeContext& time);
    WorldParams& world();
    const WorldParams& world() const;

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
        int systemId,
        const ShipDescriptor& descriptor,
        const glm::dvec3& positionMeters,
        const ShipInitData& initData,
        const glm::mat4& orientation = glm::mat4(1.0f)
    );

    EntityId spawnStation(
        ObjectType type,
        int systemId,
        const glm::dvec3& positionMeters,
        const glm::mat4& orientation = glm::mat4(1.0f)
    );

    bool setStaticObjectOrbitalMotion(
        EntityId id,
        const std::string& parentBodyId,
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
        int systemId,
        const std::string& parentBodyId,
        double parentRadiusMeters,
        double parentGravitationalParameterM3s2
    );


    bool startBestRepairJobForFirstMissingSlot(EntityId targetShipId);


    void setPlayerControl(const ShipControlState& control);
    void applyControl(EntityId id, const ShipControlState& control);
    const SimulationSnapshot& snapshot() const;
    void debugForceFullShipGraphPayload();
    void setTick(std::uint64_t tick);
    EntityId playerId() const { return m_playerId; }
    double serverTime() const { return m_serverTimelineClock.timeSeconds(); }
    std::unordered_map<EntityId, std::unique_ptr<Ship>>& ships();
    const std::unordered_map<EntityId, std::unique_ptr<Ship>>& ships() const;
    const std::unordered_map<EntityId, StaticObject>& staticObjects() const;
    bool setStaticObjectMapInfo(
        EntityId id,
        const std::string& name,
        const std::string& owner,
        const std::string& parentBodyId = {},
        const std::string& hubId = {},
        const std::string& hubModuleId = {}
    );

    void setCelestialBodyKinematicStateAu(
        int systemId,
        const std::unordered_map<std::string, glm::dvec3>& positionsAu,
        const std::unordered_map<std::string, glm::dvec3>& velocitiesAuPerSecond
    );
    void setCelestialBodyGravityParameters(
        int systemId,
        const std::string& bodyId,
        double radiusMeters,
        double gravitationalParameterM3s2
    );
    void setOrbitalUniverseTimeSeconds(double t);

    int activeCelestialSystemId() const noexcept
    {
        return m_activeCelestialSystemId;
    }

    void buildInitialScene(
        const game::world_state::InitialWorldState& initialState
    );

    bool resolveCelestialBodyMeters(
        int systemId,
        const std::string& bodyId,
        glm::dvec3& outCenterMeters,
        double& outRadiusMeters
    ) const;

    /*
        Возвращает абсолютную мировую скорость центра
        небесного тела из текущего серверного kinematic cache.
    */
    bool resolveCelestialBodyVelocityMetersPerSecond(
        int systemId,
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

    // Starts a transactional debug-only trajectory branch from one already-
    // published authoritative epoch. Production ship state is never moved onto
    // that branch; disabling the diagnostic simply discards it.
    bool beginUniverseTrajectoryDiagnostic(double startUniverseTimeSeconds);

    // Read-only presentation transform. During accelerated diagnostics this is
    // resolved from the diagnostic branch; otherwise it is the production
    // transform. Gameplay systems must continue to use Ship::transform().
    ShipTransform presentationShipTransform(EntityId shipId) const;

    bool universeDiagnosticTrajectoryActive() const noexcept
    {
        return m_universeDiagnosticTrajectories.active();
    }

    void updateShipReferenceFrames(double dt);

    const game::navigation::HubNavigationFrame* hubNavigationFrame(
        const std::string& hubId
    ) const;


    const std::unordered_map<std::string, world::hubs::OrbitalHubRuntime>&
    orbitalHubs() const
    {
        return m_orbitalHubs;
    }

    void rebuildHubNavigationFrames(double frameDeltaSeconds);
    void prepareReferenceFramesForSpawn();

    void registerHubMotionLabShip(
        EntityId shipId,
        game::diagnostics::HubMotionLabActorKind kind,
        const std::string& hubId
    );

    bool isHubMotionLabShip(EntityId shipId) const noexcept;

    game::diagnostics::HubMotionLabActorKind hubMotionLabActorKind(
        EntityId shipId
    ) const noexcept;

    void registerActivationCadenceLabShip(EntityId shipId);
    bool isActivationCadenceLabShip(EntityId shipId) const noexcept;

    const std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationPlannerDecision
    >& activationPlannerDecisions() const noexcept
    {
        return m_activationPlannerDecisions;
    }

    void upsertActivationClaim(
        const game::simulation::activation::ActivationClaim& claim
    );

    void clearActivationClaimsFromSource(EntityId sourceId);

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
    void debugLogActivationShadow(double dt);
    void debugLogGravitySample(const Ship& ship);

    void rebuildNavigationGravityContext();
    void updateDynamicNavigationContext(double dt);
    void updateHubMotionLabActors();
    void updateActivationCadenceLabClaim(double serverTimeSeconds);
    void updateActivationShadow();
    void endUniverseTrajectoryDiagnostic();
    void advanceUniverseTrajectoryDiagnostic(double universeDeltaSeconds);
    bool applyDiagnosticTrajectoryTransform(
        EntityId shipId,
        ShipTransform& transform
    ) const;


private:
    game::diagnostics::ServerDiagnostics& m_diagnostics;
    double m_npcRepairThinkTimerSeconds = 0.0;

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

    game::server::ServerTimelineClock  m_serverTimelineClock;
    game::simulation::UniverseDiagnosticTrajectorySession
        m_universeDiagnosticTrajectories;

    std::unordered_map<EntityId, ShipReferenceBinding> m_shipReferenceBindings;

    struct HubMotionLabRegistration
    {
        game::diagnostics::HubMotionLabActorKind kind =
            game::diagnostics::HubMotionLabActorKind::None;
        std::string hubId;
    };

    std::unordered_map<EntityId, HubMotionLabRegistration>
        m_hubMotionLabShips;

    EntityId m_activationCadenceLabShipId {0};

    // Stage 3E keeps the spatial/CPA planner and allows only NPC tactical AI
    // think cadence to consume plannedMode. Physics, HubTactical integration,
    // signals and snapshots remain full-rate until coarse/scheduled motion is
    // explicit. Sensors and signal visibility remain outside this domain.
    std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationPlannerDecision
    > m_activationPlannerDecisions;

    std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationPlanState
    > m_activationPlanStates;

    std::vector<game::simulation::activation::ActivationClaim>
        m_activationClaims;

    game::simulation::activation::InteractionHorizonPolicy
        m_activationInteractionPolicy {};

    game::simulation::activation::ActivationHysteresisPolicy
        m_activationHysteresisPolicy {};

    game::simulation::activation::ActivationExecutionPolicy
        m_activationExecutionPolicy {};

    std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationCadenceState
    > m_npcAiCadenceStates;

    double m_activationShadowEvaluationAccumulatorSeconds = 0.0;
    bool m_activationShadowEvaluated = false;
    double m_activationShadowCsvAccumulatorSeconds = 0.0;
    bool m_activationShadowCsvInitialized = false;

    std::unordered_map<std::string, glm::dvec3> m_hubVelocityMetersPerSecond;

    
    
    // The dynamic simulation currently advances one star-system context at a
    // time. The id is explicit so entities from another system can never be
    // evaluated against the active system's celestial cache by accident.
    int m_activeCelestialSystemId = -1;

    std::unordered_map<std::string, glm::dvec3> m_celestialBodyPositionsAu;
    std::unordered_map<std::string, glm::dvec3> m_celestialBodyVelocitiesMetersPerSecond;

    struct CelestialBodyGravityParameters
    {
        double radiusMeters = 0.0;
        double gravitationalParameterM3s2 = 0.0;
    };

    std::unordered_map<std::string, CelestialBodyGravityParameters>
        m_celestialBodyGravityParameters;

    double m_orbitalUniverseTimeSeconds = 0.0;
    std::unordered_map<std::string, game::navigation::HubNavigationFrame>
        m_hubNavigationFrames;


    std::vector<game::navigation::GravityBody> m_gravityBodies;
    std::vector<game::navigation::OrbitalCorridor> m_orbitalCorridors;


};
