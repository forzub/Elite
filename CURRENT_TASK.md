# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-MAP-1` — isolated NavigationMap API + CPU reference acceptance

## Architecture decision

Stop extending the old route-wide runtime pipeline as the basis for NPC/player
navigation.

Rejected foundation:

```text
GeometricPathPlanner
    -> whole-route trajectory materialization
    -> dense whole-route collision validation
    -> periodic synchronous rebuild
```

Ruckig remains a candidate final local motion primitive, but it does not solve
free-space routing, scene reduction or dynamic traffic. New Navigation v2 code
must not be organized around preserving the old planner.

Canonical design document:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

## Navigation v2 working-space contract

The active dynamic navigation domain is **ship-centered**, because ordinary play
away from a station occurs in the ship's active local working space.

This does not replace precise system/world simulation state. The boundary is:

```text
authoritative system/world state
        -> NavigationMap publication
        -> ship-centered NavigationWorld
```

The NavigationMap working origin may translate/rebase with the active ship/domain.
Axes remain stable navigation/travel axes and do not rotate with instantaneous
hull roll/pitch/yaw.

A Hub remains **Hub-local** for its authored geometry, docking ports and scheduled
bots. It publishes/transforms only the relevant subset when interaction with the
active NavigationWorld becomes possible.

## Current implementation: separate NavigationMap block

The new block is:

```text
src/world/navigation/map/NavigationMap.h
src/world/navigation/map/NavigationMap.cpp
src/world/navigation/map/CMakeLists.txt
src/world/navigation/map/README.md
```

The API boundary is intentionally narrow.

### Ingress

One authoritative snapshot is published by value:

```text
DynamicWorldUpdate
    sourceRevision
    WorkingFrame
    actors[]
        entityId
        position
        velocity
        acceleration
        radius
        flags
        motionRevision
```

The block owns the transformed result. It does not keep references to external
Store/game/scene/Hub objects.

### Internal ownership

Hidden behind PImpl:

```text
system/world -> map coordinate transform
actor table
prediction cache
sparse spatial cells
future CPU/GPU backend objects
```

These structures must never become cross-module shared data.

### Egress

Only compact derived products are returned by value:

```text
queryCorridor()
querySphere()
stats()
```

A planner receives only the small relevant candidate subset plus revisions and
query diagnostics. It does not receive the map's actor table or cell structure.

## Current CPU reference

The first backend intentionally prioritizes correctness and boundary stability:

```text
constant-acceleration endpoint prediction
conservative future swept sphere
sparse 3D cell hash
corridor broadphase + conservative exact test
sphere broadphase + conservative exact test
```

Prediction:

```text
p1 = p0 + v*T + 0.5*a*T^2
travel_bound = |v|*T + 0.5*|a|*T^2
swept_sphere = sphere(p0, radius + travel_bound)
```

There is no fixed actor-per-cell correctness cap in the CPU reference.

The CPU implementation is not a decision against GPU compute. It is the behavior
oracle and stable API needed before moving the backend to GPU.

## Existing GPU evidence

`NAV-V2-GPU-0` remains available under:

```text
benchmarks/navigation_gpu/
```

It already measures deterministic 1k/5k/10k dynamic fields with P/V/A prediction,
3D spatial bins, corridor filtering and all-agent candidate queries. The next GPU
step must fit behind `NavigationMap` rather than create another public API.

## Immediate acceptance task

Run from MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh
```

Expected:

```text
NAVIGATION MAP BOUNDARY CONTRACT: PASS
NAVIGATION MAP CONTRACT TESTS: PASS
```

The behavioral contract verifies:

1. published actor data is owned by the block;
2. huge authoritative coordinates rebase into precise local ship-centered values;
3. stable working-basis conversion is owned by the block;
4. moving actors that can enter a corridor survive conservative prediction;
5. local queries use spatial cells rather than deliberately scanning the actor table;
6. out-of-bounds/rejected inputs are explicit;
7. invalid frame publication is atomic and cannot corrupt the accepted map.

## Next measurement after PASS

Add a CPU NavigationMap benchmark using the same deterministic 1k/5k/10k
`cruise` and `hub` datasets as the existing GPU benchmark. Measure:

```text
snapshot publication/rebuild ms
corridor query median/p95
sphere/local query median/p95
actors examined per query
occupied cells
memory estimate
```

Then compare CPU and GPU on the user's Quadro RTX 3000.

Decision criteria:

- if CPU sparse map is already comfortably inside the <0.5 ms main-thread budget
  for the relevant update/query cadence, keep dynamic ownership on CPU until a
  stronger reason for GPU appears;
- if CPU publication/conflict work is too expensive but GPU remains <1-2 ms,
  implement a GPU backend behind the same NavigationMap API;
- do not introduce synchronous bulk GPU readback;
- do not wire the old route-wide planner into the new block merely to make it
  appear integrated.

## Following capability

After backend ownership is measured, start `NAV-V2-SPACE-1` for static
free-space/clearance inside the same NavigationMap boundary:

```text
cached/baked free volume
clearance
connectivity/portals
local invalidation
agent-envelope queries
```

Static representation is still open: sparse voxel/octree bricks, convex
free-space cells or a hybrid region graph are candidates.

## Performance budget

Targets, not cross-machine hard assertions:

```text
main-thread CPU navigation work          < 0.5 ms typical
                                          < 1.0 ms normal peak
GPU dynamic NavigationWorld update       < 1.0 ms preferred
                                          < 2.0 ms heavy-scene ceiling
full/precision route solve               async only
```

NavigationMap must reduce the scene before expensive local planning. No normal
per-agent path may scan the full world or synchronously rebuild a dense route.
