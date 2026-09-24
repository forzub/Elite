# CONTINUE PROMPT — verify playable manual docking SHOW ROUTE

Continue in GitHub repository `forzub/Elite`, branch `main`.

Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, the final dated sections of
`src/game/navigation/STAGE12_END_TO_END.md`, and
`src/game/navigation/NAVIGATION_GUIDANCE_CONTRACT.md`.

The current code implements:

```text
SHOW ROUTE
 -> ClientShipCommand BeginDockingGuidancePreparation
 -> ControlRegistry Human -> Autopilot (identity retained)
 -> server repeatedly executes physical BrakeToStop
 -> client prediction suppressed, numbered human samples still sent
 -> authoritative vrel + pitch/yaw/roll rates settle
 -> buildAuthoritativeHubSnapshot at the accepted epoch
 -> async DockingAdvisoryPlanner
 -> publish Hub Map line + fixed spatial HUD gates
 -> ClientShipCommand CompleteDockingGuidancePreparation
 -> server restores Human
```

Required UI/product behavior:
- 500 m nominal gate spacing; final remainder retained;
- recommended speed at projected upper-left of every spatial gate;
- blinking localized `cockpit.docking.manual_mode`;
- card-close cancels the task;
- after first valid entry, leaving the corridor cancels the task;
- automatic DOCKING remains disabled.

Inspect first:
- `src/game/server/ControlRegistry.h`;
- `src/game/server/GameServer.{h,cpp}`;
- `src/game/client/GameClient.{h,cpp}`;
- `src/game/SpaceState.{h,cpp}`;
- `src/game/navigation/DockingAdvisoryPlanner.{h,cpp}`;
- `src/render/cockpit/GuidanceCorridorRenderer.cpp`;
- `src/assets/localization/ui/cockpit/flight.json`;
- `tests/navigation_runtime/DockingAdvisoryPlannerTests.cpp`;
- `tests/architecture_contracts/ControlRegistryContractTests.cpp`;
- `tests/architecture_contracts/check_manual_docking_advisory.py`.

Next action is verification. Run all locally available focused gates, but do not
claim Windows/game acceptance without target-machine evidence. For the game
test, start moving and rotating before pressing SHOW ROUTE and capture
`[DockPrep]` / `[DockAdvisory]` logs if the sequence fails.

Mandatory state protocol: after every state-affecting result synchronize
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active
Stage-12 journal, affected contracts, and recreate this prompt. Commit/push one
coherent iteration to `main`. Never record an unrun gate as passed.
