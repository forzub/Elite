# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

## Start here

Read, in order:
1. `CURRENT_TASK.md`
2. `CURRENT_STATE.md`
3. `PROJECT_STATE.md`
4. `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
5. `src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md`
6. `src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md`
7. `src/game/navigation/STAGE12_END_TO_END.md`

## Exact latest target-machine evidence

Tested checkout:

```text
519313ba430b73b557622436ed7df9a5a9a832cf
```

Architecture contract: PASS.
navigation_runtime: 13/14 PASS.
Only `maneuver_corner_family_matrix` failed.

Do not claim a newer checkout is target-machine accepted until the user supplies
fresh evidence.

## What the test actually proved

The navigation execution core is already substantially healthy.

Strong:
- rigid-body Cobra geometry and actuator directionality;
- Newtonian vs Assisted physical distinction;
- follower P/V tracking;
- RadiusTurn;
- physical high-slip DriftTurn;
- deterministic PilotSkill degradation.

Not yet good enough:
- StopTurnGo compound phase handoff;
- post-drift attitude recovery;
- actual Newtonian-vs-Assisted elapsed-time comparison;
- general B6/B7 planner integration.

### Healthy expert RadiusTurn

Both laws approximately:
- P error 0.115 m;
- V error 0.013 m/s;
- attitude error 1.82 deg;
- min corner speed 7.99 m/s;
- slip 5.85 deg;
- hull half-width 17.88 m;
- no corridor violation;
- no tracking-envelope exceed.

### Physically successful expert DriftTurn

Both laws approximately:
- 10 m/s retained;
- 101 deg slip;
- P error 0.298 m;
- V error 0.0025 m/s;
- hull half-width 17.38 m;
- no corridor violation;
- no tracking-envelope exceed;
- BUT final attitude error ~14.38 deg.

Therefore drift mechanics work; exit attitude recovery does not yet meet the
5 deg expert quality target.

## Primary failure

Expert/Newtonian/StopTurnGo:

```text
P error                   27.257718 m
V error                    0.409381 m/s
minimum corner speed       2.933115 m/s
center cross-track        21.026823 m
required hull half-width  34.330940 m
corridor half-width       32.0 m
corridor violation         2.330940 m
tracking exceeded ticks   40
```

Root cause:
the fixture currently hands every compound phase off by authored time.

That is valid for moving internal RadiusTurn/DriftTurn reference phases, but
invalid for StopTurnGo.

Canonical architecture is now:
- StopTurnGo capture phases are state-gated;
- RadiusTurn/DriftTurn moving internals may be schedule/program driven;
- time expiry can never stand in for waypoint/velocity/attitude capture.

Do not widen corridor or weaken tolerances.

## Critical timing warning

Current totals:
- StopTurnGo 15.18 s;
- RadiusTurn 12.88 s;
- DriftTurn 11.16 s.

Do NOT use these as performance rankings.
They are largely authored durations, so Newtonian/Assisted equality is partly
constructed.

Real timing must be:
common entry-gate timestamp -> actual common exit/terminal-state capture.

## Next code change

Implement explicit compound phase modes, for example:

```text
ScheduledMoving
StateCapture
```

`StateCapture` behavior:
- execute reference normally until nominal program end;
- after end, continue tracking the terminal reference;
- advance only when terminal P/V/attitude/omega envelope is satisfied;
- otherwise bounded timeout -> explicit failure;
- never silently advance because time expired.

Use StateCapture for StopTurnGo:
- braking/corner capture;
- attitude capture before outgoing acceleration.

Use ScheduledMoving for:
- continuous RadiusTurn internal phases;
- continuous DriftTurn internal phases.

Then:
- measure actual entry->terminal time;
- rerun 18 CORNER-MATRIX rows;
- rerun 9 CORNER-COMPARE rows.

Expected current CTest count remains 14 unless implementation adds a separate
isolated test.

## Next quality gate

Expert:
- StopTurnGo really stops/captures;
- RadiusTurn remains green;
- DriftTurn keeps high slip/speed and exits <=5 deg attitude error;
- all remain inside the same 32 m rigid-hull corridor;
- final P/V thresholds remain unchanged.

Only after this gate:
- compose 3-4 segment mixed-angle 3D corridor;
- compare real route time for Newtonian vs Assisted;
- compare PilotSkill;
- add doctrine/speed profiles: Rational / Freestyle / Extreme;
- then move the validated physical candidates toward B6/B7.

## Repository workflow invariant

Work directly in the repo.

After every state-affecting project event:
- rewrite `CURRENT_TASK.md`;
- rewrite `CONTINUE_PROMPT.md` from the current true state;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update architecture/migration/purity docs when contracts change.

Always distinguish:
- last actually target-machine tested checkout;
- newer unverified code/docs HEAD.

Never weaken tests merely to obtain green.
