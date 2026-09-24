# CONTINUE PROMPT — Elite Navigation v2 / manual docking acceptance

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing project behavior/state, read `AGENTS.md`, the newest sections
of `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those four files before the next slice. Regenerate this
prompt from current truth every iteration.

## Latest live evidence

The latest Windows run reached an active manual docking route and ended with:

`[DockAdvisory] left request=8 ... gate=12 ... lateral_m=27.1992 vertical_m=60.1343 bounds_m=60,60`

The supplied tail does not show the former dock-axis failure. The immediate
cancellation was triggered by only 0.1343 m beyond the nominal vertical
corridor. Request serial 8 also proves SHOW ROUTE is not globally one-shot:
each action creates a new serial.

## Implemented correction awaiting Windows verification

Current Cobra remains aft-main-only; Assisted may not invent a fore engine.
The commissioning defect was that BrakeToStop attitude acquisition ran only in
Newtonian. Assisted now acquires tail-to-velocity attitude when reverse main is
absent, allowing the real aft main to perform strong braking. Its stop
controller asks for the fixed-step delta-v needed to settle but the existing
main/RCS actuator clamps remain authoritative.

Manual docking corridor policy now has nominal, warning/critical and release
states. Nominal transit center tolerance remains 60 m and narrows to terminal
dock fit. Warning begins at 80% of a nominal axis. Crossing nominal no longer
deletes the route. The release envelope expands lateral/vertical tolerance by
max(25%,10 m), and cancellation requires 0.35 s continuously beyond release.

Warning state is published to the cockpit HUD and makes docking frames blink.
Displayed frame extent is explicitly ship dimension plus twice the center
tolerance, so the terminal frame equals the dock usable aperture after required
wall clearance. Speed labels render only for gates within 500 m. Every docking
frame marks its semantic bottom (-up edge) with an outward T.

Automatic DOCKING remains disabled. This slice is manual SHOW ROUTE acceptance.

## Next target gate

1. Pull canonical main.
2. Run the focused local-flight, manual-docking architecture and
   docking-advisory native tests.
3. Build canonical `build/EliteGame.exe`.
4. Test SHOW ROUTE from non-zero VREL in Newtonian.
5. Test SHOW ROUTE from non-zero VREL in Assisted.
6. Press SHOW ROUTE repeatedly; every press must produce a new serial and
   `stabilizing -> planning -> handoff_wait -> human_control=1`.
7. Fly deliberately near/outside nominal: frames must blink, small nominal
   excursions must remain recoverable, sustained outside-release motion must
   cancel after the grace interval.
8. Verify no speed text beyond 500 m, frame sizes follow corridor semantics,
   and bottom marker orientation remains correct under roll.

Capture complete `[DockPrep]`, `[DockAdvisory]` and
`[DockAdvisory] corridor-warning` lines on failure. Do not relax hardware
truth, widen geometry merely to hide a defect, or restore retired
DockingPathPlanner/GuidanceTunnel code.

The user wants implementation directly in GitHub followed by exact Windows
pull/test/build/run commands; do not deliver patch files.
