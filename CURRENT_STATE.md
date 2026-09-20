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
f626fb0373499928e0ae89585c3bd992e5436c92
```

Results:
- architecture contract PASS;
- runtime 18/19;
- all previously accepted 18 tests remain green;
- final composite is the only failure.

## What now works in the final composite

Newtonian completed the full composite successfully:
- production exact-static detour;
- B7 Extreme -> DriftPass;
- dynamic invalidation;
- first AdjustedClear;
- three persistent-hazard continuation suffixes;
- return to NominalClear;
- narrow portal;
- final StateCapture.

Measured Newtonian composite:
- completed phases: 7;
- dynamic bypass segments: 4;
- replans: 1;
- minimum static clearance: 10.631322 m;
- minimum dynamic clearance: 3.175166 m;
- max hull half-width: 22.276390 m;
- max slip: 33.112982 deg;
- tracking exceeded ticks: 0;
- final P error: 0.022675 m;
- final speed: 0.075007 m/s;
- final forward error: 3.521282 deg;
- total: 109.14 s.

Assisted also proves:
- initial AdjustedClear;
- first authority-bounded replacement;
- two persistent-hazard continuation suffixes;
- all executed continuation segments keep positive physical clearance and zero tracking-envelope violations.

## Newly exposed production quality defect

Assisted failed at persistent-hazard continuation iteration 2:

```
position=(181.667547,59.045896,-14.484331)
hazard=(183.280102,20.668719,0)
target=(175.628419,50.983151,13.773784)
```

The important pattern is the sequence of production adjusted targets:

```
+Z -> +Z -> -Z -> +Z
```

The local planner was changing bypass side between bounded replans.

Root cause in production `LocalAvoidancePlanner`:
- for each minimum deflection ring, azimuths were tested in regenerated local-basis order;
- planner returned the first safe azimuth;
- after vehicle motion, the transverse basis changes;
- "first safe" can therefore switch to the opposite physical side even when the current motion is already committed to a safe side.

Newtonian happened to physically survive this zig-zag. Assisted reached a state where the next opposite-side no-stop transit was no longer physically authorable. This is a real local-planner continuity defect, not merely a test-helper problem.

## Current unverified production fix

Production commits:

```
86640b05145938ec0880a3a26539957aaa72f085
ef2e6ec85823c229d6cadb6aa8dbe5b65e209193
```

Behavior change:
- preserve the existing smallest-deflection-ring priority;
- evaluate all safe azimuths inside that ring;
- choose the safe direction with maximum alignment to current actual velocity;
- deterministic azimuth index remains the tie-break;
- no hidden planner state is introduced;
- no deflection, clearance, horizon or safety limit is widened.

This adds side continuity/hysteresis through current kinematics rather than memory.

## New focused regression

Commit:

```
6064a22f565fd7cc82568b0babf2721891c8d925
```

`NavigationRuntimePlannerTests` now includes a symmetric dynamic-obstacle fixture where:
- +/-Z bypasses are both geometrically valid;
- current vehicle velocity carries a small -Z component;
- selected adjusted target must remain on -Z;
- selected direction must be strongly aligned with current motion.

This regression specifically prevents reintroduction of "first safe azimuth" side flipping.

## Current gate

Final composite remains open. Expected suite remains **19 tests**.

If this production continuity correction works:
- repeated `AdjustedClear` targets should stop alternating sides;
- Assisted should remain physically authorable through the persistent hazard;
- final composite may then expose the next real seam or pass.

No physical, tracking, clearance or terminal acceptance criterion has been weakened.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
