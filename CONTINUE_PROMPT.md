# CONTINUE PROMPT — verify corrected acknowledged manual docking

Continue in GitHub repository `forzub/Elite`, branch `main`.

Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, the latest dated section of
`src/game/navigation/STAGE12_END_TO_END.md`, and
`src/game/navigation/NAVIGATION_GUIDANCE_CONTRACT.md`.

Important: commit
`61b5424608533c806672e21e77339be612dc0087` is a rejected intermediate
candidate. Static review found duplicate `controlledEntityId` declarations in
`copySnapshotForSession()` and missing authority publication in the other two
copy paths. Do not use that SHA as evidence.

The corrected design is:

```text
ControlRegistry
 -> GameServer::controlledEntityAutopilotActiveForSession()
 -> copySnapshotForSession()
 -> copyHydratedSnapshotForSession()
 -> copySparseSnapshotForSession()
 -> ClientSessionSnapshot.controlledEntityAutopilotActive
 -> snapshot wire schema 9
```

Manual SHOW ROUTE lifecycle remains:

```text
request
 -> Human -> Autopilot
 -> authoritative Autopilot=true
 -> physical Hub-relative stop + angular settle
 -> fresh authoritative start snapshot
 -> async advisory plan
 -> 500 m fixed gates + map route
 -> Complete
 -> handoff_wait
 -> authoritative Autopilot=false
 -> Human prediction resumes
```

Run focused gates before gameplay:
- architecture Python suite, especially
  `check_manual_docking_advisory.py`;
- `control_registry_contracts`;
- `wire_protocol_contracts`;
- `wire_data_plane_contracts`;
- `docking_advisory`.

Expected successful game log:
`phase=stabilizing -> [DockPrep] begin -> phase=planning ->
phase=handoff_wait -> [DockPrep] published ... human_restored=1 ->
phase=manual human_control=1`.

Keep MD state/task/project/Stage-12/prompt synchronized after every
state-affecting result. Never claim Windows/game acceptance without target
evidence.
