# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Task

Determine whether the remaining DriftTurn exit-attitude miss is a **general continuous angular-tracking problem** or a **DriftTurn-specific recovery/reference problem**.

## Latest verified target state

Exact tested checkout:

```
be4686f4dcba419f451813b1ddc246088c145e48
```

Stage-12 architecture PASS; runtime 14/15.

Important result:
- expert Newtonian StopTurnGo is now healthy;
- RadiusTurn is healthy;
- expert DriftTurn reaches the correct P/V corridor exit but finishes about 10.325 deg off the required attitude;
- requirement remains <=5 deg at the common exit.

## Candidate under test

```
5c16bedc25c422f2c79ae5def14396839ef4ee7c
```

Adds a separate long-arc probe:
- 180 deg;
- R=80 m;
- v=10 m/s;
- ~251 m / ~25.1 s of continuous curved flight;
- expert/competent/rookie;
- Newtonian/Assisted.

The probe measures attitude correction **while moving**, not only at the endpoint.

### Diagnostic decision

- If expert long-arc angular tracking is clean while DriftTurn still exits around 10 deg wrong, fix the DriftTurn recovery/reference construction.
- If expert long-arc tracking also accumulates material angular lag, inspect B9/B10 angular sampling/tracking before touching maneuver authoring.

Do not relax the existing DriftTurn 5 deg terminal gate.

## Run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py
bash tests/navigation_runtime/run_mingw64.sh
```

## After evidence

Record the exact tested HEAD and update all state MD files before the next mechanism change. Recreate `CONTINUE_PROMPT.md` from scratch every iteration.
