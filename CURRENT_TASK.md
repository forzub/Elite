# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — costed corridor semantics accepted; dedicated scaling benchmark active

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Accepted prior gates

`NAV-V2-MAP-2` is closed with measured hybrid ownership.

Static `NavigationSpace` indexing is accepted:

```text
baseline 10k corridor            ≈1.95-1.99 s
per-region adjacency             ≈21.8-22.4 ms
dense RegionSlot BFS             ≈7.9-8.0 ms
RegionSlot BVH point p95         0.0353 ms open / 0.0099 ms hub
RegionSlot BVH invalidation p95  0.0112 ms open / 0.0115 ms hub
```

Full replace/local patch remain ~44-49 ms median at 10k and stay worker/update-side.

## Costed corridor behavior — ACCEPTED

Fresh target-machine gate:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Accepted static route semantics:

```text
wall_with_aperture
    fitting agent -> through opening
    oversized agent -> rejected

canyon_vs_overflight
    distance-only -> short canyon
    clearance-aware -> longer open route
    oversized canyon agent -> open route
```

Fast `queryCorridor()` remains the topology/BFS oracle. `queryCostedCorridor()` is the policy-aware static selector.

`totalCostMetersEquivalent` is currently a coarse region/portal comparison metric; do not treat it as exact physical trajectory length.

## Accepted moving-goal design

`src/world/navigation/PURSUIT_HORIZON.md` defines future receding intercept/pursuit behavior. Pursuit implementation is later than the current static route-quality gate.

## Active gate — dedicated costed scaling benchmark

New isolated harness:

```text
benchmarks/navigation_space_costed/
```

Pinned topology scales:

```text
open_1k / open_5k / open_10k
hub_1k  / hub_5k  / hub_10k
```

Two policy profiles run against the same published topology:

```text
distance_only
clearance_aware
```

The graph includes deterministic reduced-clearance portals so the second policy performs genuine alternate-route evaluation rather than the same route with a different constant.

Measured products:

```text
median/p95 ms
regions visited
portals examined
region-path length
coarse meter-equivalent cost
```

## RUN NOW

Only the new benchmark contract + benchmark are required. `NavigationSpace.cpp` was not changed after the accepted behavior gate, so do not rerun the old boundary/behavior/scaling suite.

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_costed_benchmark.py
bash benchmarks/navigation_space_costed/run_mingw64.sh
```

Send the complete output.

Default run is intentionally modest:

```text
warmup=1
iterations=5
```

## Decision after measurement

Use the measured 10k p95 rather than intuition:

- `<=15 ms p95`: accept deterministic Dijkstra reference as-is and proceed to turn/curvature cost;
- `15-40 ms p95`: acceptable as worker/reference solve, but optimize queue/search core before using it frequently for many active NPC replans;
- `>40 ms p95` or pathological scaling: optimize costed search first, likely indexed heap and/or admissible A* heuristic, before adding more cost terms.

Do not compare costed timing directly to the BFS oracle as equivalent work. Do not yet wire this into live `EliteGame` / `EliteServer`, implement pursuit runtime, or remove legacy migration navigation.
