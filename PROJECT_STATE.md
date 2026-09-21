# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Evidence boundary

Accepted baseline:
```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest target-tested checkout:
```
81d0c23ae42bba0352026d6c2306cc6976c04bda
```

Current unverified code baseline before documentation sync:
```
abfd7a6a26168f177968f0dd4299f3c712105fc0
```

## B4 current architecture

Ordinary local avoidance is receding-horizon and command-continuous.

A local solve is responsible for the **next safe executable geometric segment**,
not for completing an artificial leave-route-and-return loop inside one horizon.

```text
nominal route
 -> bounded physical horizon
 -> projected/static conflict
 -> safe short offset segment if available
 -> execute
 -> refresh from actual state
 -> continue offset or begin route reacquisition
```

If no safe short offset exists, navigation remains active and returns braking
intent. It is not disabled and it does not surrender ownership.

The route is reacquired progressively. No fixed distance, including 30 m, is a
required merge point.

## Removed/superseded behavior

Still forbidden:
- angular deflection fan;
- azimuth branch search;
- persistent left/right branch continuity;
- branch-switch API;
- mandatory Brake-before-changing-side recovery.

Also superseded:
- mandatory same-horizon current->bypass->merge proof;
- treating the current bounded nominal endpoint as a compulsory return point.

## Current milestone

Target-machine validation of the corrected receding-horizon B4 behavior.

The final composite must distinguish:
- geometry can evade -> continue through proved local free space;
- geometry cannot evade -> braking command while navigation continues;
- physical compiler cannot execute geometric bypass -> higher maneuver
  ownership brakes/replans rather than accepting an impossible maneuver.

## Other block status

Accepted/strong:
- B0 snapshot ownership;
- B7 maneuver decision mechanics;
- B8 accepted program;
- B9 sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 propulsion/physics;
- B14 scheduler mechanics.

Still incomplete/transitional:
- B1 shared influence builder;
- B2 unified objective;
- B3 coarse vehicle-aware topology feasibility;
- B4 receding-horizon local bypass under target validation;
- B5 general production maneuver compiler;
- B6 generalized continuous proof;
- B11 explicit safety/reflex monitor;
- ordinary-live migration of accepted-program execution.

## State protocol

After every state/evidence change synchronize project state MDs, active Stage-12,
and recreate `CONTINUE_PROMPT.md` from scratch.

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

## 2026-09-20 viewer HUD iteration

Target feedback confirmed the standalone viewer now builds and opens, but the first
UI-less build was not self-explanatory: no visible buttons and no legend/state panel.

Viewer updated with an in-window diagnostic HUD:
- clickable PLAY/PAUSE, PREV, NEXT, NEXT REPLAN and FIT buttons;
- right-side live state panel with law/frame/time/phase/status/clearance;
- explicit `WHAT IS HAPPENING` explanation derived from trace phase/status;
- color legend for every diagnostic primitive;
- visible controls reminder;
- `portal_102` explicitly marked as the current known clearance-loss area.

The HUD uses a tiny built-in bitmap font and existing OpenGL primitives; no new UI
framework/dependency was introduced.

Validation state: previous viewer build/open is target-verified; the new HUD commit
still needs one target build/open check.

Same target run also showed planner fixture progress: the old route-context assertion
no longer fails; it now reaches `fixture must produce a safe adjusted target`. That
is a separate planner-fixture task and was not mixed into this HUD change.

## 2026-09-20 viewer physics/trace accuracy iteration

User clarified the viewer must remain an ordinary decorated Windows window, simply
maximized to the desktop work area; no exclusive/borderless game fullscreen.

Verified architecture of the current tool:
- composite test executes real Planner -> AcceptedManeuverProgram -> Follower ->
  runtime bridge -> SharedShipPhysics first;
- the viewer only replays the resulting JSON; it does not run planner/follower live;
- previous apparent low performance was primarily 0.10 s trace quantization (~10 Hz)
  with nearest-frame display, not evidence of a heavily loaded CPU.

Implemented, not yet target-verified:
- smooth interpolation between trace samples during playback;
- trace schema v2 records actual full ship basis forward/right/up (roll preserved);
- trace schema v2 records sampled AcceptedManeuverProgram reference P + full basis;
- viewer draws actual nose and program-reference nose separately and reports angular
  tracking error;
- viewer renders AcceptedManeuverProgram tracking.positionErrorMeters as a translucent
  tracking tube. This is explicitly NOT called a Planner volumetric corridor because
  NavigationRuntimePlanner::Result does not publish one;
- trace records hazard velocity + planner look-ahead;
- bottom-left `ГОРИЗОНТ КОБРЫ` inset projects the moving hazard and its predicted
  envelope tunnel onto Cobra's right/up plane perpendicular to actual forward;
- Russian HUD/Cyrillic bitmap support and Russian window title;
- interactive frame slider;
- ordinary GLFW window starts maximized.

Newtonian diagnostic correction:
- the composite test previously authored replacement/continuation programs with
  OrientationMode::VelocityAligned even under Newtonian law, making Newtonian motion
  visually look Assisted;
- Newtonian local bypass and the un-oriented portal-102 transit now preserve current
  body attitude (FixedStart); Assisted remains velocity-aligned;
- this change is unverified until the next target run.

Still pending and must not be faked in replay UI:
- pilot-skill selector (expert/average/loser);
- flight doctrine selector (standard/extreme);
- meaningful obstacle checkbox producing a different simulation, not merely hiding
  the obstacle;
- meaningful Assisted/Newtonian selector that runs/regenerates the scenario.
These require extracting/reusing a live scenario runner inside the tool or generating
distinct authoritative traces. Do not add cosmetic controls that leave physics unchanged.

## 2026-09-20 live navigation diagnostic stand

Architecture changed: `tools/navigation_runtime` is no longer a passive JSON trace
viewer. The JSON file is now an INPUT SCENARIO only. The executable calculates the
route and vehicle motion in-process when the user presses `РАССЧИТАТЬ`.

New production-chain runtime:
`scenario.json -> NavigationSpace/NavigationMap -> NavigationRuntimePlanner ->
AcceptedManeuverProgram -> TrajectoryFollower -> NavigationRuntimeControlBridge /
PilotSkillExecutor -> SharedShipPhysics / DynamicMotionSystem -> in-memory trace -> 3D`.

New files:
- `tools/navigation_runtime/NavigationScenarioRuntime.h`;
- `tools/navigation_runtime/NavigationScenarioRuntime.cpp`;
- `tools/navigation_runtime/scenario.json`.

Real UI inputs:
- control law: Assisted / Newtonian;
- pilot: Expert / Average / Loser;
- flight style: Standard / Extreme;
- sudden-obstacle checkbox;
- `РАССЧИТАТЬ` button.

Pilot choice changes the actual PilotSkillExecutor profile (reaction delay, decision
rate, latency, response, slew and deterministic command error). Flight style changes
real cruise speed and planner horizon/cost aggressiveness. Control law changes the
actual local flight law and maneuver attitude semantics.

Sudden obstacle semantics:
- unchecked: sudden obstacle is never published;
- checked: it is absent from initial NavigationMap and appears only after
  `activation_time_s` during the simulation;
- therefore the initial route has no foreknowledge of the surprise obstacle;
- after activation normal receding-horizon replanning reacts to it.

`scenario.json` supports:
- start P/V/forward/up;
- optional forced ship route points (default empty);
- final position;
- optional final forward/up constraints;
- final constant speed requirement;
- static sphere/box/capsule obstacles;
- moving obstacles defined by velocity vector OR route points + speed;
- sudden obstacle, including spawn relative to live ship forward/right/up.

The default scenario leaves `ship_route_points` empty so Planner must calculate the
route around the static obstacle rather than replay authored portal points.

Calculated white route now records the live sequence of Planner `selectedTarget`
decisions. Adjusted targets are marked as turn/bypass points. Actual motion remains
a separate rendered path.

Static obstacles from JSON are carried into the calculated TraceDocument and rendered
in 3D. `last_calculated_trace.json` is still saved for diagnostics, but is an OUTPUT,
not an input to the stand.

Build architecture:
`tools/navigation_runtime/CMakeLists.txt` now links the production navigation/control/
physics stack directly instead of being an OpenGL-only replay executable.

Validation state: this live-runtime iteration is committed but NOT YET compiled/run on
the target MinGW64 machine. Do not claim acceptance until the target gate passes.

## 2026-09-20 target video diagnosis — live stand integration bug

User supplied a screen recording of the first live-stand behavior.

Observed on-screen:
- playback around t≈41 s showed phase `ТОРМОЖЕНИЕ` and status `ДАННЫЕ УСТАРЕЛИ`;
- Cobra made almost no meaningful route progress;
- right diagnostic panel visibly jumped/blinked because conditional rows (especially
  the one-frame `СОБЫТИЕ: ПЕРЕПЛАНИРОВАНИЕ`) were inserted/removed from flow layout;
- the default no-sudden-obstacle run therefore did not demonstrate routing quality.

Root cause found in `NavigationScenarioRuntime.cpp`:
`NavigationRuntimePlanner::plan(... dynamicResultAgeSeconds ...)` was called with
`vehicle.timeSeconds` (absolute simulation time) instead of the age of the freshly
queried NavigationMap snapshot. With `maxResultAgeSeconds=0.25`, every plan after
0.25 s became `StaleHold`, correctly triggering fail-closed braking.

Fix committed:
- live stand now passes `0.0` for the just-created dynamic snapshot age;
- this is the correct semantics for the current synchronous query->plan call.

HUD fix committed:
- right panel uses hard fixed Y slots for mode/frame/time/phase/status/orientation/
  clearance/event/calculation result/explanation/legend/controls;
- optional data displays `-` instead of adding/removing rows;
- replan indication may still change color/text, but cannot move any other text;
- calculation result is always visible during playback so a partial failed trajectory
  is not silently presented as a successful solution.

Interpretation of previous daytime tests:
- they were not fake: they exercised real production planner/follower/control/physics
  components and found genuine local failures;
- however the composite fixture staged the scenario in manually authored phases and
  handcrafted AcceptedManeuverPrograms, so it did NOT prove a free-running arbitrary
  start->world->finish orchestration loop;
- the live stand exposed exactly that missing integration gate.

Important remaining architecture gap:
- current live stand still converts Planner output to a locally authored quintic
  `makeShortProgram`; it does not yet drive every nominal/replan segment through the
  full production B5/B6/B7/B8 physical compile/proof/selection/acceptance chain;
- therefore the next task is to replace that local adapter with the actual production
  maneuver compilation/proof chain before treating live-stand success as navigation
  acceptance evidence.

Validation state: snapshot-age and fixed-HUD patches are committed but not yet target
recompiled/replayed after this video.

## 2026-09-20 architecture correction — global corridor vs local dynamic avoidance

User clarified the intended ownership and this matches the existing LocalAvoidancePlanner contract:

- the nominal global route/corridor is built once from start to finish;
- it remains authoritative while the destination and static navigation world remain unchanged;
- moving obstacles do NOT trigger global route reconstruction;
- dynamic snapshots are consumed only by a bounded local monitor/avoidance layer;
- local avoidance may temporarily leave the nominal corridor, then progressively reacquire it;
- follower/autopilot continuously executes the current accepted trajectory/control program;
- the global planner is invoked again only when the goal changes or the static navigation world/corridor revision becomes invalid.

`maxResultAgeSeconds` / the ~0.25 s freshness limit is therefore a dynamic-snapshot safety contract, NOT a global replanning cadence.

Current live stand violates this architecture because it calls NavigationRuntimePlanner::plan repeatedly and therefore recomputes the static corridor every short execution slice.

Important current capability gap:
`NavigationSpace::queryCostedCorridor` currently returns a region/portal corridor and portal centers. Exact static NavigationObstacle geometry is used to prove/reject local segments, but the global corridor search does not yet synthesize a full geometric path around arbitrary exact obstacles inside one coarse region.

Therefore the default one-region + wall live scenario cannot honestly demonstrate the requested `start -> finish` global corridor yet. A proper global geometric corridor product must be added/cached first, then local dynamic avoidance must operate against that immutable nominal corridor.

## 2026-09-20 clarification — corridor is not the physical collision tunnel

User clarified terminology:
- `corridor` is a navigation/test abstraction around the nominal route used to ask
  whether the ship can generally proceed along that route;
- it is NOT the authoritative physical swept volume of the Cobra;
- exact wall contact / aperture passage must be decided by the future physical
  `tunnel` product: the time-parameterized swept volume of the actual hull along the
  accepted trajectory, including body orientation;
- therefore corridor visualization may be approximate and route-centric;
- tunnel proof is where it becomes critical whether Cobra's real hull clips walls.

Implication:
- do not over-engineer global corridor generation as exact hull clearance geometry;
- global planning may retain a centerline/polyline + coarse corridor for navigation;
- exact physical feasibility remains downstream ownership of trajectory/tunnel proof
  against exact static geometry and dynamic occupancy.

## 2026-09-20 full-chain evidence audit — why green slices did not mean green navigation

Re-audit performed after the live 3D stand exposed bad end-to-end behavior.

### Core conclusion

The previous tests were not fictitious: many of them exercised real production
Follower, PilotSkillExecutor, propulsion/physics, exact HitVolume geometry,
NavigationMap, NavigationSpace and parts of NavigationRuntimePlanner.

However, the evidence boundary was repeatedly over-interpreted. Most green tests
proved a component or a pre-authored slice, not the autonomous full chain:

`world/objective -> one retained global route -> local geometric response -> physical
maneuver generation -> continuous tunnel/proof -> decision -> ACCEPT -> follower ->
pilot -> physics -> event-driven monitor/replan -> finish`.

### What the main green test families actually proved

`ManeuverProgramExecutionLabTests`, `ManeuverCorridorMatrixTests`,
`ManeuverFlyThrough3dTests`, `ManeuverRigidBodyCorridorTests` and much of
`ManeuverChainedLimitMatrixTests` construct AcceptedManeuverProgram objects in the
test itself, then execute them through the real Follower/Bridge/Pilot/physics stack.
They prove execution/tracking of an already-good authored program. They do not prove
that production planning can generate that program from arbitrary world truth.

`NavigationRuntimePlannerTests` use deliberately authored region/portal fixtures. For
example the forced detour is already encoded by three regions and two known portals.
This validly proves topology/portal selection and local planner semantics, but not
automatic route synthesis around arbitrary raw obstacle geometry.

`OrdinaryPhysicalManeuverCompilerTests` genuinely test production B5 Newtonian
maneuver generation. They also explicitly pin that Assisted is unsupported in the
current first B5 slice, and candidates still require downstream continuous proof.
Therefore successful Assisted execution matrices were execution proofs of authored
programs, not proof of a production Assisted B5/B6/B7/B8 chain.

`NavigationCompositeProvingGroundTests` uses real planner/follower/physics components
but glues them together with test-owned helpers. Its topology is pre-authored through
portals; `makeProgram()` authors quintic programs; `buildDoctrineChoice()` authors
candidate programs/decision annotations; `fitAuthorityBoundedReplacement()` is a
test-side physical fitter. The file itself documents that production general B5
authoring remained a migration gap. The composite therefore was a proving ground,
not a production orchestrator.

The authoritative GameSimulation NavigationRuntimeLab was a real live test and remains
strong evidence for its narrow scope: real world objects/HitVolumes, real map/space,
real planner/control/physics and replication. But its slit/tunnel topology, approach,
entry/exit portals, start and goal are deterministic authored fixture data. It proves
that the live runtime can execute that authored topology/local situation; it does not
prove arbitrary `start + raw obstacles + finish -> full route/program` synthesis.

### Existing repository documentation already warned about this

`NAVIGATION_PIPELINE_AUDIT.md` explicitly says a unit test passing inside one stage is
not enough and records P6 ordinary maneuver generation / P7 proof integration / P8
ordinary decision integration / P9-P10 handoff as incomplete or transitional.

`NAVIGATION_V2_BLOCK_ARCHITECTURE.md` states B3 is event-driven, B5 current production
compiler is Newtonian-only, B6 proof is mandatory before ACCEPT, and the follower may
only execute the already accepted maneuver.

The project mistake was therefore not lack of warnings in code/docs; it was promoting
slice-level green evidence to a broader 'system works' interpretation.

### Why the new live stand looked dramatically worse

The new stand attempted, for the first time in this workflow, to synthesize much more
of the chain from a raw JSON scenario inside one executable. That immediately exposed
missing orchestration and also introduced new stand-side integration defects:
- absolute simulation time was mistakenly supplied as dynamic snapshot age, causing
  StaleHold/fail-closed braking;
- the stand periodically called the combined NavigationRuntimePlanner, incorrectly
  recomputing global/static planning instead of retaining one global route;
- the stand still uses its own `makeShortProgram()` quintic adapter, bypassing the
  complete production B5/B6/B7/B8 compile/proof/selection/acceptance chain;
- its default one-region + wall scenario asks current topology machinery to do a more
  general raw-obstacle route-synthesis job than the earlier authored-portal fixtures.

Thus the bad video is not evidence that Follower/PilotSkill/physics were fake. It is
evidence that the missing orchestration/handoff layers were never fully proved, and
that the newly written stand-side glue was itself incorrect.

### Evidence policy from now on

No collection of component/slice tests may be described as full navigation acceptance.
A true end-to-end acceptance must start from world/objective input and use the same
production products/handoffs as the game without test-authored maneuver programs or
test-only route/physics adapters.

Every test/result should be classified as one of:
- component/unit proof;
- execution of authored program;
- planner/topology slice;
- authoritative authored-world integration;
- true autonomous scenario end-to-end.

Only the last category may support a statement that the complete navigation chain
works for the scenario being tested.


## 2026-09-20 — Stage split implemented: Stage 1 static nominal route

The navigation diagnostic workflow is now explicitly split into two stages.

### Stage 1 — current implementation

One retained nominal route is built once from start to finish through static obstacle
geometry.

New production-facing component:
- `src/game/navigation/NominalRoutePlanner.h/.cpp`.

It wraps the existing `GeometricPathPlanner` backend behind an explicit v2 ownership
contract and returns a sparse polyline only. It accepts optional required authored
checkpoints and a coarse navigation envelope/clearance.

Nominal-route validity is revision driven:
- goal revision changed -> rebuild;
- static-world revision changed -> rebuild;
- dynamic-world revision changed -> **do not rebuild**.

The dynamic revision is deliberately present in `ValidityQuery` and deliberately
ignored by nominal-route invalidation. A regression test pins this behavior.

The live diagnostic stand now runs only Stage 1 on `РАССЧИТАТЬ`:

```text
JSON start/checkpoints/finish
 + static obstacles
 -> NominalRoutePlanner
 -> retained static route polyline
 -> viewer
