# Navigation v2 — trajectory, vehicle control, docking and NPC pilot model

**Status:** planned contract for the trajectory-aware stage; not yet implemented  
**Updated:** 2026-09-17 Europe/Kyiv  
**Parent architecture:** `NAVIGATION_WORLD_V2.md`

## Why this document exists

`LocalHorizonPlanner` and the first `LocalAvoidancePlanner` deliberately solve a simpler problem: identify nearby dynamic risk and choose a geometrically plausible temporary target without blocking the frame.

They are **not yet a six-degree-of-freedom flight simulator**.

Current local navigation represents the controlled ship by P/V/A plus `radiusMeters`. `NavigationMap` likewise publishes dynamic actors with a radius and a conservative swept sphere. This is safe and cheap for broadphase/receding-horizon filtering, but it does not yet model:

- hull length/width/height or current hull attitude;
- oriented swept hull volume;
- angular velocity / angular acceleration;
- body-axis thrust authority or individual thruster layout;
- time required to rotate the hull before strong braking/thrust;
- assisted `Elite`-style control versus Newtonian free-flight behavior;
- oriented passage through non-circular apertures;
- moving/rotating docking targets and terminal pose matching;
- imperfect NPC reaction and control execution.

Those belong to the next trajectory-aware feasibility layer, not to persistent static `NavigationSpace` cost and not to the compact dynamic broadphase itself.

## Layering contract

```text
NavigationSpace / NavigationMap
    coarse free-space + compact dynamic candidates
            |
            v
LocalHorizonPlanner / LocalAvoidancePlanner
    bounded hazard assessment + candidate temporary target
            |
            v
trajectory-aware maneuver feasibility
    vehicle shape + attitude + thrust/control authority
    oriented apertures + docking pose constraints
            |
            v
pilot/controller execution model
    player/autopilot/NPC skill profile
            |
            v
flight control / thruster allocation / physics
```

The local navigator proposes *where progress could go*. The trajectory-aware layer must prove *whether this particular vehicle, in its current attitude and control mode, can actually get there in time without collision and with any required terminal orientation*.

## Geometry fidelity

Use different fidelity for different jobs.

### Broadphase / mass NPC filtering

Keep a cheap conservative bound such as sphere/capsule/swept sphere. It may overestimate occupied space, but it must not underestimate it.

### Precision maneuver feasibility

The trajectory-aware layer may use an oriented body proxy such as:

- OBB for ordinary ships;
- capsule / multi-capsule for elongated craft;
- small convex compound for unusual large hulls;
- exact physics narrow-phase only when required downstream.

The important rule is that collision clearance during a maneuver depends on **orientation over time**, not only center position.

A long ship rotating near a station wall may collide with its tail even if its center point is clear. The precision layer must account for that before declaring a maneuver safe.

## Oriented apertures / narrow-passage contract

A passable opening is not always characterized by one scalar radius.

Example:

```text
flat ship + flat slot
```

A conservative sphere may say "does not fit" even when the real hull fits easily after the ship rolls into the correct attitude. Conversely, a centerline may fit while a badly oriented wing, fin or tail clips the aperture.

Therefore precision passage through a non-circular aperture must reason about **pose**:

```text
position + orientation + hull proxy
```

not only center position + radius.

The trajectory-aware layer may receive an oriented aperture/corridor description from the static-space/geometry owner and must be able to prove that the ship's swept oriented body remains inside the permitted volume for the whole passage.

Typical constraints include:

- required roll/pitch/yaw window at aperture entry;
- hull width/height projected into the aperture plane;
- clearance margin around the oriented hull;
- maximum attitude error while inside the narrow section;
- sufficient distance before the opening to rotate into the required attitude;
- sufficient distance after the opening to rotate or accelerate back toward the next maneuver state.

The planner is allowed to select an **attitude target as part of the maneuver**, for example:

```text
approach slot
    -> roll to fit
    -> stabilize attitude
    -> translate through aperture
    -> clear aperture
    -> resume normal attitude/trajectory
```

This precision proof is intentionally later and more expensive than shared sphere broadphase. Do not replace the mass-NPC broadphase with full oriented-body tests everywhere.

## Attitude and thrust authority

Trajectory feasibility must consume, directly or through the authoritative flight/physics boundary, at least the information required to model:

```text
position / linear velocity / linear acceleration
attitude
angular velocity
body dimensions / collision proxy
available linear acceleration by body axis or thruster set
available angular acceleration / rate limits
braking / reverse / lateral thrust authority
mass / inertia when the physics model requires them
active flight-control mode
```

Do not duplicate authoritative ship physics inside navigation. Navigation receives a compact capability/reachability description from the flight/physics owner.

## Flight-control modes

The project requires the trajectory-aware stage to distinguish the two intended control families instead of assuming that every ship can instantaneously redirect its acceleration vector.

### `Elite` / assisted-classic behavior

Assisted control may strongly couple commanded travel direction and hull attitude, suppress unwanted drift, and provide airplane-like steering behavior where appropriate.

A maneuver can prefer smooth nose/velocity alignment and controller-generated braking without exposing every raw thruster decision to navigation.

The exact acceleration authority still comes from the ship/controller capability contract; assisted mode is not permission to violate physical acceleration limits.

### `Newton` behavior

Velocity vector and hull orientation are independent state.

The ship may continue travelling one way while rotating another way. A strong braking or direction-change maneuver may require:

```text
coast while rotating
    -> acquire braking/thrust attitude
    -> apply thrust / flip-and-burn
    -> rotate toward the next desired attitude
```

Therefore the safe-distance calculation eventually includes rotation time and the translation accumulated during that rotation. A target change alone is not evidence that the ship can avoid a head-on collision.

The exact game-level control-mode state remains owned by flight control. Navigation must consume it; do not create a second independent `Elite/Newton` mode flag with conflicting authority.

## Airplane-like pass versus flip-and-burn

These are maneuver strategies, not merely visual styles.

The trajectory-aware layer may compare feasible strategies such as:

- smooth heading change with forward-biased thrust;
- lateral translation while maintaining attitude;
- rotate-then-thrust;
- flip-and-burn;
- emergency maximum-braking maneuver.

Which strategies are available depends on ship capabilities and active control mode. Selection can also depend on pilot/controller policy, comfort, risk and urgency.

A maneuver is accepted only after its predicted swept vehicle volume is safe against relevant static space and dynamic candidates for the bounded horizon.

## Docking is a terminal 6DoF pose problem

Docking is part of the same trajectory-aware system. It is not a special case that only aims the ship's center at a point.

A dock exposes a **docking frame** / terminal pose contract. At minimum it needs:

```text
position
orientation
mating-surface normal
reference up/roll axis
linear velocity
angular velocity
optional linear/angular acceleration when material
capture tolerances
```

The ship's docking interface exposes its own body-local docking frame.

A successful docking approach must converge both translation and rotation:

```text
relative position      -> capture position tolerance
relative linear speed  -> capture speed tolerance
relative attitude      -> orientation tolerance
relative angular rate  -> capture angular-rate tolerance
```

### Bottom-to-bottom / no upside-down docking

The project requires a defined top/bottom orientation for both ship and dock.

Do not encode this as a vague visual convention. Each docking interface has a local mating frame:

```text
matingNormal
referenceUp / rollReference
```

At contact/capture:

- the two mating-surface normals are anti-aligned because the physical faces point toward each other;
- the configured roll/up reference is aligned according to the port convention;
- therefore a ship cannot satisfy docking merely by reaching the point while rolled 180 degrees.

For the required `bottom of ship -> bottom of dock` case, the docking-port metadata defines that pairing explicitly. The terminal pose solver then aligns the two interface frames rather than guessing from global world-up.

This also supports future side/top ports without rewriting the solver.

## Moving and rotating docks

A dock may translate, rotate, or both. Navigation must therefore target the **predicted docking frame at capture time**, not the current world-space port point.

For a port offset `r` from the dock/station reference origin, its instantaneous linear velocity includes rotational motion:

```text
v_port = v_origin + omega x r
```

and the trajectory layer may also need angular/linear acceleration over the bounded docking horizon when those terms are significant.

The docking solution is naturally expressed in the dock's moving local frame:

```text
ship world pose
    -> transform to predicted dock-local frame
    -> reduce relative position / velocity
    -> reduce relative attitude / angular rate
    -> enter capture corridor
    -> match terminal docking frame
```

A rotating dock therefore does not require the ship to be globally stationary. At capture, the ship must match the dock port's local translational and angular motion closely enough to satisfy the docking tolerances.

Examples:

