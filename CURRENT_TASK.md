# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Track:** Navigation v2 end-to-end integration  
**Stage:** 12A-2 — authoritative GameSimulation proving actor gate

## Accepted baseline

12A-1 is accepted on:

```text
af58b46cdb01ad254097383e5e4274c733c4e28e
```

with:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
navigation_runtime 3/3 PASS
EliteGame build PASS
EliteServer build PASS
```

## Candidate under test

One isolated authoritative NPC now uses the new production planner:

```text
existing NAV STRESS physical objects
 -> StaticObject::hitComponent
 -> NavigationHitVolumeAdapter
 -> hit-volume-derived NavigationMap broadphase
 -> NavigationRuntimePlanner
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState
 -> authoritative physics
```

The actor is `NAVIGATION V2 RUNTIME LAB` (instance 9030), pinned Active, while
ordinary NPCs keep their previous baseline path.

The current physical route deliberately crosses `NAV STRESS CUBE 08` if no
avoidance occurs.

## RUN NOW

The already accepted `navigation_space` and `navigation_local` suites were not
changed after their green run, so do not rerun them for this gate.

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh

bash build_mingw64.sh
```

Expected:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
 - authoritative GameSimulation isolates one Active stage-12 lab actor
   on real NAV STRESS hit volumes

navigation_runtime:
    3/3 PASS
      navigation_runtime_control
      navigation_runtime_planner
      navigation_replication_truth

EliteGame build PASS
EliteServer build PASS
```

The `navigation_runtime_planner` executable now additionally pins
authoritative `HitVolume -> NavigationObstacle OBB` conversion.

## What this gate does NOT prove yet

A green build/test proves the authoritative ownership/wiring and geometry
adapter contract. It does **not** yet prove that the live simulated ship reaches
the destination without contact.

After this gate is green, add live deterministic evidence for:

```text
CUBE 08 appears in the accepted local candidate/conflict set
the plan/intent deviates from the blocked straight line
authoritative ship motion follows the changed command
minimum separation stays positive
the ship continues toward / reaches the target
replicated execution truth matches that same command
```

Then replace the temporary single open `NavigationSpace` lab region with
static/precision topology generated from the exact hit-volume OBB product.
