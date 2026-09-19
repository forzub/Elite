# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Status

The DriftTurn corner-family defect is **closed and accepted**.

Exact tested checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

Target result:
- architecture PASS;
- runtime 15/15;
- expert DriftTurn final attitude ~0.03884 deg;
- zero expert tracking-envelope violations;
- zero corridor violation;
- moving attitude capture succeeded.

## Current task

Design and implement the next **mixed-angle multi-segment 3D corridor** quality gate.

### Purpose

Move from isolated/orthogonal maneuver validation to chained 3D route execution where:
- segment directions are not axis-aligned;
- successive direction changes exercise yaw + pitch composition;
- route progress continues through multiple maneuver transitions;
- rigid-body corridor occupancy is measured continuously.

### Required coverage

At minimum:
- 3-4 connected 3D segments;
- mixed turn angles, not only 90 deg;
- at least one segment with all X/Y/Z components;
- full hull corridor-width measurement;
- final P/V/attitude metrics;
- tracking-envelope exceed count;
- Newtonian + Assisted;
- expert strict gate;
- competent/rookie diagnostic rows.

Prefer to reuse the existing accepted maneuver/follower execution path rather than introducing test-only control shortcuts.

## Iteration rule

After every state/evidence change, update all state MD files and recreate `CONTINUE_PROMPT.md` from scratch.
