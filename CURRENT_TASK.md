# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — costed static corridor semantics pending target-machine behavior gate

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Accepted prior gates

`NAV-V2-MAP-2` is closed with measured hybrid ownership.

`NAV-V2-SPACE-1` boundary/reference is accepted. Static graph/index performance progression is also accepted:

```text
baseline 10k corridor            ≈1.95-1.99 s
per-region adjacency             ≈21.8-22.4 ms
dense RegionSlot BFS             ≈7.9-8.0 ms
RegionSlot BVH point p95         0.0353 ms open / 0.0099 ms hub
RegionSlot BVH invalidation p95  0.0112 ms open / 0.0115 ms hub
```

The static ordinary-query side is now comfortably below the main-thread budget. Full replace/local patch are ~44-49 ms median at 10k because graph + BVH are rebuilt transactionally; keep them worker/update-side for now.

Raw evidence: `benchmarks/navigation_space/RUN_LOG.md`.

## Accepted moving-goal design

`src/world/navigation/PURSUIT_HORIZON.md` defines receding moving-goal/intercept pursuit. It is not the active implementation task yet.

## Active candidate — costed corridor v1

Fast `queryCorridor()` remains the deterministic topology/BFS oracle.

New public static-space products:

```text
CorridorCostPolicy
CostedCorridorResult
queryCostedCorridor(query, policy)
```

Policy:

```text
distanceWeight
preferredClearanceMultiple
clearancePenaltyMeters
```

Static route cost v1:

```text
edge cost = distanceWeight * geometric_distance
          + clearance_penalty
```

Traversal remains fail-closed on physical fit. Clearance penalty only chooses among already traversable alternatives.

Pinned behavior cases:

```text
wall_with_aperture
    small/fitting agent -> portal through opening
    oversized agent -> no route through opening

canyon_vs_overflight
    distance-only -> short canyon
    clearance-aware -> longer open overflight
    agent too large for canyon -> overflight regardless of preference
```

This is the first explicit contract that NPCs may fly through holes/tunnels/canyons rather than treating the enclosing obstacle as one solid keep-out volume.

## RUN NOW

Only the architecture + behavior gate is required. The accepted scaling benchmark does not need to be repeated because the existing BFS/BVH hot paths were not changed.

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
```

Send the complete output.

## After PASS

1. accept aperture/canyon static route semantics;
2. add a small dedicated costed-corridor performance benchmark before using it broadly at 10k topology scale;
3. then add the next semantic term only from gameplay need: turn/curvature cost first, dynamic traffic/risk later in the combined NavigationWorld/local planner;
4. pursuit runtime remains after static route quality is accepted;
5. live `EliteGame` / `EliteServer` integration remains later.

Do not delete legacy migration navigation yet and do not move transactional topology rebuild onto the frame path.
