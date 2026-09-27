# CONTINUE PROMPT — Elite Navigation v2 / docking live validation

Work in public repository `forzub/Elite`, branch `main`.

At the start of EVERY iteration read:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After every state-affecting iteration:
1. update relevant state/task/project/end-to-end MD files;
2. update CONTROL_LAW_MANEUVER_MODEL if flight doctrine changes;
3. REGENERATE THIS FILE FROM SCRATCH;
4. commit directly to public GitHub `main`.

Do not send patch files. Fresh Windows/MSYS2 verify/build/live evidence is
required before calling newest behavior accepted.

## Flight doctrine

Assisted is a game-flight model, not a raw RCS physics model:
- hull nose defines intended travel direction;
- course/VREL follows nose with finite lag, target <= about 2–3 s;
- forward/reverse main controls longitudinal speed;
- automatic velocity-to-nose stabilization is ordinary Assisted course
  authority;
- physical manoeuvre/RCS (Cobra currently 2 m/s²) is precision authority, not
  ordinary route curvature authority.

Automatic Assisted must execute the SAME flight law as manual Assisted.

Newtonian/heavy is separate:
- faster, less maneuverable;
- velocity independent from hull attitude;
- doctrine = coast -> rotate -> main burn -> coast -> flip/rotate -> brake;
- docking uses mostly piecewise-straight geometry;
- precision RCS is not fake lateral main thrust.

## Accepted evidence before newest live fixes

Fresh Windows native evidence:
```text
navigation_runtime_control .......... PASS
maneuver_tracking_controller ........ PASS
docking_advisory .................... PASS
accepted_maneuver_program_builder ... PASS
trajectory_generator_angular ........ PASS
manual docking static contract ...... PASS
```

Cockpit center boresight is accepted by the user.

Terminal angular pose/omega remain physical boundary conditions. No final-sample
omega snap is allowed.

## Latest live finding

User supplied a cockpit screenshot and runtime log.

Observed:
- visible tunnel looked as though it belonged to another trajectory;
- first piece was forward, then geometry transitioned awkwardly;
- desired form:
  `straight current-hull segment -> smooth arc -> route to dock`;
- START DOCKING without prior CALCULATE TRAJECTORY made the button green and
  displayed AUTOMATIC DOCKING MODE;
- ship did not move;
- log had `dock_request=1` but no `[DockAuto]` lifecycle lines.

## Newest route/Automatic fixes already on main

### Law-aware visible guidance

SpaceState visible docking guidance now calls the explicit control-law vehicle
profile overload:
```cpp
makeNavigationVehicleProfile(
    effectivePhysics,
    envelope,
    guidanceControlLaw
);
```

Assisted geometry therefore uses Assisted course authority rather than the old
2 m/s² precision-RCS lateral limit.

`request.roundTurns = guidanceAssisted`.

### Straight prefix -> tangent first arc

DockingAdvisoryPlanner no longer protects the whole 500 m nose-first lead from
filleting.

Current behavior:
- exact hull-forward launch ray is retained;
- `initialForwardAcceptedLeadMeters` stores actual available lead;
- `initialForwardProtectedStraightMeters` preserves a visible straight prefix;
- remaining lead is available to the first circular fillet via
  `maximumLaunchCut`;
- normal visible guidance cadence is now 150 m instead of 500 m;
- launch region gets additional dense sampling.

Native regression requires:
- multiple straight-prefix frames;
- a real later heading transition;
- no >30 degree first-turn discrete kink.

### START DOCKING is self-contained

Manual CALCULATE TRAJECTORY is not a prerequisite.

Cold Automatic:
```text
START DOCKING
 -> phase=route-preflight
 -> BeginDockingGuidancePreparation
 -> physical stop / settle
 -> authoritative Hub snapshot
 -> law-aware advisory route
 -> visible tunnel publication
 -> temporary authority hand-back
 -> phase=requested
 -> BeginAutomaticDocking
 -> server Stabilizing / async planning / execution
```

