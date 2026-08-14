#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "src/game/network/WireCompression.h"
#include "src/game/network/WireDataCodec.h"

namespace
{
using namespace game::network;
using namespace game::network::wire;

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "[FAIL] wire data-plane contract: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) <= 1.0e-9;
}

bool near(float a, float b)
{
    return std::abs(a - b) <= 1.0e-5f;
}

world::coordinates::WorldPosition position(
    std::int64_t x,
    std::int64_t y,
    std::int64_t z,
    double lx,
    double ly,
    double lz)
{
    world::coordinates::WorldPosition result;
    result.cell.x = x;
    result.cell.y = y;
    result.cell.z = z;
    result.localMeters = glm::dvec3(lx, ly, lz);
    return result;
}

SimulationSnapshot makeSnapshot()
{
    SimulationSnapshot snapshot;
    snapshot.metadata.serverTick = 991u;
    snapshot.metadata.serverTimeSeconds = 123.5;
    snapshot.metadata.universeTimeSeconds = 4567.25;
    snapshot.metadata.universeTimelineRevision = 12u;

    snapshot.replication.entitySetMode =
        ReplicatedEntitySetMode::SparseRetainMissing;
    snapshot.replication.removedShipIds = {EntityId{700u}, EntityId{701u}};
    snapshot.replication.removedObjectIds = {EntityId{800u}};
    snapshot.replication.removedHubIds = {"hub:gone"};

    ShipSnapshot ship;
    ship.id = EntityId{42u};
    ship.role = ShipRole::Player;
    ship.typeId = ObjectType::CobraMk1;
    ship.acknowledgedControlTick = 77u;
    ship.motionLabKind = game::diagnostics::HubMotionLabActorKind::FastOrbit;
    ship.transform.worldPosition = position(2, -3, 4, 10.0, 20.0, -30.0);
    ship.transform.position = glm::vec3(10.0f, 20.0f, -30.0f);
    ship.transform.orientation[0][1] = 0.25f;
    ship.transform.pitchRate = 0.1f;
    ship.transform.yawRate = 0.2f;
    ship.transform.rollRate = -0.3f;
    ship.transform.pitchInput = 0.4f;
    ship.transform.yawInput = -0.5f;
    ship.transform.rollInput = 0.6f;
    ship.transform.cruiseActive = true;
    ship.transform.jumpActive = false;
    ship.transform.targetSpeed = 125.0f;
    ship.transform.forwardVelocity = 121.0f;
    ship.transform.targetSpeedRate = 3.0f;
    ship.transform.localVelocity = glm::vec3(1.0f, 2.0f, 3.0f);
    ship.transform.referenceVelocityMetersPerSecond = glm::dvec3(4.0, 5.0, 6.0);
    ship.transform.relativeVelocityMetersPerSecond = glm::vec3(7.0f, 8.0f, 9.0f);
    ship.transform.strafeInput = 0.11f;
    ship.transform.liftInput = -0.12f;
    ship.transform.forwardInput = 0.91f;

    auto& motion = ship.transform.motion;
    motion.mode = game::navigation::MotionMode::HubTactical;
    motion.systemId = 3;
    motion.parentBodyId = "earth";
    motion.hubId = "earth_orbital_hub";
    motion.travelFrame.systemId = 3;
    motion.travelFrame.frameId = "travel:42";
    motion.travelFrame.originMeters = glm::dvec3(100.0, 200.0, 300.0);
    motion.travelFrame.linearVelocityMps = glm::dvec3(1.0, 2.0, 3.0);
    motion.travelFrame.linearAccelerationMps2 = glm::dvec3(0.1, 0.2, 0.3);
    motion.travelFrame.localToWorldBasis[1][2] = 0.125;
    motion.travelFrame.angularVelocityWorldRadPerSecond = glm::dvec3(0.01, 0.02, 0.03);
    motion.travelFrame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.001, 0.002, 0.003);
    motion.travelFrame.valid = true;
    motion.matchedToReferenceFrame = true;
    motion.matchedReferenceFrameId = "hub-frame";
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::BrakeToStop;
    motion.pendingReferenceVelocityMatch = true;
    motion.referenceVelocityMps = glm::dvec3(11.0, 12.0, 13.0);
    motion.localPositionMeters = glm::dvec3(14.0, 15.0, 16.0);
    motion.localVelocityMps = glm::dvec3(17.0, 18.0, 19.0);
    motion.targetForwardSpeedMps = 20.0;
    motion.forwardSpeedMps = 21.0;
    motion.strafeSpeedMps = 22.0;
    motion.liftSpeedMps = 23.0;
    motion.engineAccelerationMps2 = glm::dvec3(0.4, 0.5, 0.6);
    motion.desiredTacticalVelocityMps = glm::dvec3(24.0, 25.0, 26.0);
    motion.worldVelocityMps = glm::dvec3(27.0, 28.0, 29.0);
    motion.desiredRelativeVelocityMps = glm::dvec3(30.0, 31.0, 32.0);
    motion.orbitalAssistEnabled = true;
    motion.orbitalAssistMaxAngleDeg = 33.0;
    motion.orbitalAssistStrength = 0.34;
    motion.gravityAccelerationMps2 = glm::dvec3(0.0, -9.8, 0.0);
    motion.primaryGravityBodyId = "earth";
    motion.primaryGravityDistanceMeters = 35.0;
    motion.primaryGravityAltitudeMeters = 36.0;
    motion.primaryGravityAccelerationMps2 = 37.0;
    motion.orbitalCorridorId = "corridor:test";
    motion.orbitalCorridorState = 2;
    motion.orbitalAltitudeMeters = 38.0;
    motion.orbitalAltitudeErrorMeters = 39.0;
    motion.orbitalTargetSpeedMps = 40.0;
    motion.orbitalTangentialSpeedMps = 41.0;
    motion.orbitalRadialSpeedMps = 42.0;
    motion.orbitalSpeedErrorMps = 43.0;
    motion.navigationPlan.type = game::navigation::NavigationPlanType::JumpRoute;
    motion.navigationPlan.state = game::navigation::NavigationPlanState::Active;
    motion.navigationPlan.targetSystemId = "tau_ceti";
    motion.navigationPlan.targetBodyId = "tau_ceti_b";
    motion.navigationPlan.targetHubId = "tau_hub";
    motion.navigationPlan.plannedExitPositionMeters = glm::dvec3(44.0, 45.0, 46.0);
    motion.navigationPlan.plannedExitVelocityMps = glm::dvec3(47.0, 48.0, 49.0);
    motion.navigationPlan.plannedExitTimeSeconds = 50.0;
    motion.navigationPlan.arrivalErrorMeters = 51.0;
    motion.navigationPlan.arrivalAngleErrorDeg = 52.0;
    motion.navigationPlan.valid = true;
    motion.altitudeMeters = 53.0;
    motion.orbitalPhaseRadians = 0.54;
    motion.planeOffsetMeters = 55.0;
    motion.lockedToFramePosition = true;

    ship.referenceFrame.systemId = 3;
    ship.referenceFrame.frameId = "hub-frame";
    ship.referenceFrame.matchedToReferenceFrame = true;
    ship.referenceFrame.type = game::navigation::MotionMode::HubTactical;
    ship.referenceFrame.bodyId = "earth";
    ship.referenceFrame.hubId = "earth_orbital_hub";
    ship.referenceFrame.moduleId = "module:a";
    ship.referenceFrame.originMeters = glm::dvec3(1000.0, 2000.0, 3000.0);
    ship.referenceFrame.velocityMetersPerSecond = glm::dvec3(4.0, 5.0, 6.0);
    ship.referenceFrame.accelerationMetersPerSecond2 = glm::dvec3(0.4, 0.5, 0.6);
    ship.referenceFrame.angularVelocityWorldRadPerSecond = glm::dvec3(0.01, 0.02, 0.03);
    ship.referenceFrame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.001, 0.002, 0.003);
    ship.referenceFrame.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    ship.referenceFrame.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    ship.referenceFrame.normalAxis = glm::dvec3(0.0, 0.0, 1.0);
    ship.referenceFrame.localPositionMeters = glm::dvec3(5.0, 6.0, 7.0);
    ship.referenceFrame.localVelocityMetersPerSecond = glm::dvec3(8.0, 9.0, 10.0);
    ship.referenceFrame.universeTimeSeconds = 4567.25;
    ship.referenceFrame.valid = true;

    SignalReceptionResult reception;
    reception.owner = EntityId{99u};
    reception.sourceDisplayClass = SignalDisplayClass::Local;
    reception.sourceLabel = "Beacon 99";
    reception.sourceWorldPosition = position(1, 1, 1, 2.0, 3.0, 4.0);
    reception.sourceWorldPos = glm::vec3(2.0f, 3.0f, 4.0f);
    reception.distance = 100.0f;
    reception.emittedPower = 200.0f;
    reception.receivedPower = 150.0f;
    reception.noiseFloor = 2.0f;
    reception.interferencePower = 3.0f;
    reception.occlusionFactor = 0.75f;
    reception.signalToNoiseRatio = 12.0f;
    reception.stability = 0.95f;
    reception.semanticState = SignalSemanticState::Decoded;
    ship.receptions.push_back(reception);

    game::RadarContact contact;
    contact.id = EntityId{100u};
    contact.distance = 1234.5;
    contact.localPosition = glm::vec3(1.0f, 2.0f, 3.0f);
    ship.radarContacts.push_back(contact);

    game::damage::DamageEvent damage;
    damage.type = game::damage::DamageType::Explosion;
    damage.energy = 5000.0;
    damage.worldPosition = position(4, 5, 6, 7.0, 8.0, 9.0);
    damage.position = glm::vec3(7.0f, 8.0f, 9.0f);
    damage.direction = glm::vec3(0.0f, 1.0f, 0.0f);
    ship.damageEvents.push_back(damage);

    ship.shipCoreStatus.reactor.temperature = 900.0;
    ship.shipCoreStatus.reactor.criticalTemp = 1200.0;
    ship.shipCoreStatus.reactor.outputMW = 10.0;
    ship.shipCoreStatus.reactor.maxOutputMW = 20.0;
    ship.shipCoreStatus.reactor.throttle = 0.5;
    ship.shipCoreStatus.reactor.instability = 0.1;
    ship.shipCoreStatus.reactor.status = 1u;
    ship.shipCoreStatus.reactor.integrity = 0.9;
    ship.shipCoreStatus.reactor.generatedHeat = 3.0;
    ship.shipCoreStatus.reactor.heatGenerationMW = 4.0;
    ship.shipCoreStatus.thermal.temperature = 400.0;
    ship.shipCoreStatus.thermal.thermalMass = 500.0;
    ship.shipCoreStatus.thermal.storedHeat = 600.0;
    ship.shipCoreStatus.thermal.heatVolume = 700.0;
    ship.shipCoreStatus.thermal.thermalCriticalTemp = 1000.0;
    ship.shipCoreStatus.cooling.coolantTemp = 350.0;
    ship.shipCoreStatus.cooling.panels.push_back({0.8, 0.7});
    ship.shipCoreStatus.cooling.damagedPanelCount = 1;
    ship.shipCoreStatus.cooling.failedPanelIndices = {2};
    game::PowerConsumerStatus consumer;
    consumer.name = "radar";
    consumer.requestedPowerMW = 3.5;
    consumer.allocatedPowerMW = 3.0;
    consumer.priority = 2;
    consumer.operational = true;
    consumer.heatTransfer = 0.25;
    ship.shipCoreStatus.powerBus.availablePowerMW = 11.0;
    ship.shipCoreStatus.powerBus.totalRequestedMW = 12.0;
    ship.shipCoreStatus.powerBus.consumers.push_back(consumer);
    game::AlertStatus alert;
    alert.severity = 1;
    alert.system = "reactor";
    alert.message = "warm";
    alert.value = 900.0;
    alert.threshold = 850.0;
    ship.shipCoreStatus.alerts.push_back(alert);
    ship.shipCoreStatus.warningSystems = {"reactor"};
    ship.shipCoreStatus.criticalSystems = {"none"};

    ship.graph.hasModules = true;
    ship.graph.hasStructuralLinks = true;
    ship.graph.hasAssemblyModules = true;
    ship.graph.hasDetachedFragments = true;
    ship.graph.hasRepairJobs = true;
    ship.graph.hasDebugHitVolumes = true;

    game::simulation::ObjectModuleSnapshot module;
    module.moduleId = "module:a";
    module.state = 2u;
    module.health = 0.75f;
    module.aliveSupportCount = 3;
    ship.graph.modules.push_back(module);

    game::simulation::StructuralLinkSnapshot link;
    link.id = "link:a-b";
    link.ownerModuleId = "module:a";
    link.moduleAId = "module:a";
    link.moduleBId = "module:b";
    link.kind = 2;
    link.health = 0.8f;
    link.maxHealth = 1.0f;
    link.impulseTolerance = 12.0f;
    link.loadBearing = true;
    link.destroyed = false;
    link.autoGenerated = true;
    link.center = glm::vec3(1.0f, 2.0f, 3.0f);
    link.halfSize = glm::vec3(0.1f, 0.2f, 0.3f);
    link.orientation[2][1] = 0.5f;
    ship.graph.structuralLinks.push_back(link);

    game::simulation::ObjectAssemblyModuleSnapshot assembly;
    assembly.moduleId = "module:a";
    assembly.rotationAngleRad = 0.3f;
    ship.graph.assemblyModules.push_back(assembly);

    game::simulation::DebugHitVolumeSnapshot hit;
    hit.moduleId = "module:a";
    hit.subsystemId = "reactor";
    hit.layerIndex = 1;
    hit.priority = 2;
    hit.center = glm::vec3(1.0f, 1.0f, 1.0f);
    hit.halfSize = glm::vec3(2.0f, 2.0f, 2.0f);
    hit.orientation[0][2] = 0.4f;
    hit.destructible = true;
    hit.destroyed = false;
    hit.health = 0.7f;
    hit.maxHealth = 1.0f;
    hit.supportLinkVolume = true;
    hit.supportLinkId = "link:a-b";
    hit.supportModuleId = "module:b";
    ship.graph.debugHitVolumes.push_back(hit);

    game::simulation::ObjectDetachedFragmentSnapshot fragment;
    fragment.moduleId = "fragment:a";
    fragment.originalModuleId = "module:a";
    fragment.moduleClass = "panel";
    fragment.providedReplacementTags = {"panel", "metal"};
    fragment.worldPosition = position(1, 2, 3, 4.0, 5.0, 6.0);
    fragment.position = glm::vec3(4.0f, 5.0f, 6.0f);
    fragment.orientation[3][0] = 7.0f;
    fragment.linearVelocity = glm::vec3(8.0f, 9.0f, 10.0f);
    fragment.angularVelocity = glm::vec3(0.1f, 0.2f, 0.3f);
    fragment.salvageable = true;
    fragment.repairable = true;
    fragment.canReattach = false;
    fragment.debugHitVolumes.push_back(hit);
    fragment.homeLocalModel[3][1] = 4.5f;
    fragment.homeCenterLocal = glm::vec3(1.0f, 0.0f, -1.0f);
    ship.graph.detachedFragments.push_back(fragment);

    game::simulation::ObjectRepairJobSnapshot repair;
    repair.moduleId = "module:a";
    repair.droneWorldPosition = position(0, 0, 0, 1.0, 2.0, 3.0);
    repair.fragmentWorldPosition = position(0, 0, 0, 4.0, 5.0, 6.0);
    repair.homeWorldPosition = position(0, 0, 0, 7.0, 8.0, 9.0);
    repair.dronePosition = glm::vec3(1.0f, 2.0f, 3.0f);
    repair.fragmentPosition = glm::vec3(4.0f, 5.0f, 6.0f);
    repair.homePosition = glm::vec3(7.0f, 8.0f, 9.0f);
    repair.state = 3u;
    ship.graph.repairJobs.push_back(repair);

    snapshot.ships.push_back(ship);

    WorldSignal signal;
    signal.systemId = 3;
    signal.type = SignalType::Transponder;
    signal.displayClass = SignalDisplayClass::Local;
    signal.address.actor = 123u;
    signal.address.channel = 7u;
    signal.worldPosition = position(9, 8, 7, 6.0, 5.0, 4.0);
    signal.position = glm::vec3(6.0f, 5.0f, 4.0f);
    signal.power = 1000.0f;
    signal.maxRange = 2000.0f;
    signal.enabled = true;
    signal.label = "TX-123";
    signal.owner = EntityId{42u};
    snapshot.signals.push_back(signal);

    ObjectSnapshot object;
    object.id = EntityId{501u};
    object.type = ObjectType::Station;
    object.systemId = 3;
    object.worldPosition = position(3, 3, 3, 100.0, 200.0, 300.0);
    object.position = glm::vec3(100.0f, 200.0f, 300.0f);
    object.orientation[1][0] = 0.2f;
    object.linearVelocityMps = glm::dvec3(1.0, 1.5, 2.0);
    object.hubAttachment.systemId = 3;
    object.hubAttachment.hubId = "earth_orbital_hub";
    object.hubAttachment.moduleId = "station:prime";
    object.hubAttachment.localOffsetMeters = glm::dvec3(10.0, 20.0, 30.0);
    object.hubAttachment.localRotationDeg = glm::dvec3(0.0, 90.0, 0.0);
    object.hubAttachment.inheritHubOrientation = true;
    object.hubAttachment.valid = true;
    object.displayName = "Station Alpha";
    object.ownerName = "Faction A";
    object.navigationVisible = true;
    object.navigationParentBodyId = "earth";
    object.orbitalMotion.enabled = true;
    object.orbitalMotion.centerMeters = glm::dvec3(0.0, 0.0, 0.0);
    object.orbitalMotion.parentRadiusMeters = 6371000.0;
    object.orbitalMotion.altitudeMeters = 400000.0;
    object.orbitalMotion.orbitalPeriodSeconds = 5400.0;
    object.orbitalMotion.orbitalPeriodPolicy = world::orbits::OrbitalPeriodPolicy::Kepler;
    object.orbitalMotion.selfRotationPeriodSeconds = 30.0;
    object.orbitalMotion.inclinationDeg = 51.6;
    object.orbitalMotion.longitudeOfAscendingNodeDeg = 10.0;
    object.orbitalMotion.argumentOfPeriapsisDeg = 20.0;
    object.orbitalMotion.initialPhaseDeg = 30.0;
    object.orbitalMotion.epochSeconds = 40.0;
    object.graph.hasModules = true;
    object.graph.modules.push_back(module);
    snapshot.objects.push_back(object);

    game::simulation::OrbitalHubSnapshot hub;
    hub.id = "earth_orbital_hub";
    hub.name = "Earth Hub";
    hub.owner = "Faction A";
    hub.systemId = 3;
    hub.parentBodyId = "earth";
    hub.worldPosition = position(3, 4, 5, 6.0, 7.0, 8.0);
    hub.worldVelocityMps = glm::dvec3(9.0, 10.0, 11.0);
    hub.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.001, 0.0);
    hub.orientation[2][0] = 0.33f;
    hub.primeModuleId = "station:prime";
    hub.motion = object.orbitalMotion;
    snapshot.hubs.push_back(hub);

    snapshot.session.playerNavigation.currentSystemId = 3;
    snapshot.session.playerNavigation.worldPosition = ship.transform.worldPosition;
    snapshot.session.playerNavigation.orientation = ship.transform.orientation;
    snapshot.session.playerNavigation.systemLocalMeters = glm::dvec3(1.0, 2.0, 3.0);
    snapshot.session.playerNavigation.systemLocalAu = glm::dvec3(0.1, 0.2, 0.3);
    snapshot.session.playerNavigation.forward = glm::vec3(0.0f, 0.0f, -1.0f);
    snapshot.session.playerNavigation.up = glm::vec3(0.0f, 1.0f, 0.0f);
    snapshot.session.predictionWorldParams.linearDrag = 0.02f;
    snapshot.session.predictionWorldParams.maxSafeDecel = 45.0f;
    snapshot.session.universeTimeSeconds = 4567.25;
    snapshot.session.universeTimeScale = 10.0;
    snapshot.session.universeTimelineRevision = 12u;
    snapshot.session.configuredUniverseTimeScale = 10000.0;
    snapshot.session.universeTimeSimulation = true;
    snapshot.session.universeDate = "3026-08-14";

    return snapshot;
}

