# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest final-composite target attempt

Exact tested checkout:

```
18f93e15f3baa5218d459b289ae89beb170f1c54
```

Results:
- architecture contract PASS;
- runtime 18/19;
- all previously accepted 18 tests remain green;
- only `navigation_composite_proving_ground` failed.

Production dynamic bypass now works:

```
[COMPOSITE-PLAN]
law=newtonian
status=adjusted_clear
adjusted=1
nominal_dynamic_conflicts=1
primary_conflict=12060
probes=20
ordinary_exhausted=0
nominal_static_blocked=0
position=(138.841366,52.623525,0)
velocity=(9.263267,3.401145,0)
hazard=(185.850434,42.920559,0)
selected_target=(159.856528,40.750781,17.815749)
```

Failure moved downstream:

```
composite replacement did not clear dynamic hazard
```

## Root cause

The production planner selected a valid bounded `AdjustedClear` target.

The test then authored a new 6 s quintic from the live P/V state to that target with an 8 m/s terminal speed.

Offline reconstruction of that exact curve shows approximately:
- peak total acceleration ~5.61 m/s2;
- peak acceleration transverse to the velocity-aligned body ~5.03 m/s2.

The Cobra fixture gives only:
- lateral authority 2.0 m/s2;
- vertical authority 2.0 m/s2;
- and B10 needs feedback reserve.

Therefore the test-side time-program authoring created a physically unproved replacement curve even though the production planner target itself was safe.

This exposes the known honesty boundary: full production B5 Assisted/general physical-program authoring is still incomplete.

## Current unverified fix candidate

Commit:

```
7444c5930586300d6cac48bd4b2fa63b27e96bd6
```

The replacement helper is now capability-aware.

It searches the shortest replacement duration/exit speed satisfying all of:
- dense planned peak transverse feed-forward <= 1.35 m/s2;
- minimum planned speed >= 0.50 m/s;
- minimum planned conservative dynamic clearance >= 1.50 m.

The 1.35 m/s2 feed-forward cap intentionally leaves reserve below the 2.0 m/s2 manoeuvre authority for B10 feedback.

Candidate search:
- duration 8..32 s;
- terminal speeds 4 m/s then 2 m/s;
- 768 dense analytic samples.

New diagnostics:
- `[COMPOSITE-REPLACEMENT]`: duration, exit speed, planned transverse FF, planned minimum speed and planned dynamic clearance;
- `[COMPOSITE-REPLACEMENT-ACTUAL]`: actual clearance, slip, forward tracking error, tracking exceeded ticks, terminal P/V errors.

Actual replacement additionally requires:
- zero tracking-envelope exceeded ticks;
- actual conservative dynamic clearance >0.5 m.

No planner, B7, tracking gains, physical authority, clearance or terminal threshold was weakened.

## Current gate

Final composite remains open. Expected suite: **19 tests**.

If green:
- accept final composite;
- close synthetic maneuver behavior lab;
- move primary evaluation into NAV STRESS/game.

If red:
- use planned vs actual replacement diagnostics to locate the exact remaining composition defect.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
