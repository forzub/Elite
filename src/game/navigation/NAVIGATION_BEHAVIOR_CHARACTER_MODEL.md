# Navigation behavior character model

**Status:** architecture contract / Stage-12 correction
**Updated:** 2026-09-19 Europe/Kyiv
**Related:** `CONTROL_LAW_MANEUVER_MODEL.md`, `../MANEUVER_DECISION_TREE.md`, `TRAJECTORY_EXECUTION_REPLAN_MODEL.md`, `../PILOT_SKILL_MODEL.md`

## Non-negotiable rule: a route must be flyable by this vehicle

Navigation is forbidden to promote a purely geometric path into an executable route or accepted segment.

~~~text
free space exists
    !=
this vehicle can fly through it from its current state
~~~

Every executable navigation product must be feasible for the actual current:

~~~text
position / velocity / acceleration
attitude / angular velocity
hull / HitVolumes
control law
main-engine authority and directionality
RCS / manoeuvre-thruster authority
angular authority
speed/load limits
damage-degraded capability
pilot execution envelope
available time / distance before the hazard
~~~

This rule applies at the correct level of abstraction.

### Global/topological route

A global route does not numerically integrate every engine pulse across the whole system. It may remain coarse, but every graph edge / portal / passage must pass a conservative vehicle-feasibility filter:

~~~text
hull can fit
required orientation is reachable
required braking/turning room exists
required control-law maneuver family exists
vehicle is not capability-blocked by damage
~~~

An edge that only a smaller, more agile or differently controlled vehicle can traverse must not be considered a valid edge for this vehicle.

### Local executable segment

A local segment is stricter. It must be time-parameterized and dynamically feasible before `ACCEPT`.

~~~text
geometry candidate
    -> physically realizable maneuver candidate
    -> continuous geometry + capability proof
    -> ACCEPT
~~~

A geometric visibility ray is only a free-space hint. It is not an executable maneuver.

## Propulsion selection is part of maneuver planning

Navigation must not request an arbitrary acceleration vector and assume propulsion allocation will somehow realize it.

For a main-engine-dominant Newtonian craft, material delta-v normally comes from hull orientation plus the main engine.

~~~text
ordinary substantial course change:
    coast / rotate
    -> acquire thrust attitude
    -> main-engine burn
    -> coast / trim
    -> rotate and/or flip-and-burn when braking is required
~~~

RCS / manoeuvre thrusters are primarily for:

~~~text
small trim corrections
velocity-vector cleanup
docking / parking
portal capture
close formation
low-speed translation
fine passage centering
attitude-coupled correction where rotating the hull is undesirable
~~~

RCS may still be the correct primary translation source for a small drone or a vehicle whose physical profile says so. The rule is not "never use RCS"; the rule is "use the real propulsion architecture and choose the maneuver that fits it."

For the current Cobra-class lab actor, a large lateral bypass must not be modeled as tens of m/s^2 of sideways acceleration while keeping the nose on the future portal. The planner should generate a turn/burn/coast/brake sequence using the main engine and reserve the 2 m/s^2 manoeuvre system for trim.

## Behavior character has two independent inputs

~~~text
behavior character =
    situation / doctrine
    +
    pilot model and transient pilot state
~~~

Vehicle capability is a third independent truth. Neither doctrine nor pilot skill may invent thrust, shrink the hull or alter collision geometry.

# 1. Situation / doctrine

Situation defines what outcomes are desirable and what risks are acceptable. It changes candidate generation bounds and ranking coefficients, not physical truth.

The initial families are:

### Ordinary / Rational transit

Intent:
- safe travel with large reserve;
- wide, comfortable deviations are acceptable;
- stopping or nearly stopping to establish a better attitude is acceptable;
- prefer clearance and smooth control over minimum time;
- avoid contact and unnecessary load.

Typical coefficients:
- high clearance reserve;
- low time urgency;
- low overload allowance;
- strong collision/contact penalty;
- low obstacle-hugging preference;
- high comfort/smoothness preference;
- stop/brake penalty near zero when it creates a safer solution.

### Extreme / attack / escape

Intent:
- preserve useful speed and tactical progress;
- minimize exposure time;
- minimize projected silhouette toward the threat when tactically useful;
- prefer cover and routes close to masking geometry when they remain physically viable;
- accept narrower clearances, higher loads and aggressive control;
- survivable glancing contact or loss of expendable external equipment may be acceptable if the alternative is materially worse.

Typical coefficients:
- high time/progress weight;
- high threat-exposure weight;
- high silhouette weight;
- positive cover/obstacle-hugging preference;
- lower clearance reserve;
- higher accepted load/overload band;
- non-zero contact allowance;
- expendable-component loss may be allowed;
- stopping carries a high penalty unless tactically necessary.

Attack and escape may share the same extreme maneuver generator while differing in objective/threat geometry.

### Precision ingress / squeeze / retrieval

Intent:
- deliberately pass through constrained geometry;
- reduce speed early;
- acquire exact aperture/portal frame;
- align hull and velocity;
- maximize fit/control margin;
- use fine RCS where it is physically appropriate;
- allow near-stop, staging and repeated capture attempts.