void testSnapshotRoundTrip()
{
    const SimulationSnapshot source = makeSnapshot();

    std::vector<std::uint8_t> payload;
    require(encodeSimulationSnapshot(source, payload),
        "SimulationSnapshot encode failed");
    require(!payload.empty(), "SimulationSnapshot payload is empty");

    SimulationSnapshot decoded;
    require(decodeSimulationSnapshot(payload, decoded),
        "SimulationSnapshot decode failed");

    require(decoded.metadata.serverTick == source.metadata.serverTick,
        "snapshot server tick mismatch");
    require(decoded.replication.entitySetMode ==
            ReplicatedEntitySetMode::SparseRetainMissing,
        "sparse replication mode mismatch");
    require(decoded.replication.removedShipIds.size() == 2u,
        "removed ship lifecycle mismatch");
    require(decoded.replication.removedHubIds == source.replication.removedHubIds,
        "removed hub lifecycle mismatch");
    require(decoded.ships.size() == 1u, "ship count mismatch");
    require(decoded.ships[0].id == EntityId{42u}, "ship identity mismatch");
    require(decoded.ships[0].transform.motion.navigationPlan.targetSystemId == "tau_ceti",
        "nested navigation plan was not preserved");
    require(near(decoded.ships[0].transform.motion.travelFrame.localToWorldBasis[1][2], 0.125),
        "double matrix state mismatch");
    require(decoded.ships[0].graph.modules.size() == 1u,
        "module graph mismatch");
    require(decoded.ships[0].graph.detachedFragments.size() == 1u,
        "detached fragment graph mismatch");
    require(decoded.ships[0].graph.detachedFragments[0].providedReplacementTags.size() == 2u,
        "nested string-vector mismatch");
    require(decoded.ships[0].shipCoreStatus.powerBus.consumers.size() == 1u,
        "ship systems payload mismatch");
    require(decoded.signals.size() == 1u && decoded.signals[0].label == "TX-123",
        "world signal payload mismatch");
    require(decoded.objects.size() == 1u && decoded.objects[0].displayName == "Station Alpha",
        "object payload mismatch");
    require(decoded.hubs.size() == 1u && decoded.hubs[0].id == "earth_orbital_hub",
        "hub payload mismatch");
    require(decoded.session.playerNavigation.currentSystemId == 3,
        "per-session navigation mismatch");
    require(decoded.session.universeDate == "3026-08-14",
        "session universe date mismatch");

    auto withTrailingGarbage = payload;
    withTrailingGarbage.push_back(0xAAu);
    SimulationSnapshot rejected;
    require(!decodeSimulationSnapshot(withTrailingGarbage, rejected),
        "snapshot decoder accepted trailing garbage");

    auto wrongSchema = payload;
    require(wrongSchema.size() >= 2u, "snapshot payload missing schema prefix");
    wrongSchema[0] = 0u;
    wrongSchema[1] = static_cast<std::uint8_t>(SimulationSnapshotWireSchemaVersion + 1u);
    require(!decodeSimulationSnapshot(wrongSchema, rejected),
        "snapshot decoder accepted wrong data schema version");
}

