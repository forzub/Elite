# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this rule.

## Accepted exact target baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Chained acceptance

Newtonian:
- 4/4 phases;
- no P/V/q/omega seam reset;
- max slip 35.114714 deg;
- hull half-width 17.419551 m / 25 m;
- final P 0.024611 m;
- final speed 0.024960 m/s;
- final forward 0.029227 deg;
- tracking exceeded 0.

Assisted:
- 4/4 phases;
- no seam reset;
- aligned phase max slip 1.139399 deg;
- max chain slip 1.934322 deg;
- hull 17.419551 m / 25 m;
- final P 0.024611 m;
- final speed 0.024960 m/s;
- final forward 0.035921 deg;
- tracking exceeded 0.

Negative contracts all PASS:
- insufficient turn horizon -> NoPhysicalCandidate;
- stopping reserve 110 m > 60 m available -> reject;
- Cobra needs 13.238202 m half-width > 12 m available -> reject;
- Assisted cannot select all-NewtonianOnly candidate population;
- dynamic hazard -> immediate LocalHorizon replan, old accepted execution stops.

## Current task

Build the **single final composite end-to-end laboratory proving ground**.

This is the last synthetic navigation behavior gate.

## Composite scenario requirements

One uninterrupted expert scenario for Newtonian and Assisted must combine:
- authoritative-style static topology/obstacles;
- wide then narrow passage pressure;
- acceleration and hard moving turn;
- B7 doctrine/control-law-dependent physical alternative;
- B8 accepted physical program;
- B9/B10 follower/tracking;
- PilotSkill;
- SharedShipPhysics/DynamicMotionSystem;
- a dynamic hazard appearing while a program is already accepted;
- NavigationExecutionReplanPolicy invalidating that obsolete program;
- replacement program from actual current state, with no reset;
- final precision P/V/attitude capture.

Use production components directly wherever available. Do not write a second fake production planner inside the test.

## Output

One `[COMPOSITE]` row per law:
- law;
- doctrine / selected family;
- phases;
- replans;
- invalidation reason;
- minimum full-hull clearance;
- maximum slip;
- tracking exceeded ticks;
- terminal P/V/forward error;
- total simulated seconds.

## Final lab acceptance

If this composite passes on exact target-machine checkout:
- record exact tested HEAD;
- declare laboratory maneuver behavior testing complete;
- next task becomes in-game NAV STRESS trajectory/corridor visualization and live behavior review.

Do not continue inventing synthetic maneuver matrices after that unless a real game defect requires a focused regression.

## Architecture nuance

A green final lab does not magically close remaining production migration/generalization work:
- B1/B2/B3/B4;
- full B5 Assisted/general compiler;
- generalized B6 ownership;
- explicit B11 bounded reflex;
- final ordinary-live B7-B10 seam retirement.

Those are implementation/integration tasks, not reasons to keep extending the isolated maneuver laboratory.

**Again: recreate this prompt from scratch after every state-affecting iteration.**
