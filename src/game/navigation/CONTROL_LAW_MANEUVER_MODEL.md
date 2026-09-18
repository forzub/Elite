# Local control-law maneuver model

**Status:** architecture contract / Stage-12 active integration
**Updated:** 2026-09-18 Europe/Kyiv
**Related:** `src/game/MANEUVER_DECISION_TREE.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`, `src/game/navigation/LocalFlightControlLaw.h`

## Purpose

Elite has two materially different local control laws:

~~~text
LocalFlightControlLaw::Assisted
    = Elite-classic / airplane-like assisted flight

LocalFlightControlLaw::Newtonian
    = inertial velocity + independently rotatable hull
~~~

They share world geometry and route intent, but they do not share the same maneuver library.

Navigation must separate:

~~~text
where can a body physically pass?
what maneuver families can this control law execute?
which maneuver should the game/controller choose?
~~~

## Geometry truth versus conservative maneuver envelope

The physical ship never becomes a sphere. Exact collision truth remains the real oriented HitVolume / hull geometry.

Different conservative proxies answer different planning questions.

### Assisted / Elite-classic proxy

For ordinary and precision passage planning, use an oriented hull proxy:

~~~text
vehicle OBB (brick)
    aligned with commanded/required vehicle attitude
~~~

Because assisted flight strongly couples travel direction to vehicle forward, an aperture question is primarily:

~~~text
is there an allowed vehicle pose/roll
for which the oriented brick fits the opening?
~~~

The continuous verifier still checks the swept oriented hull while turning. A single static OBB test is only the cheap first fit test.

### Newtonian free-rotation proxy

For the coarse question:

~~~text
is there enough free volume to rotate the craft arbitrarily,
including a 180-degree flip for main-engine braking?
~~~

use the conservative rotation sphere:

~~~text
radius = maximum distance from center of mass to hull geometry
~~~

If that sphere is clear, the craft can rotate anywhere inside it without the coarse planner knowing the exact intermediate attitude.

This sphere is not the final narrow-passage shape.

For an actual constrained pass the Newtonian precision layer uses the time history of the oriented hull:

~~~text
p(t), v(t), q(t), omega(t)
        ->
continuous swept OBB / exact HitVolume proof
~~~

That preserves valid maneuvers such as passing a slit sideways, rolling edge-on, drifting through while the nose points elsewhere, entering while rotating, and flip-and-burn immediately after clearing an obstacle.

## Propulsion semantics

### Assisted / Elite-classic

~~~text
velocity approximately follows vehicle forward direction
controller suppresses large persistent slip
braking / target-speed reduction is a normal assisted action
~~~

Navigation may request slow approach, forward turn, roll to fit, go-around, assisted stop and assisted reverse/backtrack where the controller physics allows it.

Assisted control is not permission to invent thrust; all linear/angular authority remains physical.

### Newtonian

~~~text
velocity vector and hull attitude are independent
main engine is forward-only
small RCS handles limited lateral/reverse correction
large braking requires hull rotation then main-engine burn
~~~

A strong stop is therefore:

~~~text
coast
 -> rotate tail toward velocity (~180 deg)
 -> acquire braking attitude
 -> main-engine burn
 -> optionally rotate toward the next travel attitude
~~~

The ship may keep inertial velocity while changing orientation for passage fit or threat silhouette.

## Maneuver libraries

The route planner does not issue an arbitrary acceleration vector and assume the flight law can realize it. It constructs candidates from a control-law-compatible family.

### Assisted candidate families

~~~text
DirectForward
VisibilityTurn
BankedVisibilityTurn
BrakeAndTurn
PrecisionAlignedPass
GoAround
AssistedStop
AssistedReverseOrBacktrack
EmergencyGlancingPass
~~~

### Newtonian candidate families

~~~text
Coast
CoastAndRotate
DriftPass
SideOnPass
RollThroughPassage
RcsTrim
TurnThenBurn
FlipAndBurn
BurnThenTurn
ReverseEscape
Precision6DoFPass
EmergencyGlancingPass
~~~

