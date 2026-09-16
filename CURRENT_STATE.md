# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted locally  
**Navigation:** `NAV-V2-MAP-1` — isolated ship-centered NavigationMap API + CPU reference implemented; MinGW behavioral measurements pending

## Stable baseline outside navigation

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the shared runtime/editor schema authority.
Renderer feature work remains paused while Navigation v2 is established.

## Navigation architecture reset

The previous live chain is no longer accepted as the foundation for scalable
navigation:

```text
GeometricPathPlanner
    -> route-wide TrajectoryGenerator / RuckigRoutePlanner
    -> dense route samples
    -> route-wide obstacle validation
    -> GuidanceTunnel
```

The Ruckig cutover proved that the custom spline was only part of the problem. In
one successful runtime route with six coarse points, the surrounding pipeline
still produced roughly ten thousand trajectory samples; synchronous docking work
could still block the client update for hundreds of milliseconds.

Therefore Navigation v2 does **not** continue by optimizing the old route-wide
pipeline. `GeometricPathPlanner`, current route-wide sampling and current guidance
plumbing are legacy/migration code. `SmoothPathOptimizer` remains retired.

Ruckig remains only a candidate local kinematic primitive after routing/avoidance
has selected a target state. It is not the free-space/path-search authority.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

## Coordinate decision

Navigation v2 preserves multiple intentional coordinate domains.

- Authoritative long-lived state remains in precise system/world coordinates.
- Ordinary active navigation uses a **ship-centered NavigationWorld** working
  frame. Its origin may translate/rebase with the active ship/domain, but its
  axes are stable navigation/travel axes rather than instantaneous hull attitude.
- A Hub keeps its own **Hub-local** geometry, docking ports and scheduled local
  bots. Only the relevant subset is transformed/published into the active
  ship-centered NavigationWorld.
- Hull-local coordinates remain an execution/flight-control concern.
- Render/player-relative coordinates remain presentation-only.

## NAV-V2-MAP-1: NavigationMap block

A new isolated block now exists under:

```text
src/world/navigation/map/
```

Public boundary:

```text
NavigationMap.h
```

Implementation/support:

```text
NavigationMap.cpp
CMakeLists.txt
README.md
```

The block is intentionally not a bag of shared structures. It owns its data and
its coordinate conversion.

Ingress:

```text
NavigationMap::DynamicWorldUpdate
    source revision
    active WorkingFrame in authoritative system/world coordinates
    P/V/A/radius/flags for dynamic actors
```

The update is passed by value. The map transforms the snapshot internally into
ship-centered coordinates and owns the resulting actor table, prediction cache
and sparse cell index. No caller receives pointers/references/views into those
structures.

Egress is only compact derived data by value:

```text
queryCorridor() -> relevant predicted actor candidates
querySphere()   -> relevant local predicted actor candidates
stats()         -> revisions/counts/diagnostics
```

The public header deliberately has no dependency on GLM, OpenGL, GLFW, game
state, scene or renderer code. `NavigationMap` uses PImpl so a future GPU backend
can replace the CPU internals without changing planner call sites.

### Current CPU reference backend

The first backend is deliberately simple and deterministic:

```text
owned dynamic actor table
constant-acceleration endpoint prediction
conservative swept sphere
sparse 3D cell hash
corridor broadphase + exact conservative sphere/segment test
sphere broadphase + exact conservative sphere/sphere test
```

Conservative first-stage prediction:

```text
p1 = p0 + v*T + 0.5*a*T^2
travel_bound = |v|*T + 0.5*|a|*T^2
swept_sphere = sphere(p0, radius + travel_bound)
```

There is no fixed actor-per-cell correctness cap in the CPU reference.
Out-of-bounds and rejected inputs remain explicit statistics.

Behavioral and architecture contracts:

```text
tests/navigation_map/NavigationMapContractTests.cpp
tests/navigation_map/CMakeLists.txt
tests/navigation_map/run_mingw64.sh
tests/architecture_contracts/check_navigation_map_boundary.py
```

