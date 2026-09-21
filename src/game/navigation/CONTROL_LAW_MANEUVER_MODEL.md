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

For the current Cobra-class propulsion model this additionally means:

~~~text
main engine = aft-only in BOTH control laws
negative longitudinal acceleration demand != hidden fore main engine
if bounded RCS cannot supply the requested braking vector:
    rotate hull until aft main can contribute
    then burn
~~~

The difference between Assisted and Newtonian is therefore controller doctrine
and velocity/attitude coupling, not a different imaginary propulsion set.

### Newtonian

~~~text
velocity vector and hull attitude are independent
main engine is forward-only
small RCS handles limited lateral/reverse correction
large braking requires hull rotation then main-engine burn
~~~

For a main-engine-dominant craft this is also the default ordinary course-change model. A material lateral delta-v is not requested as if the ship had an omnidirectional main engine.

A Newtonian turn does **not** imply stopping before the turn. The ship normally preserves useful inertial velocity while attitude changes ahead of the required delta-v.

~~~text
substantial course change:
    keep useful current V
    -> lead-rotate hull toward the future acceleration / delta-v vector
    -> begin main-engine burn while V remains non-zero
    -> bend V continuously toward the desired route
    -> coast / RCS trim
    -> lead-rotate toward the next burn or flip-and-burn when braking is required
~~~

Stopping is only one maneuver candidate when geometry/doctrine makes it advantageous.

The planner must account for the finite time required to acquire thrust attitude:
angular acceleration/speed + pilot reaction/latency determine how early hull rotation must begin.
RCS is normally precision authority: trim, close formation, docking, parking, portal capture, low-speed centering and small residual velocity cleanup. A craft or drone whose actual propulsion profile makes omnidirectional thrusters primary is allowed to use them as primary translation; the maneuver generator follows the real vehicle profile rather than a hard-coded ship assumption.

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
ProjectedBypass
BankedProjectedBypass
SpeedAdjustedBypass
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

## Unexpected-obstacle visible-horizon bypass

Unexpected local obstacles are not a second global-routing problem.

The accepted route/trajectory already contains the known static world. The
local emergency layer consumes only a bounded physical horizon around the
currently accepted trajectory.

Its canonical construction is:

~~~text
accepted trajectory tangent
        |
        v
plane normal to the trajectory
        |
        +-- project predicted moving-obstacle swept occupancy
        +-- apply known exact-static corridor constraints
        |
        v
search reachable lateral/vertical metric offsets
        |
        v
prove temporary bypass
        |
        v
merge back to the original accepted trajectory
~~~

The search space is expressed in **meters of offset**, not angular rays. There
is no 15/30/45/60/75-degree fan, no persistent left/right branch identity and
no branch-switch state.

Longitudinal speed is part of maneuver compilation. A vehicle may slow enough
to make a safe offset reachable, but stopping is not the normal way to choose a
different side of an obstacle.

If projected local free space is exhausted, the local layer reports:

~~~text
LocalBypassExhausted
~~~

That means only that no safe bounded bypass was demonstrated inside the current
physical horizon. Higher ownership may then reduce speed, choose a different
local waypoint/portal, backtrack, or invoke an emergency/contact-expected
maneuver. A full stop is one possible fail-safe candidate, not the default
local avoidance algorithm.

### Control-law-specific physical realization

The geometric bypass target is shared, but its physical realization remains
control-law-specific.

Assisted may use aligned lateral/vertical motion, banked turns and speed
adjustment.

Newtonian may preserve inertial velocity while rotating, trim with RCS, drift
through the free projected region, or rotate/burn to reacquire the merge
trajectory.

Every accepted physical candidate still passes capability and continuous
geometry proof.

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
geometric free-space ray != executable maneuver
route/segment feasibility always includes the current vehicle capability
a propulsion allocator is not allowed to rescue an impossible planner request

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


## Variable-speed route invariant

Speed is not a flight-style constant and is not required to remain uniform along a route.

The maneuver-authoring contract is:

- START speed and FINISH speed constrain only their boundary states unless a caller
  explicitly publishes a wider local speed restriction.
- Intermediate speed is free to vary within vehicle capability, geometry, control-law
  authority, mission constraints and safety proof.
- A speed reduction requires a concrete reason local to the maneuver:
  - curvature / available lateral acceleration;
  - braking distance for an upcoming required state;
  - obstacle clearance / swept-hull feasibility;
  - explicit local speed restriction;
  - docking / formation / placement precision;
  - tracking recovery;
  - emergency avoidance.
- A difficult bend may legally require a large slowdown or a complete stop. A stop is a
  valid maneuver candidate, not a planner failure.
- A clear segment may legally be flown substantially faster than the requested terminal
  speed if the ship can still satisfy all later constraints.
- A local speed constraint must not silently become a global cruise cap on unrelated
  route segments.
- In the absence of a physical, geometric, mission or safety reason, the planner must not
  invent braking merely to make the speed profile numerically uniform.

This rule is independent of FlightStyle. STANDARD / EXTREME may influence which
clearance/risk candidate is preferred, but neither style owns a nominal speed.

Current implementation note: exact moving terminal speed is already treated as a terminal
boundary rather than a global cap. The current scalar multi-point path solver still
collapses curvature into a worst-case route-wide speed ceiling; localizing curvature and
other local speed limits is the next trajectory-authoring correction required by this
contract.
