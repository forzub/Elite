# Elite — CURRENT TASK

**Updated:** 2026-09-18
**Stage:** 12A-5 — identify first physical exact-static violation

## Why this is not another full rerun

The last run already proved:
- stationary CUBE 08 stays out of NavigationMap;
- exact CUBE 08 is detected;
- the first live bounded segment hits it;
- planner selects adjusted avoidance;
- runtime unit composition passes.

Only the physical execution still violates exact geometry.

## RUN

Only diagnostics/build/self-test are needed:

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

The self-test now stops on the first exact-static physical intersection.

Read the line:

```text
violation_entity=...
proving_obstacle_entity=...
violation_start_map=(...)
violation_end_map=(...)
selected_target_map=(...)
planner_status=...
```

Interpretation:

```text
violation_entity == proving_obstacle_entity
    physical execution cuts through CUBE 08

violation_entity != proving_obstacle_entity
    chosen CUBE 08 bypass enters another exact HitVolume
```

That distinction determines the next code change. Do not reintroduce stationary
spheres or weaken exact-static acceptance.
