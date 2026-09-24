# CONTINUE PROMPT — Elite Navigation v2 / manual docking live gate

Work in public repository `forzub/Elite`, canonical branch `main`.

Before changing behavior, read:
- `AGENTS.md`
- newest sections of `CURRENT_STATE.md`
- newest sections of `CURRENT_TASK.md`
- newest sections of `PROJECT_STATE.md`
- newest sections of `src/game/navigation/STAGE12_END_TO_END.md`

After every state-affecting result, update those four Markdown files before
starting the next slice. Regenerate this `CONTINUE_PROMPT.md` from the current
truth every iteration; do not accumulate obsolete instructions here.

## Current verified state

The last Windows target evidence is checkout `D:\\__elite\\work` at
`84d59f4d`. Its built `EliteGame.exe` does not contain
`[DockAdvisory] axis request=`. SHOW ROUTE briefly appears and then the old
runtime logs only:

`[DockAdvisory] request=1 failed=dock moved off approach axis`

Therefore that result is from the pre-fix executable and does not test the
current Hub-local docking correction.

Verified corrected code baseline: `512b917c8d09bc22b479a1c82203bd6b003b7ccf`.
Documentation publication was recorded by `046ba6e6`. The correction keeps
the final advisory gate and the semantic dock/standoff in one tactical
Hub-local frame. Runtime samples ship/module from one authoritative tick,
rejects Hub/timeline/attachment changes, and logs numeric local axis delta
before any `dock moved off approach axis` failure. Cockpit/map world
projection is presentation-only. The 2 m axis guard remains intentionally
strict. Focused local docking and JSON numeric-locale regressions passed before
publication; Windows live acceptance is still open.

Manual SHOW ROUTE preparation is server-owned: request temporary Autopilot
authority, physically brake/settle, capture authoritative start state, plan and
publish route, send Complete, then wait for a newer session snapshot confirming
Autopilot=false before restoring local Human prediction. Automatic DOCKING is
still disabled.

## Immediate task

Do **not** change navigation math from the old bare failure. First prove the
target is running the published correction:

1. Fast-forward the Windows checkout from `84d59f4d` with
   `git pull --ff-only origin main`; preserve all untracked trace JSON/TXT.
2. Verify source contains `[DockAdvisory] axis request=`.
3. Run focused `json_numeric_locale` and `docking_advisory` tests.
4. Build using the repository's canonical `bash build_mingw64.sh`.
5. Verify rebuilt `build/EliteGame.exe` contains the same axis marker.
6. Run the canonical executable and capture combined stdout/stderr.
7. Press SHOW ROUTE and keep the route valid for >45 s.

Expected lifecycle evidence includes startup `LC_NUMERIC=C`,
`phase=stabilizing`, `phase=planning`, `phase=handoff_wait`, server
publication/hand-back, and `phase=manual human_control=1`.

If the corrected route still disappears, use the complete numeric
`[DockAdvisory] axis ... delta_local_m=... delta_m=...` or
`[DockAdvisory] left ...` line plus tick/time/Hub/module identity to diagnose
the next defect. Do not widen tolerances or reintroduce old
DockingPathPlanner/GuidanceTunnel machinery to hide it.

The user's preference is to apply code directly in GitHub, not deliver patch
files. When implementation is actually required, update the repository and
then provide exact Windows pull/check/test/build/run commands.
