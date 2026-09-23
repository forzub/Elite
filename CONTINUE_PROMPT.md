# CONTINUE PROMPT — validate M1, preserve the complete navigation contract

Continue in GitHub repository `forzub/Elite`, branch `main`.

`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md` is the
normative architecture. M1 is implemented as a candidate but not accepted; M2
has not started.

## Read completely before editing

1. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`;
6. `src/game/navigation/NAVIGATION_API_CONTRACT.md`;
7. relevant sections of `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`.

For the active M1 gate inspect only the referenced path:

- `src/game/navigation/ManeuverProgramTimeline.h`;
- `src/game/navigation/ManeuverProgramSampler.cpp`;
- `src/game/navigation/TrajectoryFollower.cpp`;
- `src/game/navigation/ManeuverPhaseGate.cpp`;
- `src/game/navigation/NavigationFrameBoundary.h`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- focused sampler and runtime E2E tests;
- navigation architecture checks.

Follow other files only where this code references them.

## Exact active state

The latest target build/link passed, then both identity and moving/rotating-frame
runs failed identically at storage page 1 with `PROGRAM_PAGE_BEFORE_START`.
Duplicate page-clock arithmetic was the cause.

The candidate now has one pure `ManeuverProgramTimeline` owner. Runtime selects
by the next page's canonical start; Sampler, Follower and phase gate use the same
window/elapsed-time API. Unit regressions pin floating-point page boundaries and
later-page completion.

The M1 frame E2E compares every identity/non-identity NavLocal clock, position,
velocity, full basis, control demand and physical acceleration frame. It
requires identical terminal outcome without requiring the known 78-infeasible-
segment M3/M4 plan to complete.

## Immediate action

Run the exact chained MinGW64 command in `CURRENT_TASK.md`, including the
focused `maneuver_program_sampler` test and `navigation_runtime_pipeline`.
Preserve the checkout hash and full output.

Required M1 marker:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

If it passes, accept M1 separately from any later physical-authoring failure and
activate M2. If it fails, repair only the exact timeline/frame boundary without
weakening trace comparisons.

## Normative later-stage semantics

Do not lose these requirements during M2 and later implementation:

```text
certified NavigationSpace
 -> typed route corridor / portals
 -> STANDARD or EXTREME doctrine + risk budget
 -> NEWTONIAN or ASSISTED physical maneuver
 -> nominal capability/resource/continuous hull proof
 -> pilot execution envelope and realized skill error
 -> authoritative physics/contact attribution
```

- free regions are certified cell/convex unions; AABB is broadphase only;
- scalar sphere clearance cannot erase an orientation-traversable slit;
- portal includes aperture, normal, depth, orientation and transit state;
- Squeeze is an orthogonal constrained-passage profile, not a third doctrine;
- `ConstrainedRisk` is still nominally hull-clear and actuator-feasible;
- pilot skill affects latency/error/recovery and may cause envelope departure;
- Assisted cannot invent hardware; Newtonian and Assisted may produce different
  maneuvers through the same corridor.

## Governing invariant

```text
accepted nominal maneuver == capability-checked maneuver
                          == continuously collision-proved maneuver
                          == actuator program executed by physics
```

Risk may reduce robustness margin; it may not falsify geometry or physics.

## After M1 acceptance only

Activate M2: split `NavigationScenarioRuntime.cpp` into a thin composition root
and production-owned parsing, route composition, maneuver planning/proof,
execution harness and diagnostics modules. Preserve behavior. Do not begin
octree/portal work before the ownership split and truthful physical planning
stages required by the blueprint.

## Mandatory iteration protocol

After every state change update the blueprint, state/task/project documents,
Stage-12 journal and affected API/control contracts; recreate this prompt; run
all available gates; inspect the complete diff; commit and push one coherent
iteration to `main`. Never claim a check that did not run.
