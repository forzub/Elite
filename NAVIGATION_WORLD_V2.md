# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-18 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-TRAJECTORY-1` — moving/rotating docking 6DoF, stage 9B continuous final approach candidate

`main` is the only canonical development branch.

## 1. Architecture summary

Navigation v2 uses one shared **NavigationWorld** for the active play area. It is not one complete independent world/planner per NPC.

```text
AUTHORITATIVE SYSTEM / PHYSICS STATE
            |
            v
shared ship-centered NavigationWorld
    + static NavigationSpace
    |   free space / clearance / portals / cached corridors
    |
    + dynamic NavigationMap
    |   mass P/V/A prediction / swept bounds / bins
    |   compact relevant/conflict candidates
    |
    + LocalHorizon / LocalAvoidance
            |
            v
bounded precision trajectory layer
    oriented passages / gaps
    attitude reachability
    static continuous passage
    emergency mitigation / contact severity
    moving-gap prediction
    moving continuous passage
    docking terminal / continuous approach
            |
            v
flight control / Ruckig / authoritative physics
```

Accepted hybrid ownership:

```text
CPU
    persistent static free-space / clearance
    connectivity / portals
    cached global corridor search
    deterministic local/precision reference work

GPU
    mass P/V/A prediction
    conservative swept bounds
    spatial bins
    all-agent neighbor/conflict reduction
```

No backend-specific actor tables, graph nodes or GPU buffers cross public navigation boundaries.

## 2. Different work runs at different rates

The system must not recalculate a complete route for every ship every simulation tick.

```text
physics / authoritative actor state
    every simulation tick

dynamic NavigationWorld broadphase / prediction
    every tick or near that rate, shared across actors

local horizon / avoidance
    receding horizon; may be staggered across NPCs

precision passage / emergency / docking
    only for compact conflict or authored precision candidates

global corridor
    revision/event driven, cached and reused
```

No `N ships x full global pathfinding x every tick` design is allowed.

## 3. Coordinate contract

Authoritative physical/system state and navigation working coordinates are separate.

The NavigationWorld origin may translate/rebase with the active domain. Its axes are stable navigation/travel axes and do not roll/pitch/yaw with the controlled hull. Hull-local coordinates belong to vehicle/control; render/player-relative coordinates belong to presentation.

Hub/station/carrier/interior geometry remains owned by its local domain. Only the relevant subset is published into the active NavigationWorld.

## 4. Closed foundations

### `NAV-V2-MAP-2` — CLOSED / ACCEPTED

Accepted 10k target-machine references:

```text
CPU compact queries                    <0.2 ms p95
GPU cruise total                        1.3226 ms p95
GPU hub total                           1.6258 ms p95
```

No synchronous frame-thread GPU dispatch/wait/bulk-readback path is allowed.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Accepted static-space capabilities include sparse free-space regions, explicit portals, envelope clearance, traversable apertures/tunnels/canyons, deterministic costed corridors, local invalidation/patching and turn-aware route state.

Final 10k turn-aware target-machine evidence:

```text
open_10k turn p95   12.0072 ms
hub_10k turn p95    11.9065 ms
```

Gate was `<=40 ms p95` on the worker path.

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Accepted deterministic fan:

```text
15 degree x 8 azimuths
30 degree x 8 azimuths
maximum 16 probes
```

Deliberate `1024 x 17` stress:

```text
519.9286 us p95
```

inside the `<1.0 ms normal peak` CPU design budget.

`ConflictHold` means only that the cheap local layer cannot prove a safe temporary target. It never means navigation must stop producing commands.

## 5. Precision trajectory chain — accepted components

```text
BoundedGapCandidateBuilder
OrientedPassageEvaluator
AttitudeReachabilityEvaluator
EmergencyPassageMitigator
ContinuousPassageTrajectoryEvaluator
EmergencyContactSeverityScorer
MovingGapPredictor
MovingPassageTrajectoryEvaluator
DockingTerminalEvaluator
```

### Oriented passage / bounded gap

Precision uses an oriented hull proxy rather than center + radius only. A conservative sphere may reject a flat slot that a correctly rolled hull can traverse.

`BoundedGapCandidateBuilder` receives one known primary conflict and only already-reduced neighbors. No global all-pairs work is allowed.

Hard output cap:

```text
<= 8 gap candidates
```

Accepted stress:

```text
top8_1024 = 24.0699 us p95
```

### Attitude reachability

A geometric fit is not enough. The requested hull attitude must be physically reachable before entry.

Reference fixture:

```text
90 degree roll
max angular acceleration = 90 deg/s^2
max angular speed        = 90 deg/s
closing speed            = 10 m/s

30 m -> ReachableCoast
15 m -> ReachableWithBraking
 8 m -> UnreachableBeforeEntry
```

`UnreachableBeforeEntry` is not a no-command state.

