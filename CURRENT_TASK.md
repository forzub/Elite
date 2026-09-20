# CURRENT TASK

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

## Task

Run the first target-machine gate for the corrected B4 receding-horizon bypass.

The contract under test is:

```text
safe short avoidance segment exists
    -> AdjustedClear
    -> continue forward through the safe segment
    -> do NOT require immediate same-horizon return to the route

no safe short avoidance segment exists
    -> ConflictHold
    -> active braking command
    -> navigation remains active
    -> re-evaluate on fresh world truth
```

After the obstacle is passed, route reacquisition is progressive under physical
control limits. There is no fixed 30 m return distance.

## What changed

Production:
- removed mandatory exact-static proof of bypass->merge from ordinary B4;
- removed mandatory time-coupled dynamic proof of the return leg;
- current proof covers the short segment actually selected for execution;
- equal-offset candidates prefer more forward progress;
- on-route merge point is now only a reacquisition reference.

Tests:
- repaired invalid portal/blocker geometry;
- replaced mandatory-return regression with a regression that requires a valid
  bypass even when immediate return is blocked;
- preserved no-space -> active braking coverage;
- architecture checker now pins the short-segment/receding-horizon contract.

## Next gate

Run architecture + navigation runtime on the exact pulled HEAD.

Inspect especially:
- `navigation_runtime_planner`;
- `navigation_composite_proving_ground`;
- selected bypass offset and forward distance;
- projection/static/dynamic rejection counts;
- whether the composite can either execute a safe bypass or correctly brake
  when physical authoring says it cannot evade.

Do not turn a geometric AdjustedClear into a claim of physical executability;
B5/B6 still own maneuver capability/proof.

## Exit criterion

Green target gate with:
- valid bypass when safe space exists;
- active braking when no safe short segment exists;
- no navigation shutdown;
- no forced same-horizon merge;
- no restored angular fan/branch mechanism.

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

## 2026-09-20 next task after 18:24 run

1. Repair only the oriented-portal fixture geometry: keep the agent inside region 1
   while maintaining >2.75 m dynamic separation at both start and staging endpoint.
2. Do not change B4 safety thresholds.
3. Restructure the composite after `NominalClear`: continue bounded planner/monitor
   updates while flying toward portal 102; do not execute an unmonitored 10 s scripted
   portal leg while the hazard is still active.
4. Add a compact visual trace/export for ship path, hazard path, safety envelope,
   selected targets and replan points so behavioral failures can be inspected directly.
5. Rerun architecture + 19-test runtime gate.

## Visualization placement decision

First visualization belongs next to `tests/navigation_runtime/NavigationCompositeProvingGroundTests.cpp`,
not inside the main game renderer yet.

Plan:
- composite test emits a deterministic trace file for the exact failing run;
- a small standalone debug viewer under `tests/navigation_runtime/visualizer/` renders it;
- show ship path, hazard path + inflated envelope, selected local targets,
  reacquisition references, portal geometry, and every replan point;
- after the behavior is understood and stable, reuse the same trace/debug data
  in the in-game NAV STRESS overlay.

## Immediate task — validate 3D viewer

Run the target MinGW64 runtime gate once to generate
`tools/navigation_runtime/last_trace_newtonian.json` even if the known composite
assertion still fails. Then build/run the standalone viewer:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_mingw64.sh || true
bash tools/navigation_runtime/run_mingw64.sh
```

Viewer acceptance for this iteration:
- window opens;
- route polyline and turn points are visible;
- ship is an oriented rectangular box with an unambiguous nose arrow;
- playback shows actual ship + hazard motion;
- replan/selected/reacquisition/portal markers are visible;
- viewer reaches the clearance-loss area from the failing composite trace.

After visual inspection, use the trace to fix the monitored topology-resume
problem; do not weaken navigation clearance or physical limits.

## Next target gate after viewer include/fixture fixes

Pull latest main and rerun the runtime tests plus viewer:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_mingw64.sh || true
bash tools/navigation_runtime/run_mingw64.sh
```

Expected checks:
- `navigation_runtime_planner` should get past the future-portal route-context fixture;
- composite should again write a Newtonian trace even if its known clearance assertion remains;
- viewer should now compile using `glad/include` and open the 770-frame trace;
- visually inspect the transition from `replan_2 / nominal_clear` into `portal_102`
  where monitoring currently appears to stop.

## Immediate validation — HUD build/open

Pull latest main and run only the viewer; the existing 770-frame Newtonian trace is
already present from the target composite run:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tools/navigation_runtime/run_mingw64.sh
```

HUD acceptance:
- visible top buttons: PLAY/PAUSE, PREV, NEXT, NEXT REPLAN, FIT;
- buttons respond to left mouse clicks;
- right panel explains current law/frame/time/phase/status/clearance;
- `WHAT IS HAPPENING` changes across phases/replans;
- legend makes every scene color/marker understandable;
- known `portal_102` failure area is called out during playback.

After HUD acceptance, return to the separate planner-fixture failure
`fixture must produce a safe adjusted target`, then use the viewer to inspect/fix
continuous monitoring through the portal-102 leg.
