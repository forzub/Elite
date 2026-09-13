# Elite — CURRENT TASK

**Updated:** 2026-09-13  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`

## Goal

Turn SEMANTICS from a technically capable but visually noisy workspace into a guided production workflow, while beginning Model Asset Binary v5 behind an explicit draft boundary.

## Work order

1. **SEMANTICS visual master**
   - visually enforce `STRUCTURE -> VISUAL BINDINGS -> KINEMATICS -> STRUCTURAL LINKS -> CHECK`;
   - show only controls relevant to the active step;
   - move tests/technical diagnostics/counters under `?`;
   - make the active top-level wizard tab slightly lighter;
   - keep repair/advanced tools contextual instead of permanently open.

2. **Binary v5 implementation behind draft/opt-in code**
   - implement explicit LE read/write primitives;
   - implement common header + chunk directory parsing/validation;
   - add package-id matching;
   - implement `STRS`, then manifest chunks, then `LINF/RGRF/MESH`;
   - keep v4 production reader/writer unchanged until acceptance.

3. **Acceptance**
   - v5 manifest and independent LOD round trips;
   - malformed/truncated/overlap tests;
   - unknown optional vs required chunk behavior;
   - stale `.elmesh` package-id rejection;
   - v4 migration coverage;
   - Cobra + Zenith runtime/editor smoke.

## Definition of done for the next SEMANTICS UI iteration

A user opening SEMANTICS can answer, without reading engineering diagnostics:

- where am I in the process;
- what do I need to do now;
- what button performs that action;
- what is the next stage;
- where to open technical details if something fails.

If the screen cannot answer those five questions visually, the UI iteration is not accepted.
