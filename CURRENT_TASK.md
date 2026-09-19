# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

Exact target-tested checkout:

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Results:
- architecture PASS;
- navigation runtime 16/16;
- continuous 3D fly-through accepted with verbose target diagnostics.

## Closed stage: continuous 3D fly-through

Expert Newtonian and Assisted both complete the 5-segment ~35/60/90/120 deg route with:
- 9/9 phases;
- zero corridor violation;
- zero tracking-envelope exceed ticks;
- min route speed ~3.999 m/s;
- max hull half-width ~17.474 m inside 32 m;
- final P ~0.120 m;
- final speed error ~0.00007 m/s;
- final attitude error ~0.00032 deg.

Measured Expert turn radii:
- 35 deg -> ~90.25 m;
- 60 deg -> ~44.78 m;
- 90 deg -> ~21.09 m;
- 120 deg -> ~8.62 m.

The Newtonian/Assisted rows are numerically identical because this trajectory is tangent-aligned and remains inside the same manoeuvre/RCS authority. This test proves continuous 3D execution, not law divergence.

## Current task: speed/doctrine matrix

Canonical code doctrines:
- `Rational`;
- `PrecisionRetrieval`;
- `Extreme`;
- `CombatEscape`.

Need a test that gives the decision layer physically meaningful alternatives instead of one identical trajectory.

At minimum provide candidate alternatives that differ in:
- transit time;
- entry/exit speed;
- minimum clearance;
- slip/drift or body-alignment cost;
- maneuver family;
- law compatibility;
- threat exposure where relevant;
- expected contact / damage only where explicitly allowed.

The execution side must then run the selected accepted program through:
`B9/B10 -> PilotSkill -> real physics`.

The goal is to prove:
1. doctrine selects different physically truthful candidates when the trade changes;
2. Newtonian/Assisted filter incompatible maneuver families before ranking;
3. selected programs remain executable and corridor-safe;
4. Extreme/CombatEscape do not bypass hard survival constraints;
5. PrecisionRetrieval genuinely trades time/speed for clearance/control margin;
6. Rational behaves as the balanced baseline.

Do not use “Freestyle” as a canonical doctrine name until historical project evidence explicitly maps it.

## Iteration rule

After every code/evidence change, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
