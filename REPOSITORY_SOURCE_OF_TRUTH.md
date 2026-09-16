# Elite — Repository source of truth

**Updated:** 2026-09-16  
**Canonical repository:** `forzub/Elite`  
**Canonical development branch:** `main`

## Single-branch rule

`main` is the only canonical game-development branch.

Long-lived parallel game-development branches are prohibited. Do not create or adopt `chatgpt/*`, feature, experiment, staging, anchor, or personal branches as an alternative project baseline.

A temporary rescue branch is allowed only to preserve divergent/unpublished history long enough to inspect and merge it safely. A rescue branch is not a development branch: do not continue feature work on it, do not name it as canonical in project-state Markdown, and delete it after its history has been incorporated into `main`.

If a tool or workflow temporarily requires a non-`main` ref, merge its result back into `main` in the same work slice and return project state/documentation to `main` before handoff.

Before coding or reporting project state, verify the exact GitHub ref being inspected. For a normal development iteration the answer must be `main` / `origin/main` unless the operation is explicitly a short-lived recovery step.

## 2026-09-16 full branch reconciliation

The repository had accumulated one active divergence plus a set of old staging/anchor refs.

Primary active divergence:

```text
old main
chatgpt/mae-v01075-semantic-workflow-motion-v5
rescue/local-97500
```

These three histories were content-audited and reconciled by merge commit:

```text
9352fe7589ec109827e9633403d81bba46bdc926
```

The audit established that rescue and the remote development line carried the same current NavigationMap implementation; the remote development line additionally contained the later `NAV-V2-MAP-2` architecture-contract correction and CPU NavigationMap benchmark; old `main` contained current project-state/history plus asset-license/provenance material that had to be retained.

A subsequent branch inventory found older localization/editor staging refs. Most already pointed to ancestors of `main`. Two unique historical tips remained:

```text
62b74e4708b75b0668924c279f36949e27a2e241  localization completion/staging line
ea94d792b8ab727760eaf7c5f7fed3b3c942d12a  v0.10.75 workflow-master/motion-v5 draft
```

Their history was absorbed by merge commit:

```text
255930012d025a6d9f55c08a84f888e9b8ff8de8
```

The current accepted `main` tree was deliberately retained for that historical merge. The old side branches contained superseded/intermediate editor structures; resurrecting those files would have reverted later accepted architecture. Their commits are preserved in `main` history without replacing current code.

After these merges every non-`main` branch tip present in the repository inventory is an ancestor of `main`. The old refs are therefore cleanup-only and may be deleted without losing commit history.

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
8. Do not leave temporary branches behind after their commits are ancestors of main.
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