# CURRENT TASK — dynamic local avoidance over immutable retained route

**Date:** 2026-09-21  
**Status:** STATIC RETAINED-ROUTE EXECUTION TARGET-PROVEN / DYNAMIC OVERLAY NEXT

## Accepted current evidence

The diagnostic stand now proves the static two-stage chain:

```text
Stage 1
scene + static obstacles
 -> NominalRoutePlanner
 -> immutable retained route

Stage 2
same retained route
 -> Ruckig
 -> route-leg AcceptedManeuverPrograms
 -> TrajectoryFollower
 -> PilotSkillExecutor
 -> SharedShipPhysics / DynamicMotionSystem
 -> finish
```

User target diagnostics (exact checkout SHA was not included in the supplied output):
- Stage-1 route: 4 points, 323.75 m, static detour YES;
- Expert / Standard / Newtonian: 3 phases, 2 handoffs, final P error 0.15 m,
  final speed 0.10 m/s, max follower error 1.08 m, no static contact;
- Expert / Extreme / Newtonian: final P error 0.10 m, final speed 0.48 m/s,
  max follower error 0.95 m, no static contact;
- Expert / Extreme / Assisted: same observed quality, no static contact.

The old 102 consecutive-Ruckig-sample microprogram interpretation is closed. The
four-point route now produces three physical route-leg phases.

## Current task

Enable the scenario's moving/sudden-obstacle inputs as a **local dynamic overlay**
without changing the retained Stage-1 route.

Required behavior:

```text
immutable retained route
  -> execute/follow
  -> bounded dynamic monitor sees predicted conflict
       -> safe executable local bypass available:
            accept short off-route bypass and continue
       -> no safe executable bypass:
            active braking command
            navigation remains alive
  -> fresh monitor updates
  -> obstacle clears / bypass completes
  -> progressive reacquisition of retained route
  -> continue to finish
```

Non-negotiable rules:
- no dynamic-triggered NominalRoutePlanner/global route rebuild;
- no arbitrary 30 m or same-horizon mandatory merge;
- no navigation shutdown on ConflictHold/no-space;
- surprise obstacle is absent before `activation_time_s`;
- local avoidance must respect actual vehicle speed/acceleration/control-law capability;
- the retained white route must remain bitwise/geometrically unchanged throughout
  Stage 2/3 execution.

## First dynamic acceptance scenario

Use the existing `scenario.json` sudden obstacle:
- checkbox enabled;
- actor appears only after activation;
- first acceptance mode: Expert / Standard / Newtonian.

Required evidence:
- retained route unchanged;
- dynamic obstacle publication/activation observed;
- at least one local monitor decision after activation;
- either a safe physically executed bypass or explicit active braking when bypass is
  not physically available;
- no dynamic contact;
- navigation continues after the event;
- finish eventually reached.

Only after this is correct expand to:
- Extreme;
- Assisted;
- Average/Loser pilot profiles.

## Static proof boundary

Current `COARSE STATIC CONTACT` is still route-envelope-level evidence, not final
oriented swept-hull B6 tunnel proof. Do not call the whole navigation system accepted.

## Mandatory state protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