A matching already-visible corridor may still be reused.

Server Automatic also sets:
```cpp
request.hasInitialForward = true;
request.initialForward = currentForwardMap;
request.initialForwardLeadMeters =
    max(500.0, hull.lengthMeters*10.0);
request.gateSpacingMeters = 150.0;
request.terminalGateSpacingMeters = 150.0;
```

### Automatic observability

Client should emit:
```text
[DockAuto] request=N phase=route-preflight
[DockAuto] request=N phase=requested ...
```

Server should emit either:
```text
[DockAuto] begin ... phase=stabilizing
```
or explicit rejection:
- `invalid-command`
- `ship-not-found`
- `ship-not-matched-to-target-hub`
- `autopilot-authority-denied controller=...`

## Latest compile regression and correction

Fresh MinGW build failed at:
```text
GameServer.cpp:994:
error: 'command' was not declared in this scope
```

Cause:
the new Automatic authority-denied diagnostic was accidentally inserted into
`beginDockingGuidancePreparation()`, which has only `requestSerial`.

Corrected on main:
- manual preparation uses:
  `[DockPrep] ... request=<requestSerial>`;
- Automatic uses:
  `[DockAuto] ... request=<command.requestSerial>`;
- Automatic authority-denied diagnostic is now in
  `beginAutomaticDocking()`;
- successful Automatic begin output is flushed.

Post-edit source verification:
- zero `command.requestSerial` references inside
  `beginDockingGuidancePreparation()`;
- expected Automatic authority-denied diagnostic is present inside
  `beginAutomaticDocking()`.

This was compile-only; no route/control/physics semantics changed.

## Current HEAD progression

The compile fix commit itself:
```text
abccc4956d8122ed1d75ec209cfa6d62e3ee5fc5
```

Documentation commits follow it. Pull current `main` and use whatever HEAD is
reported by `git log -1 --oneline`.

## Immediate target gate

Run:
```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash build_mingw64.sh
```

If build fails, use the first compiler error only; do not change navigation
semantics by guess.

If build succeeds:
```bash
build/EliteGame.exe
```

## Exact live test after successful build

Do NOT press CALCULATE TRAJECTORY first.

1. Start in Assisted.
2. Select the docking port.
3. Press START DOCKING directly.
4. Capture every `[DockAuto]` and `[DockAdvisory]` line.
5. Expected beginning:
```text
[DockAuto] request=N phase=route-preflight
[DockAdvisory] request=N phase=stabilizing ...
[DockAdvisory] request=N phase=settled ...
[DockAdvisory] request=N phase=planning ...
[DockAdvisory] request=N route=...
[DockAdvisory] request=N phase=handoff_wait ...
[DockAdvisory] request=N ... human_control=1
[DockAuto] request=N phase=requested ...
[DockAuto] begin ... phase=stabilizing
```
6. Then expect async planning, optional physical alignment/replan, then
   `phase=executing`.
7. Tunnel must visually show:
   several frames ahead of the boresight -> smooth bend -> route toward dock.
8. Ship must actually move once execution begins.

## Non-negotiable invariants

- Planner owns route/reference/control-law-compatible maneuver program.
- Follower closes bounded tracking error only.
- Manual and Automatic Assisted share one game-flight law.
- Visible Assisted guidance uses the same law-aware capability model.
- Physical RCS is not ordinary Assisted course authority.
- Newtonian ordinary transit cannot use precision RCS as fake main thrust.
- No direct authoritative position/velocity/orientation rewrites.
- No planner-only target collision bypass.
- No stale program execution after physical alignment.
- No hidden corridor during Automatic.
- No synchronous Automatic plan-retry storm.
- START DOCKING must not depend on prior manual route calculation.
- Current docking scope ends at collision-free pre-capture; physical latch is
  later work.

## Verification status

Newest compile correction is committed.
Fresh target MinGW build after the correction is PENDING.
Cold START DOCKING live evidence is PENDING.