```

It no longer runs the old half-second Planner/Follower/physics loop during Stage 1.
Therefore the two previously identified stand defects are removed from the canonical
Stage-1 path:
- no periodic global route replanning;
- no stand-local `makeShortProgram()` presented as production maneuver execution.

The ship remains at the start pose in Stage 1. The viewer shows the calculated route
and static obstacles only.

### Corridor/tunnel boundary

`route_envelope_radius_m` and `route_clearance_m` are coarse route/corridor inputs.
They are not exact physical collision proof. Exact Cobra wall/aperture contact belongs
to the later time-parameterized swept-hull tunnel proof.

### Dynamic-ready boundary

The scenario schema still accepts moving obstacles and a sudden obstacle. Stage 1 does
not use them for route reconstruction. Stage 2 will consume them as a local dynamic
overlay against the retained nominal route.

### Validation state

Code is committed but the new Stage-1 target has not yet been compiled/run on the
user's MinGW64 machine. Do not claim target acceptance until that run is supplied.


## 2026-09-20 Stage-1 cleanup lock

The Stage-1 diagnostic boundary was tightened after the initial implementation:
- old stand-local `makeShortProgram`, periodic planner loop, Follower/PilotSkill/
  SharedShipPhysics execution and dynamic publication code were removed from
  `NavigationScenarioRuntime.cpp` rather than merely left dormant;
- `tools/navigation_runtime/CMakeLists.txt` now links only the nominal-route/static
  geometry core for Stage 1;
- moving/sudden obstacle records remain parseable in the scenario schema as reserved
  Stage-2 inputs, but cannot influence the Stage-1 route;
- a new architecture checker pins that Stage 1 cannot regress into execution/replan
  ownership;
- target validation is still pending on the user's MinGW64 machine.


## Architecture-gate compatibility fix

During pre-handoff audit, the existing `check_geometric_path_planner.py` was found to
encode an obsolete ownership assumption: it required the same geometric planner .cpp
to appear twice in root CMake. Current runtime architecture already compiles it once in
shared `EliteNavigationGeometry`, which both client/server navigation runtime reuse.
The checker now pins that shared-library ownership instead of duplicate compilation.
This is an architecture-test correction only; route behavior is unchanged.


## 2026-09-21 target Stage-1 validation result

User target machine checkout: `5da0be0d05ef91958a0e7dc9adda3b4eb8fdee29`.

Observed:
- new `nominal_route_planner` test PASS;
- architecture suite stopped on an obsolete pre-System-frame marker in
  `check_navigation_live_runtime_control.py`;
- old Stage-2 `navigation_runtime_planner` still fails its adjusted-target fixture;
- old Stage-2 composite still fails at the narrow-passage full-hull check after a
  Newtonian dynamic bypass;
- all other 18/20 runtime tests passed.

Interpretation:
- Stage 1 static nominal route code itself passed its new behavioral test;
- the two red runtime tests are Stage-2 execution/local-dynamic regressions and must
  remain visible, but they do not invalidate Stage-1 static route acceptance;
- the architecture failure was not a behavior regression: the checker still expected
  `...Map...` control-field names while production had already migrated to explicit
  `...System...` names.

Fixes committed after this run:
- updated `check_navigation_live_runtime_control.py` to the current System-frame field
  names and `applySystemAccelerationDemand` API;
- labeled `nominal_route_planner` with CTest label `navigation_stage1`;
- added `tests/navigation_runtime/run_stage1_mingw64.sh` as the focused Stage-1 gate.

The focused gate runs only:
1. shared geometric-path architecture contract;
2. Stage-1 nominal-route architecture contract;
3. `nominal_route_planner` behavioral test;
4. Stage-1 viewer build.

This is not hiding the old red tests. It prevents Stage-2 failures from being
misreported as Stage-1 route-construction failures.


## 2026-09-21 — Stage-1 viewer observability fix after user screenshot

User screenshot showed a blank 3D area before pressing `РАССЧИТАТЬ`. After calculation the scene/route appeared, but there was no movement, making it impossible from the UI to tell whether Planner or Follower had failed.

Clarification: Stage 1 is route-only by design; Follower is not linked or executed. The UI was misleading because legacy playback controls remained visible as ordinary controls and the pre-calculation scene had no sufficiently explicit authored-scene rendering contract.

Fixes now committed:
- `TraceDocument` carries explicit authored `sceneStartMapMeters` / `sceneFinishMapMeters` independent of route existence;
- `loadScenarioPreview()` and calculated results populate those endpoints;
- viewer renders a reference grid, a large green START marker, a large yellow FINISH marker/ring, static obstacle geometry and Cobra-at-start before calculation;
- `fitCamera()` explicitly includes authored scene endpoints, so preview framing no longer depends on a calculated route;
- fixed diagnostics panel is placed below the top controls and shows the chain state without vertical reflow;
- Stage-1 playback controls are visually labeled `ПОЛЁТ: ЭТАП 2` / `FOLLOWER: OFF` and cannot start playback when only the one route frame exists;
- `ScenarioRunResult` now exposes diagnostics;
- calculation writes console diagnostics plus `tools/navigation_runtime/last_route_plan.log`;
- diagnostic chain explicitly distinguishes `SCENE`, `PLANNER`, and `FOLLOWER: NOT RUN (STAGE 1)`;
- Stage-1 architecture checker now pins pre-calculation scene endpoints, preview loading and diagnostic ownership.

This does not start Stage 2. No Follower/physics execution was reintroduced.

Target validation pending for these viewer/diagnostic changes.


## 2026-09-21 — Follower restored as Stage 2 of the same diagnostic stand

User correctly rejected the previous interpretation where splitting work into two stages
removed Follower/physics from the viewer entirely. The intended split is by ownership,
not by executable:

```text
Stage 1: Calculate
  static scene -> NominalRoutePlanner -> retained route polyline

