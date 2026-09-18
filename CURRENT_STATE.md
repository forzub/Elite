# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** 11B-1 — authoritative NPC runtime ownership

## Progress

```text
[████████████████████░░░] 10 / 12 major stages closed

1–10 ACCEPTED
11   ACTIVE — live game/server/guidance + physics
     11A runtime control seam                  ACCEPTED
     11B-1 authoritative NPC runtime ownership ACTIVE
     11B-2 replicated guidance/debug truth     PENDING
12   end-to-end stress/debug + legacy retire   PENDING
```

## Latest accepted gate — 11A

Target-machine evidence on `d7c77d5868b3178be3c392f0a8fecad5b57e3b69`:

```text
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
navigation_runtime_control 1/1 PASS
navigation_trajectory/pilot 11/11 PASS
canonical EliteGame build PASS
canonical EliteServer build PASS
```

The candidate warning from unused `hasManualTranslationInput()` is removed in the active 11B branch.

## Accepted live control chain

```text
Navigation v2 acceleration intent
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> ShipControlState direct navigation demand
    -> SharedShipPhysics / ShipController / DynamicMotionSystem
    -> authoritative fixed-step motion
```

Navigation never writes authoritative position, velocity or angular rate directly.

## Active 11B-1

The old NPC steering authority is retired:

```text
REMOVED:
NpcAiSystem::computeControl()
yawInput = sin(position ...)
direct forwardInput / targetSpeedRate steering
```

`NpcAiSystem` now publishes only:

```text
NpcNavigationGoal
PilotSkillProfile
```

`NpcNavigationIntentController` converts one goal plus actual ship state into a nominal Navigation v2 acceleration intent. Initial goal modes are deliberately minimal:

```text
Hold
MaintainForwardCruise
```

These are ownership fixtures, not the final NPC behavior catalog.

## Per-NPC authoritative state

`GameSimulation` owns persistent per NPC:

```text
NavigationRuntimeControlBridge
last navigation execution time
latest NavigationRuntimeControlBridge::ExecutionSnapshot
```

Runtime:

```text
NpcAiSystem::computeGoal
 -> NpcNavigationIntentController::buildIntent
 -> per-NPC NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState
```

If the bridge fails, control fails closed and that NPC's bridge state is discarded. There is no fallback to the retired direct steering path.

Activation-decimated elapsed time is preserved exactly by advancing the bridge in bounded `<=0.25 s` pieces.

## Guidance/debug truth seam

`GameSimulation::npcNavigationExecutionSnapshots()` exposes the exact executed demand/revision used for control. 11B-2 will replicate this same truth for client guidance/debug rather than running a second planner.

## Next gate

```bash
python tests/architecture_contracts/check_navigation_live_runtime_control.py
python tests/architecture_contracts/check_navigation_live_npc_ownership.py
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh
bash build_mingw64.sh
```

After green: continue directly into 11B-2 replication/guidance.
