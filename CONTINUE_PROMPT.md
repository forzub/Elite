# Elite Navigation v2 — continue prompt

Use this as the complete handoff prompt for a new chat. **Replace this entire file on every state-affecting iteration; never append history here.** Detailed history stays in `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md`.

## Repository / workflow

- GitHub: `forzub/Elite`, branch `main`.
- Target machine: Windows 10, MSYS2 MinGW64, g++ 15.2.0, CMake + Ninja.
- Local checkout: `D:/__elite/work`.
- Work directly in repo, then give exact local verification commands.
- After every state-affecting pass: update the four historical MDs and fully rewrite this file.
- Last target-machine checkout actually exercised: `46f6a37da6775a1d044391f773476df1bb07bc6a`.
- Last fully accepted Stage-12 target-machine baseline remains `daaf038021cdf8b9561db60fdd35e7cefce0b2df`.

## Non-negotiable Navigation v2 contracts

Automatic loop:
```text
PLAN -> prove short segment -> ACCEPT -> EXECUTE + MONITOR -> REPLAN only on invalidation
```

Never restore per-frame replanning, never silently stop navigation authority, and never re-enable legacy route planners.

### New hard feasibility invariant

```text
GEOMETRIC PATH != EXECUTABLE ROUTE
```

A geometric free-space ray may seed search, but it may not cross `ACCEPT` until the actual current vehicle can execute it from its current P/V/A/attitude/angular state with its real propulsion, control law, damage-degraded capability and bounded pilot execution error.

Global route may be coarse, but every edge/portal must be vehicle-feasible at its abstraction level. Local accepted segments must be time-parameterized and dynamically feasible.

Canonical document: `src/game/navigation/NAVIGATION_BEHAVIOR_CHARACTER_MODEL.md`.

## Propulsion contract

For a main-engine-dominant Newtonian craft, substantial course-change delta-v normally uses:

```text
coast / preserve useful inertia
-> rotate hull toward burn vector
-> main-engine burn
-> coast / RCS trim
-> rotate / flip-and-burn for later braking as required
```

The manoeuvre/RCS system is normally trim/precision/parking/docking/portal-capture authority, not a hidden omnidirectional main engine. Different craft may use omnidirectional thrusters as primary translation only when their actual propulsion profile says so.

## Behavior character model

Behavior character has two independent inputs.

### Situation / doctrine
Initial families:
- Ordinary / Rational: wide clearance, smooth motion, low urgency, stopping/almost stopping is acceptable.
- Extreme / Attack / Escape: preserve useful speed/progress, minimize threat exposure and projected silhouette, use cover/masking geometry, tolerate narrower clearances/high loads and possibly survivable glancing contact or loss of expendable external equipment.
- Precision ingress / squeeze / retrieval / docking: slow early, exact aperture acquisition, alignment, high clearance/control margin, staging/near-stop allowed, fine RCS appropriate.

Doctrine changes search bounds, ranking weights, allowed risk/load/contact bands and willingness to stop. It never changes physics.

### Pilot model / transient pilot state
Pilot parameters include:
- reaction delay;
- perception/decision rate;
- command latency;
- anticipation quality;
- response bandwidth/damping/overshoot;
- command slew/aggressiveness;
- deterministic precision/noise;
- hull-size/clearance judgement uncertainty;
- spatial judgement/control-law familiarity;
- risk/confidence bias.

Impact, blast/shock wave, injury, G exposure, fatigue, panic/stress, sensor disruption, etc. may temporarily degrade these values. This expands execution uncertainty or worsens control response; it never changes hull geometry or propulsion capability.

## Current live failure

Fresh self-test reached:
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

This proves route discovery/motion are not globally dead. A real visibility bypass is found and replicated, but it is not completed.

## Localized defect

Working:
- scene + HitVolume publication;
- global/static corridor;
- dynamic OBB collision truth;
- geometric LocalAvoidance;
- frame conversion;
- PilotSkill DTO/reaction/latency/filter;
- authoritative propulsion physics.

Broken seam:
```text
LocalAvoidance geometric AdjustedClear
  -> NavigationRuntimePlanner desired velocity
  -> GameSimulation AcceptedShortSegment
```

Two defects:
1. ordinary visibility is geometry-only and can select a ray the actual craft cannot acquire in time;
2. GameSimulation currently sets `accepted.alignForward = lastPlan.portalTraversalActive`, so an AdjustedClear bypass can be forced to point the hull at a future slit portal even though the planner itself did not request portal alignment during AdjustedClear.

For current Cobra:
- maxLinearGs = 7.5 => forward main authority ~73.55 m/s^2;
- manoeuvreThrusterAccel = 2 m/s^2;
- main engine forward-only;
- lab uses Newtonian.
So tens of m/s^2 of lateral desired acceleration while holding the nose at +Z is not a physically executable maneuver.

## Required next architecture/code path

Do not fix by moving the fixture farther away, weakening self-test, restoring sphere authority, or using RCS as a sideways main engine.

Implement toward:
```text
free-space candidate
  -> control-law + propulsion compatible maneuver primitives
  -> time/capability/pilot-aware continuous proof
  -> maneuver decision under doctrine
  -> accepted time-parameterized segment
  -> PilotSkill execution
  -> physics
```

Concrete immediate corrections:
1. AdjustedClear must not inherit future-portal forward alignment unless its selected maneuver explicitly requires that attitude.
2. Ordinary visibility rays are candidates only; before ACCEPT they must become physically realizable maneuvers.
3. Newtonian main-engine-dominant avoidance must be able to rotate + main-burn + coast + brake/flip-burn rather than demanding impossible lateral RCS.
4. Situation/doctrine and pilot profile must both be explicit inputs to candidate generation/proof/ranking.

Before declaring the algorithmic fix accepted, add bounded event diagnostics on plan/replan/segment transitions (not per-frame spam): selected deflection/target, P/V, accepted alignment, ideal/executed/applied acceleration split, replan reason, route and moving-plane progress.

## Verification

After the next candidate:
```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_foundation_lock.py &&
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py &&
bash tests/navigation_runtime/run_mingw64.sh &&
bash build_mingw64.sh &&
./build/headless_server/EliteServer.exe --self-test-navigation
```

Run NavigationMap/LocalHorizon/LocalAvoidance isolated MinGW suites too if those layers change.
