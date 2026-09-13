# v0.10.75 runtime acceptance

1. Open `SOURCE → LODS → GEOMETRY → SURFACES → SEMANTICS → PHYSICS → DAMAGE → VALIDATE → BUILD`.
2. Exactly one compact numbered workflow master must appear for each active stage.
3. Active top-level wizard tab (`.current`) must be visibly brighter than inactive tabs.
4. Workflow step buttons must scroll/focus the corresponding working block; they must not mutate asset state by themselves.
5. Long explanatory copy should not occupy the work area; round `?` controls must expose it. Warnings/errors/status needed for action remain visible.
6. In SEMANTICS, switching TREE ↔ STRUCTURAL GRAPH must replace the process sequence without changing semantic data.
7. Legacy cleanup remains available but visually secondary.
8. Existing SEMANTICS selection/binding/motion/structural actions must behave exactly as before.
9. Fresh log: no new `ReferenceError`, `TypeError`, websocket exception or `state_invariant` diagnostic.
10. Binary save/load behavior remains v4; v5 files are design/schema only in this candidate.
