# CURRENT TASK — credible static corner maneuvers before dynamic avoidance

**Date:** 2026-09-21  
**Status:** STATIC E2E EXECUTES / BEHAVIOR QUALITY BLOCKER FOUND / DYNAMIC OVERLAY PAUSED

## Why this task supersedes the dynamic-overlay step

The retained-route Stage-2 chain now reaches the finish, but the user's video exposed
behavior that is not acceptable for an Expert pilot:
- ~25.5 degree corners can become full StopTurnGo;
- Newtonian velocity can bend before body attitude has acquired the physically useful
  thrust direction;
- Newtonian body reference can rotate toward braking and then rotate again for the
  next leg;
- Assisted inherits the same stop because the common Ruckig waypoint velocity is zero;
- Standard and Extreme currently differ mostly by 10 vs 18 m/s requested max speed.

Do not add dynamic obstacle behavior on top of this until ordinary static maneuver
authoring is credible.

## Confirmed root causes

### Route geometry

The former box visibility graph lacked edge-midpoint support nodes. The old default
route was approximately:

```text
(0,0,0) -> (110,-27,-45) -> (190,-27,-45) -> (300,0,0)
length ~= 323.75 m
```

The intended nearest-face route is approximately:

```text
(0,0,0) -> (110,-27,0) -> (190,-27,0) -> (300,0,0)
length ~= 306.53 m
```

Candidate route fix:
`e53312cc9b00119e69f1c7676edf14b5d21fea64`.

Regression:
`8e0d4c4c6ca7d4d7ae3e7cc71387ebff8b335302`.

### StopTurnGo fallback

`TrajectoryGenerator::buildWaypointVelocities()` tries one synthetic shortcut chord
around each corner. If that chord intersects the inflated wall, the waypoint remains
at zero target velocity. Ruckig then stops there.

A blocked shortcut chord does **not** imply that the vehicle must stop. It only proves
that this particular shortcut construction is invalid.

### Newtonian reference semantics

The current Stage-2 pipeline creates translational P/V/A first. Afterwards
`buildReferenceAttitudes()` points Newtonian forward toward requested acceleration.
That lets RCS alter velocity while attitude is still rotating and creates excessive
body rotation around braking/leg transitions.

The physical allocator itself remains directionally meaningful; the defect is the
upstream physical maneuver reference.

## Active implementation objective

Replace ordinary corner handling with control-law-aware physical maneuver authoring.

Required behavior for Expert:
- shallow/medium bend + adequate space/authority -> continuous pass, not mandatory stop;
- Newtonian -> body/thrust-aware lead turn / coast-drift / bounded RCS trim /
  main-engine burn as the geometry and delta-v require;
- Assisted -> smooth continuous pass using its longitudinal assisted authority and
  bounded RCS;
- a stop is legal only when geometry, terminal state, vehicle authority or safety
  actually requires it;
- no full flip merely to negotiate a small heading change;
- body/velocity slip must be intentional and bounded by the selected maneuver;
- the exact maneuver accepted by Follower must be the same maneuver that passed
  capability and geometry proof.

Standard vs Extreme must become a doctrine difference:
- Standard: more clearance/reserve, smoother lower-slip choices;
- Extreme: faster/more aggressive proved choices, may accept larger controlled slip
  and use more of the safe envelope;
- neither mode may violate hard safety/authority proof.

## Architecture boundary

Do not solve this by:
- increasing RCS to make arbitrary vectors work;
- widening collision clearance;
- forcing waypoint velocities nonzero without a physical maneuver proof;
- moving maneuver choice into Follower;
- adding viewer-only motion logic.

Use:
- B4 geometric local/route path;
- B5 control-law-aware maneuver compiler;
- B6 continuous maneuver proof;
- B7 doctrine selection;
- B8 AcceptedManeuverProgram;
- B9/B10 Follower execution.

Current ordinary B5 supports Newtonian first. General ordinary B6 remains a real gap
and should be filled rather than bypassed.

## New observability already committed

`ac30d441ebdafe232af0bf0fe7e8882da9f38767`:
- triangular-prism Cobra diagnostic hull;
- smaller translucent nose marker;
- thick velocity vector proportional to speed;
- numeric speed label at its tip.

`c4c63c9751a4b5b381da290a570ee98775148eb6`:
- exact retained route points in Stage-1 diagnostics;
- retained interior waypoint speeds;
- maximum body/velocity angle.

## Immediate target-machine gate

Run latest main and inspect:
- nominal route length/points;
- whether the new box regression passes;
- viewer compiles;
- `RETAINED WAYPOINT SPEEDS`;
- `MAX BODY/VELOCITY ANGLE`;
- video/visual behavior with Expert Standard Newtonian and Assisted.

After this evidence, implement the physical corner-authoring correction.

## Mandatory state protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
