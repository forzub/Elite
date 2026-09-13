# Elite — CURRENT TASK

**Updated:** 2026-09-13  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Editor candidate:** v0.10.77

## Immediate goal

Verify the transport-isolation candidate, then continue directly into **backend message routing + session/persistence extraction**.

## Current dependency direction

```text
UI / feature command
        ↓
command transport
        ↓
WebSocket lifecycle
        ↓
backend

backend JSON / binary
        ↓
WebSocket receive split
        ↓
binary codec / transfer manager (when binary)
        ↓
current session handler
        ↓
application / view / authored asset effects
```

The transport layer must remain free of THREE scene ownership, DOM feature rendering, and authored ModelAsset algorithms.

## v0.10.77 transport pass

Expected complete boundaries:

- WebSocket connection/reconnect is outside `model_asset_editor.html`;
- JSON command `send(...)` implementation is outside the shell;
- diagnostic queue and transport dispatch are outside the shell;
- ELWIR001 binary reader/decoder is isolated and editor-state free;
- binary asset/LOD transfer bookkeeping is isolated from DOM/THREE;
- transport runtime delivers terminal messages back to the existing session boundary without owning their semantics.

## Verification

Run:

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_editor_transport_layers.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor -j 8
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual transport smoke:

- editor connects and catalog/settings arrive;
- open an asset;
- full binary asset payload renders;
- switch to/load/reload an authored LOD;
- reconnect behavior remains functional;
- SAVE and RESTORE still work;
- diagnostic/invariant failures, if triggered, do not break command dispatch.

## Next implementation wave after verification

1. replace `handle(msg)` with a declarative type -> handler router;
2. create a session/asset acceptance adapter for metadata/full payload and resident LOD state;
3. move WORKING SAVE acknowledgement/bookkeeping into persistence effects;
4. move settings load/save/timeout acknowledgement into settings persistence effects;
5. preserve feature-specific patch handlers as feature adapters rather than absorbing them into transport;
6. add architecture contracts rejecting new message-type routing chains in the HTML shell.

After session/persistence, remaining major work is THREE/viewport runtime isolation and final EditorViewState/view-shell extraction.
