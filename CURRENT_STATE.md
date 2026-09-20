# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target-machine baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Accepted evidence:
- Stage-12 architecture contract PASS;
- navigation_runtime 18/18 PASS;
- chained transition + physical-limit matrix PASS.

## Latest actually tested checkout

```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Target evidence:
- architecture PASS;
- compile/link PASS;
- 17/19 runtime tests PASS;
- failures in runtime planner and final composite.

## Current unverified code baseline before state-sync commits

```
70dc7bb9c024a388c39b79d54a12774475ca8b32
```

## B4 contract corrected after first target failure

The former same-horizon two-segment requirement is rejected.

Canonical ordinary local behavior is now:

```text
accepted route / trajectory
    -> physical visible horizon
    -> predicted dynamic/static occupancy
    -> can a safe executable short segment avoid the obstacle?
         yes -> accept that short off-route segment and keep moving
         no  -> issue active braking intent and keep navigation alive
    -> next receding-horizon update
         -> continue bypass while required
         -> begin reacquiring nominal line when safe
         -> converge back under physical steering/acceleration limits
```

There is **no arbitrary distance at which the ship must return to the route**.
In particular, no 30 m merge requirement exists.

The on-route `mergeTargetMapMeters` / runtime mirror is currently only a
reacquisition reference. It is not a mandatory endpoint of the current local
segment and is not required to be reachable inside the same horizon.

## Navigation continuity on failure

`ConflictHold` does not switch navigation off.

Runtime behavior is command-producing:

```text
no safe current bypass
    -> ConflictHold
    -> holdIntent
    -> negative velocity demand / braking
    -> physics continues
    -> navigation planner remains active
    -> fresh world state is evaluated again
