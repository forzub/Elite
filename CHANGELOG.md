# Changelog

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
