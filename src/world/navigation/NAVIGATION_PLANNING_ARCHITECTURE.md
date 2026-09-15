# Navigation Planning Architecture

**Updated:** 2026-09-15  
**Status:** active design baseline for client/runtime navigation  
**Current implementation wave:** NAV-LIVE-1 bounded rolling docking reconnect

## Core rule

Navigation is split by time horizon and authority. A long route is not itself a
reason for high CPU cost. Expensive work appears when global search, smoothing
and obstacle validation are repeated at interactive/frame frequency.

The target architecture has four distinct products:

| Layer | Responsibility | Update policy |
| --- | --- | --- |
| Global route | Collision-free coarse polyline/topology through free regions and passages to the target | On new target or route invalidation. Cheap validity check may run around 1 Hz; do not solve again when the old route remains valid. Stagger different actors. |
| Local motion | Smooth near-term trajectory, speed, orientation and moving-obstacle response | Bounded physical horizon; rebuild when live state materially changes |
| Execution | Thruster/control commands and tracking-error correction | Every physics tick |
| Guidance tunnel | Visible nearby gates sampled from an already accepted trajectory | Cheap presentation refresh; must not become another global planner |

The global route and the local trajectory are separate immutable products. HUD
presentation does not own navigation authority.

## Local planning horizon

A local solve must extend far enough that a new solution can arrive and the
vehicle can still stop or turn safely. The translational lower bound is:

```text
D >= v * T_latency + v^2 / (2 * a_brake) + safety_margin
```

The actual horizon also includes turning demand. Current NAV-LIVE-1 adds a
turn-distance term derived from the vehicle minimum turn radius and the course
change toward the already accepted trajectory.

The horizon must look through an upcoming narrow passage early enough to align
before entry. Replanning only after reaching the aperture is invalid even when
the aperture itself is collision-free.

## Sampling and collision precision

`TrajectoryGenerationRequest::maxCurveChordErrorMeters = 0.03` is a geometric
curve-to-chord approximation tolerance. It is **not** a three-centimetre motion
step. Current docking trajectory samples are normally spaced about 3..7 m.

Precision should be adaptive:

- clear straight space: validate large spans;
- turns and obstacle boundaries: refine;
- narrow passages: refine according to actual clearance;
- terminal assembly/docking: refine to final alignment accuracy.

Object size alone does not determine the required step. A large vehicle can
have a small clearance. Collision validation must cover the swept motion between
poses so a coarse discrete step cannot jump through a thin wall.

## Geometry responsibilities

Do not collapse the following concepts into one volume:

| Geometry | Responsibility |
| --- | --- |
| Render mesh | Visible surface |
| Collision geometry | Physical solid matter |
| Hit volumes | Damage/picking ownership of a part |
| Navigation geometry | Free/blocked regions and passages for a particular vehicle/envelope |

The current verified docking path uses navigation obstacle primitives derived
from runtime object bounds; it does not yet consume full editor-authored hit or
collision geometry.

A single enclosing box around a station/ship is unsuitable for navigation when
the model contains a real passage. A rectangular opening should be represented
by the surrounding solid beams/walls, not by one box that fills the hole.
Navigation semantics may additionally describe a passage explicitly: axis,
cross-section, entry/exit poses and policy. Traversability is still checked
against the complete moving vehicle envelope and, when towing, the combined
envelope.

## Current performance findings

### GeometricPathPlanner

The current visibility-graph search is lazy in construction but can still test
connections from expanded nodes against many other support nodes. Each segment
validation iterates navigation obstacles. In dense scenes the practical cost
can therefore approach a `nodes^2 * obstacles` pattern. Route length is not the
primary cost driver.

This remains a global-planner optimization target after live timings identify
whether it dominates the real request.

### GuidanceTunnel reconnect

Before NAV-LIVE-1 a rolling current-pose reconnect could run `SmoothPathOptimizer`
over the complete remaining route. Several candidates are generated; every
candidate can resample and collision-check the path. A visual correction could
therefore behave like a second route planner.

NAV-LIVE-1 changes the expensive reconnect policy:

1. locate current progress on the accepted trajectory;
2. derive a local horizon from latency + braking + turning + safety margin;
3. smooth and collision-check only current pose -> local rejoin;
4. stitch the untouched, already accepted trajectory tail after the rejoin;
5. keep the real docking terminal as the final tunnel endpoint.

If the live terminal has moved materially, the code deliberately falls back to
the previous full reconnect. A separate bounded terminal-tail solver is required
before that fallback can safely be removed.

## Deterministic navigation test scenes

Build these before a dense stress field so failures have a known cause:

1. one obstacle, then a small obstacle group;
2. wall with a wide opening;
3. wall with a tight but passable opening;
4. wall with an impossible opening;
5. a turn inside a corridor;
6. a dock behind an obstacle that requires a broad turning loop;
7. a moving object crossing the route;
8. the same passage for a drone without cargo and with an attached/towed part;
9. only then: dense obstacle field and many actors with staggered route checks.

Each scene should distinguish route-topology failure, orientation/turning
failure, braking/horizon failure, collision-validation failure and CPU budget
failure.

## Safety invariants

- Never reduce collision fidelity merely to hide a frame stall.
- Never label a local planning-horizon endpoint as the real docking terminal.
- A locally rebuilt trajectory segment must be collision-validated with the
  same canonical navigation geometry used by the global route.
- An immutable accepted tail may be reused without re-solving it when its world
  assumptions remain valid.
- Material target/obstacle motion invalidates the relevant cached product; it is
  not solved by visually translating old gates.
