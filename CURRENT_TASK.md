# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** route/trajectory performance  
**Stage:** NAV-RUCKIG-1 — production Ruckig-first A/B integration

## Accepted prerequisite

NAV-RUCKIG-0 isolated MinGW spike is accepted locally.

The complete spike test now passes after two integration fixes:

- MinGW strict-C++20 `M_PI` compatibility is applied only to upstream Ruckig;
- Elite scalar acceleration/jerk limits are conservatively mapped to Ruckig per-axis bounds and then revalidated against the authoritative scalar envelope.

The user confirmed the full test passed. The benchmark number itself was not pasted into chat, so do not invent or record a speed figure yet.

## Dependency contract

Use only the reviewed MIT-licensed Community Edition source:

```text
Ruckig v0.19.4
commit a8db97a4e9c55e5160a3855f739fa3b270df8e4c
```

Production build setup now lives in `cmake/EliteRuckigNavigation.cmake` and is shared by client, server and navigation-guidance tests.

Requirements remain:

- exact pinned commit;
- `BUILD_CLOUD_CLIENT=OFF`;
- examples/upstream tests/benchmark/Python/shared build surfaces OFF;
- upstream generic cache options restored after Ruckig configuration;
- Ruckig/adapter C++20 private to their targets;
- `EliteGame` and `EliteServer` remain C++17;
- no Ruckig Pro or cloud waypoint functionality.

## Implemented NAV-RUCKIG-1 candidate

`LocalGuidancePlanner::predictLeg()` now tries the accepted `RuckigTrajectorySolver` first for every local state-to-state leg.

If Ruckig rejects a leg or fails numerically, the previous deterministic shooting path remains intact and runs as fallback. That fallback still uses up to six `TrajectoryPredictor` calls, preserving the old reference behavior during A/B acceptance.

Safety and policy are unchanged:

- `TrajectorySafetyEvaluator` still validates every selected candidate;
- obstacle/restricted-volume/scheduled-traffic logic is unchanged;
- docking terminal-state policy is unchanged;
- detour/emergency selection is unchanged;
- no ship-control or authoritative state ownership moved into Ruckig.

`LocalGuidanceBackendDiagnostics` now reports per planning call:

- `ruckigLegAttempts`;
- `ruckigLegSuccesses`;
- `ruckigFallbacks`;
- `legacyPredictorCalls`;
- `ruckigSolveMicroseconds`;
- `legacyFallbackMicroseconds`;
- `lastRuckigFailure`.

The production client/server both link the private `EliteNavigationRuckig` library. The existing navigation-guidance regression suite is wired to the same dependency seam.

## Acceptance now

Pull and run:

```bash
git fetch origin
git pull --ff-only

python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py

bash tests/navigation_ruckig/run_mingw64.sh
bash tests/navigation_guidance/run_mingw64.sh

cmake --build build --target EliteGame
```

The main build may re-run CMake and fetch the pinned Ruckig source into the main build tree the first time.

### Acceptance criteria

1. Both architecture contracts PASS.
2. Isolated Ruckig suite still ends with `NAVIGATION RUCKIG SPIKE: PASS`.
3. Existing navigation-guidance regression suite remains green; Ruckig-first must not alter accepted docking/detour/emergency semantics.
4. `EliteGame` builds successfully without raising the application-wide C++ standard above C++17.
5. Runtime route calculation no longer produces the previous machine-scale stall in the representative bad case, or the diagnostics clearly identify why fallback is still dominating.

Do not remove the legacy predictor during this wave.

## Runtime evidence needed after build

Reproduce the same route calculation that used to be expensive and capture the resulting navigation diagnostics. The important distinction is:

```text
ruckigLegAttempts ~= ruckigLegSuccesses
legacyPredictorCalls ~= 0
```

versus a fallback-heavy result such as:

```text
ruckigFallbacks > 0
legacyPredictorCalls >> 0
```

If Ruckig succeeds for normal legs and the freeze disappears, NAV-RUCKIG-1 can be accepted and the old shooting path can move toward reference-only status. If fallbacks dominate, use `lastRuckigFailure` plus timings to fix the actual unsupported case rather than deleting the fallback.

## Deferred renderer acceptance

GPU-P0 static spheres remain accepted. GPU-P0.1 repeated System Map circles still has contracts PASS but lacks the final clean rebuild/runtime visual acceptance after Core-tail cleanup. That status is preserved.

Station-adjacent renderer freezes remain out of scope.
