# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted locally  
**Navigation:** `NAV-V2-GPU-0` — isolated ship-centered NavigationWorld GPU benchmark implemented; local compile/run measurements pending

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

This avoids forcing all Hub/private-domain state to rebase around the player and
also avoids global-coordinate precision/scale contaminating local navigation.

## NavigationWorld v2 direction

The shared active NavigationWorld is intended to serve both the player and NPCs:

```text
static free-space / clearance representation
+ dynamic actor table (P/V/A/bounds/flags)
+ dynamic spatial index
+ predicted swept bounds
+ active route corridors
+ local conflict candidates
        |
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

## NAV-V2-GPU-0 implementation

An isolated OpenGL 4.3 compute benchmark now exists:

```text
benchmarks/navigation_gpu/CMakeLists.txt
benchmarks/navigation_gpu/main.cpp
benchmarks/navigation_gpu/run_mingw64.sh
benchmarks/navigation_gpu/README.md
tests/architecture_contracts/check_navigation_gpu_benchmark.py
```

It is intentionally **not linked into EliteGame**.

The benchmark evaluates deterministic `cruise` and denser `hub` actor fields at:

```text
1,000
5,000
10,000 actors
```

GPU pass A performs P/V/A endpoint prediction, a conservative future swept sphere,
3D spatial binning and selected-corridor filtering. GPU pass B performs all-agent
neighbor/conflict candidate queries using only relevant cells.

The prototype uses a ship-centered +/-12 km working cube, 600 m cells and a
fixed 64-actor cell capacity. Cell overflow is explicit and makes the structural
result invalid; it is never silently accepted.

The conservative future envelope is intentionally simple:

```text
p1 = p0 + v*T + 0.5*a*T^2
travel_bound = |v|*T + 0.5*|a|*T^2
swept_sphere = sphere(p0, radius + travel_bound)
```

This is a benchmark representation, not the final production sweep.

Measured output includes:

```text
gpu_bin_ms
gpu_neighbor_ms
gpu_total_ms / gpu_p95_ms
cpu_submit_ms
candidate pairs / neighbor checks
corridor actor count
occupied cells / overflow / out-of-bounds
max swept radius
SSBO memory
readback bytes
```

Only a 32-byte aggregate statistics block is read back. The 1,000-actor cases
also compute an O(N^2) CPU reference for pair/corridor counts; `reference_ok=1`
is required before performance measurements are trusted.

## Performance targets

Design budget for Navigation v2:

```text
main-thread navigation CPU:     < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld:    < 1.0 ms preferred
                                < 2.0 ms heavy-scene target ceiling
full/precision route solve:     asynchronous; never a synchronous frame blocker
```

These GPU numbers are design targets, not a machine-independent test assertion.
GPU time competes with rendering. If the 10k benchmark is not comfortably within
budget, compare a CPU spatial-index baseline before deciding on GPU ownership.

## Current acceptance gate

Run from MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_navigation_gpu_benchmark.py
bash benchmarks/navigation_gpu/run_mingw64.sh
```

The benchmark writes `navigation_gpu_benchmark.csv` and prints its absolute path.

Acceptance for this wave is evidence, not an assumed speedup:

- architecture contract PASS;
- benchmark compiles on the production OpenGL 4.3/GLFW/GLAD stack;
- both 1k CPU-reference checks report `reference_ok=1`;
- `overflow=0` and `out_of_bounds=0` for measured default scenarios;
- collect 1k/5k/10k GPU median/p95, CPU submission, memory and candidate counts;
- only after those numbers decide GPU vs CPU/hybrid dynamic spatial ownership.

No live NavigationWorld integration should occur before this benchmark is
measured.
