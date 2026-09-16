# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-GPU-0` — ship-centered dynamic-world GPU prototype benchmark

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

The previous Ruckig integration remains useful evidence and Ruckig remains a
candidate final local motion primitive, but it does not solve free-space routing,
scene reduction or dynamic traffic. New Navigation v2 code must not be organized
around preserving the old planner.

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
        -> transform relevant subset
        -> ship-centered NavigationWorld
```

The NavigationWorld origin may translate/rebase with the active ship/domain.
Axes remain stable navigation/travel axes and do not rotate with instantaneous
hull roll/pitch/yaw.

A Hub remains **Hub-local** for its authored geometry, docking ports and scheduled
bots. It publishes/transforms only the relevant subset into the active ship
NavigationWorld when interaction becomes possible.

## Immediate task: measure dynamic-world cost before integration

The isolated benchmark is now implemented under:

```text
benchmarks/navigation_gpu/
```

It must answer whether GPU compute is actually a good owner for the dynamic
NavigationWorld broadphase/prediction layer on the target machine.

The benchmark uses the production OpenGL 4.3 + GLFW + GLAD stack but is not part
of `EliteGame`.

### Dataset

Two deterministic workloads, each at 1k / 5k / 10k actors:

```text
cruise
    wide ship-centered field
    speeds up to 250 m/s
    accelerations up to 8 m/s^2

hub
    denser local field
    speeds up to 120 m/s
    accelerations up to 6 m/s^2
```

Each actor carries P/V/A plus radius. The default prediction horizon is 3 seconds.

### GPU pass A

For every actor:

```text
P/V/A -> predicted endpoint
     -> conservative swept sphere
     -> fixed 3D spatial bin
     -> selected-route corridor relevance
```

The first prototype uses a +/-12 km ship-centered cube, 600 m cells and 64 actor
slots per cell. This is deliberately simple and bounded. Any overflow is reported
and invalidates the structural result.

### GPU pass B

For every actor:

```text
own swept radius + global max swept radius
    -> conservative neighboring-cell range
    -> candidate actors from only those cells
    -> swept-sphere overlap
    -> de-duplicated possible conflict pairs
```

No normal path may scan all scene actors per agent.

### Correctness gate

The 1,000-actor workloads compute an O(N^2) CPU reference using the same
conservative swept-sphere definition. Before trusting GPU timing:

```text
reference_pairs == gpu candidate pairs
reference_corridor == gpu corridor actors
overflow == 0
out_of_bounds == 0
```

### Measurements

Collect:

```text
gpu_bin_ms
gpu_neighbor_ms
gpu_total_ms
gpu_p95_ms
cpu_submit_ms
pairs
neighbor_checks
corridor actor count
occupied cells
overflow / out_of_bounds
max_sweep_radius_m
memory_mib
readback_bytes
```

Only 32 bytes of aggregate SSBO statistics are read back per measured scenario.
The design intent is to keep detailed conflict products GPU-resident or
asynchronously buffered rather than introduce a frame-blocking bulk readback.

## Performance budget to test

Targets, not hard cross-machine assertions:

```text
main-thread CPU navigation submission   < 0.5 ms typical
                                         < 1.0 ms normal peak
GPU dynamic NavigationWorld update      < 1.0 ms preferred
                                         < 2.0 ms heavy-scene target ceiling
full/precision route solve              async only
```

A result above 2 ms is not hidden or waived. It means compare another data
structure and a CPU spatial-index baseline before runtime integration.

## Collision / damage boundary already fixed

NavigationWorld owns conservative free-space/avoidance information, not exact
collision consequences.

```text
Navigation -> avoid / predict
Physics    -> exact CCD / TOI / contact
Damage     -> hit ownership / detach / breach / destruction
```

The spatial broadphase may be shared.

Small visual holes do not open routes. A breach becomes navigable only when its
clearance admits a given agent. Topology changes dirty/rebuild only the affected
navigation region. Detached pieces register as new dynamic actors.

## Run now

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_navigation_gpu_benchmark.py
bash benchmarks/navigation_gpu/run_mingw64.sh
```

For a longer measurement after the default run:

```bash
bash benchmarks/navigation_gpu/run_mingw64.sh \
    --warmup 10 \
    --iterations 50 \
    --horizon 3 \
    --output navigation_gpu_benchmark.csv
```

## Decision after results

Do not wire the prototype into the game yet. First inspect the six benchmark rows
and answer:

1. GPU median/p95 at 10k actors for `cruise` and `hub`.
2. Whether dense Hub traffic causes any fixed-cell overflow.
3. How many actors survive corridor filtering compared with total actors.
4. Whether neighbor-check growth is acceptable.
5. Whether GPU cost is low enough to justify taking time away from rendering.
6. Whether the next experiment should be GPU-resident conflict processing, a CPU
   BVH/spatial-hash baseline, or a hybrid.

Only then begin `NAV-V2-SPACE-1` (static free-space/clearance representation) or
`NAV-V2-DYN-1` (runtime dynamic NavigationWorld integration).
