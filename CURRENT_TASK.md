# CURRENT TASK — execute navigation blueprint M1

Date: 2026-09-22

Status: **ARCHITECTURE SPECIFIED / IMPLEMENTATION NOT STARTED**

Primary specification:

```text
src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md
```

## Objective

Implement migration stage **M1 — repair coordinate and API boundaries** without
changing motion behavior or tuning around the existing physical-planning defect.

The current active stand may only be trusted in an identity navigation frame.
`tools/navigation_runtime/NavigationScenarioRuntime.cpp` contains a private
`toSystemIntent()` that copies NavLocal vectors into System vectors instead of
using the canonical `NavigationFrameBoundary`. Initial transforms are also
written across that boundary directly. The existing lexical architecture
checks fail on harmless formatting yet do not catch this semantic bypass.

## Required M1 changes

1. Inventory every NavLocal/System/World conversion in the active runtime.
2. Delete the tool-local conversion implementation.
3. Route control-intent conversion through `NavigationFrameBoundary`.
4. Convert initial position, orientation, linear state and angular state through
   the same canonical boundary.
5. Make time/frame snapshot inputs explicit at orchestration boundaries.
6. Add a product-chain E2E with a non-identity frame that is translated,
   rotated and moving; include angular-frame motion if supported by the API.
7. Replace brittle exact-source-string checks with semantic contract checks that
   reject the real bypass and accept harmless formatting.
8. Re-run all architecture/purity checks and the navigation runtime gate.

## Non-goals for M1

- do not tune follower gains or tracking-loss timeouts;
- do not extend the scalar-progress-first Ruckig pipeline;
- do not enable dynamic avoidance;
- do not redesign topology yet;
- do not accept an actuator-infeasible program;
- do not add another coordinate-conversion helper.

## Acceptance gate

- no active runtime calculation copies a vector between semantic frames;
- all frame conversion flows through the canonical API;
- identity and non-identity frame E2Es pass;
- purity tests detect semantic dependency violations, not source formatting;
- the target-machine build and `navigation_runtime_pipeline` result are recorded;
- all mandatory state documents are updated.

After M1 is green, proceed to M2 from the blueprint. Do not skip directly to
octree work: the current monolith must first be split at its real ownership
boundaries, then acceptance must be made truthful.