Typical users:
- repair drones;
- boarding/retrieval craft;
- maintenance corridors;
- docking;
- wreck / breach entry.

Typical coefficients:
- very high geometry/clearance precision;
- low speed priority;
- high alignment priority;
- high stop/stage tolerance;
- high RCS/fine-control willingness;
- contact normally forbidden unless explicitly escalated to emergency/contact-expected behavior.

More doctrines may be added, but they must be explicit profiles rather than hidden special cases.

## Doctrine coefficients

Do not collapse hard safety/capability constraints into one magic score.

First filter impossible candidates. Only then rank feasible candidates using explicit terms such as:

~~~text
time / progress
route deviation
minimum clearance
control reserve
braking reserve
fuel / delta-v
load / overload
smoothness / comfort
threat exposure
projected silhouette
cover / obstacle masking
contact severity
critical-component risk
expendable-component loss
escape reserve / returnability
~~~

A doctrine provides weights, preferences and allowed bands for these terms.

## 2. Pilot model

Pilot skill changes how safely and accurately a physically valid maneuver can be executed. It does not change vehicle capability or world geometry.

Persistent pilot traits include:

~~~text
reaction delay
decision/perception rate
command latency
control bandwidth / response frequency
damping / overshoot tendency
command slew / aggressiveness
anticipation quality
deterministic precision error / noise
hull-size / clearance judgement
spatial judgement
control-law familiarity
risk tolerance / confidence bias
~~~

### Hull / obstacle judgement

An inexperienced pilot should not make the obstacle physically larger or smaller. Planning instead expands the execution uncertainty envelope.

Example:

~~~text
physical hull radius / OBB: unchanged

expert pilot:
    small execution uncertainty margin

poor pilot:
    larger predicted tracking/clearance uncertainty
    earlier braking
    wider portal capture tolerance requirement
    lower accepted speed for the same opening
~~~

If the poor pilot still attempts an aggressive maneuver, the execution model may overshoot or react late and consume that margin.

### Stick handling / over-control

Pilot skill may alter:

~~~text
response damping
overshoot
command slew
decision cadence
noise
latency
~~~

A poor or stressed pilot can therefore visibly "saw" the controls, over-correct and need another capture attempt without any fake position teleport or random collision injection.

## Transient pilot state

Pilot quality may be temporarily degraded by events such as:

~~~text
impact / collision
blast or shock wave
injury
extreme acceleration / G exposure
fatigue
panic / stress
sensor disruption
vehicle damage that worsens control feedback
~~~

Represent this as temporary modifiers on the pilot execution/estimation profile, for example:

~~~text
reactionDelay *= factor
decisionRate *= factor
commandNoise += delta
damping *= factor
hullJudgementUncertainty += delta
anticipation *= factor
~~~

Modifiers decay/recover according to gameplay rules. They must remain deterministic for tests/replay when deterministic simulation is required.

## Correct planning chain

~~~text
WORLD TRUTH
    geometry + dynamic actors
        |
VEHICLE TRUTH
    hull + propulsion + angular authority + damage + control law
        |
GLOBAL FEASIBILITY
    which regions/portals are reachable by this vehicle
        |
SITUATION / DOCTRINE
    ordinary / extreme / precision / ...
        |
LOCAL FREE-SPACE CANDIDATES
    direct / visibility / portal / gap / recovery
        |
MANEUVER GENERATION
    control-law- and propulsion-compatible primitives
        |
PILOT-AWARE EXECUTION ENVELOPE
    latency / precision / anticipation / tracking uncertainty
        |
CONTINUOUS DYNAMIC + GEOMETRY PROOF
        |
MANEUVER DECISION
    rank only physically valid candidates under doctrine
        |
ACCEPTED TIME-PARAMETERIZED SEGMENT
        |
PILOT SKILL EXECUTION
        |
AUTHORITATIVE PHYSICS
        |
MONITOR
~~~

## Current Stage-12 defect this contract forbids

The live chain currently allows:

~~~text
LocalAvoidance geometric AdjustedClear ray
    -> desired velocity
    -> arbitrary acceleration request
    -> AcceptedShortSegment
~~~

without proving that the current craft can acquire that ray.

The same packing step can also set `alignForward` from the existence of a future portal, changing the maneuver semantics while a local bypass is active.

Both are forbidden by this contract.

A future portal may constrain the final handoff state, but it may not force the hull attitude during an earlier bypass unless the selected maneuver explicitly requires that attitude.

## Acceptance invariants

~~~text
GEOMETRICALLY CLEAR != EXECUTABLE

accepted route edge:
    must be feasible for this vehicle class/current capability

accepted local segment:
    must be dynamically/time feasible from current P/V/A/q/omega

main-engine-dominant Newtonian craft:
    substantial delta-v normally uses rotate + main burn
    RCS is not a hidden sideways main engine

doctrine:
    changes priorities / accepted risk / candidate bounds
    never changes physics

pilot:
    changes execution quality / uncertainty / reaction
    never changes geometry or propulsion capability

future portal orientation:
    may constrain the future terminal state
    must not overwrite an unrelated current bypass attitude

impact/stress:
    may temporarily degrade pilot profile
    must not corrupt world truth
~~~
