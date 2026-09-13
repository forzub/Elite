# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.82
**WebUI architectural layer separation:** ~100%
**Localization architecture:** closure candidate

## Accepted baseline

The user locally verified the v0.10.81 architecture contracts, CMake build and runtime startup/basic workflow. Application state, orchestration, transport, backend session/persistence, EditorViewState and THREE viewport responsibilities are physically separated behind explicit module boundaries.

## v0.10.82 localization closure

Localization now has the same boundary discipline:

- `i18n/catalog.js` is a PURE catalog resolver/formatter;
- `i18n/dom.js` is the generic declarative DOM localization adapter;
- static shell text uses `data-i18n*` keys instead of a central list of element IDs;
- `effects/i18n.js` owns locale orchestration only;
- a locale selected before backend settings arrive is authoritative and is persisted when settings become available;
- contextual help and the SEMANTICS compact workflow resolve all five locales through the central catalog instead of hard-coded RU-vs-EN branches;
- backend status messages can carry `messageKey` + `messageParams`, with raw English retained only as a diagnostic/legacy fallback;
- a localization architecture contract validates catalog completeness, referenced keys and the key boundaries.

The status bar permanently exposes the language shortcut: `Ctrl+Alt+F12`.

## Testing strategy

`run_model_asset_editor_impacted.py` maps changed paths to the architecture contracts that can be affected. Use it during normal iterations to avoid rerunning unrelated contracts. A full architecture gate remains mandatory before a release/architecture closure.

## Remaining architecture debt outside WebUI/localization

Production ModelAsset binary v4 still has one independent CMake translation-unit closure task: list binary `.cpp` files directly in the target, remove facade `.cpp` aggregation includes, enforce that rule, then build/test v4 save/load.
