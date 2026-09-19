# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from current truth. Never incrementally patch stale prompt prose. The recreated prompt must repeat this rule.

## Accepted exact target baseline

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Target evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 16/16 PASS;
- verbose continuous 3D fly-through diagnostics captured.

## Continuous 3D fly-through is accepted

Strict Expert Newtonian and Assisted:
- phases 9/9;
- final P ~0.120 m;
- final speed error ~0.00007 m/s;
- final forward error ~0.00032 deg;
- min route speed ~3.999 m/s;
- max center cross-track ~8.478 m;
- max hull required half-width ~17.474 m;
- corridor violation 0;
- tracking-envelope exceeded ticks 0.

Expert per-corner:

```
35 deg  -> min speed ~7.629 m/s, radius ~90.25 m, slip ~0.033 deg
60 deg  -> min speed ~6.928 m/s, radius ~44.78 m, slip ~0.081 deg
90 deg  -> min speed ~5.657 m/s, radius ~21.09 m, slip ~1.088 deg
120 deg -> min speed ~3.999 m/s, radius ~8.62 m, slip ~1.369 deg
```

## Critical interpretation

Newtonian and Assisted FLY3D metrics are numerically identical for Expert, Competent and Rookie.

Do not misinterpret this as physical equivalence.

The accepted fly-through reference:
- keeps body forward nearly tangent to velocity;
- stays within the same <=2 m/s2 manoeuvre/RCS authority;
- does not demand law-specific reverse/main-thrust behavior;
- generates very small expert slip.

Therefore both laws are expected to execute the same reference almost identically.

Law divergence is separately proven by rigid-body/law-stress tests.

## Current task: speed/doctrine matrix

Canonical doctrine enum from `ManeuverDecisionController`:
- `Rational`;
- `PrecisionRetrieval`;
- `Extreme`;
- `CombatEscape`.

Current selection semantics:
- Rational prioritizes collision-free / mission cost / closing speed / reserve / exposure / time;
- PrecisionRetrieval prioritizes collision-free, low mission cost, low closing speed, larger clearance, then lower entry speed;
- Extreme prioritizes progress and time, then threat exposure, exit speed and damage/impact costs, while hard critical-risk filtering remains above doctrine;
- CombatEscape prioritizes progress, threat exposure, then time and exit speed;
- control-law compatibility is filtered before doctrine ranking.

## Next implementation goal

Create a runtime speed/doctrine test with multiple physically truthful maneuver candidates for the same problem. Candidate choices must differ enough that doctrine and control law can select different outcomes.

Cover at least:
- safe/high-clearance slower passage;
- balanced passage;
- fast/tight passage;
- Newtonian-only high-slip/drift or flip-and-burn option where appropriate;
- Assisted-compatible alternative;
- threat-exposure trade for CombatEscape;
- optional expected-contact candidate only when policy explicitly permits it.

Then execute the selected `AcceptedManeuverProgram` through:
`sampler -> B10 -> PilotSkill -> real physics`.

Acceptance must prove:
- deterministic doctrine selection;
- law incompatibilities rejected before ranking;
- selected candidate physically executes;
- no hidden tolerance/corridor weakening;
- hard critical-risk envelope remains dominant over style/doctrine;
- doctrine differences are visible in speed/time/clearance/slip metrics.

Do not call the current `PrecisionRetrieval` doctrine merely `Precision` in canonical docs.

Do not invent a mapping for older “Freestyle” terminology; locate project evidence first if needed.

**Again: recreate this entire prompt from scratch after every state-affecting iteration.**