The behavioral contract covers:

- ownership/revisions;
- large authoritative coordinates -> ship-centered rebase;
- stable non-hull basis transform;
- dynamic corridor filtering;
- sparse local query behavior;
- atomic rejection of an invalid/non-orthogonal frame.

The block is still isolated from live docking/navigation runtime. That is
intentional until the API and backend measurements are accepted.

## NavigationWorld v2 direction

The shared active NavigationWorld is intended to serve both the player and NPCs:

```text
NavigationMap
    static free-space / clearance representation (next capability)
    dynamic actor table P/V/A
    dynamic spatial index
    predicted swept bounds
        |
        +-> active route corridors
        +-> local conflict candidates
        +-> mass NPC steering / avoidance
        +-> precision local planner (dock / repair / special)
        +-> temporary target state
        +-> local motion / controller
```

Velocity and acceleration are actor-owned. They are not copied into every spatial
cell. Detailed future tubes are generated lazily only for actors that can enter a
selected corridor/local physical horizon.

Static station/Hub free space must ultimately be cached/baked as navigation data
with clearance/connectivity and local invalidation. The exact representation
(sparse octree/voxel bricks, convex free-space cells, hybrid region graph) remains
open until benchmark evidence exists.

## Navigation / collision / damage boundary

These are separate authorities even if they share broadphase/spatial data:

```text
Navigation
    conservative envelopes / clearance / predicted conflicts

Physics / Collision
    candidate pairs -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local navigation invalidation
```

Render, collision, hit/damage and navigation geometry intentionally need not
match.

A visual hole does not automatically open a navigation route. A breach becomes
navigable only when its clearance admits the requesting agent envelope. A
repair drone may pass where a ship cannot. Topology-changing breaches dirty only
the affected navigation region and may publish an explicit outside<->inside
breach portal. Detached fragments become new dynamic actors/obstacles.

## Existing NAV-V2-GPU-0 evidence

The isolated OpenGL 4.3 compute benchmark remains available:

```text
benchmarks/navigation_gpu/CMakeLists.txt
benchmarks/navigation_gpu/main.cpp
benchmarks/navigation_gpu/run_mingw64.sh
benchmarks/navigation_gpu/README.md
tests/architecture_contracts/check_navigation_gpu_benchmark.py
```

It evaluates deterministic `cruise` and denser `hub` actor fields at 1k / 5k /
10k actors. GPU pass A performs P/V/A prediction, conservative swept bounds,
3D spatial binning and selected-corridor filtering. GPU pass B performs all-agent
neighbor/conflict candidate queries using only relevant cells.

That benchmark is no longer intended to define a second public navigation API.
Its next useful role is to become/compare against a backend behind
`NavigationMap`.

## Performance targets

Design budget for Navigation v2:

```text
main-thread navigation CPU:     < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld:    < 1.0 ms preferred
                                < 2.0 ms heavy-scene target ceiling
full/precision route solve:     asynchronous; never a synchronous frame blocker
```

These GPU numbers are design targets, not a machine-independent test assertion.
GPU time competes with rendering.

## Current acceptance gate

Run from MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh
```

Then keep the existing GPU evidence available:

```bash
python tests/architecture_contracts/check_navigation_gpu_benchmark.py
bash benchmarks/navigation_gpu/run_mingw64.sh
```

Acceptance for `NAV-V2-MAP-1`:

- architecture boundary contract PASS;
- standalone C++ behavioral contract compiles and PASSes under production MinGW64;
- no public API dependency leak from NavigationMap;
- no fixed per-cell actor cap in the CPU reference;
- corridor/sphere results are deterministic and revision-tagged;
- invalid frame publication leaves the previously accepted map intact.

After that, benchmark CPU NavigationMap with 1k/5k/10k actors against the existing
GPU prototype, then choose the dynamic backend. The following wave can add static
free-space/clearance (`NAV-V2-SPACE-1`) without exposing its storage across the
same API boundary.
