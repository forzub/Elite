# Elite Navigation v2 — continue prompt

Use this as the complete handoff prompt for a new chat. **Replace this entire file on every state-affecting iteration; never append history here.** Detailed history stays in `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md`.

## Repository / workflow

- GitHub: `forzub/Elite`, branch `main`.
- Target machine: Windows 10, MSYS2 MinGW64, g++ 15.2.0, CMake + Ninja.
- Local checkout: `D:/__elite/work`.
- Work directly in repo, then give exact local verification commands.
- After every state-affecting pass: update the four historical MDs and fully rewrite this file.
- Last target-machine checkout actually exercised remains `46f6a37da6775a1d044391f773476df1bb07bc6a`.
- Last fully accepted Stage-12 baseline remains the accepted baseline already recorded in the state files.

## Mandatory pipeline audit

Canonical: `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`.

Audit every stage as:
```text
INPUT
RESPONSIBILITY
OUTPUT
HANDOFF
PERFORMANCE
VERDICT
```

A stage is not accepted merely because its isolated test is green. Its output must be necessary/sufficient for the next stage, preserve maneuver semantics, and not steal another layer's responsibility.

## Hard feasibility rule

```text
GEOMETRIC FREE SPACE != EXECUTABLE ROUTE
```

No route/segment may cross ACCEPT until it is feasible for the actual current vehicle from current P/V/A/q/omega with real propulsion, control law, damage-degraded capability and bounded pilot execution uncertainty.

Global topology may remain coarse, but every edge/portal must be vehicle-feasible at its abstraction level.

## Portal semantics

Portal existence does NOT imply mandatory centerline capture or hull-to-normal alignment.

Correct question:
```text
for this opening geometry/motion
+ this vehicle
+ this current state
+ this pilot execution envelope
+ this Situation/Doctrine

which passage maneuver is physically safe/acceptable?
```

Conceptual passage classes:
- `FreeTransit`: wide/high-margin opening, low relative speed, no unnecessary stop/centering/alignment.
- `PrecisionCapture / PrecisionTransit`: tight fit, large cross-track/relative motion, required orientation, docking/retrieval/Precision doctrine, or large pilot uncertainty.
- `ExtremeTransit / ContactExpected`: higher speed/load, smaller margin, cover/silhouette priority, survivable glancing contact/expendable loss only when explicitly allowed.

A wide safe portal in Ordinary/Rational behavior should behave almost like ordinary free space.

## Command ownership / target API

Canonical: `src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md`.

Ownership:
1. Mission/objective owner chooses the semantic goal/terminal contract.
2. Global route chooses region/portal/corridor topology only.
3. Maneuver planner compiles the next bounded physical maneuver.
4. Accepted product contains the SAME program that was capability/geometry-proved.
5. Follower samples that program and adds bounded feedback only.
6. PilotSkill applies reaction/latency/precision degradation.
7. Propulsion allocator maps vehicle-level command to actual main/RCS/torque actuators.
8. Physics is final authority.

Target accepted API:
```text
AcceptedManeuverProgram
    revision / validity / proof revisions
    maneuver family / doctrine provenance
    bounded time domain

    reference:
        P(t)
        V(t)
        q/body-basis(t)
        omega(t)

    feed-forward:
        A_ff(t)
        alpha_ff(t)

    terminal tolerances
    tracking envelope / feedback reserve
    collision/clearance witness
```

Program may be analytic or a small fixed-capacity knot/control-key set. It must be bounded.

Why both trajectory + feed-forward:
- trajectory only would force executor to re-solve control;
- acceleration only loses reference state needed for tracking/invalidations;
- proof and execution must consume the same program.

Planner must not bit-drive individual thrusters. It publishes vehicle-level attitude/acceleration program proven against the same capability semantics the allocator uses.

## Current API is transitional

`AcceptedShortSegment` currently stores:
- target position;
- target velocity;
- optional fixed acceleration;
- optional forward alignment.

`TrajectoryFollower` then derives acceleration from target velocity. This can create a different control history from the one that was proved.

Both `NavigationRuntimePlanner.h` and `AcceptedShortSegment.h` are explicitly marked transitional.

Migration target:
```text
current:
    target point/velocity -> follower re-derives control

target:
    accepted proved maneuver program
        -> follower samples same program
        -> bounded tracking feedback
```

## Newtonian turning

A Newtonian turn does NOT imply stopping.

For a main-engine-dominant craft:
```text
preserve useful current inertial V
-> rotate hull in advance toward the future required delta-v / acceleration vector
-> begin main-engine burn while V is still non-zero
-> bend V continuously toward the desired route
-> coast / RCS trim
-> rotate early toward next burn or flip-and-burn if braking is required
```

Angular acceleration/speed plus pilot reaction/latency determine how early lead-rotation begins.

RCS is normally small trim/precision/parking/docking/portal-capture authority, not a fake sideways main engine. A drone with actual strong omnidirectional propulsion may legitimately use a different maneuver family.

