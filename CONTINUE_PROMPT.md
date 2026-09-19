# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Mandatory workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. Every recreated prompt must repeat this same rule.

## Exact accepted target baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Target evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime **17/17 PASS**;
- speed/doctrine matrix PASS.

## Newly accepted B7 behavior

The B7 lab/runtime gate is closed.

Expected and observed choices:
- Rational -> `balanced`;
- PrecisionRetrieval -> `precision`;
- Extreme Newtonian -> `newtonian_drift_dash`;
- Extreme Assisted -> `fast`;
- CombatEscape -> `low_threat_escape`.

A faster `reckless_shortcut` with criticalRisk=0.90 was rejected while preferred-risk candidates existed.

Every selected `AcceptedManeuverProgram` then executed through:
`TrajectoryFollower/B10 -> PilotSkill -> SharedShipPhysics/DynamicMotionSystem`
with zero tracking-envelope exceed ticks.

Representative measured rows:
- balanced: 24.0 s, actual clearance ~13.31 m, peak ~9.75 m/s, slip ~1.77 deg;
- precision: 30.0 s, clearance ~22.30 m, peak ~7.37 m/s, slip ~1.98 deg;
- Newtonian Extreme drift dash: 18.0 s, clearance ~2.62 m, peak ~14.11 m/s, slip ~34.08 deg;
- Assisted Extreme fast: 20.0 s, clearance ~7.59 m, peak ~12.28 m/s, slip ~2.08 deg;
- CombatEscape: 22.5 s, clearance ~13.45 m, peak ~10.81 m/s.

B7 selection behavior is accepted, but final ordinary-live production wiring through B7 remains a later integration task.

## Current active laboratory task

Build **chained transitions + negative / physical-limit matrix**.

### Chained transition requirement

One compound execution must preserve real state across program boundaries:

```
moving transit
 -> hard continuous turn
 -> law-specific maneuver
 -> precision braking/capture
```

No vehicle reset is allowed at seams.

Measure:
- P/V continuity;
- attitude/angular-rate continuity;
- seam tracking error;
- minimum speed;
- slip;
- full-hull corridor/obstacle clearance;
- terminal P/V/q.

Newtonian may use a high-slip family where physically proved.
Assisted must use an Assisted-compatible alternative.

### Negative/limit requirement

Add deterministic cases for:
1. insufficient turn room;
2. insufficient braking distance;
3. corridor narrower than full rigid hull;
4. no control-law-compatible candidate;
5. accepted program invalidated by new obstacle/world evidence.

A correct negative result is one of:
- reject before ACCEPT;
- choose a different proved candidate;
- bounded fail-closed brake/recovery;
- invalidate accepted program and trigger replan.

Never continue an invalid/impossible program merely to make progress.

## Block exit criterion

Close this block only when:
- cross-family handoffs are physically continuous without resetting state;
- impossible programs cannot reach ACCEPT;
- world invalidation stops obsolete execution;
- hard geometry/capability constraints remain stronger than doctrine.

After this block, run one final composite end-to-end proving ground. If that passes, stop extending laboratory behavior tests and move the primary evaluation loop into the game.

**Again: recreate this entire prompt from scratch after every state-affecting iteration.**
