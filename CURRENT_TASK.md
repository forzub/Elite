# Elite — CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv
**Branch:** `main`

## Last exact target-machine checkout

```text
519313ba430b73b557622436ed7df9a5a9a832cf
```

Evidence:
- architecture contract PASS;
- navigation_runtime 13/14 PASS;
- only `maneuver_corner_family_matrix` failed.

## Current quality assessment

The current system is not generally broken.

What is already strong:
- rigid-body Cobra dimensions and actuator-source truth are working;
- Newtonian and Assisted are physically distinct;
- continuous follower P/V tracking is accurate;
- RadiusTurn is already high quality;
- a real high-slip DriftTurn is physically executable;
- PilotSkill changes execution quality in a deterministic and measurable way.

What is not yet good enough:
- StopTurnGo compound phase orchestration;
- drift exit-attitude recovery;
- honest measured Newtonian-vs-Assisted timing;
- full B6/B7 production integration.

## Primary failure to repair

Expert / Newtonian / StopTurnGo:

```text
final_pos_error_m=27.257718
final_velocity_error_mps=0.409381
min_corner_speed_mps=2.933115
max_center_cross_track_m=21.026823
max_hull_required_half_width_m=34.330940
corridor_half_width_m=32.0
max_corridor_violation_m=2.330940
tracking_envelope_exceeded_ticks=40
```

Root cause:
- all compound internal phases currently advance by authored program time;
- that is acceptable for continuous moving RadiusTurn/DriftTurn phases;
- it is invalid for StopTurnGo capture phases;
- Newtonian therefore advances flip -> brake -> rotate -> exit before actual
  waypoint / near-zero-speed / attitude capture is complete.

Do NOT widen the corridor or weaken tolerances.

## Architectural correction already committed

Family-specific phase handoff is now canonical:

### StopTurnGo
State-gated:
1. capture waypoint;
2. capture near-zero velocity;
3. capture required attitude / angular-velocity envelope;
4. only then release the next phase.

Program duration is a reference horizon, not evidence of capture.

### RadiusTurn / DriftTurn
Moving internal reference phases may remain schedule/program driven.

External maneuver completion remains:
- common outgoing gate crossed;
- exit P/V/attitude envelope satisfied.

Time expiry may never pretend a failed capture succeeded.

## What the successful rows proved

### Expert RadiusTurn

Both Newtonian and Assisted:

```text
final P error       ~0.115 m
final V error       ~0.013 m/s
final attitude      ~1.82 deg
min corner speed    ~7.99 m/s
max slip            ~5.85 deg
hull half-width     ~17.88 m
corridor violation  0
tracking exceed     0
```

This is already a strong continuous-turn result.

### Expert DriftTurn

Both laws:

```text
min corner speed    10.0 m/s
max slip            ~101 deg
final P error       ~0.298 m
final V error       ~0.0025 m/s
hull half-width     ~17.38 m
corridor violation  0
tracking exceed     0
final attitude      ~14.38 deg
```

Conclusion:
- powered drift works physically;
- trajectory and speed tracking are strong;
- exit attitude recovery is insufficient.

### PilotSkill differentiation

RadiusTurn:
- competent: ~0.19 m P error, ~4.18 deg final attitude;
- rookie: ~0.41-0.47 m P error, ~0.64 m/s V error, ~8.93 deg attitude.

DriftTurn:
- competent: ~9.97 m/s retained, ~47.3 deg final attitude,
  188 envelope-exceeded ticks;
- rookie: ~9.88 m/s retained, ~4.39 m P error, ~8.30 deg attitude,
  135 envelope-exceeded ticks.

PilotSkill is therefore already a real execution-envelope input, not cosmetic
variation.

## Timing is NOT accepted yet

Current printed totals:

```text
StopTurnGo 15.18 s
RadiusTurn 12.88 s
DriftTurn 11.16 s
```

are authored schedule durations.

They are not valid performance results because Newtonian/Assisted equality is
partly built into the fixture.

Valid timing must be measured:
- common entry-gate crossing timestamp;
- actual state/capture progression;
- common outgoing terminal gate timestamp.

## Next implementation task

1. Add two internal phase execution modes:
   - `ScheduledMoving`;
   - `StateCapture`.
2. Use `StateCapture` for StopTurnGo brake/waypoint/attitude phases.
3. Keep RadiusTurn/DriftTurn moving phases scheduled.
4. Extend capture phases beyond nominal reference end while follower continues
   tracking the terminal sample, until:
   - terminal envelope is satisfied; or
   - explicit bounded timeout/failure occurs.
5. Measure actual entry->exit/terminal time.
6. Re-run all 18 CORNER-MATRIX rows and 9 CORNER-COMPARE rows.
7. If StopTurnGo becomes correct, address DriftTurn exit attitude recovery
   without weakening the 5 deg expert exit requirement.

## Acceptance order

Do not proceed to the 3-4 segment mixed-angle corridor until:
1. expert StopTurnGo is physically correct;
2. expert RadiusTurn stays green;
3. expert DriftTurn exits with valid attitude;
4. timing is actual measured timing, not authored duration.

Then:
- build 3-4 segment 3D corridor;
- compare Newtonian/Assisted;
- compare PilotSkill;
- later add doctrine/speed profiles (Rational / Freestyle / Extreme);
- feed real candidates into B6 proof and B7 decision.

## Do not

- do not widen the 32 m corridor to hide StopTurnGo failure;
- do not weaken expert P/V/attitude thresholds;
- do not treat current 15.18/12.88/11.16 s as benchmark rankings;
- do not run the old 120 s obstacle live gate yet.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update canonical architecture/migration/purity docs when ownership/contracts
  change.
