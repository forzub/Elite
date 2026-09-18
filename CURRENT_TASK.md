# Elite — CURRENT TASK

**Updated:** 2026-09-18
**Stage:** 12A-5 — resume exact-static live avoidance after placement fix

## Candidate HEAD

```text
546a8868b0a25c655310263252877c5f5b48d4c5
```

## Previous result

Placement is correct:

```text
placement_map=(975,-1300,-6200)
expected=(975,-1300,-6200)
error=17.7 micrometres
```

The previous FAIL was only the old 1 micrometre diagnostic threshold.

## RUN

Only the affected architecture/build/live gate is needed:

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

## Expected next evidence

Placement gate should now pass.

Then look for:

```text
first_live_probe_blocked=1
exact_obstacle_block=1
exact_static_block=1
adjusted=1
```

and ultimately:

```text
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```

If a physical exact-static violation occurs, self-test will fail fast with:

```text
violation_entity=...
proving_obstacle_entity=...
violation_start_map=(...)
violation_end_map=(...)
selected_target_map=(...)
planner_status=...
```
