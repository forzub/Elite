# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — static-space reference accepted; adjacency optimization pending target-machine rerun

## Source of truth

`main` is the only game-development baseline. Read `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel development branches.

## Closed prior gate

`NAV-V2-MAP-2` selected hybrid ownership:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local search

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Fresh target-machine evidence:

```text
NAVIGATION SPACE BOUNDARY CONTRACT: PASS
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

Public `NavigationSpace.h` remains backend-neutral. The current CPU reference is free-space AABB regions + explicit portals with agent-envelope clearance, fail-closed invalidation and transactional local patching.

## Baseline scaling evidence — ACCEPTED

Benchmark contract passed and the first target-machine run completed with `warmup=1`, `iterations=3`.

10k results:

```text
open_10k
    regions=10000 portals=28600
    point med/p95      0.2787 / 0.2812 ms
    corridor med/p95   1991.0357 / 2008.2074 ms
    invalidate med/p95 2.5731 / 2.9869 ms
    patch med/p95      7.6135 / 7.8543 ms
    portals examined   285,913,245

hub_10k
    regions=10000 portals=28600
    point med/p95      0.2784 / 0.2845 ms
    corridor med/p95   1951.6453 / 1956.0686 ms
    invalidate med/p95 2.8412 / 3.2230 ms
    patch med/p95      8.6678 / 8.7083 ms
    portals examined   285,913,245
```

From 1k to 10k, corridor time grew roughly 102-105x and portal examinations roughly 106-108x. Root cause is the original BFS scanning the complete portal map for every visited region.

Raw evidence: `benchmarks/navigation_space/RUN_LOG.md`.

## Optimization 1 — private per-region adjacency

Implemented on `main` without changing `NavigationSpace.h`:

```text
regionId -> ordered portalId list
```

Properties:

- rebuilt transactionally with `replaceStaticWorld()` and `applyLocalPatch()`;
- stable PortalId ordering preserves deterministic BFS tie-breaking;
- invalidation keeps the same topology and only changes validity flags;
- corridor BFS now examines only portals adjacent to the current region;
- architecture contract rejects regression to a full portal-map scan inside BFS.

Point lookup, invalidation and whole-map transactional patch copying remain deliberately unchanged so the next bottleneck can be measured cleanly.

## RUN NOW

Because `NavigationSpace.cpp` changed, rerun behavior + architecture before remeasuring:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

Send the complete output of all four commands.

## Decision after optimized measurement

Do not add another optimization before seeing the rerun.

Expected next candidates, chosen only from evidence:

- if corridor is now cheap but invalidation dominates: add spatial region index usable by point lookup + invalidation;
- if patch dominates: replace full transactional map copying/rebuild with bounded/chunked topology updates;
- if corridor remains too expensive: compact graph bookkeeping / costed graph representation becomes next;
- only add hierarchy/bricks if measured scale requires them.

Live `EliteGame` / `EliteServer` integration remains later. Do not repair the old route-wide planner or delete migration code yet.
