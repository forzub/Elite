# Elite — CURRENT STATE

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Model Asset Editor architecture:** closed at the current target boundary  
**ModelAsset binary v4 architecture:** independent translation units closed  
**Game runtime decomposition:** R0 seams + dual-source model ingress accepted  
**Renderer baseline:** OpenGL 4.3 Core accepted locally  
**GPU-P0:** System Map static textured spheres accepted locally  
**GPU-P0.1:** repeated planar System Map circles/orbits accepted locally  
**Navigation:** NAV-RUCKIG-0 accepted; NAV-LIVE-3 active

## Accepted runtime baseline

`EliteNavigationGeometry` and `EliteAssemblyGeometry` remain the shared
runtime/deterministic geometry seams.

Dual-source model ingress remains accepted:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains the single schema/version authority.
Runtime-model consumer migration is queued behind current navigation/performance
work.

## Renderer status

OpenGL 4.3 Core, GPU-P0 resident System Map textured spheres and GPU-P0.1
instanced planar circles/orbits are accepted locally. The renderer wave is
paused.

The current freeze is measured inside client docking/navigation work, not scene
submission. The older station-adjacent renderer issue remains a separate deferred
problem.

## Live docking path

```text
SpaceState::updateDockingGuidance()
    -> ClientNavigationPlanningSnapshotFactory
    -> DockingPathPlanner::plan()
    -> world::navigation::TrajectoryGenerator::generate()
    -> GuidanceTunnelBuilder::build()
    -> GuidanceState publication
```

The initial solve and rolling reconnect are still synchronous on the client
update thread.

## Measured 19-obstacle baseline

Latest user runtime evidence:

```text
[DockingPerf]
total_ms      ~= 470.3
snapshot_ms   ~=   0.41
geometric_ms  ~=   7.42
trajectory_ms ~= 373.3
tunnel_ms     ~=  15.5
```

The route snapshot correctly contained 19 obstacles, but the first stress layout
was visually poor: objects clustered around the Hub without forcing the baseline
route to cross them.

`navigation_perf.log` resolved the CPU ownership:

- initial `SmoothPathOptimizer`: about 367 ms;
- spline sampling: roughly 7..8 ms per support-level candidate;
- collision/safety validation: roughly 39..60 ms per candidate;
- selected trajectory remained support level 2 with 3862 samples.

Therefore collision narrow-phase repetition, not spline evaluation, is the main
cost of the 19-obstacle initial trajectory.

## Rolling replan storm

The same capture contained 274 smoother calls:

```text
1 initial call
195 rolling calls with 10 control-path points
78 rolling calls with 12 control-path points
```

The rolling structure is exactly 39 `GuidanceTunnel` rebuild attempts × 7
candidate curve families. Those rolling calls consumed about 14.4 seconds of
smoother CPU in the short run.

The old 0.25 s rolling interval was shorter than one ~0.37 s reconnect attempt,
so the synchronous client could become due for another heavy solve immediately.
The hard-envelope branch could also bypass the cadence timer completely. This
explains the observed near-total loss of interactive motion.

## NAV-LIVE-3 implementation

### Conservative obstacle broadphase

`src/world/navigation/NavigationObstacleGeometry.cpp` now rejects obvious misses
with an enclosing-sphere broadphase before running the exact inflated OBB,
capsule, or sphere segment test.

The broadphase encloses the exact inflated shape, so a miss is provably safe to
skip and a hit still executes the previous narrow phase. Collision radius,
clearance and exact final collision semantics are unchanged.

### Synchronous reconnect containment

Until reconnect moves off the frame thread:

- rolling checks are capped at 1 Hz;
- the immediate hard-envelope heavy-solve bypass is disabled;
- normal pose, predicted-exit, course, attitude and target-motion checks remain
  on the throttled policy.

This is a temporary client-stall guard, not the intended final architecture.
Final target remains worker/latest-request-wins publication.

### Route-crossing stress field

The 16 diagnostic obstacles remain real Hub-attached `StaticObject`s and therefore
use normal render/snapshot/navigation paths. Their layout is now four deterministic
bands across the baseline player-to-cube-A route rather than a harmless cloud near
the Hub. The two authored docking targets remain unchanged.

### Navigation performance log

`SmoothPathOptimizer` still writes detailed phase data to
`navigation_perf.log`. Every process that links the smoother now prints its exact
absolute path at startup:

```text
[NavigationPerf] log_path=...
```

This resolves the previous ambiguity where tests and `EliteGame` could create
same-named files in different working directories.

## Navigation architecture baseline

`src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md` remains authoritative:

```text
global coarse route
    -> bounded detailed local motion
    -> per-physics-tick execution
    -> cheap guidance-tunnel presentation
```

Collision fidelity remains non-negotiable. Optimization order is broadphase /
spatial indexing / bounded work / scheduling, not looser collision geometry.

## Current acceptance gate

Run under MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only
unset ELITE_TRACE_RUNTIME

python tests/architecture_contracts/check_navigation_stress_field.py
python tests/architecture_contracts/check_live_docking_guidance.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
D:/__elite/work/build/EliteGame.exe
```

Verify the route-crossing obstacle layout, record the startup navigation log
path, calculate a route, and test several seconds of manual flight. Compare new
`safety_ms`, `trajectory_ms`, rolling `dock_ms`, and `tunnel_builds` against the
19-obstacle baseline above.
