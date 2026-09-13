# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.79
**Whole-editor separation:** ~90%

## Immediate goal

Verify the v0.10.79 viewport-core split, then finish the remaining renderer adapters and extract `EditorViewState`.

## v0.10.79 viewport-core pass

Expected complete boundaries:

- scene/bootstrap lifecycle is outside `model_asset_editor.html`;
- renderer resize/frame loop and world axes are viewport runtime responsibilities;
- raycaster and camera-fit behavior are viewport runtime responsibilities;
- geometry cache and THREE BufferGeometry/material creation are outside the shell;
- `rebuildScene` and visibility orchestration are outside the shell;
- LOD/SEMANTICS keep their domain calculations and call renderer effects through a narrow scene bridge;
- transport/session/application modules remain independent of THREE.

## Verification

Run:

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_editor_transport_layers.py
python tests/architecture_contracts/check_model_asset_editor_session_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_layers.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual smoke should include open asset, LOD switching, SOURCE/WORKING viewport mode, fit view, geometry visibility/isolation, SURFACES material preview, SEMANTICS motion preview, collision/socket visibility, SAVE/RESTORE and reconnect.

## Next decomposition wave — renderer adapters

1. extract edge and normal overlays;
2. extract collision / structural proxy / socket rendering;
3. extract socket-camera preview and viewport picking wiring;
4. keep PHYSICS / DAMAGE / SEMANTICS calculations in their feature domains and expose only render plans to viewport adapters.

Expected progress after renderer-adapter extraction: **95–96%**.

## Final WebUI closure

- move `EditorViewState`, visibility adapters and invariant scheduling out of HTML;
- reduce `model_asset_editor.html` to markup, imports, composition/bootstrap and minimal DOM binding;
- add a final shell contract rejecting application/session/transport/viewport implementations in HTML.

Expected WebUI architecture completion after this closure: **~100%**.

## Parallel binary debt

After WebUI closure, finish the independent-CMake-translation-unit gate for the production v4 binary subsystem. This is tracked separately from the WebUI separation percentage.
