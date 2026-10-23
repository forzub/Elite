# Elite repository agent contract

This file defines repository-level working rules for coding/maintenance agents.

## Canonical project context

`PROJECT_STATE.md` on `main` is the ONE living project-state and
continuation file. Read it before implementing a new slice; corroborate its
claims against current code and target-machine logs. Do not reconstruct
project state from old chats or superseded summaries.

`CURRENT_STATE.md` and `CURRENT_TASK.md` are legacy dated histories, NOT
parallel living state files. Read them or stage-specific architecture contracts
only when investigating the relevant historical design. The normative
navigation architecture remains in its dedicated source contracts (for example
`src/game/navigation/NAVIGATION_V2_EXECUTION_ARCHITECTURE.md`).

## Mandatory state recording

After every **state-affecting event**, update `PROJECT_STATE.md` only,
before beginning the next implementation slice. Record changed scope,
ownership or acceptance criteria, real gate results, diagnosed root causes,
completed changes and explicit next steps. Do not append similar status
sections into `CURRENT_STATE.md`, `CURRENT_TASK.md`, or stage documents.
Update a separate architecture/contract document only when THAT document's
actual normative contract changes.

Separate source-audited changes from compiled tests and live Windows flight
evidence. Never turn a proposed fix into an accepted result just by committing
it.

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
