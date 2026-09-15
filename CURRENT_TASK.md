# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** route/trajectory performance  
**Stage:** NAV-RUCKIG-0 — isolated Ruckig state-to-state trajectory spike

## Why this task is active

The route/trajectory calculation can stall the machine. The current local planner generates state-to-state legs with the sequential `TrajectoryPredictor` and can repeat a complete prediction up to six times for shooting correction. Docking, detour and emergency branches can multiply those solves.

The user explicitly chose to try Ruckig Community Edition before doing a direct GPU rewrite of that sequential integrator.

## Dependency contract

Use only the reviewed MIT-licensed Community Edition source:

```text
Ruckig v0.19.4
commit a8db97a4e9c55e5160a3855f739fa3b270df8e4c
```

For this spike:

- fetch the exact commit;
- `BUILD_CLOUD_CLIENT=OFF`;
- examples/tests/benchmark/Python/shared build surfaces OFF;
- keep upstream C++20 private to the Ruckig adapter target;
- do not raise the rest of Elite above C++17;
- do not use Ruckig Pro or cloud waypoint functionality.

License text is stored at `src/assets/licenses/RUCKIG-MIT.txt`; provenance is in `THIRD_PARTY_LICENSES.md`.

## Implemented spike boundary

Added:

- `src/game/navigation/RuckigTrajectorySolver.h`;
- `src/game/navigation/RuckigTrajectorySolver.cpp`;
- `tests/navigation_ruckig/CMakeLists.txt`;
- `tests/navigation_ruckig/RuckigTrajectorySolverTests.cpp`;
- `tests/navigation_ruckig/run_mingw64.sh`;
- `tests/architecture_contracts/check_ruckig_navigation_spike.py`.

The production `LocalGuidancePlanner` and main `EliteGame` build are intentionally untouched in this wave.

## Solver contract under test

The adapter uses an accelerating co-moving terminal frame rather than feeding orbital-scale world coordinates directly to Ruckig. Elite gravity is sampled at the start and target, a reference gravity acceleration is folded into the frame, and Ruckig solves the remaining relative 3-DOF motion.

After generation the adapter reconstructs world-space states and validates the candidate against Elite's scalar proper-acceleration and proper-jerk limits using fresh `GravityFieldSystem` samples. The result is returned as `TrajectoryPredictionResult` so later planner integration does not need to adopt Ruckig types.

Ruckig is only a candidate **leg generator**. It does not replace:

- `TrajectorySafetyEvaluator`;
- obstacle/restricted-volume/traffic checks;
- docking terminal policy;
- route selection;
- ship control/authority.

## Latest local result

The architecture contract passed. The first MinGW build then failed inside pinned upstream Ruckig before our solver linked because `ruckig/roots.hpp` uses `M_PI`, while strict MinGW `-std=c++20` hides that non-standard macro.

This is now fixed in the isolated spike CMake with a target-local MinGW portability shim:

```cmake
if(MINGW)
    target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)
endif()
```

The guard test now requires that shim so the issue cannot silently return. No solver correctness or performance conclusion should be drawn from the failed build; execution never reached the tests.

## Local acceptance now

Pull the fix and rerun:

```bash
git fetch origin
git pull --ff-only

python tests/architecture_contracts/check_ruckig_navigation_spike.py
bash tests/navigation_ruckig/run_mingw64.sh
```

The existing FetchContent checkout/build directory may be reused; no manual deletion should be necessary because CMake will regenerate the target compile definitions.

Expected functional coverage:

- stationary 100 m transfer reaches exact requested position/velocity;
- orbital-scale coordinates remain numerically stable through the co-moving frame;
- Earth-like gravity case preserves endpoint and gravity diagnostics;
- physically infeasible one-second transfer is rejected;
- benchmark prints `RUCKIG BENCHMARK: ... avg=... us/solve`;
- final line: `NAVIGATION RUCKIG SPIKE: PASS`.

Do not impose a hard benchmark threshold yet. Record the actual average solve time on the developer machine first.

## If the spike passes

Next wave: NAV-RUCKIG-1 production A/B integration.

1. Add a private C++20 Ruckig navigation library to the main CMake graph while leaving the application C++17.
2. Make local state-to-state `predictLeg()` Ruckig-first.
3. Keep the existing shooting predictor as deterministic fallback/reference.
4. Keep safety evaluation unchanged.
5. Add explicit timing/fallback counters.
6. Build `EliteGame` and reproduce the previously expensive route calculation.

Do not delete the old predictor until runtime A/B evidence shows that Ruckig covers the required cases.

## Deferred renderer acceptance

GPU-P0 static spheres remain accepted. GPU-P0.1 repeated System Map circles still has contracts PASS but lacks the final clean rebuild/runtime visual acceptance after Core-tail cleanup. That status is preserved; starting NAV-RUCKIG-0 does not retroactively accept GPU-P0.1.

Station-adjacent renderer freezes remain out of scope.
