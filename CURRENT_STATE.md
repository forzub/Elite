# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted exact target-machine baseline

Exact tested checkout:

```
9435725206b88f0ae953f058a294b1d6a7608e78
```

Evidence:
- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **16/16 PASS**.
- continuous `maneuver_fly_through_3d`: PASS.
- verbose FLY3D diagnostics captured successfully.

## Accepted continuous 3D fly-through measurements

Route:
- 5 connected moving segments;
- ~35 / 60 / 90 / 120 deg turns;
- nominal endpoint speed 8 m/s;
- full Cobra OBB measured against 32 m half-width corridor.

Strict Expert, both laws:
- phases: 9/9;
- final P error: ~0.120 m;
- final speed error: ~0.00007 m/s;
- final attitude error: ~0.00032 deg;
- minimum route speed: ~3.999 m/s;
- maximum center cross-track: ~8.478 m;
- maximum required hull half-width: ~17.474 m;
- corridor violation: 0;
- maximum forward tracking error: ~1.474 deg;
- tracking-envelope exceeded ticks: 0.

Per-corner Expert measurements:

| angle | min speed | observed min radius | max slip | max hull half-width |
|---:|---:|---:|---:|---:|
| 35 deg | ~7.629 m/s | ~90.25 m | ~0.033 deg | ~14.59 m |
| 60 deg | ~6.928 m/s | ~44.78 m | ~0.081 deg | ~17.47 m |
| 90 deg | ~5.657 m/s | ~21.09 m | ~1.088 deg | ~16.20 m |
| 120 deg | ~3.999 m/s | ~8.62 m | ~1.369 deg | ~14.48 m |

Competent and Rookie also complete 9/9 in both laws with zero tracking-envelope violations. Rookie reaches larger tracking/slip error, as expected, but remains diagnostic rather than strict.

## Key finding: Newtonian vs Assisted are identical in this test

For Expert, Competent and Rookie, the measured Newtonian and Assisted FLY3D rows are numerically identical.

This does **not** mean the two flight laws are physically equivalent.

This particular reference:
- keeps body forward closely tangent to velocity;
- keeps corner feed-forward inside the same <=2 m/s2 manoeuvre/RCS authority;
- does not request a large law-specific longitudinal/reverse maneuver;
- produces only small expert slip (~0.03 to ~1.37 deg).

Therefore both laws execute essentially the same bounded physical command.

The earlier rigid-body/law-stress tests still prove the laws diverge when the maneuver actually exercises:
- Newtonian one-directional main thrust / flip-and-burn;
- Assisted fore-main reverse authority / controlled-speed envelope;
- materially different body/velocity alignment.

## Capability accepted

The planner/follower/physics stack can execute continuous chained 3D fly-through with:
- nonzero corner speed;
- mixed 3D turn angles;
- continuous attitude/velocity reference;
- strict full-hull corridor occupancy;
- both local control laws;
- all PilotSkill rows measured.

The fly-through mechanism is accepted. No further tuning is needed here.

## Current task

Proceed to **speed/doctrine coverage**.

Canonical doctrine names in code are:
- `Rational`;
- `PrecisionRetrieval`;
- `Extreme`;
- `CombatEscape`.

Do not silently rename `PrecisionRetrieval` to `Precision`, and do not assume an old “Freestyle” label maps to any current doctrine until the historical contract is located.

The next test should create physically distinct maneuver candidates so doctrine and control law can genuinely change:
- selected family;
- speed profile;
- slip/drift;
- clearance/radius;
- time-to-objective;
- threat exposure / risk trade where applicable.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` **from scratch**.
