# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Track:** Navigation v2 live integration  
**Stage:** 11B-1 — corrected roll-axis fixture rerun

## Progress

```text
[████████████████████░░░] 10 / 12 major stages closed

1–10 ACCEPTED
11A  ACCEPTED
11B-1 ACTIVE
11B-2 PENDING
12    PENDING
```

## 11A accepted

```text
d7c77d5868b3178be3c392f0a8fecad5b57e3b69
architecture PASS
runtime 1/1 PASS
trajectory/pilot 11/11 PASS
EliteGame + EliteServer canonical build PASS
```

## Active candidate

Production:

```text
src/game/simulation/NpcAiSystem.h/.cpp
src/game/navigation/NpcNavigationIntentController.h/.cpp
src/game/simulation/GameSimulation.h/.cpp
src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md
```

Tests/contracts:

```text
tests/navigation_runtime/NavigationRuntimeControlTests.cpp
tests/architecture_contracts/check_navigation_live_npc_ownership.py
```

### Ownership invariant

```text
NPC AI          -> goal/policy only
Navigation v2   -> acceleration intent
Pilot skill     -> execution timing/error
Ship control    -> capability-constrained demand
Physics         -> authoritative motion
```

`NpcAiSystem` must not emit `ShipControlState`, yaw/pitch/roll keys or throttle/RCS keys.

### Initial nominal goal controller

`MaintainForwardCruise` computes a desired relative velocity along actual ship forward and a bounded velocity-error acceleration demand.

`Hold` drives desired relative velocity to zero.

Angular demand damps actual pitch/yaw/roll rates but is expressed back in world axes and still passes through the accepted capability seam.

### Activation correctness

If activation wakes an NPC after more than the pilot executor's 0.25 s maximum step, the exact elapsed time is processed as multiple deterministic bounded substeps. Time is never discarded.

### Fail closed

Any bridge failure:

```text
zero ShipControlState
erase per-NPC bridge
erase latest execution snapshot
erase last execution time
```

No direct-steering fallback.

## First target-machine attempt — NOT ACCEPTED

Green evidence:

```text
check_navigation_live_runtime_control.py PASS
check_navigation_live_npc_ownership.py   PASS
navigation_trajectory                    11/11 PASS
EliteGame                                build PASS
```

Failures:

```text
navigation_runtime compile:
  missing GLM_ENABLE_EXPERIMENTAL in standalone harness

EliteServer link:
  missing NavigationRuntimeControlBridge.cpp
  missing NpcNavigationIntentController.cpp
  missing PilotSkillExecutor.cpp
```

Repairs are committed and the architecture checker now pins both wiring requirements.

## Second target-machine attempt — NOT ACCEPTED

The lightweight runtime boundary was necessary because the isolated test must not link all ShipCore/equipment/damage systems merely to exercise NPC intent conversion. `NpcNavigationIntentController` now accepts `NpcNavigationKinematicState` rather than full `Ship`.

## Third target-machine attempt — NOT ACCEPTED

Everything except one runtime assertion passed:

```text
live runtime architecture PASS
live NPC ownership architecture PASS
navigation trajectory/pilot 11/11 PASS
EliteGame build PASS
EliteServer build PASS

navigation_runtime 0/1
  NPC nominal intent must damp roll through world angular demand
```

Diagnosis: fixture error. With identity orientation, ship forward is `-Z`; positive roll damping is a `+Z` world vector whose projection onto forward is negative. Production controller already did this correctly.

The fixture now checks ship-axis projections, not raw world components.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_live_runtime_control.py
python tests/architecture_contracts/check_navigation_live_npc_ownership.py
bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh
bash build_mingw64.sh
```

Expected:

```text
11A architecture PASS
11B-1 ownership architecture PASS
navigation_runtime_control 1/1 PASS
trajectory/pilot 11/11 PASS
canonical EliteGame + EliteServer build PASS
```

## Next after green

11B-2:
- replicate accepted intent/execution revision;
- feed client guidance/debug from the same server truth;
- no client-side NPC replan;
- then close stage 11 and move to end-to-end stage 12.
