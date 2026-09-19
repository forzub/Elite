# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`

## Accepted evidence

Revised B5 semantics are target-machine green by supplied evidence:
- architecture contract PASS;
- navigation_runtime 10/10 PASS;
- ordinary_physical_maneuver_compiler PASS;
- RCS-feasible delta-v still exposes a main-engine alternative for B7: PASS;
- 10,000 B5 compiles: 30,824 us total = 3,082.4 ns/compile;
- scheduler 5000 actors: 2,670 us total;
- EliteGame / EliteServer BUILD PASS;
- production build 23.861 s.

The supplied paste did not contain `git rev-parse HEAD`; do not invent an exact
tested B5 hash.

## Current task — maneuver execution laboratory

Before B6, measure whether an already-authored AcceptedManeuverProgram is
actually executed accurately by the current autopilot/physics stack.

Current code candidate before documentation commits:

```text
78fd1e356138f94f6e6b8990053d80fdc419eb4d
```

Execution chain:

```text
AcceptedManeuverProgram
 -> B9 ManeuverProgramSampler
 -> B10 ManeuverTrackingController
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> SharedShipPhysics
 -> DynamicMotionSystem propulsion
 -> DynamicMotionSystem translation
 -> measured trajectory
```

This deliberately bypasses route search and obstacle planning.

### Scenario A — straight stop-to-stop

```text
START  (0,0,0)
FINISH (0,0,-100 m)
Vstart = 0
Vend   = 0
duration = 20 s
```

Smooth quintic reference with braking demand kept inside the real ~2 m/s2
maneuver-thruster authority.

Measure:
- final position error;
- final residual speed;
- maximum geometric cross-track;
- maximum overshoot;
- completion time.

Initial limits:
- final error <= 3 m;
- final speed <= 1 m/s;
- cross-track <= 1 m;
- overshoot <= 3 m.

### Scenario B — two 100 m legs at 90 degrees

```text
START (0,0,0)
    |
    | 100 m
    v
CORNER (0,0,-100)
    -> stop
    -> yaw -90 deg
    -> 100 m
FINISH (100,0,-100)
```

The baseline stops at the corner before yawing. A later fixture may test a
continuous curved/non-stop corner.

Measure:
- corner capture error;
- final position error;
- final speed;
- overshoot;
- total simulated time.

Initial limits:
- corner error <= 3 m;
- final error <= 5 m;
- final speed <= 1.5 m/s.

### Scenario C — corridor

The complete two-leg route is monitored against a 5 m half-width polyline
corridor.

Measure:
- maximum cross-track from the polyline;
- maximum corridor violation.

Initial requirement:
- no corridor exit.

### Pilot profile

First pass uses expert execution:
- reaction delay = 0;
- command latency = 0;
- high decision rate;
- high command slew.

Purpose: isolate B9/B10 + physics. After baseline behavior is known, repeat the
same route under realistic pilot-skill delays.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected navigation_runtime count: **11**.

New target:
`maneuver_program_execution_lab`.

Verbose output will contain:

```text
[MOVEMENT] scenario=straight_100m arrival=...
 completed=...
 final_pos_error_m=...
 final_speed_mps=...
 max_cross_track_m=...
 max_overshoot_m=...
 simulated_s=...

[MOVEMENT] scenario=right_angle_100m_100m arrival=...
 corner_error_m=...
 max_corridor_violation_m=...
```

The lab is intentionally allowed to fail. Do not relax thresholds before
inspecting the metrics.

## Interpretation

- straight FAIL -> debug terminal execution/follower/PilotSkill/physics before B6;
- straight PASS + 90-degree FAIL -> debug segment transition/attitude/corner capture;
- both PASS -> repeat with realistic pilot latency, then proceed to B6;
- corridor-only FAIL -> inspect tracking envelope/corner geometry.

Do not run the long 120 s obstacle-navigation live gate for this experiment.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update architecture/migration/purity docs when ownership changes.
