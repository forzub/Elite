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
| B5 Maneuver Compiler | OrdinaryPhysicalManeuverCompiler (Newtonian first slice) + precision MovingPassage precursor | ISOLATED GREEN / REVISED CANDIDATE | rerun after main-engine-option ownership correction, then feed exact candidate to B6 |
| B6 Continuous Prover | MovingPassageTrajectoryEvaluator + exact-static same-Hermite proof inside NavigationRuntimePlanner | GOOD PARTS / MIXED OWNER | extract general proof API |
| B7 Maneuver Decision | ManeuverDecisionController | EXISTS / BYPASSED | feed ordinary proved candidates through it |
| B8 Program Acceptance | AcceptedShortSegment + packing in GameSimulation | DEFECT / TRANSITIONAL | first migration target: AcceptedManeuverProgram |
| B9 Program Sampler | no clean block | MISSING | first implementation after B8 |
| B10 Tracking Controller | TrajectoryFollower | CLOSE / WRONG INPUT | migrate to sampled accepted program; bounded feedback only |
| B11 Safety Monitor/Reflex | GameSimulation exact execution checks + NavigationExecutionReplanPolicy; TacticalCollisionMonitor elsewhere | MIXED / PARTIAL | extract explicit short-horizon safety status/reflex contract |
| B12 Pilot Skill | PilotSkillExecutor / NavigationRuntimeControlBridge | KEEP | no route ownership |
| B13 Propulsion/Physics | DynamicMotionSystem / SharedShipPhysics | KEEP | remains final capability authority |
| B14 Work Scheduler | NavigationWorkScheduler + NavigationExecutionReplanPolicy; live lab integration candidate in GameSimulation | ISOLATED ACCEPTED / LIVE CANDIDATE | prove live enqueue -> dispatch -> complete -> commit, then generalize beyond lab |

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


## B14 isolated acceptance evidence; live scheduler integration candidate — 2026-09-19

### Isolated B14 gate

Target-machine evidence supplied:
- Stage-12 architecture contract: PASS;
- `navigation_runtime`: 9/9 PASS;
- `navigation_work_scheduler`: PASS;
- 5000 actors:
  - enqueue: 1518 us;
  - dispatch + complete: 1258 us;
  - total: 2776 us;
- canonical EliteGame: BUILD PASS;
- canonical EliteServer: BUILD PASS;
- production build: 23.001 s real;
- architecture contract: 0.204 s real.

The exact `git rev-parse HEAD` line was not included in the supplied B14 gate
excerpt, so no exact tested hash is invented here. The previously named exact
B10 baseline remains `2abd79a6181a322fe15425994ab771942e47bc26`;
the B14 gate is accepted by its observed code/test evidence.

### Live B14 integration candidate

GameSimulation's isolated Stage-12 lab actor now routes dirty replans through:

```text
NavigationExecutionReplanPolicy
    -> NavigationPlannerJob
    -> NavigationWorkScheduler::enqueue
    -> bounded dispatchSlice
    -> existing NavigationRuntimePlanner::plan
    -> NavigationWorkScheduler::complete
    -> commit only on CompletedCurrent
    -> existing AcceptedShortSegment packing
```

No planner geometry, local-avoidance policy or ACCEPT representation changes in
this slice.

Live scheduler revision inputs:
- dynamic navigation source/world revision;
- objective revision;
- a monotonic capability revision generated from actual current linear/angular
  authority;
- per-actor planner job revision.

Runtime diagnostics now record:
- accepted/replaced/duplicate/stale enqueues;
- dispatch count;
- current/stale completions;
- maximum pending/in-flight depth;
- dispatch total/max microseconds;
- planner total/max microseconds.

The headless navigation self-test now requires:
- scheduler dispatch count > 0;
- `schedulerDispatchCount == planCount`;
- every dispatched planner result completes current in the synchronous lab;
- no stale completion;
- max pending == 1 and max in-flight == 1 for the single lab actor;
- every authoritative planner commit occurs only after B14 completion.

Current live-integration code/contract candidate before documentation commits:
`c7479ef471e572ceca8e839d0360cd56137ec3c1`.

Target-machine live gate is pending.


## B5 ordinary physical compiler candidate — 2026-09-19

The failed live B14 gate is blocked by the pre-existing ordinary visibility
handoff, not by scheduler ownership. The former premature portal-alignment bug
is already closed (`first_bypass_align_forward=0`), and PilotSkill revision
audit shows segment target revisions do not restart reaction delay.

