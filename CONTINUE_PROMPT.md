# CONTINUE PROMPT — Elite Navigation v2 / two-stage Automatic docking

Work in public repository `forzub/Elite`, branch `main`.

At the start of EVERY iteration read the newest relevant sections of:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- `src/game/navigation/STAGE12_END_TO_END.md`
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`
- this file

After every state-affecting iteration:
1. update CURRENT_STATE / CURRENT_TASK / PROJECT_STATE / STAGE12_END_TO_END as appropriate;
2. update CONTROL_LAW_MANEUVER_MODEL when doctrine changes;
3. REGENERATE THIS FILE FROM SCRATCH;
4. commit directly to public GitHub `main`.

Do not send patch files. Fresh Windows/MSYS2 verify/build/live evidence is required before declaring newest behavior accepted.

## Hard user contracts

Manual visible docking corridor cadence is fixed:
- ordinary frames: 500 m;
- final-zone frames: 250 m.

Do NOT change these values without an explicit user request.
Do NOT add launch-only 125 m frames or arbitrary fractional transition frames.

The user also requires docking to have a real stop boundary:
```text
long approach -> hold point -> STOP -> final short ingress
```

Do not collapse this back into one trajectory.

## Flight-family doctrine

Assisted:
- hull nose defines desired travel direction;
- course follows nose with finite lag (target about <=2–3 s);
- forward/reverse main controls longitudinal speed;
- automatic velocity-to-nose stabilization is ordinary Assisted course authority;
- physical manoeuvre/RCS is precision authority, not ordinary route-curvature authority;
- angular speed/acceleration limits remain real.

Newtonian/heavy:
- velocity independent from hull attitude;
- ordinary doctrine is coast -> rotate -> main burn -> coast -> rotate/flip -> brake;
- docking uses mostly long piecewise-straight legs;
- precision RCS is not fake sustained lateral main thrust.

## Controller ownership

Do NOT create a second NPC ship entity for player Autopilot.

Correct model:
```text
same Ship entity
Human controller -> Autopilot controller
same ShipDynamics / installed hardware / fixed-step physics
```

`ControlRegistry::takeAutopilotControl()` changes command ownership only.

## Latest live evidence

User saw:
```text
[DockAuto] request=1 phase=plan-failed
reason=trajectory:angular trajectory cannot reach requested terminal state
action=restore-human
```

This proved Automatic reached server planning and Human was restored only because the executable program failed acceptance.

The code review found the architectural cause:
the long approach trajectory appended a pre-capture point and forced that same long program to end with:
- moving pre-capture terminal velocity;
- exact docking-port orientation;
- exact docking-port angular velocity.

That violated the agreed two-stage docking sequence.

## Current Automatic architecture on main

`DockingAutomaticRuntime` now has:
```cpp
enum class Stage {
    ApproachHold,
    FinalIngress
};
```

### Stage 1 — ApproachHold

- Build ordinary collision-free docking route.
- End at the DockingAdvisory standoff/hold gate.
- Terminal translational velocity = zero.
- Do NOT append pre-capture to the long route.
- Do NOT require exact dock terminal orientation/omega.
- Execute with normal AcceptedManeuverProgram/Follower/Bridge/Ship physics.

On completion:
```text
stage=approach-hold phase=hold-complete next=final-ingress
```

Then:
- command BrakeToStop;
- discard transit program/control bridge;
- stabilize on the real authoritative ship state;
- start a new planning job for FinalIngress.

### Stage 2 — FinalIngress

- Recompute current/predicted docking-port pose.
- Build only the short hold-to-pre-capture route.
- Use three collinear points so scalar path-progress timing is available.
- Initial alignment uses the complete dock basis.
- Exact terminal port orientation and angular velocity apply only here.
- Generic angular-time relaxation may slow this short route if needed.
- Completion ends at collision-free pre-capture; physical latch/contact remains later work.

## Yaw / pitch / roll

Docking attitude is full 3-D.

FinalIngress target basis:
```text
forward = -port.forward
up      =  port.up
right   = derived orthogonally
```

Forward constrains nose direction.
Up constrains roll around the nose axis.

Existing Aligning compares both forward and up vectors, and angular state is represented by a 3-D omega vector/quaternion kinematics. Therefore yaw, pitch and roll are all represented. There is no yaw-only limitation.

## Angular timing

General trajectory generation still supports slowing translation when an exact angular terminal boundary needs more time. It does NOT widen max angular rate/acceleration or terminal tolerances.

Detailed angular rejection reasons include:
- `terminal-omega-unreachable-before-sample=...`
- `terminal-orientation-error-deg=...`
- `terminal-omega-error=...`
- `invalid-angular-step ...`

For the long ApproachHold stage, exact dock terminal angular state is no longer requested.

## Manual launch geometry and 500m frames

Latest native test failure:
```text
nose-first corridor contains a hard first-turn kink: 36.0804 deg
```

That test criterion was wrong.

A 500 m published corridor frame is a sparse chord of the underlying route. A >30 degree change between consecutive display chords does not prove that the underlying curve has a geometric discontinuity.

DockingAdvisoryPlan now exposes:
```cpp
bool initialTurnPresent;
double initialTurnRadiusMeters;
```

The planner sets these only when the first nose-first transition is an actual tangent circular fillet.

Native test now requires:
- first published gap about 500 m;
- first segment along hull nose;
- `initialTurnPresent == true`;
- positive launch turn radius;
- later published route visibly departs the initial axis.

It no longer invents a 30-degree limit between 500 m display chords.

## Dock picking

Docking ports are semantic subtargets:
- physical hit polygon OR 22 px semantic hit radius;
- pick priority 1000;
- nearest dock wins among overlapping ports;
- parent infrastructure triangle picking runs only if no dock overlay consumed the click.

`verify_docking.sh` includes the map-object overlay contract.

## Immediate Windows/MSYS2 gate

Run:
```bash
cd /d/__elite/work

