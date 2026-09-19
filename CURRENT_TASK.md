# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`

## Last exact target-machine accepted execution baseline

```text
b5f558b18c8ef8e1b5d9562df36b34cb621c648f
```

Fresh supplied evidence:
- architecture contract PASS;
- navigation_runtime 11/11 PASS;
- simple maneuver execution lab PASS;
- B5 compiler PASS;
- scheduler PASS.

Simple execution metrics:
- straight 100 m:
  - final error = 0.0285925 m;
  - final speed = 0.102262 m/s;
  - cross-track = 0;
  - overshoot = 0;
- 90-degree 100 m + 100 m:
  - final error = 0.0285925 m;
  - final speed = 0.102262 m/s;
  - maximum route cross-track = 0.0470123 m;
  - corner error = 0.0285925 m;
  - 5 m corridor violation = 0.

The obsolete unused helper warning in the simple lab has now been removed.

## Current task — 4-leg 3D corridor matrix

Current code candidate before this documentation commit:

```text
d0f8b07787339074e225cd02cfe96c93bad0446a
```

New target:
`maneuver_corridor_matrix`.

The route is a 4-segment 3D polyline:

```text
P0 (  0,   0,    0)
P1 (  0,   0, -120)
P2 ( 85,  35, -190)
P3 ( 25, 100, -265)
P4 (120,  55, -340)
```

Each translation leg is stop-to-stop and uses a smooth quintic accepted
reference with feed-forward sized to remain inside the ~2 m/s2 manoeuvre/RCS
braking authority.

Between legs:
- ship stops at the waypoint;
- accepted program performs a bounded 3D attitude transition;
- next accepted line program starts only after the turn completes.

This is the baseline 3D execution test. A later fixture will remove the stop and
test continuous rounded/high-speed corners.

## Corridor

Corridor half-width:

```text
5.0 m
```

It is measured as a tube around the complete 3D polyline.

Every physics tick records:
- route cross-track;
- active-leg cross-track;
- corridor violation;
- endpoint overshoot;
- follower tracking-envelope status.

## Matrix

Same accepted route is executed in both local control laws:

```text
Newtonian
Assisted
```

Three deterministic pilots:

### expert
- reaction delay: 0;
- command latency: 0;
- decision: 100 Hz;
- response: 10 Hz;
- high slew;
- no command noise.

### competent
Exact current production `NpcAiSystem` baseline:
- reaction delay: 0.12 s;
- decision: 12 Hz;
- latency: 0.06 s;
- response: 2 Hz;
- damping: 0.85;
- linear noise: 0.02 m/s2;
- angular noise: 0.002 rad/s2;
- current production slew/policy values.

### rookie
Deterministic degraded profile:
- reaction delay: 0.30 s;
- decision: 6 Hz;
- latency: 0.12 s;
- response: 1.2 Hz;
- damping: 0.72;
- larger deterministic command noise;
- lower command slew.

Total:

```text
2 laws x 3 pilots = 6 rows
```

## Metrics printed per row

```text
[CORRIDOR-MATRIX]
 pilot=...
 law=...
 valid=...
 completed=...
 legs=.../4
 rotations=.../3
 final_pos_error_m=...
 final_speed_mps=...
 max_route_cross_track_m=...
 max_active_leg_cross_track_m=...
 corridor_half_width_m=5
 max_corridor_violation_m=...
 max_waypoint_error_m=...
 max_overshoot_m=...
 max_forward_angle_error_deg=...
 tracking_envelope_exceeded_ticks=...
 simulated_s=...
```

## First-pass acceptance policy

Do not invent gameplay limits for the lower-skill pilots before measuring them.

Strict control rows:
- expert/Newtonian;
- expert/Assisted.

They must:
- remain valid;
- finish all 4 translation legs and 3 rotations;
- stay inside the 5 m corridor;
- finish within 1.0 m;
- finish <=0.6 m/s residual speed.

Competent and rookie rows are diagnostic on the first target-machine pass:
- they must remain numerically/structurally valid;
- their actual deterministic tracking envelopes are printed;
- completion/corridor behavior is measured before acceptance limits are pinned.

This is intentional. If a lower-skill pilot leaves the corridor, B6 may need to
reserve additional geometry clearance as a function of PilotSkill rather than
pretending all pilots track identically.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected navigation_runtime count: **12**.

New test:
`maneuver_corridor_matrix`.

The run script also repeats it verbosely after the full suite, so all six
`[CORRIDOR-MATRIX]` rows should be visible.

Do not run the old 120 s obstacle-navigation live gate yet.

## Interpretation after first matrix

1. Expert fails:
   execution stack/generic 3D attitude tracking is still wrong; fix before B6.

2. Expert passes, competent grows but stays inside:
   quantify production PilotSkill reserve and feed it into B6.

3. Competent or rookie exits 5 m corridor:
   do not simply widen the test. Determine whether:
   - B10 reserve is insufficient;
   - accepted program must be slower;
   - B6 needs skill-dependent corridor clearance;
   - or that pilot class legitimately cannot be assigned a corridor this narrow.

4. Newtonian vs Assisted differs materially:
   isolate the control-law-specific propulsion/speed-envelope effect.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update architecture/migration/purity docs when ownership changes.
