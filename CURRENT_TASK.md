# CURRENT TASK — Target-validate Assisted recovery and sparse attitude program

Date: 2026-09-22

Status: **CODE + REGRESSIONS READY / TARGET MINGW64 VALIDATION REQUIRED**

Code candidate before documentation commits:

```text
13ef6bd731ef6d8e78c75c72bf7a59f524b30bcb
```

## Reproduced failure

```text
ASSISTED / EXPERT / STANDARD
10.00 -> 10.00 m/s

Ruckig max speed          11.71 m/s
final physical speed     100.64 m/s
final position error     2411.14 m
reference clock hold     47.70 s
max body/velocity angle  179.99 deg
physical terminal state  MISSED
```

The route/speed profile did not request 100 m/s. The execution loop ran away.

## Two coupled root causes

1. **Frozen moving-reference derivatives**
   - reference time paused on envelope violation;
   - the fixed sample kept its non-zero linear feed-forward and angular-rate
     derivative;
   - bounded recovery feedback could not cancel an indefinitely replayed
     moving-sample command.

2. **Dense -> sparse attitude-rate alias**
   - FreeTransit keeps <=16 accepted samples;
   - old code copied an instantaneous angular velocity from a dense source
     sample into each sparse sample;
   - B9 interpolated that isolated rate across a long sparse interval, so the
     commanded angular rate could be much larger than the actual sparse basis
     motion.

## Candidate behavior

B10:
- inside envelope: accepted moving reference is authoritative;
- outside envelope: zero moving-sample A_ff/alpha_ff, damp actual hull rate,
  bounded feedback only;
- resume the moving reference after reacquisition.

FreeTransit authoring:
- sparse attitude bases remain authoritative;
- interior angular velocity is re-derived from neighboring sparse bases/times;
- first/last sparse angular velocity = 0;
- sparse angular acceleration feed-forward = 0;
- angular velocity is capability-clamped.

Viewer:
- F / ВПИСАТЬ fits authored/accepted scene geometry, not the complete runaway
  execution history.

## Pinned regressions

- `testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives()`
- `testAssistedLowSpeedDoesNotRunAwayDuringReferenceHold()`
- existing Newtonian high-speed reacquisition: 26.15 -> 11.75 m/s.

## Run now

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

First interactive gate:

```text
ASSISTED / EXPERT / STANDARD
10.00 -> 10.00 m/s
```

Acceptance:
- no speed runaway;
- no persistent hull tumbling;
- recovery may pause reference progress but must converge;
- PROGRAM PHASES COMPLETE: YES;
- PHYSICAL TERMINAL STATE: REACHED;
- final position error <= 5 m;
- final speed within terminal tolerance;
- no coarse static contact;
- F / ВПИСАТЬ remains usable even for a failed run.

Then re-run:
`NEWTONIAN / EXPERT / STANDARD, 26.15 -> 11.75 m/s`.

Do not widen tolerances or alter route geometry to make this pass.
Do not enable dynamic avoidance yet.
