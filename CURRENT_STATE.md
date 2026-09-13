# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.83
**WebUI architectural layer separation:** ~100%
**Localization architecture:** closed candidate

## Accepted baseline

The user locally verified v0.10.81 architecture contracts, CMake build and runtime startup/basic workflow. Application state, orchestration, transport, backend session/persistence, EditorViewState and THREE viewport responsibilities are physically separated behind explicit module boundaries.

## v0.10.83 localization completion

Localization is a first-class architectural boundary:

- one five-locale catalog is authoritative;
- `i18n/catalog.js` is the PURE resolver/formatter;
- `i18n/dom.js` handles declarative static DOM translation;
- `effects/i18n.js` owns locale orchestration and document-title refresh;
- a language selected before asset/settings arrival remains authoritative and persists later;
- static shell, contextual help, SEMANTICS workflow, status bar, geometry statistics, axis legend and storage/read-model labels resolve through localization keys;
- backend protocol supports stable `messageKey` + `messageParams`; legacy/raw diagnostic messages are allowed to fall back to English exactly as the product fallback policy requires;
- the localization contract rejects full English-prose copies inside populated non-English locale values and rejects known direct high-visibility UI bypasses.

English fallback is intentional only when a translation is genuinely absent. A populated locale entry that merely copies English prose is treated as a localization defect.

## Testing strategy

`run_model_asset_editor_impacted.py --base HEAD^` runs contracts selected by changed paths during normal iteration. The full architecture gate remains mandatory for closure/release.

## Next architecture task

After localization acceptance: **finish binary v4 translation-unit architecture** — list binary `.cpp` files directly in the CMake target, remove facade `.cpp` aggregation includes, enforce the boundary contract, and build/test v4 save/load.
