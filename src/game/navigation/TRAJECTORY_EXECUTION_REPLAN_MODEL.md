# Trajectory execution and replanning model

**Status:** architecture contract / Stage-12 active integration
**Updated:** 2026-09-18 Europe/Kyiv
**Related:** `src/game/MANEUVER_DECISION_TREE.md`, `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`, `src/game/navigation/GuidanceCorridor.h`, `src/game/navigation/RoutePlan.h`

## Core rule

Automatic execution is not `plan every fixed tick`.

~~~text
plan
 -> prove / accept short trajectory segment
 -> execute accepted segment
 -> monitor validity
 -> replan only when invalidated / completed / expired
~~~

The fixed-step loop may monitor tracking error, exact collision safety and new hazard evidence every frame. Monitoring is cheap and does not mean invoking the route/trajectory planner every frame.

## Why this is valid

Once a short segment has been proven against the current vehicle capability and the predicted world inside its validity horizon, automatic control is deterministic. The ship should follow the accepted acceleration/attitude program instead of asking the planner to reinvent the next command every simulation step.

Replanning during a valid automatic segment is required only when an assumption changes:

~~~text
goal/mission intent changed
topology/current route branch invalidated
new hazard invalidated accepted segment
vehicle capability changed (damage/thruster loss)
tracking error exceeded accepted envelope
segment completed
segment validity horizon expired
~~~

Predicted motion of already-known actors inside the accepted segment is not by itself a reason to replan each frame. It was part of the proof. A materially new observation or prediction error is an invalidation event.

## Automatic mode

Automatic authority owns the selected trajectory.

~~~text
accepted trajectory epoch
    -> follower samples current desired state / acceleration / attitude
    -> PilotSkillExecutor applies real skill/latency limits
    -> physics executes
    -> monitor compares actual state with accepted envelope
~~~

If monitoring stays green, the planner sleeps.

A short segment may contain multiple control keys. Following those keys is execution, not replanning.

## Manual mode

Manual mode never gives trajectory steering authority to the guidance product. The player can leave the recommended corridor at any moment.

Manual guidance therefore uses:

~~~text
periodic local refresh
+ immediate local refresh on corridor exit
+ immediate refresh on new hazard / capability change
~~~

The default policy currently exposes a configurable 0.25 s local refresh interval. It is a policy value, not a hard gameplay constant.

Leaving a manual corridor normally rebuilds only the short local suffix:

~~~text
existing global RoutePlan stays
existing topology/portal branch stays
new ship state becomes local planning seed
new short guidance corridor is generated ahead of the player
~~~

A full global route rebuild is required only when the player invalidates the route branch itself, changes the goal, crosses into incompatible topology, or otherwise makes the current global plan invalid.

## Replan scope

Two explicit scopes are required:

~~~text
LocalHorizon
    replace only executable/advisory short suffix
    preserve accepted global route and remaining portal sequence

FullRoute
    rebuild global/topological route
~~~

This prevents a manual wobble or a small obstacle from triggering a galaxy/system/maze solve.

## Relationship to bounded visibility

Bounded-visibility steering remains a local candidate generator.

Under automatic control, once its short bypass trajectory is accepted, that bypass is executed until its local segment ends or is invalidated. The planner does not need to reselect 15/30/45/... degrees every fixed frame.

Under manual control, the same solution becomes a visible guidance corridor. The local suffix is refreshed periodically because the player may choose not to follow it.

## Relationship to precision passages

Tunnel/dock/wreck entry especially benefits from accepted-segment execution.

Example:

~~~text
capture trajectory accepted:
  t0..t1 remove cross-track velocity
  t1..t2 align hull
  t2..t3 enter on normal
  t3..t4 transit

automatic:
  execute this program unless invalidated

manual:
  display the same corridor/pose gates
  refresh short suffix if player departs
~~~

This prevents the planner from moving its own staging target every frame while the controller is trying to converge onto it.

## Relationship to moving hazards

Dynamic worlds still require continuous observation. The important distinction is:

~~~text
new observation every frame != full new plan every frame
~~~

The monitor asks whether the accepted trajectory remains inside its proven uncertainty/safety envelope. Only a violated prediction or newly relevant conflict wakes the local planner.

For extremely fast-changing encounters, accepted segment duration naturally becomes shorter. Replanning cadence therefore emerges from trajectory validity, not from render/fixed-frame frequency.

## Execution versus planning products

A mature automatic trajectory product is an **accepted maneuver program**, not just a target point/velocity.

It should retain:

~~~text
trajectoryRevision
goalRevision
acceptedAt
validUntil
source topology/space revision
vehicle capability revision

time-parameterized reference state:
    P(t)
    V(t)
    q(t) / body basis(t)
    omega(t)

feed-forward control proved with that same trajectory:
    A_ff(t)
    alpha_ff(t)

tracking envelope / reserved feedback authority
collision/clearance witness
terminal state / next handoff
~~~

The program may be analytic or a small fixed-capacity set of control knots. It must remain bounded.

The follower samples **this same proved program** and adds only bounded feedback correction around it. It must not regenerate a new velocity trajectory from a target point.

