# CONTINUE PROMPT — Elite Navigation dynamic overlay over retained route

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read first:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`
- `src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md`
- `src/game/navigation/TRAJECTORY_EXECUTION_REPLAN_MODEL.md`
- `src/game/navigation/LocalAvoidancePlanner.h/.cpp`
- `src/game/navigation/NavigationRuntimePlanner.h/.cpp`
- `src/game/navigation/NavigationExecutionReplanPolicy.h/.cpp`
- `tools/navigation_runtime/NavigationScenarioRuntime.h/.cpp`
- `tools/navigation_runtime/scenario.json`
- `tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp`
- `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`

After every state-affecting event update the mandatory Markdown state files and
**recreate this CONTINUE_PROMPT.md from scratch again**.

## Current verified behavior

Static two-stage diagnostic execution now works on the user's target machine.

Retained Stage-1 route:
- start (0,0,0), finish (300,0,0);
- one static wall;
- 4 route points;
- 323.75 m;
- static detour YES.

Observed Stage-2 target results:
- Expert/Standard/Newtonian: 3 physical phases, 2 handoffs, final P error 0.15 m,
  final speed 0.10 m/s, max follower error 1.08 m, no contact;
- Expert/Extreme/Newtonian: final P error 0.10 m, final speed 0.48 m/s,
  max follower error 0.95 m, no contact;
- Expert/Extreme/Assisted: same observed terminal/tracking quality, no contact.

The supplied target output did not include `git rev-parse HEAD`; do not invent a
tested SHA.

The former 102-microprogram integration bug is closed. Ruckig trajectory samples are
now downsampled into meaningful route-leg AcceptedManeuverPrograms and handed off by
ManeuverPhaseGate.

## Active task

Enable moving/sudden obstacles as a bounded **local dynamic overlay** over the same
immutable retained route.

Canonical ownership:

```text
NominalRoutePlanner once
  -> retained route
  -> Ruckig / accepted execution
  -> per-tick/periodic local dynamic monitor
       conflict? -> local bypass or braking
       clear?    -> continue / progressively reacquire retained route
```

Hard rules:
- dynamic actors MUST NOT call/rebuild NominalRoutePlanner;
- dynamic-world revision MUST NOT invalidate the retained static route;
- surprise obstacle is absent before its authored activation time;
- if there is enough physical authority/space, evade;
- if there is not, actively brake and keep navigation alive;
- after bypass, do not snap back or require an immediate same-horizon return leg;
- no fixed 30 m merge rule;
- local segments must be physically executable for current vehicle/control law;
- retained route geometry must remain unchanged for before/after comparison.

Start with existing `scenario.json` sudden obstacle and
Expert/Standard/Newtonian. After that passes, extend to Extreme/Assisted and pilot-skill
variation.

## Evidence policy

Do not call the complete navigation system accepted from this static/dynamic viewer
alone. Current static-contact evidence is still coarse; final B6 oriented swept-hull
tunnel proof remains a separate requirement.

## Build/test handoff

Always provide exact target-machine commands. Any command sequence that builds an
executable must be followed by a separate exact launch command.

If target output changes project state, update all mandatory Markdown files and recreate
this prompt from scratch again.
