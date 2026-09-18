# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Stage:** 12A-4 — exact static HitVolume OBB / narrow-passage geometry

## Accepted baseline

12A-3 is accepted on:

```text
9246530e5eb539e227a1af69461113f2b30ec93a
```

The live server proved CUBE 08 detection, adjusted target selection, pilot
execution, physically bounded motion, positive clearance, >3.5 km goal
progress, and exact same-tick sparse/canonical replication truth.

## Candidate under test

Candidate HEAD:

```text
4e8b0f6fcc35e7ecdce3d4e37762156b357b59e9
```

Current code adds:

```text
NavigationSpace::StaticSpaceUpdate::obstacles
NavigationSpace::queryPoint() exact obstacle rejection
NavigationSpace::querySegment() exact inflated OBB/capsule/sphere proof
LocalAvoidance exact nominal-segment proof
LocalAvoidance exact adjusted-probe proof
live NAV STRESS HitVolume OBB publication in map frame
```

The static layer is sourced from the same authoritative `HitComponent`
volumes used by collision/damage. No render-mesh substitute is introduced.

Two critical regressions are pinned:

1. a rotated exact OBB blocks a point/segment with blocker identity;
2. a 4 m aperture remains traversable for a fitting envelope even though the
   two obstacles' conservative enclosing spheres overlap the centreline; the
   oversized envelope must fail.

The live server gate now additionally requires:

```text
exact_static=1
exact_static_obstacles>0
exact_static_query=1
exact_static_block=1
max_exact_static_examined>0
```

before the previously accepted CUBE 08 physical/replication evidence may pass.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_space/run_mingw64.sh
bash tests/navigation_local/run_mingw64.sh
bash tests/navigation_runtime/run_mingw64.sh

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

## Expected

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS

navigation_space:
    PASS
    exact static OBBs reject occupied points/segments
    exact OBB gap survives overlapping conservative spheres

navigation_local:
    2/2 PASS

navigation_runtime:
    3/3 PASS

EliteGame build PASS
EliteServer build PASS

[NAV-SELFTEST] ... exact_static=1 exact_static_obstacles=>0 exact_static_query=1 exact_static_block=1 max_exact_static_examined=>0 ...
               obstacle_candidate=1 obstacle_conflict=1 adjusted=1 ...
               min_conservative_clearance_m=>0 progress_m=>3500 ...
               replication_error_mps2=0 canonical_replication_error_mps2=0

[PASS] navigation-runtime CUBE 08 caused authoritative avoidance
       with exact static HitVolume geometry, positive clearance
       and replicated execution
```

Do not weaken OBB inflation, aperture size, behavior thresholds or replication
equality to obtain PASS. A failure should identify either exact geometry
publication, static segment proof, local avoidance composition, physical
execution or replication.
