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

## Latest target-machine attempt

Exact tested checkout:

```
69f8ca4dbcb44df5340b94f45640bcb7d6e6ed1a
```

Results:
- architecture contract PASS;
- runtime 17/19;
- failures:
  - `navigation_runtime_planner` focused side-continuity regression;
  - `navigation_composite_proving_ground`.

All other 17 tests remained green.

## Focused regression failure

Failure:

```
adjusted visibility must preserve the current -Z avoidance side
```

The regression fixture itself was invalid:
- it reused the generic `region()` helper;
- that helper fixes Z bounds to [-10,+10] m;
- the test assumed +/-Z probes hundreds of meters away were both statically legal;
- exact-static proof therefore rejected the intended +/-Z candidates before continuity scoring.

This fixture bug is now corrected with a genuinely wide 3D region.

## Composite evidence

Velocity-only continuity did not solve the real production issue.

Newtonian:
- first adjusted target changed to Y-dominant;
- first continuation then moved to +Z;
- next replan selected a target near the opposite Z side;
- test-side physical author could no longer produce a valid no-stop continuation.

The key conclusion is that **instantaneous velocity is not a sufficient ownership signal for avoidance-branch continuity**.

## Architecture correction

The canonical execution loop is:

```
ACCEPT short local segment
 -> EXECUTE
 -> MONITOR
 -> REPLAN
```

The execution/accepted-program layer knows which bounded local segment is currently authoritative.

Therefore continuity belongs to that accepted segment and must be passed explicitly into the next planner call. It must not be reconstructed heuristically from instantaneous velocity alone.

## Current unverified production candidate

Production/API commits:

```
71b80c4529e1bc776e2a2dbf209059a2ccff44f9
4bde26ee2fbb531116f960d089d488067f1bfd6d
76022a5199422dd80ef4caff611539b53c39e731
40d7b9852bc6f265ae02ecc0bf9d7b4e002d5b96
```

Changes:
- `LocalAvoidancePlanner::Query` now accepts:
  - `preferredDirectionValid`;
  - `preferredDirectionMap`.
- `NavigationRuntimePlanner::AgentState` now accepts:
  - `localAvoidanceContinuityValid`;
  - `localAvoidanceContinuityDirectionMap`.
- runtime planner validates and forwards this hint into local avoidance;
- inside the minimum safe deflection ring:
  - explicit accepted-segment direction has priority;
  - instantaneous velocity is only fallback;
  - nominal forward remains the final fallback;
  - deterministic azimuth index still breaks ties.

This introduces no hidden planner state and changes no safety envelope.

## Updated focused regression

Commit:

```
bd31307fbda3d512a579f521efc1664199b1de46
```

The regression now:
- creates a genuinely wide 3D static region;
- makes +/-Z bypass candidates actually legal;
- deliberately sets current velocity away from the desired branch;
- explicitly sets accepted local continuity toward -Z;
- requires the next AdjustedClear to preserve -Z.

This tests ownership correctly: accepted segment continuity wins over incidental current velocity.

## Final composite integration

Commit:

```
09bc81e03cab6c251b50678585161cc73e814ddb
```

The composite now:
- captures the direction of each accepted adjusted target before execution;
- carries that direction across the segment;
- supplies it back to `NavigationRuntimePlanner` on the next bounded replan;
- updates the hint only when a new AdjustedClear segment is accepted;
- prints the active continuity vector in `[COMPOSITE-RESUME]`.

## Current gate

Expected suite remains **19 tests**.

If focused regression passes but composite still flips sides:
- inspect whether direction-only continuity is insufficient and the contract needs a stronger branch/plane identifier.

If 19/19:
- accept final composite;
- close synthetic maneuver behavior lab;
- move primary evaluation into actual NAV STRESS/game.

No tracking, clearance, hull, authority or terminal threshold was weakened.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