~~~text
A_command = A_ff + bounded tracking correction
alpha_command = alpha_ff + bounded tracking correction
~~~

Large deviation invalidates the accepted maneuver and wakes replanning.

The objective owner chooses the mission destination. The route layer chooses topology/corridor. The maneuver planner compiles the next physical program. The follower tracks it. The propulsion allocator chooses actual actuators. Physics remains final authority.

See `NAVIGATION_COMMAND_OWNERSHIP.md`.

## Current implementation seam

`NavigationExecutionReplanPolicy` now defines the scheduling decision independently of the route/trajectory algorithm.

It produces:

~~~text
None
LocalHorizon
FullRoute
~~~

and distinguishes automatic continuation from manual guidance-only refresh.

The current NavigationRuntimeLab still invokes `NavigationRuntimePlanner::plan` in its fixed-step path. That is now explicitly considered transitional behavior. The next integration slice must insert an accepted-segment/follower seam so this lab proves `plan count << execution tick count` during stable automatic flight.

## Acceptance targets

~~~text
AUTO stable segment:
    many physics/execution ticks
    zero planner calls until completion/invalidation

AUTO new hazard:
    immediate LocalHorizon replan
    global route preserved

AUTO capability damage:
    immediate LocalHorizon replan

MANUAL inside corridor:
    periodic local guidance refresh only

MANUAL exits corridor:
    immediate LocalHorizon refresh

MANUAL invalidates topology branch:
    FullRoute replan
~~~

## Invariants

~~~text
execution tick != planning tick
monitoring tick != planning tick
manual corridor != autopilot authority
manual local deviation != full route rebuild
known predicted motion != automatic replan
new invalidating evidence -> replan
automatic accepted trajectory remains authoritative until invalidated
~~~


### Target-machine result after WORLD -> MAP correction

Target-machine run against
`60da8fc21a458b481894f3edebb266c02ed8d2be` produced:

~~~text
architecture contract: FALSE NEGATIVE
full MinGW client/server build: PASS
EliteServer --self-test-navigation: FAIL on exact-static entity 28
~~~

Architecture-check failure:

~~~text
[FAIL] accepted automatic segment must be monitored against exact-static geometry without restoring per-frame planning
~~~

Root cause of that check is textual/stale only: the architecture script still
requires the retired helper name `exactExecutionBlocked`, while production
code now intentionally uses `exactExecutionSegmentBlocked`. The build and
live runtime both compile/use the new helper. The gate must be updated rather
than reverting production code.

Live witness:

~~~text
segment_revision=129
planner_status=AdjustedClear
last_replan_reason=StaticSafetyInvalidated
static_invalidations=3
previous_monitor_blocker=28
previous_executed_forecast_blocked=1

velocity=(-9.77789,-2.20347,+6.24257)
ideal_accel=(-16.0268,-6.93006,-42.1738)
executed_accel=(-16.7456,-7.03306,-41.9582)
~~~

The WORLD -> MAP correction is validated by the witness: ideal follower and
actually executed acceleration now agree closely in the same NavigationMap
frame. Coordinate-frame mismatch is no longer the cause of the entity-28
impact.

The remaining failure is dynamic viability. The monitor detects the obstacle
and wakes the local planner, but `AdjustedClear` continues to optimize route
progress while the ship still has finite momentum toward the wall. Replanning
alone cannot instantaneously remove velocity. Static-safety invalidation must
therefore be able to select a short active recovery maneuver (brake/escape)
before returning to ordinary progress.

No Stage-12 baseline promotion. Last target-machine accepted baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~


## Canonical coordinate-frame contract

Stage-12 local navigation has only one translational calculation frame.

~~~text
GLOBAL / SYSTEM
    large-scale world/orbit storage and inter-system composition
            |
            | one explicit boundary transform
            v
LOCAL INERTIAL / TACTICAL
    NavigationMap
    NavigationSpace
    planner
    AcceptedShortSegment
    TrajectoryFollower
    execution safety monitor
    local collision prediction
~~~

Hull/body axes are not a third translational navigation frame. They exist for
attitude and thruster/capability resolution only.

In the current hub fixture the NavigationMap working basis is the tactical
local basis:

~~~text
local X = hub normal
local Y = hub radial
local Z = -hub prograde
~~~

The words `map`, `tactical local` and local navigation coordinates must not
be treated as separate physical systems of reference. `NavigationMap::WorkingFrame`
is a representation/boundary descriptor for that one local frame.

Rules:
- planner/monitor state `P/V/A` must all be in the same local frame;
- no WORLD-space acceleration may be combined with local position/velocity;
- GLOBAL/WORLD conversion is allowed only at explicit simulation boundaries;
- body-local vectors are allowed only inside attitude/thruster allocation;
- field names that historically contain `Map` after a world conversion are
  legacy naming debt and must not be trusted as frame proof;
- new navigation code should prefer a single local-frame transform helper over
  ad-hoc dot-product conversions scattered through runtime code.

This contract exists specifically to prevent representation names from
multiplying into accidental extra coordinate systems.
