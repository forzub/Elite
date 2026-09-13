# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.80
**Whole-editor separation:** ~96%

## Verified baseline entering this pass

User reported the complete v0.10.78 test set PASS and local build success. The v0.10.79 viewport-core extraction passed GitHub architecture and JavaScript syntax validation.

## v0.10.80 renderer-adapter candidate

Viewport ownership is now split into explicit renderer adapters:

- `viewport/overlays.js` — edge/normal overlay creation, disposal and edge-hit mutation request;
- `viewport/attachments.js` — collision volumes, structural proxies, socket markers, semantic transform refresh and socket camera preview;
- `viewport/picking.js` — raycast decision/effect routing for pivots, edges, semantic graph, sockets, collisions and meshes;
- existing `viewport/runtime.js`, `geometry.js`, `scene.js` remain the viewport core.

The HTML shell no longer implements these renderer algorithms. Feature decisions and backend mutations are injected through callbacks rather than making renderer adapters own transport.

## Remaining WebUI architecture work

The principal remaining monolithic block is view-state ownership:

- `ProjectedVisibilitySet`;
- `EditorVisibilityMapAdapter`;
- `HiddenRenderNodeAdapter`;
- `EditorViewState`;
- view invariant scheduling/projection glue;
- residual composition/bootstrap cleanup in the HTML shell.

After extracting that block and adding a final shell contract, WebUI decomposition is expected to be effectively complete.

## Separate binary debt

Production binary remains v4. The CMake independent-translation-unit cleanup is tracked separately and does not reduce the WebUI separation percentage.