Stage 2: Execute
  retained route polyline -> time trajectory -> Follower -> PilotSkill -> physics
```

The same viewer now supports both stages.

### Stage 1 remains unchanged

`РАССЧИТАТЬ` calls only `NominalRoutePlanner`. It builds the static route once. The
user's target evidence already showed a valid four-point 323.75 m detour around the
wall. Control law, pilot skill and flight style do not alter this nominal route.

### Stage 2 restored correctly

After a successful Stage 1 the playback button becomes `ЗАПУСТИТЬ ПОЛЁТ`.

Stage 2 consumes `calculatedRoute.routePoints` directly and MUST NOT invoke
`NominalRoutePlanner::plan`, `NavigationRuntimePlanner::plan`, or any other global
route search.

Current static Stage-2 chain:

```text
retained Stage-1 polyline
 -> TrajectoryGenerator / RuckigRoutePlanner
 -> time-parameterized trajectory
 -> bounded AcceptedManeuverProgram chunks
 -> TrajectoryFollower
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> SharedShipPhysics + DynamicMotionSystem
 -> playback trace
```

The former stand-local `makeShortProgram()` quintic shortcut is NOT restored.

### Execution modes

The existing viewer selectors now become real Stage-2 inputs:
- Newtonian / Assisted -> physical local flight law + reference-attitude policy;
- Expert / Average / Loser -> real PilotSkillExecutor profiles;
- Standard / Extreme -> Ruckig route speed request.

Pilot execution starts from neutral revision zero so the first real route intent exercises
the selected pilot's reaction-delay model.

### Static-only dynamic boundary

Dynamic/sudden obstacles remain reserved for the next pass. Stage 2 currently reports
`DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS`; it does not silently use the sudden
obstacle checkbox and does not rebuild the route.

### Diagnostics

Stage 2 writes:
- stdout lines prefixed `[NAV-STAGE2]`;
- `tools/navigation_runtime/last_execution.log`;
- `tools/navigation_runtime/last_execution_trace.json`.

Diagnostics separate Ruckig trajectory generation, Follower, Pilot bridge, final errors,
route deviation and coarse static contact.

### Physical-proof limitation

The current static execution path uses the coarse route envelope for static collision
checks and is useful for observing Follower/physics behavior. It is NOT yet final B6
oriented swept-hull/tunnel proof. Do not promote a visually successful run to full
navigation acceptance.

### Validation state

This restored Stage-2 execution path is committed but has NOT yet been compiled/run on
the user's MinGW64 target. The last target-verified fact remains: Stage-1 nominal route
planner passed and produced the four-point 323.75 m static detour.


## 2026-09-21 — immutable retained route for Stage-2 comparisons

Viewer state now keeps the successful Stage-1 `TraceDocument` as a separate immutable
retained-route snapshot. Stage-2 execution traces no longer replace the only copy of the
Planner result.

Changing control law, pilot skill, flight style or the reserved sudden-obstacle toggle
after an execution run restores the retained Stage-1 route and marks only Stage 2 stale.
The next execution run reuses the exact same route without calling Planner again.

This is required for clean comparisons such as Expert/Newtonian vs Average/Assisted:
route geometry is held constant while only the execution layer changes.


## 2026-09-21 — target gate false negative in Stage-2 architecture checker

Target run stopped before compilation with:

`[FAIL] static-route/two-stage navigation: Stage-2 execution path missing TrajectoryGenerator::generate`

This was a checker defect, not a runtime defect. The checker isolated only the body of
`executeCalculatedRoute()`, while the actual Ruckig call is correctly delegated to
`buildExecutionTrajectory()`:

```text
executeCalculatedRoute()
 -> buildExecutionTrajectory()
 -> TrajectoryGenerator::generate()
```

Fix committed in `tests/architecture_contracts/check_navigation_stage1_nominal_route.py`:
- execution function must call `buildExecutionTrajectory()`;
- trajectory-builder slice must contain `calculatedRoute.routePoints` and
  `TrajectoryGenerator::generate`;
- global-planner prohibition is checked across the owned Stage-2 trajectory-builder +
  execution text.

No production navigation/runtime behavior changed in this fix.

Validation state: target compilation of the restored Stage-2 viewer still has NOT been
reached. Rerun the focused gate from latest main.


## 2026-09-21 — second Stage-2 architecture checker false negative

Target gate stopped with:
`Stage-2 execution path missing DynamicMotionSystem::applySystemAccelerationDemand`.

Runtime call is present. The checker failed because production formatting splits the C++
scope operator/call across lines:

```cpp
game::navigation::DynamicMotionSystem::
    applySystemAccelerationDemand(...)
```

The architecture checker previously searched an exact single-line string.

Fix:
- added whitespace-insensitive C++ token normalization via `compact_cpp()`;
- Stage-2 execution and trajectory-builder call checks now compare normalized token
  streams rather than raw formatting;
- runtime behavior is unchanged.

Validation state: target compilation of the restored two-stage viewer still has not yet
been reached; rerun the focused gate from latest main.


## 2026-09-21 — Follower first-chunk handoff bug + full initial kinematics

Target Stage-2 execution reached real runtime and produced:
- cached Planner route OK;
- Ruckig trajectory OK, 1521 samples;
- 102 AcceptedManeuverProgram chunks;
- Follower FAIL after only 24 viewer frames;
- max Follower position error only 0.02 m;
- final position still ~295 m from goal.

The failure signature localizes the problem to the first AcceptedManeuverProgram chunk
boundary, not route tracking quality. At Standard 10 m/s the Ruckig sampling interval is
0.05 s for this scenario. A 16-sample program therefore spans about 0.75 s, matching the
~24 trace frames at 30 Hz before failure.

Root cause found in Stage-2 program selection:

```cpp
vehicle.timeSeconds + 1.0e-9 >= next.acceptedAtUniverseTimeSeconds
```

This can activate the next program fractionally before its acceptance time. The canonical
ManeuverProgramSampler treats any negative elapsed time as `BeforeStart`, and
TrajectoryFollower maps `BeforeStart` to `InvalidInput`.

Fix committed:
- remove the positive epsilon from future-program activation;
- keep the current chunk until simulation time is actually >= next acceptedAt;
- pre-sample the active program before Follower and record exact failure reason/index/
  execution time/program acceptedAt in Stage-2 diagnostics;
- architecture gate now forbids the early-switch expression and pins the new diagnostics.

The user's video is consistent with this: Cobra moves only a few meters, then execution
stops at the first chunk handoff.

### Initial ship state correction

The scenario previously supplied only position, velocity and body orientation. That was
incomplete for a physically continuous Newtonian execution handoff.

Stage-2 initial kinematic state is now explicitly:
- position P;
- linear velocity V;
- linear acceleration A;
- body orientation (forward/up basis);
- pitch/yaw/roll angular rates.

New scenario fields under `start`:
- `acceleration`;
- `pitch_rate_rad_s`;
- `yaw_rate_rad_s`;
- `roll_rate_rad_s`.

Default current scenario remains:
- P = (0,0,0) m;
- V = (6,0,0) m/s;
- A = (0,0,0) m/s2;
- forward = +X;
- up = +Y;
- angular rates = 0.

`TrajectoryGenerationRequest` now owns `initialAccelerationMps2`, validates it, and
passes it into the initial Ruckig state instead of hardcoding zero. A focused Ruckig route
test now requires initial acceleration to be preserved at the first trajectory sample.

The execution vehicle initializes pitch/yaw/roll rate from the scenario. No global route
recalculation semantics changed.

Validation state: these fixes are committed but not yet target-rebuilt/re-run.


## 2026-09-21 — retained-route Stage-2 target execution PASS

User-supplied target diagnostics prove that the route-leg phase correction works in the
real diagnostic stand. The supplied output does not contain `git rev-parse HEAD`, so
this evidence is intentionally not attached to an inferred commit SHA.

Stage 1 remained unchanged:
- START = (0,0,0) m with V=(6,0,0) m/s;
- FINISH = (300,0,0) m;
- one static obstacle;
- 4 retained route points;
- retained route length 323.75 m;
- static detour YES.

Observed Stage-2 runs:

| Pilot / style / law | Ruckig samples | phases / handoffs | final P error | final speed | max route dev | max follower error | contact |
|---|---:|---:|---:|---:|---:|---:|---|
| Expert / Standard / Newtonian | 1521 | 3 / 2 | 0.15 m | 0.10 m/s | 3.26 m | 1.08 m | NO |
| Expert / Extreme / Newtonian | 1416 | 3 / 2 | 0.10 m | 0.48 m/s | 3.26 m | 0.95 m | NO |
| Expert / Extreme / Assisted | 1416 | 3 / 2 | 0.10 m | 0.48 m/s | 3.26 m | 0.95 m | NO |

All three runs report:
- `ROUTE EXECUTION COMPLETE: YES`;
- `FOLLOWER: EXECUTED`;
- `FOLLOWER FAIL REASON: NONE`;
- `PILOT BRIDGE: EXECUTED`;
- `COARSE STATIC CONTACT: NO`.

This directly closes the previous 102-microprogram failure mode. The current four-point
retained route is executed as three meaningful physical route-leg phases with two real
ManeuverPhaseGate handoffs.

The exact retained-route E2E gate now exercises the previously missing chain:

```text
scenario.json
 -> NominalRoutePlanner once
 -> immutable retained route
 -> Ruckig
 -> route-leg AcceptedManeuverPrograms
 -> TrajectoryFollower
 -> PilotSkillExecutor
 -> SharedShipPhysics / DynamicMotionSystem
 -> authored finish
