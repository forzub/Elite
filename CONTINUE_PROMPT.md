# CONTINUE PROMPT — verify cleaned current navigation tests

Continue in GitHub repository `forzub/Elite`, branch `main`.

Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, and the latest section of
`src/game/navigation/STAGE12_END_TO_END.md`.

Latest target failure was:

```text
Local-flight-control architecture check failed:
shared angular safety envelope lost: angularAccelerationEnvelope
```

Root cause was stale test architecture, not lost physics. The current angular
authority chain is:

```text
ShipParams
 -> ShipDynamics.h
    angularAccelerationLimitRadPerSec2()
    angularLoadRateLimitRadPerSec()
    pitch/yaw/rollRateLimitRadPerSec()
 -> ShipController
```

A broader audit also retired the dead `tests/navigation_guidance` suite, which
still compiled removed `DockingPathPlanner.cpp` and `GuidanceTunnel.cpp`.
Do not restore it.

Current docking/manual guidance ownership:

```text
SHOW ROUTE
 -> authoritative temporary Autopilot takeover
 -> physical Hub-relative stop/angular settle
 -> fresh authoritative Hub planning snapshot
 -> DockingAdvisoryPlanner
 -> GeometricPathPlanner
 -> fixed 500 m spatial gates + Hub Map route
 -> acknowledged Human hand-back
```

Current focused tests include:
- `tests/navigation_runtime/GeometricPathPlannerTests.cpp`;
- `tests/navigation_runtime/DockingAdvisoryPlannerTests.cpp`;
- `tests/architecture_contracts/check_manual_docking_advisory.py`;
- `tests/architecture_contracts/check_test_suite_source_integrity.py`;
- wire protocol/data-plane authority contracts.

Next: rerun the architecture suite. If green, run current focused runtime
targets and then the in-game docking commissioning sequence. Never fix a stale
grep check by reintroducing duplicate/retired runtime code.

After every state-affecting result synchronize CURRENT_STATE.md,
CURRENT_TASK.md, PROJECT_STATE.md, STAGE12_END_TO_END.md and recreate this
prompt. Never mark target/game acceptance without target evidence.
