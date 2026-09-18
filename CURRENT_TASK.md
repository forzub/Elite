# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Stage:** 12A-5 — first-live exact segment diagnosis

## Candidate HEAD

```text
32bb51739ee3512d25dbaa940f94fd3743152eb6
```

## Run

Only the runtime regression and affected production/self-test need rerun:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

## What each result means

### navigation_runtime planner PASS

The planner itself can handle the exact live-scale case:
1300 m OBB inside ~1420 m first horizon with an unrelated dynamic actor.

### self-test fast FAIL: first live segment misses CUBE 08

Expected diagnostic includes:

```text
first_live_probe_blocked=0
first_live_horizon_m=...
first_live_agent_map=(...)
first_live_goal_map=(...)
first_live_bounded_target_map=(...)
```

That means GameSimulation live geometry differs from the intended first segment.

### self-test fast FAIL: planner lost proven block

```text
first_live_probe_blocked=1
planner_exact_static_block=0
```

That isolates the bug inside planner/LocalAvoidance composition.

### PASS

Still requires:

```text
obstacle_candidate=0
obstacle_conflict=0
configured_route_exact_block=1
exact_obstacle_block=1
exact_static_block=1
adjusted=1
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```

Do not reintroduce stationary spheres.
