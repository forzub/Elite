# Model Asset Editor — production asset maintenance

After the first BUILD, the compiled v4 asset is the editable working authority. Source OBJ/MTL files are inputs for explicit maintenance operations, not a reason to reconstruct the full asset.

## Folder source scan

For folder-authoritative assets:

- `LOD<N>/*.obj` are ordinary source parts;
- `LOD<N>/variants/**/*.obj` are hidden replacement variants;
- OBJ + referenced/sibling MTL bytes form one editor source revision fingerprint.

`SCAN SOURCE CHANGES` is read-only. It reports changed, new, missing and previously-untracked source revisions. Missing files are never interpreted as an automatic delete.

## Local replacement

Replacing one ordinary part swaps only its `RenderGeometryDefinition::mesh` payload. The existing geometry id, RenderNodes, semantic binding, semantic hierarchy, joints, collision/physics, sockets and DAMAGE data remain intact. The component receives only the maintenance debt that genuinely depends on changed geometry: PREPARE, SURFACES and generated LODs when present.

Adding a new ordinary part creates geometry + one zero-transform RenderNode in the current authored LOD. The editor does not invent semantic ownership, collision or physics. The new part therefore starts with PREPARE/SURFACES/SEMANTICS debt.

## Variants

A newly discovered `variants/**/*.obj` remains hidden geometry with no normal RenderNode. Existing opaque variant identity and replacement compatibility survive a source replacement. A new variant starts with PREPARE/SURFACES plus REPLACEMENT debt; choosing at least one compatible base visual resolves REPLACEMENT.

## Selected derived LODs

`REGENERATE DERIVED LODS` on a selected LOD0 part or variant updates only generated LOD documents (`sourceKind=generated`, `generatedFromLod=0`). It uses the target LOD's already-authored relative geometric error. Manual LOD documents are never overwritten automatically and keep the local LODS debt until reviewed manually.

## Publication rule

Component maintenance debt is editor-only authoring state and never enters the runtime `.elmodel/.elmesh` format. VALIDATE and BUILD fail while any local debt remains. Unrelated components and their completed wizard work are not reopened merely because one source mesh changed.
