# CONTINUE PROMPT — Elite Navigation v2 / rerun docking after shortened-axis endpoint-clearance fix

Work in public repository `forzub/Elite`, branch `main`.

Before changing behavior/state read `AGENTS.md`, newest sections of
`CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and
`src/game/navigation/STAGE12_END_TO_END.md`. After every state-affecting
result synchronize those files and regenerate this prompt.

## Latest verified failure

Canonical `bash verify_docking.sh` successfully configured and built the
standalone navigation-runtime test, then native CTest failed:

`far preferred-axis blocker cancelled route instead of shortening lead:
no collision-free geometric path shortened=1 final_axis_m=8390`.

Meaning:
- preferred 9 km final axis was correctly treated as soft;
- the binary search selected the last epsilon-clear point immediately adjacent
  to the inflated blocker;
- that point was unsuitable as a visibility-graph goal.

## Current source fix

After finding the longest clear prefix, Planner now:
- retreats the docking-axis join by at least `max(50 m, 4*hullRadius)`;
- keeps the join on the exact semantic docking axis;
- if nominal geometric search still cannot reach it, retreats farther toward
  the mandatory ingress in increasing steps and retries;
- never retreats below mandatory ingress = `max(700 m, 3*standoff)`.

Regression additionally requires at least 25 m clearance from the inflated
far-axis blocker at the accepted shortened join.

Other current truth:
- Assisted is default;
- manual preferred final-axis lead = 9000 m;
- preferred terminal radius = 6000 m;
- open-transit manual release = 120 m with 1.00 s grace;
- automatic docking executor is still pending.

## Immediate gate

From `D:\__elite\work`:

```bash
git pull --ff-only origin main
git log -1 --oneline
bash verify_docking.sh
```

If PASS:

```bash
bash build_mingw64.sh
build/EliteGame.exe
```

Live acceptance:
- route calculation returns;
- far preferred-axis obstruction may shorten the axis but must not cancel the
  task;
- broad turn remains usable or radius relaxation is explicitly logged;
- default law is ASSISTED and DockPrep braking is rapid.

Do not provide patch files; commit directly to GitHub.
