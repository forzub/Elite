# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** 11A — live runtime control seam gate

## Progress

```text
[████████████████████░░░] 10 / 12 major stages closed

1–10 CLOSED / ACCEPTED
11   ACTIVE
     11A runtime control seam                 ACTIVE
     11B NPC/guidance authoritative ownership PENDING
12   stress/debug + legacy retirement         PENDING
```

## Newly closed — `PilotSkillExecutor`

Accepted on:

```text
b042321b65084950aa91784a5e02344430e5c2bc
NAVIGATION PILOT SKILL CONTRACT: PASS
11/11 navigation_trajectory CTest PASS
100% tests passed
```

## Active candidate — 11A

New/changed production files:

```text
src/game/navigation/NavigationRuntimeControlBridge.h/.cpp
src/game/navigation/DynamicMotionSystem.h/.cpp
src/game/shared/SharedShipPhysics.cpp
src/game/ship/ShipController.h/.cpp
src/game/ship/core/ShipControlState.h
src/game/simulation/GameSimulation.cpp
CMakeLists.txt
```

Contract/tests:

```text
src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md
tests/navigation_runtime/NavigationRuntimeControlTests.cpp
tests/navigation_runtime/CMakeLists.txt
tests/navigation_runtime/run_mingw64.sh
tests/architecture_contracts/check_navigation_live_runtime_control.py
```

### Required runtime chain

```text
ideal accepted acceleration intent
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> direct ShipControlState navigation demand
    -> real capability clamps
    -> authoritative fixed-step motion
```

No fake keyboard conversion is allowed.

### Capability truth

Angular demand:

```text
map-space vector
    -> ship pitch/yaw/roll axes
    -> existing angular acceleration/rate/load envelope
```

Linear demand:

```text
positive forward
    -> real main engine

remaining/reverse/lateral/vertical
    -> real manoeuvre thrusters
```

Reverse demand may not invent a reverse main engine.

### Manual ownership

Material manual attitude input overrides navigation angular demand.

Any material manual translation/cruise/jump/alignment command keeps the established input path and suppresses direct navigation linear demand for that sample.

### Runtime snapshot

`NavigationRuntimeControlBridge::ExecutionSnapshot` carries both ideal and executed demand plus the intent/active-target revisions and skill timing diagnostics. It is the future guidance/debug source for stage 11B.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_live_runtime_control.py
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh
bash build_mingw64.sh
```

Expected:

```text
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
navigation_runtime_control 1/1 PASS
navigation trajectory/pilot 11/11 PASS
canonical EliteGame + EliteServer build PASS
```

## Next after green

Immediately start 11B:

1. retire current `NpcAiSystem::sin(position)` steering as authority;
2. make NPC AI produce goals/policy while Navigation v2 owns maneuver intent;
3. maintain per-NPC `NavigationRuntimeControlBridge` state in authoritative simulation;
4. publish the same accepted intent/execution revision to guidance/debug;
5. prove no second guidance planner and no direct physics mutation.