### Emergency mitigation

Accepted invariant:

```text
no collision-free proof != no navigation command
```

Priority:

```text
1. collision-free maneuver if reachable
2. stop before contact if reachable
3. otherwise least-severity explicit non-safe mitigation
```

`EmergencyMitigatedContact` is never `Clear`.

### Continuous static passage

`ContinuousPassageTrajectoryEvaluator` verifies one complete analytic ship segment:

```text
translation: cubic Hermite P/V -> P/V
attitude: shortest-arc smoothstep
33 pose samples
32 conservative between-sample proofs
```

Vehicle authority remains explicit by body axis plus angular rate/acceleration.

Control semantics:

```text
Newtonian
    velocity and hull attitude may diverge

EliteAssisted
    same physical authority
    plus supplied velocity-to-forward slip policy
```

Accepted eight-candidate batch performance:

```text
41.3527 us p95 = 0.04135 ms
```

The static verifier is frozen without contrary live evidence.

### Emergency contact severity

Already-predicted unavoidable contacts are ranked by rigid-body contact-point relative motion:

```text
v_ship_contact = v_center + omega x r
v_rel = v_ship_contact - v_surface
v_n = max(0, -dot(v_rel, normalTowardFreeSpace))
```

Hard bound:

```text
<= 8 candidates
<= 4 contact witnesses per candidate
```

Peak normal closing speed outranks energy/momentum/glancing/progress tie-breakers.

## 6. Moving gaps — ACCEPTED

`MovingGapPredictor` receives one already-selected obstacle pair and compact motion:

```text
P / V / A
angular velocity
conservative radius
snapshot revision
```

Fixed work:

```text
33 time samples
32 continuous intervals
```

Whole-interval gap width uses exact distance to the relative-position chord minus the constant-acceleration deviation bound:

```text
|a_rel| * dt^2 / 8
```

so hidden close/reopen events between samples fail closed.

Per sample it publishes gap center/velocity, separation axis/rate and both boundary surface normals/material velocities including `omega x r`.

Target-machine acceptance:

```text
bc84940ff23209d9731a3d5332b8af3794182790
NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS
7/7 trajectory CTest PASS
```

## 7. Moving continuous ship passage — ACCEPTED

`MovingPassageTrajectoryEvaluator` composes one accepted moving-gap prediction with one concrete ship P/V/attitude/OBB/capability segment.

It synchronizes:

```text
33 ship states <-> 33 gap states
32 ship intervals <-> 32 moving-gap intervals
```

Continuous proof includes changing gap width, transverse ship/gap-center motion, passage-frame rotation, relative OBB sweep and body-axis authority. Point samples alone never prove safety.

Target-machine acceptance:

```text
18799ab2c026b6d6dce3da11f9225eae3a3c5f35
NAVIGATION TRAJECTORY MOVING PASSAGE CONTRACT: PASS
8/8 trajectory CTest PASS
```

## 8. Collision / damage ownership

Navigation performs predictive feasibility and may emit expected-contact evidence. It is **not** the authoritative collision solver.

```text
Navigation / trajectory
    intent
    collision-free proof when possible
    expected-contact witness when not

Physics / Collision
    authoritative broadphase / narrow phase
    CCD / TOI / contact manifold
    impulse / friction / restitution / ricochet

Damage / Structural
    damage / breach / detach / destruction
```

After real contact, navigation replans from actual physics state.

## 9. Moving / rotating docking 6DoF — ACTIVE

Docking is relative pose/motion matching between explicit ship and dock interface frames, not center-point arrival.

### Stage 9A — terminal 6DoF capture ACCEPTED

Authority:

```text
src/world/navigation/DOCKING_TERMINAL_MODEL.md
```

Both interfaces expose:

```text
surface semantic
local port offset
mating normal
referenceUp / rollReference
```

Current required pairing:

```text
Bottom(ship) -> Bottom(dock)
```

For a moving/rotating dock port:

```text
p_origin(t) = p0 + v0*t + 0.5*a*t^2
v_origin(t) = v0 + a*t
v_port      = v_origin + omega x r
```

Terminal capture requires bounded:

```text
relative port position
relative port linear velocity
mating normals anti-aligned
referenceUp / roll aligned
relative angular velocity
```

A 180-degree rolled ship is invalid even when mating normals oppose correctly.

Target-machine acceptance:

```text
835271539619b7dd02efc54ff54df51d64b49fce
NAVIGATION TRAJECTORY DOCKING TERMINAL CONTRACT: PASS
9/9 trajectory CTest PASS
```

The single unused-helper warning seen during that build has been removed. Dock-port prediction is now a shared bounded helper for 9B.

### Stage 9B — continuous final docking approach ACTIVE CANDIDATE

Authority:

```text
src/world/navigation/DOCKING_APPROACH_MODEL.md
```

