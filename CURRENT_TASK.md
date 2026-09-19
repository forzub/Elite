# Elite — CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv
**Branch:** `main`

## Target-machine result

Exact tested checkout:

```text
519313ba430b73b557622436ed7df9a5a9a832cf
```

Architecture contract: PASS.

navigation_runtime:
- 13/14 PASS;
- only `maneuver_corner_family_matrix` failed.

## Primary failure

Expert / Newtonian / StopTurnGo:

```text
completed=1
final_pos_error_m=27.257718
final_velocity_error_mps=0.409381
min_corner_speed_mps=2.933115
max_center_cross_track_m=21.026823
max_hull_required_half_width_m=34.330940
corridor_half_width_m=32.0
max_corridor_violation_m=2.330940
tracking_envelope_exceeded_ticks=40
```

The fixture currently advances every compound internal phase by schedule/time.
That was introduced for moving RadiusTurn/DriftTurn phases, but it is wrong for
StopTurnGo.

StopTurnGo semantically requires:
- waypoint capture;
- near-zero speed;
- required braking attitude/capture;
- only then rotate/accelerate onto the outgoing leg.

The current Newtonian sequence continues to the next phase before those states
are actually captured. Because Newtonian has the extra flip/aft-main braking
sequence, the error accumulates strongly. Do not widen the corridor or weaken
the tolerances.

## What already looks healthy

Expert RadiusTurn, both laws:
- completed;
- final position error ~0.115 m;
- velocity error ~0.013 m/s;
- final attitude error ~1.82 deg;
- minimum corner speed ~7.99 m/s;
- slip ~5.85 deg;
- required hull half-width ~17.88 m;
- zero 32 m corridor violation.

Expert DriftTurn, both laws:
- retains 10 m/s;
- material slip ~101 deg;
- final position error ~0.298 m;
- velocity error ~0.0025 m/s;
- required hull half-width ~17.38 m;
- zero corridor violation.

However drift final attitude error is still ~14.38 deg, so after the primary
StopTurnGo fix the drift recovery phase is expected to be the next failing
quality gate.

## Timing caveat

Printed totals:

```text
StopTurnGo 15.18 s
RadiusTurn 12.88 s
DriftTurn 11.16 s
```

are currently authored schedule durations, not valid measured comparative
performance. Since internal phases are time-authored, Newtonian and Assisted
total times are identical by construction.

Real comparison must measure:
- common entry-gate crossing;
- actual maneuver execution/capture;
- common exit/terminal-state capture.

## Current task

1. Split compound phase semantics:
   - StopTurnGo capture phases use terminal-state completion;
   - RadiusTurn/DriftTurn moving phases remain scheduled/program-driven.
2. Keep common rigid-hull corridor and all existing tolerances.
3. Measure actual elapsed entry->exit/terminal time rather than authored sum.
4. Re-run 18-row matrix.
5. Then address drift final attitude recovery if it remains outside 5 deg.

## Do not

- do not widen 32 m corridor to make Newtonian StopTurnGo green;
- do not weaken expert P/V/attitude quality limits;
- do not interpret current fixed totals as real law performance comparison;
- do not run old 120 s obstacle live gate yet.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update canonical architecture/migration/purity docs when ownership/contracts
  change.
