#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
ACTIVATION = ROOT / "src/game/simulation/activation"
GAME_SIM_CPP = ROOT / "src/game/simulation/GameSimulation.cpp"
GAME_SIM_H = ROOT / "src/game/simulation/GameSimulation.h"
CADENCE_LAB = ROOT / "src/game/diagnostics/ActivationCadenceLab.h"
SCENE_CPP = ROOT / "src/game/scene/GameSceneSetup.cpp"
SHARED_PHYSICS_CPP = ROOT / "src/game/shared/SharedShipPhysics.cpp"
SHIP_CONTROLLER_CPP = ROOT / "src/game/ship/ShipController.cpp"

# Activation is a physical/gameplay scheduling domain. Perception,
# communications and client presentation must remain separate.
forbidden_include_tokens = (
    "Radar",
    "Sensor",
    "SignalReceiver",
    "WorldSignal",
    "Transponder",
    "Knowledge",
    "PresentationPolicy",
    "ClientWorldState",
    "GameClient",
    "Ship.h",
    "StaticObject.h",
    "ObjectDescriptorRegistry",
)

violations = []
for path in sorted(ACTIVATION.glob("*.h")):
    text = path.read_text(encoding="utf-8")
    include_lines = "\n".join(
        line for line in text.splitlines() if line.lstrip().startswith("#include")
    )
    for token in forbidden_include_tokens:
        if token in include_lines:
            violations.append(
                f"{path.relative_to(ROOT)}: forbidden dependency {token}"
            )

interaction = (ACTIVATION / "InteractionHorizon.h").read_text(encoding="utf-8")
required = (
    "lookAheadSeconds",
    "safetyMarginMeters",
    "gameplayRangeMeters",
    "timeToClosestSeconds",
    "closestSurfaceDistanceMeters",
    "entersEnvelopeWithinHorizon",
)
for token in required:
    if token not in interaction:
        violations.append(f"InteractionHorizon contract missing {token}")

spatial = (ACTIVATION / "SpatialBounds.h").read_text(encoding="utf-8")
if "LogicalDimensions" not in spatial:
    violations.append(
        "SpatialBounds must derive conservative bounds from existing LogicalDimensions"
    )

spatial_index_path = ACTIVATION / "ActivationSpatialIndex.h"
if not spatial_index_path.exists():
    violations.append("ActivationSpatialIndex.h is missing")
else:
    spatial_index = spatial_index_path.read_text(encoding="utf-8")
    for token in (
        "ActivationSpatialIndex",
        "cellSizeMeters",
        "maxVisitedCellsPerQuery",
        "conservativeQueryRadius",
        "maxAnchorRadiusMeters",
        "referenceVelocityMetersPerSecond",
        "maxAnchorResidualSpeedMetersPerSecond",
        "usedFallback",
        "std::sort",
    ):
        if token not in spatial_index:
            violations.append(f"ActivationSpatialIndex contract missing {token}")

shadow_path = ACTIVATION / "ActivationShadow.h"
if not shadow_path.exists():
    violations.append("ActivationShadow.h is missing")
else:
    shadow = shadow_path.read_text(encoding="utf-8")
    shadow_required = (
        "ActivationShadowDecision",
        "PlayerPinnedActive",
        "CurrentInteraction",
        "PredictedInteraction",
        "NoInteractionWithinHorizon",
        "SimulationMode::Active",
        "SimulationMode::Prewarm",
        "SimulationMode::Coarse",
        "anchor.systemId != subjectSystemId",
        "evaluateActivationShadowCandidates",
        "candidateAnchorCount",
        "comparableAnchorCount",
        "broadphaseFallback",
    )
    for token in shadow_required:
        if token not in shadow:
            violations.append(f"ActivationShadow contract missing {token}")

claim_path = ACTIVATION / "ActivationClaim.h"
if not claim_path.exists():
    violations.append("ActivationClaim.h is missing")
else:
    claim = claim_path.read_text(encoding="utf-8")
    claim_required = (
        "ActivationClaimKind",
        "Combat",
        "ProjectileThreat",
        "Docking",
        "minimumMode",
        "expiresAtServerTimeSeconds",
        "activationClaimCanRaise",
        "evaluateActivationClaims",
    )
    for token in claim_required:
        if token not in claim:
            violations.append(f"ActivationClaim contract missing {token}")

state_path = ACTIVATION / "ActivationStateMachine.h"
if not state_path.exists():
    violations.append("ActivationStateMachine.h is missing")
else:
    state = state_path.read_text(encoding="utf-8")
    state_required = (
        "activeReleaseDelaySeconds",
        "prewarmReleaseDelaySeconds",
        "ActivationPlanState",
        "PromoteToActive",
        "DemoteToPrewarm",
        "DemoteToCoarse",
        "updateActivationPlan",
        "transitionSerial",
        "lastTransition",
        "lastTransitionServerTimeSeconds",
    )
    for token in state_required:
        if token not in state:
            violations.append(f"ActivationStateMachine contract missing {token}")

