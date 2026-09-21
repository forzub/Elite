# CONTINUE PROMPT — Elite Navigation: validate frozen-reference recovery

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. **recreate this `CONTINUE_PROMPT.md` from scratch again**.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp`
- `src/game/navigation/ManeuverTrackingController.cpp/.h`
- `src/game/navigation/ManeuverProgramSampler.cpp/.h`
- `src/game/navigation/TrajectoryFollower.cpp/.h`
- `src/game/navigation/ManeuverPhaseGate.cpp/.h`
- `tests/navigation_runtime/ManeuverTrackingControllerTests.cpp`
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`.

## Current candidate

Code baseline before documentation commits:

```text
59ff756996229bf15a122eb0fe43cf0d9a14b245
```

Target-machine acceptance is NOT yet established.

## Latest reproduced failure

Interactive viewer:

```text
ASSISTED / EXPERT / STANDARD
START  10.00 m/s
FINISH 10.00 m/s
```

Evidence:

```text
Ruckig OK
calculated max speed       11.71 m/s
program phases complete    NO
physical terminal state    MISSED
final speed                100.64 m/s
final position error       2411.14 m
reference clock hold       47.70 s
max body/velocity angle    179.99 deg
coarse static contact      YES
```

Do not misdiagnose this as the route/Ruckig speed calculator. The accepted
trajectory remained near 10-12 m/s.

## Root cause now fixed in candidate

The reference-clock hold froze a **moving** sample in time. The fixed pose still
carried non-zero trajectory derivatives:
- linear acceleration feed-forward;
- reference angular velocity;
- potentially angular feed-forward.

That turned reacquisition into an indefinite acceleration/spin command. B10's
bounded feedback reserve could not cancel the frozen feed-forward.

New B10 behavior:
- inside envelope: accepted moving reference unchanged;
- outside envelope: geometric reacquisition target, zero A_ff/alpha_ff, damp
  actual angular velocity toward zero, bounded feedback only;
- after physical recovery: accepted moving reference resumes automatically.

Regression:
`testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives()`.

## Camera fix

`NavigationRuntimeViewer::fitCamera()` now ignores the complete physical
execution history when calculating normal scene fit. A runaway ship can no
longer make the route/obstacles microscopic. The trace itself is still rendered.

## Existing contracts that remain

- reference clock progress pauses while `follower.trackingErrorExceeded`;
- phase gate uses the same delayed reference time;
- physical navigation does not switch off on envelope violation;
- FreeTransit longitudinal deadbands are evaluated before the envelope;
- B10 owns tracking feedback/damping;
- lower physics owns actual capability clamping;
- requested control-law selector and effective displayed law remain separate;
- dynamic avoidance is still disabled for this static gate.

## Immediate target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Re-run first:
`ASSISTED / EXPERT / STANDARD, 10.00 -> 10.00 m/s`.

Required result:
- no runaway acceleration;
- no persistent tumbling;
- reference hold converges instead of feeding the failure;
- program phases complete;
- physical terminal state reached;
- final position error <= 5 m;
- terminal speed within tolerance;
- no coarse static contact;
- F / ВПИСАТЬ produces a usable nominal scene scale.

Then re-run the existing high-speed case:
`NEWTONIAN / EXPERT / STANDARD, 26.15 -> 11.75 m/s`.

Do not claim PASS without target evidence.
Do not enable dynamic avoidance yet.
If the Assisted 10 -> 10 run still fails, inspect the new telemetry around the
FIRST `follower_reacquiring` frame before changing route geometry or widening
tolerances.
