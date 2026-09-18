# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Stage:** 12A-4 — corrected exact static HitVolume OBB gate

## Candidate HEAD

```text
a40d41bcfff4aecee94b495c0c5dd64223c7fe2b
```

## What the previous run proved

On `80b90a6abc43982f3c6da9f76e20a05b2cd25d46`:
- architecture PASS;
- NavigationSpace PASS;
- both production builds PASS;
- exact static publication/query/block evidence was live;
- physical progress reached 6382.44 m.

The local test failure was an invalid test fixture.
The runtime-planner failure exposed a real portal-boundary contract edge.
The live failure exposed an obsolete sphere-clearance acceptance criterion.

All three are corrected in this candidate.

## RUN NOW

Only affected gates need rerun:

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

## Expected live evidence

```text
exact_static=1
exact_static_obstacles>0
exact_static_query=1
exact_static_block=1
max_exact_static_examined>0
exact_static_motion_samples>0
exact_static_violation=0

obstacle_candidate=1
obstacle_conflict=1
adjusted=1
lateral_exec=1
progress_m>3500

replication_error_mps2=0
canonical_replication_error_mps2=0
```

`min_conservative_clearance_m` may be negative. That now means only that the
route entered an enclosing broadphase sphere; it is not a collision if the
exact swept HitVolume proof stays clean.
