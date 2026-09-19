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

## Fresh accepted evidence

Target-machine:
- navigation_runtime 12/12 PASS;
- 4-leg 3D corridor matrix PASS;
- expert + competent complete in both Newtonian and Assisted;
- rookie stops after first leg because first 3D attitude transition never
  satisfies terminal timing;
- no tested pilot exits 5 m corridor before completion/failure.

Assisted is the project's aircraft-like/"самолётный" local control law.

The calm 3D route produced identical Newtonian/Assisted metrics because it
never reaches the controlled-speed boundary where the execution laws differ.

## Current candidate

Code candidate before docs:
`88ad3a8921239cf2865c32c0c8711b7514094aa1`.

The existing maneuver_corridor_matrix target now runs explicit law stress:
- 2 m/s2 lateral RCS;
- 10 m/s controlled-speed envelope;
- 8 s;
- expert PilotSkill.

Expected:
- Newtonian >12 m/s;
- Assisted <=10.05 m/s;
- difference >2 m/s.

It prints:
`[LAW-STRESS] law=newtonian ...`
and
`[LAW-STRESS] law=assisted ...`.

Then the existing 2 laws x 3 pilots 3D corridor matrix runs unchanged.

This validates execution-law divergence only.
Ordinary planner-side B5 is still Newtonian-only; do not claim full Assisted
planner support yet.

## Run now

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

TIMEFORMAT='[TIMING] architecture_contract real_s=%R user_s=%U sys_s=%S'
time python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_runtime/run_mingw64.sh
```

Expected CTest count: 12.

Capture both LAW-STRESS lines and all six CORRIDOR-MATRIX lines.

After green:
- accept execution-law seam;
- analyze rookie attitude timeout;
- proceed toward B6;
- later add planner-side Assisted B5 family.

Every state-affecting iteration must synchronize CURRENT_TASK,
CONTINUE_PROMPT, CURRENT_STATE, PROJECT_STATE and STAGE12_END_TO_END.
