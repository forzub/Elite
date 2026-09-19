# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`

## Fresh target-machine evidence

Supplied target-machine results:
- architecture contract: PASS, 0.191 s;
- navigation_runtime: 10/10 PASS;
- ordinary_physical_maneuver_compiler: PASS;
- B5 diagnostic: 10,000 compiles = 30,495 us total = 3,049.5 ns/compile;
- navigation_work_scheduler: 5000 actors = 2,801 us total;
- EliteGame / EliteServer: BUILD PASS;
- production build: 23.544 s.

The supplied excerpt did not contain `git rev-parse HEAD`, so do not fabricate
an exact tested B5 hash.

## Control-ownership decision

Automatic navigation is split deliberately:

```text
B4 local geometry
    -> where a path/corridor can go

B5 physical maneuver compiler
    -> HOW this vehicle will move:
       RCS trim?
       lead-rotate?
       main-engine burn?
       coast?
       brake / flip-and-burn?

B6 continuous proof
    -> can this exact maneuver be executed safely?

B7 maneuver decision
    -> which proved maneuver is selected for doctrine/objective?

B8 ACCEPT
    -> freeze exact maneuver program

B9/B10 follower/autopilot
    -> sample + bounded tracking only
```

The follower/autopilot must NOT decide:
"RCS is insufficient, rotate hull and use main engine."
That changes maneuver family and attitude/thrust history, so it belongs on the
planner side and must be proved before ACCEPT.

For main-engine-dominant Newtonian craft:
- main engine is normal translation authority for material delta-v;
- RCS is trim/precision/docking/residual-correction authority.

## B5 correction after green gate

The first green compiler emitted LeadRotateMainBurn only if direct body-axis
feed-forward was already infeasible. That is safe but too narrow.

Revised B5 now:
- keeps Trim when RCS/body-axis authority can perform it;
- ALSO exposes a LeadRotateMainBurn candidate whenever a non-zero Newtonian
  delta-v can be physically compiled;
- publishes `mainEngineCandidateAvailable`;
- keeps `leadRotateRequired` only for cases where direct feed-forward is
  actually infeasible;
- leaves final selection to B7.

New regression:
`testRcsFeasibleLateralChangeStillExposesMainEngineOption`.

Thus B5 generates alternatives; B7 selects. Follower never substitutes engine
strategy.

## Current unverified candidate

The main-engine-option ownership correction was made after the supplied green
gate. It therefore needs one short isolated rerun before B6 begins.

Do NOT run the long 120 s live gate yet.

Run:

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

Expected:
- architecture PASS;
- navigation_runtime 10/10 PASS;
- ordinary_physical_maneuver_compiler PASS;
- new RCS-feasible + main-engine-option regression PASS;
- B5 10k timing visible;
- scheduler PASS;
- EliteGame / EliteServer BUILD PASS.

## Next after rerun

Implement B6 proof around the SAME B5 candidate:
1. capability consistency over all candidate samples/intervals;
2. static exact-HitVolume sweep;
3. bounded dynamic influence proof;
4. proof witness + reserves/revisions;
5. no trajectory mutation inside proof.

Only then integrate B4 -> B5 -> B6 -> B7 -> B8 and rerun the logged live ordered
flight gate.

## Documentation invariant

After every state-affecting iteration:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update canonical architecture/migration/purity docs when ownership changes.
