# CONTINUE PROMPT — verify corrected navigation-test cleanup

Continue in GitHub repository `forzub/Elite`, branch `main`.

Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, and the latest section of
`src/game/navigation/STAGE12_END_TO_END.md`.

The navigation test layer has been cleaned:
- stale local-flight angular raw-symbol checks now validate canonical
  ShipDynamics accessors;
- live replication guidance uses DockingAdvisoryPlanner and wire schema 9;
- foundation/geometric checks target current advisory ownership;
- dead `tests/navigation_guidance` all-in-one suite is removed;
- useful geometric coverage moved to
  `tests/navigation_runtime/GeometricPathPlannerTests.cpp`.

Important: commit `91f24d13726cd2192d7b240277aa5a659622caf3`
contains a false-positive bug only in the newly added meta-check regex: it could
parse `.cpp` as `.c`. Use the corrected HEAD after that commit.

The corrected `check_test_suite_source_integrity.py` accepts only complete
C/C++ source/header extensions and repository-owned CMake source forms
(`${ELITE_SOURCE_ROOT}/...` or test-local bare filenames).

Next action: rerun `tests/architecture_contracts/run_mingw64.sh`. If green,
run the focused geometric and docking-advisory native targets, then proceed to
the in-game SHOW ROUTE commissioning.

Keep state/task/project/Stage-12/prompt synchronized after every
state-affecting result. Never mark target/game acceptance without target
evidence.