planner_path = ACTIVATION / "ActivationPlanner.h"
if not planner_path.exists():
    violations.append("ActivationPlanner.h is missing")
else:
    planner = planner_path.read_text(encoding="utf-8")
    for token in (
        "ActivationPlannerDecision",
        "evaluateActivationClaims",
        "updateActivationPlan",
    ):
        if token not in planner:
            violations.append(f"ActivationPlanner contract missing {token}")

execution_path = ACTIVATION / "ActivationExecutionPolicy.h"
if not execution_path.exists():
    violations.append("ActivationExecutionPolicy.h is missing")
else:
    execution = execution_path.read_text(encoding="utf-8")
    for token in (
        "ActivationExecutionPolicy",
        "activeNpcAiIntervalSeconds",
        "prewarmNpcAiIntervalSeconds",
        "coarseNpcAiIntervalSeconds",
        "activeShipMotionControlIntervalSeconds",
        "prewarmShipMotionControlIntervalSeconds",
        "coarseShipMotionControlIntervalSeconds",
        "shipMotionControlIntervalSeconds",
        "advanceShipMotionControlCadence",
        "activeShipSystemsIntervalSeconds",
        "prewarmShipSystemsIntervalSeconds",
        "coarseShipSystemsIntervalSeconds",
        "activeShipMaintenanceIntervalSeconds",
        "prewarmShipMaintenanceIntervalSeconds",
        "coarseShipMaintenanceIntervalSeconds",
        "ActivationCadenceState",
        "npcAiIntervalSeconds",
        "shipSystemsIntervalSeconds",
        "shipMaintenanceIntervalSeconds",
        "advanceNpcAiCadence",
        "advanceShipSystemsCadence",
        "advanceShipMaintenanceCadence",
        "SimulationMode::Scheduled",
        "std::numeric_limits<double>::infinity()",
    ):
        if token not in execution:
            violations.append(f"ActivationExecutionPolicy contract missing {token}")

sim_cpp = GAME_SIM_CPP.read_text(encoding="utf-8")
sim_h = GAME_SIM_H.read_text(encoding="utf-8")
cadence_lab = CADENCE_LAB.read_text(encoding="utf-8") if CADENCE_LAB.exists() else ""
scene_cpp = SCENE_CPP.read_text(encoding="utf-8")

integration_required = (
    "void GameSimulation::updateActivationShadow()",
    "updateActivationShadow();",
    "makeSpatialBounds(core.descriptor().logicalDimensions())",
    "makeSpatialBounds(descriptor.logicalDimensions())",
    "tr.motion.worldVelocityMps",
    "obj.linearVelocity",
    "id == m_playerId",
    "ActivationSpatialIndex spatialIndex",
    "spatialIndex.rebuild(anchors)",
    "spatialIndex.query(",
    "evaluateActivationShadowCandidates(",
    "m_activationPlanStates.try_emplace",
    "evaluateActivationPlan(",
    "upsertActivationClaim(",
    "clearActivationClaimsFromSource(",
)
for token in integration_required:
    if token not in sim_cpp:
        violations.append(f"GameSimulation activation integration missing {token}")

for token in (
    "m_activationPlannerDecisions",
    "m_activationPlanStates",
    "m_activationClaims",
    "m_activationHysteresisPolicy",
    "m_activationExecutionPolicy",
    "m_npcAiCadenceStates",
    "m_shipMotionControlCadenceStates",
    "m_shipMotionControlStepDecisions",
    "m_shipSystemsCadenceStates",
    "m_shipMaintenanceCadenceStates",
):
    if token not in sim_h:
        violations.append(f"GameSimulation activation state missing {token}")

# Stage 3E.1 proves the cadence consumer in the real GameSimulation loop with
# one ordinary AI-eligible diagnostic NPC. It must not be registered as a Hub
# Motion Lab actor, because those actors intentionally bypass NpcAiSystem.
for token in (
    "ActivationCadenceLabEnabled",
    "activationCadenceLabDemand",
    "prewarm-claim",
    "active-claim",
):
    if token not in cadence_lab:
        violations.append(f"activation cadence lab missing {token}")

if "spawnActivationCadenceLabNpc" not in scene_cpp:
    violations.append("activation cadence lab NPC spawn helper is missing")

build_scene_start = scene_cpp.find("EntityId buildGameScene(")
build_scene_end = scene_cpp.find("EntityId buildPromoScene", build_scene_start)
build_scene_region = (
    scene_cpp[build_scene_start:build_scene_end]
    if build_scene_start >= 0 and build_scene_end > build_scene_start
    else ""
)
if "spawnActivationCadenceLabNpc(sim, stationPos);" not in build_scene_region:
    violations.append("activation cadence lab NPC helper exists but is not called from buildGameScene")
