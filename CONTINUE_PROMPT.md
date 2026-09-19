# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

Read first:
1. CURRENT_TASK.md
2. CURRENT_STATE.md
3. PROJECT_STATE.md
4. src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md
5. src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md
6. src/game/navigation/STAGE12_END_TO_END.md
7. src/game/navigation/NAVIGATION_PURITY_CONTRACT.md
8. src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md

## Fresh evidence

Target-machine:
- architecture PASS 0.191 s;
- navigation_runtime 10/10 PASS;
- B5 ordinary compiler PASS;
- 10,000 B5 compiles = 30,495 us = 3,049.5 ns/compile;
- B14 scheduler 5000 actors = 2,801 us;
- client/server BUILD PASS;
- production build 23.544 s.

No rev-parse line was supplied in that excerpt; do not invent exact tested hash.

## Critical ownership rule

Planner side chooses physical maneuver strategy.

B5 may generate:
- RCS Trim;
- LeadRotateMainBurn;
- later CoastAndRotate / FlipAndBurn / etc.

B6 proves exact candidates.
B7 selects among proved candidates.
B8 freezes exact program.

Follower/autopilot B9/B10:
- samples accepted program;
- adds bounded tracking feedback;
- may not decide to rotate hull/use main engine instead of RCS;
- if accepted program cannot be followed, invalidate/replan.

For main-engine-dominant Newtonian craft:
main engine is primary translation authority for material delta-v;
RCS is precision/trim authority.

## Current candidate

After the green B5 gate, compiler semantics were corrected:
a main-engine LeadRotateMainBurn option is now exposed even when Trim is also
physically feasible. B7 still owns final choice.

New regression:
`testRcsFeasibleLateralChangeStillExposesMainEngineOption`.

This correction is unverified on target machine.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh

TIMEFORMAT='[TIMING] build_mingw64 real_s=%R user_s=%U sys_s=%S'
time bash build_mingw64.sh
```

Do not rerun the long live self-test yet.

## Next after green rerun

Build B6 continuous proof for the exact B5 candidate, then integrate
B4 -> B5 -> B6 -> B7 -> B8 and rerun the logged live gate.

Every state-affecting iteration must rewrite this file and CURRENT_TASK and
update CURRENT_STATE / PROJECT_STATE / STAGE12_END_TO_END.
