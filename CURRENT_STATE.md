# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** 11B-2 — replicated guidance/debug truth

## Progress

```text
[█████████████████████░░] 10 / 12 major stages closed

1–10 ACCEPTED
11   ACTIVE — live game/server/guidance + physics
     11A runtime control seam                  ACCEPTED
     11B-1 authoritative NPC runtime ownership ACCEPTED
     11B-2 replicated guidance/debug truth     ACTIVE
12   end-to-end stress/debug + legacy retire   PENDING
```

## Latest accepted live gate — 11B-1

Target-machine evidence on:

```text
fb83b8d80f29c6c5e4e12b8a2fca731ffea7b8e8
```

passed:

```text
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
NAVIGATION LIVE NPC OWNERSHIP CONTRACT: PASS
navigation_runtime_control 1/1 PASS
navigation_trajectory/pilot 11/11 PASS
EliteGame build PASS
EliteServer build PASS
```

11B-1 is accepted. The old direct NPC steering authority remains retired.

## Accepted live ownership chain

```text
NpcAiSystem
    goal + policy only
        |
        v
NpcNavigationIntentController
    nominal Navigation v2 acceleration intent
        |
        v
per-NPC NavigationRuntimeControlBridge
        |
        v
PilotSkillExecutor
        |
        v
ShipControlState direct navigation demand
        |
        v
SharedShipPhysics / ShipController / DynamicMotionSystem
        |
        v
authoritative motion
```

No fallback to `sin(position)` steering exists.

## Active 11B-2 candidate

Authority:

```text
src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md
```

New replicated DTO:

```text
game::simulation::NavigationExecutionSnapshot
```

Per ship it carries:

```text
valid
intentRevision
activeTargetRevision
ideal linear/angular acceleration demand
executed linear/angular acceleration demand
emergency / urgency
reactionBlocked
decisionSampled
queuedCommandApplied
pendingCommandCount
```

### Server publication

`GameSimulation::buildReplicationSnapshot()` fills the present alternative of `ShipSnapshot.navigationExecution` directly from the same per-NPC execution snapshot that produced live control.

No presentation-side reconstruction is involved.

### Wire contract

Canonical snapshot schema now includes a sparse field:

```text
ShipSnapshot.navigationExecution =
    variant<monostate, NavigationExecutionSnapshot>

absent execution -> one variant-tag byte
present execution -> tag + full execution payload
```

and the binary version is intentionally bumped:

```text
SimulationSnapshotWireSchemaVersion = 8
```

Cross-version decoding therefore fails closed.

### Client hydration

`ClientShipState.navigationExecution` resolves the sparse variant for both newly hydrated and already-known ships. `monostate` clears the local execution observation.

Sparse replication semantics remain unchanged:
- omitted ship -> retain;
- updated ship -> replace;
- explicit removal -> erase.

### Read-only navigation workspace

`ReplicatedNavigationExecutionState` indexes the truth by both:

```text
EntityId
ShipInstanceId
```

The stable `ShipInstanceId` lets a `NavigationAssetRef::Ship` resolve the current runtime entity.

`ClientNavigationWorkspace` exposes only:

```text
syncReplicatedNavigationExecution(...)
const replicatedNavigationExecution()
```

There is no mutable getter for planners. The mirror is rebuilt only when `lastSimulationMetadata().serverTick` advances, not every render frame.

### Guidance/debug presentation

`GuidanceCorridorHudPresentation` now exposes optional authoritative metadata for the selected route executor:

```text
authoritative entity id
intent revision
active pilot target revision
executed linear/angular demand
emergency
reaction blocked
```

This metadata is read-only. `LocalGuidancePlanner`, `DockingPathPlanner`, `ClientNavigationPlanningSnapshotFactory`, `TrajectoryPredictor`, and `TrajectorySafetyEvaluator` do not consume or rewrite it.

## 11B-2 gate

The new runtime suite contains:

```text
navigation_replication_truth
```

which proves:
- execution payload survives binary snapshot encode/decode;
- stable ship identity resolves current execution truth;
- guidance presentation exposes exact server execution metadata;
- no corridor is fabricated when only execution truth exists.

The existing wire data-plane contract was also extended and pins the absent sparse execution payload to exactly one variant-tag byte.

## Next after 11B-2 acceptance

1. close all of stage 11;
2. progress becomes 11/12;
3. start stage 12 end-to-end scenarios;
4. prove real obstacle/conflict/docking/post-impact behavior;
5. stress CPU/GPU/runtime cadence;
6. make Shift+F12/debug visualize the accepted live truth;
7. retire legacy route-wide navigation only after stable v2 ownership.
