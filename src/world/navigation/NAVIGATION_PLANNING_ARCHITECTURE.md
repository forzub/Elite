# Navigation Planning Architecture

**Updated:** 2026-09-15  
**Status:** active runtime navigation authority  
**Current implementation wave:** NAV-RUCKIG-1 canonical live motion backend

## Core rule

Navigation is separated into topology, motion, execution and presentation.

```text
navigation snapshot
    -> GeometricPathPlanner / DockingPathPlanner     coarse collision-free topology
    -> RuckigRoutePlanner                            time-parameterized route motion
    -> RuckigTrajectorySolver                        state-to-state primitive
    -> optional bounded Ruckig live-pose reconnect   local correction only
    -> GuidanceTunnel                                presentation sampling only
    -> future follower                               per-tick execution/control
```

The topology layer decides where free space is. Ruckig decides how a vehicle can
move through that selected topology under kinematic constraints. Guidance
presentation consumes accepted motion and never owns route authority.

## Canonical runtime backend

`game::navigation::RuckigRoutePlanner` is the canonical route-to-trajectory API.
`world::navigation::TrajectoryGenerator` remains temporarily as a compatibility
facade for older callers.

`game::navigation::RuckigTrajectorySolver` is the state-to-state primitive used
by both route motion and rolling current-pose reconnects.

Hard contract:

```bash
python tests/architecture_contracts/check_ruckig_live_navigation.py
```

It rejects any reintroduction of `SmoothPathOptimizer` into live route generation
or rolling reconnect and verifies the old smoother is fail-closed in production.

## Layer ownership

| Layer | Responsibility | Update policy |
| --- | --- | --- |
| Global topology | Coarse collision-free polyline through free regions/passages | New target or invalidation; cheap validity checks may be staggered around 1 Hz |
| Ruckig motion | Position/velocity/acceleration trajectory through accepted topology | New accepted route; bounded local correction on material live-state change |
| Execution | Follow accepted trajectory, issue control/thruster commands, correct tracking error | Every physics tick |
| Guidance tunnel | Visible gates/recommended speed sampled from accepted motion | Cheap presentation refresh; no topology search or spline optimization |

## Collision-gated waypoint blending

A coarse topology vertex is a routing support, not automatically a mandatory
stop. The Ruckig route backend may assign a non-zero through-waypoint velocity,
but only under an explicit local safety contract.

For an internal waypoint:

1. derive incoming and outgoing directions;
2. reject U-turns and authored zero-speed constraints;
3. choose a local entry/exit distance from adjacent leg lengths;
4. test the direct entry->exit corner-cut chord with the complete navigation
   envelope;
5. derive a conservative speed from adjacent speed limits, lateral acceleration,
   available blend distance and turn angle;
6. solve the actual adjacent state transitions with Ruckig;
7. swept-check every generated Ruckig chord against canonical obstacles.

If any Ruckig/collision check fails, only the adjacent through velocities are
relaxed to zero and the route is retried. The fallback is therefore a safe stop
at the coarse vertex. There is no global spline fallback.

This is deliberately local. Ruckig never receives permission to search for a
different obstacle topology.

## Rolling live-pose reconnect

A live manual-guidance correction also uses Ruckig rather than a custom spline.

For a stable terminal the expensive correction is bounded to a physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

The current ship state is solved to a rejoin state on the accepted trajectory,
including the accepted rejoin velocity. The unchanged accepted tail is stitched
back onto the product. The local rejoin is never exposed as the docking terminal.

If the terminal moved materially, or a bounded Ruckig rejoin is infeasible, a
full Ruckig reconnect is attempted through sparse accepted-route supports plus a
final docking-axis alignment point. Every leg remains collision validated.

## SmoothPath retirement

The old custom B-spline implementation has been removed. The temporary
`SmoothPathOptimizer` symbol exists only to make migration fail safely:

- production: always invalid, message directs caller to Ruckig;
- old navigation test target only: `ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT`
  enables a minimal non-smoothing polyline shim for one stale regression.

No production CMake target may define that macro. Once the final stale spline
test is migrated, the compatibility header/source and their build-list entries
should be deleted completely.

## Collision invariants

Ruckig is a motion generator, not a collision system. Every generated route or
reconnect segment is accepted only after canonical navigation-geometry checks.

| Geometry | Responsibility |
| --- | --- |
| Render mesh | visible surface |
| Collision geometry | physical solid matter |
| Hit volumes | damage/picking ownership |
| Navigation geometry | free/blocked regions and authored passages for a vehicle envelope |

A coarse enclosing box must not fill a real navigable hole. Future authored
navigation geometry must preserve passages and use the complete moving envelope,
including attached/towed cargo.

Swept checks are mandatory. Sampling density is an implementation detail; a
coarse discrete step must never jump through a thin obstacle.

## Performance model

Old NAV-LIVE-3 measurements showed the custom spline stack spending hundreds of
milliseconds in repeated candidate sampling/collision checks and entering a
rolling replan storm. Those measurements are historical baseline only.

Current instrumentation:

- `[DockingPerf]`: snapshot / geometric / trajectory / tunnel stages;
- `[GuidanceReplan]`: rolling correction cost;
- `[RuckigRoutePerf]`: Ruckig attempts/successes/solve time, swept collision
  segment count and surviving blended-waypoint count.

If topology search dominates, optimize/cull/cache `GeometricPathPlanner`; do not
push global search into Ruckig. If Ruckig/validation dominates, improve bounded
candidate policy, broadphase/spatial indexing and scheduling without reducing
collision fidelity. If synchronous initial planning remains visible, move the
immutable solve to a worker with generation IDs/latest-request-wins publication.

## Deterministic test scenes

Keep deterministic scenarios before random stress scenes:

1. one obstacle and a small obstacle group;
2. wall with wide, tight and impossible openings;
3. corridor turn;
4. dock behind an obstacle requiring a broad detour;
5. moving object crossing the route;
6. drone/ship without and with attached cargo;
7. dense seeded obstacle field and many actors with staggered checks.

Focused Ruckig route tests additionally cover a clear corner that retains
through velocity and an obstacle-blocked corner that must relax to a stop.

## Non-negotiable rules

- No custom spline optimizer in live route generation or live reconnect.
- No collision-fidelity reduction to hide stalls.
- No local horizon endpoint masquerading as the real terminal.
- No HUD-owned route solving.
- No second global path search inside Ruckig.
- A through-waypoint blend exists only if local eligibility and actual swept
  Ruckig motion both pass collision validation.
- Reuse an immutable accepted tail only while its assumptions remain valid.
- Material environment/target changes invalidate the relevant product; they do
  not justify dragging stale guidance through space.
