#pragma once

#include "src/game/network/WireBinaryCodec.h"
#include "src/game/network/MapSnapshotMessage.h"
#include "src/game/simulation/SimulationSnapshot.h"

namespace game::network::wire::binary
{

/*
    CANONICAL DATA-PLANE WIRE SCHEMA
    =================================

    The field order below IS the binary protocol for SimulationSnapshot and
    MapResponse payloads. The generic binaryizer in WireBinaryCodec.h walks
    these tuples for both encoding and decoding, so write/read order cannot
    silently diverge into two hand-maintained lists.

    Adding replicated data normally means:
      1. add the field to the in-memory DTO,
      2. add it once to that DTO's WireSchema tuple below,
      3. bump the data-plane schema version in WireDataCodec.h,
      4. extend the round-trip contract test.

    TCP, framing, compression and ServerRunner must not learn the new field.
*/

#define ELITE_WIRE_SCHEMA(TYPE, ...)                                      \
    template<>                                                            \
    struct WireSchema<TYPE>                                               \
    {                                                                     \
        template<typename U>                                              \
        static auto fields(U& v)                                          \
        {                                                                 \
            return std::tie(__VA_ARGS__);                                 \
        }                                                                 \
    }

#define ELITE_WIRE_ENUM_RANGE(TYPE, MIN_VALUE, MAX_VALUE)                 \
    template<>                                                            \
    struct WireEnumRange<TYPE>                                            \
    {                                                                     \
        static constexpr auto minValue = MIN_VALUE;                       \
        static constexpr auto maxValue = MAX_VALUE;                       \
    }

// Empty variant alternative used for sparse replicated payloads. It consumes
// only the variant tag and carries no fields of its own.
template<>
struct WireSchema<std::monostate>
{
    template<typename U>
    static auto fields(U&)
    {
        return std::tie();
    }
};

// -------------------------------------------------------------------------
// Enum validation. Every enum crossing this data-plane boundary is explicit.
// -------------------------------------------------------------------------
ELITE_WIRE_ENUM_RANGE(
    game::network::ReplicatedEntitySetMode,
    game::network::ReplicatedEntitySetMode::FullAuthoritativeSet,
    game::network::ReplicatedEntitySetMode::SparseRetainMissing
);
ELITE_WIRE_ENUM_RANGE(ShipRole, ShipRole::Player, ShipRole::NPC);
ELITE_WIRE_ENUM_RANGE(
    game::simulation::ShipSimulationTier,
    game::simulation::ShipSimulationTier::TrafficShell,
    game::simulation::ShipSimulationTier::WreckShell
);
ELITE_WIRE_ENUM_RANGE(
    game::simulation::ShipMotionFidelity,
    game::simulation::ShipMotionFidelity::RouteOnly,
    game::simulation::ShipMotionFidelity::FullControl
);
ELITE_WIRE_ENUM_RANGE(
    game::simulation::ShipMaterializedFidelity,
    game::simulation::ShipMaterializedFidelity::Response,
    game::simulation::ShipMaterializedFidelity::Detailed
);
ELITE_WIRE_ENUM_RANGE(ObjectType, ObjectType::None, ObjectType::OrbitalCargoShuttle);
ELITE_WIRE_ENUM_RANGE(
    game::diagnostics::HubMotionLabActorKind,
    game::diagnostics::HubMotionLabActorKind::None,
    game::diagnostics::HubMotionLabActorKind::MatchPlayer
);
ELITE_WIRE_ENUM_RANGE(
    game::navigation::MotionMode,
    game::navigation::MotionMode::Inertial,
    game::navigation::MotionMode::Destroyed
);
ELITE_WIRE_ENUM_RANGE(
    game::navigation::LocalFlightControlLaw,
    game::navigation::LocalFlightControlLaw::Newtonian,
    game::navigation::LocalFlightControlLaw::Assisted
);
ELITE_WIRE_ENUM_RANGE(
    game::ship::plant::PhysicalControlModelId,
    game::ship::plant::PhysicalControlModelId::AssistedI,
    game::ship::plant::PhysicalControlModelId::NewtonianX
);
ELITE_WIRE_ENUM_RANGE(
    game::navigation::NavigationAssetKind,
    game::navigation::NavigationAssetKind::None,
    game::navigation::NavigationAssetKind::Drone
);
ELITE_WIRE_ENUM_RANGE(
    game::navigation::VelocityAlignmentMode,
    game::navigation::VelocityAlignmentMode::None,
    game::navigation::VelocityAlignmentMode::BrakeToStop
);
ELITE_WIRE_ENUM_RANGE(
    SignalType,
    SignalType::Planets,
    SignalType::None
);
ELITE_WIRE_ENUM_RANGE(
    SignalDisplayClass,
    SignalDisplayClass::Global,
    SignalDisplayClass::Other
);
ELITE_WIRE_ENUM_RANGE(
    SignalSemanticState,
    SignalSemanticState::None,
    SignalSemanticState::Decoded
);
ELITE_WIRE_ENUM_RANGE(
    game::damage::DamageType,
    game::damage::DamageType::Laser,
    game::damage::DamageType::EMP
);
ELITE_WIRE_ENUM_RANGE(
    world::orbits::OrbitalPeriodPolicy,
    world::orbits::OrbitalPeriodPolicy::Fixed,
    world::orbits::OrbitalPeriodPolicy::Kepler
);
ELITE_WIRE_ENUM_RANGE(
    world::celestial::BodyType,
    world::celestial::BodyType::Star,
    world::celestial::BodyType::Unknown
);
ELITE_WIRE_ENUM_RANGE(
    world::celestial::SystemMapRingDisplayMode,
    world::celestial::SystemMapRingDisplayMode::LayeredBands,
    world::celestial::SystemMapRingDisplayMode::ParticleCloud
);
ELITE_WIRE_ENUM_RANGE(
    world::celestial::SystemMapRingVisibilityClass,
    world::celestial::SystemMapRingVisibilityClass::Main,
    world::celestial::SystemMapRingVisibilityClass::Diffuse
);
ELITE_WIRE_ENUM_RANGE(
    world::celestial::SystemMapObjectKind,
    world::celestial::SystemMapObjectKind::Unknown,
    world::celestial::SystemMapObjectKind::Ship
);
ELITE_WIRE_ENUM_RANGE(
    world::celestial::DetailSceneKind,
    world::celestial::DetailSceneKind::None,
    world::celestial::DetailSceneKind::LocalObject
);
ELITE_WIRE_ENUM_RANGE(
    world::celestial::DetailObjectClass,
    world::celestial::DetailObjectClass::None,
    world::celestial::DetailObjectClass::Hub
);

// -------------------------------------------------------------------------
// Identity / coordinates / generic runtime primitives.
// -------------------------------------------------------------------------
ELITE_WIRE_SCHEMA(EntityId, v.value);
ELITE_WIRE_SCHEMA(DroneInstanceId, v.value);

ELITE_WIRE_SCHEMA(
    game::navigation::NavigationAssetRef,
    v.kind,
    v.shipInstanceId,
    v.droneInstanceId
);

ELITE_WIRE_SCHEMA(
    game::navigation::OwnedNavigationAsset,
    v.asset,
    v.materializedEntityId,
    v.typeId,
    v.displayName,
    v.worldPosition,
    v.worldVelocityMps,
    v.kinematicsValid,
    v.commandable
);

ELITE_WIRE_SCHEMA(
    world::coordinates::GalacticCell,
    v.x,
    v.y,
    v.z
);

ELITE_WIRE_SCHEMA(
    world::coordinates::WorldPosition,
    v.cell,
    v.localMeters
);

ELITE_WIRE_SCHEMA(
    game::network::SnapshotMetadata,
    v.serverTick,
    v.serverTimeSeconds,
    v.universeTimeSeconds,
    v.universeTimelineRevision
);

ELITE_WIRE_SCHEMA(
    game::network::ReplicationEnvelope,
    v.entitySetMode,
    v.removedShipIds,
    v.removedObjectIds,
    v.removedHubIds
);

ELITE_WIRE_SCHEMA(
    game::navigation::KinematicFrame,
    v.systemId,
    v.frameId,
    v.originMeters,
    v.linearVelocityMps,
    v.linearAccelerationMps2,
    v.localToWorldBasis,
    v.angularVelocityWorldRadPerSecond,
    v.angularAccelerationWorldRadPerSecond2,
    v.valid
);

ELITE_WIRE_SCHEMA(
    game::ship::telemetry::EngineLiveTelemetry,
    v.valid,
    v.count,
    v.controlTick,
    v.thrust01,
    v.available01,
    v.nozzleYawRad,
    v.nozzlePitchRad,
    v.forceBodyNewtons,
    v.electricalMW,
    v.wasteHeatMW
);

ELITE_WIRE_SCHEMA(
    game::navigation::DynamicMotionState,
    v.mode,
    v.systemId,
    v.parentBodyId,
    v.hubId,
    v.travelFrame,
    v.matchedToReferenceFrame,
    v.matchedReferenceFrameId,
    v.localControlLaw,
    v.velocityAlignmentMode,
    v.pendingReferenceVelocityMatch,
    v.referenceVelocityMps,
    v.localPositionMeters,
    v.localVelocityMps,
    v.targetForwardSpeedMps,
    v.assistedTargetSpeedHold,
    v.assistedFreeRotation,
    v.freeFlight,
    v.freeFlightThrust,
    v.freeFlightReferenceId,
    v.freeFlightReferenceVelocityMps,
    v.freeFlightCaptureRefusal,
    v.freeFlightOrbitBodyId,
    v.freeFlightOrbitAltitudeM,
    v.freeFlightOrbitSpeedMps,
    v.freeFlightOrbitRadialMps,
    v.freeFlightOrbitCircularMps,
    v.freeFlightOrbitEscapeMps,
    v.freeFlightOrbitPeriapsisAltM,
    v.freeFlightOrbitApoapsisAltM,
    v.freeFlightOrbitBound,
    v.freeFlightOrbitSafeAltM,
    v.freeFlightOrbitDenseAltM,
    v.freeFlightDragMps2,
    v.freeFlightImpactSerial,
    v.freeFlightSurfaceState,
    v.destroyedCause,
    v.freeFlightGroundHeightM,
    v.freeFlightGroundMaterial,
    v.freeFlightGroundSlopeDeg,
    v.freeFlightGroundFeature,
    v.freeFlightReferenceIsRoot,
    v.jEngine,
    v.velocityReferenceKind,
    v.jAccelCommandMps2,
    v.jAccelMps2,
    v.mainEngineMaxMps2,
    v.jStopping,
    v.jChargeT,
    v.jChargeCapacityT,
    v.shipMassT,
    v.jumpCoreRatingT,
    v.jumpState,
    v.jumpTargetSystemId,
    v.jumpTargetName,
    v.jumpDistanceLy,
    v.jumpCostT,
    v.jumpChargeElapsedS,
    v.jumpChargeDurationS,
    v.jumpRefusal,
    v.jumpRefusalSerial,
    v.jumpSerial,
    v.zoneFinesTotalCr,
    v.zoneViolationSerial,
    v.zoneViolationKind,
    v.zoneViolationZoneId,
    v.assistedThrottleTrimWasActive,
    v.forwardSpeedMps,
    v.strafeSpeedMps,
    v.liftSpeedMps,
    v.mainEngineAccelerationMps2,
    v.manoeuvreAccelerationMps2,
    v.mainGimbalRightCommand01,
    v.mainGimbalUpCommand01,
    v.mainGimbalYawRad,
    v.mainGimbalPitchRad,
    v.mainPropulsionTorqueBodyNm,
    v.mainPropulsionAngularAccelerationBodyRadPerSec2,
    v.mainTrimLoad01,
    v.mainTrimUsableScale01,
    v.engineTelemetry,
    v.mainEngineDeliveredThrust01,
    v.mainEngineReportedSlotCount,
    v.requestedMainAngularAccelerationBodyRadPerSec2,
    v.assistedWorkingThrustAxisEmergency,
    v.assistedWorkingThrustAxisBody,
    v.assistedStabilizationAccelerationMps2,
    v.engineAccelerationMps2,
    v.manoeuvreGasPressure01,
    v.manoeuvreGasDepleted,
    v.desiredTacticalVelocityMps,
    v.worldVelocityMps,
    v.desiredRelativeVelocityMps,
    v.orbitalAssistEnabled,
    v.orbitalAssistMaxAngleDeg,
    v.orbitalAssistStrength,
    v.gravityAccelerationMps2,
    v.primaryGravityBodyId,
    v.primaryGravityDistanceMeters,
    v.primaryGravityAltitudeMeters,
    v.primaryGravityAccelerationMps2,
    v.orbitalCorridorId,
    v.orbitalCorridorState,
    v.orbitalAltitudeMeters,
    v.orbitalAltitudeErrorMeters,
    v.orbitalTargetSpeedMps,
    v.orbitalTangentialSpeedMps,
    v.orbitalRadialSpeedMps,
    v.orbitalSpeedErrorMps,
    v.altitudeMeters,
    v.orbitalPhaseRadians,
    v.planeOffsetMeters,
    v.lockedToFramePosition
);

ELITE_WIRE_SCHEMA(
    ShipTransform,
    v.worldPosition,
    v.position,
    v.orientation,
    v.pitchRate,
    v.yawRate,
    v.rollRate,
    v.pitchInput,
    v.yawInput,
    v.rollInput,
    v.cruiseActive,
    v.jumpActive,
    v.targetSpeed,
    v.forwardVelocity,
    v.targetSpeedRate,
    v.localVelocity,
    v.referenceVelocityMetersPerSecond,
    v.relativeVelocityMetersPerSecond,
    v.motion,
    v.strafeInput,
    v.liftInput,
    v.forwardInput
);

ELITE_WIRE_SCHEMA(
    game::simulation::ShipReferenceFrameSnapshot,
    v.systemId,
    v.frameId,
    v.matchedToReferenceFrame,
    v.type,
    v.bodyId,
    v.hubId,
    v.moduleId,
    v.originMeters,
    v.velocityMetersPerSecond,
    v.accelerationMetersPerSecond2,
    v.angularVelocityWorldRadPerSecond,
    v.angularAccelerationWorldRadPerSecond2,
    v.radialAxis,
    v.progradeAxis,
    v.normalAxis,
    v.localPositionMeters,
    v.localVelocityMetersPerSecond,
    v.universeTimeSeconds,
    v.valid
);

// -------------------------------------------------------------------------
// Signals / contacts / damage.
// -------------------------------------------------------------------------
ELITE_WIRE_SCHEMA(
    SignalAddress,
    v.actor,
    v.channel
);

ELITE_WIRE_SCHEMA(
    WorldSignal,
    v.systemId,
    v.type,
    v.displayClass,
    v.address,
    v.worldPosition,
    v.position,
    v.power,
    v.maxRange,
    v.enabled,
    v.label,
    v.owner
);

ELITE_WIRE_SCHEMA(
    SignalReceptionResult,
    v.owner,
    v.sourceDisplayClass,
    v.sourceLabel,
    v.sourceWorldPosition,
    v.sourceWorldPos,
    v.distance,
    v.emittedPower,
    v.receivedPower,
    v.noiseFloor,
    v.interferencePower,
    v.occlusionFactor,
    v.signalToNoiseRatio,
    v.stability,
    v.semanticState
);

ELITE_WIRE_SCHEMA(
    game::RadarContact,
    v.id,
    v.distance,
    v.localPosition
);

ELITE_WIRE_SCHEMA(
    game::damage::DamageEvent,
    v.type,
    v.energy,
    v.worldPosition,
    v.position,
    v.direction
);

// -------------------------------------------------------------------------
// Ship systems/status.
// -------------------------------------------------------------------------
ELITE_WIRE_SCHEMA(
    game::ReactorStatus,
    v.temperature,
    v.criticalTemp,
    v.outputMW,
    v.maxOutputMW,
    v.throttle,
    v.instability,
    v.status,
    v.integrity,
    v.generatedHeat,
    v.heatGenerationMW
);

ELITE_WIRE_SCHEMA(
    game::ThermalStatus,
    v.temperature,
    v.thermalMass,
    v.storedHeat,
    v.heatVolume,
    v.thermalCriticalTemp
);

ELITE_WIRE_SCHEMA(
    game::RadiatorPanelStatus,
    v.health,
    v.efficiency
);

ELITE_WIRE_SCHEMA(
    game::CoolingStatus,
    v.coolantTemp,
    v.totalEfficiency,
    v.allocatedPowerMW,
    v.requestedPowerMW,
    v.radiatedPowerMW,
    v.pumpCapacity,
    v.pumpHeatMJ,
    v.dt,
    v.panels,
    v.damagedPanelCount,
    v.criticalPanelCount,
    v.failedPanelIndices
);

ELITE_WIRE_SCHEMA(
    game::PowerConsumerStatus,
    v.name,
    v.requestedPowerMW,
    v.allocatedPowerMW,
    v.priority,
    v.operational,
    v.heatTransfer
);

ELITE_WIRE_SCHEMA(
    game::PowerBusStatus,
    v.availablePowerMW,
    v.overloaded,
    v.totalRequestedMW,
    v.consumers
);

ELITE_WIRE_SCHEMA(
    game::AlertStatus,
    v.severity,
    v.system,
    v.message,
    v.value,
    v.threshold
);

ELITE_WIRE_SCHEMA(
    game::EngineReactorStatus,
    v.moduleId,
    v.installed,
    v.coreTemperatureK,
    v.safeTemperatureK,
    v.generatedHeatMW,
    v.electricalOutputMW,
    v.padHeatMW,
    v.coolantHeatMW,
    v.storedHeatMW,
    v.padIntegrity01
);

ELITE_WIRE_SCHEMA(
    game::ShipCoreStatus,
    v.reactor,
    v.engineReactors,
    v.propulsionReactorsOn,
    v.propulsionStartupSeconds,
    v.propulsionSwitchRevision,
    v.newtonianRcsExecuted01,
    v.newtonianRcsSampleValid,
    v.thermal,
    v.cooling,
    v.powerBus,
    v.alerts,
    v.warningSystems,
    v.criticalSystems
);

// -------------------------------------------------------------------------
// Runtime object/module graph.
// -------------------------------------------------------------------------
ELITE_WIRE_SCHEMA(
    game::simulation::ObjectModuleSnapshot,
    v.moduleId,
    v.state,
    v.health,
    v.laserDamage01,
    v.impactDamage01,
    v.thermalDamage01,
    v.radiationDamage01,
    v.wearDamage01,
    v.internalFailureDamage01,
    v.hasEngineThermalState,
    v.engineTemperatureK,
    v.engineCoolantTemperatureK,
    v.engineImpactShock01,
    v.engineImpactShockSecondsRemaining,
    v.emitterIntactArea01,
    v.emitterMicrocrack01,
    v.emitterLaserDamage01,
    v.emitterImpactDamage01,
    v.emitterThermalDamage01,
    v.emitterRadiationDamage01,
    v.emitterWearDamage01,
    v.emitterEquivalentOperatingHours,
    v.aliveSupportCount
);

ELITE_WIRE_SCHEMA(
    game::simulation::StructuralLinkSnapshot,
    v.id,
    v.ownerModuleId,
    v.moduleAId,
    v.moduleBId,
    v.kind,
    v.health,
    v.maxHealth,
    v.impulseTolerance,
    v.loadBearing,
    v.destroyed,
    v.autoGenerated,
    v.center,
    v.halfSize,
    v.orientation
);

ELITE_WIRE_SCHEMA(
    game::simulation::ObjectAssemblyModuleSnapshot,
    v.moduleId,
    v.rotationAngleRad
);

ELITE_WIRE_SCHEMA(
    game::simulation::DebugHitVolumeSnapshot,
    v.moduleId,
    v.subsystemId,
    v.layerIndex,
    v.priority,
    v.center,
    v.halfSize,
    v.orientation,
    v.destructible,
    v.destroyed,
    v.health,
    v.maxHealth,
    v.supportLinkVolume,
    v.supportLinkId,
    v.supportModuleId
);

ELITE_WIRE_SCHEMA(
    game::simulation::DetachedFragmentMass,
    v.moduleDryKg,
    v.cargoKg,
    v.containerTareKg,
    v.totalKg,
    v.known
);

ELITE_WIRE_SCHEMA(
    game::simulation::ObjectDetachedFragmentSnapshot,
    v.moduleId,
    v.originalModuleId,
    v.moduleClass,
    v.providedReplacementTags,
    v.worldPosition,
    v.position,
    v.orientation,
    v.linearVelocity,
    v.angularVelocity,
    v.salvageable,
    v.repairable,
    v.canReattach,
    v.debugHitVolumes,
    v.homeLocalModel,
    v.homeCenterLocal,
    v.mass
);


ELITE_WIRE_SCHEMA(
    game::simulation::ObjectRepairJobSnapshot,
    v.moduleId,
    v.droneWorldPosition,
    v.fragmentWorldPosition,
    v.homeWorldPosition,
    v.dronePosition,
    v.fragmentPosition,
    v.homePosition,
    v.state
);

ELITE_WIRE_SCHEMA(
    game::simulation::ObjectGraphSnapshot,
    v.hasModules,
    v.hasStructuralLinks,
    v.hasDebugHitVolumes,
    v.hasAssemblyModules,
    v.hasDetachedFragments,
    v.hasRepairJobs,
    v.modules,
    v.structuralLinks,
    v.assemblyModules,
    v.detachedFragments,
    v.repairJobs,
    v.debugHitVolumes
);

// -------------------------------------------------------------------------
// Top-level replicated world entities.
// -------------------------------------------------------------------------
ELITE_WIRE_SCHEMA(
    game::simulation::NavigationExecutionSnapshot,
    v.valid,
    v.intentRevision,
    v.activeTargetRevision,
    v.idealLinearAccelerationDemandSystemMps2,
    v.idealAngularAccelerationDemandSystemRadPerSec2,
    v.executedLinearAccelerationDemandSystemMps2,
    v.executedAngularAccelerationDemandSystemRadPerSec2,
    v.emergency,
    v.hazardUrgency01,
    v.reactionBlocked,
    v.decisionSampled,
    v.queuedCommandApplied,
    v.pendingCommandCount
);

// Signed motor-basis limits are authoritative server data, not HUD guesses.
ELITE_WIRE_SCHEMA(
    game::ship::telemetry::ShipControlAuthorityTelemetry,
    v.valid,
    v.model,
    v.sourceActuatorRevision,
    v.positive,
    v.negative,
    v.workingForwardAccelerationMps2,
    v.workingThrustAxisBody,
    v.workingThrustAxisEmergency,
    v.workingThrustMotorIndex,
    v.rcsPairFeasibility01
);

ELITE_WIRE_SCHEMA(
    game::simulation::ShipTrafficSnapshot,
    v.active,
    v.callsign,
    v.phase,
    v.destination,
    v.routeOffsetsMeters
);

ELITE_WIRE_SCHEMA(
    game::simulation::ShipDockDispatcherSnapshot,
    v.active,
    v.phase,
    v.queuePosition,
    v.queueLength,
    v.departureQueuePosition,
    v.holdPointId,
    v.padId,
    v.stationControlled,
    v.estimatedWaitSeconds,
    v.movementsAhead,
    v.holdValid,
    v.holdLocalMeters,
    v.entryLocalMeters,
    v.axisOutLocal
);

ELITE_WIRE_ENUM_RANGE(
    game::ship::refit::RefitOperationKind,
    game::ship::refit::RefitOperationKind::Repair,
    game::ship::refit::RefitOperationKind::Install);

ELITE_WIRE_SCHEMA(game::ship::refit::RefitOperation, v.kind, v.moduleId, v.productId);

ELITE_WIRE_SCHEMA(
    game::ship::refit::RefitShipMetrics,
    v.valid, v.massTonnes, v.forwardAccelerationMps2, v.reverseAccelerationMps2, v.rcsAccelerationMps2,
    v.angularAccelerationRadPerSec2, v.mainEnginesOperational, v.mainEnginesInstalled, v.propulsionPowerAvailableMW,
    v.propulsionPowerRatedMW, v.servicePowerOutputMW, v.burnAccelerationMps2, v.burnSeconds, v.maxEngineTemperatureK,
    v.engineThermalMargin01, v.maxReactorTemperatureK, v.reactorThermalMargin01, v.serviceCoolantK, v.serviceCoolantCriticalK,
    v.propulsionCoolantK, v.emergencyCoolingMW, v.radar, v.radarRangeKm, v.receiver, v.transmitter, v.transponder,
    v.lifeSupport, v.computer, v.jumpCore, v.jumpMassWithinRating, v.docksOperational, v.moduleState, v.moduleHealth01,
    v.moduleDamage01, v.moduleWear01);

ELITE_WIRE_SCHEMA(game::ship::refit::RefitDelta, v.key, v.labelRu, v.unit, v.before, v.after, v.flag);

ELITE_WIRE_SCHEMA(game::ship::refit::RefitCost, v.repairLevel, v.labourHours, v.labourCr, v.materialCr, v.totalCr);

ELITE_WIRE_SCHEMA(
    game::ship::refit::RefitPreview,
    v.ok, v.error, v.operation, v.before, v.after, v.deltas, v.cost, v.applicable, v.applyNote);

ELITE_WIRE_SCHEMA(
    game::simulation::ShipRefitSnapshot,
    v.owned, v.ownerActor, v.registrationId, v.shipName, v.previewSerial, v.previewValid, v.preview,
    v.applySerial, v.applyResult);

ELITE_WIRE_SCHEMA(
    game::simulation::CombatBeamSnapshot,
    v.from, v.to, v.powerMW, v.hit, v.colour);
ELITE_WIRE_SCHEMA(
    game::simulation::CombatMissileSnapshot,
    v.id, v.position, v.velocityMps, v.specId);
ELITE_WIRE_SCHEMA(
    game::simulation::CombatImpactSnapshot,
    v.serial, v.where, v.energyMJ, v.kind, v.moduleId);
ELITE_WIRE_SCHEMA(
    game::simulation::ShipCombatSnapshot,
    v.laserId, v.missileId, v.missiles, v.laserFiring, v.beams, v.missilesInFlight, v.impacts);

ELITE_WIRE_SCHEMA(
    ShipSnapshot,
    v.id,
    v.instanceId,
    v.role,
    v.typeId,
    v.catalogShipId,
    v.physicalControlModel,
    v.acknowledgedControlTick,
    v.motionLabKind,
    v.simulationTier,
    v.motionFidelity,
    v.materializedFidelity,
    v.transform,
    v.referenceFrame,
    v.navigationExecution,
    v.receptions,
    v.radarContacts,
    v.damageEvents,
    v.shipCoreStatus,
    v.controlAuthorityTelemetry,
    v.graph,
    v.dockDispatcher,
    v.traffic,
    v.refit,
    v.combat
);

ELITE_WIRE_SCHEMA(
    game::simulation::HubAttachmentSnapshot,
    v.systemId,
    v.hubId,
    v.moduleId,
    v.localOffsetMeters,
    v.localRotationDeg,
    v.localAngularVelocityDegPerSecond,
    v.inheritHubOrientation,
    v.valid
);

ELITE_WIRE_SCHEMA(
    world::orbits::OrbitalMotion,
    v.enabled,
    v.centerMeters,
    v.parentRadiusMeters,
    v.altitudeMeters,
    v.orbitalPeriodSeconds,
    v.orbitalPeriodPolicy,
    v.selfRotationPeriodSeconds,
    v.inclinationDeg,
    v.longitudeOfAscendingNodeDeg,
    v.argumentOfPeriapsisDeg,
    v.initialPhaseDeg,
    v.epochSeconds
);

ELITE_WIRE_SCHEMA(
    ObjectSnapshot,
    v.id,
    v.type,
    v.systemId,
    v.worldPosition,
    v.position,
    v.orientation,
    v.linearVelocityMps,
    v.angularVelocityWorldRadPerSecond,
    v.hubAttachment,
    v.displayName,
    v.ownerName,
    v.navigationVisible,
    v.navigationParentBodyId,
    v.orbitalMotion,
    v.graph
);

ELITE_WIRE_SCHEMA(
    game::simulation::OrbitalHubSnapshot,
    v.id,
    v.name,
    v.owner,
    v.systemId,
    v.parentBodyId,
    v.worldPosition,
    v.worldVelocityMps,
    v.angularVelocityWorldRadPerSecond,
    v.orientation,
    v.primeModuleId,
    v.motion
);

// -------------------------------------------------------------------------
// Per-session authoritative state.
// -------------------------------------------------------------------------
ELITE_WIRE_SCHEMA(
    game::radar::RadarTrackId,
    v.value
);

ELITE_WIRE_SCHEMA(
    game::radar::RadarTrackReport,
    v.trackId,
    v.relativePositionMeters,
    v.relativeVelocityMps,
    v.rangeMeters,
    v.positionUncertaintyMeters,
    v.velocityUncertaintyMps,
    v.confidence,
    v.kind
);

ELITE_WIRE_SCHEMA(
    game::radar::RadarScanReport,
    v.scanSequence,
    v.measuredAtUniverseTimeSeconds,
    v.availableAtUniverseTimeSeconds,
    v.tracks
);

ELITE_WIRE_SCHEMA(
    game::navigation::NavigationSolution,
    v.estimatedWorldPosition,
    v.estimatedWorldVelocityMps,
    v.epochUniverseTimeSeconds,
    v.positionUncertaintyMeters,
    v.velocityUncertaintyMps,
    v.fixRevision
);

ELITE_WIRE_SCHEMA(
    game::simulation::ClientNavigationSensorSnapshot,
    v.radarInstalled,
    v.radarOperational,
    v.hasRadarScan,
    v.latestRadarScan,
    v.hasNavigationSolution,
    v.navigationSolution
);

ELITE_WIRE_SCHEMA(
    world::celestial::PlayerNavigationState,
    v.currentSystemId,
    v.worldPosition,
    v.orientation,
    v.systemLocalMeters,
    v.systemLocalAu,
    v.forward,
    v.up
);

ELITE_WIRE_SCHEMA(
    WorldParams,
    v.linearDrag,
    v.maxSafeDecel
);

ELITE_WIRE_SCHEMA(
    game::simulation::ClientSessionSnapshot,
    v.playerNavigation,
    v.ownedNavigationAssets,
    v.navigationSensors,
    v.predictionWorldParams,
    v.universeTimeSeconds,
    v.universeTimeScale,
    v.universeTimelineRevision,
    v.configuredUniverseTimeScale,
    v.universeTimeSimulation,
    v.universeDate
);

ELITE_WIRE_SCHEMA(
    SimulationSnapshot,
    v.metadata,
    v.replication,
    v.ships,
    v.signals,
    v.objects,
    v.hubs,
    v.session
);

// -------------------------------------------------------------------------
// Map data plane. Maps keep their already accepted client-composition model;
// this schema only serializes the fields that actually exist in MapResponse.
// -------------------------------------------------------------------------
ELITE_WIRE_SCHEMA(
    world::celestial::GalaxyMapSystem,
    v.id,
    v.name,
    v.starType,
    v.starsCount,
    v.positionLy,
    v.jurisdiction
);

ELITE_WIRE_SCHEMA(
    world::celestial::GalaxyMapObject,
    v.id,
    v.name,
    v.objectType,
    v.positionLy,
    v.description,
    v.tags
);

ELITE_WIRE_SCHEMA(
    world::celestial::GalaxyMapSnapshot,
    v.universeTimeSeconds,
    v.universeDate,
    v.systems,
    v.objects
);

ELITE_WIRE_SCHEMA(
    game::network::GalaxyMapResponse,
    v.requestId,
    v.metadata,
    v.snapshot
);




#undef ELITE_WIRE_ENUM_RANGE
#undef ELITE_WIRE_SCHEMA

} // namespace game::network::wire::binary
