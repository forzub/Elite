# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** 12A-2 — authoritative GameSimulation proving actor

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed

1–10 ACCEPTED
11   ACCEPTED — live game/server/guidance + physics
12   ACTIVE
     12A-1 live NavigationWorld composition seam       ACCEPTED
     12A-2 authoritative GameSimulation proving actor  CANDIDATE
```

## Accepted Stage 11 baseline

Stage 11 remains accepted on target-machine evidence from:

```text
9650c44cca23741dae3f4acf2c9a96a4ab4c5713
```

with the live architecture contracts, runtime 2/2, trajectory/pilot 11/11,
wire-data-plane 1/1, EliteGame and EliteServer all passing.

## Stage 12A-1 acceptance

Target-machine evidence on:

```text
af58b46cdb01ad254097383e5e4274c733c4e28e
```

passed:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
navigation_runtime 3/3 PASS
  navigation_runtime_control
  navigation_runtime_planner
  navigation_replication_truth
EliteGame build PASS
EliteServer build PASS
```

Accepted composition:

```text
NavigationSpace costed corridor
 -> ordered portalCentersMapMeters
 -> NavigationRuntimePlanner
 -> LocalHorizon / LocalAvoidance
 -> NavigationRuntimeControlBridge::Intent
 -> PilotSkillExecutor
```

## Stage 12A-2 candidate

The planner is now wired into authoritative `GameSimulation` for one isolated
diagnostic NPC only:

```text
NAVIGATION V2 RUNTIME LAB
 -> real GameSimulation NPC cadence/ownership
 -> NavigationRuntimePlanner
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState
 -> authoritative ship physics
```

Ordinary NPCs still use the accepted baseline `NpcNavigationIntentController`
path. The Stage-12 proving actor is pinned `Active` so activation decimation
cannot contaminate navigation evidence.

### Existing physical proving field

No duplicate test field was added. The scene already contains deterministic
physical `NAV STRESS CUBE/CYLINDER` objects created by
`spawnHubGuidanceTestModules()`.

The lab route is deliberately aligned through the existing
`NAV STRESS CUBE 08`:

```text
visual hub start = { 975, -1300, -8000 }
CUBE 08          = { 975, -1300, -4900 }
visual hub goal  = { 975, -1300,  1000 }
```

so a straight-line controller cannot pass the fixture unnoticed.

### Geometry truth candidate

`NavigationHitVolumeAdapter` converts authoritative local
`HitComponent/HitVolume` OBBs into world navigation OBBs and also supplies the
conservative entity radius used by the current NavigationMap broadphase.

Current 12A-2 runtime uses the **hit-volume-derived conservative radius** for the
stress objects in `NavigationMap`. Exact per-volume OBB conversion is already
implemented and contract-tested, but has **not yet** been wired into a generated
`NavigationSpace`/precision static topology. Do not claim exact narrow-gap
proof from this slice yet.

## Stage 12 authority

```text
src/game/navigation/STAGE12_END_TO_END.md
```

After 12A-2 compiles and passes its target-machine gate, the next evidence is
live behavior: prove the authoritative lab ship actually detects CUBE 08,
changes plan/command, avoids collision, and continues toward the goal. Then wire
exact hit-volume OBBs into static/precision topology rather than a second
presentation-only geometry path.
