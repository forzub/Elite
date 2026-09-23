# CONTINUE PROMPT — build a spatial physical chain before proof or execution

Continue in repository `forzub/Elite`, branch `main`.

The normative architecture is
`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`. M1 is
accepted, the M2 scenario-I/O boundary is in place, and direct replacement of
the legacy translation-first physical author is authorized.

## Read before editing

1. `AGENTS.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
6. final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`;
7. `src/game/navigation/NAVIGATION_API_CONTRACT.md`.

Inspect the active files:

- `OrdinaryPhysicalManeuverCompiler.{h,cpp}`;
- `PhysicalManeuverSearchCoordinator.{h,cpp}`;
- their focused tests;
- files referenced directly by the next replacement seam.

## Accepted evidence

Exact commit `9af337c2e23a32d5f11d34a3e048ecd98842674d` passed
`maneuver_chained_limit_matrix` and
`ordinary_physical_maneuver_compiler` on MinGW64: 2/2 tests passed, 0 failed,
0.12 s.

The typed physical rejection contract is accepted. Do not reopen it by
returning generic failure, inventing omnidirectional authority or allowing main
burn before its required Newtonian attitude.

Exact commit `b506397ca30f223ee6cb29597c8673e0815626d1` also passed the
corrected `physical_maneuver_search_coordinator` test (1/1, 0.04 s).

These are unit/contract results, not integrated-system acceptance.

Remote commit `15f4c6c6f856cc9cf7974ef1527730315807c880` also built and
ran in the target viewer. The observer layer is visible and correctly remains
unaccepted. A 20.60 m/s Newtonian run then exposed 50/801 infeasible legacy
actuator intervals and tracking invalidation at 5.33 s on the first corner,
without collision.

## Implemented candidate

`tools/navigation_runtime` now builds an observer-only physical frontier from
explicit scenario/vehicle/runtime API values. `TracePhysicalSearch` records
ranked alternatives, typed rejections and sampled physical candidates; trace
JSON preserves it. The viewer draws rejection markers and unproved candidates,
body-forward axes and acceleration vectors separately from legacy Ruckig,
accepted reference and actual motion. Its title includes
`PHYS-OBS=... (НЕ ПРИНЯТО)`.

The adapter does not construct `AcceptedManeuverProgram` and does not influence
Follower or physics. Every candidate remains `requiresContinuousProof=true`.

## Immediate work

Do not continue into B6 proof or accepted-program execution yet. Replace the
initial-only physical probe with a typed spatial capture task and a deterministic
receding-horizon candidate chain.

The viewer must distinguish:

- legacy route and legacy Ruckig reference;
- ranked/rejected alternatives and typed witness reasons;
- selected unproved physical candidate;
- attitude and thrust/acceleration phases;
- future proved tunnel, accepted reference and actual motion.

The observer adapter may depend on tool/runtime presentation types. The pure
coordinator/compiler may not depend on trace, viewer, JSON, filesystem or
OpenGL. Observer mode must not steer physics or construct an accepted program.

The key uncovered API defect is that
`geometricTargetPositionMapMeters` currently affects candidate metadata but not
candidate dynamics. First add a regression proving that a changed capture
plane/corner/terminal volume cannot be silently ignored when desired velocity
happens to be the same. Targets on the same unconstrained ray may share a short
progress primitive; typed progress/capture semantics decide eligibility. Then
make those semantics an explicit solve input.

Chain bounded candidates by exact terminal position, velocity, attitude and
angular velocity. Do not advance to the next corridor leg until its capture
condition is met. On failure, use typed witnesses to vary speed, arrival time,
terminal sample or corridor while keeping the objective active.

Only after straight, corner and high-speed Newtonian chains are spatially and
physically coherent should B6 continuously prove the exact sampled candidates.
Keep proof output separate from B5 and do not publish B8 acceptance until exact
capability, resource and geometry checks all refer to the same maneuver history.

## Accepted coordinator design

`PhysicalManeuverSearchCoordinator` consumes a revisioned ranked frontier of
explicit corridor/terminal/speed/arrival-time alternatives. It owns a bounded
attempt budget and resumable cursor, but it does not generate mission doctrine,
change capability, prove geometry or publish accepted programs.

First target run of commit
`3492ca3ba314dcf250c5d3ebc03c6e8cc0c3dce6` compiled the new test.
It exposed a fixture error: a 135-degree rotation needs about 4.60 s before
burn, but the supposedly feasible alternative allowed only 4.0 s. The corrected
fixture uses 6.0 s and passed at `b506397c`.

Accepted semantics:

- typed rejection advances to later ranked alternatives;
- `SearchPending` resumes without repeated work;
- `FrontierExhausted` retains objective ownership and rejection history;
- `SharedStateBlocked` preserves untried alternatives;
- stale objective or frontier revision fails before physical work; the two
  revisions remain independent.

The read-only viewer snapshot is target-validated as instrumentation. It also
proved that the current single primitive cannot validate a route. Spatial
receding-horizon chaining precedes continuous proof; literal execution and
accepted publication remain later gates.

## Non-negotiable model

```text
NavigationIntent + TerminalContract
 -> ranked revisioned frontier
 -> bounded physical coordinator
 -> physical candidates or typed witnesses
 -> exact capability/resource + continuous swept-hull proof
 -> AcceptedManeuverProgram
 -> literal actuator execution + bounded feedback reserve
```

A failed attempt never disables navigation. The objective remains active, the
last still-proved short program may continue, and exhaustion requests a new
frontier or proved safe fallback. Unavoidable contact is a separate physically
executable, explicitly contact-predicted mitigation solve.

## Forbidden shortcuts

- no tracking-timeout increase or weakened high-speed assertion;
- no accepted interval with infeasible actuator allocation;
- no unreachable mathematical reference called a trajectory;
- no sphere/AABB broadphase used as exact free-space truth;
- no STANDARD/EXTREME modification of ship physics;
- no pilot skill used to repair nominal feasibility;
- no synthetic reverse-main authority in ASSISTED mode;
- no coordinator mutation of measured state or vehicle capability.

## Iteration protocol

After each state-affecting event update the blueprint, current state/task,
project state, Stage-12 journal and this prompt. Run all available gates, inspect
the complete diff, commit and push one coherent iteration to `main`. Never claim
a target result that was not run.