These are candidate generators, not scripted animation names. Every accepted candidate still passes real capability and continuous geometry checks.

## Ordinary bounded visibility remains cheap

First local attempt:

~~~text
direct A -> B
15 deg
30 deg
45 deg
60 deg
75 deg
~~~

This is intentionally the cheap, progress-preserving local layer.

The 75-degree limit is not a vehicle capability limit. It separates ordinary local bypass from recovery / retreat / extreme maneuver selection.

## No-safe-progress escalation

Failure of the ordinary 0..75-degree visibility fan must not map directly to permanent Hold.

Instead it produces:

~~~text
NoSafeProgressInOrdinaryFan
~~~

and triggers bounded recovery candidate generation.

### Common recovery candidates

~~~text
1. wider local turn / escape sector (>75 deg)
2. known backtrack corridor / previous viable portal
3. safe stop/brake, if physically reachable
4. retreat / reverse-sector candidate
5. mandatory precision passage candidate
6. emergency contact-expected candidate
~~~

The exact ordering is doctrine-dependent and belongs to ManeuverDecisionController.

### Assisted recovery

Assisted may generate wider forward-turn and controlled low-speed recovery candidates first.

If progress is not mandatory:

~~~text
brake -> turn -> reacquire route
~~~

If progress is mandatory:

~~~text
wider turn / go-around / reverse-sector
 -> precision passage
 -> contact-expected emergency
~~~

### Newtonian recovery

Newtonian has additional useful choices because attitude and velocity can separate:

~~~text
coast while rotating away from blocker
RCS trim while preserving inertial velocity
sideways passage
hard 90..150 deg attitude change while still drifting
180 deg flip-and-burn
burn into a retreat vector
rotate for minimum threat silhouette while velocity stays on escape line
~~~

A Newtonian reverse maneuver is never modeled as instant negative velocity. It is a bounded turn/burn trajectory.

## Optimal Newtonian trajectory implementation

A general nonlinear optimal-control solve is possible, but is not the default per-frame game algorithm.

Recommended implementation:

~~~text
bounded motion-primitive generation
        |
        v
small parameter search
(time / entry speed / burn duration / attitude / roll)
        |
        v
continuous 6DoF trajectory verification
        |
        v
candidate annotations
(time, clearance, exit speed, contact severity, escape reserve)
        |
        v
ManeuverDecisionController
~~~

For a main-engine-dominant craft, useful primitive sequences include coast, coast+rotate, rotate+burn, flip+burn, burn+rotate, drift+roll, side-on passage, and entry-alignment+transit+exit-burn.

The existing continuous trajectory evaluators already provide much of the verification machinery: linear capability, angular capability, oriented hull, continuous interval geometry and assisted slip constraints.

A later optimizer may replace the parameter search without changing ownership.

## Doctrine interaction

Control law answers what is physically/maneuverably available. Doctrine answers which available maneuver is desirable.

~~~text
Rational + Assisted
    brake early, align brick, pass with margin

Rational + Newtonian
    preserve escape reserve, flip-and-burn if required, avoid contact

PrecisionRetrieval + either law
    slow down, acquire aperture frame, maximize clearance

Extreme + Assisted
    aggressive minimum-time forward turn / narrow pass

Extreme + Newtonian
    drift, side-on pass, late flip-and-burn, expendable scrape if needed

CombatEscape + Newtonian
    keep useful escape velocity while rotating hull for minimum silhouette
~~~

## Acceptance invariants

~~~text
physical hull != planning sphere

Assisted:
    OBB fit + swept oriented proof
    velocity/forward coupling is a policy constraint

Newtonian:
    rotation sphere only for coarse free-rotation proof
    exact constrained passage uses time-varying oriented hull
    main-engine braking requires real attitude change

75 deg:
    ordinary visibility-search boundary
    not a terminal failure
    not a vehicle turn limit

NoSafeProgress:
    requests recovery candidate generation
    never means disable control

ManeuverDecisionController:
    chooses among already physical candidates
    never invents thrust or geometry
~~~
