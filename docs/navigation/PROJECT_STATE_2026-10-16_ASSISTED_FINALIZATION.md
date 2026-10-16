# PROJECT STATE — ASSISTED FINALIZATION — 2026-10-16

> **Authoritative pre-planning sequence:**
> `docs/navigation/ASSISTED_PREPLANNING_ENTRY_CONTRACT.md`.
> If a test or implementation conflicts with that document, the test/implementation
> must be investigated; the entry algorithm must not be redefined to make the test green.

## Active focus

**Newtonian development is PAUSED.**

Current work returns temporarily to the frozen Assisted line to produce a
fully-green, explicitly verified Assisted baseline before Newtonian development
continues.

Do not advance Newtonian planner/autopilot behavior while this state is active.

## Recovery points

Pre-cleanup recovery branch:

- `restore/pre-assisted-final-cleanup-2026-10-16`
- commit: `8d2062edc38569f1d05afc7c7acbe9275ccc1568`

Earlier accepted Assisted recovery boundary remains:

- `restore/assisted-final-accepted-2026-10-14`
- accepted lineage documented in
  `docs/navigation/ASSISTED_FINAL_ACCEPTANCE_2026-10-14.md`

## Why Assisted was reopened

A full run of `tests/navigation_runtime` on 2026-10-16 produced:

- 64 tests total
- 61 passed
- 3 failed

Failures:

1. `traffic_route_graph`
   - `blue exit lost mandatory tangent-straight departure`
2. `assisted_safe_course_recovery`
   - `wall-clock latency invented Assisted maneuver progress`
3. `segmented_route_composer`
   - `terminal free stage missing`

The physical-model/runtime-switch work itself remained green, including:

- `ship_control_execution_gateway`
- `ship_physical_control_models`
- `ship_physical_actuator_plant`
- `engine_thrust_hud_presentation`
- `ship_physical_corridor_matrix`
- `ship_physical_damage_matrix`
- `ship_state`

## Important finding about the Assisted failure

The failing Assisted safe-course implementation and its regression test were
compared between the frozen Assisted boundary and the current branch.

The following files are unchanged since the frozen Assisted baseline:

- `src/game/navigation/AssistedSafeCourseRecovery.cpp`
- `src/game/navigation/AssistedSafeCourseRecovery.h`
- `src/game/navigation/assisted/AssistedSafeCourseExecutor.h`
- `tests/navigation_runtime/AssistedSafeCourseRecoveryTests.cpp`
- `src/world/navigation/NavigationObstacleGeometry.h`
- `src/world/navigation/NavigationObstacle.h`

Therefore the observed `assisted_safe_course_recovery` failure is not currently
attributed to the recent Newtonian implementation work. It appears to be an
unresolved red regression already present in, or not fully exercised at, the
previous Assisted acceptance boundary.

Before changing Assisted behavior, verify this directly against the frozen
baseline if necessary.

## Assisted finalization rule

The existing accepted Assisted physical/control semantics remain authoritative.

Do NOT redesign Assisted to make tests green.

Allowed changes are limited to:

1. reproduce a concrete failure;
2. determine whether the defect is implementation, test expectation, or shared
   law-neutral infrastructure;
3. add/retain a narrow regression proving the defect;
4. make the smallest correction consistent with
   `ASSISTED_FINAL_ACCEPTANCE_2026-10-14.md`;
5. run the full Assisted/runtime regression suite;
6. preserve all already-green physical and navigation contracts.

No Newtonian-specific semantics may be introduced into Assisted helpers.

## Diagnostic physical-model switching

Temporary diagnostic controls remain enabled:

- `Ctrl+F9`: cycle I / Y / X layout;
- `Ctrl+F10`: switch Assisted / Newtonian family.

These are diagnostic construction overrides only.

Production intent remains unchanged: later the physical family/layout state
must come from the ship descriptor/construction definition rather than keyboard
selection.

## Current runtime physical-switch state

Runtime switching is wired through authoritative ship state and changes real
physics, not only presentation:

- construction model changes;
- actuator topology is rebuilt;
- native flight-control law follows the selected family;
- capability is rebuilt from the selected model and actuator slots;
- execution gateway receives the same runtime model and slot state;
- HUD reads the replicated runtime model.

The diagnostic override deliberately creates a healthy synthetic actuator
topology for the selected construction; damage from the original descriptor
topology is not transferred across diagnostic model changes.

## Exit condition for this temporary Assisted phase

Assisted finalization is complete only when:

1. the three currently red navigation-runtime tests are understood;
2. every Assisted defect is fixed without changing accepted physics;
3. the full `navigation_runtime` suite is green, or any intentionally unrelated
   failure is explicitly documented and isolated;
4. the accepted Assisted stress proof still passes;
5. a new recovery point is created for the fully-green Assisted baseline.

Only then resume Newtonian V1 work from
`docs/navigation/NEWTONIAN_V1_PLANNING_CHARTER_2026-10-15.md`.
