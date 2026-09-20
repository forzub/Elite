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

## Active avoidance — projected visible horizon

`LocalAvoidancePlanner` composes the accepted nominal route with the compact
dynamic candidates from `NavigationMap` and exact static read access through
`NavigationStaticQueryApi`.

It does **not** own an angular ray fan, persistent left/right branch state or a
second route planner.

Flow:

~~~text
accepted nominal trajectory / local target
        |
        v
LocalHorizonPlanner
    physical visible horizon
    nominal conflict prediction
        |
        +-- nominal dynamic + static clear
        |       -> NominalClear
        |
        v
trajectory tangent F
        |
        v
normal plane Pi, Pi perpendicular to F
        |
        +-- project moving-obstacle P(t) swept occupancy
        +-- retain exact-static corridor constraints
        |
        v
deterministic metric offset grid (meters)
        |
        v
projected occupancy rejection
        |
        v
exact-static segment proof
        |
        v
time-coupled dynamic proof
        |
        +-- safe offset -> AdjustedClear / PassThrough
        |                 + merge target on original trajectory
        |
        +-- none safe   -> localBypassExhausted / fail closed
~~~

### Why projection is trajectory-relative

Known persistent static geometry belongs to the route/corridor plan. The local
layer reacts to an unexpected obstacle by asking a smaller question:

> what temporary lateral/vertical displacement inside the current physical
> horizon avoids the predicted obstacle and still allows reacquisition of the
> accepted trajectory?

The solver therefore works in the plane normal to the nominal trajectory
rather than rebuilding global topology or choosing an abstract left/right
branch.

### Dynamic prediction

For each relevant `NavigationMap::Candidate`, current state is aged to the
completed snapshot epoch and the actor is predicted over the visible horizon.
Its motion projects to a swept segment/capsule in the normal plane.

A candidate offset must:
- clear projected dynamic occupancy with vehicle + safety inflation;
- pass exact-static segment proof from the current position;
- pass a time-coupled dynamic sample check over the same horizon.

The output publishes both:
- a temporary off-route bypass target;
- `mergeTargetMapMeters`, the point on the original bounded trajectory to
  reacquire after the unexpected obstacle is passed.

### Speed and physical execution

This local layer selects free-space geometry. It does not pretend every
geometric offset is an executable maneuver.

Downstream physical maneuver compilation may:
- retain speed;
- reduce speed to make the offset reachable;
- use control-law-specific lateral/vertical/rotational authority.

A full stop is not the normal avoidance algorithm. If no bounded local offset
can be proved, `localBypassExhausted` fails closed and higher ownership may
slow further, change the local waypoint/portal, backtrack or choose an
emergency maneuver.

### Exact static ownership

All temporary bypass targets are strict exact-static queries. The only endpoint
exception remains the already-proven nominal portal boundary supplied by the
global corridor.

Static and dynamic evidence must come from coherent revisions. The local solver
does not manufacture free space and does not restore stationary objects as
dynamic spheres.

### Pinned behavior

~~~text
nominal_clear
    -> NominalClear, no offset search

crossing_dynamic_obstacle
    -> predicted normal-plane occupancy
    -> temporary metric offset
    -> AdjustedClear
    -> merge target remains on nominal trajectory

head_on_with_free_lateral_space
    -> bypass without mandatory stop

exact_static_blocker
    -> projected offsets filtered by exact static geometry

obstacle_disappears
    -> immediate return to nominal trajectory

no_offset_fits
    -> localBypassExhausted / fail closed

stale_snapshot
    -> StaleHold before offset search
~~~

The old angular deflection fan and branch-continuity mechanism are removed from
the production API and implementation. The Stage-12 architecture checker
explicitly rejects their identifiers if they are reintroduced.

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
