# Pursuit Horizon — moving-goal navigation contract

**Status:** accepted Navigation v2 design contract; implementation is later than `NAV-V2-SPACE-1`  
**Updated:** 2026-09-16 Europe/Kyiv  
**Canonical branch:** `main`

## Purpose

Navigation v2 needs two different goal semantics without creating two different navigation systems:

```text
fixed / strategic goal
    A -> cached coarse corridor -> successive local targets -> B

moving / pursuit goal
    A -> cached/receding corridor -> predicted intercept horizon -> moving target
```

A pursuer must not repeatedly fly toward the target's stale current position and must not rebuild a complete global route every frame.

The pursuit mode therefore uses a **receding moving-goal horizon**. The final goal is an actor whose future state changes continuously; the planner chooses a bounded predicted intercept state/region and executes only the near part of the solution before updating it.

## Target information

The prediction source is policy-controlled. A pursuer may use only information available to that agent/gameplay relationship:

```text
observed/shared target P/V/A
observation age / confidence
known static NavigationSpace topology
known dynamic conflict candidates
optional allied/shared route intent
```

An adversary must not automatically consume the target NPC's private planned route merely because both actors are simulated by the same process. Shared intent is allowed only when gameplay policy explicitly permits it, for example allied formation or coordinated traffic.

## Intercept horizon

The pursuer derives a bounded future time `T_goal` from relative geometry, speed capability, latency and maneuver limits. Exact tuning is not frozen here.

First-order prediction may use:

```text
P_target(T) = P + V*T + 0.5*A*T^2
```

A constant-velocity intercept estimate may be used as an initial guess, then clamped to the configured pursuit horizon and refined against static/dynamic feasibility.

The important invariant is that the target is **not** treated as a fixed point at its current position.

## Geometry-aware prediction

Pure kinematic extrapolation may not predict a target straight through known walls, terrain, station structure or other non-traversable static space.

NavigationSpace is used to constrain the predicted target motion:

```text
target observed motion
        |
short forward prediction
        |
known static free-space / portals
        |
+-------+--------+
|                |
free continuation   obstacle / narrow passage
|                |
lead point        clamp/branch prediction through
                  plausible traversable region/portal
```

This is intentionally a **small local prediction**, not an attempt to infer the target's entire future plan.

Examples:

- a target flying toward an open canyon can be predicted deeper along the canyon rather than through its wall;
- a target approaching a sufficiently large aperture can be predicted through that portal;
- if several future branches are plausible, the pursuer may retain a conservative intercept region until more motion evidence resolves the branch.

## Replanning policy

Do not rebuild the global corridor merely because the target moved a few meters.

The preferred behavior is:

1. keep the current coarse corridor while the predicted intercept remains in the same corridor/region neighborhood;
2. update the local intercept state at a higher cadence;
3. rebuild the coarse corridor only when the predicted intercept moves to a materially different region/portal branch, the current corridor is invalidated, or prediction error exceeds policy thresholds;
4. execute only the near segment on a receding physical horizon;
5. include snapshot/prediction age in the safety horizon.

Typical trigger classes:

```text
intercept region/portal changed
corridor revision invalidated
large target heading/acceleration change
prediction error exceeded
sensor/observation age exceeded
pursuit target lost/reacquired
```

## Relationship to static and dynamic NavigationWorld blocks

```text
NavigationSpace (CPU)
    free-space / clearance / portals / coarse corridor
                       |
                       +------------------+
                                          |
NavigationMap (dynamic)                   |
    target P/V/A + conflicts              |
          |                               |
          v                               v
     pursuit predictor ----------> predicted intercept state/region
                                          |
                                  local/precision planner
                                          |
                                  temporary target state
                                          |
                                  RuckigTrajectorySolver
```

The moving-goal layer consumes NavigationSpace and NavigationMap products. It does not own a second world representation.

## Pursuit vs ordinary route

A fleeing NPC with a strategic destination can still use the ordinary cached route contract:

```text
A -> B -> execute successive local targets
```

The pursuing NPC uses the same world/corridor machinery with a moving final condition:

```text
A -> predicted target region/state at T_goal
```

If the pursued actor changes course, the pursuer updates the intercept horizon. This naturally produces lead pursuit instead of following the target's historical breadcrumb trail.

## Future acceptance cases

Implementation should eventually include deterministic tests for at least:

```text
straight-line lead pursuit
crossing-target interception
large target heading change
pursuit through a traversable aperture
pursuit through / around a canyon branch
known obstacle ahead of target prevents prediction through solid geometry
moving intercept remains on current corridor -> no global replan
intercept moves to another region/portal branch -> coarse replan
stale/lost target -> bounded fallback rather than infinite extrapolation
```

This contract does not prescribe combat tactics or AI intent. It defines only how a moving navigation goal is represented and updated.