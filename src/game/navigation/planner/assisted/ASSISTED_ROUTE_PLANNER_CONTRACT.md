# Assisted Route Planner V3 boundary contract

Production ordinary Assisted routing is owned by:

- `planner/assisted/AssistedRoutePlannerApi.h`
- `planner/assisted/AssistedRoutePlanner.cpp`

The historical `DockingAdvisoryPlanner` remains in the repository only as a
legacy regression/recovery implementation. Production `RoutePlanner::plan`
MUST NOT call or include it.

## Input boundary

The Assisted planner accepts one free-space leg only:

- oriented start state: position, forward/tangent, speed;
- oriented goal state: position, forward/tangent, terminal speed;
- vehicle physical limits;
- ordinary geometry policy;
- ordinary cost weights;
- static navigation obstacles.

It MUST NOT accept:

- traffic graphs;
- compiled traffic routes or stages;
- BLUE/GREEN/RED semantic types;
- docking-port definitions;
- follower/autopilot runtime state;
- semantic via-point lists.

Mandatory semantic passages are split into free-space legs outside this module.

## Geometry rules

For a fixed design speed, minimum turn radius is a HARD constraint.

The planner may:

1. rotate the candidate turn plane in 3D;
2. change the arc sweep;
3. choose another collision-free candidate;
4. reduce design speed and recompute the radius only if no admissible geometry
   exists at the higher speed.

It MUST NOT silently shrink a turn below the radius required by the current
design speed.

The protected straight before/after a boundary is independent from turn-radius
budget.

## Optimization rule

Hard constraints are checked before cost ranking.

Among admissible candidates, the ordinary Assisted cost function may rank:

- route length;
- unnecessary delta-v / speed changes;
- maneuver count;
- traversal time.

A collision, topology violation, reverse semantic passage or sub-minimum radius
is never represented as a finite penalty.

## Output boundary

The planner returns generic `planner::RoutePlan` geometry only:

- Line / CircularArc / future CubicBezier primitives;
- dense execution gates sampled from those primitives;
- sparse display gates;
- local speed ceilings.

It does not execute controls and does not interpret traffic semantics.
