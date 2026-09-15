# Navigation Planning Architecture

**Updated:** 2026-09-15  
**Status:** active runtime navigation authority  
**Current implementation wave:** NAV-RUCKIG-1 canonical live motion backend

## Core rule

Navigation is separated into topology, motion, execution and presentation. No
presentation component is allowed to grow into another route planner.

```text
navigation snapshot
    -> GeometricPathPlanner / DockingPathPlanner     coarse collision-free topology
    -> RuckigRoutePlanner                            time-parameterized motion
    -> optional bounded Ruckig live-pose reconnect   local correction only
    -> GuidanceTunnel                                presentation sampling only
    -> future follower                               per-tick execution/control
```

The coarse route decides **where free space is**. Ruckig decides **how the vehicle
can move through an already selected local/topological route under kinematic
constraints**. Guidance rendering consumes accepted motion; it does not invent a
new path.

## Canonical runtime backend

`game::navigation::RuckigRoutePlanner` is the canonical route-to-trajectory API.
`world::navigation::TrajectoryGenerator` remains temporarily as a compatibility
facade for older callers and delegates to that API.

`game::navigation::RuckigTrajectorySolver` is the state-to-state primitive used
by both:

- the route trajectory backend;
- rolling current-pose guidance reconnects.

The old `SmoothPathOptimizer` must not appear in either live path. It may remain
only as dead/legacy test code until those tests are migrated and the source can
be deleted completely.

Architecture contract:

```bash
python tests/architecture_contracts/check_ruckig_live_navigation.py
```

It rejects any reintroduction of `SmoothPathOptimizer` into
`TrajectoryGenerator.cpp` or `GuidanceTunnel.cpp`.

## Layer ownership

| Layer | Responsibility | Update policy |
| --- | --- | --- |
| Global topology | Coarse collision-free polyline through free regions/passages | New target or invalidation; cheap validity checks may be staggered around 1 Hz |
| Ruckig motion | Position/velocity/acceleration trajectory along accepted topology | New accepted route; bounded local correction on material live-state change |
| Execution | Follow accepted trajectory, issue control/thruster commands, correct tracking error | Every physics tick |
| Guidance tunnel | Visible gates and recommended speed sampled from accepted motion | Cheap presentation refresh; no topology search/spline optimization |

## Ruckig route baseline

The first deliberately conservative implementation solves every consecutive
coarse-topology segment as a state-to-state Ruckig leg. Intermediate coarse
vertices currently use zero terminal velocity. This can look stop-and-go, but it
has two important properties:

1. a state-to-state polynomial cannot silently shave an obstacle corner;
2. collision ownership stays explicit and testable.

Through-waypoint velocity blending is a later bounded optimization. It is legal
only when the blended Ruckig transition itself passes swept collision validation.
A failed blend falls back to the safe stop-at-waypoint behavior; it must never
fall back to a global spline smoother.

## Rolling live-pose reconnect

A live manual-guidance correction no longer generates spline/bulge/loop candidate
families. It uses Ruckig.

For a stable terminal, the expensive correction is bounded to a physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

The current ship state is solved to a rejoin state on the accepted trajectory,
including the accepted rejoin velocity. The unchanged accepted tail is then
stitched back onto the product. The local rejoin is never exposed as the real
docking terminal.

If the live terminal moved materially, or a bounded Ruckig rejoin is infeasible,
a full Ruckig reconnect is attempted through sparse accepted-route supports and
a final docking-axis alignment point. Every generated leg must still pass swept
collision checks.

## Collision invariants

Ruckig is a motion generator, not a collision system. Every live generated
segment is accepted only after canonical navigation-geometry validation.

- Render mesh: visual surface.
- Collision geometry: physical solid matter.
- Hit volumes: damage/picking ownership.
- Navigation geometry: blocked/free regions and authored passages for a vehicle envelope.

A coarse enclosing box must not fill a real navigable hole. Future authoring work
must preserve openings/passages in navigation geometry and use the complete
vehicle envelope, including attached/towed cargo.

Swept checks remain mandatory. Sampling density is a validation detail; a coarse
discrete step must never be able to jump through a thin obstacle.

## Performance model

Old NAV-LIVE-3 measurements showed the custom spline stack spending hundreds of
milliseconds in repeated candidate sampling/collision checks and entering a
rolling replan storm. Those timings are now a **historical baseline**, not the
intended implementation.

Current performance should be measured from:

- `[DockingPerf]` for snapshot / geometric / trajectory / tunnel stages;
- `[GuidanceReplan]` for rolling correction cost;
- `[RuckigRoutePerf]` in `navigation_perf.log` for Ruckig legs, solve time and
  swept collision segment count.

If the initial global topology search becomes dominant, optimize/cull/cache
`GeometricPathPlanner` rather than pushing topology responsibility into Ruckig.
If Ruckig/swept validation becomes dominant, improve local candidate sets,
validation broadphase/spatial indexing and scheduling without reducing safety.
If a synchronous initial solve is still visible, move immutable planning to a
worker and publish latest-request-wins generations.

## Deterministic test scenes

Keep deterministic scenarios before random stress scenes:

1. one obstacle and a small obstacle group;
2. wall with wide, tight and impossible openings;
3. corridor turn;
4. dock behind an obstacle requiring a broad detour;
5. moving object crossing the route;
6. drone/ship without and with attached cargo;
7. dense seeded obstacle field and many actors with staggered checks.

Tests must distinguish topology failure, motion infeasibility, collision failure,
terminal-state failure and CPU-budget failure.

## Non-negotiable rules

- No `SmoothPathOptimizer` in live route generation or live tunnel reconnect.
- No collision-fidelity reduction to hide frame stalls.
- No local horizon endpoint masquerading as the real terminal.
- No HUD-owned route solving.
- No second global path search inside Ruckig.
- Reuse an immutable accepted tail only while its assumptions remain valid.
- Material environment/target changes invalidate the relevant product; they do
  not justify visually dragging stale guidance through space.
