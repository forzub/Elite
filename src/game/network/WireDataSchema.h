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

// -------------------------------------------------------------------------
// Enum validation. Every enum crossing this data-plane boundary is explicit.
// -------------------------------------------------------------------------
ELITE_WIRE_ENUM_RANGE(
    game::network::ReplicatedEntitySetMode,
    game::network::ReplicatedEntitySetMode::FullAuthoritativeSet,
    game::network::ReplicatedEntitySetMode::SparseRetainMissing
);
ELITE_WIRE_ENUM_RANGE(ShipRole, ShipRole::Player, ShipRole::NPC);
ELITE_WIRE_ENUM_RANGE(ObjectType, ObjectType::None, ObjectType::RepairDroneDebug);
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
    game::navigation::VelocityAlignmentMode,
    game::navigation::VelocityAlignmentMode::None,
    game::navigation::VelocityAlignmentMode::BrakeToStop
);
ELITE_WIRE_ENUM_RANGE(
    game::navigation::NavigationPlanType,
    game::navigation::NavigationPlanType::None,
    game::navigation::NavigationPlanType::JumpRoute
);
ELITE_WIRE_ENUM_RANGE(
    game::navigation::NavigationPlanState,
    game::navigation::NavigationPlanState::Empty,
    game::navigation::NavigationPlanState::Failed
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
    game::navigation::NavigationPlan,
    v.type,
    v.state,
    v.targetSystemId,
    v.targetBodyId,
    v.targetHubId,
    v.plannedExitPositionMeters,
    v.plannedExitVelocityMps,
    v.plannedExitTimeSeconds,
    v.arrivalErrorMeters,
    v.arrivalAngleErrorDeg,
    v.valid
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
    v.forwardSpeedMps,
    v.strafeSpeedMps,
    v.liftSpeedMps,
    v.engineAccelerationMps2,
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
    v.navigationPlan,
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
    game::ShipCoreStatus,
    v.reactor,
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
    v.homeCenterLocal
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
    ShipSnapshot,
    v.id,
    v.instanceId,
    v.role,
    v.typeId,
    v.acknowledgedControlTick,
    v.motionLabKind,
    v.transform,
    v.referenceFrame,
    v.receptions,
    v.radarContacts,
    v.damageEvents,
    v.shipCoreStatus,
    v.graph
);

ELITE_WIRE_SCHEMA(
    game::simulation::HubAttachmentSnapshot,
    v.systemId,
    v.hubId,
    v.moduleId,
    v.localOffsetMeters,
    v.localRotationDeg,
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
    world::celestial::DetailSpatialCell,
    v.level,
    v.maximumLevel,
    v.x,
    v.y,
    v.z,
    v.centerAu,
    v.edgeAu
);

ELITE_WIRE_SCHEMA(
    world::celestial::DetailTarget,
    v.sceneKind,
    v.focusClass,
    v.systemId,
    v.systemPositionLy,
    v.anchorId,
    v.focusId,
    v.spatialCell
);

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
    world::celestial::CelestialBodyDisplayName,
    v.name,
    v.actors
);

ELITE_WIRE_SCHEMA(
    world::celestial::SystemMapRingVisualProfile,
    v.displayProfile,
    v.renderMode,
    v.recognizabilityPriority,
    v.artisticWidthScale,
    v.mainBandEmphasis,
    v.secondaryBandEmphasis,
    v.faintBandEmphasis,
    v.diffuseBandEmphasis,
    v.gapContrast,
    v.radialStructureStrength,
    v.fineStructureStrength,
    v.edgeSoftnessScale,
    v.brightnessVariation,
    v.minimumVisibleWidthPx,
    v.minimumMainBandWidthPx,
    v.continuousFill,
    v.particleDensity,
    v.particleOpacityScale,
    v.particleSizePxRange,
    v.radialJitter,
    v.azimuthalClumping,
    v.clusterScale,
    v.softness,
    v.artisticOcclusionEnabled,
    v.backHalfBrightness,
    v.innerEdgeDarkening
);

ELITE_WIRE_SCHEMA(
    world::celestial::SystemMapRing,
    v.name,
    v.innerRadiusKm,
    v.outerRadiusKm,
    v.composition,
    v.tint,
    v.opacity,
    v.opticalDepth,
    v.radialNoiseStrength,
    v.radialBrightnessVariation,
    v.azimuthalAsymmetry,
    v.edgeSoftness,
    v.visibilityClass,
    v.displayMode,
    v.visualOpacityScale,
    v.radialStructureScale,
    v.particleDensityScale,
    v.particleClumpiness,
    v.particleRadialJitter,
    v.particleSizePxRange,
    v.castsShadow
);

ELITE_WIRE_SCHEMA(
    world::celestial::SystemMapBody,
    v.id,
    v.name,
    v.alternativeNames,
    v.parentId,
    v.environmentPresetId,
    v.type,
    v.positionAu,
    v.orbitCenterAu,
    v.orbitRadiusAu,
    v.drawOrbit,
    v.orbitalPeriodDays,
    v.orbitalDirection,
    v.orbitalPhaseOffsetRad,
    v.radiusKm,
    v.rotationPhaseRad,
    v.dayLengthHours,
    v.rotationDirection,
    v.axialTiltDeg,
    v.axisNodeDeg,
    v.textureLongitudeOffsetDeg,
    v.color,
    v.ringPlaneInclinationOffsetDeg,
    v.ringVisual,
    v.rings
);

ELITE_WIRE_SCHEMA(
    world::celestial::SystemMapObject,
    v.id,
    v.stableId,
    v.name,
    v.owner,
    v.parentBodyId,
    v.kind,
    v.positionAu,
    v.systemId,
    v.hasOrbit,
    v.orbitCenterAu,
    v.orbitRadiusAu,
    v.orbitInclinationDeg,
    v.orbitLongitudeOfAscendingNodeDeg,
    v.orbitArgumentOfPeriapsisDeg
);

ELITE_WIRE_SCHEMA(
    world::celestial::SystemMapSnapshot,
    v.systemId,
    v.systemName,
    v.universeTimeSeconds,
    v.universeTimeScale,
    v.universeDate,
    v.bodies,
    v.objects,
    v.systemPositionLy
);

ELITE_WIRE_SCHEMA(
    game::network::GalaxyMapResponse,
    v.requestId,
    v.metadata,
    v.snapshot
);

ELITE_WIRE_SCHEMA(
    game::network::SystemMapResponse,
    v.requestId,
    v.metadata,
    v.systemId,
    v.snapshot
);

ELITE_WIRE_SCHEMA(
    game::network::DetailMapResponse,
    v.requestId,
    v.metadata,
    v.target
);

ELITE_WIRE_SCHEMA(
    game::network::HubMapResponse,
    v.requestId,
    v.metadata,
    v.systemId,
    v.hubId
);

#undef ELITE_WIRE_ENUM_RANGE
#undef ELITE_WIRE_SCHEMA

} // namespace game::network::wire::binary
