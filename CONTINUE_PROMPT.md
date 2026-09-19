# Elite Navigation v2 — continue prompt

Use this as the complete handoff prompt for a new chat. **Replace this entire file on every state-affecting iteration; never append history here.** Detailed history stays in `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md`.

## Repository / workflow

- GitHub: `forzub/Elite`, branch `main`.
- Target machine: Windows 10, MSYS2 MinGW64, g++ 15.2.0, CMake + Ninja.
- Local checkout: `D:/__elite/work`.
- Work directly in repo, then give exact local verification commands.
- After every state-affecting pass: update the four historical MDs and fully rewrite this file.
- Last target-machine checkout actually exercised: `46f6a37da6775a1d044391f773476df1bb07bc6a`.
- Last fully accepted Stage-12 baseline remains the accepted baseline already recorded in the state files.
- Current unverified code/contract candidate before documentation commits: `db79542ad0547f34dfadcb933bd1135067c145c7`.

## Audit method now mandatory

Canonical document: `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`.

Audit every stage left-to-right using:

```text
INPUT
RESPONSIBILITY
OUTPUT
HANDOFF
PERFORMANCE
VERDICT
```

A green isolated test is insufficient if the stage omits necessary truth, passes unnecessary owner state, re-derives neighbor semantics, or produces an output that cannot contribute to the final navigation task.

Ask for every code block:
- Which pipeline stage owns it?
- What authoritative input does it need?
- What exact question does it answer?
- What minimal/sufficient output does the next stage need?
- Does it preserve semantics without re-deriving them?
- Is it bounded at the cadence where it runs?
- Would deleting it make the system clearer without losing required truth?

## Hard feasibility rules

```text
GEOMETRIC FREE SPACE != EXECUTABLE ROUTE
```

A geometric ray may only be a candidate. It must not cross `ACCEPT` until the actual current vehicle can execute it from current P/V/A/q/omega with its real propulsion, control law, damage-degraded capability and bounded pilot execution uncertainty.

Global/topological routing may remain coarse but every edge/portal must be vehicle-feasible at that abstraction level. Local accepted segments must be time-parameterized and dynamically feasible.

For a main-engine-dominant Newtonian craft, substantial delta-v normally uses hull rotation + main-engine burn, with coast / trim / later rotate or flip-and-burn as required. The small RCS/manoeuvre system is precision/trim/parking/docking/capture authority, not a hidden omnidirectional main engine.

## Behavior character

Behavior character = **Situation/Doctrine + Pilot model/transient pilot state**. Vehicle capability is separate physical truth.

Situation families:
- Ordinary/Rational: large reserve, wide deviations, smooth movement, stopping/almost stopping allowed.
- Extreme/Attack/Escape: speed/progress, threat exposure, projected silhouette, cover/masking; narrower clearance, higher load, survivable glancing contact/expendable equipment loss may be acceptable.
- Precision ingress/retrieval/docking/squeeze: slow early, exact aperture acquisition/alignment, staging/near-stop, fine RCS.

Pilot model includes reaction, decision cadence, latency, anticipation, damping/overshoot, slew/aggressiveness, deterministic precision error, hull/clearance judgement uncertainty, spatial/control-law skill, confidence/risk bias. Collision/blast/G/fatigue/panic/injury/sensor disruption may temporarily degrade these values. Pilot never changes hull geometry or propulsion capability.

## Performance / hundreds of ships

Do not use dense N x N as the architecture.

Current NavigationMap already uses a spatial hash and bounded local queries; current Stage-12 failure is not an N^2 failure.

Scalable target:

```text
WORLD DYNAMIC SNAPSHOT
    -> one scene-wide spatial broadphase
    -> sparse unordered potentially-interacting pairs
    -> batch/SIMD kinematic filtering
    -> per-agent influence lists
    -> exact HitVolume/trajectory proof only for survivors
```

Matrix/SIMD computation is useful after sparse pair isolation. Desired pair work is approximately O(N*k), with k = local physically relevant neighbors. Per-agent duplicate pair discovery in the current prototype is a profiling/optimization follow-up.

## Current pipeline audit status

- P0 world/scene publication: OK for current fixture.
- P1 dynamic broadphase: OK prototype / optimization follow-up.
- P2 static/global topology: incomplete vehicle-feasibility filtering beyond current geometry/traversal constraints, not immediate failure.
- P3 doctrine: architecture defined, live integration missing.
- P4 pilot: execution core exists; planning uncertainty/transient degradation partial.
- P5 local free-space: geometry works, but AdjustedClear output is only geometric.
- P6 physical maneuver generation: missing for ordinary visibility; PRIMARY architecture gap.
- P7 continuous proof: good precision components exist but ordinary visibility does not use them.
- P8 ManeuverDecisionController: exists; ordinary live chain bypasses it.
- P9 accepted product: one concrete semantics bug corrected in current candidate.
- P10 follower: coherent relative to accepted input.
- P11 typed NavLocal/System boundary: coherent.
- P12 PilotSkill core: coherent.
- P13 propulsion/physics: coherent and correctly exposes impossible upstream requests.
- P14 monitor/replan scheduling: architecturally correct but depends on truthful accepted maneuvers.

## Current live failure from last target-machine run

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

This proves route discovery/motion are not dead. A real geometric visibility bypass is found and replicated, but it does not complete.

## First P9 correction now in main

Previous defect:
```cpp
accepted.alignForward = lastPlan.portalTraversalActive;
```

This re-derived current maneuver attitude from future route context.

Now:
- `NavigationRuntimePlanner::Result` publishes `selectedManeuverRequiresForwardAlignment` and `selectedManeuverForwardMap`.
- only the selected CURRENT nominal portal capture/transit maneuver sets them;
- AdjustedClear leaves them false;
- GameSimulation copies them into AcceptedShortSegment;
- runtime regression pins future-portal + AdjustedClear => no inherited portal alignment;
- architecture gate forbids restoring the old inference.

## One-shot P5 -> P9 -> P13 witness added

The first real moving-pair visibility bypass now records:
- time, replan reason, segment revision;
- selected deflection/target;
- agent P/V;
- desired velocity;
- accepted alignForward + desired forward;
- follower ideal acceleration;
- PilotSkill executed acceleration;
- applied main-engine acceleration;
- applied RCS acceleration;
- applied total acceleration.

The second-phase self-test failure prints this witness. It is one-shot, not per-frame spam.

## Next task

1. Target-machine verify current candidate.
2. Use the printed first-bypass witness to confirm exact P5/P9/P13 data continuity.
3. Then repair P6/P7, not the test fixture:
   - LocalAvoidance ray remains a free-space candidate only.
   - Generate control-law/propulsion-compatible maneuvers.
   - For current Newtonian Cobra prefer physically valid coast/rotate/main-burn/trim/brake/flip-and-burn primitives instead of arbitrary lateral acceleration.
   - Include real angular/linear authority, time-to-hazard and pilot execution uncertainty.
   - Continuously prove hull/geometry/dynamic safety before ACCEPT.
   - Feed only physically valid candidates to ManeuverDecisionController/doctrine ranking.

Do not weaken the ordered self-test, move the blocker away merely to pass, restore conservative sphere as collision authority, re-enable legacy planners, or use RCS as a fake sideways main engine.

## Verification command

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_foundation_lock.py &&
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py &&
bash tests/navigation_runtime/run_mingw64.sh &&
bash build_mingw64.sh &&
./build/headless_server/EliteServer.exe --self-test-navigation
```

If NavigationMap/LocalHorizon/LocalAvoidance changes in the next slice, also run their isolated MinGW suites.
