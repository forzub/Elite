# CONTINUE PROMPT — rerun manual docking gate after stale flight-law check fix

Continue in GitHub repository `forzub/Elite`, branch `main`.

Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, and the latest section of
`src/game/navigation/STAGE12_END_TO_END.md`.

Latest target result stopped before the docking gate with:

```text
Local-flight-control architecture check failed:
shared local motion law lost: params.maxCombatSpeed
```

Root cause: stale static check. The intended current boundary is:

```text
ShipParams raw fields
 -> ShipDynamics.h centralized interpretation
    - controlledSpeedLimitMps()
    - effectiveLinearGs()
    - forwardMainAccelerationLimitMps2()
    - reverseMainAccelerationLimitMps2()
 -> DynamicMotionSystem consumes accessors
```

Do not reintroduce direct `params.maxCombatSpeed` / `params.maxGs` reads into
DynamicMotionSystem merely to satisfy grep checks. Native
`LocalFlightControlContractTests.cpp` already exercises actual speed,
acceleration, Newtonian/Assisted, overspeed and BrakeToStop behavior.

Manual docking candidate still requires target verification:
Human -> authoritative Autopilot -> physical Hub-relative stop/angular settle ->
fresh authoritative planning snapshot -> 500 m fixed advisory gates -> Complete
-> authoritative Human hand-back -> manual prediction resumes.

After each target result synchronize CURRENT_STATE.md, CURRENT_TASK.md,
PROJECT_STATE.md, STAGE12_END_TO_END.md and recreate this prompt. Never record
an unrun gate as passed.
