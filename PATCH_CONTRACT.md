# Model Asset Editor patch contract

## v0.10.66 dynamic UI localization authority

Dynamic user-facing UI added outside the frozen SOURCE / LODS / GEOMETRY / SURFACES implementation must resolve through the Model Asset Editor localization table. The active locale set is exactly `en`, `ru`, `zh-Hans`, `es`, `ja`; Russian or English fallback text must not become the effective UI of another locale. Literal `tr()` keys used by the WebUI must exist in the dictionary. The localization regression test scans controlled SEMANTICS/shared dynamic HTML/JS and user-facing prompt/status sinks for text that bypasses `tr()`. The four accepted pre-SEMANTICS stages remain fingerprint-frozen and are not reopened by localization cleanup.

## v0.10.64 pre-SEMANTICS acceptance freeze

SOURCE, LODS, GEOMETRY and SURFACES are accepted and frozen before SEMANTICS. Ordinary blank clicks in any 3D viewport clear all selection projections but must not change LOD, visibility/isolation or camera state. The Model Asset Editor localization table must remain complete for `en`, `ru`, `zh-Hans`, `es` and `ja`; visible pre-SEMANTICS UI must not introduce English-only strings.

## v0.10.64 persistent instance families

This patch is an explicit exception to the accepted SOURCE / LODS / GEOMETRY
freeze. It does **not** reopen their layout. It changes only the data semantics
required after GEOMETRY consolidation:

- a SOURCE mesh consolidated into another geometry remains a persistent logical
  identity with `representation=instance`;
- the logical identity points to one canonical `RenderGeometryDefinition` via
  `instanceOfGeometryId` and owns the exact RenderNode ids that represent it;
- alias rows display canonical mesh properties and are visibly marked INSTANCE;
- geometry-owned edits made through any family member resolve to the canonical
  mesh, so topology/surface/material/orientation properties are shared;
- RenderNode transforms remain per-instance placement data;
- converting an existing canonical family into another instance rebases the
  entire family atomically instead of splitting it;
- SOURCE scan may report that an alias source file changed, but it must never
  resurrect a duplicate geometry automatically.

The updated SOURCE/LODS/GEOMETRY fingerprints lock this approved exception.
