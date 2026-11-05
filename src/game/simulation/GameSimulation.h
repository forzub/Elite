#pragma once

#include "src/game/navigation/landing/LandingAutopilot.h"
#include <functional>
#include "src/game/simulation/ShipRefitSnapshot.h"
#include "src/game/navigation/orbital/OrbitalTransferPlanner.h"
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
#include "src/game/simulation/ShipSimulationTier.h"
#include "src/game/simulation/ShipSimulationDetailPolicy.h"

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/OrbitalCorridorSystem.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/autopilot/AutopilotLawController.h"
#include "src/game/navigation/NavigationRuntimePlanner.h"
#include "src/game/navigation/AcceptedShortSegment.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationExecutionReplanPolicy.h"
#include "src/game/navigation/NavigationWorkScheduler.h"
#include "src/game/diagnostics/NavigationRuntimeLab.h"
#include "src/world/navigation/map/NavigationMap.h"
#include "src/world/navigation/space/NavigationSpace.h"

#include <cstdio>
#include "src/game/traffic/DockServiceTraffic.h"
#include "src/game/combat/Penetration.h"
#include "src/game/simulation/ShipCombatSnapshot.h"


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

    // Simulation-detail transitions are independent from actor identity.
    EntityId spawnTrafficShell(
        int systemId,
        const ShipDescriptor& descriptor,
        const glm::dvec3& positionMeters,
        const glm::dvec3& velocityMetersPerSecond,
        const ShipInitData& initData,
        const glm::mat4& orientation = glm::mat4(1.0f),
        std::uint32_t formationId = 0,
        std::uint16_t formationSlot = 0
    );

    bool materializeShipShell(EntityId id);
    bool markShipDerelict(EntityId id);
    bool dematerializeShip(
        EntityId id,
        game::simulation::ShipSimulationTier targetTier
    );

    game::simulation::ShipSimulationTier shipSimulationTier(
        EntityId id
    ) const noexcept;

    game::simulation::ShipMotionFidelity shipMotionFidelity(
        EntityId id
    ) const noexcept;

    game::simulation::ShipMaterializedFidelity shipMaterializedFidelity(
        EntityId id
    ) const noexcept;

    bool shipUsesDetailedSimulation(EntityId id) const noexcept;
    bool promoteShipToDetailedSimulation(EntityId id);

    bool submitClientObservationInterest(
        EntityId observerId,
        std::uint64_t serial,
        const std::vector<
            game::simulation::ShipClientObservationInterestEntry>& entries
    );

    const game::simulation::ShipShellState* shipShell(EntityId id) const;
    // Earth Orbital Hub dock traffic model (nullptr until the dock exists).
    game::traffic::dock::DockServiceTraffic* dockServiceTraffic() noexcept;
    // Pilot asks the dock dispatcher to leave the pad (docked ships only).
    bool requestDockDeparture(EntityId shipId);

    // Orbital Transfer (GameSimulationOrbitalTransfer.cpp): hub -> hub travel
    // as an authoritative navigation mode; the tactical envelope is untouched.
    struct ActiveOrbitalTransfer
    {
        std::string sourceHubId;
        std::string targetHubId;
        std::string parentBodyId;
        game::navigation::orbital::CentralBody body;
        game::navigation::orbital::TransferLimits limits;
        game::navigation::orbital::OrbitalTransferPlan plan;
        game::navigation::orbital::TransferCarrier carrier;
        game::navigation::orbital::InterplanetaryEstimate estimate;
        bool abstracted = false;      // interplanetary: time-compressed transit, not flight
        bool thrustEnabled = true;
        double engineMaxAccelMps2 = 0.0;
        double lastOrbitalTimeSeconds = 0.0;
        double transitEndRealSeconds = 0.0;
        double peakSpeedMps = 0.0;
        double lastLogOrbitalSeconds = -1.0e30;
    };
    bool requestOrbitalTransfer(EntityId shipId, const std::string& targetHubId);
    void cancelOrbitalTransfer(EntityId shipId);

    // Ctrl+J free flight (GameSimulationFreeFlight.cpp): on - the travel frame is detached and
    // carries the ship's global motion under gravity; off - the ship is re-based into the
    // nearest hub frame with its actual relative position and velocity.
    // One leg of a route flown on the J drive (GameSimulationRoute.cpp).
    struct FreeFlightLeg
    {
        enum class Kind { Hub, Orbit, Point };
        Kind kind = Kind::Hub;
        std::string hubId;              // Hub
        std::string bodyId;             // Orbit
        double orbitAltitudeM = 0.0;    // Orbit
        glm::dvec3 pointWorld {0.0};    // Point
        bool passThrough = false;       // a guide point of a route: fly through, no stop
        double remainingAfterM = 0.0;   // route length after this leg (braking for pass-through)
        std::string action;             // at a stop: "refuel"
    };
    // A planned route: legs in order (route planner -> RouteLeg command)
    struct ActiveRoute
    {
        std::vector<FreeFlightLeg> legs;
        std::size_t index = 0;
        bool legDone = false;
        bool waitingExit = false;
        double serviceSeconds = -1.0;   // a stop action running (refuel)
        double idleSeconds = 0.0;
    };
    struct ActiveFreeFlight
    {
        std::string sourceHubId;
        double peakSpeedMps = 0.0;
        double lastLogSeconds = -1.0e30;
        double elapsedSeconds = 0.0;
        std::string primaryBodyId;
        double primaryAltitudeMeters = 0.0;
        // the dominant body's centre one tick ago (its velocity by difference)
        std::string lastPrimaryId;
        glm::dvec3 lastPrimaryCenterMeters {0.0};
        bool insideDenseLayers = false;
        bool devDropDone = false;
        double heatingDamageSeconds = 0.0;
        // J off away from a hub: the ship flies in its own ballistic frame (global
        // motion under gravity) with ordinary local flight from rest inside it
        bool localFrame = false;
        // J autopilot to a body (empty = off)
        std::string autopilotTargetId;
        std::string autopilotDepartureId;
        int autopilotPhase = 0;
        double autopilotSeconds = 0.0;
        double autopilotRampSeconds = 0.0;   // the drive ramp: counts only outside the J keep-out
        double orbitCoastSeconds = 0.0;      // landing: the coast on the parking orbit before the de-orbit
        glm::dvec3 orbitNormal {0.0};        // landing: the parking orbit plane, fixed once (zero = not yet)
        bool orbitAcquire = false;           // landing: close to the orbit - aim at its nearest point
        bool bellyLanding = false;           // landing: the ventral thrusters hold the weight - belly down
        double autopilotLastLogSeconds = -1.0e30;
        bool autopilotHaveLastCenter = false;
        // the landing module (planner + transit + descent) once the approach is over
        game::navigation::landing::LandingRequest landingRequest;
        game::navigation::landing::LandingAutopilot landing;
        bool landingStarted = false;
        // a route leg (hub / orbit / point) flown by the route autopilot
        FreeFlightLeg leg;
        bool legActive = false;
        bool legArrived = false;
        double legSeconds = 0.0;
        double legLastLog = -1.0e30;
        double legDriveStartSeconds = -1.0;
        std::string legDepartureHubId;
        bool legHaveLastBody = false;
        glm::dvec3 legLastBodyCentre {0.0};
        double landingPlanSeconds = 0.0;
        // landed: the touchdown spot, body-fixed (it turns with the body)
        bool landedBodyKnown = false;
        glm::dvec3 landedBodyDirection {0.0};
        double landedGroundM = 0.0;
        // the reference body's basis one tick ago (surface velocity = its turn)
        bool haveLastBasis = false;
        glm::dmat3 lastBasis {1.0};
        std::string lastBasisBodyId;
        glm::dvec3 autopilotLastTargetCenter {0.0};
        // J off near a body (below its J floor): NORMAL flight against the body's surface. The
        // travel frame moves with the ground under the ship, so the local velocity the flight
        // laws see is the velocity over the ground; every tick the server folds what the laws
        // did into the ship's world motion, applies real gravity, and splits it again.
        bool bodyFrame = false;
        std::string bodyFrameId;
        bool bodyHavePrevious = false;
        glm::dvec3 bodyPreviousCentre {0.0};
        bool bodyPreviousBasisValid = false;
        glm::dmat3 bodyPreviousBasis {1.0};
        bool bodyLiftOff = false;            // the laws pushed the ship off the ground this tick
        glm::dvec3 bodyTraceLocalStart {0.0};  // DEV trace: the local velocity handed to the laws
        // the landing virtual pilot of normal flight: ordinary Newtonian controls, applied
        // before the flight step (they override the pilot's input while it flies)
        bool pilotActive = false;
        bool pilotRestoreLaw = false;
        ShipControlState pilotControl {};
        game::navigation::LocalFlightControlLaw lawBeforeLanding = game::navigation::LocalFlightControlLaw::Assisted;
        double pilotLastLogSeconds = -1.0e30;
        // the J engine (motion.jEngine): its own ramp - 1 g doubling every 2 s while it thrusts
        double jRampSeconds = 0.0;
        // automatic stabilization (author 2026-11-05 cc): armed by the J engine going off, by a
        // new sphere of influence or by the orbit closing; fires after a coast on a safe closed orbit
        bool autoStabilizeArmed = false;
        bool lastBound = true;
        double coastSeconds = 0.0;
    };
    // J off below a body's J floor: normal flight against its surface (see bodyFrame)
    void enterBodyFrame(EntityId shipId, Ship& ship, ActiveFreeFlight& f, const game::navigation::GravityBody& body,
                        const glm::dvec3& bodyVelocityMps);
    void updateSurfaceLanding(EntityId shipId, Ship& ship, ActiveFreeFlight& f, double dt);
    void applySurfaceLandingPilots();
    bool toggleFreeFlight(EntityId shipId);
    // Flight architecture (author 2026-11-05 cc). Stabilization on / off: off = NORMAL flight
    // (no speed envelope, the velocity against space or the body of the sphere of influence);
    // on = the frame of a hub (capture), of the ground (low and slow) or of the ship's own
    // ballistic trajectory (orbit), the 500 m/s manoeuvre envelope. The J engine: only in normal
    // flight, only far from objects and above the J floor (else refused).
    bool toggleStabilization(EntityId shipId);
    bool toggleJEngine(EntityId shipId);
    // 0 = J may engage here; else the refusal code (7 an object too close, 8 below the J floor)
    int jEngineBlocked(EntityId shipId, int systemId, const glm::dvec3& shipWorld, std::string& why) const;
    // J terrain contact: ground (terrain or sea surface) height above the body
    // radius in a world direction from the body centre; false = unknown (the
    // field is still loading) -> the reference sphere is used.
    struct TerrainSample
    {
        double heightM = 0.0;     // terrain or sea surface above the body radius
        int material = -1;        // world::celestial::surface::SurfaceMaterial, TerrainWater = sea / lake
        double slopeDeg = 0.0;
        int feature = 0;          // world::celestial::surface::FeatureClass (crater, mountain range, ...)
    };
    static constexpr int TerrainWater = 100;
    using TerrainHeightProvider =
        std::function<bool(const std::string& bodyId, const glm::dvec3& worldDirection, TerrainSample& ground)>;
    void setTerrainHeightProvider(TerrainHeightProvider provider) { m_terrainHeight = std::move(provider); }
    // body-fixed basis of a body in the world (columns prime, north, east - the
    // renderer's / terrain's orientation): landing sites turn with the body
    using BodyOrientationProvider = std::function<bool(const std::string& bodyId, glm::dmat3& worldFromBody)>;
    void setBodyOrientationProvider(BodyOrientationProvider provider) { m_bodyOrientation = std::move(provider); }
    void setFreeFlightThrust(EntityId shipId, int command);
    // J autopilot: fly to a body and land on it (empty id = the dominant body); engage false cancels
    void setFreeFlightAutopilot(EntityId shipId, const std::string& bodyId, bool engage, const std::string& landingSpec = {});
    // Route autopilot: fly one leg (enters J if needed); finishes matched and leaves J.
    bool startFreeFlightLeg(EntityId shipId, const FreeFlightLeg& leg);
    // A whole route (legs in order): flies them, stops / actions included.
    bool startRoute(EntityId shipId, std::vector<FreeFlightLeg> legs);
    glm::dvec3 routePointWorld(const FreeFlightLeg& leg) const;
    void cancelRoute(EntityId shipId);
    const ActiveRoute* activeRoute(EntityId shipId) const
    {
        const auto it = m_routes.find(shipId);
        return it == m_routes.end() ? nullptr : &it->second;
    }

    // ---- combat (GameSimulationCombat.cpp): lasers, missiles, hits on hit volumes
    struct CombatLoadout
    {
        std::string laserId, missileId;
        int missiles = 0;
    };
    void setLaserFiring(EntityId shipId, bool firing);
    // A missile at the ship nearest the boresight inside the seeker cone (none: unguided).
    bool launchMissile(EntityId shipId);
    // DEV: next laser / missile type of the catalog
    void cycleCombatWeapon(EntityId shipId, bool missile);
    // DEV: a Cobra Mk I target `distanceM` ahead of the ship, broadside, in its hub frame
    EntityId spawnCombatTarget(EntityId nearShipId, double distanceM);
    CombatLoadout& combatLoadout(EntityId shipId);
    game::simulation::ShipCombatSnapshot combatSnapshotFor(EntityId shipId) const;

    // Repair & Refit (F7): a preview / DEV apply of one module operation on
    // a ship owned by the commanding ship's owner (subject 0 = that ship).
    // Results are published in the subject's ShipSnapshot::refit.
    void requestRefitPreview(EntityId commandingShip, EntityId subject, const game::ship::refit::RefitOperation& operation);
    void applyRefit(EntityId commandingShip, EntityId subject, const game::ship::refit::RefitOperation& operation);
    [[nodiscard]] game::simulation::ShipRefitSnapshot refitSnapshotFor(EntityId ship) const;
    const ActiveFreeFlight* freeFlight(EntityId shipId) const;
    const ActiveOrbitalTransfer* orbitalTransfer(EntityId shipId) const;
    // This ship's dispatcher status (phase None when unknown to the dock).
    game::docking::traffic::DockTrafficShipStatus dockDispatcherStatus(EntityId shipId) const;
    std::size_t shipShellCount() const noexcept { return m_shipShells.size(); }
    const std::unordered_map<EntityId, game::simulation::ShipShellState>& shipShells() const noexcept { return m_shipShells; }

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
        bool inheritHubOrientation = true,
        const glm::dvec3& localAngularVelocityDegPerSecond = glm::dvec3(0.0)
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
    SimulationSnapshot buildReplicationSnapshot(std::uint64_t serverTick);
    void debugForceFullShipGraphPayload();
    EntityId playerId() const { return m_playerId; }

    // Multiplayer ownership is independent from the legacy primary-player
    // compatibility alias. Every entity controlled by a connected server
    // session is pinned Active and excluded from NPC authority.
    bool setPlayerControlled(EntityId id, bool controlled);
    bool isPlayerControlled(EntityId id) const noexcept;
    const std::unordered_set<EntityId>& playerControlledShipIds() const noexcept
    {
        return m_playerControlledShipIds;
    }

    const std::unordered_map<
        EntityId,
        game::navigation::NavigationRuntimeControlBridge::ExecutionSnapshot
    >& npcNavigationExecutionSnapshots() const noexcept
    {
        return m_npcNavigationExecutionSnapshots;
    }
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
    bool setStaticObjectIdentity(
        EntityId id,
        const std::string& name,
        const std::string& owner
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
        double gravitationalParameterM3s2,
        double atmosphereOuterRadiusMeters
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
    void updateOrbitalTransfers();
    void updateFreeFlights(double dt);
    void handOverToHub(EntityId shipId, Ship& ship, const std::string& hubId, const glm::dvec3* worldOverride, const glm::dvec3* velOverride);

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

    void registerInterplanetaryTransferLabShip(EntityId shipId);
    bool isInterplanetaryTransferLabShip(EntityId shipId) const noexcept;

    void registerActivationCadenceLabShip(EntityId shipId);
    bool isActivationCadenceLabShip(EntityId shipId) const noexcept;

    void registerNavigationRuntimeLabShip(
        EntityId shipId,
        const std::string& hubId
    );
    bool isNavigationRuntimeLabShip(EntityId shipId) const noexcept;
    const game::navigation::NavigationRuntimePlanner::Result*
    navigationRuntimeLabLastPlan() const noexcept;
    const game::diagnostics::NavigationRuntimeLabObservation&
    navigationRuntimeLabObservation() const noexcept
    {
        return m_navigationRuntimeLabObservation;
    }

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
    void updateInterplanetaryTransferLabActor();
    void updateActivationCadenceLabClaim(double serverTimeSeconds);
    void initializeNavigationRuntimeLab();
    void publishNavigationRuntimeLabStaticGeometry();
    bool buildNavigationRuntimeLabIntent(
        EntityId id,
        Ship& ship,
        game::navigation::NavigationRuntimeControlBridge::Intent& outIntent
    );
    void updateActivationShadow();
    bool updateNpcNavigationControl(
        EntityId id,
        Ship& ship,
        const NpcNavigationGoal& goal,
        double executionTimeSeconds,
        double executionDeltaSeconds
    );

    void initializeShipStateFlightTest();
    void updateShipStateFlightTest();
    // DEV: ELITE_CATALOG_SHIPS=all | <passport id>[,<passport id>...] spawns
    // catalog ships (real ShipCore from their design passports, primitive
    // placeholder geometry) in a row ahead of the player.
    void updateCatalogShipLineup();
    bool isShipStateFlightTestShip(EntityId id) const noexcept;
    void applyPhysicalCorridorHotStart();

    void initializeFiveHundredShipHerd();
    void initializeShipTierTransitionTest();
    void updateShipShells(double dt);
    // Earth Orbital Hub dock traffic (GameSimulationDockTraffic.cpp).
    void updateDockServiceTraffic(double dt);
    // Pilot ships and the dock dispatcher: request on approach, station
    // capture at the portal, kinematic pose while station-controlled.
    void updateDockPilotShips();
    bool isDockStationControlledShip(EntityId id) const noexcept;
    bool removeTrafficShell(EntityId id);
    void updateShipSimulationDetail(double dt);
    void updateShipTierTransitionTest();
    void updateFiveHundredShipHerdDiagnostics();
    bool buildFiveHundredShipHerdGoal(
        EntityId id,
        NpcNavigationGoal& outGoal
    ) const;
    game::simulation::SimulationMode activationExecutionMode(
        EntityId shipId
    ) const noexcept;
    game::simulation::SimulationMode motionExecutionMode(
        EntityId shipId
    ) const noexcept;
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

    // Lightweight authoritative ships live outside m_ships by design: merely
    // existing in the universe must not allocate ShipCore/module/damage state.
    std::unordered_map<EntityId, game::simulation::ShipShellState> m_shipShells;

    // Dock service traffic: dispatcher + shuttles + busy traffic, realised as
    // TrafficShell ships whose pose the traffic model owns.
    std::unique_ptr<game::traffic::dock::DockServiceTraffic> m_dockServiceTraffic;
    std::unordered_map<std::uint64_t, EntityId> m_dockTrafficShellByActor;
    std::unordered_set<EntityId> m_dockTrafficShells;
    double m_dockTrafficPhaseOffsetRad = 0.0;
    double m_dockTrafficStatusSeconds = -1.0e30;
    double m_dockTrafficRouteSeconds = -1.0e30;
    double m_dockTrafficZoneRadiusMeters = 0.0;
    bool m_dockObserverPlaced = false;
    // Replicated traffic identity + remaining route per traffic ship.
    std::unordered_map<EntityId, game::simulation::ShipTrafficSnapshot> m_dockTrafficInfo;
    std::FILE* m_dockTrafficLog = nullptr;
    bool m_dockTrafficDisabled = false;
    std::unordered_set<EntityId> m_dockStationControlledShips;
    double m_dockPilotTraceSeconds = -1e9;
    std::unordered_map<
        EntityId,
        game::simulation::ShipSimulationLifecycleState
    > m_shipSimulationLifecycle;

    game::simulation::ShipMotionFidelityPolicy
        m_shipMotionFidelityPolicy {};
    std::unordered_map<
        EntityId,
        game::simulation::ShipMotionFidelityRuntimeState
    > m_shipMotionFidelityStates;
    std::unordered_map<
        EntityId,
        game::simulation::ShipClientObservationInterestState
    > m_shipClientObservationInterests;
    double m_shipMotionFidelityEvaluationAccumulatorSeconds = 0.0;
    std::unordered_map<EntityId, StaticObject> m_staticObjects;
    std::unordered_map<std::string, world::hubs::OrbitalHubRuntime> m_orbitalHubs;
    std::unordered_map<EntityId, ActiveOrbitalTransfer> m_orbitalTransfers;
    std::unordered_map<EntityId, ActiveFreeFlight> m_freeFlights;
    TerrainHeightProvider m_terrainHeight;
    BodyOrientationProvider m_bodyOrientation;
    void updateFreeFlightAutopilot(EntityId shipId, Ship& ship, ActiveFreeFlight& f, double dt);
    void updateFreeFlightLeg(EntityId shipId, Ship& ship, ActiveFreeFlight& f, double dt);
    // J keep-out (JDrive.h): the nearest hub / station / beacon or other ship whose keep-out holds a world point
    struct JKeepOutHit
    {
        std::string objectId;
        glm::dvec3 positionMeters {0.0};
        glm::dvec3 velocityMps {0.0};
        double distanceMeters = 0.0;
        bool ship = false;
    };
    bool jKeepOut(EntityId shipId, int systemId, const glm::dvec3& worldMeters, JKeepOutHit& hit) const;
    std::vector<EntityId> m_pendingStabilize;   // automatic stabilization after the free-flight pass
    std::vector<EntityId> m_pendingJExit;   // legs that arrived: J off after the free-flight pass
    std::unordered_map<EntityId, ActiveRoute> m_routes;
    void updateRoutes(double dt);

    // combat state
    struct CombatMissile
    {
        std::uint32_t id = 0;
        EntityId shooter, target;
        std::string specId;
        glm::dvec3 positionW {0.0}, velocityW {0.0};
        glm::dvec3 heading {0.0, 0.0, -1.0};
        double ageS = 0.0;
        double closestM = 1.0e18;   // closest approach to the target so far
        bool passReported = false;
    };
    struct CombatTrack
    {
        glm::dvec3 positionW {0.0}, velocityW {0.0};
        bool valid = false;
    };
    void updateCombat(double dt);
    void flushCombatDamage(EntityId targetId, game::damage::DamageType type, const world::coordinates::WorldPosition& where);
    void recordCombatImpact(EntityId targetId, const world::coordinates::WorldPosition& where, double energyMJ,
                            std::uint8_t kind, const std::string& moduleId);
    std::unordered_map<EntityId, CombatLoadout> m_combatLoadouts;
    std::unordered_set<EntityId> m_laserFiring;
    // DEV targets: passive hulls, no NPC pilot
    std::unordered_set<EntityId> m_combatTargetDummies;
    std::unordered_map<EntityId, game::combat::BurnState> m_burnStates;
    std::unordered_map<EntityId, double> m_burnFlushSeconds;
    std::unordered_map<EntityId, std::vector<game::simulation::CombatBeamSnapshot>> m_combatBeams;
    std::unordered_map<EntityId, std::vector<game::simulation::CombatImpactSnapshot>> m_combatImpacts;
    std::unordered_map<EntityId, CombatTrack> m_combatTracks;
    std::vector<CombatMissile> m_combatMissiles;
    std::uint32_t m_nextCombatSerial = 1;
    double m_combatClockS = 0.0;

    // Compatibility alias used by the current single-system navigation and
    // local-client presentation paths. Multiplayer authority lives in
    // m_playerControlledShipIds; this field must not be used to decide whether
    // a ship is player-controlled.
    EntityId                            m_playerId;
    std::unordered_set<EntityId>        m_playerControlledShipIds;
    WorldParams                         m_world;

    std::vector<WorldSignal>            m_worldSignals;
    std::vector<Planet>                 m_planets;
    std::vector<InterferenceSource>     m_interferenceSources;
    ShipControlState                    m_playerControlState;
    NpcAiSystem                         m_npcAiSystem;

    // State/physics proving actor. It changes construction and degradation
    // continuously without resetting kinematics.
    std::unordered_map<std::uint32_t, game::simulation::ShipRefitSnapshot> m_refitPublications;
    std::vector<std::string> m_catalogLineupRequest;
    bool m_catalogLineupSpawned = false;
    bool m_shipStateFlightTestRequested = false;
    EntityId m_shipStateFlightTestShipId {0};
    double m_shipStateFlightTestStartSeconds = 0.0;
    double m_shipStateFlightTestLastLogSeconds = -1.0;
    std::size_t m_shipStateFlightTestPhase = static_cast<std::size_t>(-1);
    std::uint64_t m_shipStateFlightControlTick = 0;

    bool m_physicalCorridorHotStartRequested = false;
    bool m_physicalCorridorHotStartApplied = false;

    struct FiveHundredShipHerdMember
    {
        std::size_t ordinal = 0;
        std::size_t flightIndex = 0;
        std::size_t slotInFlight = 0;
    };

    bool m_fiveHundredShipHerdRequested = false;
    bool m_fiveHundredMaterializedShipHerdRequested = false;
    bool m_fiveHundredFullMaterializedStressRequested = false;
    bool m_shipAutomaticDetailDisabled = false;

    bool m_shipTierTransitionTestRequested = false;
    EntityId m_shipTierTransitionProbeId {0};
    double m_shipTierTransitionStartSeconds = 0.0;
    int m_shipTierTransitionStage = 0;
    double m_fiveHundredShipHerdStartSeconds = 0.0;
    double m_fiveHundredShipHerdLastLogSeconds = -1.0;
    glm::dvec3 m_fiveHundredShipHerdCenterMeters {0.0};
    std::unordered_map<EntityId, FiveHundredShipHerdMember>
        m_fiveHundredShipHerd;

    std::unordered_map<
        EntityId,
        std::unique_ptr<game::navigation::NavigationRuntimeControlBridge>
    > m_npcNavigationControlBridges;

    std::unordered_map<
        EntityId,
        game::navigation::NavigationRuntimeControlBridge::ExecutionSnapshot
    > m_npcNavigationExecutionSnapshots;

    std::unordered_map<EntityId, double>
        m_npcNavigationLastExecutionTimeSeconds;

    std::unordered_map<
        EntityId,
        game::navigation::autopilot::AutopilotLawController
    > m_npcAutopilotLawControllers;

    // Snapshot graph payload control.
    // Heavy structural data is sent only on first sight / explicit dirty events.
    std::unordered_set<EntityId>        m_initializedShipGraphIds;
    std::unordered_map<EntityId, int>   m_shipGraphPayloadPublicationsRemaining;
    // Independently publish small module-state deltas when propulsion/lifetime
    // revision changes, without resending structural links and hit geometry.
    std::unordered_map<EntityId, std::uint64_t>
        m_lastPublishedShipModuleRevision;
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

    EntityId m_interplanetaryTransferLabShipId {0};
    // evaluateInterplanetaryTransferLab() consumes elapsed transfer time, not
    // the world's absolute universe epoch. Without this fence a late epoch
    // clamps the diagnostic ship straight onto Earth's orbital radius.
    double m_interplanetaryTransferLabStartUniverseTimeSeconds = 0.0;
    double m_interplanetaryTransferLabDeparturePhaseRad = 0.0;
    bool m_interplanetaryTransferLabDeparturePhaseInitialized = false;
    EntityId m_activationCadenceLabShipId {0};

    EntityId m_navigationRuntimeLabShipId {0};
    std::string m_navigationRuntimeLabHubId;
    bool m_navigationRuntimeLabInitialized = false;
    bool m_navigationRuntimeLabStaticGeometryPublished = false;
    bool m_navigationRuntimeLabHasPreviousExactSafetyPosition = false;
    glm::dvec3 m_navigationRuntimeLabPreviousExactSafetyPositionMap {0.0};
    std::uint64_t m_navigationRuntimeLabSourceRevision = 0;
    std::unique_ptr<world::navigation::NavigationMap>
        m_navigationRuntimeLabMap;
    std::unique_ptr<world::navigation::NavigationSpace>
        m_navigationRuntimeLabSpace;
    game::navigation::NavigationRuntimePlanner::Result
        m_navigationRuntimeLabLastPlan {};
    game::navigation::AcceptedShortSegment
        m_navigationRuntimeLabAcceptedSegment {};

    // B14 live integration remains lab-scoped until its target-machine gate is
    // accepted. Planner geometry/behavior stays unchanged; this service owns
    // only when a dirty planning request is allowed to run.
    game::navigation::NavigationWorkScheduler
        m_navigationRuntimeLabWorkScheduler {};
    std::uint64_t m_navigationRuntimeLabNextPlannerJobRevision = 1;
    std::uint64_t m_navigationRuntimeLabCapabilityRevision = 1;
    bool m_navigationRuntimeLabSchedulerCapabilityInitialized = false;
    game::navigation::AcceptedShortSegment::CapabilitySnapshot
        m_navigationRuntimeLabSchedulerCapability {};

    std::uint64_t m_navigationRuntimeLabNextSegmentRevision = 1;
    game::diagnostics::NavigationRuntimeLabObservation
        m_navigationRuntimeLabObservation {};

    // Stage 4A/4B: the stabilized activation plan controls real materialized
    // work lanes. Motion-control evaluation is decimated for Prewarm/Coarse
    // while cheap kinematic propagation remains fixed-step. GameSimulation still
    // publishes a full authoritative source set; Stage M7 performs per-session
    // sparse ship transport later in ServerRunner. Sensors
    // and signal visibility remain outside the activation domain.
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

    std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationCadenceState
    > m_shipMotionControlCadenceStates;

    // Per-fixed-tick cache: attitude/control-force evaluation and
    // HubTactical engine-command refresh must use the same accumulated dt.
    // The map is cleared each tick but retains capacity.
    std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationCadenceDecision
    > m_shipMotionControlStepDecisions;

    std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationCadenceState
    > m_shipSystemsCadenceStates;

    std::unordered_map<
        EntityId,
        game::simulation::activation::ActivationCadenceState
    > m_shipMaintenanceCadenceStates;

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
        // Catalog atmosphere outer radius; equals radiusMeters for airless bodies.
        double atmosphereOuterRadiusMeters = 0.0;
    };

    std::unordered_map<std::string, CelestialBodyGravityParameters>
        m_celestialBodyGravityParameters;

    double m_orbitalUniverseTimeSeconds = 0.0;
    std::unordered_map<std::string, game::navigation::HubNavigationFrame>
        m_hubNavigationFrames;


    std::vector<game::navigation::GravityBody> m_gravityBodies;
    std::vector<game::navigation::OrbitalCorridor> m_orbitalCorridors;


};
