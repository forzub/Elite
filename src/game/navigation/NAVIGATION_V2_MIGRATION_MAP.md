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



## B8/B9 target-machine acceptance and B10 candidate — 2026-09-19

Target-machine verified baseline:

```text
701881ddae861cd5593e425de91600e048bd417c
```

Verified on Windows 10 / MSYS2 MinGW64:
- `navigation_runtime`: 7/7 PASS;
- new `maneuver_program_sampler`: PASS;
- canonical `EliteGame` build: PASS;
- canonical `EliteServer` build: PASS.

The B8/B9 API is therefore accepted as a compiled/tested migration seam.

### B10 extracted

New block:
- `ManeuverTrackingController.h/.cpp`

Responsibility:
```text
ManeuverReferenceSample
+ actual vehicle state
+ tracking envelope / feedback reserve
    -> A_ff + bounded A_feedback
    -> alpha_ff + bounded alpha_feedback
```

Hard invariants:
- zero tracking error => feedback exactly zero;
- therefore executed ideal command equals accepted `A_ff/alpha_ff` exactly;
- feedback magnitude is clamped to the authority reserve carried by the accepted program;
- envelope violation is reported, not repaired by hidden path planning;
- no NavigationMap/NavigationSpace/GameSimulation/NavigationRuntimePlanner dependency;
- no world query, planner call or unbounded vector in B8/B9/B10 hot-path code.

`TrajectoryFollower` now has a second overload for the Navigation-v2 path:
```text
AcceptedManeuverProgram
 -> B9 ManeuverProgramSampler
 -> B10 ManeuverTrackingController
 -> NavigationLocalControlIntent
```

The old `AcceptedShortSegment` overload remains active for the live Stage-12 fixture until the next live migration slice.

New isolated regression:
- `maneuver_tracking_controller`.

Candidate baseline before documentation commits:
```text
66d89b93e3edf0817bb8d88b78e405180887cecc
```

Target-machine gate is pending.


## B10 first target-machine gate — failed; corrective candidate

The first B10 target-machine gate failed before tests.

Observed evidence:
- architecture contract: FAIL in 0.173 s;
- failure was a contract-check false positive: the checker rejected the words
  `NavigationMap / NavigationSpace` inside the documentation comment stating
  that AcceptedManeuverProgram does **not** own them;
- isolated navigation_runtime configure completed in about 0.2 s, then MinGW
  compile failed;
- production `build_mingw64.sh` failed on the same header error after
  17.209 s;
- no runtime tests executed.

Compile root cause:

```text
const Policy& policy = {}
```

inside the nested B10 API is not accepted by the target MinGW/g++ 15.2 build.
The same declaration contaminated every translation unit including
TrajectoryFollower/GameSimulation.

Correction:
- replace braced default-reference arguments with explicit 3-argument and
  4-argument overloads in ManeuverTrackingController;
- apply the same explicit-overload pattern to TrajectoryFollower;
- architecture contract now checks actual dependency syntax (includes,
  owner/type access and query calls) rather than bare words in comments;
- contract pins the MinGW-safe overload form;
- navigation_runtime timing script no longer exits before printing timings on
  failure. Configure/build/tests/total timing lines and the failing phase are
  now printed even when configure/build/test fails.

Corrective code/contract baseline before documentation commits:

```text
4a3d196c1574e91b747f05d194db7a35ad5c5517
```

B10 remains target-machine pending. B8/B9 acceptance remains at
`701881ddae861cd5593e425de91600e048bd417c`.


## B10 corrective rerun — production build green, isolated gate evidence pending

Fresh target-machine evidence from the corrective rerun:
- canonical `EliteGame`: BUILD PASS;
- canonical `EliteServer`: BUILD PASS;
- full `build_mingw64.sh` wall time: **44.989 s**.

The previously failing MinGW header/API defect is therefore closed in the
production compile/link path.

The supplied excerpt does not include:
- the architecture-contract result;
- the `navigation_runtime` 8/8 result;
- the local `git rev-parse HEAD` line.

Therefore B10 is still **not accepted** yet. Production build evidence is green,
but the isolated B9/B10 behavioral gate and architecture lock still require
fresh target-machine output.

Current repository documentation HEAD at the time of this evidence:
`2abd79a6181a322fe15425994ab771942e47bc26`.
Do not equate that with the tested checkout unless the target-machine
`git rev-parse HEAD` output is supplied.

No persistent log was required for this successful build.


## B10 accepted; B14 scheduler candidate — 2026-09-19

### B10 target-machine acceptance

Verified checkout:

```text
2abd79a6181a322fe15425994ab771942e47bc26
```

Target-machine evidence:
- Stage-12 architecture contract: PASS;
- `navigation_runtime`: 8/8 PASS;
- `maneuver_tracking_controller`: PASS;
- canonical `EliteGame`: BUILD PASS;
- canonical `EliteServer`: BUILD PASS;
- production `build_mingw64.sh`: 44.989 s real;
- architecture contract: 0.186 s real.

B10 is accepted as a clean execution block:
`AcceptedManeuverProgram -> ManeuverProgramSampler -> ManeuverTrackingController -> TrajectoryFollower`.

### B14 scheduler candidate

Candidate code/contract baseline before documentation commits:

```text
1ba23241d760b3a57c917189e333e4fbc0365ea4
```

New production block:
- `NavigationWorkScheduler.h/.cpp`;
- value-owned `NavigationPlannerJob`.

Scheduler responsibilities:
- urgent / normal / background queues;
- bounded dispatch slices (`maxJobs <= 128` + deterministic cost units);
- deterministic age promotion / starvation prevention;
- one pending actor job slot with duplicate suppression;
- monotonically increasing per-actor job revision;
- stale rejection before planner dispatch;
- in-flight ticket ownership and stale-result rejection before commit;
- O(1)-like actor-slot replacement with lazy queue tombstones;
- amortized compaction so physical queue storage cannot grow without bound;
- capacity-pressure cleanup of jobs invalidated by newer world/actor revisions.

The scheduler deliberately owns no NavigationMap, NavigationSpace,
NavigationRuntimePlanner, planner callback or wall clock.

New regression:
- `navigation_work_scheduler`.

Scale fixture:
- 5000 synthetic actors;
- fixed 128-job dispatch slices;
- deterministic FIFO inside equal effective priority;
- no lost/duplicated jobs;
- queue/in-flight/physical records drain to zero;
- enqueue and dispatch/complete timing printed diagnostically;
- no wall-clock threshold is used for correctness.

`tests/navigation_runtime/run_mingw64.sh` now also re-runs only the successful
scheduler test in verbose diagnostic mode so its internal timing line is visible
even though ordinary CTest hides stdout for passing tests.

This slice is not wired into GameSimulation yet. Live scheduling migration is a
later separately gated step.
