# CONTINUE PROMPT — Elite Navigation v2 / docking request handoff diagnostic

Work in public repository `forzub/Elite`, branch `main`.

At the start of every iteration read:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After every state-affecting iteration update the relevant MD files and regenerate this file from scratch. Commit directly to public main. Do not send patch files.

Hard user contracts:
- manual visible docking frames: 500 m ordinary / 250 m final;
- do not change that cadence without explicit instruction;
- Automatic docking is two-stage:
  ApproachHold -> real stop -> FinalIngress;
- exact dock orientation/omega belongs only to FinalIngress;
- full 3-D alignment includes yaw, pitch and roll via forward+up basis;
- player Autopilot uses the same Ship entity with controller ownership switched Human -> Autopilot.

Current latest live symptom:
```text
dock_request=1
```
but there are NO `[DockAuto]` or `[DockAdvisory]` lifecycle lines and the ship does not move.

Interpretation:
- a pending docking request exists in the client workspace;
- there is currently no evidence that route-preflight, BeginAutomaticDocking, or server Automatic began;
- therefore this observed failure is earlier than Planner/Follower/physics.

Newest diagnostic code on main:
SpaceState now emits once per new pending serial:
```text
[DockRequest] serial=N
 mode=automatic|guidance
 last_path=...
 prep_serial=...
 auto_serial=...
 visible_route=0|1
 needs_preflight=0|1
```

This iteration intentionally changes no flight, trajectory, authority, physics, or docking-state behavior.

Immediate target procedure:
```bash
cd /d/__elite/work
git pull --ff-only origin main
bash build_mingw64.sh
build/EliteGame.exe
```

Press START DOCKING once and capture:
1. the first `[DockRequest]` line;
2. every following `[DockAuto]` and `[DockAdvisory]` line.

Use the first missing transition to locate the defect:
- no `[DockRequest]`: UI request is created after/away from SpaceState processing or workspace handoff is wrong;
- `mode=guidance` after START DOCKING: UI action mode bug;
- `mode=automatic needs_preflight=1` but no route-preflight: SpaceState orchestration branch bug;
- route-preflight but no DockAdvisory stabilizing: preparation command dispatch bug;
- DockAdvisory completes but no phase=requested: preparation hand-back / automatic-start transition bug;
- phase=requested but no server begin/rejected: client->server command transport/dispatch bug;
- server begin then plan-failed: return to server Planner diagnostics.

Existing major architecture:
- Assisted manual/Automatic share the same game-flight law;
- physical RCS is precision authority, not ordinary Assisted course authority;
- Newtonian is a separate heavy/inertial family;
- Automatic server runtime stages are ApproachHold and FinalIngress;
- long approach stops at hold point;
- FinalIngress recomputes port pose and owns exact terminal angular state;
- launch geometry is tangent-filleted internally while user-visible corridor cadence remains 500/250;
- dock ports have semantic click priority over parent infrastructure.

Verification status:
Newest request-handoff diagnostics are committed to public main.
Fresh target build/live `[DockRequest]` evidence is pending.
