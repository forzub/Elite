#pragma once

#include "src/game/navigation/rules/CapsuleRules.h"
#include "src/game/navigation/rules/JumpSumps.h"
#include "src/game/fuel/RefuelRules.h"

#include "src/game/navigation/landing/LandingAutopilot.h"
#include <algorithm>
#include <functional>
#include "src/game/simulation/ShipRefitSnapshot.h"
#include <cstdint>

#include <vector>
#include <map>
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
#include "src/game/traffic/HubSecurity.h"
#include "src/game/simulation/ShipSimulationDetailPolicy.h"

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/globalflight/BodyAutopilot.h"
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
    // TEMPORARY (author 2026-11-21, Ctrl+Alt+R): structure + modules, reactors (containment emergencies, the dead-reactor
    // marks), heat, spin and the "out of the fight" state back to new
    bool debugFullRestoreShip(EntityId id);
    // a hub city pad (ClientShipCommand::LandOnPad; GameSimulationPadLanding.cpp): land = the pad service flies the ship
    // there and holds it; !land = the pilot takes the controls
    bool requestPadLanding(EntityId shipId, const std::string& frameModuleId, const std::string& address, bool land);
    // the ship into a hub's navigation frame where it is (normal flight off): the pad service, the docking request
    bool takeShipIntoHubFrame(EntityId shipId, const std::string& hubId);
    bool enterHubFrameForModule(EntityId shipId, const std::string& moduleId);   // ClientShipCommand::EnterHubFrame
    // G (author 2026-11-21): the landing legs out / in; a broken set stays broken (a repair restores it)
    void toggleLandingGear(EntityId shipId);
    // DEV: every lane tube of a hub against its static objects' collision boxes ([LaneCheck] lines)
    void devLaneClearanceReport(const std::string& hubId) const;
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
    // The pilot took the controls during the station's axial departure: released early.
    bool releaseDockDepartureToPilot(EntityId shipId);


    // Ctrl+J free flight (GameSimulationFreeFlight.cpp): on - the travel frame is detached and
    // carries the ship's global motion under gravity; off - the ship is re-based into the
    // nearest hub frame with its actual relative position and velocity.
    // One leg of a route flown on the J drive (GameSimulationRoute.cpp).
    // navigation zones (GameSimulationZones.cpp): red entries and skipped blue gates fine the ship
    void updateZoneViolations(double dt);
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
        // the plant's J ramp for a global-flight command: counts only outside the keep-out
        double globalRampSeconds = 0.0;
        double globalLogSeconds = 0.0;      // the plant's command log (every 2 s)
        double globalIdleSeconds = 0.0;     // no global command for this long: the ramp restarts after 0.5 s
        // the J field against the medium (author 2026-11-12: no fixed cap - the medium resists above ~0.1 c,
        // jdrive::mediumResistanceMps2): the field the charge pays for this tick, the medium's resistance of the
        // last tick, and the pilot field's own level (its ramp state); motion.jAccelMps2 is the NET acceleration
        double jChargeAccelMps2 = 0.0;
        double jMediumMps2 = 0.0;
        double jFieldMps2 = 0.0;
        // a route leg (hub / orbit / point) flown by the route autopilot
        // the sub-machines of a free flight (architecture audit item 3): one struct each
        struct GroundFrame     // the ground stabilization frame (landing / landed)
        {
            bool active = false;
            std::string id;
            bool havePrevious = false;
            glm::dvec3 previousCentre {0.0};
            bool previousBasisValid = false;
            glm::dmat3 previousBasis {1.0};
            bool liftOff = false;            // the laws pushed the ship off the ground this tick
            glm::dvec3 traceLocalStart {0.0};  // DEV trace: the local velocity handed to the laws
        } ground;
        // landed: the touchdown spot, body-fixed (it turns with the body)
        bool landedBodyKnown = false;
        glm::dvec3 landedBodyDirection {0.0};
        double landedGroundM = 0.0;
        // the reference body's basis one tick ago (surface velocity = its turn)
        bool haveLastBasis = false;
        glm::dmat3 lastBasis {1.0};
        std::string lastBasisBodyId;
        // J off near a body (below its J floor): NORMAL flight against the body's surface. The
        // travel frame moves with the ground under the ship, so the local velocity the flight
        // laws see is the velocity over the ground; every tick the server folds what the laws
        // did into the ship's world motion, applies real gravity, and splits it again.
        // the landing virtual pilot of normal flight: ordinary Newtonian controls, applied
        // before the flight step (they override the pilot's input while it flies)
        // the J engine (motion.jEngine): its own ramp - 1 g doubling every 2 s while it thrusts
        // automatic stabilization (author 2026-11-05 cc): armed by the J engine going off, by a
        // new sphere of influence or by the orbit closing; fires after a coast on a safe closed orbit
        bool autoStabilizeArmed = false;
        bool lastBound = true;
        double coastSeconds = 0.0;
    };
    // J off below a body's J floor: normal flight against its surface (see bodyFrame)
    void enterBodyFrame(EntityId shipId, Ship& ship, ActiveFreeFlight& f, const game::navigation::GravityBody& body,
                        const glm::dvec3& bodyVelocityMps);
    // the plant: executes a global-flight command (the client's, or the server-run body autopilot's)
    // within the rules - the J field toward a velocity target, the main engines along the nose, the
    // hull's slew; the frame requests switch normal flight <-> the ground stabilization
    void applyGlobalFlightCommand(EntityId shipId, Ship& ship, ActiveFreeFlight& f, const ShipControlState& command, double dt);
    void applyGlobalFrameRequests();
    bool toggleFreeFlight(EntityId shipId);
    // Flight architecture (author 2026-11-05 cc). Stabilization on / off: off = NORMAL flight
    // (no speed envelope, the velocity against space or the body of the sphere of influence);
    // on = the frame of a hub (capture), of the ground (low and slow) or of the ship's own
    // ballistic trajectory (orbit), the 500 m/s manoeuvre envelope. The J engine: only in normal
    // flight, only far from objects and above the J floor (else refused).
    bool toggleStabilization(EntityId shipId);
    bool toggleJEngine(EntityId shipId);
    // the interstellar jump (GameSimulationJump.cpp): START (the charge-up) / cancel
    // DEV: normal flight at a point of the current system (AU from its star), at rest against the star
    bool devPlaceShipAtPoint(EntityId id, const glm::dvec3& au);
    // a ship into a deep-space place (StarAtlasDatabase::DeepSpaceIdBase.., no bodies): normal flight at rest in the
    // place's frame at positionM from its centre (a sump catch between the systems; DEV start deep:<n>)
    // the onboard computer holds the lane records (JUMP_MECHANICS 10k): alive, or DEV ELITE_DEV_NAV_COMPUTER_DEAD
    // THE ONE writer of the motion's mirrors of the ship's own facts (the duplicated-truth pass, «у mutable факта должен
    // быть один хозяин»: the owner is ShipCore's effective physics / devices; the motion carries a COPY for the wire and
    // the rules). Called at the spawn, in free flight and at every publication (in a hub frame the copy went stale - a
    // cargo load changed the mass and the motion kept the old one).
    static void syncReplicatedShipFacts(game::navigation::DynamicMotionState& m, const Ship& ship)
    {
        const auto physics = ship.core().effectivePhysics();
        m.shipMassT = physics.massKg / 1000.0;
        m.jumpCoreRatingT = physics.jumpCoreRatingT;
        m.navComputerState = navComputerWorks(ship) ? 1 : 2;
    }
    static bool navComputerWorks(const Ship& ship)
    {
        static const bool devDead = std::getenv("ELITE_DEV_NAV_COMPUTER_DEAD") != nullptr;
        // a hull built in code without a catalog computer (the legacy Cobra) has it integrated: it works
        return !devDead && (ship.core().devices().computer || ship.core().descriptor().catalog.computerModuleIds.empty());
    }
    // the lane records the ship carries (JUMP_MECHANICS 10h)
    void setLaneRecords(EntityId id, const std::string& records)
    {
        if (Ship* ship = getShip(id))
            ship->core().transform().motion.laneRecords = records;
    }
    void setDataCarriers(EntityId id, const std::string& carriers)
    {
        if (Ship* ship = getShip(id))
            ship->core().transform().motion.dataCarriers = carriers;
    }
    void refreshNavComputerState(EntityId id)
    {
        if (Ship* ship = getShip(id))
            ship->core().transform().motion.navComputerState = navComputerWorks(*ship) ? 1 : 2;
    }
    bool placeShipInDeepSpace(EntityId id, int systemId, const glm::dvec3& positionM);
    // a sump catch between the systems decided at the request (the server's hidden field, JUMP_MECHANICS 8): the jump
    // comes out in the deep-space place at positionM from its centre instead of the target
    struct JumpDeepCatch
    {
        int placeId = -1;
        glm::dvec3 positionM {0.0};
        // the approach shells' currents on the way (InterstellarJumpRoute.h interstellarShellDriftM; raw, before the lock)
        glm::dvec3 shellDriftM {0.0};
        std::string name;
    };
    bool requestJump(EntityId shipId, int targetSystemId, const std::string& targetName, double distanceLy, const std::string& exitBodyId,
                     const glm::dvec3& interstellarDirWorld = glm::dvec3(0.0),
                     const JumpDeepCatch* deepCatch = nullptr);
    void cancelJump(EntityId shipId, std::uint8_t why);
    // the jump sumps of a system (asteroid / debris swarms, JumpSumps.h), given by the server from the atlas
    void setJumpSumps(int systemId, const std::vector<game::navigation::jump::JumpSump>& sumps) { m_jumpSumps[systemId] = sumps; }
    // 0 = J may engage here; else the refusal code (7 an object too close, 8 below the J floor)
    // sweepSeconds > 0: the ship's straight motion over the next sweepSeconds is tested too (a keep-out / a J floor
    // ENTERED within it counts - audit 2: at 0.5 c a tick is 3000 km, a 20 km keep-out was crossed between two samples)
    int jEngineBlocked(EntityId shipId, int systemId, const glm::dvec3& shipWorld, std::string& why,
                       const glm::dvec3& shipVelocityMps = glm::dvec3(0.0), double sweepSeconds = 0.0) const;
    // J terrain contact: ground (terrain or sea surface) height above the body
    // radius in a world direction from the body centre; false = unknown (the
    // field is still loading) -> the reference sphere is used.
    struct TerrainSample
    {
        double heightM = 0.0;     // terrain or sea surface above the body radius (as rendered, ~1 m detail)
        double smoothHeightM = 0.0;   // the same on the 30 m relief (diagnostics)
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
    // a route stop's service stay at a hub (the client flies the route, NAVIGATION_V2 §1)
    void startHubService(EntityId shipId, const std::string& hubId, const std::string& action);
    // ---- refuelling at a station (GameSimulationRefuel.cpp, docs/FUEL_STATIONS.md): any ship (pilot, route stop,
    // NPC). `hubId` empty = the station the ship stands at; false = refused (the reason is in motion.refuelRefusal)
    bool requestRefuel(EntityId shipId, const std::string& hubId = std::string());
    void cancelRefuel(EntityId shipId, const char* why);
    // the station a ship stands at for a service: docked on a pad of a hub's dock, or held nearly still in a hub's
    // frame (a tender comes alongside, like bunkering at anchor); empty = none. `tender` tells which.
    std::string serviceStationOf(EntityId shipId, bool* tender = nullptr) const;

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
    // DEV combat trials (author 2026-11-10, balance data): the target fights back (ELITE_DEV_TARGET_FIGHTS=duel|ram)
    // and ship-ship contacts (ELITE_DEV_SHIP_COLLISIONS=1). Off by default - the game is unchanged.
    void devCombatStep(double dt);
    void disableShipIfCombatLost(EntityId shipId, Ship& ship);   // cockpit or all propulsion reactors lost
    void noteCombatAggression(EntityId targetId, EntityId attackerId);
    void collideShips(double dt);
    struct StaticTrack { glm::dvec3 p {0.0}; double t = 0.0; bool valid = false; };
    std::unordered_map<EntityId, StaticTrack> m_staticTracks;   // static object velocity for contacts   // ordinary ship-ship collisions (hit boxes, impulse + spin, local damage)
    // Reactor explosions (author 2026-11-10): cheap vacuum model - a short-range flash on the nearby modules and N
    // fragments (own hull: rays through the penetration model; other ships: a random number of hits by the solid
    // angle they cover, each on a random module). Processed one tick later (chain reactions without recursion).
    struct PendingExplosion
    {
        EntityId shipId;
        std::string moduleId;
        double energyMJ = 0.0;
        std::string cause;
    };
    std::vector<PendingExplosion> m_pendingExplosions;
    std::unordered_set<std::string> m_explodedReactors;   // "<ship>#<module>"
    void queueReactorExplosion(EntityId shipId, const std::string& moduleId, const std::string& cause);
    void explodeReactor(const PendingExplosion& e);
    // Containment emergency (author 2026-11-10): a reactor does not blow up at once. The emergency runs for an
    // unknown fuse (20-60 s), the core heats up, the siren sounds (critical alert); at the end it blows up or
    // collapses quietly - by chance. The crew can switch the propulsion reactors off (lower chance, not certain),
    // eject in the EC-2 cockpit, or (repair games) send a repair drone that may stabilise it in time.
    struct ContainmentEmergency
    {
        EntityId shipId;
        std::string moduleId, cause;
        double startS = 0.0, fuseS = 30.0, chance = 0.5, stabilise01 = 0.0, lastLogS = -10.0;
        double startTemperatureK = 0.0;
        double droneDispatchedS = -1.0;   // Ctrl+R during the emergency: the drone flies 8 s, then works
    };
    std::vector<ContainmentEmergency> m_containment;
    void startContainmentEmergency(EntityId shipId, const std::string& moduleId, double chance, const std::string& cause);
    void updateContainmentEmergencies(double dt);
    std::unordered_map<EntityId, EntityId> m_combatHostility;      // NPC -> the enemy it fights (combat AI)
    // Laser controller (author 2026-11-10): one law, ship-specific limits (reactor headroom, coolant).
    struct LaserUnit
    {
        double temperatureK = 0.0;   // laser body; 0 = take the coolant's
        double capacitorMJ = 0.0;
        double dischargeLeftS = 0.0;
        double electricalMJ = 0.0, beamMJ = 0.0, lastTraceS = -1.0;
        int shots = 0;
    };
    std::unordered_map<EntityId, LaserUnit> m_laserUnits;
    double laserControllerStep(EntityId shooterId, Ship& ship, const game::combat::LaserSpec& laser, double dt, bool firing);
    CombatLoadout& combatLoadout(EntityId shipId);
    game::simulation::ShipCombatSnapshot combatSnapshotFor(EntityId shipId) const;

    // Repair & Refit (F7): a preview / DEV apply of one module operation on
    // a ship owned by the commanding ship's owner (subject 0 = that ship).
    // Results are published in the subject's ShipSnapshot::refit.
    void requestRefitPreview(EntityId commandingShip, EntityId subject, const game::ship::refit::RefitOperation& operation);
    void applyRefit(EntityId commandingShip, EntityId subject, const game::ship::refit::RefitOperation& operation);
    // a refused apply (no money, not at a berth): published like an apply result, the ship untouched
    void refuseRefit(EntityId commandingShip, EntityId subject, const std::string& why);
    // a new ship `offsetM` ahead of `near` in its hub's frame (orbits with the station), at rest relative to it
    EntityId spawnShipBeside(EntityId near, ShipRole role, const ShipDescriptor& d, const ShipInitData& init, double offsetM);
    // a ship at `hubId`'s frame + localOffsetM (the restore of a saved ship); {} = no such hub frame
    EntityId spawnShipAtHub(ShipRole role, const ShipDescriptor& d, const ShipInitData& init, int systemId, const std::string& hubId,
                            const glm::dvec3& localOffsetM);
    [[nodiscard]] game::simulation::ShipRefitSnapshot refitSnapshotFor(EntityId ship) const;
    const ActiveFreeFlight* freeFlight(EntityId shipId) const;
    // This ship's dispatcher status (phase None when unknown to the dock).
    game::docking::traffic::DockTrafficShipStatus dockDispatcherStatus(EntityId shipId) const;
    // The pilot's automatic docking engaged / dropped (ClientShipCommand::RequestDockArrival).
    // dockModuleId: the dock asked for (empty: the nearest hex dock of the ship's system)
    bool requestDockArrival(EntityId shipId, bool engage, const std::string& dockModuleId = {});
    // Hub-local geometry of the dispatcher's assignment for the ship's snapshot.
    void fillDockDispatcherGeometry(EntityId shipId, game::simulation::ShipDockDispatcherSnapshot& out) const;
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

    // hit volumes from the object's production package (hub kit / settlement modules: no module descriptors)
    bool setStaticObjectPackageHitVolumes(EntityId objectId, std::vector<game::damage::HitVolume> volumes);

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
    bool isShipDestroyed(EntityId id) const noexcept;
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
    // bumped by every structural change of the static objects (their hit volumes included): a cache key
    std::uint64_t staticObjectsGeneration() const noexcept { return m_staticObjectsGeneration; }
    const std::unordered_map<EntityId, StaticObject>& staticObjects() const;
    // the berth engine rule (GameSimulationPadLanding.cpp): landed - no engines; launched - manoeuvre thrusters only
    void filterBerthControl(EntityId shipId, ShipControlState& control);
    void launchFromBerth(EntityId shipId);

    // the hub cities' radar set (traffic not made real, stray bodies, the debris field) with velocity and kind - the player's radar (GameServer) reads this
    struct HubRadarContact
    {
        std::uint32_t key = 0;
        glm::dvec3 worldPosition {0.0};
        glm::dvec3 worldVelocity {0.0};
        double radarCrossSection = 1.0;
        std::uint8_t kind = 0;   // game::radar::RadarTrackKind*
        double boundingRadiusMeters = 0.0;   // the body's real size (a hull's half diagonal, a piece's half size)
    };
    std::vector<HubRadarContact> hubRadarContacts(const glm::dvec3& observerWorld, int systemId, std::size_t maxCount) const;
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
        double atmosphereOuterRadiusMeters,
        double surfacePressurePa = 0.0
    );
    void setOrbitalUniverseTimeSeconds(double t);

    int activeCelestialSystemId() const noexcept
    {
        return m_activeCelestialSystemId;
    }

    // Multi-system tick (author 2026-11-08: "подключишь симуляцию всех систем"): GameServer runs
    // update() once per occupied system, switching the celestial context before each pass. Pass 0
    // also runs the once-per-tick work (clocks, combat, traffic shells, dev scenarios); the other
    // passes advance only the ships / hubs / zones of their own system.
    void setSystemPass(int index) noexcept { m_systemPassIndex = index; }
    bool firstSystemPass() const noexcept { return m_systemPassIndex == 0; }
    bool passOwns(const Ship* ship) const;
    bool passOwns(EntityId id) const;
    // A command that reads the system context (gravity bodies, hubs, corridors) must run with its ship's system
    // active (audit 2026-11-19). Logs and counts a violation; cheap. contextViolations(): the total (tests).
    bool commandContextOk(EntityId id, const char* where) const;
    long long contextViolations() const { return m_contextViolations; }
    // DEV / load test: the ship leaves for systemId at once (the jump's instant part, no charge,
    // no checks); its system's pass puts it on the exit body's orbit like a jump exit.
    bool devPlaceShipInSystem(EntityId shipId, int systemId, const std::string& systemName, const std::string& exitBodyId);
    // DEV start point (ELITE_DEV_START=hub:<id>@<km>): normal flight <km> above the hub (radially away from the
    // body it orbits), at the hub's velocity, the hub as the velocity reference.
    bool devPlaceShipNearHub(EntityId shipId, const std::string& hubId, double distanceKm);

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
    void updateFreeFlights(double dt);
    // DEV (live checks of the loss path: cargo, salvage): the ship destroyed as on a surface impact; false = no ship /
    // already destroyed
    bool devDestroyShip(EntityId shipId);
    // an impact beyond survival: the whole ship destroyed (every module), a wreck under physics only
    void destroyShipOnImpact(EntityId shipId, Ship& ship, std::uint8_t cause, double speedMps, double decelerationG,
                             const game::navigation::capsule::Outcome& capsule);
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
    // Live traffic (author 2026-11-21): a materialized traffic ship (dock traffic / promoted city air traffic) near a
    // player's crosshair at weapon range, a ship that fights or was damaged, becomes a real NPC: physics, collisions as a
    // free body, its own pilot. While it has no fight it flies to its script's point (TrackFormationSlot); it goes back
    // to the script with a hysteresis (long out of the crosshair, intact, no fight, on its point).
    struct LiveTraffic
    {
        double promotedS = 0.0;
        double outSinceS = -1.0;
        game::navigation::MotionMode savedMode = game::navigation::MotionMode::Inertial;
        game::navigation::KinematicFrame savedTravelFrame;
        bool savedMatched = false;
        std::string savedMatchedId;
        std::string reason;
    };
    struct TrafficSlot
    {
        std::string hubId;
        glm::dvec3 localPosition {0.0};   // in the hub navigation frame
        glm::dvec3 localVelocity {0.0};
        glm::mat4 orientation {1.0f};
        glm::dvec3 worldPosition {0.0};
        glm::dvec3 worldVelocity {0.0};
        bool eligible = false;            // in open space (not on / near the pads)
        double atS = -1.0;
    };
    // Hub city pads (author 2026-11-21, GameSimulationPadLanding.cpp): the city's pad service flies a ship that asked
    // for a building's pad (ClientShipCommand::LandOnPad): to the transit height over the city, across, down onto the
    // pad; then it holds it there. Any control input (index 0) gives the ship back to its pilot.
    struct PadLanding
    {
        std::string frameModuleId;   // the city frame module (HubPlan)
        std::string address;         // the building ("B4-07")
        glm::dvec3 padLocal {0.0};   // the pad in the city frame
        glm::dvec3 outLocal {0.0, 1.0, 0.0};   // its outward normal (up; down for the lower tier)
        double transitLocal = 300.0;           // the transit height over the plane (along outLocal)
        int phase = 0;               // 0 to the transit height, 1 across, 2 down, 3 landed
        double speedMps = 0.0;
        double sinceS = 0.0;
        double logS = -1.0e9;
        bool autoLanding = true;     // the pad's service lands the ship (false: brought over the zone and handed over)
        double gearNagS = -1.0e9;
        std::vector<glm::dvec3> route;   // hubPadRoute: the climb point, the detours, over the pad (city frame)
        std::size_t leg = 0;             // the across leg flown now (route index)
        double touchdownS = 0.0;         // the legs took the ship (phase 3): the damped sag runs from here
        double touchdownMps = 0.0;       // its touchdown speed
        double touchdownPitch = 0.0;     // the rock of the sag applied so far
        // LANDED = coupled to the pad (author 2026-11-25: «он имеет сцепку с площадкой ... двигатели - не нужны»): its
        // attitude relative to the pad's axes, taken level at the touchdown; then the pad carries it (a ring's spin too)
        bool landedAttitudeKnown = false;
        glm::dmat3 landedAttitude {1.0};
        int bay = 0;                     // the landing place of the module (the dispatcher: hubBerthPlaces)
        bool hangar = false;             // a repair hangar: phase 2 = down to its door, in, down onto the bay
        int hangarStep = 0;
        glm::dvec3 doorFrontLocal {0.0}, bayEntryLocal {0.0};
    };
    std::unordered_map<EntityId, PadLanding> m_padLandings;
    std::unordered_map<EntityId, std::pair<std::string, double>> m_padNotices;   // "taken" / "refused:<key>", server time
    std::string padServiceStatus(EntityId shipId) const;   // ShipSnapshot::padService
    // the city's landing places reserved / taken now ("<address>#<bay>", "!" after it = reserved, on the way in) -
    // ObjectSnapshot::occupiedBerths (the cards, the capture zones' colour)
    std::vector<std::string> cityOccupiedBerths(EntityId cityId) const;
    // the BERTH ENGINE RULE (author 2026-11-22: «если игрок сел на площадку - управление маршевыми и манёвровыми не
    // работает. Только по нажатию ctrl+L - Launch - включаются манёвровые. После выхода - включаются все»): the player's
    // control filtered while landed (nothing) / launched but still in the place's zone (manoeuvre thrusters only)
    struct ManoeuvreOnly
    {
        std::string frameModuleId;
        glm::dvec3 padLocal {0.0}, outLocal {0.0, 1.0, 0.0};
        double since = 0.0;
    };
    std::unordered_map<EntityId, ManoeuvreOnly> m_manoeuvreOnly;
    void updatePadLandings(double dt);
    // the cockpit-capsule (GameSimulationCapsule.cpp): the take-over of a lost hull, the wreck left behind
    void updateCapsules(double dt);
    // a turning deck under the ship (a ring city turning with its wheel, memory ring-rim-landing; DeckFrame.h): its
    // axis and spin in the ship's travel frame, or none
    void updateDeckFrame(EntityId shipId, Ship& ship);
    std::unordered_set<EntityId> m_capsuleTakeoverPending;
    struct CapsuleWake { glm::dvec3 position {0.0}; glm::dvec3 velocity {0.0}; double logClockS = 0.0; };
    std::unordered_map<EntityId, CapsuleWake> m_capsuleWake;
    // the pads near the players: an automatic pad's capture zone takes a ship with its legs out; a hand-flown pad counts a soft
    // touchdown on legs as landed; a hard one breaks the legs (GameSimulationPadLanding.cpp)
    void updatePadTouchdowns(double dt);
    bool startPadService(EntityId shipId, const std::string& frameModuleId, const std::string& address, int phase);
    std::unordered_map<EntityId, std::uint8_t> m_landingGear;   // 0 in, 1 out, 2 broken (ShipSnapshot::landingGear)
    std::unordered_map<EntityId, LiveTraffic> m_liveTraffic;
    std::unordered_map<EntityId, TrafficSlot> m_trafficSlots;
    bool isScriptedTrafficShip(EntityId id) const noexcept;
    bool isLiveTrafficShip(EntityId id) const noexcept { return m_liveTraffic.count(id) != 0; }
    // the script's pose of a materialized traffic ship this tick; true when the script may write it (not live)
    bool noteTrafficSlot(EntityId id, const std::string& hubId, const glm::dvec3& worldPosition,
                         const glm::dvec3& worldVelocity, const glm::mat4& orientation, bool eligible);
    void updateLiveTraffic(double dt);
    bool liveTrafficGoal(EntityId id, const Ship& ship, NpcNavigationGoal& outGoal) const;
    void releaseLiveTraffic(EntityId id, const char* why);
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
    // One site per hex dock (keyed by its hub module id): its own dispatcher / pads / capture / NPC traffic.
    struct DockSite
    {
        std::string dockModuleId;
        std::string hubId;
        std::unique_ptr<game::traffic::dock::DockServiceTraffic> traffic;
        std::unordered_map<std::uint64_t, EntityId> shellByActor;
        double phaseOffsetRad = 0.0;
        double statusSeconds = -1.0e30;
        double routeSeconds = -1.0e30;
        double zoneRadiusMeters = 0.0;
    };
    std::map<std::string, DockSite> m_dockSites;
    std::unordered_map<EntityId, std::string> m_pilotDockSite;   // pilot ship -> the dock site handling it
    const std::vector<std::pair<const StaticObject*, const StaticObject*>>& hexDockPairs() const;
    mutable std::vector<std::pair<const StaticObject*, const StaticObject*>> m_hexDockPairsCache;
    mutable std::uint64_t m_hexDockPairsCacheGeneration = 0;
    // bumped by EVERY structural change of m_staticObjects (insert / erase / identity / hub attachment / hit volumes):
    // caches of pointers or of derived lists validate by it, never by the container size (erase + insert keeps it)
    std::uint64_t m_staticObjectsGeneration = 1;
    mutable bool m_hexDockPairsCacheValid = false;
    bool hexDockPair(const std::string& dockModuleId, const StaticObject*& dock, const StaticObject*& base) const;
    DockSite* pilotDockSite(EntityId shipId);
    const DockSite* pilotDockSite(EntityId shipId) const;
    void updateDockSiteTraffic(DockSite& site, const StaticObject* dock, const StaticObject* base, int siteIndex);
    void updateDockSitePilots(DockSite& site, const StaticObject* dock, const StaticObject* base);
    std::unordered_set<EntityId> m_dockTrafficShells;
    bool m_dockObserverPlaced = false;
    // Replicated traffic identity + remaining route per traffic ship.
    std::unordered_map<EntityId, game::simulation::ShipTrafficSnapshot> m_dockTrafficInfo;
    std::FILE* m_dockTrafficLog = nullptr;
    bool m_dockTrafficDisabled = false;
    std::unordered_set<EntityId> m_dockStationControlledShips;
    double m_dockPilotTraceSeconds = -1e9;
    // Port fees (TRADE_DESIGN §5.2, step 4): one open call per pilot ship at a berth - the docking fee is booked when
    // the dispatcher reports Docked, the berth fee when the ship leaves the berth (EconomyServer is idempotent per
    // callId too). Runs in the ship's system pass (updateDockSitePilots).
    struct PortCall
    {
        std::string callId;
        std::string hubId;
        std::string dockModuleId;
        double dockedAtSeconds = 0.0;   // the server timeline clock
    };
    std::unordered_map<EntityId, PortCall> m_portCalls;
    std::unordered_set<EntityId> m_portCallsChecked;   // pilot ships whose saved economy call was looked at (restart)
    void updatePortCall(EntityId id, const DockSite& site, bool docked);
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
    std::unordered_map<EntityId, ActiveFreeFlight> m_freeFlights;
    TerrainHeightProvider m_terrainHeight;
    BodyOrientationProvider m_bodyOrientation;
    // J keep-out (JDrive.h): the nearest hub / station / beacon or other ship whose keep-out holds a world point
    struct JKeepOutHit
    {
        std::string objectId;
        glm::dvec3 positionMeters {0.0};
        glm::dvec3 velocityMps {0.0};
        double distanceMeters = 0.0;
        bool ship = false;
    };
    bool jKeepOut(EntityId shipId, int systemId, const glm::dvec3& worldMeters, JKeepOutHit& hit,
                  const glm::dvec3& shipVelocityMps = glm::dvec3(0.0), double sweepSeconds = 0.0) const;
    std::vector<EntityId> m_pendingStabilize;
    struct ActiveJump
    {
        int targetSystemId = -1;
        int fromSystemId = -1;   // the exit error's beacon lock (JumpRules.h exitLockFactor)
        std::string targetName;
        std::string exitBodyId;
        double distanceLy = 0.0;
        double costT = 0.0;
        double durationS = 0.0;
        double elapsedS = 0.0;
        bool jumped = false;
        int waitTicks = 0;
        std::string exitSampleId;
        glm::dvec3 exitLastCentre {0.0};
        bool inSystem = false;     // an in-system jump (JumpRules.h): no system change, the exit at exitBodyId
        double distanceAu = 0.0;
        // ---- the procedure (JUMP_MECHANICS.md 10b): 0 to the start zone, 1 aim the nose, 2 charge; then the jump
        int phase = 0;
        std::string zoneBodyId;              // the zone is held against this body ("" = an absolute point)
        glm::dvec3 zoneOffsetM {0.0};        // the zone point relative to it
        glm::dvec3 interstellarDir {0.0};    // world direction to the target star (interstellar)
        std::vector<glm::dvec3> waypointsStarRel;   // in-system intermediate step ends, relative to the star
        std::string starId;
        int legIndex = 0;
        double totalCostT = 0.0;
        double chargedHoldS = 0.0;
        double lineCheckS = 0.0;
        glm::dvec3 exitRadial {0.0};
        glm::dvec3 currentDriftM {0.0};
        JumpDeepCatch deepCatch;             // placeId >= 0: caught by a swarm between the systems      // the currents' drift of the steps so far (raw, before the beacons' compensation)
        glm::dvec3 lastStepFromM {0.0};      // where the last step started (its length scales the exit error)         // in-system final exit: the target's side facing the incoming line
        std::unordered_map<std::string, glm::dvec3> lastCenters;   // body velocities by difference
        std::unordered_map<std::string, glm::dvec3> velocities;
        bool commandValid = false;
        ShipControlState command;            // what the procedure flies (applied in the free-flight pass)
    };
    // the procedure's helpers (GameSimulationJump.cpp)
    std::string jumpLineBlockedBy(const glm::dvec3& from, const glm::dvec3& to, const std::string& excludeBodyId,
                                  const std::string& excludeBodyId2 = {}) const;
    bool planJumpZone(EntityId shipId, ActiveJump& j, const glm::dvec3& shipM, const glm::dvec3& legEndM, bool lineMustBeClear = true);
    glm::dvec3 jumpLegEnd(const ActiveJump& j, const glm::dvec3& shipM) const;
    double jumpLegCostT(const ActiveJump& j, const glm::dvec3& fromM, double massT) const;
    std::unordered_map<EntityId, ActiveJump> m_jumps;
    // a jump asked while the station holds the ship (docked / departing): the procedure asks for the departure and
    // starts itself when the station releases the ship (author 2026-11-18: the computer runs the modes it needs)
    struct PendingJumpRequest
    {
        int targetSystemId = -1;
        std::string targetName;
        double distanceLy = 0.0;
        std::string exitBodyId;
        glm::dvec3 interstellarDirWorld {0.0};
        bool hasDeepCatch = false;
        JumpDeepCatch deepCatch;
        double ageS = 0.0;             // how long the procedure has been waiting
        std::string waitingFor;        // what for (the log)
    };
    std::unordered_map<EntityId, PendingJumpRequest> m_pendingJumpRequests;
    std::unordered_map<int, std::vector<game::navigation::jump::JumpSump>> m_jumpSumps;
    // the step from -> to may fall into a sump: true = caught, the ship is out at the catch point and the jump is over
    bool jumpSumpCatch(EntityId shipId, ActiveJump& j, const glm::dvec3& from, const glm::dvec3& to);
    // the currents' drift of a straight step in the active system (JumpCurrents.h), raw
    glm::dvec3 jumpCurrentDriftHere(int systemId, const glm::dvec3& from, const glm::dvec3& to) const;
    void updateJumps(double dt);
    bool shipAwaitingJumpExit(EntityId shipId) const;   // automatic stabilization after the free-flight pass
    struct ZoneWatch
    {
        std::unordered_map<std::string, bool> inside;
        std::unordered_map<std::string, bool> gatePassed;
        std::string lastBody;
    };
    std::unordered_map<EntityId, ZoneWatch> m_zoneWatch;
    double m_zoneCheckSeconds = 0.0;
    // a route stop's service at a hub (the client's route asks; the server fills the J charge - an
    // authority action): the ship must stay in that hub's frame for the whole stay
    struct HubService
    {
        std::string hubId;
        std::string action;
        double seconds = 0.0;
        bool refuelAsked = false;     // the refuel service was asked (GameSimulationRefuel.cpp)
        double refuelRetryS = 0.0;
    };
    std::unordered_map<EntityId, HubService> m_hubServices;
    void updateHubServices(double dt);
    // refuelling jobs (GameSimulationRefuel.cpp): one per ship; the stations' stock left (t) - starts from the data
    // file, drained by the jobs, not restocked yet (tankers later)
    struct RefuelService
    {
        std::string hubId;
        game::fuel::RefuelJob job;
        game::fuel::PumpSpec pump;
        bool tender = false;
        double priceCrPerT = 0.0;
        // the station's fuel this job reserved at its start (the economy's stock minus the other running jobs'
        // reservations there); the pump draws it down; the delivery is written to the economy at the end
        double allocatedT = 0.0;
        double stockT = 0.0;
    };
    std::unordered_map<EntityId, RefuelService> m_refuels;
    // (no station stock here: EconomyServer owns the stock and the price - audit 2026-11-19)
    std::unordered_map<int, double> m_npcRefuelScanSecondsBySystem;   // per system: updateRefuels runs in every system pass
    mutable long long m_contextViolations = 0;
    void updateRefuels(double dt);
    void endRefuel(EntityId shipId, RefuelService& service, game::fuel::RefuelPhase end, const char* why);

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
        int systemId = -1;          // flown in this system's pass only (multi-system tick)
    };
    struct CombatTrack
    {
        glm::dvec3 positionW {0.0}, velocityW {0.0};
        bool valid = false;
    };
    void updateCombat(double dt, bool tickStart);
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

    // ---- hub city damage (GameSimulationCityDamage.cpp; game::simulation::CityDamage.h)
    struct CityRayHit
    {
        EntityId object {};
        int volume = -1;
        double t = 0.0;                  // along the ray, metres
        glm::dvec3 pointW {0.0};
    };
    struct CityDamageState
    {
        std::unordered_map<std::int32_t, double> energyMJ;          // per node: the energy taken
        std::unordered_map<std::int32_t, std::uint8_t> variant;     // per node: CityBuildingVariant (absent = intact)
        std::unordered_map<std::int32_t, double> thresholdMJ;       // per node: its first-stage threshold (cached)
        std::unordered_map<std::int32_t, std::string> folder;       // per node: its module folder (cached)
    };
    // the first city volume the world segment startW -> endW meets; the city moves over the step: the start is tested
    // against where it was dt seconds ago (a missile's step), dt = 0 for an instant ray (a laser)
    bool castCitySegment(const glm::dvec3& startW, const glm::dvec3& endW, double dt, CityRayHit& out) const;
    // energy into the building of a city volume; kind: 0 laser, 1 shaped warhead, 2 kinetic slug
    void damageCity(const CityRayHit& hit, double energyMJ, int kind);
    std::vector<game::simulation::CityBuildingState> cityBuildingStates(EntityId objectId) const;
    void devCityFireStep();          // DEV: ELITE_DEV_CITY_FIRE (the player's weapons fired on a schedule)
    std::unordered_map<std::uint64_t, CityDamageState> m_cityDamage;
    // The scheduled traffic of a hub city (game::navigation::traffic::HubTrafficSchedule, shared with the clients that
    // draw it): a visit the player's weapon meets becomes a real ship the server simulates from then on (author
    // 2026-11-18: «игрок нападает на корабль. тот становится настоящим и сервер начинает его симуляцию»).
    struct ScheduledVisitHit
    {
        EntityId city {};
        int visit = -1;                  // index into the city's schedule visits
        double t = 0.0;                  // along the segment, metres
        glm::dvec3 pointW {0.0};
    };
    bool castScheduledVisits(const glm::dvec3& startW, const glm::dvec3& endW, double dt, ScheduledVisitHit& out) const;
    EntityId materializeScheduledVisit(const ScheduledVisitHit& hit);
    // the visits of a city now real ships (replicated: the clients stop drawing them)
    std::vector<std::uint32_t> cityMaterializedVisits(EntityId objectId) const;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_materializedVisits;

    // ---- hub security / traffic control (GameSimulationHubSecurity.cpp, docs/HUB_SECURITY.md)
    void updateHubSecurity(double dt);
    // a kinematic (scripted) traffic ship's pose with its collision-avoidance offset layer applied (any peaceful server
    // ship on a collision course with a real ship evades within its own Newtonian limits); hubId: whose control reports it
    void applyTrafficEvasion(EntityId self, glm::dvec3& position, glm::dvec3& velocity, const glm::dvec3& forward,
                             const glm::dvec3& up, double lengthMeters, double dt, const std::string& hubId);
    void refreshEvasionThreats();
    void noteHubSecurityEvent(const std::string& hubId, int kind, EntityId ship, const std::string& text);
    void despawnPromotedAirShip(EntityId shipId);
    // the radar sees EVERYTHING (author 2026-11-22: «радар просто обязан ловить и корабли трафика ... если есть
    // ограничение по количеству - то ближайшие»; «даже обломки»): the hub cities' air traffic not made real and the stray
    // bodies of their defence ring, nearest first, at most maxCount
    std::vector<std::uint32_t> cityPromotedAirSlots(EntityId objectId) const;
    struct HubSecurityCity
    {
        EntityId city {};
        std::string hubId;
        const game::navigation::HubPlan* plan = nullptr;
        const game::navigation::traffic::HubLaneNetwork* lanes = nullptr;
        game::traffic::security::DefenceLayout defence;
        double lastStrayCheck = -1.0;
    };
    // per system (GameServer runs one simulation pass per active system each tick): the cities and the 4 Hz timer
    struct HubSecuritySystem
    {
        std::vector<HubSecurityCity> cities;
        std::uint64_t staticGeneration = 0;
        double checkSeconds = 0.0;
    };
    std::unordered_map<int, HubSecuritySystem> m_securityBySystem;
    // Lifetime (audit 2026-11-19): ONE place knows every EntityId-keyed table.
    //  forgetShipDetail(id) - the full ship's transient state (combat units, burns, tracks, evasion, NPC control and
    //                         cadence state); called when a ship becomes a shell (the id lives on);
    //  forgetShip(id)       - everything the id owns, detail included; called when the id leaves the simulation.
    // A new EntityId-keyed member MUST be added to one of them.
    void forgetShipDetail(EntityId id);
    void forgetShip(EntityId id);
    struct HubSecurityState
    {
        game::traffic::security::HubAlert alert = game::traffic::security::HubAlert::Calm;
        std::unordered_map<std::uint64_t, int> violators;   // ship -> ShipSecurityLevel
        double finesCr = 0.0;
        int intercepts = 0;
        int evasions = 0;
        int promotions = 0;
        int tracked = 0;
        double lastSummary = -1.0e30;
        bool dirty = false;
    };
    std::unordered_map<std::string, HubSecurityState> m_hubSecurity;
    struct WrongWayShip
    {
        game::traffic::security::WrongWayWatch watch;
        std::vector<double> incidentTimes;   // closed incidents (server time): the repeat escalation window
        std::string hubId;
        std::string laneId;
        double levelUntil = 0.0;
        double debugLogAt = -1.0e30;
    };
    std::unordered_map<EntityId, WrongWayShip> m_wrongWay;
    struct PromotedAirShip
    {
        EntityId city {};
        std::string hubId;
        std::uint32_t slot = 0;
        int lane = -1;
        double along0 = 0.0;
        double time0 = 0.0;
        double speedMps = 0.0;
        double lengthMeters = 30.0;
        double curSpeedMps = -1.0;   // the speed it flies now (< 0: its lane speed) - the traffic rule slows it
    };
    std::unordered_map<EntityId, PromotedAirShip> m_promotedAir;
    std::unordered_set<EntityId> m_airTrafficShells;
    struct EvasionThreat
    {
        EntityId id {};
        glm::dvec3 position {0.0};
        glm::dvec3 velocity {0.0};
        double radius = 20.0;
    };
    std::vector<EvasionThreat> m_evasionThreats;
    double m_evasionThreatsTime = -1.0;
    int m_evasionThreatsSystem = -999;
    // in stage 3 (dock traffic, hub security) the hub objects are already at the next epoch while the free-flying
    // ships are not (a step behind: 35 km/s x 20 ms = 0.7 km at Earth): the ships are projected by velocity x this
    struct EvasionTrack
    {
        game::traffic::security::EvasionOffset offset;
        std::uint64_t threat = 0;
        double nextLog = 0.0;
        double lastTime = -1.0;
    };
    std::unordered_map<EntityId, EvasionTrack> m_evasion;
    double m_securityPerfMs = 0.0;
    long long m_securityPerfTicks = 0;
    double m_securityPerfLogAt = 0.0;
    double m_combatClockS = 0.0;
    std::unordered_map<EntityId, double> m_devTargetMissileClockS;   // DEV duel: last missile launch
    std::unordered_set<EntityId> m_devTargetLaserOn;
    std::unordered_map<std::uint64_t, double> m_devContactClockS;      // DEV ram: last contact per pair
    std::unordered_map<EntityId, double> m_devRamRetreatUntilS;        // DEV ram: back off after a contact
    std::unordered_map<EntityId, glm::dvec3> m_devRamLockedW;          // DEV straight ram: the velocity locked at the aim

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

    std::unordered_map<int, double> m_activationShadowEvaluationSecondsBySystem;   // per system pass
    bool m_activationShadowEvaluated = false;
    double m_activationShadowCsvAccumulatorSeconds = 0.0;
    bool m_activationShadowCsvInitialized = false;

    std::unordered_map<std::string, glm::dvec3> m_hubVelocityMetersPerSecond;

    
    
    // The dynamic simulation currently advances one star-system context at a
    // time. The id is explicit so entities from another system can never be
    // evaluated against the active system's celestial cache by accident.
    int m_activeCelestialSystemId = -1;
    int m_systemPassIndex = 0;
    bool m_npcRepairThinkTick = false;
    std::unordered_map<int, double> m_zoneCheckSecondsBySystem;

    std::unordered_map<std::string, glm::dvec3> m_celestialBodyPositionsAu;
    std::unordered_map<std::string, glm::dvec3> m_celestialBodyVelocitiesMetersPerSecond;

    struct CelestialBodyGravityParameters
    {
        double radiusMeters = 0.0;
        double gravitationalParameterM3s2 = 0.0;
        // Catalog atmosphere outer radius; equals radiusMeters for airless bodies.
        double atmosphereOuterRadiusMeters = 0.0;
        double surfacePressurePa = 0.0;
    };

    std::unordered_map<std::string, CelestialBodyGravityParameters>
        m_celestialBodyGravityParameters;

    double m_orbitalUniverseTimeSeconds = 0.0;
    std::unordered_map<std::string, game::navigation::HubNavigationFrame>
        m_hubNavigationFrames;


    std::vector<game::navigation::GravityBody> m_gravityBodies;
    std::vector<game::navigation::OrbitalCorridor> m_orbitalCorridors;

    // the celestial context of the systems not in the current pass (multi-system tick): kept,
    // not cleared, so a switch back costs nothing and keeps the derived caches
    struct ParkedCelestialContext
    {
        std::unordered_map<std::string, CelestialBodyGravityParameters> gravityParameters;
        std::vector<game::navigation::GravityBody> gravityBodies;
        std::vector<game::navigation::OrbitalCorridor> orbitalCorridors;
    };
    std::unordered_map<int, ParkedCelestialContext> m_parkedCelestialContexts;


};
