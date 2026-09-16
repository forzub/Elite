# Elite — Repository source of truth

**Updated:** 2026-09-16  
**Canonical repository:** `forzub/Elite`  
**Canonical development branch:** `main`

## Single-branch rule

`main` is the only canonical game-development branch.

Long-lived parallel game-development branches are prohibited. Do not create or adopt `chatgpt/*`, feature, experiment, or personal branches as an alternative project baseline.

A temporary rescue branch is allowed only to preserve divergent/unpublished history long enough to inspect and merge it safely. A rescue branch is not a development branch: do not continue feature work on it, do not name it as canonical in project-state Markdown, and delete it after its history has been incorporated into `main`.

If a tool or workflow temporarily requires a non-`main` branch, merge its result back into `main` in the same work slice and return project state/documentation to `main` before handoff.

Before coding or reporting project state, verify the exact GitHub ref being inspected. For a normal development iteration the answer must be `main` / `origin/main` unless the operation is explicitly a short-lived recovery step.

## 2026-09-16 branch reconciliation

The repository had accidentally accumulated three divergent histories:

- old `main`;
- `chatgpt/mae-v01075-semantic-workflow-motion-v5`;
- the user's local line later published temporarily as `rescue/local-97500`.

They were reconciled into `main` with a three-parent merge commit. Content was audited before the merge: the rescue and remote development lines contained the same current NavigationMap implementation; the remote development line additionally contained the later `NAV-V2-MAP-2` architecture-contract fix and CPU NavigationMap benchmark; old `main` contained project-state/history and asset-license/provenance material that had to be retained.

After reconciliation, both former development lines are ancestors of `main`. They are historical/recovery refs only and must not be used for further game development.

## Mandatory verification sequence

For every game-development iteration:

```text
1. Read CURRENT_STATE.md and CURRENT_TASK.md from main.
2. Resolve origin/main HEAD on GitHub.
3. Inspect code/CMake/tests from that exact ref.
4. Verify any named implementation exists on main before calling it local-only or missing.
5. Treat user-provided local console output as target-machine evidence, not proof of unseen code.
6. If local history diverges, preserve it first with a temporary rescue ref, reconcile it into main, then delete the rescue ref.
7. Do not move the canonical baseline away from main to work around divergence.
```

The 2026-09-16 failure mode was inspecting an obsolete `main`, then treating current GitHub code on another branch as hypothetical `LOCAL / UNPUSHED` work. The corrective rule is not to keep a second canonical branch; it is to prevent branch divergence and keep `main` authoritative.

Never invent a `LOCAL / UNPUSHED` state merely to explain a repository mismatch.

## Navigation v2 source hierarchy

Architecture authority:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
NAVIGATION_WORLD_V2.md
```

Current state/task authority:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
```

Runtime decomposition authority:

```text
src/game/GAME_RUNTIME_DECOMPOSITION.md
```

NavigationMap block:

```text
src/world/navigation/map/
```

Isolated measurement programs:

```text
benchmarks/navigation_map/
benchmarks/navigation_gpu/
```

The old route-wide planner is migration code. It must not be mistaken for Navigation v2 architecture authority merely because it is still compiled into the live game.

## Evidence labels

Use these labels when recording results:

- **GITHUB / MAIN** — code inspected on `main`;
- **USER TARGET-MACHINE EVIDENCE** — console/test/runtime output supplied by the user;
- **UNVERIFIED** — expected behavior or code not yet inspected/run;
- **LOCAL-ONLY** — only when unpublished local changes are explicitly confirmed or directly evidenced.

This single-branch/source-of-truth rule is part of the project Definition of Done for every future handoff.