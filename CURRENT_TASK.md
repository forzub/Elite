# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`

## Accepted state

Revised B5 Newtonian compiler semantics are target-machine green by supplied
evidence:
- architecture contract PASS;
- navigation_runtime 10/10 PASS;
- ordinary_physical_maneuver_compiler PASS;
- RCS-feasible delta-v still exposes a main-engine alternative for B7: PASS;
- 10,000 B5 compiles: 30,824 us total = 3,082.4 ns/compile;
- scheduler 5000 actors: 2,670 us total;
- EliteGame / EliteServer BUILD PASS;
- production build 23.861 s.

The supplied paste did not contain `git rev-parse HEAD`; do not fabricate an
exact tested hash.

## Current task — maneuver execution laboratory

Before implementing B6 proof, measure whether the existing accepted-program
execution chain can follow very simple prescribed motion accurately.

This is NOT a planner/search test.

Execution chain:

```text
pre-authored AcceptedManeuverProgram
    -> B9 ManeuverProgramSampler
    -> B10 ManeuverTrackingController
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> ShipControlState
    -> SharedShipPhysics
    -> DynamicMotionSystem
    -> physical trajectory metrics
```

New fixture:
`tests/navigation_runtime/ManeuverProgramExecutionLabTests.cpp`.

### Scenario A — straight

```text
(0,0,0) -> (0,0,-100 m)
start V = 0
finish V = 0
duration = 20 s
```

Smooth quintic stop-to-stop reference. Peak reverse demand stays below the
physical ~2 m/s2 maneuver authority so no hidden impossible braking command is
required.

Measure:
- final position error;
- final speed;
- maximum geometric cross-track;
- endpoint overshoot;
- completion time.

Initial gate:
- final error <= 3 m;
- final speed <= 1 m/s;
- cross-track <= 1 m;
- overshoot <= 3 m.

### Scenario B — 90-degree two-leg route

```text
START (0,0,0)
    |
    | 100 m
    v
CORNER (0,0,-100)
    -> stop
    -> yaw -90 deg
    -> second leg 100 m
FINISH (100,0,-100)
```

The baseline deliberately stops at the corner before turning. This isolates
segment handoff/orientation tracking from continuous-corner trajectory design.

Measure:
- corner stop error;
- final error;
- final speed;
- overshoot;
- total simulated time.

Initial gate:
- corner error <= 3 m;
- final error <= 5 m;
- final speed <= 1.5 m/s.

### Scenario C — corridor

The same two-leg route is monitored against a **5 m half-width** polyline
corridor.

Measure:
- maximum distance from route polyline;
- maximum corridor violation.

Initial gate:
- no corridor exit.

### Pilot profile

First run uses expert PilotSkill:
- zero reaction delay;
- zero command latency;
- high decision rate/slew.

Purpose: isolate B9/B10 + propulsion/physics before adding pilot-skill latency.
After baseline is known, rerun the same route under realistic pilot profiles.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected CTest count is now **11**. The new test is:
`maneuver_program_execution_lab`.

The verbose diagnostic prints lines like:

```text
[MOVEMENT] scenario=straight_100m arrival=... completed=...
 final_pos_error_m=... final_speed_mps=...
 max_cross_track_m=... max_overshoot_m=... simulated_s=...

[MOVEMENT] scenario=right_angle_100m_100m arrival=...
 corner_error_m=... max_corridor_violation_m=...
```

The lab is allowed to fail. Do NOT relax the thresholds before reading the
metrics. The result tells us whether the problem is:
- terminal braking/settling on a straight line;
- program handoff at the corner;
- attitude tracking;
- corridor tracking;
- or none of the above.

No long 120 s obstacle-navigation live gate is needed for this diagnostic.

## After results

If straight fails:
- debug B9/B10/PilotSkill/physics before B6 integration.

If straight passes but 90-degree route fails:
- debug program transition/attitude/terminal state.

If both pass:
- add realistic pilot latency profile;
- then proceed with B6 proof and later continuous non-stop corner fixture.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update architecture/migration/purity docs when ownership changes.
