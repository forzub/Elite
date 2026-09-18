# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Stage:** 12A-5 — static/dynamic NavigationWorld ownership cleanup

## Candidate HEAD

```text
adc3603b8572d8aad0d35d57ec94ad33038ba890
```

## What changed

CUBE 08 and other stationary NAV STRESS infrastructure are no longer inserted
into `NavigationMap::DynamicWorldUpdate::actors`.

Stationary collision truth is now exclusively:

```text
HitVolume
 -> NavigationObstacle OBB
 -> NavigationSpace
 -> LocalAvoidance exact segment proof
```

NavigationMap remains active for time-varying infrastructure and future moving
actors.

The live gate requires CUBE 08 to be absent from dynamic ownership while still
causing the same successful maneuver through exact static identity.

## RUN

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

## Required live evidence

```text
obstacle_candidate=0
obstacle_conflict=0
exact_obstacle_block=1
dynamic_queries>0

exact_static=1
exact_static_obstacles>0
exact_static_query=1
exact_static_block=1
max_exact_static_examined>0
exact_static_motion_samples>0
exact_static_violation=0

adjusted=1
lateral_exec=1
progress_m>3500

replication_error_mps2=0
canonical_replication_error_mps2=0
```

Do not reintroduce stationary spheres to make the test pass.
