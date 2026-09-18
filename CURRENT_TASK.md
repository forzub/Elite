# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Track:** Navigation v2 end-to-end integration  
**Stage:** 12A-1 — live NavigationWorld runtime-planner gate

## Accepted baseline

Stage 11 remains accepted on target-machine evidence from:

```text
9650c44cca23741dae3f4acf2c9a96a4ab4c5713
```

with all live architecture contracts green, runtime 2/2, trajectory/pilot 11/11, wire-data-plane 1/1 and both canonical production builds passing.

Stage 12 authority:

```text
src/game/navigation/STAGE12_END_TO_END.md
```

## Candidate under test

The missing live composition seam is now implemented:

```text
NavigationSpace::queryCostedCorridor
 -> ordered portalCentersMapMeters
 -> NavigationRuntimePlanner
 -> LocalHorizon / LocalAvoidance
 -> NavigationRuntimeControlBridge::Intent
 -> PilotSkillExecutor
```

Production library:

```text
EliteNavigationWorldRuntime
```

is shared by `EliteGame` and `EliteServer`.

The candidate deliberately does **not** modify authoritative motion directly and is not yet connected to `GameSimulation`. First prove this layer on the target machine.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_space/run_mingw64.sh
bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh

bash build_mingw64.sh
```

Expected new evidence:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS

navigation_space:
    PASS
    ordered selected portal centers preserved

navigation_local:
    2/2 PASS

navigation_runtime:
    3/3 PASS
      navigation_runtime_control
      navigation_runtime_planner
      navigation_replication_truth

EliteGame build PASS
EliteServer build PASS
```

## What the new runtime-planner gate proves

- a selected static corridor exposes real ordered portal steering centers;
- a multi-region detour turns the first selected portal into a bounded live target;
- the same authored portal fails closed for an oversized hull;
- a moving crossing actor coming from `NavigationMap` changes the command to braking/hold;
- planner output crosses the already accepted `PilotSkillExecutor` bridge;
- client and server compile the same runtime-planning implementation;
- no planner code writes authoritative P/V/angular-rate state.

## Next after green

Wire the accepted `NavigationRuntimePlanner` into authoritative `GameSimulation` ownership, then add the deterministic station-adjacent proving-ground publisher using actual hit-volume/static-space geometry.

That next slice is where the physical obstacle field begins to matter to a real NPC/autopilot. Only after that gate is green do we expose the exact same proving ground in EliteGame and build the `Shift+F12` raw NavigationWorld visualization.
