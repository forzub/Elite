# Elite — CURRENT TASK

**Updated:** 2026-09-18
**Stage:** 12A-6a — live rotating-object angular-motion publication

## Candidate HEAD

```text
f556c36a47c4ecb6ebeb713b41f6443527a16f76
```

## What is being tested

One real scene object already rotates:

```text
GUIDANCE DOCK CUBE A
angular velocity = 2 deg/s around hub-visual local Z
```

12A-6a proves this physical motion survives:

```text
scene authority
    -> world angular velocity
    -> NavigationMap working-frame transform
    -> dynamic Candidate
```

Expected magnitude:

```text
2 deg/s = 0.034906585... rad/s
```

No avoidance behavior is changed yet.

## RUN

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_map/run_mingw64.sh

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

## Expected

NavigationMap unit test:
```text
NAVIGATION MAP CONTRACT TESTS: PASS
 - ship-centered rebase / stable basis / angular motion
```

Live result adds:
```text
rotating_actor_seen=1
rotating_actor_omega_verified=1
rotating_actor_omega_error=0
```

The already accepted 12A-5 evidence must remain green:
```text
obstacle_candidate=0
exact_obstacle_block=1
adjusted=1
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```

If angular publication fails, do not touch MovingGapPredictor yet; fix the
motion DTO/frame boundary first.
