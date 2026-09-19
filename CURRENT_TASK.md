# Elite — CURRENT TASK

**Updated:** 2026-09-19 Europe/Kyiv
**Branch:** `main`

## Last exact target-machine checkout

```text
a69771e3efb5b54834b002a5000b79d75a8f5e80
```

Fresh evidence on that checkout:
- Stage-12 architecture contract: PASS, 0.211 s;
- navigation_runtime: 9/9 PASS;
- isolated navigation_work_scheduler: PASS;
- 5000 scheduler jobs:
  - enqueue 2053 us;
  - dispatch+complete 1261 us;
  - total 3314 us;
- EliteGame / EliteServer BUILD PASS;
- production build: 45.371 s;
- live navigation self-test: FAIL rc=56 after 15.144 s;
- log: `D:\__elite\work\build\logs\navigation_live_scheduler_20260919-221240.log`.

## Interpretation of failed live B14 gate

Do not roll back B14 and do not weaken the 120 s ordered-flight gate.

The failure reproduces the already documented pre-B14 behavior defect:
- visibility bypass exists;
- same-tick replication exists;
- no exact-static collision;
- moving obstacle plane is not passed;
- direct-route recovery / slit capture / tunnel transit do not complete.

The old premature portal-alignment defect is already fixed:
`first_bypass_align_forward=0`.

PilotSkill revision semantics are also not the cause:
- AcceptedShortSegment `goalRevision` remains the PilotSkill intent revision;
- segment revision is only targetRevision;
- reaction delay restarts only when the intent revision changes.

Primary blocker remains the B4 -> B5 handoff:
`AdjustedClear` is geometric, but the ordinary runtime path still turns it into
an arbitrary desired acceleration before physical maneuver compilation.

## Current candidate — first isolated B5 slice

Code/contract baseline before documentation commits:

```text
233d4023e81d5d466043a08c67acd8d49846c4b5
```

New:
- `src/game/navigation/OrdinaryPhysicalManeuverCompiler.h`
- `src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp`
- `tests/navigation_runtime/OrdinaryPhysicalManeuverCompilerTests.cpp`

### B5 responsibility

Input:
- current P/V/body basis/angular velocity;
- geometric target + desired velocity;
- directional propulsion capability;
- angular capability;
- B10 feedback authority reserve;
- pilot/control response reserve;
- control law;
- bounded primitive duration.

Output:
- fixed-capacity physical maneuver candidates;
- never world/geometry proof;
- every candidate keeps `requiresContinuousProof=true`.

### First supported law

Newtonian only.

Candidate families:
- `Coast`;
- `Trim` when the requested feed-forward acceleration already fits current
  body-axis authority;
- `LeadRotateMainBurn` when material delta-v cannot be produced directly by
  RCS/body-axis authority.

Assisted explicitly returns `UnsupportedControlLaw` for this first slice.
Do not silently reuse Newtonian semantics.

### LeadRotateMainBurn contract

```text
current inertial V
    -> quintic lead-rotation
       bounded by angular acceleration + angular speed
       + control-response reserve
       no main-engine translation during lead phase
    -> forward main-engine burn
       feed-forward aligned with vehicle forward
       B10 feedback reserve already subtracted
    -> bounded short-horizon result
```

B5 does not claim collision safety. B6 must prove the exact candidate before B8
can accept it.

### Regression coverage

The isolated tests include:
- forward body-axis trim;
- large lateral delta-v -> lead-rotate/main-burn;
- no angular authority -> fail closed;
- B10 feedback reserve removes feed-forward authority;
- Assisted remains unsupported explicitly;
- reconstruction of the live ~75-degree failure class:
  ~41 m/s^2 desired lateral acceleration with ~2 m/s^2 RCS must NOT be accepted
  as omnidirectional direct acceleration;
- 10,000 dirty-actor B5 compiles with timing diagnostic.

Expected timing output:

```text
[TIMING] ordinary_physical_maneuver_compiler compiles=10000 total_us=... per_compile_ns=...
```

Timing is diagnostic only.

## Run this iteration

Do NOT rerun the long live self-test yet; B5 is not wired into live planning.

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

Expected:
- architecture contract PASS;
- navigation_runtime **10/10 PASS**;
- new `ordinary_physical_maneuver_compiler` PASS;
- verbose B5 diagnostic prints 10,000-compile timing;
- scheduler remains PASS;
- EliteGame / EliteServer BUILD PASS.

No persistent log is required for this isolated gate. If it fails and a log is
created for diagnosis, the command/script must print its exact path last.

## Next slice after B5 isolated PASS

Implement B6 proof for this exact B5 candidate before any live ACCEPT:
1. continuous/sampled capability consistency;
2. swept static exact-HitVolume proof;
3. bounded dynamic candidate proof;
4. reserve/proof witness;
5. same candidate in -> same candidate out, annotations only.

Then connect:
`B4 geometric AdjustedClear -> B5 physical candidate -> B6 proof -> B7/B8`.

Only after that run the logged live scheduler/ordered-flight gate again.

## Documentation invariant

After each state-affecting event:
- rewrite `CURRENT_TASK.md`;
- rewrite `CONTINUE_PROMPT.md`;
- update `CURRENT_STATE.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- update architecture/migration/purity docs when ownership changes.
