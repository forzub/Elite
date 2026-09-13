# Elite — CURRENT STATE

**Updated:** 2026-09-13  
**Authoritative working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Model Asset Editor line:** v0.10.77 candidate

`CURRENT_STATE.md` + `CURRENT_TASK.md` are authoritative for the active iteration.

## Accepted / frozen baseline

- SOURCE / LODS / GEOMETRY / SURFACES remain accepted unless a regression requires a targeted repair.
- Cleaned SEMANTICS workflow remains accepted.
- MODEL ROOT is implicit identity and is not serialized as a semantic node.
- Semantic parts are asset-wide; each LOD owns its own render geometry / RenderNodes / bindings.
- PHYSICS is Model Asset authoring metadata. Runtime world simulation remains outside this editor pass.
- Production model binary remains v4; v5 remains a draft target.

## Application architecture

Authoritative application control remains:

`action -> ApplicationController -> reducer/store -> selector`

`EditorViewState` remains authoritative for active/resident LOD, viewport selection, visibility and isolation.

The v0.10.76 orchestration pass is retained:

- dedicated application bootstrap;
- workflow transition effect adapter;
- declarative stage renderer registry;
- no `setWizardStage(...)` implementation in the HTML shell;
- no top-level stage `if(stage===...)` dispatch in the HTML shell.

## v0.10.77 transport isolation candidate

Transport responsibilities are now split under:

`src/assets/webui/model_asset_editor/transport/`

- `diagnostics.js` — bounded diagnostic queue and global JS error/rejection capture;
- `commands.js` — JSON command dispatch and command-status behavior;
- `binary_wire.js` — pure ELWIR001 reader/decoder;
- `binary_transfers.js` — asset/LOD binary transfer bookkeeping, payload application and delta reuse;
- `websocket.js` — WebSocket lifecycle, JSON/binary receive split and reconnect;
- `runtime.js` — transport composition root connecting those adapters to the existing session handler.

The HTML shell no longer owns:

- `new WebSocket(...)` lifecycle/reconnect;
- `send(...)` implementation;
- diagnostic queue/flush implementation;
- `EditorWireReader` / ELWIR001 binary decoder;
- binary transfer map/bookkeeping.

The transport layer intentionally does not own THREE, DOM rendering, authored asset algorithms, or EditorViewState rules.

Architecture contract:
`tests/architecture_contracts/check_model_asset_editor_transport_layers.py`

## Remaining whole-editor decomposition

The next boundary is now the large backend/session handler still in the HTML shell:

1. replace monolithic `handle(msg)` with a declarative backend message router;
2. extract asset metadata/full-payload acceptance and resident LOD merge/reuse effects;
3. extract SAVE / RESTORE / WORKING-save bookkeeping;
4. extract settings persistence / acknowledgement / timeout effects;
5. then isolate THREE / viewport runtime;
6. physically extract the remaining EditorViewState/view adapters;
7. reduce `model_asset_editor.html` toward static shell + bootstrap only.

## Binary subsystem parallel debt

The logical binary split is accepted, but final CMake translation-unit closure remains separate work: register binary `.cpp` files directly, remove `.cpp` aggregation from the facade, strengthen the contract, then rebuild/smoke-test production v4 save/load.
