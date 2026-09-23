# CURRENT TASK — observer-only visualization of the replacement path

Date: 2026-09-23

Status: **VISUAL DATAFLOW CANDIDATE READY — TARGET BUILD/INSPECTION NEXT**

## Implemented observer candidate

The runtime now builds one observer-only initial receding-horizon frontier from
explicit API inputs. Four ranked alternatives vary arrival/program horizon;
the coordinator exposes attempts, witnesses and any unproved physical candidate
samples through `TracePhysicalSearch`.

The trace JSON and viewer preserve four separate meanings:

- legacy retained route/Ruckig reference;
- rejected physical alternatives;
- unproved B5 physical candidate set with attitude and acceleration vectors;
- accepted reference/actual motion from the still-legacy execution chain.

There is deliberately no B5-to-B8 conversion and no physical observer input to
Follower or physics.

## Active gate

Build `navigation_runtime_pipeline_tests` and `navigation_runtime_viewer` on the
MinGW target, run the pipeline and open the viewer. Inspect at least straight,
corner and high-speed Newtonian starts. Confirm red rejection markers, separate
cyan/yellow candidate curves/vectors, and the window-title marker
`PHYS-OBS=... (НЕ ПРИНЯТО)`.

The legacy high-speed terminal failure remains expected until the replacement
path gains continuous proof and execution authority. Do not weaken that test.

## Accepted coordinator evidence

Exact commit `b506397ca30f223ee6cb29597c8673e0815626d1` passed
`physical_maneuver_search_coordinator` on MinGW64 (1/1, 0.04 s). Combined with
the earlier exact `9af337c` physical compiler/limit pass, the pure compiler and
bounded-search contracts are accepted.

This is not acceptance of the integrated navigation system.

## Implemented visual slice contract

The new physical search is connected to `tools/navigation_runtime` in
observer-only mode. It does not steer the ship or publish
`AcceptedManeuverProgram`.

Required viewer layers:

- legacy coarse route and legacy Ruckig reference, clearly labeled;
- ranked physical alternatives and their provenance;
- rejected alternatives with typed witness/reason;
- selected unproved physical candidate samples;
- candidate attitude axis and planned acceleration/thrust phase;
- later, proved swept tunnel, accepted reference and actual path as distinct
  products.

The observer adapter owns conversion from scenario/vehicle/runtime values into
the pure coordinator request. The coordinator and compiler remain free of
viewer, JSON, filesystem and OpenGL dependencies. All behavior-affecting
observer inputs must come through explicit policy/API values.

Exit gate:

- viewer builds and displays the new path independently of legacy execution;
- an unproved candidate cannot be mistaken for a proved or accepted program;
- visual inspection is performed on straight, corner and high-speed Newtonian
  cases before the replacement is allowed to control physics.

## Historical first target result

Exact commit `3492ca3ba314dcf250c5d3ebc03c6e8cc0c3dce6` configured and
compiled all three targets. The existing two tests passed; the new coordinator
test failed because its allegedly feasible second alternative used a 4.0 s
horizon for a 135-degree rotation whose computed attitude-acquisition lower
bound is about 4.60 s.

Correction: use a 6.0 s second horizon so the test actually contains a physical
burn window. The coordinator correctly rejected the original pair; no planner
logic, timeout or authority limit is relaxed.

## Accepted prerequisite

Exact MinGW64 commit
`9af337c2e23a32d5f11d34a3e048ecd98842674d` passed:

- `maneuver_chained_limit_matrix`;
- `ordinary_physical_maneuver_compiler`.

Result: 2/2 tests passed in 0.12 s. The typed physical infeasibility witness is
accepted. The aggregate high-speed runtime remains intentionally unresolved
until the replacement path reaches accepted-program publication.

## Implemented slice

`PhysicalManeuverSearchCoordinator` is a pure bounded search layer above
`OrdinaryPhysicalManeuverCompiler`.

Input ownership:

- mission/goal/route code owns the ranked alternative frontier;
- every alternative carries corridor, terminal, speed-schedule and
  arrival-time provenance IDs;
- the common query owns measured state, vehicle capability, control law,
  reserves and compiler policy;
- an alternative may vary only target position, desired velocity and local
  program horizon;
- an explicit policy owns the maximum attempts per worker slice;
- independent objective/frontier revisions and a cursor preserve progress
  between slices without turning a rebuilt frontier into a new mission goal.

Output states:

- `CandidateFound` — one alternative produced unproved physical candidates;
- `SearchPending` — budget ended and the cursor can resume later;
- `FrontierExhausted` — caller must produce another frontier or proved safe
  fallback, while the objective remains active;
- `SharedStateBlocked` — changing terminal alternatives cannot repair the
  shared input/law/state problem;
- `InvalidInput` — revision/frontier/policy API data is invalid.

No state accepts a maneuver, changes ship capability or disables navigation.

## Accepted focused evidence

Build and run:

```bash
cmake --build build/tests/navigation_runtime \
  --target physical_maneuver_search_coordinator_tests \
           ordinary_physical_maneuver_compiler_tests \
           maneuver_chained_limit_matrix_tests && \
ctest --test-dir build/tests/navigation_runtime \
  -R "^(physical_maneuver_search_coordinator|ordinary_physical_maneuver_compiler|maneuver_chained_limit_matrix)$" \
  --output-on-failure
```

Required behavior:

- a short-horizon rejection advances to a later feasible alternative;
- a one-attempt budget returns `SearchPending` and resume does not repeat work;
- exhausted alternatives preserve witnesses and objective ownership;
- shared control-law/state blockers do not waste the remaining frontier;
- stale objective or frontier revisions fail before physical compilation.

## Work after visual acceptance

Add literal actuator phases and consistent rigid-body propagation, including
non-zero initial angular velocity. Then introduce continuous proof over the
exact candidate. Only after both layers may the replacement publish an
`AcceptedManeuverProgram` and displace the legacy translation-first author.