```

This matches the required behavior: if there is time/space to evade, evade; if
there is not, brake and use the space that remains while continuing navigation.

## Current bypass selection

For equal lateral clearance the local solver now prefers the farther forward
bounded bypass station. This reduces gratuitous sideways kinks and better
preserves the route direction.

The selected short segment is proved against:
- exact static geometry;
- projected moving occupancy;
- time-coupled dynamic separation.

Physical maneuver authoring/capability proof remains downstream ownership.

## Test corrections

The stale portal-attitude fixture was corrected:
- start moved to X=0;
- blocker moved to X=3.5;
- staging/reacquisition reference remains X=7.

This makes both endpoints outside the 2.75 m required dynamic separation while
the nominal path is still genuinely blocked.

The old regression `testReturnLegIsAlsoProvenAgainstExactStaticGeometry()` was removed.
It is replaced by `testBypassDoesNotRequireImmediateReturnToTrajectory()`, which
deliberately blocks an immediate return leg while leaving a valid short bypass available.

The no-space regression remains and requires:
- `ConflictHold`;
- `localBypassExhausted`;
- active emergency braking intent.

## Validation status

**UNVERIFIED on target MinGW64.**

The next run must verify the new receding-horizon contract. Do not promote B4
until the target gate is green and the final composite demonstrates physically
reasonable behavior.

## Documentation protocol

After every state-affecting event update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.

## Composite physical fallback

The final composite no longer treats `fit.valid == false` as an automatic
test failure. If the geometric B4 segment cannot be authored within the
vehicle's current physical authority, the test now executes active braking
through the real PilotSkill/physics path, keeps the hazard authoritative,
and then continues the receding-horizon replan loop.

This directly pins the required rule:
- can evade physically -> execute bypass;
- cannot evade physically -> brake;
- navigation ownership remains active in both cases.

## 2026-09-20 target run `navigation_test_20260920-182430.txt`

Important: the uploaded log does **not** contain the tested HEAD line, so the
exact checkout must not be inferred from the filename or current repository HEAD.

Observed evidence:
- Stage-12 architecture contract PASS;
- compile/link PASS;
- 17/19 runtime tests PASS;
- `navigation_runtime_planner` FAIL:
  `fixture must retain the future oriented portal as route context`;
- `navigation_composite_proving_ground` FAIL:
  `composite dynamic clearance lost for newtonian`.

### Planner fixture interpretation

The previous fixture repair moved the agent to X=0, exactly onto the minimum X
boundary of region 1 in `orientedPortalCaptureSpace()`. The new failure happens
before the actual bypass assertion: the test no longer retains the oriented portal
as route context. This strongly indicates the fixture was repaired in the wrong
place rather than a B4 regression. Use an interior start while preserving >2.75 m
separation from both start and staging endpoint (for example start X=1, blocker X=4, staging X=7).

### Composite interpretation

The B4 behavior itself improved materially:
- first solve: `AdjustedClear`, 29.38 m lateral offset, 22.5 m forward;
- physical replacement authored successfully;
- actual first replacement kept +4.747 m dynamic clearance and zero tracking violations;
- next receding-horizon solve again returned `AdjustedClear`;
- continuation kept +22.821 m actual dynamic clearance and zero tracking violations;
- next solve returned `NominalClear`.

The failure occurs **after** that successful B4 sequence. The composite then leaves
the receding-horizon planner loop and executes a fixed 10 s narrow-portal program
and then final capture while the dynamic hazard remains active. That violates the
new requirement that navigation/monitoring must remain active. A bounded segment
being nominal-clear does not prove the entire following scripted portal leg is clear.

Therefore the next fix should keep planner/monitor/replan ownership active through
the resumed topology leg instead of treating `NominalClear` as permission to run an
unmonitored long scripted phase.

### Visualisation

A visual trace is now justified. The useful first visualization should plot, in the
same 2D/3D scene, ship path, hazard path and inflated safety envelope, selected B4
targets, bounded nominal/reacquisition references, portal center, and replan points.

## 2026-09-20 navigation runtime 3D viewer

Implemented an independent diagnostic viewer under `tools/navigation_runtime/`.

Files:
- `tools/navigation_runtime/NavigationTrace.h/.cpp` — shared deterministic JSON trace schema/IO;
- `tools/navigation_runtime/NavigationRuntimeViewer.cpp` — standalone GLFW/GLAD/GLM OpenGL 3D viewer;
- `tools/navigation_runtime/CMakeLists.txt` — isolated viewer build;
- `tools/navigation_runtime/run_mingw64.sh` — build/run helper;
- `tools/navigation_runtime/README.md` — controls and workflow.

The composite proving-ground test now writes the actual simulated run to
`tools/navigation_runtime/last_trace_<law>.json`. The writer is RAII-backed,
so the trace is retained when a later composite assertion throws.

Recorded data includes ship P/V/forward, dynamic-hazard P/radius, hull collision
envelope, planner safety envelope, actual dynamic clearance, phase/status,
selected B4 target, reacquisition reference, portal target and replan events.

Viewer presentation:
- topology route as a polyline;
- route turn/portal points as markers;
- actual ship trajectory;
- ship as an oriented wire rectangular box using Cobra half extents;
- explicit nose arrow;
- hazard trajectory and three wire envelopes;
- selected target / reacquisition / portal markers;
- all replan positions;
- current time/phase/status/clearance in the window title.

Controls: RMB orbit, MMB pan, wheel zoom, F fit, Space play/pause,
`[`/`]` frame step, R next replan, Esc close.

Validation state: source is committed but has NOT yet been compiled/run on the
target MinGW64 machine. Do not call the viewer accepted until that gate runs.

## 2026-09-20 target run on `c8972319390622a6b825bf0fa73e8a563c9d7068`

Verified on target MinGW64:
- navigation runtime configured and linked;
- 17/19 tests PASS;
- planner fixture still failed at future oriented portal route-context assertion;
- composite emitted `tools/navigation_runtime/last_trace_newtonian.json` with 770 frames;
- composite B4 sequence remained the same: two physically executed `AdjustedClear`
  segments, then `NominalClear`, then later dynamic-clearance failure;
- standalone viewer configure succeeded, but compile failed because the GLAD
  include root was wrong: source includes `<glad/gl.h>`, actual file is
  `glad/include/glad/gl.h`.

Fixes committed after that verified run:
- viewer CMake now includes `${ELITE_SOURCE_ROOT}/glad/include`;
- oriented-portal fixture now uses interior geometry start X=1, blocker X=4,
  stage X=7 while leaving global `baseAgent()` at its original X=0;
- these post-run fixes are UNVERIFIED until the next target run.
