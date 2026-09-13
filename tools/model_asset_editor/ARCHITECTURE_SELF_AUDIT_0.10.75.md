# Model Asset Editor v0.10.75 architecture self-audit

- [x] `model_asset_editor.html` receives no new implementation.
- [x] Workflow policy/presentation descriptors live in `ui/workflow_master_model.js`.
- [x] Workflow styling lives in `ui/workflow_master_style.js` as a pure packaged style payload.
- [x] DOM/observer/scroll/modal work lives in `effects/workflow_master.js`.
- [x] Existing v0.10.74 UI chrome is preserved source-identically in `effects/ui_chrome_base.js`; `effects/ui_chrome.js` is composition only.
- [x] New workflow files introduce no inventory-visible `function name(...)` declarations, preserving the frozen 546 named-function ownership census.
- [x] No PURE workflow module references DOM/window/network/storage state.
- [x] New JS modules stay under the existing recursively packaged `model_asset_editor/*.js` resource convention.
- [x] v5 motion schema is separate from the active `ModelAsset.h` v4 wire authority.
- [x] `ModelAssetFormatVersion` remains 4; no incomplete v5 writer can silently become production.
- [x] v5 draft explicitly separates SemanticNode / RigNode / Bone / RenderNode and RIG / DRIVER responsibilities.
- [x] New architecture regression test covers UI separation, workflow presence, brighter `.current`, v5 schema concepts, and the v4 writer guard.