- translating carrier: match carrier/port linear velocity before capture;
- rotating station rim: match the port's tangential velocity and required orientation;
- rotating docking collar: track the collar attitude and angular rate during final approach;
- moving + rotating dock: solve the combined time-varying terminal pose.

## Docking approach phases

The exact controller may evolve, but the architecture should support distinct phases such as:

```text
coarse intercept
    -> moving-dock rendezvous
    -> approach corridor acquisition
    -> attitude/roll alignment
    -> relative velocity reduction
    -> terminal pose tracking
    -> capture / latch
```

A phase may fail closed and back out if pose/velocity tolerances cannot be maintained.

The same hull-orientation rules used for flat apertures also apply to a narrow docking tunnel: a craft may need to roll first, remain within an attitude corridor while entering, and only then converge to the final docking-frame orientation.

## NPC pilot skill model

NPC piloting quality must not be represented internally by one magical scalar that directly changes collision equations.

A high-level game-facing skill level may map to an explicit `PilotSkillProfile` with parameters such as:

```text
reactionDelaySeconds
perceptionDecisionRateHz
commandLatencySeconds
inputSlewRate / controlSmoothness
closedLoopGain
closedLoopDamping
overshoot tendency
anticipation / look-ahead quality
deterministic control noise / precision error
emergency response threshold
risk / comfort preference
```

The profile affects **when and how well commands are executed**, while vehicle capability remains a separate physical truth.

### Example: FPV-style oscillation

A low-skill pilot can be intentionally under-damped:

```text
altitude too low
    -> late strong up command
    -> overshoot too high
    -> late strong down command
    -> overshoot low again
    -> oscillation
```

This should emerge from delayed/over-aggressive control response, not from directly adding random position errors.

Deterministic seeded noise may be used where variation is desired so tests and replay remain reproducible.

The same model may affect docking quality. An inexperienced NPC can acquire the docking corridor late, over-correct roll/yaw, oscillate around the centerline, require a go-around, or in a sufficiently unsafe situation collide. Dock geometry and capture tolerances remain truthful.

## Can an NPC actually crash because it is bad?

Yes, if the game wants that behavior.

The architecture should allow three distinct cases:

1. **Ideal autopilot:** minimal reaction delay, well-damped control, trajectory feasibility respected closely.
2. **Ordinary NPC pilot:** realistic reaction/control limits; usually safe but can make visibly imperfect corrections.
3. **Poor/stressed/damaged pilot:** delayed or under-damped execution can consume the available safety margin and may leave no recoverable maneuver, producing a genuine collision rather than a scripted random crash.

Other ships still observe the NPC's actual published P/V/A and conservative swept bounds. They do not assume that another pilot will execute its intended maneuver perfectly.

## Safety and game feel are separate knobs

Do not fake poor piloting by corrupting static free-space truth or shrinking actor dimensions.

Keep these separate:

```text
world truth          geometry / obstacles / actual actor state
vehicle capability   thrust / rotation / hull / damage
navigation intent    route / temporary target / avoidance choice
docking contract     moving terminal frame / aperture / capture tolerances
pilot skill          delay / precision / damping / anticipation
control execution    actual force/torque commands
```

This separation allows one ship type to feel radically different under an expert pilot, an inexperienced NPC, assisted `Elite` control, or raw Newtonian control without duplicating navigation worlds.

## Planned acceptance order

Do not implement this whole document inside the current same-region avoidance slice.

After current `LocalAvoidancePlanner` behavior and performance gates:

1. introduce a backend-neutral vehicle maneuver/capability input;
2. pin `Elite`-assisted versus `Newton` reachability fixtures;
3. add rotation-time-aware braking and head-on/crossing maneuver feasibility;
4. add oriented/compound swept-body precision checks, including a flat-ship/flat-slot fixture;
5. add terminal-pose docking fixtures: stationary dock, translating dock, rotating dock and moving+rotating dock;
6. pin docking orientation semantics, including the required bottom-to-bottom port alignment and rejection of a 180-degree rolled approach;
7. add deterministic `PilotSkillProfile` execution fixtures, including under-damped free-flight and docking oscillation cases;
8. then integrate the accepted trajectory/control/docking product into live `EliteGame` / `EliteServer` and guidance visualization.

Until those gates exist, current `radiusMeters` / swept-sphere local navigation remains a conservative reference rather than a claim of final ship-motion or docking fidelity.