void testMapResponsesRoundTrip()
{
    SnapshotMetadata metadata;
    metadata.serverTick = 100u;
    metadata.serverTimeSeconds = 10.0;
    metadata.universeTimeSeconds = 20.0;
    metadata.universeTimelineRevision = 3u;

    GalaxyMapResponse galaxy;
    galaxy.requestId = 1001u;
    galaxy.metadata = metadata;
    galaxy.snapshot.universeTimeSeconds = 20.0;
    galaxy.snapshot.universeDate = "3026-08-14";
    world::celestial::GalaxyMapSystem system;
    system.id = 4;
    system.name = "Tau Ceti";
    system.starType = "G8V";
    system.starsCount = 1;
    system.positionLy = glm::dvec3(11.0, 12.0, 13.0);
    system.jurisdiction = "independent";
    galaxy.snapshot.systems.push_back(system);
    world::celestial::GalaxyMapObject galaxyObject;
    galaxyObject.id = "overlay:test";
    galaxyObject.name = "Overlay";
    galaxyObject.objectType = "event";
    galaxyObject.positionLy = glm::dvec3(1.0, 2.0, 3.0);
    galaxyObject.description = "runtime overlay";
    galaxyObject.tags = {"runtime", "test"};
    galaxy.snapshot.objects.push_back(galaxyObject);

    SystemMapResponse systemResponse;
    systemResponse.requestId = 1002u;
    systemResponse.metadata = metadata;
    systemResponse.systemId = 4;
    systemResponse.snapshot.systemId = 4;
    systemResponse.snapshot.systemName = "Tau Ceti";
    systemResponse.snapshot.universeTimeSeconds = 20.0;
    systemResponse.snapshot.universeTimeScale = 5.0;
    systemResponse.snapshot.universeDate = "3026-08-14";
    systemResponse.snapshot.systemPositionLy = glm::dvec3(11.0, 12.0, 13.0);

    world::celestial::SystemMapBody body;
    body.id = "tau_ceti_b";
    body.name = "Tau Ceti b";
    body.alternativeNames.push_back({"TC-b", {"iau", "local"}});
    body.parentId = "tau_ceti";
    body.environmentPresetId = "terrestrial_temperate";
    body.type = world::celestial::BodyType::Planet;
    body.positionAu = glm::dvec3(0.1, 0.2, 0.3);
    body.orbitCenterAu = glm::dvec3(0.0);
    body.orbitRadiusAu = 0.3;
    body.drawOrbit = true;
    body.color = glm::vec4(0.1f, 0.2f, 0.3f, 1.0f);
    body.ringVisual.displayProfile = "faint";
    body.ringVisual.renderMode = world::celestial::SystemMapRingDisplayMode::ParticleCloud;
    world::celestial::SystemMapRing ring;
    ring.name = "R1";
    ring.innerRadiusKm = 1000.0;
    ring.outerRadiusKm = 2000.0;
    ring.composition = "ice";
    ring.visibilityClass = world::celestial::SystemMapRingVisibilityClass::Secondary;
    ring.displayMode = world::celestial::SystemMapRingDisplayMode::LayeredBands;
    body.rings.push_back(ring);
    systemResponse.snapshot.bodies.push_back(body);

    world::celestial::SystemMapObject mapObject;
    mapObject.id = EntityId{123u};
    mapObject.stableId = "entity:123";
    mapObject.name = "Remote ship";
    mapObject.owner = "player-b";
    mapObject.parentBodyId = "tau_ceti_b";
    mapObject.kind = world::celestial::SystemMapObjectKind::Ship;
    mapObject.positionAu = glm::dvec3(0.01, 0.02, 0.03);
    mapObject.systemId = 4;
    mapObject.hasOrbit = true;
    mapObject.orbitCenterAu = glm::dvec3(0.0);
    mapObject.orbitRadiusAu = 0.05;
    systemResponse.snapshot.objects.push_back(mapObject);

    DetailMapResponse detail;
    detail.requestId = 1003u;
    detail.metadata = metadata;
    detail.target.sceneKind = world::celestial::DetailSceneKind::SpatialVolume;
    detail.target.focusClass = world::celestial::DetailObjectClass::Ship;
    detail.target.systemId = 4;
    detail.target.systemPositionLy = glm::dvec3(11.0, 12.0, 13.0);
    detail.target.anchorId = "anchor";
    detail.target.focusId = "entity:123";
    detail.target.spatialCell.level = 6;
    detail.target.spatialCell.maximumLevel = 6;
    detail.target.spatialCell.x = 1;
    detail.target.spatialCell.y = 2;
    detail.target.spatialCell.z = 3;
    detail.target.spatialCell.centerAu = glm::dvec3(0.1, 0.2, 0.3);
    detail.target.spatialCell.edgeAu = 0.001;

    HubMapResponse hub;
    hub.requestId = 1004u;
    hub.metadata = metadata;
    hub.systemId = 4;
    hub.hubId = "tau_hub";

    std::vector<MapResponse> responses;
    responses.emplace_back(galaxy);
    responses.emplace_back(systemResponse);
    responses.emplace_back(detail);
    responses.emplace_back(hub);

    for (std::size_t i = 0; i < responses.size(); ++i)
    {
        std::vector<std::uint8_t> payload;
        require(encodeMapResponse(responses[i], payload), "MapResponse encode failed");
        MapResponse decoded;
        require(decodeMapResponse(payload, decoded), "MapResponse decode failed");
        require(decoded.index() == responses[i].index(), "MapResponse variant mismatch");
    }

    std::vector<std::uint8_t> payload;
    require(encodeMapResponse(responses[0], payload), "GalaxyMapResponse encode failed");
    MapResponse decodedGalaxyVariant;
    require(decodeMapResponse(payload, decodedGalaxyVariant), "GalaxyMapResponse decode failed");
    const auto& decodedGalaxy = std::get<GalaxyMapResponse>(decodedGalaxyVariant);
    require(decodedGalaxy.snapshot.systems.size() == 1u,
        "GalaxyMapResponse system payload mismatch");
    require(decodedGalaxy.snapshot.objects[0].tags.size() == 2u,
        "GalaxyMapResponse nested tags mismatch");

    require(encodeMapResponse(responses[1], payload), "SystemMapResponse encode failed");
    MapResponse decodedSystemVariant;
    require(decodeMapResponse(payload, decodedSystemVariant), "SystemMapResponse decode failed");
    const auto& decodedSystem = std::get<SystemMapResponse>(decodedSystemVariant);
    require(decodedSystem.snapshot.bodies.size() == 1u,
        "SystemMapResponse body payload mismatch");
    require(decodedSystem.snapshot.bodies[0].rings.size() == 1u,
        "SystemMapResponse ring payload mismatch");
    require(decodedSystem.snapshot.objects[0].stableId == "entity:123",
        "SystemMapResponse object payload mismatch");

    require(encodeMapResponse(responses[2], payload), "DetailMapResponse encode failed");
    MapResponse decodedDetailVariant;
    require(decodeMapResponse(payload, decodedDetailVariant), "DetailMapResponse decode failed");
    require(std::get<DetailMapResponse>(decodedDetailVariant).target == detail.target,
        "DetailMapResponse target mismatch");
}

