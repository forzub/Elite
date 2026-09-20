# CONTINUE PROMPT — Elite Navigation v2

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Read `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`,
`src/game/navigation/STAGE12_END_TO_END.md` and relevant code/tests.
After every state-affecting event synchronize them and recreate this file from scratch.

## Viewer status

The standalone viewer under `tools/navigation_runtime/` is target-verified to compile
and open with the generated 770-frame Newtonian trace.

The first visible build lacked usable UI. It has now been updated with an in-window HUD:
- clickable PLAY/PAUSE, PREV, NEXT, NEXT REPLAN, FIT;
- right panel with law/frame/time/phase/status/clearance;
- `WHAT IS HAPPENING` phase explanation;
- full color legend;
- visible controls reminder;
- `portal_102` described as the current known clearance-loss area.

The HUD uses an internal 5x7 bitmap font and existing OpenGL primitives; no external
ImGui/UI dependency was added.

Latest HUD code is not yet target-verified.

## Immediate command

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tools/navigation_runtime/run_mingw64.sh
```

## Navigation test status

Latest target runtime still has 17/19 passing.
The planner fixture progressed past the old future-portal route-context assertion and
now fails at `fixture must produce a safe adjusted target`.
The composite still performs two safe B4 bypasses, becomes nominal-clear, then loses
dynamic clearance later in the long topology/portal leg. Trace generation works.

Do not mix the planner-fixture correction into HUD validation. After HUD acceptance,
fix the fixture, then diagnose/fix continuous monitored execution through portal 102
without weakening safety radii, padding, physical authority or tracking tolerances.