if "ActivationCadenceLabEnabled" not in build_scene_region:
    violations.append("activation cadence lab spawn is not guarded by ActivationCadenceLabEnabled")
if "registerActivationCadenceLabShip" not in scene_cpp:
    violations.append("activation cadence lab NPC is not registered")
if "registerHubMotionLabShip" in scene_cpp[scene_cpp.find("spawnActivationCadenceLabNpc"):scene_cpp.find("EntityId buildGameScene")]:
    violations.append("activation cadence lab NPC must remain eligible for production NPC AI")
if "updateActivationCadenceLabClaim" not in sim_cpp:
    violations.append("activation cadence lab does not drive real gameplay claims")

# The live AI probe is a visible production NPC. Before the first authoritative
# tick it must be placed into the same canonical hub reference-frame contract
# as other ships, otherwise all-or-nothing accelerated diagnostics reject the
# entire fast-universe session.
for token in (
    "ActivationCadenceLabHubId",
    "ActivationCadenceLabLocalOffsetMeters",
):
    if token not in cadence_lab:
        violations.append(f"activation cadence lab spawn contract missing {token}")

prepare_start = sim_cpp.find("void GameSimulation::prepareReferenceFramesForSpawn()")
prepare_end = sim_cpp.find("void GameSimulation::", prepare_start + 1)
prepare_region = (
    sim_cpp[prepare_start:prepare_end]
    if prepare_start >= 0 and prepare_end > prepare_start
    else ""
)
for token in (
    "m_activationCadenceLabShipId",
    "ActivationCadenceLabHubId",
    "ActivationCadenceLabLocalOffsetMeters",
    "placeShipInReferenceFrame(",
):
    if token not in prepare_region:
        violations.append(
            f"activation cadence lab is not prepared for coherent accelerated timeline: {token}"
        )

# Stage 4A/4B materialized runtime-cost slices. Stage 4B may decimate the
# expensive attitude/control-force solver, but cheap kinematic propagation must
# remain fixed-step so authoritative transforms and full-presence snapshots stay
# continuous. Signals/perception remain outside activation scheduling.
def function_region(text: str, start_token: str, end_token: str) -> tuple[str, str]:
    start = text.find(start_token)
    end = text.find(end_token, start + len(start_token)) if start >= 0 else -1
    if start < 0 or end < 0:
        return "", text
    return text[start:end], text[:start] + text[end:]

log_region, without_log = function_region(
    sim_cpp,
    "void GameSimulation::debugLogActivationShadow(double dt)",
    "void GameSimulation::updateActivationShadow()",
)
planner_region, outside_planner_functions = function_region(
    without_log,
    "void GameSimulation::updateActivationShadow()",
    "void GameSimulation::updateHubMotionLabActors()",
)

ai_start = outside_planner_functions.find("// === 1. AI / controls / attitude ===")
ai_end = outside_planner_functions.find("ShipControlState aiControl", ai_start)
ai_region = (
    outside_planner_functions[ai_start:ai_end]
    if ai_start >= 0 and ai_end >= 0
    else ""
)

for token in (
    "activationExecutionMode(id)",
    "advanceNpcAiCadence",
    "ActivationNpcAiCadenceEnabled",
):
    if token not in ai_region:
        violations.append(f"Stage 4A NPC AI execution lane missing {token}")

maintenance_start = outside_planner_functions.find(
    "const auto maintenanceMode = activationExecutionMode(id);"
)
maintenance_end = outside_planner_functions.find(
    "if (npcRepairThinkTick", maintenance_start
)
maintenance_region = (
    outside_planner_functions[maintenance_start:maintenance_end]
    if maintenance_start >= 0 and maintenance_end >= 0
    else ""
)
for token in (
    "ActivationShipMaintenanceCadenceEnabled",
    "advanceShipMaintenanceCadence",
    "updateAssemblyRuntime(maintenanceDt)",
    "updateDetachedFragments(",
    "updateRepairJobs(",
):
    if token not in maintenance_region:
        violations.append(f"Stage 4A maintenance execution lane missing {token}")

motion_start = outside_planner_functions.find(
    "ActivationCadenceDecision\n                motionControlCadence"
)
motion_end = outside_planner_functions.find(
    "for (auto& [id, shipPtr] : m_ships)", motion_start + 1
)
motion_region = (
    outside_planner_functions[motion_start:motion_end]
    if motion_start >= 0 and motion_end >= 0
    else ""
)
for token in (
    "ActivationShipMotionControlCadenceEnabled",
    "advanceShipMotionControlCadence",
    "m_shipMotionControlStepDecisions[id]",
    "ship.updateMotionControl(",
    "ship.propagateMotionOrientation(fdt);",
    "ActivationShipSystemsCadenceEnabled",
    "advanceShipSystemsCadence",
    "ship.updateSystems(",
):
    if token not in motion_region:
        violations.append(f"Stage 4B ship motion/service lane missing {token}")