```

This is strong static integration evidence, not full Navigation-v2 acceptance. The
current static contact check is still coarse route-envelope geometry, not the final
oriented swept-hull B6 tunnel proof.

### Next active slice — dynamic local overlay on the retained route

Static retained-route execution is now sufficiently stable to start the previously
deferred dynamic layer.

Hard ownership contract:
- dynamic actors never rebuild the retained global/static route;
- moving/sudden obstacles are a bounded local monitor/avoidance overlay only;
- surprise obstacles become visible only after their authored activation time;
- if a physically executable short bypass exists, execute that bypass;
- do not require an immediate return-to-line segment inside the same horizon;
- if no safe executable bypass exists, issue active braking while navigation remains
  alive and continues evaluating fresh world state;
- after the obstacle clears, progressively reacquire the retained route under vehicle
  limits rather than teleporting or enforcing an arbitrary merge distance.

Initial dynamic acceptance should keep the same Stage-1 route immutable and first prove
Expert/Standard/Newtonian. Mode/style expansion follows after the local dynamic chain is
behaviorally correct.


## 2026-09-21 — video review: static maneuver quality blocker

User video review exposed two real quality defects that must be fixed before the
dynamic-overlay stage is promoted.

### 1. Shallow retained-route corners incorrectly become StopTurnGo

For the previous default retained route, the static planner produced approximately:

```text
(0, 0, 0)
 -> (110, -27, -45)
 -> (190, -27, -45)
 -> (300, 0, 0)
```

The two interior course changes are only about 25.5 degrees. An Expert pilot has no
behavioral reason to stop there.

The stop is caused by the current Ruckig waypoint-authoring fallback:
- `buildWaypointVelocities()` attempts a 25%-leg-length shortcut chord around each
  interior polyline point;
- on this geometry that chord intersects the inflated static wall;
- the waypoint is therefore left with zero target velocity;
- Ruckig then correctly solves a stop-at-waypoint trajectory;
- both Newtonian and Assisted inherit the same upstream StopTurnGo reference.

This is a trajectory-authoring defect, not a sensible Expert maneuver doctrine.

### 2. Newtonian hull/velocity semantics are not yet expert-quality

Low-level propulsion allocation is physically differentiated:
- Newtonian has aft-main thrust only for the main channel; reverse/main braking
  requires body reorientation, while lateral/vertical correction is bounded by RCS;
- Assisted has symmetric longitudinal main authority, with lateral/vertical correction
  still bounded by RCS.

However the Stage-2 reference is authored in the wrong order for a
main-engine-dominant Newtonian maneuver:
- Ruckig first creates a translational P/V/A history using a common vector
  acceleration budget;
- `buildReferenceAttitudes()` then points Newtonian body-forward toward the already
  requested acceleration whenever |A| > 0.35 m/s2;
- bounded RCS can therefore begin changing the velocity vector while the hull is still
  catching up;
- before a zero-speed waypoint, the acceleration vector becomes braking-oriented, so
  the body reference rotates toward the braking vector and then rotates again for the
  next leg.

The resulting motion is physically possible as an RCS-heavy correction, but it is not a
credible Expert Newtonian route maneuver. Large course changes must be authored as
body/thrust-aware physical primitives before ACCEPT, not derived by attaching attitude
after an arbitrary translational Ruckig curve.

Assisted also stops at these shallow corners because the zero waypoint velocity is
already baked into the common translational trajectory.

### 3. Standard vs Extreme is not yet a real flight doctrine

In the current diagnostic Stage 2, Standard vs Extreme primarily changes the requested
route speed (10 vs 18 m/s). It does not yet choose different physical maneuver families,
clearance use, slip tolerance or aggressiveness. Therefore visually similar maneuver
semantics are expected and are not sufficient implementation of the requested flight
styles.

### 4. Static route detour was unnecessarily long

The old box support graph had:
- 8 corners;
- 6 face centers;
- no 12 edge midpoints.

For the default wall, inflated/support half-extents are approximately
`(40, 27, 45)`. Because edge midpoints were missing, visibility A* had to pay
clearance on two transverse axes and selected the old 323.75 m route through
`(110,-27,-45)` and `(190,-27,-45)`.

The geometrically shorter same-clearance route is approximately:

```text
(0, 0, 0)
 -> (110, -27, 0)
 -> (190, -27, 0)
 -> (300, 0, 0)
```

with length about 306.53 m.

Candidate fixes committed:
- `e53312cc9b00119e69f1c7676edf14b5d21fea64` — add all box edge-midpoint support
  nodes to `GeometricPathPlanner`;
- `8e0d4c4c6ca7d4d7ae3e7cc71387ebff8b335302` — regression requiring a nearest-face
  one-plane box detour rather than unnecessary second-axis displacement;
- `ac30d441ebdafe232af0bf0fe7e8882da9f38767` — viewer uses a Cobra-like triangular
  prism, a much smaller translucent nose marker, and a thick current-velocity vector
  whose length is proportional to speed, with numeric speed at the vector tip;
- `c4c63c9751a4b5b381da290a570ee98775148eb6` — diagnostics now print every retained
  route point, interior retained-waypoint speeds and maximum body/velocity angle.

These candidates are not target-validated yet.

### Active priority change

Dynamic local avoidance is postponed, not discarded. First make the static retained
route execution behaviorally credible.

Next mechanism target:
- shallow/medium corners must remain moving when a physically safe maneuver exists;
- no zero-speed waypoint merely because one synthetic corner-cut chord is blocked;
- Newtonian Expert must use a body/thrust-aware maneuver (lead turn, drift/coast,
  RCS trim, main-burn as appropriate) rather than letting the translational vector
  rotate independently and asking attitude to catch up afterwards;
- Assisted Expert must also preserve speed on ordinary shallow bends;
- stop/flip is reserved for geometry, required terminal pose or braking physics that
  actually requires it;
- Standard and Extreme must differ by real maneuver doctrine/aggressiveness, not only
  by max-speed scalar;
- production B5/B6/B7/B8 ownership must be preserved. Do not turn geometric targets
  directly into accepted execution programs to make the viewer look good.

The already documented architecture gap remains decisive: ordinary B5 exists first for
Newtonian, but a generalized ordinary B6 continuous maneuver prover is still missing.
Do not bypass that proof boundary.


## 2026-09-21 — adaptive corner pass candidate after user retest

User retest confirms:
- the new nearest-face nominal route is logically acceptable;
- the full stops at the two shallow intermediate points remain;
- the program-reference arrow visibly leads the physical hull;
- the current velocity vector was not legible enough in the viewer.

The supplied performance log confirms the stop mechanism is still active: the latest
four-point / one-obstacle Stage-2 solves finish with `blended_waypoints=0`.
Therefore both interior waypoints are still authored as zero-speed points before this
candidate.

### StopTurnGo fallback changed

Candidate commits:
- `d2205f50259fdef05a6515fec3822055891c47d5`:
  `buildWaypointVelocities()` no longer tests only one 25%-of-leg corner chord.
  It searches from the widest proposed local blend toward a bounded smaller blend and
  keeps the widest collision-clear option. If a generated Ruckig leg still fails,
  through-speed is reduced progressively (70% steps) before falling back to zero.
- `641e6ef6aeb79d4dddcf58ce7601448723f79b9a`:
  removed the arbitrary global `0.65 * maxSpeed` waypoint cap. Curvature/available
  lateral acceleration and explicit speed limits now own the corner-speed limit.
- `20aef47942c675ae59b04c288b2ae4b3cb6de5e6`:
  focused Ruckig regressions now distinguish:
  1. wide corner chord blocked but tighter continuous turn available -> must keep moving;
  2. even minimum local blend blocked -> safe stop remains legal;
  3. the current default wall detour -> both shallow interior points must retain
     non-zero through-speed.

This makes the retained white polyline a geometric/topological intent rather than a
literal StopTurnGo execution prescription. Ruckig remains responsible for a continuous
time trajectory through the retained route; the candidate does not reintroduce the
retired custom B-spline backend.

If the target gate still collapses either default wall waypoint to zero, the next step
is to add explicit maneuver-space offset / corner-radius reserve to the execution path
rather than accepting StopTurnGo. Slightly lengthening the geometric route is allowed
when required to create real turn room.

### Viewer observability changed

Candidate commits:
- `cbdbcc2b2132f0ef29af9a73e3589d25c415a8f5`;
- `2ad3bf086c153895adefee64fb9f67bcabaa84da`;
- `f695a55454f5ccc5802c618347dfb400ec4d6bcb`.

Changes:
- speed number moved away from the ship into a fixed lower-right block;
- current velocity vector is now bright yellow, 6 px wide and 3.5 world-meters per
  1 m/s, so 10 m/s is about a 35 m arrow;
- short cyan arrow = actual physical hull nose;
- red arrow = Follower/program target nose;
- white line = retained geometric route;
- green line = actual flown trajectory;
- the lower-right block states these meanings explicitly.

Interpretation rule for the next video:
- red leading cyan by itself means the attitude reference is commanding ahead of the
  physical hull and is not automatically wrong;
- the important Newtonian fault is if the **yellow actual velocity vector** bends as
  though the requested thrust attitude were already achieved, rather than following
  the acceleration authority of the actual cyan-oriented hull/RCS system.

The deeper Newtonian architecture issue remains open: generic translational Ruckig P/V/A
is still authored before the final body/thrust-specific maneuver. The adaptive corner
candidate is intended to remove the clearly invalid zero-speed fallback first, not to
declare the complete B5/B6 Newtonian maneuver problem solved.

### Validation status

UNVERIFIED on target MinGW64.

Run the focused Ruckig route tests plus the two-stage viewer gate before promoting this
candidate.


## 2026-09-21 — target corner gate 7/8; convert Test 1 to moving-start/moving-finish

Target MinGW64 evidence from the focused Ruckig suite:
- straight route: PASS;
- diagonal stopped leg: PASS;
- clear corner keeps through velocity: PASS;
- blocked wide blend shrinks before stop: PASS;
- default wall shallow corners stay moving: PASS;
- initial acceleration preservation: PASS;
- impossible braking rejection: PASS;
- the only failure was the synthetic `truly blocked corner falls back to stop`
  assertion.

The important result is that the actual default four-point wall case now passes the
focused requirement that both shallow intermediate corners retain non-zero speed. The
single failure was not a safety failure: the test assumed that one tight fixture must
force zero speed. That assumption is stronger than the architecture contract. If the
solver proves a collision-free continuous passage, stopping only to satisfy the test is
wrong. The fixture is therefore retained as a swept-safety check, not as a mandatory
StopTurnGo assertion.

### Test-1 isolation change

For the default Standard stand, remove start/finish transients from the visual test:
- authored Standard max speed = 10 m/s;
- start velocity changed from 6 m/s to 10 m/s;
- finish speed changed from 0 m/s to 10 m/s.

The intent is a steady 10 m/s fly-through test:
```text
start already at 10 m/s
 -> shallow corner 1
 -> straight bypass leg
 -> shallow corner 2
 -> cross finish still at 10 m/s
