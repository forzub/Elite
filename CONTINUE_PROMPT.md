# CONTINUE PROMPT — validate corrected Elite navigation M1

Continue in GitHub repository `forzub/Elite`, branch `main`.

The normative architecture is
`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`. The current
implementation is still an M1 candidate; M2 has not started.

## Read first

Read completely, in order:

1. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. the final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`;
6. `src/game/navigation/NAVIGATION_API_CONTRACT.md`.

Then inspect the M1 code and tests in:

- `src/game/navigation/NavigationFrameBoundary.h`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tests/navigation_runtime/NavigationRuntimePlannerTests.cpp`;
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`;
- `tests/architecture_contracts/check_navigation_api_purity.py`;
- the other modified navigation architecture checks.

## Exact current state

M1 routes intent and initial/observed rigid-body state through
`NavigationFrameBoundary`, receives an explicit moving-frame snapshot and epoch,
advances the frame during execution, and includes a non-identity product-chain
E2E.

The first MinGW64 attempt was compile-blocked by a stale
`lowStandard.pilot = PilotLevel::Expert` line. The fixture already supplied
`pilotExecutionProfile`; the dead assignment has now been removed and the
purity checker forbids its return.

Do not treat the CTest output appended after that compiler failure as current.
It came from an older executable: it used obsolete reference-reacquisition
assertion text and lacked the new non-identity marker. Its infeasible actuator
segments are a known M3/M4 physical-authoring problem, not an M1 verdict.

## Immediate action

Obtain the complete output of the single `&&`-chained MinGW64 command in
`CURRENT_TASK.md`, including `git rev-parse HEAD`.

The independent M1 marker is:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

Never run or interpret CTest after a failed build. If compilation fails, repair
the exact defect without weakening the non-identity fixture. If the marker
passes and a later physical E2E fails, record M1 acceptance separately from the
existing physical-planner failure.

## Governing invariant

```text
accepted maneuver == capability-checked maneuver
                  == continuously collision-proved maneuver
                  == actuator program executed by physics
```

Do not restore identity-frame copies or broad settings, add ambient lookups,
accept infeasible propulsion, tune follower gains/timeouts, or start octree
integration to evade a failure.

## Only after M1 acceptance

Activate blueprint stage M2: split `NavigationScenarioRuntime.cpp` into a thin
composition root plus production-owned parsing, route composition, maneuver
planning/proof, execution-harness and diagnostics modules. M2 must preserve
behavior; it is not the physical-planner redesign.

## Mandatory iteration protocol

Every state-affecting iteration must update the blueprint, `CURRENT_STATE.md`,
`CURRENT_TASK.md`, `PROJECT_STATE.md`, the Stage-12 journal, affected contract
documents, and recreate this prompt for the next exact action. Review the full
diff, run every available gate, commit the coherent iteration and push it to
`main`. Never claim a check that did not run.
