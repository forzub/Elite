# Navigation command ownership and accepted maneuver API

**Status:** architecture contract / active Stage-12 correction
**Updated:** 2026-09-19 Europe/Kyiv
**Related:** `NAVIGATION_PIPELINE_AUDIT.md`, `NAVIGATION_BEHAVIOR_CHARACTER_MODEL.md`, `CONTROL_LAW_MANEUVER_MODEL.md`, `TRAJECTORY_EXECUTION_REPLAN_MODEL.md`

## Core ownership question

Navigation must distinguish three different questions:

~~~text
WHERE must the vehicle eventually go?
WHAT physically valid maneuver should it execute next?
HOW accurately can the current pilot/controller/actuators execute that maneuver?
~~~

They belong to different owners.

## 1. Objective owner — chooses the destination/mission contract

The mission/gameplay/AI task layer owns the semantic objective.

Examples:
- reach point/region;
- dock to terminal pose;
- form up relative to a moving ship;
- intercept;
- attack;
- escape;
- enter/retrieve through a breach;
- return a repair part to a socket.

It publishes a `NavigationObjective`-like contract, not thruster commands.

The objective may contain:

~~~text
goal position / region
goal linear velocity or relative velocity
optional terminal attitude / roll
position / velocity / attitude tolerances
deadline / urgency
target reference frame
mission semantics
selected Situation/Doctrine profile
~~~

The objective owner does **not** choose local avoidance geometry or engine pulses.

## 2. Global route owner — chooses topology/corridor

The global/static route layer chooses the reachable sequence of regions, portals and passages.

Its output is a corridor/topology product:

~~~text
region path
portal sequence
portal/aperture geometry
coarse traversal constraints
remaining route branch
~~~

It does not command the ship.

A portal is a topological/aperture opportunity, not automatically a capture instruction.

## Portal rule: ask whether the passage is safe under the current behavior model

The wrong question is:

~~~text
portal exists -> must align to portal normal
~~~

The correct question is:

~~~text
given:
    actual opening geometry / motion
    current P/V/q/omega
    actual vehicle capability
    pilot execution uncertainty
    Situation/Doctrine

which physically valid passage maneuver is acceptable?
~~~

At minimum generate two conceptual classes when geometry permits:

### FreeTransit

Use when the current or gently adjusted trajectory can cross the portal safely.

Example: Ordinary/Rational, large opening, low relative velocity, generous clearance.

~~~text
keep useful velocity
no unnecessary stop
no forced centerline capture
no forced hull-to-normal alignment unless actual fit requires it
small trim only if useful
~~~

A wide safe portal should behave almost like ordinary free space.

### PrecisionCapture / PrecisionTransit

Use when safe traversal requires controlled acquisition:

~~~text
narrow opening
tight hull fit
large cross-track velocity
large relative motion
required roll/attitude
docking/retrieval semantics
Precision doctrine
poor pilot / large execution uncertainty
~~~

Then staging, speed reduction, centerline acquisition, attitude alignment or near-stop may be correct.

### ExtremeTransit / ContactExpected

Extreme/attack/escape may accept:
- smaller clearance;
- higher transit speed;
- higher load;
- smaller execution reserve;
- obstacle masking / cover preference;
- survivable glancing contact or expendable external-component loss when explicitly allowed.

Hard vehicle feasibility remains mandatory. Doctrine can change accepted risk, not invent capability.

## 3. Maneuver planner — compiles route intent into a physical short program

The maneuver planner owns the next bounded physical solution.

Input:

~~~text
current P/V/A/q/omega
selected route/corridor/portal opportunity
dynamic influence list
static exact geometry
vehicle hull / propulsion / angular capability / damage
control law
Situation/Doctrine
pilot execution uncertainty envelope
~~~

Output is **not** just a point and not just a velocity target.

Output is a bounded, time-parameterized vehicle-level maneuver program.

## AcceptedManeuverProgram — target API

Conceptually:

~~~text
AcceptedManeuverProgram
    identity / revisions / validity
    maneuver family / doctrine provenance

    time domain [t0, t1]

    reference state:
        P(t)
        V(t)
        A_ff(t)

        q(t) or body basis(t)
        omega(t)
        alpha_ff(t)

    terminal state / tolerance
    tracking envelope
    collision/clearance proof witness
    capability revision
    world/map/space revisions
~~~

A bounded implementation may store analytic segments or a small fixed-capacity set of knots/control keys. It must not require an unbounded per-frame path array.

### Why include both trajectory and feed-forward control?

A trajectory alone says where the ship should be but forces the executor to re-solve the maneuver.

A raw acceleration program alone loses the reference state needed to detect tracking error and recover from pilot/physics deviations.

Therefore the accepted product should contain both:

~~~text
reference trajectory state
+
the feed-forward acceleration/attitude program that was actually proved
~~~

The proof and execution must refer to the same program.

## The planner does not command individual thrusters

The accepted program is vehicle-level, not hardware-bit-level.

It specifies:
- desired body attitude;
- feed-forward proper/linear acceleration;
- feed-forward angular acceleration;
- optional maneuver-family/control-allocation semantics for diagnostics/policy.

The flight-control/propulsion allocator maps that to actual main engine, RCS and torque actuators using the current physical vehicle model.