```

This isolates the two turn maneuvers from startup acceleration and terminal braking.

### Moving terminal velocity support

The previous runtime had two artificial stop constraints:
- `executeCalculatedRoute()` rejected any non-zero authored finish speed;
- `RuckigRoutePlanner::plan()` unconditionally overwrote the final waypoint velocity
  with zero.

That is now replaced by an explicit exact terminal-velocity contract in
`TrajectoryGenerationRequest`:
- `hasTerminalVelocity`;
- `terminalVelocityMps`.

Point speed constraints remain upper bounds; the new terminal velocity is the authored
final inertial velocity.

Candidate commits:
- `c9a8e381531feee4516cafa436b67809fb2d753d` — request contract;
- `9372af939d2406768e530f9e2c02e05389f0df83` — Ruckig honors exact moving terminal;
- `82dc142e5c06f8be8e6c94aec810f16b8b47302f` — runtime accepts/authors moving finish;
- `75f11481979b83706861bbc98f8b6326341a07e0` — default stand start=10, finish=10;
- `d40e4fdc7cb1048d0f45daf2598788684e49affc` — focused tests use moving terminal and
  remove mandatory-stop assumption from tight corner;
- `bc5fbaa284663d7a986e2257b8d71f4a64a610ba` — focused straight Test 1 is a true
  steady 10 m/s transit;
- `7f57df3ef4a05d6601d05aa9c94de7121e0d9884` — retained-route E2E now requires
  crossing the finish at the authored 10 m/s.

These latest moving-terminal changes are not yet target-validated.

### Next target evidence

Run focused Ruckig first. Expected qualitative result:
- 8/8 focused tests;
- default wall shallow corners moving;
- straight Test 1 starts and ends at 10 m/s without an authored stop.

Then run Stage-1/Stage-2 viewer. For Expert/Standard/Newtonian, the useful visual is now
almost entirely the two corners because start and finish no longer require speed
transients.

After that, inspect yellow actual velocity vs cyan actual hull nose vs red target nose.
The deeper Newtonian body/thrust-aware maneuver-authoring issue remains open.


## 2026-09-21 — viewer-first maneuver inspection; moving finish is fly-through

Latest target output after switching the stand to start=10 m/s and finish=10 m/s showed:
- retained route remains 306.53 m with four points;
- Ruckig trajectory generated successfully;
- both interior retained waypoints are now exactly 10.00 m/s;
- no coarse static contact;
- the run failed only at the final phase with `FINAL_CAPTURE_TIMEOUT`;
- by timeout, final speed had collapsed to 1.01 m/s and final position error had grown to
  32.94 m;
- maximum body/velocity angle reached 178.94 degrees.

This failure was caused by a semantic mismatch in the stand, not by the two shallow
corner speeds. The last phase always used `ManeuverPhaseGate::StateCapture`, which is
correct for a parking/stopped terminal but wrong for an authored moving terminal. After
the nominal moving trajectory ended, the follower kept trying to capture a fixed final
position while also owning a non-zero terminal velocity reference. That produced the
artificial braking and eventual timeout.

Candidate fix:
- `9b1251e8601172ecd83ba4f656871f92f4669fb4` — a non-zero finish speed now makes
  the final phase `ScheduledMoving`; the terminal is a fly-through boundary rather
  than a parking capture.

Process change requested by user:
- current maneuver development is inspected visually in
  `navigation_runtime_viewer.exe`;
- the viewer preparation script must not auto-run the headless Stage-2 E2E and abort
  before the user can inspect the maneuver;
- `06513569f7861ac8b6e3104994ec72ffd0d891d1` removes that automatic headless
  Stage-2 run from `run_stage1_mingw64.sh`;
- focused/static architecture tests still run, and the viewer is still built;
- the headless E2E target remains available separately for later regression work.

Current visual objective:
```text
10 m/s at start
 -> corner 1 at moving speed
 -> bypass leg
 -> corner 2 at moving speed
 -> cross finish at 10 m/s
```

Do not interpret a red target-attitude arrow leading the cyan physical nose as a fault
by itself. The key Newtonian diagnostic remains whether the yellow actual velocity
vector changes consistently with the real cyan hull attitude and bounded RCS/main-thrust
authority. The 178.94-degree body/velocity diagnostic in the failed headless run is
strong evidence that this deeper maneuver-authoring issue remains.


## 2026-09-21 — expose the real calculated curve and replace exact-corner forcing with a rounded execution guide

User viewer evidence showed visible braking / path distortion on apparently straight
parts of the retained four-point wall route and requested that the actual calculated
"spline" be drawn.

Important correction: production runtime does **not** use the retired
`SmoothPathOptimizer`. The `SmoothPathPerf` records that can still appear in the
aggregate navigation performance log come from legacy/test compatibility runs. The
current runtime route-to-motion backend is the canonical Ruckig path. Therefore the
curve that must be inspected is the complete Ruckig reference trajectory, not a hidden
B-spline.

### Runtime geometry change

The previous Ruckig route authoring still forced each coarse Stage-1 support vertex to
be an exact target position while assigning a non-zero bisector velocity there. That
can create an unnecessarily awkward local state constraint: the solver must hit one
mathematical corner point and simultaneously leave it already rotated in velocity
space.

New candidate behavior:
- Stage-1 `routePoints` remain retained and unchanged.
- Stage-2 derives a separate collision-checked `executionGuide`.
- For every interior coarse vertex it computes a physically useful tangent reserve from
  authored speed and lateral acceleration.
- The coarse vertex is replaced locally by entry/exit guide points.
- If that useful corner does not fit, the local corner can be moved farther outward into
  free space in steps instead of tightening the curve or forcing StopTurnGo.
- Ruckig parameterizes this guide.
- The full guide is published in `TrajectoryGenerationResult::executionGuidePointsMeters`.

Candidate commits:
- `0b4212345b2c38b4f775937060a81ba56cace219` — expose execution guide;
- `939a7058ba58a244282d219e0c298150a72a410f` — rounded/widenable execution guide;
- `77f9640eb77267be6a258b0293720e00f089fbe6` /
  `f9b106f8c259c0f7a39ebde11f67c776e1a35c92` — guide diagnostics.

### Viewer change

The trace now carries two new Stage-2 products:
- `executionGuidePoints`;
- `calculatedTrajectoryPoints` (all Ruckig samples).

Viewer colors:
- white = retained coarse geometric route;
- blue = widened local execution guide;
- purple = complete calculated Ruckig curve;
- green = actual flown trajectory;
- yellow arrow = actual velocity;
- cyan short arrow = real hull nose;
- red arrow = program target nose.

Blue guide support points are also drawn as small crosses.

Candidate commits:
- `3ee684742afd8ad91307b99c0a1815bd30aea722` /
  `287ab085275873ffb1f991e09902d31706edb927` — trace schema/persistence;
- `33e3b228327cfb9c6031ecb09aa67847d16d2cda` — runtime publishes guide and full
  reference plus calculated min/max speed;
- `154d9e1a544dc957769efce127b70ee36dd8b0cc` — viewer renders the products.

### Focused regression

The default wall Ruckig test now additionally requires:
- execution guide has more support points than the four-point coarse route;
- calculated reference minimum speed stays >= 7.5 m/s in this steady 10 m/s stand;
- calculated reference does not geometrically backtrack along +X.

Commit:
- `68f4377aabae1fd894be4a882fb30b0cec762d89`.

This candidate is **not target MinGW64 validated yet**.

### Immediate diagnostic objective

In the next viewer run compare:
1. white coarse route;
2. blue execution guide;
3. purple calculated Ruckig curve;
4. green actual path.

Also inspect:
- `CALCULATED MIN SPEED`;
- `CALCULATED MIN SPEED POS`;
- `CALCULATED MAX SPEED`.

If purple is smooth and remains near 10 m/s while green brakes/loops, the fault is
downstream in Follower / body-thrust execution. If purple itself brakes or loops, keep
the fix in route-to-trajectory authoring and widen/reject the guide before ACCEPT.


## 2026-09-21 — purple reference itself bows at both rounded corners; replace chord-like guide with sampled C1 curve

User viewer evidence now isolates the defect conclusively:
- the purple calculated Ruckig reference itself has two visible lateral "barrels";
- they occur at the two rounded-corner transitions;
- therefore this is upstream of Follower/Pilot. The green actual path is not the primary
  cause of these two visible bulges.

Latest perf line for the new guide shows:
- coarse_points=4;
- guide_points=6;
- rounded_corners=2;
- expanded_corners=0;
- blended_waypoints=4;
- samples=2300;
- valid=1.

The six-point guide proved that merely replacing each coarse corner with one
`entry -> exit` chord is insufficient. Each Ruckig leg is solved with non-zero
endpoint velocities. When those velocity directions are not aligned with the leg chord,
Ruckig is free to generate a lateral polynomial bow while still satisfying endpoint
states.

### Mechanism fix

Each rounded corner is now represented by a densely sampled quadratic C1 curve:
```text
entry -> quadratic samples controlled by widened corner -> exit
```

Properties:
- the quadratic control point is the current local corner (including any outward
  expansion);
- its tangent at entry matches the incoming coarse leg;
- its tangent at exit matches the outgoing coarse leg;
- sample density targets about 2.5 m per segment, clamped to 4..24 segments per corner;
- the sampled curve itself is collision checked before acceptance;
- if the curve is not safe, the existing cut/expansion search continues.

This removes the long single Ruckig leg whose endpoint velocities were forcing the
visible barrel.

Commit:
- `3dccb8a36c8e8023083b21f317b944a8097fd6a0`.

### Small-angle speed fix

The old through-speed estimate clamped
`sin(angle/2)` to at least `0.15`. Once a curve is sampled densely, each local angle
is intentionally very small. That clamp falsely treated tiny smooth direction changes
as a much tighter turn and could create unnecessary braking.

The floor is now numerical only (`1e-4`), while the explicit max speed and lateral
acceleration remain the physical bounds.

### Focused regression

Default wall regression now additionally measures the full calculated Ruckig reference
against the published execution guide.

Requirement:
- maximum distance of any purple Ruckig sample from the blue execution-guide polyline
  must be <= 1.5 m.

Existing requirements remain:
- no major braking dip below 7.5 m/s in the steady 10 m/s experiment;
- no geometric backtracking along +X;
- collision-free;
- moving finish remains moving.

Commits:
- `517904389019fbf94ddbbabbe3248cdb7b6fab20` — visible-bow regression;
- `a056973c674194b109b8f37646f6d7a9e461c5b9` — explicit test include.

This candidate is not target MinGW64 validated yet.


## 2026-09-21 — target 7/8 after dense curve: functional speed-dip failure, not compile failure

Target MinGW64 evidence:
- `ruckig_route_planner_tests.exe` compiled and linked successfully;
- 7/8 focused tests passed;
- only failure:
  `default wall shallow corners stay moving`;
- failure reason:
  `default wall calculated curve contains a major unnecessary braking dip`.

This is a **functional trajectory test failure**, not a compiler/linker failure. The
test is intentionally retained: the steady 10 m/s wall stand must not hide a large
calculated speed dip.

### Root cause found

The sampled C1 execution guide introduced short local segments, but
`buildWaypointVelocities()` still computed corner speed from a heuristic
`blendDistance` proportional to local segment length.

That made speed depend on sampling density:
```text
same geometric curve
 + more guide samples
 -> shorter local segments
 -> smaller blendDistance
 -> lower turnSpeed
