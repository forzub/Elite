# Changelog

## Model Asset Editor v0.10.66 — dynamic UI localization fence

- Completed dynamic Model Asset Editor localization for SEMANTICS and the shared SEMANTICS/PHYSICS/DAMAGE inspectors, including runtime statuses, confirmations, prompts, socket/collision/damage controls, structural graph authoring and the 3D viewport mode selector.
- Added complete `en` / `ru` / `zh-Hans` / `es` / `ja` entries for every new key; Russian is no longer an implicit fallback path for other locales.
- Strengthened the localization architecture contract: every literal `tr()` key must exist, and uncontrolled hardcoded user-facing text is rejected in unfrozen dynamic UI. SOURCE / LODS / GEOMETRY / SURFACES fingerprints remain unchanged.
- Bumped the visible editor version to 0.10.66.

## Model Asset Editor v0.10.65 — SEMANTICS higher-LOD visual explode fix

- Fixed SEMANTICS explode for higher-LOD RenderNodes that are present/resident but still UNBOUND to the asset-wide semantic tree.
- Such RenderNodes separate as temporary visual clusters around MODEL ROOT for preview/binding work; semantic ownership is never guessed or persisted by the preview.
- Bumped the visible editor version to 0.10.65.

## Model Asset Editor v0.10.64 — pre-SEMANTICS acceptance freeze

- SOURCE, LODS, GEOMETRY and SURFACES are now the frozen accepted authoring baseline before SEMANTICS.
- Blank clicks in any 3D editor viewport clear all selection projections without changing LOD, visibility/isolation or camera state.
- Model Asset Editor localization is complete for English, Russian, Simplified Chinese, Spanish and Japanese, including the active-LOD rotation dialog.

## Model Asset Editor v0.10.64 — persistent instance families

- Added persistent SOURCE-to-canonical instance links after GEOMETRY duplicate
  consolidation.
- Removed consolidated duplicate geometry payloads instead of leaving unused
  copies behind.
- Rebased complete instance families when their former canonical mesh is itself
  consolidated into another geometry.
- SOURCE, LODS, GEOMETRY and SURFACES tables now expose alias rows as INSTANCE
  links and read effective mesh properties from the canonical geometry.
- SOURCE reconciliation preserves instance links across SAVE/RESTORE and source
  scans; changed alias source files do not silently recreate independent mesh
  geometry.
- Updated accepted SOURCE/LODS/GEOMETRY freeze fingerprints only for this
  explicitly approved instance-link semantic change.
