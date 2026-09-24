# CONTINUE PROMPT — playable manual docking SHOW ROUTE

Continue in GitHub repository `forzub/Elite`, branch `main`.

The immediate target is the in-game manual docking SHOW ROUTE flow. Do not
revive the removed rolling `GuidanceTunnelBuilder`, `DockingPathPlanner` or
client-owned moving-start planning.

## Read first

1. `AGENTS.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. `src/game/navigation/NAVIGATION_GUIDANCE_CONTRACT.md`;
6. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
7. final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`.

Inspect the active code path:

- `src/game/server/ControlRegistry.h`;
- `src/game/server/GameServer.{h,cpp}`;
- `src/game/simulation/GameSimulation.{h,cpp}`;
- `src/game/ship/core/ShipControlState.h`;
- `src/game/navigation/NavigationRuntimeControlBridge.*`;
- `src/game/navigation/DockingRouteRequest.h`;
- `src/game/navigation/DockingAdvisoryPlanner.{h,cpp}`;
- `src/game/navigation/DockingAdvisoryCorridor.h`;
- `src/game/SpaceState.{h,cpp}`;
- `src/game/system_map/SystemMapRenderer.cpp`;
- `src/game/presentation/GuidanceHudPresentation.h`;
- `src/render/cockpit/GuidanceCorridorRenderer.{h,cpp}`;
- `src/assets/localization/ui/cockpit/flight.json`;
- docking/localization architecture checks and focused native tests.

## Exact required lifecycle

```text
SHOW ROUTE
 -> temporary authoritative Autopilot takeover
 -> physically stop relative to Hub + settle angular rate
 -> capture fresh authoritative rigid-body state
 -> calculate static docking advisory
 -> publish Hub Map line + fixed HUD tunnel
 -> return authority to Human
 -> player flies manually
```

Rules:

- no teleport/clamp and no change of installed control law;
- material pilot input during preparation cancels it and returns Human;
- nominal HUD gate spacing = 500 m; retain terminal gate;
- gates are fixed spatial cross-sections, not rolling/time-following frames;
- recommended speed is shown at the projected upper-left of every gate;
- cockpit top blinks localized MANUAL DOCKING MODE while guidance is active;
- all text uses the existing unified `LocalizationService`;
- closing the selected dock card cancels at any stage;
- after first valid tunnel entry, leaving the permitted corridor cross-section
  cancels guidance;
- cancellation during preparation must release Autopilot authority;
- automatic DOCKING remains disabled.

## Known current gaps

- `ControllerKind::Autopilot` exists but temporary player takeover/release is
  not wired;
- SHOW ROUTE is still a client workspace request;
- gate spacing is 350 m;
- speed-label placement is not guaranteed upper-left and `m/s` is formatted
  inside the renderer;
- no localized manual docking status exists;
- gate opacity is distance-faded;
- exit tracking still uses the provisional 60 m / 700 m envelope;
- old architecture checks still target deleted rolling-tunnel symbols.

Implement the vertical slice without hiding these gaps. Add/replace focused
tests. Run all locally available gates, but never claim Windows/gameplay
acceptance without target evidence.

## Mandatory state protocol

After every state-affecting event synchronize `CURRENT_STATE.md`,
`CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 journal, affected
contracts and recreate this prompt. Commit/push one coherent iteration to
`main`. Never record an unrun gate as passed.
