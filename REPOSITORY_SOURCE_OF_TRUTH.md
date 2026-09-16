# Elite — Repository source of truth

**Updated:** 2026-09-16  
**Canonical repository:** `forzub/Elite`  
**Canonical working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`

## Rule

For the current Navigation v2 work, the GitHub branch named above is the source of truth for code, build files, tests and project-state Markdown.

Do **not** assume that a developer workstation contains a newer or unpublished implementation merely because a file or test is not visible on `main`.

Do **not** use the repository default branch as a proxy for the current project baseline when the current-state documents name another branch.

A local working tree may be used as runtime evidence only when the user explicitly supplies its output. It is not a source of unseen code unless the user explicitly says there are local-only changes and provides or publishes them.

## Failure that caused the 2026-09-16 misread

The project was initially inspected through the repository default branch (`main`). That branch was behind the active working branch and therefore did not contain the current Navigation v2 implementation, including the NavigationMap block, runtime-library decomposition and benchmark work.

Because those files were absent on `main`, the missing GitHub code was incorrectly interpreted as newer `LOCAL / UNPUSHED` work on the user's disk. That conclusion was wrong.

The correct procedure would have been:

1. read `CURRENT_STATE.md` / `CURRENT_TASK.md` first;
2. take their declared branch as the current project baseline;
3. resolve and inspect that branch directly;
4. compare it with `main` only as historical/divergence information;
5. treat local console output as validation evidence, not as proof of a separate code version.

At the time of correction, `chatgpt/mae-v01075-semantic-workflow-motion-v5` was hundreds of commits ahead of and diverged from `main`, so inspecting `main` materially misrepresented the project state.

## Mandatory verification sequence before coding or reporting state

For every Navigation v2 iteration:

```text
1. Read CURRENT_STATE.md and CURRENT_TASK.md from the declared branch.
2. Resolve the branch HEAD on GitHub.
3. Inspect code/CMake/tests from that exact branch/ref.
4. Verify any named implementation exists on that branch before calling it local-only or missing.
5. If branch/default-branch content conflicts, trust the explicitly declared current branch and report the divergence.
6. If user-provided runtime output conflicts with repository inspection, treat it as evidence to investigate, not as proof that unseen local code exists.
```

Never invent a `LOCAL / UNPUSHED` state merely to explain a repository mismatch.

## Navigation v2 current source hierarchy

Architecture authority:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

Current state/task authority:

```text
CURRENT_STATE.md
CURRENT_TASK.md
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

Use these labels explicitly when recording results:

- **GITHUB / CANONICAL BRANCH** — code inspected on the branch named above;
- **USER TARGET-MACHINE EVIDENCE** — console/test/runtime output supplied by the user;
- **UNVERIFIED** — expected behavior or code not yet inspected/run;
- **LOCAL-ONLY** — only when the user explicitly confirms unpublished local changes or such changes are directly evidenced.

This procedure is part of the project Definition of Done for future handoffs.