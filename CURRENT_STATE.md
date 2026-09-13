# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.84
**WebUI architectural layer separation:** ~100%
**Localization architecture:** acceptance-fix candidate

## Accepted baseline

v0.10.83 completed the five-locale catalog architecture, but runtime acceptance exposed three real defects that static catalog completeness did not catch: the language control was not directly available before asset selection, the axis legend did not repaint on locale changes, and SURFACES contained an undefined `surfaceGeometryVisible()` call.

## v0.10.84 acceptance fixes

- an always-available toolbar language selector works before any asset is selected; selection applies immediately and persists once settings/transport are available;
- the settings language selector also applies immediately;
- locale refresh explicitly repaints the viewport axis legend;
- dynamic connection status is no longer a declarative `data-i18n` node, so locale changes cannot overwrite a real connected/error state with `connecting`;
- connection attempts now explicitly report `Connecting to editor backend…` through the transport lifecycle;
- SURFACES visible-count rendering uses the authoritative `surfaceGeometryVisibility()` adapter; the undefined v0.10.83 call is forbidden by a dedicated runtime contract;
- high-visibility LOD provenance/version/right-panel strings seen in acceptance screenshots are translated instead of mixing English prose into non-English locales.

## Testing strategy

`run_model_asset_editor_impacted.py --base HEAD^` remains the normal iteration gate. For v0.10.84 closure also run localization + surface-runtime contracts, CMake build and the editor runtime smoke.

## Next architecture task

After localization/runtime acceptance: **finish binary v4 translation-unit architecture** — list binary `.cpp` files directly in the CMake target, remove facade `.cpp` aggregation includes, enforce the boundary contract, and build/test v4 save/load.