void testContainerAndEnumValidation()
{
    using namespace game::network::wire::binary;

    WireWriter countWriter;
    countWriter.u32(MaxWireContainerElements + 1u);
    WireReader countReader(countWriter.bytes());
    std::vector<std::uint32_t> rejectedVector;
    require(!decodeValue(countReader, rejectedVector),
        "binaryizer accepted oversized vector element count");

    WireWriter enumWriter;
    enumWriter.i32(9999);
    WireReader enumReader(enumWriter.bytes());
    ShipRole role = ShipRole::Player;
    require(!decodeValue(enumReader, role),
        "binaryizer accepted invalid enum value");
}

void testCompressionBoundaryIsOpaqueBytes()
{
    const SimulationSnapshot source = makeSnapshot();
    std::vector<std::uint8_t> raw;
    require(encodeSimulationSnapshot(source, raw),
        "snapshot encode before compression failed");

    NoWireCompression compressor;
    CompressedWirePayload compressed;
    require(compressWirePayload(raw, compressor, compressed),
        "opaque compressor stage failed");
    require(compressed.method == WireCompressionMethod::None,
        "compression method mismatch");
    require(compressed.uncompressedBytes == raw.size(),
        "compressor changed logical size metadata");
    require(compressed.bytes == raw,
        "NoWireCompression changed opaque payload bytes");

    std::vector<std::uint8_t> restored;
    require(decompressWirePayload(compressed, compressor, restored),
        "opaque decompressor stage failed");
    require(restored == raw,
        "compress/decompress did not restore exact serialized bytes");

    SimulationSnapshot decoded;
    require(decodeSimulationSnapshot(restored, decoded),
        "snapshot decode after opaque compressor boundary failed");
    require(decoded.ships.size() == source.ships.size(),
        "compression boundary learned/broke entity count");
}


