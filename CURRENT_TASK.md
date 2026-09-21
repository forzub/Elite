# CURRENT TASK — validate speed-aware route clearance and deterministic Calculate UX

**Date:** 2026-09-22  
**Status:** IMPLEMENTED / TARGET MINWG64 VALIDATION REQUIRED

## Fixed semantics

### Flight style

There is no Standard speed and no Extreme speed.

FlightStyle owns only static clearance/risk doctrine:
- STANDARD = more safety/maneuver room;
- EXTREME = tighter pass / less clearance when tactically useful.

Scenario/runtime no longer contain:
- standard_speed_mps / extreme_speed_mps;
- standardSpeedMps / extremeSpeedMps.

The current test speed envelope comes from explicit START/FINISH speed requests.

### Speed changes the route

START/FINISH speeds are Stage-1 route inputs.

Higher speed increases a coarse inertial maneuver reserve based on real Cobra angular
rate/acceleration limits. Therefore route support points may move farther from a static
obstacle.

Current Stage-1 diagnostics expose:
- ROUTE PLANNING SPEED
- STYLE CLEARANCE
- ROUTE ADDITIONAL CLEARANCE

This reserve is a coarse B4/Stage-1 allowance only. It is not a replacement for final B6
continuous oriented swept-hull proof.

### Calculate button

No automatic solve occurs when a setting changes.

Dirty ownership:
- speed/style -> route + execution dirty;
- control law/pilot/dynamic toggle -> execution dirty only.

One click on РАССЧИТАТЬ:
- rebuilds Stage-1 if route inputs are dirty;
- executes Stage-2;
- shows a short on-screen calculation log;
- clears dirty state.

After any attempt the button is visually disabled/dim and says РАСЧЕТ ГОТОВ.
It becomes active again only after a setting invalidates the result.

There is no second Execute/ЗАПУСТИТЬ ПОЛЁТ action anymore.

## Regression added

Runtime E2E now compares:
- STANDARD 10/10 m/s;
- STANDARD 40/40 m/s;
- EXTREME 40/40 m/s.

Required:
- 40 Standard detour farther from the wall than 10 Standard;
- 40 Extreme closer than 40 Standard.

## Immediate target validation

Run:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh
```

If that passes, launch:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Viewer checks:
1. initial РАССЧИТАТЬ is enabled;
2. set STANDARD, START=10, FINISH=10 and click once;
3. note white route coordinates/clearance and the short on-screen log;
4. button must become visibly disabled / РАСЧЕТ ГОТОВ;
5. move both speeds to 40 -> button re-enables, old result is marked stale;
6. click Calculate -> white route should move farther from wall;
7. switch only STANDARD -> EXTREME at 40/40 -> button re-enables;
8. Calculate -> route should cut closer than 40/40 Standard;
9. switch pilot or Assisted/Newtonian -> Calculate re-enables, but Stage-1 route should be
   retained and only Stage-2 recomputed.

Send build/test output if anything fails. Do not claim PASS before target evidence.

## Related unresolved navigation work

The previous hull-somersault fix remains a candidate awaiting the same target run:
- FreeTransit sparse angular-acceleration feed-forward removed;
- total follower angular demand capped to physical capability;
- telemetry now exposes ideal vs pilot-executed linear/angular commands.

Dynamic obstacle avoidance remains paused until static maneuver behavior is credible.

## Mandatory state protocol

Every state-affecting iteration:
- update CURRENT_STATE.md;
- update CURRENT_TASK.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- recreate CONTINUE_PROMPT.md from scratch.
