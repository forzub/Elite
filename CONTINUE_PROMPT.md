# CONTINUE PROMPT — validate Elite navigation M1 after compile cleanup

Continue in GitHub repository `forzub/Elite`, branch `main`.

The normative target is
`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`. M1 is still
open; M2 has not started.

## Mandatory reading

Read completely, in order:

1. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. the final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`;
6. `src/game/navigation/NAVIGATION_API_CONTRACT.md`.

Inspect the active M1 path and the latest correction in:

- `src/game/navigation/NavigationFrameBoundary.h`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `src/world/navigation/TrajectoryGenerator.{h,cpp}`;
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`;
- `tests/architecture_contracts/check_navigation_api_purity.py`.

Follow other files only where these sources or the active build target reference
them.

## Exact state

M1 canonicalizes NavLocal/System conversion, makes the moving-frame snapshot and
epoch explicit, advances that frame during execution, and adds a non-identity
product-chain E2E.

Target attempt 1 failed on a stale `ScenarioRunSettings::pilot` fixture write.
That was removed and statically forbidden.

Target attempt 2 confirmed the E2E translation unit now compiles, while
architecture checks, Stage-1 nominal routing and the tracking-controller test
pass. The build then found older dead code in `TrajectoryGenerator.cpp`:

- a copy of removed `ruckigSolveMilliseconds` diagnostics;
- an orphaned `countBlendedWaypoints()` left after perf-log removal;
- its obsolete call signature and unused local result;
- an unused `arc` input on `globalGuideSpeedLimit()`.

These have been removed. No trajectory policy or output semantics changed. The
purity checker prevents both retired symbols from returning.

## Immediate action

Obtain the complete output of the single chained command in `CURRENT_TASK.md`,
including the checkout hash. Do not interpret CTest unless the corrected target
compiled and linked.

Required M1 marker:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

If it passes, accept M1 independently of any later known physical-authoring
failure. If it fails or compilation fails earlier, repair the exact defect
without weakening the fixture or changing physical-control policy.

## Governing invariant

```text
accepted maneuver == capability-checked maneuver
                  == continuously collision-proved maneuver
                  == actuator program executed by physics
```

No ambient time/filesystem lookup belongs in a pure calculation kernel. Do not
restore identity-frame copies, broad context inputs, infeasible propulsion,
follower tuning or octree work to bypass this gate.

## After M1 acceptance only

Activate M2 from the blueprint: split `NavigationScenarioRuntime.cpp` into a
thin tool composition root and production-owned parsing, route composition,
maneuver planning/proof, execution harness and diagnostics modules. Preserve
behavior during that split.

## Iteration protocol

Every state-changing iteration must update the blueprint, `CURRENT_STATE.md`,
`CURRENT_TASK.md`, `PROJECT_STATE.md`, the Stage-12 journal and any affected
contract document; recreate this prompt; run all available gates; review the
complete diff; commit and push the coherent change to `main`. Never claim a
check that did not run.