void testFullDataPlaneBytePipeline()
{
    const SimulationSnapshot source = makeSnapshot();

    std::vector<std::uint8_t> raw;
    require(encodeSimulationSnapshot(source, raw),
        "pipeline snapshot serialization failed");

    NoWireCompression compressor;
    CompressedWirePayload compressed;
    require(compressWirePayload(raw, compressor, compressed),
        "pipeline compression failed");

    std::vector<std::uint8_t> framedPayload;
    require(encodeCompressedWirePayloadEnvelope(compressed, framedPayload),
        "compression envelope encode failed");

    WireFrame frame;
    frame.kind = WireMessageKind::SimulationSnapshot;
    frame.sequence = 123456u;
    frame.payload = framedPayload;
    const auto streamBytes = encodeFrame(frame);
    require(!streamBytes.empty(), "data-plane frame encode failed");

    WireFrameDecoder streamDecoder;
    WireFrame decodedFrame;
    bool produced = false;
    for (std::size_t offset = 0; offset < streamBytes.size();)
    {
        const std::size_t chunk = std::min<std::size_t>(
            7u,
            streamBytes.size() - offset
        );
        streamDecoder.push(streamBytes.data() + offset, chunk);
        offset += chunk;
        if (streamDecoder.pop(decodedFrame))
            produced = true;
    }

    require(produced && !streamDecoder.failed(),
        "fragmented data-plane frame did not reassemble");
    require(decodedFrame.kind == WireMessageKind::SimulationSnapshot,
        "data-plane frame kind mismatch");
    require(decodedFrame.sequence == frame.sequence,
        "data-plane frame sequence mismatch");

    CompressedWirePayload decodedCompressed;
    require(decodeCompressedWirePayloadEnvelope(
                decodedFrame.payload,
                decodedCompressed),
        "compression envelope decode failed");

    std::vector<std::uint8_t> restoredRaw;
    require(decompressWirePayload(decodedCompressed, compressor, restoredRaw),
        "pipeline decompression failed");

    SimulationSnapshot decoded;
    require(decodeSimulationSnapshot(restoredRaw, decoded),
        "pipeline snapshot deserialization failed");
    require(decoded.metadata.serverTick == source.metadata.serverTick,
        "pipeline snapshot tick mismatch");
    require(decoded.ships.size() == source.ships.size(),
        "pipeline entity count mismatch");
}

} // namespace

int main()
{
    testSnapshotRoundTrip();
    testMapResponsesRoundTrip();
    testContainerAndEnumValidation();
    testCompressionBoundaryIsOpaqueBytes();
    testFullDataPlaneBytePipeline();

    std::cout
        << "[PASS] ordered schema data-plane round-trip + opaque compression boundary\n";
    return 0;
}