git pull --ff-only origin main
git log -1 --oneline

bash verify_docking.sh
```

Do not build/run if verify fails.

If verify passes:
```bash
bash build_mingw64.sh
build/EliteGame.exe
```

## Exact live test

Do NOT press CALCULATE TRAJECTORY first.

1. Select a dock.
2. Press START DOCKING.
3. Capture all `[DockAuto]` and `[DockAdvisory]` lines.
4. Expected long-stage logs:
```text
stage=approach-hold phase=planning-async
[DockAuto] planned ... stage=approach-hold phase=aligning|executing
```
5. Ship must physically travel to the hold point.
6. At hold completion:
```text
stage=approach-hold phase=hold-complete next=final-ingress
```
7. Ship must physically stop/stabilize.
8. Then:
```text
stage=final-ingress phase=planning-async
[DockAuto] planned ... stage=final-ingress phase=aligning|executing
```
9. Final alignment must be full-basis (yaw/pitch/roll), then execute the short ingress.
10. If it fails, use the first exact failure line; do not re-merge stages.

## Non-negotiable invariants

- manual visible frames = 500 m ordinary / 250 m final;
- long docking approach ends at a real hold/stop point;
- pre-capture belongs to separate FinalIngress;
- exact rotating-port orientation/omega belongs only to FinalIngress;
- same Ship entity changes controller Human -> Autopilot;
- Planner owns route/reference/program;
- Follower closes bounded tracking error only;
- Manual and Automatic Assisted share one game-flight law;
- physical RCS is not ordinary Assisted course authority;
- full docking attitude includes roll via forward+up basis;
- no direct authoritative position/velocity/orientation rewrites;
- no planner-only target collision bypass;
- no stale program after physical alignment;
- no synchronous fixed-step planner retry storm;
- START DOCKING does not require prior manual route calculation.

## Verification status

Newest two-stage Automatic code, launch-fillet diagnostic/test correction, angular diagnostics/timing relaxation, 500/250 cadence lock and dock-picking priority are committed to public main.

Fresh target `verify_docking.sh`, canonical MinGW build and live two-stage Automatic evidence are PENDING.
