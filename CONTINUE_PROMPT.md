# CONTINUE PROMPT — Elite Navigation: strict API purity gate before physical maneuver work

Continue in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. recreate this file from scratch.

Also synchronize:
- `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md`;
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`;
- `src/game/navigation/NAVIGATION_API_CONTRACT.md` when API ownership changes.

## Current code baseline before docs

`efb3999b71a18186c1e3622c6c51d9b0169b52be`

Target-machine build has NOT yet validated this second API audit.

## Strict architecture rule

All calculation-affecting inputs cross an explicit API.

Pure navigation calculation code may use only:
- arguments;
- immutable values reachable from those arguments;
- implementation-only numerical epsilons/math identities.

Forbidden:
- scenario/config file reads;
- cwd/environment lookup;
- concrete ship descriptor lookup;
- GameSimulation/global world reach-through;
- global clock;
- private behavioral thresholds;
- broad Scenario/Settings parameters used merely to fetch a few fields.

## Concrete defects found/fixed in second audit

- stale E2E reacquisition/reference-hold contract;
- leaked undeclared `terminal.*` alias from previous refactor;
- route-clearance helper reading whole Scenario/Settings;
- vehicle-profile helper reading whole Scenario/Settings;
- attitude author reading whole Scenario;
- program-page author reading whole Scenario;
- ExecutionVehicle constructor reading Scenario/Settings;
- repeated speed/velocity resolution;
- hidden 0.25 / 0.1 trajectory behavior thresholds;
- RuntimePlanner hard-coded hold urgency/emergency threshold.

## Current API structure

Strict-pure kernels:
- NominalRoutePlanner;
- GeometricPathPlanner;
- TrajectoryGenerator/RuckigRoutePlanner;
- RuckigTrajectorySolver;
- OrdinaryPhysicalManeuverCompiler;
- ManeuverProgramSampler;
- ManeuverTrackingController;
- TrajectoryFollower;
- ManeuverPhaseGate;
- NavigationExecutionReplanPolicy.

Explicit stateful:
- NavigationRuntimeControlBridge;
- PilotSkillExecutor;
- DynamicMotionSystem.

Orchestration:
- loadScenarioDefinition;
- makeScenarioVehicleParameters;
- calculateScenario;
- executeCalculatedRoute;
- explicit diagnostic writers.

NavigationRuntimePlanner is a composition planner: its `StaticQueries&`
dependency is explicit in its API, so it has no hidden lookup, but it is not
referentially pure. Do not call it a pure kernel.

## Mandatory next run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

If architecture checker/build fails, fix API/compile first.

If they pass and Stage-2 E2E still invalidates tracking, proceed to the known
physics blocker:
```text
physical maneuver feasibility first
 -> Ruckig timing second
```

Do not tune follower gains or increase tracking-loss timeout.
Do not enable dynamic avoidance.
