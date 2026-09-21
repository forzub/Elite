# CURRENT TASK — Validate frozen-reference recovery on real physics

Date: 2026-09-22

Status: **CODE CANDIDATE READY / TARGET MINGW64 VALIDATION REQUIRED**

Code candidate before documentation commits:

```text
59ff756996229bf15a122eb0fe43cf0d9a14b245
```

## Failure just reproduced

Viewer configuration:

```text
ASSISTED
EXPERT
STANDARD
START  10.00 m/s
FINISH 10.00 m/s
```

Observed:

```text
Ruckig max speed          11.71 m/s
final physical speed     100.64 m/s
final position error     2411.14 m
reference clock hold     47.70 s
max body/velocity angle  179.99 deg
physical terminal state  MISSED
```

Therefore Stage-1 speed calculation is still sane; execution/reacquisition is
not.

## Root cause

The reference-clock hold froze time on a moving FreeTransit sample but kept
executing that sample's derivatives forever:

```text
held position / attitude
+ non-zero linear A_ff
+ non-zero reference angular velocity
= internally inconsistent frozen reference
```

The bounded B10 correction reserve cannot cancel a permanently replayed
feed-forward acceleration. The frozen angular-velocity target likewise makes
the hull continue rotating around a fixed attitude target.

## Candidate correction

In `ManeuverTrackingController`:

- inside envelope: execute the accepted moving reference unchanged;
- outside envelope:
  - keep position/velocity/attitude errors for reacquisition;
  - set moving-sample linear/angular feed-forward to zero;
  - damp actual angular velocity toward zero;
  - use only the bounded tracking reserve;
- after recovery: resume the accepted moving reference automatically.

Regression:
`testEnvelopeRecoveryNeutralizesFrozenReferenceDerivatives()`.

Viewer:
`fitCamera()` fits authored/accepted scene geometry, not the complete runaway
physical history.

## Required target gate

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

Then launch:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

First re-run exactly:

```text
ASSISTED / EXPERT / STANDARD
10.00 -> 10.00 m/s
```

Acceptance:
- no unbounded speed growth;
- no persistent tumbling;
- reference hold may occur but recovery must converge;
- program phases complete;
- physical terminal state reached;
- final position error <= 5 m;
- final speed within the existing terminal tolerance;
- no coarse static contact;
- F / ВПИСАТЬ keeps the authored scene usable even if a future run fails.

Then re-run the pinned high-speed regression:
`NEWTONIAN / EXPERT / STANDARD, 26.15 -> 11.75 m/s`.

Do not enable dynamic avoidance in this gate.
Do not change the retained route merely to make the execution pass.