New clean B5 block:
- `OrdinaryPhysicalManeuverCompiler.h/.cpp`;
- fixed-capacity `OrdinaryPhysicalManeuverCandidate`;
- every candidate carries `requiresContinuousProof=true`.

First slice supports Newtonian only:
- `Coast`;
- body-axis-feasible `Trim`;
- `LeadRotateMainBurn` for material delta-v outside RCS/body-axis feed-forward
  authority.

The compiler consumes:
- current P/V/body basis/angular velocity;
- forward/reverse/lateral/vertical acceleration authority;
- angular acceleration/speed authority;
- B10 linear/angular feedback reserve;
- pilot control-response reserve;
- bounded program horizon.

It does not query NavigationMap/NavigationSpace, call a planner, read a clock or
allocate an unbounded vector.

`LeadRotateMainBurn` uses a bounded quintic attitude profile. Rotation duration
is chosen conservatively from angular speed and angular acceleration limits plus
control-response reserve. Main-engine feed-forward begins only after the lead
rotation and remains aligned with the resulting forward axis. The primitive is
a short receding-horizon candidate, not a whole-route solve.

Assisted deliberately returns `UnsupportedControlLaw` in this first slice.

New isolated regression:
- `ordinary_physical_maneuver_compiler`.

It includes the live ~75-degree failure class and proves that a ~41 m/s^2
lateral desired acceleration cannot be labeled direct-feasible when RCS is only
~2 m/s^2. It must compile to lead-rotate/main-burn or fail closed.

A diagnostic batch compiles 10,000 dirty-actor cases and prints total/per-call
timing without using wall-clock time as a correctness threshold.

Code/contract baseline before documentation commits:

```text
233d4023e81d5d466043a08c67acd8d49846c4b5
```

B5 isolated target-machine gate is pending. No live integration is performed in
this B5 slice; B6 continuous proof remains mandatory before B5 candidates may
cross B8 ACCEPT.


## B5 first target-machine gate green; main-engine option ownership correction

Fresh target-machine evidence supplied:
- architecture contract PASS: 0.191 s;
- navigation_runtime: 10/10 PASS;
- ordinary_physical_maneuver_compiler: PASS;
- 10,000 B5 compiles: 30,495 us total = 3,049.5 ns/compile;
- navigation_work_scheduler 5000 actors: 2,801 us total;
- EliteGame / EliteServer BUILD PASS;
- production build: 23.544 s.

No exact `git rev-parse HEAD` line was included in this supplied excerpt, so
the tested B5 gate is recorded by evidence without inventing a hash.

Architecture correction after the green gate:
the first compiler only emitted LeadRotateMainBurn when direct body-axis/RCS
feed-forward was already infeasible. That is safe but too narrow for the
accepted Newtonian control model.

Correct ownership is:
- B5 generates both RCS Trim and main-engine LeadRotateMainBurn when both are
  physically possible;
- B6 proves both;
- B7 planner-side decision chooses the maneuver according to doctrine/objective;
- B9/B10 follower never substitutes engines or invents a hull reorientation.

For a main-engine-dominant Newtonian craft, main engine remains the normal
translation authority for material delta-v; RCS is precision/trim authority.

Revised unverified candidate adds:
- `mainEngineCandidateAvailable`;
- main-engine candidate generation even when Trim is feasible;
- regression
  `testRcsFeasibleLateralChangeStillExposesMainEngineOption`;
- architecture lock that B5 exposes alternatives while B7 retains selection
  ownership.

This correction must receive a short isolated rerun before B6 work begins.


## Revised B5 accepted; maneuver program execution lab candidate — 2026-09-19

### Revised B5 target-machine acceptance

Fresh supplied evidence after the main-engine-option correction:
- architecture contract PASS: 0.222 s in the full pasted gate;
- navigation_runtime: 10/10 PASS;
- ordinary_physical_maneuver_compiler PASS;
- explicit regression
  `RCS-feasible delta-v still exposes a main-engine alternative for B7`: PASS;
- 10,000 B5 compiles: 30,824 us total = 3,082.4 ns/compile;
- navigation_work_scheduler 5000 actors: 2,670 us total;
- EliteGame / EliteServer BUILD PASS;
- production build: 23.861 s.

The supplied paste contains no `git rev-parse HEAD` line, so no exact tested
hash is invented. Revised B5 semantics are accepted by observed gate evidence.

