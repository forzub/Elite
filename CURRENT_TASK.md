# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Stage:** 12A-5 — corrected static/dynamic ownership live gate

## Candidate HEAD

```text
cee6bbc0b376580dbea98e6f62079edc10038cbb
```

## Previous run explained

The previous target-machine run proved:
- all architecture/unit/runtime tests PASS;
- production builds PASS;
- CUBE 08 is no longer a NavigationMap dynamic candidate/conflict;
- exact-static geometry is still published, queried and collision-free.

It failed because the proving ship had ~3.1 km to travel before CUBE 08 and
naturally accumulated ~500 m lateral drift. The real OBB was therefore no longer
on the live nominal segment, even though the authored start->goal centerline
crossed the cube.

## Correction

The proving obstacle is now 1300 m ahead at spawn, already inside the first
bounded local horizon.

Additionally, before the flight begins the static NavigationSpace snapshot must
prove that the configured start->goal line intersects the exact HitVolume
belonging to CUBE 08.

New diagnostic:

```text
configured_route_exact_block=1
```

If it is 0, self-test fails immediately with a proving-fixture error rather than
running for 120 simulated seconds.

## RUN

The navigation_space/local/runtime libraries were already green and were not
changed by this correction. Re-run only the affected gate:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

## Required live evidence

```text
obstacle_candidate=0
obstacle_conflict=0
configured_route_exact_block=1
exact_obstacle_block=1
dynamic_queries>0
max_dynamic_candidates>0

exact_static=1
exact_static_obstacles>0
exact_static_query=1
exact_static_block=1
exact_static_motion_samples>0
exact_static_violation=0

adjusted=1
lateral_exec=1
progress_m>3500

replication_error_mps2=0
canonical_replication_error_mps2=0
```

Do not reintroduce stationary spheres to force avoidance.
