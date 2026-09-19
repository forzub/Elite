# NavigationWorld v2 local layer

Stage: `NAV-V2-LOCAL-1`.

This directory owns the backend-neutral composition between accepted static route intent, compact dynamic products from `NavigationMap`, and downstream local kinematics. It does not own another NavigationWorld, actor table, spatial index, global route or GPU resource.

## Accepted `LocalHorizonPlanner` reference

Target-machine behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: 1/1 PASS
100% tests passed, 0 failed
Total Test time = 0.05 sec
```

Accepted inputs:

```text
AgentState
    entity id
    map-space P/V/A
    radius

NominalTargetState
    map-space target P/V/A supplied by upstream route/local intent

NavigationMap::QueryResult
    compact Candidate[] only

Policy
    look-ahead
    maximum result age
    braking acceleration
    turn distance budget
    safety margin
    minimum physical horizon
```

Physical horizon:

```text
latencyDistance = |v|*resultAge + 0.5*|a|*resultAge^2
brakingDistance = |v|^2 / (2*maxBrakingAcceleration)

horizonDistance = max(
    minimumHorizon,
    latencyDistance + brakingDistance + turnDistance + safetyMargin
)
```

A far nominal target becomes bounded `PassThrough`; a target inside the horizon remains `Terminal`. Dynamic candidates are aged, checked by bounded closest approach and by conservative swept-sphere intersection against the bounded intended segment.

Outputs:

```text
Clear
ConflictHold
StaleHold
```

`ConflictHold` and `StaleHold` fail closed.

## Accepted compact-candidate scaling

Target-machine run on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
scenario         p95_us     p95_ns/candidate
clear_16          0.4814          30.0873
clear_64          1.8327          28.6362
clear_256         7.9820          31.1798
clear_1024       36.9641          36.0977

conflict_16       0.5073          31.7062
conflict_64       1.9333          30.2078
conflict_256      7.5441          29.4693
conflict_1024    31.8656          31.1188

stale_1024        0.0359          0 candidates examined
```

The loop is accepted. Even the artificial 1024-candidate stress case is below `0.037 ms p95`; further optimization is not justified without new runtime evidence. Raw evidence: `benchmarks/navigation_local/RUN_LOG.md`.

## Active avoidance candidate — same-region lateral fan

`LocalAvoidancePlanner` does not replace `LocalHorizonPlanner`; it composes it with `NavigationStaticQueryApi`, the narrow read-only capability bound by the orchestration layer. The `NavigationSpace` state owner itself does not cross into the calculation.

Flow:

```text
nominal LocalHorizonPlanner result
    |
    +-- Clear ------> keep nominal target, zero avoidance probes
    |
    +-- StaleHold --> fail closed, zero avoidance probes
    |
    +-- ConflictHold
            |
            v
      query static start region
            |
            v
      deterministic 3D lateral fan
      15 deg ring x 8 azimuths
      30 deg ring x 8 azimuths
            |
            v
      static same-region proof
            |
            v
      recheck dynamic candidates through LocalHorizonPlanner
            |
            +-- first proven target -> AdjustedClear / PassThrough
            +-- none proven --------> ConflictHold
```

### Why same-region

The current `NavigationSpace` semantic free-space region is an axis-aligned box. For an agent envelope, the traversable interior of one region is a shrunken convex AABB. Therefore, when both the current agent point and an adjusted target are traversable and resolve to the same region, the entire straight segment between them is statically contained in that free-space volume.

Both endpoint results must come from the **same static publication**: identical `spaceRevision` and `sourceRevision`. Mixed-revision endpoint evidence fails closed as `StaticHold` instead of composing stale and current free-space facts.

This is intentionally conservative. It may reject a valid maneuver crossing a portal or overlapping region, but it does not invent static free space. Portal-aware local avoidance is a later extension only if runtime evidence requires it.

### Current-kinematics conflicts remain fail closed

The accepted `LocalHorizonPlanner` closest-approach test uses the ship's current P/V/A. A lateral target therefore must **not** magically erase an already predicted head-on/crossing conflict. The first avoidance slice can clear a future swept-corridor blocker when current closest approach is still safe, but head-on/current-kinematics conflicts remain `ConflictHold` until a trajectory-aware maneuver is separately demonstrated.

Pinned behavior fixtures:

```text
nominal_clear
    -> NominalClear, zero probes

swept_corridor_blocker
    -> same-region AdjustedClear when a lateral target is proven

head_on
    -> ConflictHold; lateral target cannot erase current kinematics

narrow_static_region
    -> all lateral probes rejected statically; ConflictHold

stale_snapshot
    -> StaleHold before avoidance probes

non_traversable_start
    -> StaticHold
```

The avoidance candidate is **pending target-machine compile/behavior gate**. Its multiplied probe cost is not yet accepted and will be benchmarked only after behavior passes.

## Non-goals

- no full-scene actor scan;
- no second NavigationWorld snapshot;
- no `NavigationSpace&` state-owner capability inside local-avoidance calculation;
- no GLM/OpenGL/render/game dependency;
- no route-wide dense trajectory;
- no static-cost mutation from dynamic traffic;
- no pursuit-specific intercept logic;
- no Ruckig ownership;
- no unproved portal-crossing lateral bypass.

The old GLM-based `TacticalCollisionMonitor` and `SmallCraftNavigation` remain migration/reference code only.
