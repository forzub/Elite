# Elite Navigation v2 — continue prompt

Use this file as the complete handoff prompt in a new chat. **Replace this entire file on every state-affecting iteration; never append history here.** Detailed history stays in `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md`.

## Repository / workflow

- GitHub: `forzub/Elite`, branch `main`.
- Target machine: Windows 10, MSYS2 MinGW64, g++ 15.2.0, CMake + Ninja.
- Local checkout: `D:/__elite/work`.
- Work directly in repo, then give exact local verification commands.
- After every state-affecting pass: update the four historical MDs above and fully rewrite this file.
- Last target-machine checkout actually exercised: `46f6a37da6775a1d044391f773476df1bb07bc6a`.
- Last fully accepted Stage-12 target-machine baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.

## Fixed Navigation v2 contracts

Automatic navigation:
```text
PLAN -> prove short segment -> ACCEPT -> EXECUTE + MONITOR -> REPLAN only on invalidation
```

Never restore per-frame replanning, never silently stop navigation authority, and never re-enable legacy route planners. Manual and automatic remain separate. Assisted/airplane-like and Newtonian control laws remain separate.

NavigationMap / NavigationSpace / planner / follower are NavLocal-only. System/world values cross only through `NavigationFrameBoundary`. Owner state stays behind owner boundaries.

Legacy runtime paths remain hard-off:
- old client docking route pipeline;
- old repair-drone route pipeline (fail-closed until ported);
- dormant LocalGuidance/Ruckig authority.

## Current live failure

Freshly built self-test reached:
```text
[FAIL] visibility-bypass replication succeeded but the ordered live flight did not complete moving-pair bypass, direct recovery and exact-static tunnel passage inside the 120 s bound

moving_gap_passed=0
slit_portal=0
slit_entry_capture=0
slit_entry_crossed_aligned=0
passed_obstacle_plane=0
exact_static_violation=0
simulated_s=120
slit_entry_fwd_angle_rad=0.000679006
slit_entry_lateral_mps=0.00298481
slit_entry_cross_track_m=148.3
```

This proves:
- a real visibility bypass was calculated/executed enough for same-tick replication proof;
- the actor can move;
- exact-static collision remains clean;
- failure is upstream of slit/tunnel;
- it does not finish the moving-pair bypass / plane crossing / direct recovery.

## Concrete scene numbers

NavLocal == authored visual hub axes for this lab.

- start: `(975,-1300,-6200)`
- final goal: `(975,-1300,1000)`
- moving upper center: `(975,-650,-5700)`
- moving lower center: `(975,-1150,-5700)`
- each moving GuidanceDockCube: logical `360 x 360 x 900 m`
- physical moving aperture: 140 m, centered Y=-900
- moving pair velocity: `(0,0,+1) m/s`
- slit entry center: `(975,-1180,-4950)`
- slit approach point: `(975,-1180,-5450)`
- start -> slit approach delta: approximately `(0,+120,+750)`, length ~759.5 m.
- lower moving cube front face is only about 50 m ahead of the start before hull/safety inflation, so the first bypass is physically demanding.

Cobra:
- maxLinearGs = 7.5 -> main forward authority ~73.55 m/s^2;
- manoeuvreThrusterAccel = 2.0 m/s^2;
- main engine forward-only;
- lab uses Newtonian control mode.

## End-to-end audit result

### Working stages

1. **Scene / publication:** working. Real HitVolume-derived dynamic OBBs reach NavigationMap; sphere is broadphase only. Source-precision velocity/angular tolerances are fixed.
2. **Global/static route:** working. NavigationSpace selects approach -> slit portal -> tunnel -> departure topology and provides the correct first portal/approach waypoint.
3. **Dynamic collision truth:** working. LocalHorizon sees the real moving OBB and the corrected fixture produces a true visibility conflict.
4. **Local geometric avoidance:** working as geometry. LocalAvoidance finds an AdjustedClear bounded ray.
5. **Frame conversion / runtime control / PilotSkill / physics:** individually coherent. Revisions and vectors survive DTO boundaries; physics correctly clamps to real propulsion.

### Broken integration seam

Primary defect is:
```text
LocalAvoidance AdjustedClear
  -> NavigationRuntimePlanner desired velocity
  -> GameSimulation AcceptedShortSegment
  -> TrajectoryFollower / physical execution
```

