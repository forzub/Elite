# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`

## Correction of the previous corridor test

The previous `maneuver_corridor_matrix` is NOT a hull-clearance proof.

It measured corridor distance from the ship center only, so it effectively
treated the vehicle as a material point. Its P/V/PilotSkill measurements remain
useful, but statements such as "zero 5 m corridor violation" apply only to the
center path.

For Cobra Mk1 this is physically impossible as a hull corridor:
- width = 26.0 m;
- height = 5.0 m;
- length = 22.2 m;
- body half-extents = right 13.0 m, up 2.5 m, forward 11.1 m.

## Physical control-law model

### Newtonian

Physical translation/attitude model:
- aft main thrust only for longitudinal main acceleration;
- no fore/nose main braking source;
- six-direction manoeuvre/RCS = 2.0 m/s2;
- bounded angular authority from main-nozzle vectoring / attitude actuators;
- material main-engine braking requires reorienting the hull so aft thrust
  opposes velocity.

For the stop test this means:

```text
accelerate with aft main
 -> coast while flipping ~180 deg
 -> aft-main braking burn
```

### Assisted / aircraft-like

Physical translation/attitude model:
- aft longitudinal main thrust;
- fore/nose longitudinal reverse main thrust;
- manoeuvre/RCS for lateral/vertical translation and stabilization;
- same bounded angular authority;
- no omnidirectional main engine.

For the same center trajectory:

```text
accelerate with aft main
 -> remain nose-forward
 -> brake with fore/reverse main thrust
```

Production `DynamicMotionSystem` has been changed to enforce this split.

Runtime-control regressions now pin:
- Newtonian reverse demand cannot use fore main;
- Assisted reverse demand can use fore longitudinal main;
- Assisted lateral demand must stay on RCS, not main thrust.

## Current candidate

Code/contract candidate before documentation commits:

```text
753eae5dcf1d7aae8eb05893ba75e16896a52b91
```

New target:

```text
maneuver_rigid_body_corridor
```

Expected navigation_runtime test count:

```text
13
```

## Rigid-body test vehicle

The test uses Cobra Mk1 physical/logical data:

```text
width  = 26.0 m
height = 5.0 m
length = 22.2 m

half extents:
right   = 13.0 m
up      =  2.5 m
forward = 11.1 m

aft main authority            = 7.5 g
Assisted fore main authority  = 7.5 g
RCS                            = 2.0 m/s2
angular/vectoring authority   = 3.0 rad/s2
pitch/yaw rate limit          = 2.5 rad/s
roll rate limit               = 3.0 rad/s
```

The test prints one `[VEHICLE-MODEL]` row.

## Stop trajectory

Both laws use the same 200 m center-of-mass trajectory:
- accelerate;
- 5 s centerline coast interval;
- brake to zero.

The difference is attitude and actuator source.

Newtonian:
- during coast the hull performs a smooth ~180 deg yaw flip;
- braking acceleration must align with the new backward-facing hull;
- aft main performs the brake.

Assisted:
- hull stays nose-forward;
- fore/reverse longitudinal main performs the brake.

## What is measured

Per physics tick:
- center position and velocity;
- full body orientation;
- angular velocity;
- maximum flip angle;
- center cross-track;
- all eight OBB hull corners against the corridor;
- required corridor half-width;
- tight 14 m corridor violation;
- 18.5 m flip-safe corridor violation;
- aft-main braking peak;
- fore-main braking peak;
- RCS braking peak;
- follower tracking-envelope exceed count.

Output rows:

```text
[RIGID-CORRIDOR] pilot=expert law=newtonian ...
[RIGID-CORRIDOR] pilot=expert law=assisted ...
[RIGID-CORRIDOR] pilot=competent law=newtonian ...
[RIGID-CORRIDOR] pilot=competent law=assisted ...
[RIGID-CORRIDOR] pilot=rookie law=newtonian ...
[RIGID-CORRIDOR] pilot=rookie law=assisted ...
```

The corridor centerline is extended beyond start/finish so the measurement is
transverse hull width, not an artificial endpoint-cap distance.

## Strict expert expectations

Newtonian:
- completes;
- max flip >= 170 deg;
- final forward direction remains >=170 deg from route-forward;
- aft-main braking >=5 m/s2;
- fore-main braking = 0;
- 14 m half-width corridor is too narrow by at least 2 m;
- 18.5 m half-width contains the flip envelope.

Assisted:
- completes;
- max flip <=5 deg;
- final forward direction remains <=5 deg from route-forward;
- fore-main braking >=5 m/s2;
- 14 m half-width contains the aligned hull.

Both:
- final center error <=1 m;
- final speed <=0.5 m/s.

Competent and rookie rows are diagnostic on the first run.

## Last supplied target-machine evidence

The most recent supplied run was:
- architecture PASS;
- navigation_runtime 12/12 PASS;
- low-level law stress:
  - Newtonian 15.960220 m/s;
  - Assisted 10.000000 m/s.

That proves the low-level laws are distinct, but the supplied paste did not
contain `git rev-parse HEAD`, so no exact checkout hash is invented for it.

## Run now

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

Capture:
- exact `git rev-parse HEAD`;
- `[VEHICLE-MODEL]`;
- all six `[RIGID-CORRIDOR]` rows;
- any compile warning/error;
- full test count;
- build timing.

Do NOT run the old 120 s obstacle-navigation live gate yet.

## After this gate

If rigid-body expert rows pass:
- accept physical hull/actuator execution baseline;
- use measured hull envelope as input to B6 corridor proof;
- then extend the same physical model from one straight stop to the multi-leg
  3D corridor.

If Newtonian flip/hull width fails:
- fix attitude/thrust timing or hull-envelope computation before B6.

## Documentation invariant

After every state-affecting event:
- rewrite CURRENT_TASK.md;
- rewrite CONTINUE_PROMPT.md;
- update CURRENT_STATE.md;
- update PROJECT_STATE.md;
- update src/game/navigation/STAGE12_END_TO_END.md;
- update canonical architecture/migration/purity docs when ownership/contracts
  change.