New component:

```text
DockingApproachEvaluator
```

#### Dock-local trajectory

The final precision segment is expressed in the moving/rotating dock frame rather than as a world-space rest-to-rest target.

In dock-local coordinates:

```text
corridor is static
terminal relative pose is fixed
relative terminal angular rate -> 0
```

Therefore:

```text
omega_ship(capture) = omega_dock
```

naturally follows from the trajectory representation.

Endpoint relative velocity uses the derivative of a rotating frame:

```text
v_rel_world = v_ship - v_port - omega x (p_ship - p_port)
```

#### Continuous corridor proof

The accepted `ContinuousPassageTrajectoryEvaluator` is reused as a dock-local geometry oracle with:

```text
PassageSource::DockingCorridor
33 relative poses
32 conservative continuous intervals
```

The internal geometry call receives effectively unbounded capabilities and Newtonian policy so rotating-frame coordinates are never mistaken for physical thrust authority.

#### World inertial authority

The accepted relative curve is transformed back to world kinematics:

```text
v_world = v_port + omega x r + v_rel

a_world = a_port
        + omega x (omega x r)
        + 2 * omega x v_rel
        + a_rel
```

Centripetal and Coriolis terms are therefore paid by real vehicle authority.

Between samples, body-axis thrust projection receives a conservative world-jerk + body-axis-rotation margin. Physical authority is not sample-only.

#### Angular authority

For relative attitude change `theta` over `T`:

```text
omega_rel_peak = 1.5 * theta / T
alpha_rel_peak = 6.0 * theta / T^2
```

World conservative bounds:

```text
omega_world_peak <= |omega_dock| + omega_rel_peak
alpha_world_peak <= alpha_rel_peak + |omega_dock| * omega_rel_peak
```

A sufficiently fast rotating station may therefore be physically uncapturable by a low-authority vehicle.

#### Terminal composition

After continuous geometry + inertial linear/angular authority pass, the generated final state is submitted to the accepted `DockingTerminalEvaluator`.

Only:

```text
FeasibleForCapture
```

claims both final-segment feasibility and terminal 6DoF capture.

Normal docking aborts/goes around when a safe/capturable final segment cannot be proven. Destructive impact is never successful docking.

### Stage 9B first target-machine attempt — NOT ACCEPTED

The first run built successfully and retained 9/9 prior behavior, but the new docking-approach test failed. Investigation proved the pinned fixture itself was invalid: the true Hermite excursion (`0.9622504486 m`) was smaller than the old available center travel (`0.975 m`), so a correct verifier could not reject it.

The repaired regression uses `0.9618 m` available center travel. It preserves the intended hard condition:

```text
all 33 sampled poses fit
but the analytic curve exits between samples
=> CorridorBlocked
```

The implementation's continuous proof is not weakened. Stage 9B remains pending a fresh target-machine 10/10 gate.

## 10. NPC pilot skill — PENDING

Vehicle capability and pilot skill remain separate. `PilotSkillProfile` may control reaction delay, decision rate, latency, smoothing, damping/overshoot, anticipation and deterministic execution error without corrupting geometry or physics truth.

## 11. Performance contract

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/global precision solve      asynchronous only
```

These are target-machine design budgets, not portable timing assertions.

## 12. Guidance/debug contract

Guidance visualizes the accepted navigation/trajectory/control intent; it must not run a separate planner.

`Shift+F12` raw NavigationWorld debug should eventually expose static regions/portals, dynamic predictions, local conflicts, gaps, moving-gap states, accepted continuous trajectory, emergency severity, docking frame/tolerances and pilot/controller execution state.

## 13. Progress / roadmap

```text
[████████████████░░░░░░░░] 8 / 12 major stages closed

1  NavigationMap / mass dynamic P/V/A               CLOSED
2  NavigationSpace / global corridors               CLOSED
3  LocalHorizon / LocalAvoidance                     CLOSED
4  oriented passage / bounded gaps / attitude        CLOSED
5  continuous static passage                         CLOSED
6  emergency mitigation + severity                   CLOSED
7  moving gap prediction                             CLOSED
8  moving continuous ship passage                    CLOSED
9  moving/rotating docking 6DoF                      ACTIVE
   9A terminal capture                               CLOSED
   9B continuous final approach                      ACTIVE
10 PilotSkillProfile                                 PENDING
11 live EliteGame / EliteServer / guidance + physics PENDING
12 end-to-end stress/debug + legacy retirement       PENDING
```

Legacy route-wide navigation is retired only after v2 owns the stable live path.

## 14. Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize, as applicable:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
relevant model / benchmark contracts
private elite-project-context CURRENT_STATE/CURRENT_TASK/ITERATION_LOG/DECISIONS
```

Stale state/task documentation or branch ambiguity blocks handoff.
