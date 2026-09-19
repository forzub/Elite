# Navigation v2 — current structure vs canonical blocks

**Status:** active migration audit
**Updated:** 2026-09-19 Europe/Kyiv
**Canonical target:** `NAVIGATION_V2_BLOCK_ARCHITECTURE.md`

## Summary

The current repository already contains many correct ingredients, but several are composed through `NavigationRuntimePlanner` and `GameSimulation` in ways that blur block ownership.

The migration goal is **not** a rewrite. Preserve accepted world/geometry/control components and extract clean APIs around them.

| Canonical block | Current implementation | Verdict | Required migration |
|---|---|---|---|
| B0 World Snapshot | NavigationMap publication, NavigationSpace publication, NavigationHitVolumeAdapter | KEEP | formalize shared revisioned snapshot contract later |
| B1 Shared Influence Builder | NavigationMap spatial hash + per-agent queryCorridor/querySphere | PARTIAL | retain spatial index; later publish scene-wide sparse InfluenceFrame |
| B2 Navigation Objective | NavigationRuntimePlanner::Goal, NpcNavigationGoal, task-specific target structs | FRAGMENTED | unify after execution seam is clean |
| B3 Topology Route | NavigationSpace corridor/portals | KEEP / INCOMPLETE | add coarse vehicle-feasibility metadata later |
| B4 Local Corridor | LocalHorizonPlanner + LocalAvoidancePlanner visibility fan | TRANSITIONAL | introduce RouteAlignedCorridorPlanner beside it; A/B before removal |
| B5 Maneuver Compiler | ordinary path missing; precision MovingPassage evaluator partly fills role | MISSING | create general bounded physical maneuver generation |
| B6 Continuous Prover | MovingPassageTrajectoryEvaluator + exact-static same-Hermite proof inside NavigationRuntimePlanner | GOOD PARTS / MIXED OWNER | extract general proof API |
| B7 Maneuver Decision | ManeuverDecisionController | EXISTS / BYPASSED | feed ordinary proved candidates through it |
| B8 Program Acceptance | AcceptedShortSegment + packing in GameSimulation | DEFECT / TRANSITIONAL | first migration target: AcceptedManeuverProgram |
| B9 Program Sampler | no clean block | MISSING | first implementation after B8 |
| B10 Tracking Controller | TrajectoryFollower | CLOSE / WRONG INPUT | migrate to sampled accepted program; bounded feedback only |
| B11 Safety Monitor/Reflex | GameSimulation exact execution checks + NavigationExecutionReplanPolicy; TacticalCollisionMonitor elsewhere | MIXED / PARTIAL | extract explicit short-horizon safety status/reflex contract |
| B12 Pilot Skill | PilotSkillExecutor / NavigationRuntimeControlBridge | KEEP | no route ownership |
| B13 Propulsion/Physics | DynamicMotionSystem / SharedShipPhysics | KEEP | remains final capability authority |
| B14 Work Scheduler | NavigationExecutionReplanPolicy decides scope, but planner invocation/queue is embedded in runtime | PARTIAL | add explicit dirty-agent planner job scheduler/queues |

## Primary structural defect

Current composition effectively does:

```text
NavigationRuntimePlanner
    topology
    + local geometric search
    + moving precision candidate construction
    + continuous precision proof
    + some maneuver semantics
    + immediate control intent

GameSimulation
    repacks planner result
    -> AcceptedShortSegment

TrajectoryFollower
    target velocity / fixed acceleration
    -> derives another control history
```

The planner boundary therefore does not preserve one exact proved maneuver program.

## Migration rule

Do not move working code merely to match directory aesthetics.

A block is considered separated only when:
1. it has one explicit responsibility;
2. its input/output types are value-owned or immutable/query-only;
3. downstream code consumes the product without re-deriving semantics;
4. it has an isolated deterministic test;
5. hot-path work is bounded;
6. the API does not require whole-world scans per agent.

## First migration slice

### B8 — AcceptedManeuverProgram

Create a fixed-capacity immutable execution product that can represent:
- reference position/velocity;
- feed-forward linear acceleration;
- reference body basis/orientation;
- reference angular velocity;
- feed-forward angular acceleration;
- time domain;
- terminal tolerances;
- tracking/feedback envelope;
- proof/world/capability revisions;
- maneuver provenance.

No dynamic allocation is required to execute one program.

### B9 — ManeuverProgramSampler

Create a separate sampler:
```text
AcceptedManeuverProgram + universeTime
    -> ManeuverReferenceSample
```

It performs no obstacle search and no feedback control.

### Compatibility strategy

Do not immediately delete `AcceptedShortSegment`.

Initial migration:
```text
new B8/B9 API exists + isolated tests
old live seam unchanged
```

Next slice:
```text
TrajectoryFollower gains new program/sample path
old segment path retained temporarily for live compatibility
```

Then GameSimulation packing migrates planner output to the new program.

Only after the full live target-machine gate is green may `AcceptedShortSegment` be retired.

## Scaling audit

### Safe for 100s/1000s already
- static NavigationSpace data shared;
- NavigationMap uses spatial hashing;
- PilotSkill/follower/physics work is local per actor;
- replan policy already forbids plan-every-frame.

### Must change for large populations
- duplicate per-agent dynamic candidate discovery should become shared sparse pair/influence generation;
- planner invocations need a dirty-agent queue with priorities and a per-slice budget;
- accepted execution products should be fixed-capacity;
- expensive exact proof runs only for local survivors/candidates;
- stale queued work must be rejected by revision before expensive processing.

### Planner scheduler target

```text
urgent queue:
    safety/reflex invalidation
    active maneuver capability loss

normal queue:
    completion/expiry
    local invalidation
    new objective

background/advisory queue:
    manual refresh
    far-NPC preplanning

each scheduler slice:
    discard stale revisions
    age/fairness adjustment
    consume bounded job/time budget
    publish value-owned AcceptedManeuverProgram
```

This queue is preferable to trying to plan all 1000 actors simultaneously.

