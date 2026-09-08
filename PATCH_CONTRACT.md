# Model Asset Editor patch contract

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
