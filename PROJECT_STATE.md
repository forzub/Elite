# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Canonical architecture

```
Navigation geometry / corridor
 -> physical maneuver compiler
 -> continuous proof
 -> maneuver decision
 -> AcceptedManeuverProgram
 -> sampler
 -> trajectory follower / bounded tracking
 -> PilotSkill
 -> authoritative propulsion / physics
```

Planner authors physically truthful maneuver candidates. ManeuverDecisionController chooses among them by doctrine. Follower tracks the selected accepted program with bounded residual authority.

## Accepted target baseline

Exact tested checkout:

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Accepted:
- Stage-12 architecture PASS;
- navigation runtime 16/16;
- strict StopTurnGo / RadiusTurn / DriftTurn;
- long continuous angular tracking;
- rigid-body law-specific braking;
- non-orthogonal 3D stop-to-stop corridor;
- continuous multi-corner 3D fly-through with full-hull proof.

## Continuous 3D fly-through result

Expert route metrics:
- min speed ~3.999 m/s;
- max center cross-track ~8.478 m;
- max hull half-width ~17.474 m;
- zero 32 m corridor violation;
- zero tracking-envelope violations;
- final P ~0.120 m;
- final attitude ~0.00032 deg.

Observed Expert turn radii:
- 35 deg: ~90.25 m;
- 60 deg: ~44.78 m;
- 90 deg: ~21.09 m;
- 120 deg: ~8.62 m.

Newtonian and Assisted produced numerically identical rows for all three PilotSkill profiles.

Interpretation: this accepted path stays nearly tangent-aligned and within shared manoeuvre/RCS authority, so it does not expose law-specific propulsion differences. Law divergence remains proven by separate rigid-body/law-stress gates.

## Current roadmap item

Build a **speed/doctrine matrix** that makes maneuver choice matter.

Canonical doctrine enum:
- Rational;
- PrecisionRetrieval;
- Extreme;
- CombatEscape.

Candidate generation must expose meaningful tradeoffs before ranking:
- safe slow/high-clearance;
- balanced;
- fast/tight;
- Newtonian-only drift/flip where physically appropriate;
- Assisted-compatible alternatives;
- threat-optimized escape;
- explicit contact/damage candidates only when allowed.

The test must verify both selection and real execution of the selected accepted program.

## Roadmap after doctrine gate

1. Accept speed/doctrine execution matrix.
2. Locate/reconcile any older “Freestyle” terminology from project history without inventing a mapping.
3. Move toward visible in-game evaluation of the accepted maneuver behaviors.

## State protocol

After every state-affecting event, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
