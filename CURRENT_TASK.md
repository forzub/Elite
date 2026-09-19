# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target baseline

Exact tested checkout:

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Results:
- architecture PASS;
- navigation runtime **17/17 PASS**;
- B7 speed/doctrine matrix PASS.

## Closed block

The B7 lab/runtime selection gate is accepted.

Measured doctrine behavior:
- Rational -> balanced;
- PrecisionRetrieval -> precision;
- Extreme/Newtonian -> high-slip Newtonian drift dash;
- Extreme/Assisted -> common-law fast path;
- CombatEscape -> low-threat path;
- critical-risk=0.90 reckless shortcut rejected above doctrine.

Every selected AcceptedManeuverProgram executed through follower/PilotSkill/real physics with zero tracking-envelope exceed ticks and positive full-hull clearance.

## Current task

Build the next laboratory gate:

**chained transitions + negative / physical-limit matrix**.

### Part A — chained physical transitions

At least one continuous compound run must exercise:
1. normal moving transit;
2. hard moving turn;
3. materially different maneuver family;
4. braking/capture or precision terminal state.

The handoff must preserve actual P/V/q/omega state rather than resetting the vehicle between programs.

Candidate chain:

```
fast transit
 -> moving radius/fly-through turn
 -> Newtonian drift OR Assisted aligned turn
 -> precision braking/capture
```

Strict checks:
- no state reset at phase seams;
- no hidden stop unless the selected family requires it;
- no tracking-envelope exceed;
- full rigid-hull corridor/obstacle safety;
- terminal P/V/attitude capture.

### Part B — negative/limit cases

Required cases:
- insufficient turn room;
- insufficient braking distance;
- too-narrow rigid-body corridor;
- no law-compatible maneuver candidate;
- newly invalidated accepted program / obstacle change.

Expected behavior must be explicit:
- pre-ACCEPT rejection;
- alternative maneuver selection;
- fail-closed recovery/braking;
- or program invalidation + replan.

A negative case passes when the system refuses the impossible unsafe maneuver correctly. It does not need to reach the original target.

## Exit criterion for this block

The block closes when:
- compound state handoffs remain physically continuous;
- impossible maneuvers are rejected before execution;
- invalidated programs do not continue blindly;
- no test relies on widening tolerances after failure.

After this block, the remaining laboratory gate is one final composite end-to-end proving ground before primary evaluation moves into the game.

## Iteration rule

After every code/evidence change, synchronize all project MD files and recreate `CONTINUE_PROMPT.md` from scratch.
