# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-MAP-2` — isolated NavigationMap CPU measurement

## Accepted direction

Navigation v2 is no longer built around the legacy route-wide synchronous pipeline.
The active runtime navigation domain is ship-centered; Hub keeps its own local
space and publishes only the subset that becomes relevant to the active ship
NavigationWorld.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

## NavigationMap module boundary

The separate block is now under:

```text
src/world/navigation/map/NavigationMap.h
src/world/navigation/map/NavigationMap.cpp
src/world/navigation/map/CMakeLists.txt
src/world/navigation/map/README.md
```

The public API is intentionally narrow.

Ingress is one owned snapshot by value:

```text
DynamicWorldUpdate
    sourceRevision
    WorkingFrame
    actors[] { id, P, V, A, radius, flags, motionRevision }
```

`NavigationMap` transforms and owns all internal data. Actor tables, prediction
cache, sparse cells and future GPU backend state never cross the module boundary.
The public header has no GLM/OpenGL/GLFW/game/scene dependency and uses PImpl.

Egress is only compact derived data by value:

```text
queryCorridor()
querySphere()
stats()
```

Planners receive only relevant candidates plus revisions/diagnostics; they do not
receive internal map storage.

## Current CPU reference backend

The behavior oracle currently provides:

```text
constant-acceleration endpoint prediction
conservative swept sphere
sparse 3D cell hash
corridor broadphase + conservative sphere/segment test
sphere/local broadphase + conservative sphere/sphere test
```

No fixed actor-per-cell correctness cap is used.

The standalone acceptance layer is:

```text
python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh
```

## New measurement block

CPU benchmark added under:

```text
benchmarks/navigation_map/CMakeLists.txt
benchmarks/navigation_map/main.cpp
benchmarks/navigation_map/run_mingw64.sh
benchmarks/navigation_map/README.md
```

It uses only the public `NavigationMap` API and mirrors the GPU prototype datasets:

```text
cruise: 1k / 5k / 10k actors, 18 km spawn cube, 250 m/s, 8 m/s^2
hub:    1k / 5k / 10k actors,  7 km spawn cube, 120 m/s, 6 m/s^2
prediction horizon: 3 s
```

It measures separately:

```text
snapshot publication / internal rebuild median + p95
corridor query median + p95
sphere/local query median + p95
candidate counts
cells visited / occupied cells visited / actors examined
indexed / rejected / out-of-bounds actors
```

Run from MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh
bash benchmarks/navigation_map/run_mingw64.sh
```

Optional longer run:

```bash
bash benchmarks/navigation_map/run_mingw64.sh --warmup 10 --iterations 100
```

The benchmark prints the absolute CSV path. Send the six result rows or the CSV.

## Existing GPU comparison

The OpenGL 4.3 compute prototype remains isolated under:

```text
benchmarks/navigation_gpu/
```

It measures the same `cruise` / `hub` 1k/5k/10k classes. GPU code is not a second
public navigation API; if selected, it must become an implementation backend
behind `NavigationMap`.

## Decision after CPU numbers

Use measured evidence rather than committing to GPU in advance:

- if CPU corridor/local queries are comfortably below the frame budget and rebuild
  cadence can be asynchronous/incremental, retain CPU ownership initially;
- if publication/query work becomes expensive at 5k/10k while the compute
  prototype stays under the GPU budget, add a GPU backend behind the same API;
- never expose GPU buffers/cells/actor tables through the module boundary;
- never require synchronous bulk GPU readback for normal navigation.

## Next capability

After the dynamic backend decision, start `NAV-V2-SPACE-1` inside the same module
boundary:

```text
static free-space / clearance
connectivity / portals
local invalidation
agent-envelope queries
```

Candidate representations remain sparse voxel/octree bricks, convex free-space
cells or a hybrid region graph. Choose after measurements, not by preference.

## Performance targets

```text
main-thread CPU navigation work          < 0.5 ms typical
                                          < 1.0 ms normal peak
GPU dynamic NavigationWorld update       < 1.0 ms preferred
                                          < 2.0 ms heavy-scene ceiling
full/precision route solve               async only
```
