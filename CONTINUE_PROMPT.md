# CONTINUE PROMPT — finish Elite navigation M1 timeline/frame validation

Continue in GitHub repository `forzub/Elite`, branch `main`.

The normative target architecture is
`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`. M1 remains
open until the next target evidence is classified; M2 has not started.

## Read first

Read completely:

1. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`;
6. `src/game/navigation/NAVIGATION_API_CONTRACT.md`.

Inspect the active change in:

- `src/game/navigation/ManeuverProgramTimeline.h`;
- `src/game/navigation/ManeuverProgramSampler.cpp`;
- `src/game/navigation/TrajectoryFollower.cpp`;
- `src/game/navigation/ManeuverPhaseGate.cpp`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tests/navigation_runtime/ManeuverProgramSamplerTests.cpp`;
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`;
- `tests/architecture_contracts/check_navigation_api_purity.py`.

Follow other files only when referenced by this active path.

## Exact state

The third MinGW64 run built and linked successfully. Identity and moving-frame
executions produced identical printed NavLocal telemetry but both stopped at
the first page boundary with `PROGRAM_PAGE_BEFORE_START` at 50.30 s.

Root cause: runtime selected by a reconstructed previous-page end plus epsilon;
Sampler selected by the next page's independently reconstructed start. Four
components owned page-time arithmetic.

The candidate adds one pure `ManeuverProgramTimeline` owner. Runtime selects by
the next page's own start. Sampler, Follower and phase gate consume the same
window/elapsed-time API. Follower completion now subtracts the page sequence
offset. Unit tests pin the floating-point boundary and later-page completion.

The frame E2E now compares every identity/non-identity NavLocal trace frame and
requires the same terminal outcome. It intentionally does not require the known
78-infeasible-segment physical plan to succeed: that belongs to M3/M4. This
separates the M1 coordinate invariant from later physical truthfulness.

## Immediate action

Run the complete chained target command in `CURRENT_TASK.md`, including the
focused timeline test and pipeline test. Preserve the checkout hash.

Required independent marker:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

If it passes, record M1 acceptance separately from any following physical
failure and activate M2. If it does not, repair the exact timeline/frame defect
without relaxing trace tolerances.

## Governing invariant

```text
accepted maneuver == capability-checked maneuver
                  == continuously collision-proved maneuver
                  == actuator program executed by physics
```

Do not restore duplicate clock arithmetic, identity-frame copies, ambient
lookups, infeasible-plan acceptance, follower tuning or octree work to bypass a
gate.

## After M1 acceptance

Activate only M2: split `NavigationScenarioRuntime.cpp` into a thin composition
root and production-owned parsing, route composition, maneuver planning/proof,
execution harness and diagnostics modules. Preserve behavior during the split.

## Mandatory iteration protocol

Update the blueprint, state/task/project documents, Stage-12 journal and affected
API contracts after every state change; recreate this prompt; run all available
gates; inspect the full diff; commit and push one coherent iteration to `main`.
Never claim a check that did not run.
