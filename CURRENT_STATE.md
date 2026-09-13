# Elite — CURRENT STATE

**Updated:** 2026-09-13  
**Authoritative working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Model Asset Editor line:** v0.10.75 candidate

This file is the short hand-off state for the next chat/iteration. `PROJECT_STATE.md` remains the long chronological architecture journal; its current top entry still reflects v0.10.73 and is therefore **not sufficient by itself** to recover the active task.

## Accepted / frozen baseline

- SOURCE / LODS / GEOMETRY / SURFACES remain accepted unless a regression forces a targeted repair.
- SEMANTICS domain ownership has been physically decomposed into portable core + effect adapters.
- MODEL ROOT is the implicit identity root; it is not a serialized semantic node.
- semantic transform parentage and structural support/detach are different graphs and must not be conflated.
- semantic parts are asset-wide; every LOD owns independent visual RenderNodes/bindings.
- production model binary remains **v4**.

## Current SEMANTICS state

The latest branch contains a PURE five-step workflow descriptor:

`STRUCTURE -> VISUAL BINDINGS -> KINEMATICS -> STRUCTURAL LINKS -> CHECK`

That is the intended user process, but the existing SEMANTICS workspace still visually exposes too many old summaries, diagnostics and controls at once. Functionality is ahead of UX.

Accepted UX direction:

- the current wizard tab needs a slightly lighter active background;
- the screen must present one obvious process and one obvious next action;
- each SEMANTICS step gets a visible master/workflow stage instead of a flat wall of controls;
- tests, counters, health summaries and other engineering diagnostics move under `?` help/details;
- advanced/repair controls are shown only where the current step needs them;
- button names must describe the action, not the underlying implementation;
- no technical panel may compete visually with the current user task.

The five-step model is therefore **direction accepted / visual integration still pending**.

## Model Asset Binary v5

v5 is now a **DRAFT implementation target**, not the active serializer.

The draft keeps v4 ownership (`.elmodel` semantic/runtime manifest + independent `.elmesh` RenderLODs) but replaces the early sequential/native-POD wire container with:

- explicit little-endian field encoding;
- fixed 64-byte common header;
- 48-byte end-of-file chunk directory;
- per-chunk schema versions/flags;
- 128-bit package identity shared by manifest and LOD payloads;
- optional/required unknown-chunk policy;
- manifest string table;
- independently evolvable semantic/runtime and RenderLOD chunks.

The detailed contract lives in `src/model_asset/MODEL_ASSET_BINARY_V5_DRAFT.md`.
`ModelAssetFormatVersion` stays at `4` until the migration/round-trip/corruption/runtime acceptance gate is complete.

## Immediate risk / debt

- `PROJECT_STATE.md` is chronologically useful but its head is stale relative to this branch.
- SEMANTICS workflow model exists but is not yet the final visual master.
- do not switch the production serializer to v5 merely because the draft structures compile.
- the v5 package-id generation/checksum/compression policies are deliberately unresolved until implementation evidence exists.
