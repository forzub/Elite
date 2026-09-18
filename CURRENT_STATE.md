# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Current public HEAD:** `cee6bbc0b376580dbea98e6f62079edc10038cbb`

## Major progress

```text
[██████████████████████░] 11 / 12 major stages closed
```

## Stage 12

### 12A-1 — ACCEPTED
Live NavigationWorld composition seam.

### 12A-2 — ACCEPTED
Authoritative GameSimulation proving actor.

### 12A-3 — ACCEPTED
Live CUBE 08 avoidance through real physics and exact same-tick replication.

### 12A-4 — ACCEPTED

Accepted on:

```text
8f5801b9e26cfbb8e4e5e3174587a900292e8394
```

Target-machine evidence:
- architecture PASS;
- navigation_space 1/1 PASS;
- navigation_local 2/2 PASS;
- navigation_runtime 3/3 PASS;
- EliteGame/EliteServer PASS;
- 17 exact static HitVolume obstacles;
- 3431 swept physical-motion samples;
- exact_static_violation=0;
- >3.5 km progress;
- exact sparse/canonical replication equality.

### 12A-5 — CANDIDATE

Static/dynamic ownership cleanup.

First target-machine run on
`f7e86a70fe590ab02cb72f02967f3679840c1e24` proved the ownership split:

```text
obstacle_candidate=0
obstacle_conflict=0
dynamic_queries=6000
max_dynamic_candidates=1
exact_static=1
exact_static_obstacles=17
exact_static_query=1
exact_static_violation=0
```

So stationary CUBE 08 really stayed out of NavigationMap while exact static
geometry remained active.

The run still failed because the live proving actor naturally drifted ~500 m
from the mathematical start->goal centerline before reaching CUBE 08, so the
real OBB was no longer on the ship's bounded nominal segment:

```text
exact_obstacle_block=0
exact_static_block=0
adjusted=0
max_route_deviation_m=500.232
progress_m=6821.79
```

This is a proving-fixture defect, not a reason to reintroduce stationary
conservative spheres.

Corrected candidate:
- CUBE 08 is now 1300 m ahead of the starting ship, inside the first bounded
  local horizon before rotating-frame drift can bypass it;
- start X/Y are derived directly from CUBE 08 coordinates;
- at static publication time NavigationSpace proves that the configured
  start->goal centerline intersects the exact HitVolume of the actual CUBE 08
  entity;
- the server self-test fails immediately if that fixture proof is false.

Required ownership result remains:

```text
obstacle_candidate=0
obstacle_conflict=0
configured_route_exact_block=1
exact_obstacle_block=1
adjusted=1
exact_static_violation=0
```

Stage remains **12A-5 CANDIDATE** pending the corrected live rerun.
