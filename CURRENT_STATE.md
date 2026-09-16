# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted locally  
**Navigation:** `NAV-V2-MAP-2` — isolated ship-centered NavigationMap implemented; CPU benchmark ready for MinGW measurement

## Stable baseline outside navigation

Runtime model ingress remains:

```text
legacy OBJ -> AssemblyMeshLibrary -> LegacyAssemblyModelAdapter -> ModelAsset
.elmodel   -> CompiledModelAssetReader -> ModelAssetBinary       -> ModelAsset
```

`src/model_asset/ModelAsset.h` remains shared runtime/editor schema authority.
Renderer feature work remains paused while Navigation v2 is established.

## Navigation architecture reset

The legacy route-wide synchronous chain is migration code, not the new foundation:

```text
GeometricPathPlanner
    -> route-wide trajectory materialization
    -> dense obstacle validation
    -> GuidanceTunnel
```

Ruckig remains only a possible local kinematic primitive after routing/avoidance
chooses a target state. It is not free-space/path-search authority.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

## Coordinate domains

Navigation v2 intentionally preserves separate coordinate domains:

- authoritative long-lived state remains in precise system/world coordinates;
- the active dynamic navigation working set is ship-centered;
- its working origin may translate/rebase with the active ship/domain;
- working axes are stable navigation/travel axes, not instantaneous hull attitude;
- Hub retains Hub-local authored geometry, docks and scheduled bots;
- only the Hub subset relevant to the active ship is transformed/published into
  the active NavigationWorld;
- hull-local coordinates remain flight/controller detail;
- render/player-relative coordinates remain presentation-only.

## NavigationMap ownership boundary

New block:

```text
src/world/navigation/map/
    NavigationMap.h
    NavigationMap.cpp
    CMakeLists.txt
    README.md
```

Public ingress is an owned `DynamicWorldUpdate` by value containing the working
frame, source revision and actor P/V/A/radius/flags/revision data.

The block transforms and owns all resulting state internally. Actor tables,
prediction caches, sparse spatial cells and future CPU/GPU backend resources do
not cross the boundary. `NavigationMap.h` deliberately has no dependency on GLM,
OpenGL, GLFW, scene, game state or renderer code and uses PImpl.

Public egress is only compact derived data by value:

```text
queryCorridor()
querySphere()
stats()
```

The CPU reference backend currently implements constant-acceleration endpoint
prediction, conservative swept spheres and a sparse 3D cell hash. It is the
behavior oracle for any future GPU backend.

Acceptance coverage:

```text
tests/navigation_map/NavigationMapContractTests.cpp
tests/navigation_map/run_mingw64.sh
tests/architecture_contracts/check_navigation_map_boundary.py
```

The tests cover ownership, large-coordinate rebasing, stable basis conversion,
dynamic corridor filtering, sparse local query behavior, explicit rejected/out-
of-bounds inputs and atomic rejection of invalid frame publication.

## CPU benchmark ready

New isolated benchmark:

```text
benchmarks/navigation_map/
    CMakeLists.txt
    main.cpp
    run_mingw64.sh
    README.md
```

It consumes only the public NavigationMap API. It mirrors the existing GPU
prototype scenario classes at 1k/5k/10k actors for `cruise` and `hub` and measures:

```text
snapshot publication/rebuild median + p95
corridor query median + p95
sphere/local query median + p95
candidate counts
cells visited / occupied cells / actors examined
indexed / rejected / out-of-bounds counts
```

Run:

```bash
bash benchmarks/navigation_map/run_mingw64.sh
```

The resulting CSV is the next evidence needed before selecting CPU-only or a GPU
backend.

## Existing GPU evidence

`benchmarks/navigation_gpu/` remains an isolated OpenGL 4.3 compute prototype.
It measures deterministic P/V/A prediction, spatial binning, corridor filtering
and all-agent candidate queries for the same broad 1k/5k/10k scenario classes.
It does not define a separate runtime API. If GPU is selected, it must fit behind
`NavigationMap`.

## Navigation / collision / damage boundary

These remain separate authorities even where broadphase data is shared:

```text
Navigation
    conservative envelopes / clearance / predicted conflicts

Physics / Collision
    candidate pairs -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local navigation invalidation
```

Render, collision, hit/damage and navigation geometry need not match. A breach is
navigable only when clearance admits the requesting agent envelope.

## Performance targets

```text
main-thread navigation CPU       < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred, < 2.0 ms heavy-scene target
full/precision route solve       asynchronous; never a frame-thread blocker
```

These are design targets, not cross-machine test assertions.

## Next step

Measure the CPU benchmark on the user's MinGW64 machine, compare with the existing
GPU compute benchmark, then choose the dynamic backend without changing the public
NavigationMap API. After that begin `NAV-V2-SPACE-1` for static free-space,
clearance, connectivity/portals and local invalidation inside the same ownership
boundary.
