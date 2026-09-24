# CONTINUE PROMPT — verify acknowledged manual docking SHOW ROUTE

Continue in GitHub repository `forzub/Elite`, branch `main`.

Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, the latest dated sections of
`src/game/navigation/STAGE12_END_TO_END.md`, and
`src/game/navigation/NAVIGATION_GUIDANCE_CONTRACT.md`.

Current implementation:

```text
SHOW ROUTE
 -> begin preparation command
 -> server ControlRegistry Human -> Autopilot
 -> session snapshot: controlledEntityAutopilotActive=true
 -> physical Hub-relative BrakeToStop + angular damping
 -> fresh authoritative stopped planning snapshot
 -> async DockingAdvisoryPlanner
 -> publish Hub Map route + fixed 500 m spatial gates
 -> send Complete
 -> keep local prediction suppressed
 -> server restores Human
 -> newer session snapshot: controlledEntityAutopilotActive=false
 -> resume Human prediction
```

Expected successful log order:
`phase=stabilizing -> [DockPrep] begin -> phase=planning ->
phase=handoff_wait -> [DockPrep] published ... human_restored=1 ->
phase=manual human_control=1`.

Relevant gates:
- architecture Python suite, especially
  `check_manual_docking_advisory.py`;
- `control_registry_contracts`;
- `wire_protocol_contracts`;
- `wire_data_plane_contracts`;
- `docking_advisory`.

Snapshot data-plane schema is 9. Wire framing protocol is 10.

Product requirements:
- physical stop relative to Hub; no direct velocity edit;
- angular rates physically damped;
- planning start from fresh authoritative stopped state;
- fixed gates nominally 500 m, terminal remainder allowed;
- recommended speed at projected upper-left of every gate;
- blinking localized MANUAL DOCKING MODE;
- Human controls effective only after authoritative hand-back;
- close-card cancellation and post-entry corridor-departure cancellation;
- automatic DOCKING remains disabled.

After every state-affecting result synchronize state/task/project docs, Stage-12,
affected contracts, and recreate this prompt. Never mark Windows/game acceptance
without target-machine evidence.
