# Elite repository agent contract

This file defines repository-level working rules for coding/maintenance agents.

## Canonical project context

Before changing project behavior or project state, read:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- the authoritative document for the active stage.

For NavigationWorld v2 Stage 12 the active stage document is:
- `src/game/navigation/STAGE12_END_TO_END.md`.

Do not reconstruct current project state from old chat context when repository
Markdown and target-machine evidence are available.

## Mandatory state recording

After every **state-affecting event**, synchronize the Markdown context before
starting the next implementation slice.

State-affecting events include:
- activating a new candidate/slice;
- changing scope, ownership, architecture, or an acceptance contract;
- receiving a target-machine gate result;
- diagnosing a failed gate/root cause that changes the next action;
- accepting/closing a slice;
- changing the current task/next step.

At minimum update:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- the active stage document.

A code edit that does not change project state/contract does not require a
fictional state transition, but any meaningful gate result or change of direction
must be recorded.

## Evidence semantics

Use hashes as **verified baselines / acceptance evidence**, not as "current
HEAD" fields. A commit that updates documentation changes HEAD immediately, so
"current HEAD" stored inside the same documentation is self-invalidating.

Do not mark a runtime slice ACCEPTED without the required target-machine
evidence. Failed gates are part of project history and should be recorded with
their root cause and the correction chosen; do not erase them merely because a
later attempt passes.

## Canonical branch

`main` is the canonical development branch unless the state documents
explicitly say otherwise.
