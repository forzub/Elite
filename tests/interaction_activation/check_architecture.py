#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
ACTIVATION = ROOT / "src/game/simulation/activation"
GAME_SIM_CPP = ROOT / "src/game/simulation/GameSimulation.cpp"
GAME_SIM_H = ROOT / "src/game/simulation/GameSimulation.h"
CADENCE_LAB = ROOT / "src/game/diagnostics/ActivationCadenceLab.h"
SCENE_CPP = ROOT / "src/game/scene/GameSceneSetup.cpp"

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
        "ActivationCadenceState",
        "npcAiIntervalSeconds",
        "advanceNpcAiCadence",
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

build_scene_start = scene_cpp.find("EntityId buildGameScene(GameSimulation& sim)")
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

# Stage 3E allows exactly one production consumer of the activation plan:
# NPC tactical AI think cadence. Physics/control application/HubTactical/
# signals/snapshots must remain full-rate until coarse/scheduled motion exists.
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
outside_ai_region = (
    outside_planner_functions[:ai_start] + outside_planner_functions[ai_end:]
    if ai_region
    else outside_planner_functions
)

if "m_activationPlannerDecisions" not in ai_region:
    violations.append("Stage 3E NPC AI cadence does not consume the activation plan")
if "advanceNpcAiCadence" not in ai_region:
    violations.append("Stage 3E NPC AI cadence does not use the execution policy")
if "ActivationNpcAiCadenceEnabled" not in ai_region:
    violations.append("Stage 3E NPC AI cadence lacks the rollback feature flag")
if "m_activationPlannerDecisions" in outside_ai_region:
    violations.append(
        "Stage 3E activation plan leaked beyond NPC AI cadence into another production loop"
    )

if "m_activationPlannerDecisions" not in log_region:
    violations.append("activation planner diagnostic writer is missing")

if "m_activationPlannerDecisions" not in planner_region:
    violations.append("activation planner evaluator does not publish decisions")

if "simulation_activation_shadow.csv" not in log_region:
    violations.append("Stage 3D must keep the temporary real-scene activation CSV")

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
):
    if token not in log_region:
        violations.append(f"Stage 3D CSV missing {token}")

# The exact evaluator must now receive only spatial candidates in production.
if "evaluateActivationShadow(" in planner_region:
    violations.append(
        "Stage 3D GameSimulation must not use the legacy all-pairs shadow evaluator"
    )

# Keep physical planner cadence conservative while only NPC AI think cadence is gated.
if "m_activationShadowEvaluationAccumulatorSeconds >= 0.20" not in sim_cpp:
    violations.append("activation planner must remain rate-limited at 5 Hz")

if violations:
    print("Interaction activation architecture check failed:")
    for violation in violations:
        print(f"- {violation}")
    sys.exit(1)

print("[PASS] interaction activation architecture guard")
