# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — static turn-cost v2 candidate pending target-machine behavior gate

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Accepted prior gates

Static indexing and costed corridor v1 are accepted.

Key accepted target-machine evidence:

```text
10k coarse BFS corridor          ≈7.3-8.2 ms
10k BVH point p95                <=0.0353 ms
10k local invalidation p95       <=0.0115 ms
```

Costed corridor v1 behavior:

```text
wall aperture: fitting agent passes, oversized agent rejected
canyon vs overflight: distance/clearance policy selects the expected branch
```

Dedicated costed-scaling benchmark:

```text
open_10k distance_only p95       8.1733 ms
open_10k clearance_aware p95     8.5020 ms
hub_10k  distance_only p95       7.7609 ms
hub_10k  clearance_aware p95     9.6103 ms
```

The predeclared threshold was `<=15 ms p95`, so Dijkstra v1 is accepted as-is. Do not optimize it into A* merely because a more sophisticated algorithm exists.

Raw evidence: `benchmarks/navigation_space_costed/RUN_LOG.md`.

## Active candidate — static turn cost v2

Design contract:

```text
src/world/navigation/STATIC_TURN_COST.md
```

Public policy field:

```text
turnPenaltyMetersPerRadian
```

Fast-path invariant:

```text
turnPenaltyMetersPerRadian == 0
    -> keep accepted v1 RegionSlot Dijkstra
```

Turn-aware invariant:

```text
turnPenaltyMetersPerRadian > 0
    -> search state = (RegionSlot, incoming PortalId)
```

This is mandatory because turn cost depends on arrival direction. Region-only state is incorrect for turn-aware routing.

Turn angle is coarse/static only:

```text
incoming portal center -> current region center
current region center  -> outgoing portal center
```

The special start state has no turn penalty because static `CorridorQuery` does not own current ship heading/velocity. Speed-dependent turn radius, angular acceleration and braking remain local/precision-planner concerns.

Pinned new acceptance fixture:

```text
zigzag_vs_smooth
    turnPenalty=0
        -> slightly shorter zig-zag branch

    positive turnPenalty
        -> smoother branch once saved turn cost exceeds the distance delta
```

Existing aperture and canyon fixtures must remain unchanged.

## RUN NOW

`NavigationSpace.cpp`, behavior tests and the architecture contract changed. Run only the static-space architecture + behavior gate:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
```

Send the complete output.

Do **not** rerun the accepted CPU/GPU/static/costed scaling benchmarks for this behavior gate. The zero-turn fast path remains separate by construction.

## After PASS

1. accept static turn-cost semantics;
2. add a small dedicated turn-aware performance benchmark because expanded `(region,incoming portal)` state can cost materially more than v1;
3. if turn-aware scaling is acceptable, stop adding static policy terms for now;
4. next combine accepted static corridor with dynamic conflict/local-horizon selection;
5. then implement pursuit/receding-intercept consumer;
6. live game/server integration remains after isolated NavigationWorld contracts are stable.

Do not put ship velocity, dynamic traffic or pursuit prediction into persistent `NavigationSpace` static cost.
