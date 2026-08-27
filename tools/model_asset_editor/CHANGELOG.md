# Elite Model Asset Editor changelog

## 0.4.0 — independently editable LOD package / `.elmodel` v3

- `.elmodel` is a lightweight semantic manifest; heavy geometry lives in one
  `<asset>.lodN.elmesh` file per LOD level.
- LOD files are first-class editor documents with independent `LOADED`,
  `UNLOADED`, `CLEAN` and `DIRTY` state.
- `Load`, `Reload`, `Unload` and `Save` operate on one LOD only. `Save manifest`
  writes semantic metadata only. `Save all` writes only dirty/missing package
  members and does not rewrite clean LOD files.
- v3 opens the manifest plus LOD0 by default; other LOD payloads remain off disk
  until explicitly loaded or an operation genuinely requires all LODs.
- External `.elmesh` entries bind to stable `GeometryDefinition::id`, not the
  transient `G0/G1/...` UI vector index.
- Structural operations that must touch every LOD (basis conversion, breaking
  an instance, deleting geometry definitions) load all LODs first and mark each
  affected payload dirty.
- Existing monolithic `.elmodel` v2 files remain readable for migration. Saving
  writes v3 beside the untouched v2 source binary.
- Editor version and asset-format version are visible in the window/UI.
- Compiled `.elmodel`/`.elmesh` packages are generated artifacts and are kept
  out of Git; editor source, tests and documentation are intended to be tracked.

## 0.2.x — model workbench / native import / instancing

- Native OBJ import retaining topology, materials, normals and UV data.
- Shared GeometryDefinition + Node instancing.
- Collision, sockets, joints and detached rigid-body metadata.
- Baked duplicate -> instance consolidation with LOD0 rigid fitting.
- Operation status/progress feedback and storage diagnostics.