if motion_region:
    control_gate = motion_region.find("if (motionControlCadence.execute)")
    control_call = motion_region.find("ship.updateMotionControl(")
    propagate_call = motion_region.find("ship.propagateMotionOrientation(fdt);")
    systems_gate = motion_region.find("ActivationShipSystemsCadenceEnabled")
    if min(control_gate, control_call, propagate_call, systems_gate) < 0:
        violations.append("Stage 4B motion lane is incomplete")
    elif not (control_gate < control_call < propagate_call < systems_gate):
        violations.append(
            "Stage 4B must gate control evaluation, then propagate orientation unconditionally before systems cadence"
        )

input_start = outside_planner_functions.find(
    "const auto decisionIt =\n                m_shipMotionControlStepDecisions.find(id);"
)
input_end = outside_planner_functions.find(
    "/*\n        Accelerated trajectory diagnostics", input_start
)
input_region = (
    outside_planner_functions[input_start:input_end]
    if input_start >= 0 and input_end >= 0
    else ""
)
for token in (
    "m_shipMotionControlStepDecisions.find(id)",
    "!decisionIt->second.execute",
    "motionControlDt",
    "DynamicMotionSystem::applyLocalFrameInput(",
):
    if token not in input_region:
        violations.append(f"Stage 4B HubTactical control refresh missing {token}")

# Cheap translation propagation must stay fixed-step and outside the control
# cadence gate. Sparse replication is a later slice with explicit omission
# semantics; Stage 4B keeps every materialized entity present in snapshots.
for token in (
    "DynamicMotionSystem::updateLocalFrameMotion(",
    "shipPtr->core().desc().physics,\n                dt",
    "snapshot.ships.push_back(s);",
):
    if token not in outside_planner_functions:
        violations.append(f"Stage 4B fixed-step/full-presence contract missing {token}")

shared_physics = SHARED_PHYSICS_CPP.read_text(encoding="utf-8")
ship_controller = SHIP_CONTROLLER_CPP.read_text(encoding="utf-8")
for token in (
    "evaluateControl(transform, params, control, world, dt);",
    "propagateOrientation(transform, dt);",
    "controller.updateControlRates(dt, params, transform, world);",
):
    if token not in shared_physics:
        violations.append(f"SharedShipPhysics Stage 4B split missing {token}")
for token in (
    "void ShipController::updateControlRates(",
    "void ShipController::propagateOrientation(",
    "updateControlRates(dt, params, ship, world);",
    "propagateOrientation(dt, ship);",
):
    if token not in ship_controller:
        violations.append(f"ShipController Stage 4B split missing {token}")

if "m_activationPlannerDecisions" not in log_region:
    violations.append("activation planner diagnostic writer is missing")

if "m_activationPlannerDecisions" not in planner_region:
    violations.append("activation planner evaluator does not publish decisions")

if "simulation_activation_shadow.csv" not in log_region:
    violations.append("Stage 4B must keep the real-scene activation CSV")

for token in (
    "requested_mode",
    "planned_mode",
    "plan_transition",
    "transition_serial",
    "last_transition",
    "last_transition_time_s",
    "claim_kind",
    "broadphase_candidates",
    "broadphase_comparable",
    "broadphase_fallback",
    "npc_ai_eligible",
    "npc_ai_lab",
    "npc_ai_lab_phase",
    "npc_ai_interval_s",
    "npc_ai_time_since_think_s",
    "npc_ai_think_count",
    "npc_ai_skipped_frames",
    "npc_ai_last_think_time_s",
    "ship_motion_control_interval_s",
    "ship_motion_control_update_count",
    "ship_motion_control_skipped_frames",
    "ship_systems_interval_s",
    "ship_systems_update_count",
    "ship_maintenance_interval_s",
    "ship_maintenance_update_count",
):
    if token not in log_region:
        violations.append(f"Stage 4B CSV missing {token}")

# The exact evaluator must now receive only spatial candidates in production.
if "evaluateActivationShadow(" in planner_region:
    violations.append(
        "Stage 3D GameSimulation must not use the legacy all-pairs shadow evaluator"
    )

# Keep physical planner cadence conservative while the first materialized work lanes are gated.
if "m_activationShadowEvaluationAccumulatorSeconds >= 0.20" not in sim_cpp:
    violations.append("activation planner must remain rate-limited at 5 Hz")

if violations:
    print("Interaction activation architecture check failed:")
    for violation in violations:
        print(f"- {violation}")
    sys.exit(1)

print("[PASS] interaction activation architecture guard")
