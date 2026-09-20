# Planner / Follower architecture hypothesis

**Status:** architecture decision candidate
**Updated:** 2026-09-19 Europe/Kyiv
**Repository:** `forzub/Elite`
**Related:** `NAVIGATION_PIPELINE_AUDIT.md`, `NAVIGATION_COMMAND_OWNERSHIP.md`, `TRAJECTORY_EXECUTION_REPLAN_MODEL.md`

## Decision summary

Navigation v2 should expose two top-level runtime worlds:

```text
NAVIGATION / PLANNER WORLD
    known world truth + objective + vehicle/pilot/doctrine
        -> route/corridor
        -> physically feasible bounded maneuver program
        -> AcceptedManeuverProgram

AUTOPILOT / FOLLOWER WORLD
    AcceptedManeuverProgram + actual state
        -> tracking control
        -> safety monitoring / bounded reflex
        -> PilotSkill / propulsion / physics
        -> completion or invalidation -> planner wake-up
```

This is compatible with the already documented command-ownership model, but it sharpens the implementation boundary.

## Important clarification: world knowledge versus ray tests

The planner does **not** need to discover obstacles as if it were a sensor.

The simulation already owns authoritative geometry and motion. NavigationMap and NavigationSpace are world-query structures, not perception models.

Therefore:

```text
ray / segment / sweep query
    != "look to see whether an obstacle exists"

ray / segment / sweep query
    = cheap geometric proof against already-known world truth
```

The production local emergency solver no longer uses an angular ray fan. It
uses the accepted trajectory tangent as a reference, projects predicted moving
occupancy onto the normal plane, searches metric lateral/vertical offsets, and
uses segment/OBB/sweep queries only as geometric proof primitives.

## Planner world responsibilities

The planner owns:

1. semantic objective / terminal contract input;
2. coarse topology/corridor selection;
3. local free-space path construction from authoritative world data;
4. conversion of geometric path into a physically flyable maneuver;
5. continuous capability + geometry proof;
6. publication of the exact bounded program that passed proof.

It does not run every physics tick unless invalidation requires it.

### Target output

```text
AcceptedManeuverProgram
    identity / revisions / validity interval
    maneuver family / doctrine provenance

    P(t), V(t), A_ff(t)
    q(t) / body basis(t), omega(t), alpha_ff(t)

    terminal tolerances
    tracking envelope / reserved feedback authority
    collision/clearance witness
    capability / world revision witnesses
```

The same program is proved and executed.

## Route-aligned corridor planning extension

The current unexpected-obstacle layer already uses a route-aligned normal plane
for one bounded visible horizon. For more complex **known** local geometry, this
can be extended longitudinally into a route-aligned configuration-space
corridor without introducing a second planner.

Given start A and target/corridor point B:

```text
e_s = normalize(B - A)
e_u, e_v = stable perpendicular basis

world point X
    -> s = dot(X-A, e_s)
    -> u = dot(X-A, e_u)
    -> v = dot(X-A, e_v)
```

The planner then works in longitudinal slabs along `s`.

For every relevant static/dynamic obstacle:

1. use authoritative HitVolume / exact dynamic witness;
2. inflate by ship envelope + required clearance + pilot/tracking uncertainty;
3. project its occupied interval into the route frame;
4. insert forbidden cross-section regions only into the slabs where the obstacle exists;
5. preserve longitudinal ordering; never collapse the entire tunnel into one 2-D projection.

Free-space components across adjacent slabs form a small forward graph/DAG. The planner chooses a smooth progress-preserving chain through those components.

Use adaptive slab boundaries at obstacle entry/exit planes, portals and important topology changes rather than a centimeter-scale uniform grid.

### Why this is attractive

- world truth is consumed directly; no pseudo-sensor discovery loop;
- deterministic and naturally bounded by the local corridor;
- obstacle work is easy to vectorize/batch after broadphase isolation;
- conservative hull inflation is explicit;
- the same path can directly generate a visible manual guidance tunnel;
- obstacle order along the route is preserved;
- keeps local search metric and trajectory-relative rather than angular.

### Required limitations

A single A->B coordinate frame is not a universal global planner.

It assumes monotonic progress in `s`. It cannot by itself represent:
- U-shaped traps requiring temporary backward motion;
- routes around large topology barriers where the correct first move is away from B;
- branches that leave the local corridor;
- arbitrary maze/station topology.

Therefore the route-aligned corridor planner is a **local/corridor solver**, not a replacement for NavigationSpace topology.

Correct hierarchy:

```text
NavigationSpace / global topology
    -> corridor / portal branch
    -> route-aligned local configuration-space planner
    -> physical maneuver compiler
```