```

This is non-physical.

Candidate fix:
- use the three-point circumcircle curvature instead:
  `kappa = 2*|AB x BC| / (|AB| |BC| |AC|)`;
- local lateral-acceleration speed limit:
  `v_max = sqrt(a_lateral / kappa)`;
- this is invariant to guide discretization density;
- Ruckig perf log now records `min_speed_mps` and `max_speed_mps`.

Commit:
- `4dcebe77580e8abc7a0f9f4a22cec65f3ba985d5`.

### Remove start/finish heading transients from the steady stand

The prior "10 m/s start and finish" still had a hidden directional transient:
- start velocity was +X while the first retained route leg points
  `(110,-27,0)`;
- terminal velocity was +X while the final retained route leg points
  `(110,+27,0)`.

So the stand still asked Ruckig to perform extra heading changes at both boundaries.

The default scenario and focused wall fixture now align:
- start velocity and hull forward with the first route-leg tangent;
- finish velocity/forward with the final route-leg tangent;
- magnitude remains exactly 10 m/s.

Commits:
- `5c408b76b36a86bd6b4fecacc377142d80726796`;
- `3a22a4ae0aca037df4c9b8c76759506ed8a840c6`.

This makes the experiment genuinely:
```text
already moving along route at 10
 -> corner 1
 -> bypass
 -> corner 2
 -> leave along route at 10
```

### Viewer source revision control

Per user request, every viewer build now displays the exact Git source revision:
- CMake runs `git rev-parse --short=12 HEAD`;
- compile definition `ELITE_NAV_VIEWER_REVISION` is injected;
- the revision appears in both the native window title and the in-window HUD.

Commits:
- `2c1baf4854f083c2e3e44ed16136c70237dfafcd`;
- `1e84a9cac2f26da8e2802e1b97e8582f869f218d`.

The revision shown by the viewer is the HEAD that existed at CMake configure time. The
normal `run_stage1_mingw64.sh` configure/build flow refreshes it after every pull.

All changes after the reported 7/8 target run are currently unverified on target.


## 2026-09-21 — web review confirms dense 3-D Ruckig waypoint chaining is the wrong integration

Latest target evidence:
- focused binary still compiles/links;
- 7/8 tests pass;
- default wall test reports `min_speed=0.009 m/s`;
- viewer shows a large fan / convergence of purple calculated reference segments near
  the rounded route node.

The performance log explains the visual failure. After densifying the corner guide, the
runtime started feeding many close geometric samples to the 3-D state-to-state Ruckig
adapter. Recent lines show examples such as:
- `guide_points=52`, hundreds/thousands of Ruckig leg attempts;
- current default wall case `guide_points=18`, `legs=74`,
  `ruckig_ok=17`, `min_speed_mps=0.0094`.

This fan is therefore not a mysterious renderer artifact. It is the result of our own
integration: every close guide point became a new 3-D target state with its own target
velocity. Ruckig then solved many local polynomial state-to-state transitions and the
route nearly stopped at one of them.

### External verification

Official Ruckig documentation says:
- Ruckig's basic/open-source strength is state-to-state online trajectory generation;
- full local intermediate-waypoint trajectory calculation is a Pro feature;
- waypoint problems are significantly harder;
- Ruckig recommends as few waypoints as possible;
- it explicitly recommends filtering waypoint lists and prefers waypoints far apart.

This matches the target evidence exactly. The current dense chaining was a misuse of the
state-to-state solver.

The standard alternative architecture is also consistent with TOPP/TOPPRA:
```text
geometric path p(s)
    +
time parameterization s(t)
    =
trajectory p(s(t))
```

### Architecture correction implemented

Do not feed dense spatial samples to 3-D Ruckig.

New ownership:
```text
Stage-1 coarse route
 -> local rounded execution guide p(s)
 -> scalar jerk-limited Ruckig progress s(t)
 -> map s(t) back onto p(s)
 -> swept collision validation
 -> AcceptedManeuverProgram / Follower
```

Ruckig remains in the architecture, but in the role it is good at:
- true single-leg state-to-state solves still use the existing 3-D adapter;
- curved / multi-point routes use new one-dimensional
  `RuckigTrajectorySolver::solveProgress()`;
- dense guide points define geometry only and are never independent 3-D Ruckig targets.

Commits:
- `2e815ef993a31acd40596f170c67a124f9337bf9` — scalar progress API;
- `a50de59958887efbe50e2cdacee8c67623b2ebb8` — scalar Ruckig implementation;
- `9f166f4286dd03197705b0cd9e4ba63e33d000bc` — path-progress trajectory mapping;
- `3c58f4c3719d712e524e86e5f030820486af3102` — stop chaining dense 3-D legs;
- `c6f7b160878a3582362574691a12c96fd3ff3623` — keep stored guide sparse;
- `b9e7ce64182ce761edb45b4751d680950ab33ff5` /
  `3843194d999e302ca32171de3d49f3f21987de64` — contract docs corrected.

The current scalar timing uses one conservative path-wide speed cap for the current
slice (including curvature/range/positive point limits). This deliberately favors a
correct monotone reference over aggressive per-corner optimization. Later work may
introduce a sparse set of meaningful speed zones, but must never return to one Ruckig
3-D solve per geometric sample.

### Viewer revision and clutter

The previous revision stamp could be outside a cropped screenshot. Viewer now also
draws an always-visible in-window `NAV REV <sha>` badge below the top controls.

Per-sample cyan execution-guide crosses were removed. The BLUE guide line remains, but
internal geometric sampling no longer looks like dozens of commanded target states.

Commit:
- `1199a0c04ee7cf3a6938bef0ca9c3bb55d7bd4c8`.

### Validation state

This architecture correction has NOT yet been built or run on target MinGW64.
Do not call it PASS until the focused target gate and viewer confirm:
- no near-zero speed on the steady 10 m/s wall route;
- no purple fan/barrels;
- `NAV REV` visible and matches the current build revision.


## 2026-09-21 — excessive hull rotation isolated to Newtonian attitude authoring

Latest viewer/video evidence after scalar path-progress correction:
- purple calculated path is much cleaner;
- current wall-case perf reaches `min_speed_mps=10.0000`,
  `max_speed_mps=10.0000`, `guide_points=10`, one scalar Ruckig solve;
- remaining visually dominant defect is large hull rotation relative to the velocity
  vector during the shallow turn.

The cause is explicit in `buildReferenceAttitudes()`, not hidden physics:
```cpp
if (law == Law::Newtonian && acceleration > 0.35)
    requestedForward = normalize(sample.accelerationMps2);