### New execution laboratory

Before B6 integration, add an execution-only diagnostic that removes route
search/world geometry from the equation.

New fixture:
`tests/navigation_runtime/ManeuverProgramExecutionLabTests.cpp`.

Execution chain under test:

```text
pre-authored AcceptedManeuverProgram
 -> B9 sampler
 -> B10 bounded tracker
 -> PilotSkillExecutor
 -> ShipControlState
 -> SharedShipPhysics attitude
 -> DynamicMotionSystem propulsion/translation
 -> measured physical trajectory
```

Scenarios:
1. 100 m straight stop-to-stop;
2. 100 m first leg -> stop -> 90-degree yaw -> 100 m second leg;
3. same two-leg route monitored against a 5 m half-width polyline corridor.

The straight reference uses a smooth quintic stop-to-stop trajectory sized so
peak reverse/braking demand remains inside the real ~2 m/s2 maneuver authority.
The 90-degree route deliberately stops at the corner before rotating; a later
fixture may test a continuous non-stop corner after baseline execution is known.

Metrics printed per scenario:
- final position error;
- final residual speed;
- maximum geometric cross-track;
- maximum endpoint overshoot;
- corner capture error;
- maximum corridor violation;
- simulated completion time;
- arrival classification ON_TARGET / OVERSHOOT / UNDERSHOOT_OR_UNSETTLED.

The first laboratory profile uses expert PilotSkill with zero reaction delay and
zero command latency. This isolates program-following/physics accuracy from
human-skill latency. A later pass can repeat the same geometry under realistic
pilot profiles.

Initial acceptance limits:
- straight: <=3 m final position, <=1 m/s final speed, <=1 m cross-track,
  <=3 m overshoot;
- right-angle: <=3 m corner miss, <=5 m final miss, <=1.5 m/s final speed,
  remain inside 5 m corridor.

The lab is wired into navigation_runtime and is intentionally allowed to fail:
the first target-machine run is meant to measure the actual execution behavior,
not to hide it.


## 4-leg 3D corridor pilot/control-law matrix candidate — 2026-09-19

Previous simple execution lab is target-machine accepted on exact checkout:

```text
b5f558b18c8ef8e1b5d9562df36b34cb621c648f
```

Observed:
- architecture contract PASS;
- navigation_runtime 11/11 PASS;
- straight 100 m:
  - final position error 0.0285925 m;
  - final speed 0.102262 m/s;
  - zero cross-track / zero overshoot;
- right-angle 100 m + 100 m:
  - final position error 0.0285925 m;
  - final speed 0.102262 m/s;
  - maximum route cross-track 0.0470123 m;
  - corner error 0.0285925 m;
  - zero 5 m corridor violation;
- maneuver execution lab PASS;
- B5 compiler PASS;
- scheduler PASS.

New code/contract candidate before documentation commits:

```text
d0f8b07787339074e225cd02cfe96c93bad0446a
```

New target:
`maneuver_corridor_matrix`.

Route has four stop-to-stop 3D translation legs through five points:

```text
P0 (0,   0,   0)
P1 (0,   0, -120)
P2 (85, 35, -190)
P3 (25,100, -265)
P4 (120,55, -340)
```

Three bounded quaternion/axis-angle attitude transitions connect the legs.
The same route is executed in:
- Newtonian;
- Assisted.

PilotSkill matrix:
- expert: zero reaction/latency, high command bandwidth, no noise;
- competent: exact current production NpcAiSystem baseline
  (0.12 s reaction, 12 Hz decision, 0.06 s latency, 2 Hz response,
  current deterministic command noise/slew);
- rookie: deterministic degraded profile with 0.30 s reaction, 6 Hz decision,
  0.12 s latency, slower response/slew and larger deterministic command noise.

Total rows: 2 laws x 3 pilots = 6.

The corridor is a 5 m half-width tube around the 3D route polyline.

Every row reports:
- valid/completed;
- translation legs completed /4;
- rotations completed /3;
- final position error;
- final residual speed;
- maximum route cross-track;
- maximum active-leg cross-track;
- maximum corridor violation;
- maximum waypoint error;
- maximum overshoot;
- maximum forward-angle tracking error;
- tracking-envelope-exceeded tick count;
- total simulated seconds.

First target-machine pass is deliberately asymmetric:
- expert rows are strict and must complete inside the 5 m corridor with
  <=1 m final error and <=0.6 m/s residual speed;