If the local solver cannot connect free space monotonically, it reports corridor failure and the topology layer changes the branch.

## Dynamic obstacles

A moving obstacle is not just a static 2-D projection.

Its occupancy is time dependent:

```text
forbidden region = F(s, u, v, t)
```

The initial implementation may use bounded predicted swept volumes/time windows per slab. Precision encounters may continue using continuous trajectory evaluators.

The route geometry and its time parameterization therefore cannot be completely independent. A geometric corridor is only a candidate until the maneuver compiler proves timing and vehicle feasibility.

## Vehicle shape

A simplified ship silhouette is valid only if conservative for the planned attitude family.

Options:
- sphere/capsule for cheap broadphase;
- orientation-aware OBB/support radius for ordinary proof;
- exact authored HitVolumes for precision/final narrow phase.

For a long ship in a narrow slit, rotating the hull changes the cross-section materially. The planner must not use one fixed small silhouette if the accepted maneuver rotates the ship.

## Terminal-state cases

Ordinary transit may have loose terminal conditions.

Docking, formation capture, retrieval, weapon/intercept geometry or other strict tasks may require:

```text
P(t1) ~= P_goal
V(t1) ~= V_goal / relative V_goal
q(t1) ~= q_goal
omega(t1) ~= omega_goal
```

For airplane/assisted control, this often creates effective turn-radius/curvature constraints.

For Newtonian control there is no single fixed "turn radius". Feasibility comes from:
- current momentum;
- angular acceleration/speed;
- main-engine direction;
- translational acceleration limits;
- lead rotation;
- available time/distance;
- braking/flip-and-burn requirements.

Strict terminal tasks therefore need a boundary-value maneuver solve or a small set of physically meaningful maneuver primitives, not just a geometric radius.

## Maneuver authority: planner decides, follower tracks

For automatic flight the planner side owns the **physical maneuver family**,
not merely a geometric waypoint.

Examples of planner-side decisions:
- keep current attitude and use RCS trim;
- lead-rotate the hull and then use the main engine;
- coast while rotating;
- brake / flip-and-burn;
- acquire a precision passage attitude.

For the default main-engine-dominant Newtonian craft, main propulsion is the
preferred translation mechanism for material delta-v. RCS is primarily
precision/trim authority. The physical compiler therefore exposes a main-engine
candidate even when a slow RCS solution is also physically possible.

Ownership remains split:
- B5 generates physical alternatives;
- B6 proves the exact alternatives;
- B7 chooses among proved alternatives according to doctrine/objective;
- B8 freezes the chosen program;
- B9/B10 follower executes that program and adds only bounded feedback.

The follower must **not** decide "RCS is weak, rotate and use main engine".
Doing so would make it a hidden second planner and would invalidate the
planner's geometry/capability proof. If execution cannot remain inside the
accepted envelope, the follower invalidates/wakes planner work instead.

## Autopilot / follower world

Top-level execution may still be called one "follower" world, but internally it must separate three responsibilities.

### 1. Program follower

Samples the accepted program and applies bounded tracking feedback.

```text
A_cmd = A_ff + bounded tracking feedback
alpha_cmd = alpha_ff + bounded tracking feedback
```

It does not choose a new route or target velocity.

### 2. Safety monitor / bounded reflex

Continuously checks the short future execution horizon against new invalidating evidence.

For an imminent unexpected hazard it may apply a bounded emergency reflex to avoid immediate collision/impact.

This reflex is intentionally limited. If it materially changes the trajectory, the accepted program becomes invalid and the planner is woken immediately.

This avoids turning the follower into a hidden second route planner.

### 3. Recovery after disturbance

For small disturbances, impact impulses or ordinary tracking error inside the reserved envelope, the follower should not return by the shortest geometric path. It should continue making mission progress while reducing trajectory error smoothly.

Conceptually:

```text
tracking objective =
    forward/program progress
    + cross-track error decay
    + velocity/attitude error decay
```

If the deviation exceeds the proved tracking envelope, replan rather than forcing recovery to an obsolete path.

## Shared scene-wide computation

The user's "navigation world for all / autopilot world for all" is the preferred scaling direction, with one nuance: **batch shared work, but do not replan every actor every tick.**

### Navigation world frame

Once per relevant world update:

```text
authoritative dynamic snapshot
    -> spatial broadphase
    -> sparse potentially-interacting pairs
    -> batch/SIMD kinematic filter
    -> per-agent influence lists
```

Static topology/geometry is revisioned and reused until it changes.

Then only agents whose program is missing/expired/invalidated/goal-changed enter the planner batch.

### Autopilot world frame

Every fixed physics/control tick:

```text
all active AcceptedManeuverPrograms
    + all current vehicle states
    -> batched follower sampling
    -> batched tracking/safety monitoring
    -> control intents per vehicle
```

This is naturally SoA/SIMD-friendly.

The planner cadence and follower cadence must remain separate.

## Comparison with current repository

Already aligned:
- NavigationMap receives authoritative dynamic world snapshots;
- NavigationMap uses a spatial hash, not mandatory dense N^2;
- NavigationSpace owns static topology/exact geometry;
- NavigationExecutionReplanPolicy already encodes plan -> accept -> execute -> monitor;
- TrajectoryFollower is already separated from obstacle search;
- command-ownership docs already target AcceptedManeuverProgram;
- scene-wide sparse pair generation is already documented as the scaling target;
- PilotSkill and propulsion/physics are separate owners.

Still divergent:
- the current projected visible-horizon solver is one bounded normal-plane slice, not yet a longitudinal multi-slab local corridor;
- ordinary AdjustedClear remains geometric rather than physically time-parameterized;
- NavigationRuntimePlanner still mixes topology/local search/precision proof/control-intent production;
- AcceptedShortSegment is transitional and the follower can re-derive control;
- there is no explicit general safety-reflex contract;
- dynamic candidate discovery is still issued per agent, so pairs may be rediscovered;
- complex known local geometry may still require a richer route-aligned longitudinal corridor representation.

## Architecture verdict

The two-world hypothesis is **accepted as the preferred direction**, with these constraints:

1. keep global/topological routing above the local route-aligned corridor solver;
2. remove ray-fan **search** semantics, not geometric segment/sweep proof primitives;
3. planner must publish the exact physically proved maneuver program;
4. follower may track/recover inside a bounded envelope but must not become a second normal planner;
5. imminent-hazard reflex is allowed only as a bounded safety override and must trigger replan after material deviation;
6. shared world/influence computation should be batch/scene-wide;
7. planner runs only for agents that need a new program; follower runs every control tick.

## Recommended implementation order

1. Keep `AcceptedManeuverProgram` as the single execution product and continue migrating ordinary live paths onto it.
2. Preserve current NavigationSpace topology and exact-geometry services.
3. Keep the projected visible-horizon solver as the **only** ordinary unexpected-obstacle local path; do not restore a parallel angular-fan fallback.
4. Extend the same local owner longitudinally into route-aligned slabs only when known complex geometry requires more than one normal-plane slice.
5. Complete maneuver compilation from local geometry -> Newtonian/Assisted physical program.
6. Add explicit bounded safety-reflex contract to the autopilot world.
7. Move dynamic influence isolation from repeated per-agent discovery toward a shared scene-wide sparse pair/influence frame.
8. Retire remaining transitional AcceptedShortSegment/control-intent seams only after target-machine evidence.


## Rigid-body maneuver proof

Planner-side maneuver generation/proof must account for the complete rigid body,
not only the center path.

A proved automatic maneuver therefore includes:
- P(t), V(t);
- body attitude/basis(t);
- angular velocity/acceleration;
- physical actuator family/source;
- hull occupancy through the corridor over time.

Newtonian braking is not equivalent to Assisted braking:
- Newtonian main braking requires the hull to turn so aft thrust opposes
  velocity;
- Assisted may remain nose-forward and use fore/reverse longitudinal main
  thrust.

The follower executes that proved attitude/thrust history. It may not hide a
missing Newtonian flip by applying an impossible reverse main engine or an
omnidirectional acceleration vector.


## Corner passage is maneuver-family specific

A follower does not decide that a route vertex was passed merely because the
center crossed a coordinate.

The accepted maneuver family defines the passage condition:
- StopTurnGo: waypoint capture + near-stop + outgoing attitude capture;
- RadiusTurn: continuous proved turn and outgoing-gate crossing;
- DriftTurn: continuous proved high-slip turn and outgoing-gate crossing.

For moving corner families, internal reference-phase handoff is time/program
driven. The externally meaningful completion event is the proved outgoing gate
and exit-state envelope.

The same PilotSkill profile and rigid hull may therefore produce different
corner times, corridor-width requirements and tracking reserves for different
families and control laws.


### Compound phase handoff invariant

A compound accepted maneuver may contain multiple physical phases, but their
handoff condition must preserve family semantics.

`StopTurnGo` is state-capture driven:
- brake/capture waypoint;
- satisfy near-stop velocity envelope;
- satisfy required attitude/angular-velocity envelope;
- only then release the next phase.

`RadiusTurn` and `DriftTurn` are continuous moving maneuvers. Their internal
sample/phase progression may remain time/program driven, but route completion is
still an outgoing-gate + exit-state event.

Time expiry may never be used to pretend that a failed capture succeeded.
