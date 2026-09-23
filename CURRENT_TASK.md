# CURRENT TASK — bounded physical maneuver search coordinator

Date: 2026-09-23

Status: **IMPLEMENTED LOCALLY — TARGET BUILD/TEST REQUIRED**

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

## Focused evidence required

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

## Next slice after acceptance

Add literal actuator phases and consistent rigid-body propagation, including
non-zero initial angular velocity. Then introduce continuous proof over the
exact candidate. Only after both layers may the replacement publish an
`AcceptedManeuverProgram` and displace the legacy translation-first author.
