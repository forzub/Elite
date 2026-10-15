# NEWTONIAN V1 PLANNING CHARTER — 2026-10-15

## Starting point

Newtonian development starts only after the Assisted R2 acceptance boundary.

Accepted Assisted baseline includes the dock-change hotfix:

1. release requested translational acceleration;
2. damp pitch/yaw/roll rate;
3. require angular speed <= 0.01 rad/s;
4. hold the settled angular state for 0.20 s;
5. only then capture the authoritative planning snapshot;
6. preserve linear velocity;
7. after snapshot capture, expensive planning may run asynchronously.

The live dock-change regression no longer reaches
`initial-omega-outside-capability` / `DockClientStartFailure`.
A retryable geometric planner failure is allowed to replan; the subsequent
route can enter execution from effectively zero hull angular rate.

## Same product functionality

Assisted and Newtonian must expose the same user-level navigation capability:

- route from current state to a selected dock/target;
- semantic traffic stages;
- BLUE/controlled-volume traversal;
- obstacle avoidance;
- mandatory portal straights;
- terminal docking approach;
- automatic mode;
- manual/HUD route presentation;
- recommended speed/corridor presentation;
- recoverable replanning where physically possible;
- the same explicit hard failures:
  - initial state physically unsalvageable;
  - ship does not fit.

The user should not lose navigation features merely because the ship uses a
Newtonian flight law.

## Shared law-neutral layer

The following may remain shared when they contain no flight-law assumptions:

- world/navigation obstacle representation;
- semantic traffic graph and ordered stage topology;
- portal/volume ownership and containment contracts;
- target/dock frame descriptions;
- law-neutral route request/result DTOs;
- route stage identifiers and progress intervals;
- HUD sampling/visualization data;
- collision-query primitives;
- diagnostics/failure vocabulary where meanings are genuinely common.

Shared does NOT mean mutable cross-law implementation state.

## Newtonian is NOT Assisted with different numbers

A Newtonian craft has a fundamentally different reachable-state set.

Assisted can request substantial acceleration independently of hull forward
direction and can brake without a 180-degree flip.

Newtonian-I is dominated by one aft main thrust axis:

- useful main acceleration is primarily along hull forward;
- main-engine braking requires turning the hull approximately 180 degrees;
- attitude consumes time and angular authority;
- acceleration direction and hull orientation are coupled;
- a braking manoeuvre changes both attitude and future thrust direction;
- coasting is a first-class flight phase;
- RCS is for fine correction, not a substitute for the main engine;
- route feasibility therefore depends on position, velocity, attitude,
  angular velocity and remaining turn/brake room together.

These are algorithmic differences, not parameter changes.

## Required Newtonian planner responsibilities

A production Newtonian planner must reason about at least:

### 1. Translational geometry

Choose collision-free spatial route geometry through free space and mandatory
semantic passages.

### 2. Velocity programme

Author feasible speed/velocity evolution along the route, including deliberate
coast, acceleration and deceleration regions.

### 3. Attitude programme

Reserve time/distance for hull rotations required by thrust direction.

The planner must never assume that desired acceleration can be produced while
the hull points somewhere unrelated.

### 4. Flip-and-brake manoeuvres

A required deceleration may need a sequence such as:

- stop accelerating;
- coast;
- begin flip early enough;
- complete/near-complete braking attitude;
- burn main engine against velocity;
- rotate again for the next acceleration/terminal attitude.

This sequence must be proven before the route is accepted.

### 5. Turn feasibility

A geometric curve is not sufficient proof.

For each curved or direction-changing portion, the planner must prove that the
ship can produce the required change in velocity with its available combination
of hull attitude, main thrust and permitted RCS.

### 6. Corridor sizing

Newtonian manoeuvres generally require more spatial reserve than Assisted.
Corridor/tunnel feasibility must use Newtonian-specific swept motion and cannot
reuse an Assisted radius merely with a multiplier.

### 7. Terminal docking

Terminal approach may require explicit phases:

- arrival/coast alignment;
- flip or braking orientation;
- velocity removal;
- final hull alignment;
- hold.

The terminal programme must be authored by Newtonian logic, not by forcing the
Assisted terminal trajectory through different parameters.

## Proposed Newtonian route product

The public route can remain compatible with the shared RoutePlan DTO for HUD,
semantic stages and spatial geometry, but Newtonian execution requires its own
law-specific manoeuvre programme.

Conceptually:

```
NewtonianPlan
├─ RoutePlan              // shared spatial/semantic view
└─ NewtonianManeuverProgram
   ├─ Coast
   ├─ RotateForThrust
   ├─ MainBurn
   ├─ LeadTurn
   ├─ FlipForBraking
   ├─ BrakeBurn
   ├─ Reorient
   └─ TerminalHold
```

Do not add Newtonian-only mutable state to Assisted route objects merely for
code reuse. A Newtonian programme may reference shared route progress/stages by
value.

## Planner split

Target architecture:

```
Shared semantic/navigation input
           │
           ├─────────────────────┐
           │                     │
AssistedPlanningPipeline   NewtonianPlanningPipeline
           │                     │
AssistedRoutePlanner       NewtonianRoutePlanner
           │                     │
Assisted optimizer         Newtonian reachability/
and dynamics               manoeuvre planner
           │                     │
Assisted route/program     Newtonian route/program
           │                     │
AssistedAutopilot          NewtonianAutopilot
```

No runtime `if (law == Newtonian)` inside an Assisted helper is an acceptable
substitute for this split.

## Current repository status

The current `NewtonianRoutePlanner` / `NewtonianPlanningPipeline` are only a
hard API boundary. They currently disable the continuous Assisted optimizer and
select the older non-rounded backend.

That is useful isolation scaffolding, but it is NOT considered the finished
Newtonian planner.

Newtonian V1 work starts by replacing that implementation behind the existing
Newtonian API without changing Assisted outputs.

## First implementation milestone

Before obstacle-rich docking, prove a pure Newtonian manoeuvre solver on simple
space:

1. known initial position/velocity/attitude/angular velocity;
2. target velocity/course;
3. no obstacles;
4. aft main only;
5. explicit angular limits;
6. produce:
   - coast/rotate/burn/flip/brake sequence;
   - predicted time;
   - predicted swept path;
   - terminal state;
7. forward-simulate and verify the predicted terminal tolerances.

Only after that solver is green should it be embedded in the Newtonian route
planner.

## Regression rule

Every Newtonian milestone must preserve the frozen Assisted regression set.

If Newtonian needs different semantics, isolate or duplicate the code. Do not
change Assisted behavior to make Newtonian easier.
