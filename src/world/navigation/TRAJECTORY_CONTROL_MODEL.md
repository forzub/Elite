# Navigation v2 — trajectory, vehicle control and NPC pilot model

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
            |
            v
pilot/controller execution model
    player/autopilot/NPC skill profile
            |
            v
flight control / thruster allocation / physics
```

The local navigator proposes *where progress could go*. The trajectory-aware layer must prove *whether this particular vehicle, in its current attitude and control mode, can actually get there in time without collision*.

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
4. add oriented/compound swept-body precision checks where sphere broadphase is too conservative;
5. add deterministic `PilotSkillProfile` execution fixtures, including a deliberately under-damped oscillation case;
6. then integrate the accepted trajectory/control product into live `EliteGame` / `EliteServer` and guidance visualization.

Until those gates exist, current `radiusMeters` / swept-sphere local navigation remains a conservative reference rather than a claim of final ship-motion fidelity.
