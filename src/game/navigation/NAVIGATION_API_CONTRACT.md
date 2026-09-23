# Navigation API Contract

Status: active architecture contract.

This document defines the active Stage-1/Stage-2 navigation boundaries. The
purpose is to prevent calculation code from reaching across subsystem
boundaries for hidden state, configuration, vehicle data, world data, clocks or
I/O.

## Rule 1 — every calculation receives all required data through its API

A navigation calculation may use:
- its function arguments;
- immutable data contained by those arguments;
- implementation-only mathematical constants such as epsilon or exact
  polynomial constants.

A calculation may NOT:
- open scenario/config files;
- inspect process working directory or environment variables;
- read a concrete ship descriptor by name;
- reach into GameSimulation or another owner for missing state;
- query a global/singleton clock;
- invent vehicle capability from local literals;
- silently use another subsystem's defaults because its caller omitted policy.

I/O and concrete descriptor selection belong to orchestration boundaries only.

## Canonical input domains

### VehicleDynamicsProfile

Owner: application / vehicle system.

Contains vehicle facts only:
- ShipParams physics;
- body/collision dimensions;
- capability revision.

Concrete ship selection (for example Cobra) occurs outside the navigation core.
Navigation never assumes a particular ship type.

Derived vehicle views are produced only through:
- ShipDynamics.h;
- NavigationVehicleProfileAdapters.h;
- ManeuverCapabilityAdapters.h.

### ScenarioDefinition

Owner: scenario/world orchestration.

Immutable parsed snapshot:
- start state;
- terminal state;
- static/dynamic obstacle definitions;
- reference frame;
- world physics;
- goal/static/dynamic revisions.

File parsing ends at loadScenarioDefinition(). No Stage-1 or Stage-2
calculation reopens the scenario source.

### ScenarioRunSettings

Owner: caller/test/viewer.

Contains doctrine/policy only:
- requested control law;
- pilot execution profile;
- flight style;
- ScenarioNavigationPolicy;
- TrajectoryGenerationPolicy;
- explicit I/O policy;
- optional boundary-speed overrides.

### RetainedStaticRoute

Owner: Stage 1.

Immutable Stage-1 product:
- goal revision;
- static-world revision;
- vehicle-capability revision;
- planning speed;
- additional clearance;
- retained route points.

Stage 2 validates all provenance before use and never rebuilds the global route.

## Active calculation APIs

| Component | Input | Output | Purity |
| --- | --- | --- | --- |
| NominalRoutePlanner::plan | NominalRoutePlanner::Request | Plan | pure |
| GeometricPathPlanner::plan | explicit geometric request/policy | geometric path | pure |
| TrajectoryGenerator::generate | TrajectoryGenerationRequest | trajectory result | pure |
| RuckigTrajectorySolver | explicit Ruckig request | numeric trajectory/progress | pure |
| buildReferenceAttitudes (runtime adapter) | trajectory, initial basis/omega, terminal endpoint, law, vehicle params, navigation policy | attitude stream | pure |
| makeProgramPhase (runtime adapter) | trajectory/attitude slice, revisions, proof clearance, vehicle, policy | AcceptedManeuverProgram page | pure |
| ManeuverProgramSampler::sample | accepted program + universe time | sampled state/actuator command | pure |
| ManeuverTrackingController::track | program + reference + measured agent state + explicit gains | bounded control intent | pure |
| TrajectoryFollower::follow | accepted program + time + measured state + tracking policy | follower result | pure composition |
| ManeuverPhaseGate::evaluate | program + time + follower status + explicit gate policy | gate decision | pure |
| NavigationExecutionReplanPolicy::evaluate | policy + query | replan scope/reason | pure |
| OrdinaryPhysicalManeuverCompiler::compile | explicit Query | physical maneuver candidates | pure |
| NavigationRuntimeControlBridge | explicit pilot profile; step(time, dt, intent) | ShipControlState + execution snapshot | deterministic stateful |
| PilotSkillExecutor | explicit execution profile; step(time, dt, command) | executed command | deterministic stateful |
| DynamicMotionSystem | mutable motion state + ShipParams + frame + dt + command | updated physical motion state | deterministic stateful |

## Orchestration-only functions

These may perform composition/side effects because they are the declared
boundary:

- loadScenarioDefinition(): file -> ScenarioDefinition;
- makeScenarioVehicleParameters(): ShipDescriptor -> VehicleDynamicsProfile;
- calculateScenario(): Scenario + Settings + Vehicle -> RetainedStaticRoute;
- executeCalculatedRoute(): Scenario + Settings + RetainedStaticRoute +
  Vehicle -> execution result;
- diagnostic writers: only through ScenarioRuntimeIoPolicy.

No lower calculation component may include or call these orchestration APIs.

## Clock contract

One accepted physical maneuver has one monotonic universe-time basis.

Fixed-capacity AcceptedManeuverProgram objects are storage pages only. Page
selection is an indexing concern and must not create:
- a new clock;
- a new accepted-at time;
- a hidden phase capture;
- a reference-time freeze.

Any subsystem that needs time receives it explicitly.

## Vehicle / policy separation

Vehicle facts and doctrine are different inputs.

VehicleDynamicsProfile answers:
- what actuators exist;
- what force/angular authority exists;
- physical envelope/dimensions.

Policy answers:
- how much reserve to keep;
- tracking gains/tolerances;
- route-clearance doctrine;
- numerical/trajectory policy.

A policy may choose how to use hardware. It may not invent hardware.

## Failure semantics

A pure calculation returns an invalid/failure result. It does not reach across a
boundary for replacement data.

If an accepted maneuver becomes unreachable:
- Autopilot may apply bounded safety behavior;
- the accepted maneuver is invalidated;
- orchestration requests a new Planner product from measured state.

Follower does not silently become a planner.

## Automated guard

tests/architecture_contracts/check_navigation_api_purity.py is the mandatory
static architecture gate.

It checks:
- no ambient I/O/time/random/concrete-ship dependency in pure kernels;
- public request/policy seams exist;
- runtime calculation helpers use narrow explicit arguments;
- scenario parsing owns terminal-state mutation;
- terminal aliases do not leak outside helpers that explicitly receive them;
- old frozen-reference/reacquisition semantics do not return;
- vehicle/capability projections use canonical helpers.

## Normative architecture and current checker limitation

`NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md` is the normative end-to-end API
and ownership specification. This file remains the detailed purity contract for
calculation boundaries.

The current Python guard is a transitional lexical check, not a proof of
semantic purity. It is known to be sensitive to harmless source layout/DTO
names while failing to detect the active runtime's private NavLocal-to-System
copy. Migration M1 must make frame conversions canonical and replace brittle
exact-string conditions with semantic contract tests. Until then, a guard PASS
cannot substitute for non-identity-frame E2E evidence.

## 2026-09-23 — active runtime boundary candidate

The active diagnostic runtime now obeys the intended API shape:

- composition resolves `ScenarioFrameDefinition` once into a value-owned
  `KinematicFrame` snapshot and explicit epoch;
- `ExecutionVehicle` validates that snapshot but does not reconstruct or look
  up a frame;
- `NavigationFrameBoundary` is the only NavLocal/System conversion owner;
- relative angular velocity is treated as state, so conversion adds/subtracts
  frame angular velocity rather than rotating it as a free vector;
- trace and Follower inputs are converted observations, not direct reads of
  system-space orientation vectors.

The Python guard is now formatting-independent for these operations and the
product-chain fixture is mandatory. Target compilation/E2E is still required
before this candidate is promoted to an accepted contract implementation.
