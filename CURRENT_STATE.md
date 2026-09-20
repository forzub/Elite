# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted exact target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted target evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Closed laboratory blocks

Accepted:
- StopTurnGo / RadiusTurn / DriftTurn;
- long moving attitude;
- full Cobra rigid-body corridor;
- non-orthogonal 3D route;
- continuous 3D fly-through;
- speed/doctrine select -> real execution;
- chained cross-family execution with no P/V/q/omega reset;
- StateCapture terminal semantics;
- fail-closed turn/braking/hull/law/invalidation limits.

## Current unverified candidate: FINAL composite proving ground

New source:

```
tests/navigation_runtime/NavigationCompositeProvingGroundTests.cpp
```

CTest:

```
navigation_composite_proving_ground
```

Expected full runtime suite: **19 tests**.

Candidate commits:
- `ecef36520a0957300b70f1469fcd47d209dcb7e0` — initial composite test;
- `9a74a03845003a302db38ec2102c5d688e7c214b` — Assisted control-mode enum correction;
- `0eaf1b5faddef336d4110d9b306f609cef5b0a8c` — CMake registration;
- `753d6027b76c99498132a0e4f8ecf533cf7cbcbe` — verbose runner integration.

## Composite scenario

For Newtonian and Assisted, one uninterrupted expert vehicle run exercises:

1. **Exact static blockage + production topology**
   - direct start -> final exact-static segment is blocked by `composite_static_wall`;
   - production `NavigationSpace/NavigationRuntimePlanner` selects a two-portal detour;
   - first portal is wide.

2. **Physical execution to first portal**
   - B8 AcceptedManeuverProgram;
   - B9/B10 follower/tracking;
   - PilotSkill;
   - SharedShipPhysics/DynamicMotionSystem.

3. **B7 Extreme law-specific choice**
   - Newtonian may select faster `DriftPass`;
   - Assisted must filter NewtonianOnly drift and select aligned `PrecisionTransit`.

4. **Mid-program dynamic invalidation**
   - only a prefix of the selected program executes;
   - a new NavigationMap dynamic actor is published ahead;
   - production `NavigationExecutionReplanPolicy` must return immediate LocalHorizon / DynamicHazardInvalidated;
   - obsolete accepted program is not allowed to continue.

5. **Production dynamic local bypass**
   - the new hazard is published through real `NavigationMap`;
   - `NavigationRuntimePlanner` must detect nominal dynamic conflict and return `AdjustedClear`;
   - replacement physical program starts from the actual live state at invalidation; no reset.

6. **Narrow second portal**
   - route resumes the original static topology;
   - full Cobra hull must fit a 19 m half-width passage;
   - centerline-only acceptance is insufficient.

7. **Final precision StateCapture**
   - final P/V/attitude capture at the objective.

## Composite strict checks

Per law:
- expected B7 family:
  - Newtonian -> DriftPass;
  - Assisted -> PrecisionTransit;
- exactly one dynamic-hazard replan;
- zero tracking-envelope exceeded ticks;
- conservative full-hull static clearance > 0.5 m;
- conservative dynamic-hazard clearance > 0.5 m;
- narrow passage full-hull envelope <= 19 m;
- final P <= 1.0 m;
- final speed <= 0.60 m/s;
- final forward error <= 4 deg;
- Newtonian material slip >=15 deg;
- Assisted total max slip <=8 deg.

Output:
`[COMPOSITE]` per law with family, phases, replan, clearances, hull, slip, tracking and terminal metrics.

## Important honesty boundary

The composite uses production:
- NavigationSpace;
- NavigationRuntimePlanner;
- NavigationMap;
- ManeuverDecisionController;
- NavigationExecutionReplanPolicy;
- AcceptedManeuverProgram execution path;
- B9/B10;
- PilotSkill;
- authoritative ship physics.

However physical time-program authoring in this final laboratory fixture is still test-side.

Reason:
- full production B5 Assisted/general-family compiler migration remains explicitly incomplete.

Therefore a green final composite will close **synthetic maneuver behavior testing**, not falsely claim all B1-B6/B11 production migration is complete.

## If final composite passes

Laboratory maneuver behavior testing is closed.

Next task becomes actual game/NAV STRESS:
- visualize accepted corridor;
- visualize accepted physical trajectory/tunnel;
- run real NPC/autopilot;
- inspect behavior visually;
- create focused regressions only for real defects found there.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` from scratch.
