# Navigation GPU Prototype Benchmark

**Stage:** `NAV-V2-GPU-0`  
**Status:** isolated benchmark; not wired into `EliteGame` runtime  
**API floor:** OpenGL 4.3 Core compute shaders + SSBO

## Why this benchmark exists

The previous live navigation experiment proved that replacing the custom spline
with Ruckig is not enough. The expensive part was still the surrounding
route-wide pipeline: dense trajectory materialization, repeated obstacle scans
and synchronous validation. Navigation v2 therefore starts from the opposite
end: measure how cheaply one shared ship-centered navigation world can reduce a
large dynamic scene to a small set of relevant actors before any expensive route
or motion solve runs.

This benchmark deliberately does **not** call `GeometricPathPlanner`,
`TrajectoryGenerator`, `GuidanceTunnel` or Ruckig. It measures the proposed
shared dynamic-data layer in isolation.

## Coordinate contract

The prototype uses one ship-centered working volume:

```text
ship/navigation origin = (0, 0, 0)
world half extent       = 12 km per axis
cell size               = 600 m
```

The benchmark origin is the active ship/navigation origin, not Hub-local space.
A Hub is expected to keep its own authored/local coordinates and publish only the
relevant transformed subset into the active NavigationWorld.

The working frame translates with the active navigation domain. Its axes are
intended to be stable navigation/travel axes, not the instantaneous roll/pitch/
yaw axes of the hull.

## Dynamic actor input

Each actor is a compact std430 record:

```text
position.xyz + radius
velocity.xyz
acceleration.xyz
```

No velocity or acceleration field is stored in every navigation cell. Dynamic
state remains actor-owned; the spatial grid only indexes actors.

For a prediction horizon `T`, pass 1 computes:

```text
p1 = p0 + v*T + 0.5*a*T^2
travel_bound = |v|*T + 0.5*|a|*T^2
swept_sphere = sphere(p0, actor_radius + travel_bound)
```

The sphere is intentionally conservative. It is not the final production swept
shape; it gives the first benchmark a simple no-false-negative dynamic envelope.
A later implementation may replace it with tighter capsules/convex sweeps after
profiling.

## GPU passes

### Pass A — predict + bin + corridor filter

One invocation per actor:

1. predicts endpoint from P/V/A;
2. builds the conservative swept sphere;
3. inserts the actor into a fixed 3D spatial grid;
4. counts actors whose swept sphere can intersect one selected route corridor.

The grid uses per-cell atomic counts and a fixed slot capacity. Overflow is
reported explicitly and makes the result structurally invalid; actors are never
silently discarded as an accepted result.

### Pass B — all-agent neighborhood/conflict candidates

One invocation per actor:

1. derives the conservative cell search radius from its own swept radius and the
   maximum swept radius in the current dataset;
2. scans only potentially relevant cells;
3. de-duplicates actor pairs with `otherId > id`;
4. tests swept-sphere overlap;
5. emits aggregate candidate/conflict counts.

The benchmark intentionally reads back only aggregate counters. It does not copy
a full pair list to the CPU, because the intended runtime architecture keeps
broadphase and most local conflict work GPU-resident or double-buffered.

## Scenarios

Every run evaluates both deterministic scenarios at:

```text
1,000 actors
5,000 actors
10,000 actors
```

`cruise`:
- broad 18 km spawn cube around the ship;
- speeds up to 250 m/s;
- accelerations up to 8 m/s^2;
- 9 km route corridor.

`hub`:
- denser 7 km spawn cube;
- speeds up to 120 m/s;
- accelerations up to 6 m/s^2;
- 5 km local corridor.

The default prediction horizon is 3 seconds.

## Measurements

Each row reports:

- `gpu_bin_ms` — median timestamp time for clear + prediction/binning/corridor;
- `gpu_neighbor_ms` — median all-agent neighbor/conflict pass;
- `gpu_total_ms` — median total GPU latency;
- `gpu_p95_ms` — p95 total GPU latency;
- `cpu_submit_ms` — median CPU command-submission time, excluding query waits;
- `pairs` — conservative possible conflict pairs;
- `neighbor_checks` — candidate slot comparisons actually performed;
- `corridor` — dynamic actors relevant to the selected corridor;
- `occupied_cells`;
- `overflow` and `out_of_bounds`;
- `max_sweep_radius_m`;
- SSBO `memory_mib`;
- `readback_bytes` — currently only the 32-byte aggregate statistics block.

For the 1,000-actor cases, a CPU O(N^2) reference computes the same conservative
swept-sphere pair count and corridor count. `reference_ok=1` is the correctness
gate before performance numbers are trusted.

## Build and run — MSYS2 MinGW64

From repository root:

```bash
python tests/architecture_contracts/check_navigation_gpu_benchmark.py
bash benchmarks/navigation_gpu/run_mingw64.sh
```

Optional arguments are forwarded to the executable:

```bash
bash benchmarks/navigation_gpu/run_mingw64.sh \
    --warmup 10 \
    --iterations 50 \
    --horizon 3 \
    --output navigation_gpu_benchmark.csv
```

The benchmark prints the absolute CSV path.

## Performance interpretation

These are design targets, not machine-independent pass/fail thresholds:

```text
CPU main-thread navigation submission       < 0.5 ms typical
GPU dynamic NavigationWorld update          < 1.0 ms preferred
                                            < 2.0 ms heavy-scene ceiling target
synchronous full-route solve on frame thread  forbidden
```

A GPU result above 2 ms does not fail the executable; hardware and driver differ.
It means the architecture or data representation must be changed before runtime
integration.

The important first questions are:

1. Does 10k-actor cost stay close to the GPU budget?
2. Does dense Hub occupancy create cell overflow or excessive neighbor checks?
3. How many of all actors survive corridor filtering?
4. Is 32-byte aggregate readback sufficient, allowing detailed results to stay
   GPU-resident?
5. Is GPU work cheaper enough than a CPU spatial index to justify competing with
   rendering for GPU time?

A CPU spatial-index baseline is the next comparison if the GPU numbers do not
show a clear advantage.

## What this prototype does not claim

It is **not** the final navigation map and it is **not** collision physics.

The intended v2 split remains:

```text
NavigationWorld
    static free-space / clearance representation
    dynamic actor spatial index + prediction
    route-corridor filtering
    local conflict candidates

Physics / Collision
    shared broadphase candidates where useful
    exact CCD / time of impact / contacts

Damage
    semantic hit ownership
    detach / breach / destruction
    local navigation invalidation after topology changes
```

Small holes do not automatically become navigation openings. A breach is
navigable only when its clearance admits the requesting agent; topological
breaches invalidate/rebuild only the affected navigation region.

Ruckig remains a possible final local kinematic primitive after routing/local
avoidance has selected the next target state. It is not used to discover free
space.