- competent/rookie rows are diagnostic on the first pass so their actual
  deterministic envelopes are measured before gameplay acceptance limits are
  invented.

This data is intended to feed B6 execution-reserve/clearance policy: a lower
skill pilot may require more physical corridor clearance even when the accepted
centerline program is identical.


## First 3D corridor matrix evidence; explicit law-stress added — 2026-09-19

Fresh target-machine evidence supplied:
- architecture contract PASS;
- navigation_runtime 12/12 PASS;
- maneuver_corridor_matrix PASS;
- maneuver_program_execution_lab PASS;
- B5 compiler PASS;
- scheduler PASS.

First 4-leg 3D corridor matrix:
- expert/Newtonian and expert/Assisted both complete 4/4 legs + 3/3 rotations,
  final position error 0.014967 m, residual speed 0.096233 m/s, zero 5 m
  corridor violation;
- competent/Newtonian and competent/Assisted both complete 4/4 + 3/3,
  final position error 0.111386 m, residual speed 0.078158 m/s, zero corridor
  violation;
- rookie/Newtonian and rookie/Assisted both complete only leg 1/4 and no
  rotation, with zero corridor violation but maximum forward-angle error
  19.115652 degrees.

The first calm matrix produced identical Newtonian/Assisted metrics because the
trajectory stays far below the controlled-speed boundary where their propulsion
integration differs. Therefore equality here is expected but is not sufficient
evidence that both laws were materially exercised.

A dedicated law-stress regression is now added to the same target:
- lateral RCS acceleration runs against a deliberately reachable 10 m/s
  controlled-speed envelope;
- Newtonian must accumulate RCS delta-v materially beyond the envelope;
- Assisted/aircraft-like must remain at the envelope;
- the test fails if the two modes remain physically indistinguishable.

Current code/contract candidate before documentation commits:

```text
88ad3a8921239cf2865c32c0c8711b7514094aa1
```

The matrix's obsolete unused finite(vec3) warning was removed.

Full planner-side Assisted B5 compilation is still not implemented; this
law-stress validates execution physics/control-law selection only.


## 2026-09-19 correction: point-center corridor is not hull-clearance proof

The previous `maneuver_corridor_matrix` is now classified as a **center-of-mass
tracking diagnostic only**. It is NOT accepted as proof that a real ship fits
inside the reported corridor.

Root cause:
- corridor distance was measured from `localPositionMeters` only;
- Cobra hull dimensions were not applied to corridor occupancy;
- the same stop-to-stop Trim translation reference was used for Newtonian and
  Assisted rows;
- therefore Newtonian braking did not require the physically expected 180 deg
  flip before aft-main-engine braking.

This made the calm Newtonian/Assisted corridor rows numerically identical even
though the separate law-stress correctly proved that the low-level laws differ.

Canonical Cobra Mk1 logical hull from `EliteCobraMk1Descriptor`:
- width = 26.0 m;
- height = 5.0 m;
- length = 22.2 m;
- body half-extents = (right 13.0, up 2.5, forward 11.1) m.

A 5 m half-width corridor can therefore never contain this hull even when the
centerline is perfect. Any previous "zero 5 m corridor violation" statement
refers only to center-of-mass tracking and must not be used as hull-clearance
evidence.

Required corrected physical test:
- explicit rigid-body hull envelope;
- explicit aft main / fore(reverse) main / RCS / angular-vectoring authority;
- Newtonian: aft main only for longitudinal main thrust, RCS for small
  translation, angular authority for attitude; braking requires flip and
  aft-main burn;
- Assisted/aircraft-like: forward and reverse longitudinal main authority,
  RCS for small translation/stabilization, angular authority for attitude;
- record P, V, complete body attitude, angular velocity, flip angle, actuator
  usage and hull corridor occupancy;
- corridor occupancy must be measured on the oriented hull, not only its
  center.

The previously supplied target-machine law-stress remains useful low-level
evidence:
- Newtonian final speed 15.960220 m/s;
- Assisted final speed 10.000000 m/s;
so the execution laws themselves are distinct. The invalid part is the
point-center corridor interpretation, not that low-level law distinction.


## 2026-09-19 rigid-body corridor / physical actuator candidate

The earlier center-point corridor interpretation is superseded. The old
`maneuver_corridor_matrix` remains useful only as a center-of-mass tracking /
PilotSkill diagnostic.

New code/contract candidate before documentation commits:

