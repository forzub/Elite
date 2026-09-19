# Navigation v2 purity and isolation contract

## Purpose

Navigation calculations must be testable without a running world, renderer, network session, wall clock, random source, or mutable singleton. The default rule is:

> **calculation = value-in -> value-out**

State is admitted only where the problem is inherently temporal or where an index/cache owns an explicitly published snapshot.

This contract distinguishes three categories. They are not synonyms.

## 1. Strict pure

A strict-pure component:

- receives every input explicitly;
- mutates no repository/runtime state;
- performs no I/O;
- reads no wall clock;
- consumes no random source;
- retains no hidden mutable cache;
- returns the same result for the same input values.

Current Navigation-v2 strict-pure calculation core:

| Component | Contract |
| --- | --- |
| PhysicalManeuverHorizon | speed/capability/response reserve -> physical distance/time horizon |
| NavigationExecutionSafetyProbeBuilder | current kinematics -> stopping/ideal/executed probe geometry |
| LocalHorizonPlanner | agent + target + already reduced dynamic candidates -> bounded local target/conflict result |
| BoundedGapCandidateBuilder | reduced obstacle witnesses -> bounded gap candidates |
| MovingGapPredictor | selected moving pair + time horizon -> predicted moving passage |
| MovingPassageTrajectoryEvaluator | vehicle/capability + predicted passage -> feasible trajectory witness |
| TrajectoryFollower | accepted short segment + current NavLocal state -> local control intent |
| NavigationExecutionReplanPolicy | explicit validity facts + time value -> replan decision |
| ManeuverDecisionController | explicit candidate set + doctrine -> selected candidate |

NavigationFrameBoundary is an immutable value-object transform. Its result is pure for the captured KinematicFrame; it is not allowed to look up a frame on its own.

## 2. Snapshot-pure

Snapshot-pure components own an immutable-at-query-time published snapshot or index. Query methods are deterministic and read-only, but strict functional purity does not apply because the snapshot lives inside the object.

| Component | Stateful operation | Snapshot-pure operation |
| --- | --- | --- |
| NavigationMap | replaceDynamicWorld() publishes/rebuilds the dynamic index | querySphere() / queryCorridor() |
| NavigationSpace | replaceStaticWorld() publishes/rebuilds topology + exact static geometry | query*() corridor/segment/topology queries |
| NavigationRuntimePlanner / LocalAvoidancePlanner | none | deterministic composition over explicit values plus `NavigationStaticQueryApi`; the state owner itself never crosses the calculation boundary |

Rules for snapshot-pure services and their consumers:

- state-owner objects stay at orchestration/publication ownership boundaries;
- adaptive static reads cross calculation seams only through `NavigationStaticQueryApi`;
- the static read API exposes query methods/results only: no publication, patching, invalidation, stats, storage views, or owner accessor;
- dynamic planner inputs continue to cross by value as `NavigationMap::QueryResult`;
- publication and query phases are separate;
- queries may not mutate semantic navigation state;
- query products leave by value, never as views/references to index internals;
- revision/source identity is part of the result where relevant;
- no wall-clock access inside a query;
- all time horizons/ages are explicit arguments.

## 3. Intentionally stateful

These components are allowed to mutate because their purpose is temporal execution or authoritative orchestration:

| Component | Why state is legitimate |
| --- | --- |
| PilotSkillExecutor / runtime control bridge | reaction delay, sampled decisions, command latency, slew/filter history and queued commands are sequential state |
| authoritative physics / ShipControlState | integrates physical state across fixed steps |
| GameSimulation | owns authoritative world state, publishes NavigationMap/NavigationSpace snapshots, stores accepted segment/revisions and diagnostics |
| replication/network layer | owns publication history, transport state and client hydration |

Stateful code must not absorb navigation math merely because it already has the data. It should:

1. read authoritative state;
2. construct explicit value DTOs;
3. bind any state owner to its narrow read-only API and call pure/query-capability navigation components;
4. apply/query authoritative state;
5. record the resulting transition.

The Stage-12 execution-safety seam follows this rule now: NavigationExecutionSafetyProbeBuilder constructs stopping and forecast probes without any NavigationSpace/HitVolume dependency; GameSimulation only submits those probe segments to authoritative exact-static queries and records the resulting invalidation.

The Stage-12 runtime-planning seam follows the same boundary rule. GameSimulation owns `NavigationSpace`, binds a short-lived `NavigationStaticQueryApi`, and passes only that read capability into NavigationRuntimePlanner. LocalAvoidancePlanner receives the same capability. Neither calculation surface receives `NavigationSpace&` or can publish, patch, invalidate, inspect stats, or obtain the owner.

## Isolation invariant

A navigation calculation is considered isolated only if it can be unit-tested from explicit values without constructing GameSimulation.

Exceptions are deliberate owner-level snapshot-query tests for NavigationMap and NavigationSpace, explicit read-capability composition tests for NavigationStaticQueryApi consumers, and sequential execution tests for PilotSkillExecutor.

If a future calculation needs world time, random input, geometry, pilot state or vehicle damage, the dependency must cross the boundary as an explicit value DTO or a deliberately narrow read-only API capability. A state-owner object must not cross a calculation boundary, and calculation code must not fetch global/runtime state internally.