There are TWO coupled problems.

**A. Ordinary visibility is geometric, not dynamically reachable.**
- `NavigationRuntimePlanner::AgentState` has linear/angular capability + control mode.
- But ordinary `LocalHorizonPlanner::AgentState` has only position/velocity/accel/radius.
- LocalAvoidance fan (15/30/45/60/75 deg) proves straight geometric rays but never checks whether the ship can acquire that ray before the obstacle using its actual anisotropic propulsion.
- Capability is currently consumed only by `MovingPassageTrajectoryEvaluator`.
- The lab explicitly disables MovingPassage steering authority for this ordinary encounter.
- Therefore a steep visibility ray can be declared safe while being physically untrackable with 2 m/s^2 lateral RCS.

Illustrative reconstruction from the real fixture (not the exact latest unlogged selected azimuth): a ~75 deg downward ray may imply desired velocity about `(0,-54.8,+24.5) m/s` and ideal acceleration from rest about `(0,-41.1,+18.4) m/s^2`. That is geometrically meaningful but cannot be produced with the hull fixed forward and only 2 m/s^2 RCS lateral authority.

**B. Accepted-segment packaging changes planner maneuver semantics.**
- In `NavigationRuntimePlanner`, portal alignment angular control is applied only when portal traversal is active AND local status is `NominalClear`.
- During `AdjustedClear`, planner angular intent is only stabilization; it does NOT request forward-to-portal alignment.
- But `GameSimulation` currently sets:
  `accepted.alignForward = lastPlan.portalTraversalActive`
  and `desiredForwardMap = portalNormal`
  regardless of AdjustedClear.
- Thus merely having a future oriented portal on the global route forces the follower to point the ship at +Z even while local avoidance asks for a lateral bypass.
- Latest failing evidence confirms premature alignment: `slit_portal=0` while `slit_entry_fwd_angle_rad=0.000679006` (~0.039 deg), so hull is already almost perfectly portal-aligned before the slit phase.
- With nose ~+Z, DynamicMotionSystem gives the forward component to the main engine but clamps the lateral remainder to 2 m/s^2. The executed path therefore diverges sharply from the geometric safe ray.

This is why individually correct pieces fail together.

## Missing intended architecture layer

`ManeuverDecisionController` and `CONTROL_LAW_MANEUVER_MODEL.md` already state the correct rule: do not issue an arbitrary acceleration vector and assume the flight law can realize it. Newtonian should choose from physical primitives such as coast/drift/RCS trim/turn-then-burn/flip-and-burn/precision 6DoF pass. Current live lab does not invoke `ManeuverDecisionController` for ordinary visibility.

## Next implementation/debug step

Do not weaken self-test, move the fixture farther away just to pass, restore sphere authority, or re-enable legacy planners.

First add bounded transition diagnostics (not per-frame spam) at plan/replan/bypass transitions:
- time + replan reason;
- planner status + selected deflection/target;
- agent position/velocity;
- accepted segment target/expiry/completion;
- accepted `alignForward` and desired forward;
- follower ideal acceleration;
- PilotSkill executed acceleration;
- physically applied main/RCS/total acceleration;
- route progress and moving-pair plane progress.

Then fix the contract:
1. an AdjustedClear bypass must not inherit later portal forward alignment unless that specific maneuver requires it;
2. ordinary visibility must become capability/time-aware OR escalate to a control-law-compatible trajectory/maneuver primitive before acceptance;
3. wire the intended maneuver-candidate/decision layer rather than directly turning a geometric ray into arbitrary acceleration.

The exact selected deflection/replan cycle of the latest target-machine failure is not printed yet, so runtime instrumentation should confirm the predicted divergence before declaring the algorithmic correction accepted.

## Verification after the next candidate

Run architecture + any isolated modules touched + runtime + canonical build + fresh self-test. At minimum:

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_foundation_lock.py &&
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py &&
bash tests/navigation_runtime/run_mingw64.sh &&
bash build_mingw64.sh &&
./build/headless_server/EliteServer.exe --self-test-navigation
```

If NavigationMap/LocalHorizon/LocalAvoidance changes, also run their isolated MinGW suites.