```text
753eae5dcf1d7aae8eb05893ba75e16896a52b91
```

Production control allocation changed:
- Newtonian navigation demand:
  - aft/forward main thrust only;
  - reverse demand cannot invent fore/nose main thrust;
  - reverse/lateral/vertical residual is bounded by manoeuvre/RCS authority;
- Assisted/aircraft-like navigation demand:
  - symmetric longitudinal aft/fore main thrust;
  - lateral/vertical residual remains bounded manoeuvre/RCS;
  - no omnidirectional main engine.

Assisted manual/stabilized local control was aligned to the same actuator model:
longitudinal error -> aft/fore main, lateral/vertical stabilization -> RCS.

New runtime control assertions pin:
- Newtonian reverse cannot use fore main;
- Assisted reverse can use fore longitudinal main;
- Assisted lateral demand cannot use main propulsion.

New target:
`maneuver_rigid_body_corridor`.

Rigid vehicle model uses canonical Cobra Mk1 logical dimensions:
- width 26.0 m;
- height 5.0 m;
- length 22.2 m;
- OBB half-extents (13.0, 2.5, 11.1) m.

Explicit actuator model:
- aft main: 7.5 g longitudinal authority;
- Assisted fore main: 7.5 g longitudinal reverse authority;
- RCS: 2.0 m/s2;
- angular/vectoring authority: 3.0 rad/s2 with Cobra rate limits.

The same 200 m center-of-mass stop trajectory is authored differently:
- Newtonian: main acceleration -> coast while yaw-flipping ~180 deg ->
  aft-main braking burn;
- Assisted: main acceleration -> nose-forward coast -> fore-main reverse
  braking with no hull flip.

The test measures every tick:
- center P/V;
- full body attitude;
- angular speed;
- maximum flip angle;
- aft-main / fore-main / RCS use during braking;
- oriented hull-corner corridor envelope.

The corridor centerline is extended past maneuver endpoints so width measures
transverse hull occupancy rather than finite-segment end-cap artifacts.

Strict expert invariants:
- Newtonian must complete a near-180 deg flip;
- Assisted must stay nose-forward;
- Newtonian brake must use aft main and zero fore main;
- Assisted brake must use fore main;
- aligned Assisted Cobra must fit a 14 m half-width corridor;
- Newtonian flip must require materially more than 14 m;
- 18.5 m half-width must contain the expert Newtonian flip envelope;
- both expert runs must stop within 1 m and <=0.5 m/s.

Competent and rookie rows are diagnostic on the first run.

Navigation-runtime expected test count is now 13.

The latest supplied 12/12 target-machine run did not include a
`git rev-parse HEAD` line, so no exact hash is assigned to that run.


## 2026-09-20 rigid-body baseline accepted; corner-family matrix next

Fresh target-machine evidence:
- architecture contract PASS;
- navigation_runtime 13/13 PASS;
- maneuver_rigid_body_corridor PASS;
- production build PASS, 84.197 s.

Rigid Cobra model:
- width 26.0 m, height 5.0 m, length 22.2 m;
- aft main 73.549875 m/s2;
- Assisted fore main 73.549875 m/s2;
- RCS 2.0 m/s2;
- angular/vectoring authority 3.0 rad/s2.

Expert:
- Newtonian: completed, final error 0.346002 m, final speed 0.399834 m/s,
  final forward 179.996705 deg from route, max hull half-width 17.275892 m,
  max flip 179.999972 deg, aft-main brake 8.325947 m/s2, fore main 0;
- Assisted: completed, same final P/V error, final forward 0 deg, max hull
  half-width 13.238202 m, no flip, fore-main brake 8.326298 m/s2.

Competent:
- both laws completed;
- Newtonian hull half-width 17.299164 m, final forward 179.465370 deg;
- Assisted hull half-width 13.297102 m, final forward 0.019004 deg.

Rookie:
- neither law completed inside the current fixed program horizon;
- Newtonian drifted to 33.553144 m center error and required 21.559101 m hull
  half-width;
- Assisted ended 18.896949 m from terminal target and required 13.467892 m
  hull half-width.

This accepts the rigid-body/actuator baseline and confirms the expected wider
Newtonian flip envelope.

Next task: compare three explicit corner-passage families on the same corridor:
StopTurnGo, RadiusTurn and DriftTurn, under Newtonian and Assisted with identical
pilot profiles. Total corridor time must be measured from a common entry gate to
the same final terminal gate.