```

For a constant-speed curved path, acceleration is predominantly centripetal and
approximately perpendicular to velocity. That rule therefore commands the ship to point
nearly broadside to its direction of travel even for a gentle turn.

This is especially wrong for the current stand because:
- `executionVehicleProfile.maxLateralAccelerationMps2` is already derived from
  `ShipParams::manoeuvreThrusterAccel`;
- Cobra test profile has `manoeuvreThrusterAccel = 2.0 m/s2`;
- the path curvature/speed combination is generated under that same lateral-authority
  envelope;
- therefore a gentle constant-speed turn that fits inside RCS authority does **not**
  require rotating the main engine into the centripetal acceleration vector.

The apparent sideways/out-of-plane motion in the video is primarily the commanded
broadside attitude viewed in perspective. The route and requested acceleration are
planar in this stand; the reference basis transport itself is designed to preserve the
previous roll orientation.

### Attitude fix candidate

Newtonian reference attitude now uses a minimum-cant allocation:

1. Default nose direction is velocity/path tangent.
2. If total requested acceleration is within manoeuvre/RCS authority, keep the nose on
   velocity.
3. If main-engine participation is required, calculate the smallest nose rotation such
   that:
   ```text
   positive main-engine thrust along nose
   + bounded omnidirectional RCS
   = requested acceleration
   ```
4. Do **not** point the nose directly at acceleration unless the physical demand truly
   requires that much rotation.

This keeps the hull close to the flight direction for ordinary gentle Newtonian turns,
while still allowing large cant/flip for demands that RCS cannot satisfy (hard braking,
high lateral acceleration, etc.).

Commits:
- `fcffa5bae3b4e3deab5d6f043d3c500137719dad` — minimum-cant Newtonian reference;
- `b943064d529935243863c30238ae0daf62f1c129` — report
  `MAX REFERENCE/VELOCITY ANGLE` separately from actual
  `MAX BODY/VELOCITY ANGLE`.

This candidate is not target MinGW64 validated yet.


## 2026-09-21 — viewer state authority, free-transit corridors, and main-engine visualization

User feedback after the minimum-cant Newtonian fix:
- visual behavior is now substantially better;
- UI appeared to disagree with actual control law after changing flight style
  (ASSISTED button still selected while motion looked Newtonian);
- exact 10.0 m/s tracking is undesirable for ordinary free transit: harmless
  ~10.1 m/s overshoot must not provoke a body rotation/braking manoeuvre;
- requested a visible indication of when the aft main engine is actually producing
  thrust.

Latest uploaded perf evidence remains healthy for the geometric/timing layer: the
steady wall cases at the tail of the log are one scalar Ruckig solve with
`min_speed_mps=10.0000` and `max_speed_mps=10.0000`. Therefore these changes belong
to execution/UI semantics, not the path generator.

### Redux-style viewer state

The native C++/GLFW viewer now uses one reducer-driven authoritative store:
- one `AppState`;
- explicit `ViewerActionType` actions;
- `reduceViewerState()`;
- all mode/pilot/style button clicks dispatch actions rather than directly mutating
  independent widget booleans;
- runtime control-law observations also dispatch into the same reducer.

This is intentionally Redux-style architecture, not the JavaScript Redux library.

Runtime truth is recorded in every execution frame:
- `hasRuntimeControlLaw`;
- `runtimeControlLaw`.

During playback the current frame is observed and, if a lower layer has actually changed
the law, the reducer updates `state.controlMode`. The ASSISTED/NEWTONIAN buttons are
therefore projections of the effective runtime state, not merely the last click.

Execution diagnostics now distinguish:
- `CONTROL LAW REQUESTED`;
- `CONTROL LAW EFFECTIVE`;
- `CONTROL LAW SWITCHES`.

The trace's textual law label is kept synchronized with reducer state, and effective-law
labels are localized in the viewer.

Relevant commits:
- `b503c3e35a9b8c2b0333b026b9251d6075a1afe2`;
- `1d17f9d208f9ef77a3dc8ac09753202aba7cd4c4`;
- `eedad40169c00509e01015f1a1777bd67f29965f`;
- `a60a215fde65505826b9b96c70edf511991da91f`.

### Free-transit speed/progress corridors

`AcceptedManeuverProgram::TrackingEnvelope` now has:
- `alongTrackSpeedDeadbandMps`;
- `alongTrackPositionDeadbandMeters`.

For `FreeTransit`, Follower decomposes position and velocity error into:
- cross-track components: still controlled normally;
- along-track components: ignored inside the configured corridor and corrected only
  by the excess outside it.

Current doctrine:
- STANDARD speed corridor: reference +/- 0.5 m/s;
- EXTREME speed corridor: reference +/- 1.0 m/s;
- STANDARD progress corridor: +/- 12 m;
- EXTREME progress corridor: +/- 16 m.

Thus a 10.1 m/s actual speed on a 10.0 m/s reference creates zero longitudinal speed
feedback in both styles. The vehicle is not required to chase exact schedule position
inside the progress corridor either.

This is deliberately limited to `FreeTransit`; precision docking/capture maneuvers keep
their stricter semantics.

The active speed corridor is persisted in trace frames and displayed in the viewer HUD
as e.g. `КОРИДОР V: 9.5 .. 10.5 М/С`.

Relevant commits:
- `92d65f86fe22ec1d0a404f952a0ebd0eb4935fb3`;
- `a171510889a2d235896e6ad567011c2b651ee3b7`;
- `023861170299dc7321975f2e73173af4b2547ca8`;
- `ad053356ada03d5212185b7d49d0b6aeb017ed6a`;
- `0b2e48b66744662e783b52b135ef26a714f43fb5`;
- `7a663da3b7f6bc8a92daa9433daeef40a10d41c9`;
- `3cadb29a840643524f2edafba3abb0b9091d795c`;
- `9e27bcd2392e7c18c36354374df33ab04882045f`;
- `3845b50910310494b394ec00820c86cc97b96aff`;
- `5299809e37f0a5061d56f066240619d56c6f27b4`.

### Aft-main-engine visualization

Each execution frame now records `mainEngineThrottle01` from the **actual physical**
`motion.mainEngineAccelerationMps2`, projected onto the current physical hull forward
axis and normalized by main-engine authority.

Viewer behavior:
- rear face of the diagnostic ship is filled orange only for positive aft-main thrust;
- intensity/alpha scale with actual main-engine throttle;
- HUD also shows `MAIN: N%`;
- RCS/manoeuvre-thruster acceleration does not light the rear face.

This directly answers whether a hull rotation was physically useful for main-engine
vectoring or was merely an attitude command with no aft-main thrust.

Relevant commits:
- `245bcc627fef5e0f79ad3e00e4b998c31e04b8db`;
- `2de51af96a3d9156fe7f3b5cb94a9f33d9f0e015`;
- `cbb58b16a7a15803cc8e56618d639916d748a53e`;
- `089d905f3207fa48afe3bba70935ce9413b145f6`;
- `163bee3c58443a5d0d6b4ad092ae7e6f970feef1`;
- `2e8692b68c670275e7f96654c4a990c473b885a9`.

### Validation state

These latest execution/UI changes are committed but are not yet user target-validated.
The next target viewer pass must explicitly compare STANDARD/EXTREME and
ASSISTED/NEWTONIAN while watching:
- effective button selection;
- `CONTROL LAW REQUESTED/EFFECTIVE/SWITCHES`;
- speed corridor text;
- `MAIN:%`;
- orange rear face.


## 2026-09-21 — body/engine telemetry + independent 5..50 m/s start/finish sliders

User feedback after reducer/corridor/main-engine visualization:
- current flight is visually much better, but a remaining unexplained hull somersault occurs
  near the low/straight part of the route while the main engine indicator is off;
- actual speed is about 10.1 m/s during the event;
- user asked whether logs contain body orientation changes and exact engine-on moments;
- user requested independent start and finish speed sliders, each 5..50 m/s.

### What the existing logs did and did not contain

`navigation_perf.log` is a trajectory-generator performance log. It contains Ruckig/path
timing facts only. The latest uploaded tail still shows the default wall reference as one
scalar solve with `min_speed_mps=10.0000` and `max_speed_mps=10.0000`; it cannot
explain a physical body flip.

Before this iteration, `last_execution_trace.json` already persisted:
- ship position;
- ship forward/right/up basis;
- ship velocity;
- effective runtime control law;
- actual positive aft-main throttle;
- accepted-program reference forward/right/up.

However, there was no compact human-readable time-series log correlating body attitude,
angular rate, main/RCS acceleration, and reference attitude.

### New execution telemetry

Each trace frame now additionally records:
- `shipAngularRatePyrRadPerSec`;
- `mainEngineAccelerationMps2`;
- `manoeuvreAccelerationMps2`;
- `engineAccelerationMps2`.

These fields are persisted in trace JSON.

A new file is emitted after every Stage-2 execution:
```text
tools/navigation_runtime/last_execution_telemetry.log
```

Every sampled line contains:
- time;
- phase;
- effective control law;
- position and speed;
- actual body forward/up;
- pitch/yaw/roll rates;
- main-engine throttle percent;
- main-engine acceleration vector;
- manoeuvre/RCS acceleration vector;
- combined engine acceleration;
- reference speed;
- reference forward/up;
- body-vs-velocity forward angle;
- actual-vs-reference forward angle;
- actual-vs-reference up angle.

It also marks exact sampled transitions:
- `MAIN_ON` / `MAIN_OFF`;
- `RCS_ON` / `RCS_OFF`.

This is the primary evidence for the remaining somersault. If main stays off while
`ref_forward/ref_up` themselves rotate, the problem is attitude authoring/program
sampling. If reference attitude is stable but actual body rotates, the problem is lower
in angular tracking/physics.

Relevant commits:
- `62c94992bfbde0983dbcb2d581c038f5939f890c`;
- `33854813be1d8299597a7aeb9e4bf40600403f16`;
- `72eeb3fde524136aff5edff1e33a683c9cf1e955`;
- `e8083bb2f27623e73753bb98a9b6137bfc5928c2`.

### Start/finish speed override seam

`ScenarioRunSettings` now has independent optional execution overrides:
- `startSpeedOverrideMps`;
- `finishSpeedOverrideMps`.

Negative values mean "use scenario.json authored value".

The runtime:
- preserves the authored start velocity direction and changes only its magnitude;
- applies the same effective start velocity both to trajectory generation and the actual
  simulated ship initial state;
- applies the effective finish speed to terminal Ruckig speed constraint/velocity,
  moving-terminal gate semantics, final-state validation, and diagnostics;
- Stage-1 static route geometry remains unchanged.

`ScenarioRunResult` exposes authored start/finish defaults so the viewer initializes
from scenario data, not a UI hard-code.

Relevant commits:
- `1932f80e1350a2631332b1092f7615bddc479dfe`;
- `0bebf41764177e6614efc10617ddfa034407f38b`.

### Viewer speed sliders

The reducer-owned viewer state now includes:
- `startSpeedMps`;
- `finishSpeedMps`.

Two independent sliders are visible under the main controls:
- `СТАРТ V`;
- `ФИНИШ V`;
- range 5.0 .. 50.0 m/s.

Dragging a slider dispatches `SetStartSpeed` / `SetFinishSpeed` through the same
Redux-style reducer. Changing speed after an execution restores the retained Stage-1
route and invalidates only Stage-2 execution, because speed is an execution setting and
must not force static route replanning.

Execution settings carry the current slider values into Stage-2.

Commit:
- `c199d0dbb042066075f1b320799d8e5dfa55b0a7`.

### Validation state

These telemetry and slider changes are committed but not target-MinGW64 validated yet.

Next evidence should include:
- successful runtime/viewer build;
- visible 5..50 start/finish sliders;
- one run that reproduces the unexplained somersault;
- the generated `last_execution_telemetry.log`.

Do not guess the remaining flip root cause from `navigation_perf.log`; use the new
execution telemetry.


## 2026-09-22 — control-chain audit, automatic Stage-2 refresh, speed-override validity, and somersault root cause

User raised two architectural/UX questions:
1. Who is actually controlling the ship during motion, versus merely animating a material point?
2. Do pilot/control-law/style/speed changes require another explicit Calculate press?

User also supplied `last_execution_telemetry.log`, which finally gives physical
attitude/propulsion evidence for the remaining hull somersault.

### Actual runtime control chain

The current Stage-2 execution is not a kinematic playback. The mutable ship state is a
real `ShipTransform` + `DynamicMotionState`, and the control chain is:

```text
AcceptedManeuverProgram
 -> ManeuverProgramSampler
 -> TrajectoryFollower
 -> ManeuverTrackingController
 -> NavigationSystemControlIntent
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState navigation acceleration demands
 -> SharedShipPhysics / ShipController (angular)
 -> DynamicMotionSystem (main-engine + manoeuvre/RCS allocation and translation)
 -> ShipTransform / DynamicMotionState