## Behavior character

Behavior = Situation/Doctrine + Pilot model/transient state. Vehicle capability is separate physical truth.

Situation examples:
- Ordinary/Rational: large reserve, smooth motion, stopping allowed but not mandatory.
- Extreme/Attack/Escape: preserve useful speed/progress, cover/masking, threat exposure, projected silhouette, narrower clearance/higher load/contact allowance per policy.
- Precision ingress/retrieval/docking/squeeze: slow/controlled capture, high fit/alignment precision, staging/near-stop allowed.

Pilot model includes reaction, cadence, latency, anticipation, damping/overshoot, slew/aggressiveness, deterministic control error, hull/clearance judgement, spatial/control-law skill and transient degradation from impact/blast/G/fatigue/panic/injury/sensor disruption.

## Performance / hundreds of ships

Do not use dense N x N as the architecture.

Current NavigationMap already uses spatial hashing/bounded queries.

Scalable target:
```text
WORLD DYNAMIC SNAPSHOT
 -> one scene-wide spatial broadphase
 -> sparse unordered potentially-interacting pairs
 -> batch/SIMD kinematic filter
 -> per-agent influence lists
 -> exact HitVolume/trajectory proof only for survivors
```

Use matrix/SIMD operations after sparse pair isolation. Desired pair work is approximately O(N*k), where k is local physically relevant neighbors.

## Current pipeline audit

- P0 world publication: OK current fixture.
- P1 broadphase: OK prototype / optimization follow-up.
- P2 global topology: incomplete vehicle-feasibility filtering beyond geometry/traversal.
- P3 doctrine: architecture defined, live integration missing.
- P4 pilot: execution core exists; planning uncertainty/transient degradation partial.
- P5 local free-space: geometric candidates work; AdjustedClear is not yet vehicle-feasible.
- P6 physical maneuver generation: MISSING for ordinary visibility; primary architecture gap.
- P7 continuous proof: good precision components exist but ordinary visibility does not use them.
- P8 ManeuverDecisionController exists but ordinary live chain bypasses it.
- P9 accepted product: old portal-context alignment bug corrected, but accepted API itself is transitional.
- P10 follower: must migrate from re-deriving target-velocity control to sampling accepted program + bounded feedback.
- P11 frame boundary: coherent.
- P12 PilotSkill core: coherent.
- P13 propulsion/physics: coherent.
- P14 replan scheduling: architecturally correct, depends on truthful program.

## Existing P9 correction

Old:
```cpp
accepted.alignForward = lastPlan.portalTraversalActive;
```

Now planner publishes:
```text
selectedManeuverRequiresForwardAlignment
selectedManeuverForwardMap
```

and GameSimulation copies those current-maneuver semantics rather than inferring attitude from route context.

This correction remains valid, but note the refined portal rule: a selected FreeTransit maneuver may legitimately choose little/no alignment; PrecisionTransit may require it; the decision comes from the proved maneuver, not merely portal existence.

## One-shot live witness already added

First real moving-pair visibility bypass records:
- selected deflection/target;
- agent P/V;
- accepted attitude;
- follower ideal acceleration;
- PilotSkill executed acceleration;
- applied main/RCS/total acceleration.

Use this to validate P5->P9->P13 on the next target-machine run.

## Current live failure from previous target-machine run

```text
[FAIL] visibility-bypass replication succeeded but the ordered live flight did not complete moving-pair bypass, direct recovery and exact-static tunnel passage inside the 120 s bound
moving_gap_passed=0
slit_portal=0
slit_entry_capture=0
passed_obstacle_plane=0
exact_static_violation=0
simulated_s=120
```

No current docs/code candidate is accepted until target-machine gates pass.

## Next implementation order

1. Introduce bounded `AcceptedManeuverProgram` representation.
2. Migrate runtime-lab accepted execution seam/follower to sample the same proved program + bounded tracking feedback.
3. Build ordinary Newtonian maneuver generator:
   - free-space candidate is not executable;
   - lead-rotate + main-burn/coast/trim/brake/flip-and-burn primitives;
   - preserve useful non-zero velocity;
   - capability/time/pilot-aware proof.
4. Add portal maneuver selection: FreeTransit vs PrecisionCapture/Transit vs Extreme/contact-expected according to geometry + vehicle + pilot + doctrine.
5. Feed only physically valid candidates into ManeuverDecisionController.
6. Re-audit every pipeline handoff and delete redundant/re-derived semantics.

Do not weaken tests, move blockers merely to pass, restore sphere as collision truth, re-enable legacy planners, or let follower/allocator rescue an impossible planner request.

## Verification command for current candidate

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_foundation_lock.py &&
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py &&
bash tests/navigation_runtime/run_mingw64.sh &&
bash build_mingw64.sh &&
./build/headless_server/EliteServer.exe --self-test-navigation
```
