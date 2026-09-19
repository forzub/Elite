# Elite Navigation v2 — continuation prompt

Repository: `forzub/Elite`, branch `main`.
Local checkout: `D:/__elite/work`.

Read first:
1. CURRENT_TASK.md
2. CURRENT_STATE.md
3. PROJECT_STATE.md
4. src/game/navigation/NAVIGATION_V2_BLOCK_ARCHITECTURE.md
5. src/game/navigation/NAVIGATION_V2_MIGRATION_MAP.md
6. src/game/navigation/STAGE12_END_TO_END.md
7. src/game/navigation/PLANNER_FOLLOWER_ARCHITECTURE.md

## Critical correction

Do not treat the old maneuver_corridor_matrix as hull-clearance proof.
It measures center-of-mass tracking only.

Cobra Mk1 is a rigid body:
26.0 m wide x 5.0 m high x 22.2 m long.

Navigation corridor validity must include orientation and hull occupancy.

## Physical actuator semantics

Newtonian:
- aft main only;
- bounded RCS;
- bounded angular/vectoring authority;
- main-engine braking requires physical flip.

Assisted/aircraft-like:
- aft + fore longitudinal main;
- bounded RCS for lateral/vertical translation/stabilization;
- bounded angular authority;
- no omnidirectional main engine.

DynamicMotionSystem and runtime-control tests now enforce this.

## Current candidate

Code/contract candidate before docs:
`753eae5dcf1d7aae8eb05893ba75e16896a52b91`.

New target:
`maneuver_rigid_body_corridor`.

Expected navigation_runtime count: 13.

The target runs the same 200 m center trajectory in both laws.

Newtonian:
accelerate -> coast+~180 deg flip -> aft-main brake.

Assisted:
accelerate -> nose-forward coast -> fore-main reverse brake.

The test measures:
- P/V;
- complete attitude;
- angular rate;
- actuator use;
- all 8 oriented Cobra OBB corners;
- required corridor half-width.

Strict expert expectation:
- Assisted fits 14 m half-width;
- Newtonian flip does not fit 14 m;
- Newtonian fits 18.5 m;
- Newtonian reaches ~180 deg flip and uses aft main;
- Assisted stays nose-forward and uses fore main.

Three deterministic PilotSkill profiles still run:
expert, production-competent, rookie.

## Run

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh

TIMEFORMAT='[TIMING] build_mingw64 real_s=%R user_s=%U sys_s=%S'
time bash build_mingw64.sh
```

Capture exact HEAD, VEHICLE-MODEL, all RIGID-CORRIDOR rows, warnings/errors and
build timing.

Do not run the old 120 s obstacle live gate yet.

After green:
- accept rigid-body/actuator baseline;
- feed hull envelope into B6 proof;
- then rebuild the multi-leg 3D corridor on the same physical model.

Every state-affecting iteration must synchronize CURRENT_TASK,
CONTINUE_PROMPT, CURRENT_STATE, PROJECT_STATE and STAGE12_END_TO_END.