```

So there is a concrete autopilot/controller pulling the virtual controls. Pilot skill
changes the delivered command through `PilotSkillExecutor`; body angular rates are
bounded by the actual ship parameters; translational demand is physically split into
main-engine and manoeuvre/RCS channels according to the active control law.

However, the user's concern is partly correct at the *authoring* level: the current
Stage-2 still starts from a kinematically-authored trajectory and then derives an
attitude/reference program around it. Full B5/B6 body/thrust-coupled maneuver authoring
is still not complete. The lower execution is physical; the accepted program is not yet
the final unified physical maneuver compiler/proof architecture.

### Telemetry proves the current somersault is an angular-control runaway

The uploaded telemetry shows:
- early flight: actual body basis equals reference basis, angular rate is zero, and
  both main/RCS translation channels are quiet;
- near the first turn, body and reference briefly become nearly coincident while the
  physical pitch rate is already about +0.75 rad/s;
- after that, the reference settles back toward the route tangent but pitch rate keeps
  increasing/saturating around +1.57 rad/s, so the physical body continues rotating
  *away* from the stable reference;
- during much of that runaway the main engine is OFF; manoeuvre acceleration is
  translational RCS, not evidence that the main engine needed the flip;
- a second runaway later reaches nearly 179 degrees body-vs-velocity/reference with the
  main engine still OFF, and main thrust starts only after the hull is already nearly
  inverted.

Therefore the main engine is not requesting the somersault. The angular command loop is.

### Root cause in the current accepted-program representation

`AcceptedManeuverProgram` is capped at 16 samples per phase. The runtime previously:
1. differentiated the dense reference attitude to angular velocity and angular
   acceleration;
2. downsampled those values into at most 16 program samples;
3. linearly interpolated `angularAccelerationFeedForwardMapRadPerSec2` between the
   sparse samples;
4. added that feed-forward directly to angular tracking feedback.

This can smear a short angular-acceleration pulse over a long interval. The result
matches the telemetry: the reference basis has already passed the desired orientation,
but positive angular acceleration/feed-forward can keep winding up the ship.

Additionally, the follower previously clamped only the angular *feedback reserve*, then
added angular feed-forward without a final clamp against the accepted physical angular
capability.

Candidate fixes:
- `018fd2c08874cb426a9d3fc8340c85ffb4539bad`
  - FreeTransit no longer publishes sparsely-sampled angular-acceleration feed-forward;
    body attitude is tracked using reference basis + reference angular velocity +
    closed-loop feedback.
  - explicit START/FINISH speed overrides expand the Stage-2 speed envelope so 5..50
    m/s slider values do not fail `validRequest()` merely because STANDARD nominal
    speed is 10 or EXTREME nominal speed is 18.
- `e8369c0fa21cc59f0429741540dd4903f7f556e8`
  - total follower angular acceleration demand is clamped to the accepted physical
    angular-acceleration capability after feed-forward + feedback composition.

These fixes are not target-validated yet.

### Automatic execution refresh after UI changes

The old UX did require a separate Stage-2 Execute after changing execution settings,
and the viewer could therefore continue showing the previous trace. That created the
correct impression that buttons/sliders did nothing until another explicit action.

New behavior:
- one initial `Calculate` obtains/retains Stage-1 static route and immediately queues
  Stage-2 execution;
- after a retained route exists, changing:
  - Assisted/Newtonian;
  - pilot skill;
  - Standard/Extreme;
  - sudden-obstacle toggle
  automatically queues a fresh Stage-2 execution;
- START/FINISH speed sliders queue one fresh Stage-2 execution on mouse release rather
  than once per dragged pixel;
- these execution-only changes do not re-run the static Planner.

Commit:
- `7a164a65292a47e41c19a01bdcb3f55b93d0110d`.

Therefore the intended UX is now:
```text
Calculate once for static route
 -> change any flight/execution setting
 -> Stage-2 recalculates automatically
 -> viewer plays the new trace
```

A new Stage-1 Calculate is only needed when static route facts/goal/world geometry change.

### Full control-chain telemetry

To answer "who pulled which lever" rather than infer it from hull motion, trace frames
now also store:
- Follower ideal linear acceleration command;
- Follower ideal angular acceleration command;
- PilotSkillExecutor executed linear acceleration command;
- PilotSkillExecutor executed angular acceleration command.

These are written to JSON and to `last_execution_telemetry.log` as:
- `ideal_lin_cmd`
- `ideal_ang_cmd`
- `exec_lin_cmd`
- `exec_ang_cmd`

Then the same line shows the downstream physical allocation:
- `main_a`
- `rcs_a`
- `engine_a`
- actual `pyr_rate`
- actual/reference body basis.

Commits:
- `a2ece66294ab0a8aa3c4258257dbcdbe0c72b086`;
- `4343b7d5289fca95e6989f3e7d21f691723361bc`;
- `9efe6934aa3f8141604e81e714319126a83afa6a`.

This makes the next target run able to distinguish:
```text
program/reference
 -> follower ideal command
 -> pilot-executed command
 -> physical allocator
 -> actual body/velocity
```

### Validation status

All changes in this section are committed but not yet target-MinGW64 validated.
Do not claim PASS until the user rebuilds/runs the viewer and returns the new telemetry.


## 2026-09-22 — FlightStyle = clearance doctrine; speed/style now invalidate Stage-1; Calculate has one explicit meaning

User clarified the intended semantics:

- FlightStyle must never own a nominal/cruise speed.
- STANDARD vs EXTREME answers only the risk/clearance question:
  - STANDARD: prefer more maneuver/safety room around static geometry;
  - EXTREME: accept a tighter pass when that is useful (for example to stay close to cover).
- START/FINISH speed changes are not execution-only. Higher inertia changes the
  geometric room needed for a credible maneuver, so the retained Stage-1 route may move.
- `РАССЧИТАТЬ` must have one unambiguous action:
  - enabled only when current inputs invalidate the displayed result;
  - one click performs every required calculation;
  - show a concise on-screen result log;
  - after the attempt, visibly disable the button until an invalidating input changes.

### Removed style-owned speeds

The runtime no longer contains or parses:
- `standardSpeedMps`;
- `extremeSpeedMps`;
- `standard_speed_mps`;
- `extreme_speed_mps`.

Those keys were also removed from `tools/navigation_runtime/scenario.json`.

The current test stand's trajectory speed envelope is derived only from the explicit
boundary requests:
```text
max(start speed, finish speed)
```

FlightStyle is no longer passed into FreeTransit program authoring and no longer changes
Follower speed/progress deadbands.

Relevant commits:
- `7e8c67bacfc6615c1d03faf8f771e6e5e65b66d3`;
- `75d0f50fc25c784d515f1e9f092c37600d4e6346`;
- `75c85b028b4c7b21a3ddce4c1dff3912b596c95f`.

### Speed-aware Stage-1 maneuver reserve

A first coarse speed-aware route reserve now exists in the diagnostic stand.

`routePlanningClearanceMeters()` uses:
- the larger requested START/FINISH speed;
- real ship angular acceleration/rate limits;
- a representative 30-degree turn time;
- a style-only clearance factor.

Current coarse formula:
```text
inertialLead = planningSpeed * characteristicTurnTime
additionalClearance =
    authoredClearance +
    inertialLead * styleReserveFactor
```

Current factors:
- STANDARD = 1.0;
- EXTREME = 0.35.

This is intentionally a coarse Stage-1 maneuver-space reserve, not the final B6
time-parameterized oriented swept-hull proof.

Practical consequence:
- increasing speed should usually keep the same obstacle-side topology but move support
  points farther away;
- if another passage becomes cheaper/feasible, topology/point count may also change;
- at the same speed EXTREME should route closer than STANDARD.

Stage-1 diagnostics now include:
- `ROUTE PLANNING SPEED`;
- `STYLE CLEARANCE`;
- `ROUTE ADDITIONAL CLEARANCE`.

A new E2E regression compares:
- STANDARD 10/10;
- STANDARD 40/40;
- EXTREME 40/40;
and requires the 40 m/s Standard detour to move farther from the wall and the 40 m/s
Extreme detour to cut closer.

Relevant commits:
- `7e8c67bacfc6615c1d03faf8f771e6e5e65b66d3`;
- `a8930d82b8379d26b73a1bd1f61b6f7dbdf66c5d`;
- `f3f4734d0ebc2d0a500d9607862319d26b3e6d44`;
- `70f099ccd77810da8c31356efd1951cbc0549e01`;
- `0c6700ae470b7758da014cf7fe78e719a3c61362`.

### Calculate UX/state contract

Viewer state now separates:
- `routeInputsDirty`;
- `executionInputsDirty`;
- `calculationInProgress`.

Invalidation:
- START speed -> route + execution dirty;
- FINISH speed -> route + execution dirty;
- STANDARD/EXTREME -> route + execution dirty;
- Assisted/Newtonian -> execution dirty only;
- Pilot skill -> execution dirty only;
- sudden-obstacle toggle -> execution dirty only.

No setting auto-runs a calculation anymore.

One `РАССЧИТАТЬ` click:
1. rebuilds Stage-1 only when route inputs are dirty (or no route exists);
2. executes Stage-2 from the resulting/retained route;
3. publishes a concise on-screen log with route point count/length, flight result and
   requested boundary speeds;
4. clears dirty state after the attempt.

The button is then rendered dark/dim and labeled `РАСЧЕТ ГОТОВ`. It cannot be clicked
again until an input marks the result dirty. Dirty state changes the short log to
`РЕЗУЛЬТАТ УСТАРЕЛ` and re-enables `РАССЧИТАТЬ`.

The obsolete second `UiAction::Execute`, `ЗАПУСТИТЬ ПОЛЁТ` path and SPACE-to-execute
behavior were removed. Playback controls now only control playback.

Relevant commits:
- `523d40866610f5b167ad30ffd6fbe598d91fa701`;
- `ba4b38c5a380bfbb238841fe5b4eee419730d653`;
- `8a013ea2f54ced80dc9495948196f3922a0aba8e`;
- `d254b18cf7bc7a70030c24041facf3fd73175124`;
- `e99d3be88d52283531d26cb9e631e03b018fa88f`.

Documentation was updated in `tools/navigation_runtime/README.md` to match this
one-button workflow and the new style semantics:
- `ef3c632d5e2dbbcdb51dd51dba3918de39a4e646`.

### Validation state

These changes are committed but NOT yet validated on the user's MinGW64 target.

Next target evidence must verify:
- architecture contract script passes;
- runtime E2E builds/runs;
- viewer builds;
- `РАССЧИТАТЬ` is enabled initially, visibly disabled after an attempt, and re-enabled
  only after a relevant setting changes;
- changing speed visibly changes white Stage-1 route coordinates/clearance;
- at equal speed EXTREME is closer to the obstacle than STANDARD;
- no style-owned nominal speed remains in behavior/diagnostics.