The planner's feasibility proof must use the same capability semantics as the allocator so the allocator is not asked to rescue an impossible request.

## 4. Trajectory follower — closes the loop around the accepted program

The follower must **not**:
- choose a new route;
- choose a new portal;
- invent a new target velocity from a point;
- re-solve free-space geometry.

It samples the accepted program at current elapsed time:

~~~text
P_ref, V_ref, A_ff
q_ref, omega_ref, alpha_ff
~~~

and compares against actual state.

It produces bounded feedback correction:

~~~text
A_cmd = A_ff + A_feedback
alpha_cmd = alpha_ff + alpha_feedback
~~~

where feedback is limited by the tracking envelope and the already-proved authority reserve.

Large divergence invalidates the accepted maneuver and requests replan; the follower does not silently redesign the trajectory.

## 5. PilotSkill — models execution quality

PilotSkill operates on the ideal program/control request.

It owns:
- reaction delay;
- decision cadence;
- latency;
- damping/overshoot;
- slew/aggressiveness;
- deterministic precision error.

Planning uses a conservative uncertainty envelope derived from the pilot profile; execution then produces the actual delayed/imperfect command.

Impact/blast/stress may temporarily degrade that profile.

PilotSkill does not choose route geometry.

## 6. Propulsion allocator / flight control — chooses physical actuators

Input:
- desired vehicle-level linear/angular command;
- current body attitude/state;
- actual remaining actuator capability.

Responsibility:
- allocate main-engine / RCS / torque authority;
- clamp to real limits;
- never fabricate thrust.

Output:
- actual actuator/force/torque command consumed by physics.

## 7. Physics — final authority

Physics integrates the actual force/torque and contact response.

Navigation never overrides physics truth.

## Newtonian course changes do not imply stopping

A Newtonian turn is normally a continuous velocity-vector change.

The planner should use current momentum and rotate the hull **ahead of the required delta-v**.

Typical main-engine-dominant sequence:

~~~text
coast on current V
    +
lead-rotate hull toward future required acceleration vector
    ->
begin main-engine burn while V is still non-zero
    ->
velocity vector bends continuously toward desired route
    ->
continue/trim/coast
    ->
rotate early toward the next burn or braking vector
~~~

Stopping is only one candidate, appropriate when doctrine/geometry favors it.

### Lead rotation

Because attitude has finite angular acceleration/speed and the pilot has reaction/latency, the planner must account for orientation lead time.

Conceptually:

~~~text
required future delta-v direction
        ↓
time required to acquire thrust attitude
        ↓
start rotating before the geometric turn point
        ↓
main burn begins when useful projection is available
~~~

For a Newtonian ship:

~~~text
hull forward direction
    !=
instantaneous velocity direction
~~~

and that separation is a normal maneuver, not an error.

## Main-engine/RCS semantics

For a main-engine-dominant ship such as the current Cobra lab actor:

~~~text
large delta-v / route bending
    -> orientation + main engine

small residual correction
portal centering
docking / parking
formation trim
fine low-speed translation
    -> RCS / manoeuvre thrusters
~~~

A drone with strong omnidirectional propulsion may have a different physical profile and therefore a different valid maneuver family.

## Transitional implementation warning

The current `AcceptedShortSegment` is not yet this target API.

Today it stores a mixture of:
- target position;
- target velocity;
- optional fixed acceleration;
- optional forward alignment.

Then `TrajectoryFollower` re-derives acceleration from target velocity.

That is transitional and insufficient for fully proven Newtonian maneuvers because execution can differ from the trajectory that was proved.

Migration target:

~~~text
current:
    target point/velocity
        -> follower derives command

target:
    accepted proven maneuver program
        -> follower samples same program + bounded feedback
~~~

## Acceptance invariants

~~~text
objective owner chooses mission target, not local thrust

global route chooses topology, not commands

portal existence != mandatory capture

portal passage mode is selected by
    geometry + vehicle + pilot uncertainty + doctrine

free safe opening may be crossed without stopping/alignment

maneuver planner outputs the physically proved short program

accepted program contains reference state + feed-forward control

follower tracks; it does not re-plan

propulsion allocator chooses actuators; planner does not bit-drive thrusters

Newtonian turn != stop-turn-go

main-engine-dominant Newtonian course changes may rotate early
while preserving non-zero inertial velocity

proof and execution must consume the same maneuver program


## Execution safety reflex boundary — 2026-09-19

The two-world architecture keeps only two top-level owners: Planner and Autopilot/Follower. The execution world may contain a bounded safety supervisor/reflex, but this does not grant the follower ordinary route-planning ownership.

Allowed:
- track the accepted `P/V/A/q/omega/alpha` program;
- use reserved feedback authority to reduce bounded tracking error;
- continue useful program progress while cross-track/state error decays;
- make a bounded imminent-hazard reflex when waiting for a new plan would be unsafe.

Required after a material reflex:
- mark the accepted program invalid;
- request immediate local replan;
- do not silently invent a new long-lived target/route and call it tracking.

This preserves the command ownership rule:

```text
Planner chooses/proves the maneuver.
Follower tracks it.
Safety reflex may prevent immediate loss.
Material deviation returns authority to Planner.
```

The world is authoritative and already known to navigation. Ray/segment/sweep tests inside planning or monitoring are geometric intersection/proof tools, not obstacle-perception sensors.
