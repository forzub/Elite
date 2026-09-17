# LocalHorizonPlanner — NavigationWorld v2 local composition boundary

Stage: `NAV-V2-LOCAL-1`.

`LocalHorizonPlanner` consumes **already reduced** dynamic products from `NavigationMap` plus an upstream nominal target from accepted static route intent. It does not own another NavigationWorld, actor table, spatial index, global route or GPU resource.

## Current acceptance status

Target-machine behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: 1/1 PASS
100% tests passed, 0 failed
Total Test time = 0.05 sec
```

The ownership/safety semantics below are therefore behavior-accepted. Performance scaling over compact candidate count is measured separately by `benchmarks/navigation_local/` and is not yet accepted until the target-machine run is recorded.

## Input ownership

```text
AgentState
    entity id
    map-space P/V/A
    radius

NominalTargetState
    map-space target P/V/A supplied by upstream route/local intent

NavigationMap::QueryResult
    map/source revision
    compact Candidate[] only

Policy
    look-ahead
    maximum result age
    braking acceleration
    turn distance budget
    safety margin
    minimum physical horizon
```

The current planner deliberately accepts `NavigationMap::QueryResult` rather than internal NavigationMap storage. Backend selection therefore stays behind `NavigationMap.h`.

## Physical horizon

The first reference uses:

```text
latencyDistance = |v|*resultAge + 0.5*|a|*resultAge^2
brakingDistance = |v|^2 / (2*maxBrakingAcceleration)

horizonDistance = max(
    minimumHorizon,
    latencyDistance + brakingDistance + turnDistance + safetyMargin
)
```

A nominal target farther away is clipped to a pass-through target on this bounded horizon. A nominal target already inside the horizon remains terminal and preserves its supplied terminal velocity/acceleration.

## Dynamic conflict reference

For each compact candidate the CPU reference:

1. advances candidate P/V to the completed-result age using published acceleration;
2. computes bounded relative-motion closest approach;
3. evaluates actual accelerated separation at that time;
4. checks the intended bounded segment against the candidate's published conservative swept sphere, inflated by the agent envelope, safety margin and agent latency travel.

Either test may conservatively classify a conflict.

This is deliberately a first deterministic safety reference, not the final production avoidance algorithm.

## Output

```text
Status
    Clear
    ConflictHold
    StaleHold

TargetMode
    PassThrough
    Terminal
    Hold

Result
    bounded target state
    safe-progress flag
    map/source revision used
    result age
    horizon diagnostics
    primary conflict diagnostics
    candidate/conflict counts
```

`ConflictHold` and `StaleHold` are fail-closed products. The planner does not invent an unverified lateral bypass. Downstream control may brake/hold; a later measured local-avoidance algorithm can add adjusted safe targets behind this ownership boundary.

## Candidate-count performance measurement

The dedicated downstream harness consumes only compact query products and measures:

```text
clear:     0 / 16 / 64 / 256 / 1024 candidates
conflict:     16 / 64 / 256 / 1024 candidates
stale:                           1024 candidates
```

`stale_1024` must return before candidate evaluation. `1024` is a stress scale, not an expected ordinary NavigationMap candidate count.

Raw measurement location: `benchmarks/navigation_local/RUN_LOG.md`.

## Non-goals

- no full-scene actor scan;
- no second NavigationWorld snapshot;
- no GLM/OpenGL/render/game dependency;
- no route-wide dense trajectory;
- no static-cost mutation from dynamic traffic;
- no pursuit-specific intercept logic;
- no Ruckig ownership.

The old GLM-based `TacticalCollisionMonitor` and `SmallCraftNavigation` remain migration/reference code only.
