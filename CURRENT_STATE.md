# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted locally  
**Navigation:** NAV-RUCKIG-1 compile gate accepted; live runtime regression fixed in current branch, local re-acceptance pending

## Stable baseline outside navigation

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

Renderer work remains paused while navigation is stabilized.

## Canonical live navigation path

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner / GeometricPathPlanner     coarse topology
    -> TrajectoryGenerator compatibility facade
    -> game::navigation::RuckigRoutePlanner          runtime motion
    -> RuckigTrajectorySolver                        state-to-state primitive
    -> swept NavigationObstacleGeometry validation
    -> GuidanceTunnel                                presentation
```

`SmoothPathOptimizer` is not part of either live route generation or rolling
reconnect. Its old B-spline implementation is retired.

## Accepted local compile gate

The user locally accepted the previous NAV-RUCKIG-1 build at commit `9cae20b`:

- `check_ruckig_navigation_spike.py` PASS;
- `check_ruckig_navigation_integration.py` PASS;
- `check_ruckig_live_navigation.py` PASS;
- `check_live_docking_guidance.py` PASS;
- focused `ruckig_route_planner` PASS;
- focused `guidance_tunnel_local_horizon` PASS;
- `EliteGame` compiled and linked under MinGW.

That compile gate did **not** constitute runtime acceptance.

## Live runtime regression found immediately afterwards

The first real Hub run displayed:

```text
НАВИГАЦИЯ НЕДОСТУПНА [Ruckig leg leaves the collision-free coarse corridor]
```

The cause was architectural and deterministic, not the obstacle search itself.
`RuckigTrajectorySolver` synchronized three independent scalar DoFs directly in
arbitrary world XYZ. A rest-to-rest diagonal leg can therefore bow away from the
straight collision-free chord supplied by `GeometricPathPlanner`, because X/Y/Z
may receive different normalized motion profiles. Swept validation correctly
rejected that bowed curve.

### Current fix

`RuckigTrajectorySolver` now builds a deterministic orthonormal `MotionBasis`
for every state-to-state solve:

- local +X follows the relative leg displacement;
- local Y/Z are transverse DoFs;
- position/velocity/acceleration/gravity deltas are transformed into that basis;
- Ruckig solves in the leg basis;
- sampled results are transformed back to world/planning coordinates;
- the original proper-acceleration, jerk and terminal-state validation remains;
- downstream swept obstacle validation remains authoritative.

A new focused regression requires a diagonal stopped leg to remain on its
coarse collision-free chord to `1e-5 m`.

This does not ask Ruckig to replace obstacle topology. `GeometricPathPlanner`
still decides where free space is; Ruckig supplies physically bounded state
motion in a coordinate system aligned with that spatial product.

## Hub navigation stress field

The previous diagnostic layout was rejected: all 16 stress objects had been
placed in four bands directly across player -> dock, producing an artificial
barrier rather than a useful Hub field.

Current layout:

- 2 authored docking targets remain on the +/-X service axis;
- 16 stress objects remain deterministic/reproducible;
- they are distributed over two staggered shells around the station;
- shell points have vertical variation;
- no stress object occupies the exact +/-X docking service axis;
- several independent passages remain through the field.

`check_navigation_stress_field.py` now forbids the old four-band coordinates.

## Hub Map label rule

Persistent object names on Hub Map are removed.

The single rule is now:

```text
all visible Hub Map overlay objects
    -> no permanent text
    -> mouse hover selects one nearest/highest-priority object
    -> one semi-transparent name is drawn above it
```

The policy is shared across Hub infrastructure, ships, the Hub reference and
future overlay objects. It does not depend on the stress-object type.

## Ruckig route behavior

Internal coarse waypoints may still request a conservative through velocity when
local corner-cut eligibility is clear. Every generated Ruckig sample chord is
validated against canonical navigation obstacles. If a blended corner fails,
its adjacent waypoint velocities are relaxed to zero and the route is retried;
there is no spline fallback.

Current diagnostics:

```text
[RuckigRoutePerf] total_ms=... legs=... ruckig_ok=...
                  ruckig_ms=... collision_segments=...
                  blended_waypoints=... coarse_points=...
                  obstacles=... samples=... valid=...
```

## Historical rejected baseline

The retired custom smoother with 19 obstacles produced approximately:

```text
total_ms      ~= 470
trajectory_ms ~= 373
```

and repeated synchronous reconnect stalls. These numbers are historical only;
do not optimize or restore that stack.

## Current re-acceptance

Run:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_ruckig_live_navigation.py
python tests/architecture_contracts/check_navigation_stress_field.py
python tests/architecture_contracts/check_live_docking_guidance.py
bash tests/navigation_guidance/run_ruckig_mingw64.sh
cmake --build build --target EliteGame
```

Then run `EliteGame` and verify all three runtime gates:

1. stress objects are distributed around the Hub, not lined up as a wall;
2. names appear only on hover and are semi-transparent;
3. CALCULATE ROUTE produces guidance instead of `Ruckig leg leaves the collision-free coarse corridor`.

If gate 3 still fails, preserve the exact new failure text and `[RuckigRoutePerf]`
line; do not weaken collision validation.
