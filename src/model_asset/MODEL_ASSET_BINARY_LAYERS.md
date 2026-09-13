# Model Asset Binary — layer ownership

**Status:** current v4 implementation architecture; v5 remains a draft wire target.  
**Rule:** dependencies point downward. A lower layer must never call back into the controller or public facade.

## Dependency shape

```text
ModelAssetBinary                 public compatibility facade
        |
        v
binary::controller               process / orchestration only
   |       |       |       |
   v       v       v       v
validation storage manifest_io  lod_io
                    |         |
                    v         v
              chunk registry  mesh codec
                    |
                    v
              domain chunk codecs
                    |
                    v
                  wire
```

Legacy v2/v3 codecs are isolated under the chunk/LOD compatibility boundary. They may feed migration, but current v4/v5 code must not grow new dependencies on legacy layouts.

## Layer responsibilities

### `ModelAssetBinary.cpp` — public facade

Owns no serialization logic. It preserves the existing `ModelAssetBinary` API and delegates every operation to `binary::controller`.

It must not contain file streams, wire primitives, FourCC tables, validation rules or domain codecs.

### `ModelAssetBinaryController.*` — control layer

Owns operation ordering only:

- validate before save;
- convert legacy in-memory representation when required;
- save independent LOD payloads;
- commit manifest last;
- prune stale payload files after a successful commit;
- load manifest first, then required LOD payloads;
- route legacy packages through migration.

The controller must not know how any chunk or mesh is encoded.

### `ModelAssetBinaryValidation.*`

Owns model/reference invariants. No file-system I/O, no chunk encoding and no package-path policy.

### `ModelAssetBinaryStorage.*`

Owns package filenames, LOD payload paths, package LOD count and stale-file pruning. It knows paths; it does not know bytes.

### `ModelAssetBinaryManifestIO.*`

Owns manifest file framing and generic chunk iteration. It dispatches chunks through the registry but does not contain semantic/domain encoding rules and does not perform migration/orchestration.

### `ModelAssetBinaryLodIO.*`

Owns `.elmesh` framing and RenderLOD payload load/save. Mesh-array details are delegated to the mesh codec. Semantic asset validation and package sequencing remain outside this layer.

### `ModelAssetBinaryMeshCodec.*`

Owns only `MeshLod` vertex/triangle/edge encoding. It has no file/path/controller knowledge.

### `ModelAssetBinaryChunkRegistry.cpp`

Maps FourCC ids to domain codecs. It contains dispatch metadata, not codec implementations.

### `chunks/*.cpp`

Current domain ownership is deliberately narrow:

- `MetadataChunks.cpp` — META / MATL / SIZE;
- `SemanticsChunks.cpp` — SEMN / STAT;
- `CollisionChunks.cpp` — COLL;
- `SocketChunks.cpp` — SOCK / SMET;
- `DamageChunks.cpp` — HITR / OPEN / REPR;
- `StructuralChunks.cpp` — STRL;
- `LodChunks.cpp` — LODS / LERR;
- `LegacyChunks.cpp` — v2/v3 compatibility only.

A correction to socket metadata must therefore not require editing semantic, structural or mesh code.

### `ModelAssetBinaryWire.h`

Lowest reusable v4 wire primitive: bounded reader/writer, POD/string/vector helpers and shared error assignment. No knowledge of chunks, files, controller sequencing or domain meaning.

## Current build-boundary note

The source ownership is now physically split into separate files. The root build currently names only `ModelAssetBinary.cpp`, so this iteration uses that file as a **composition-only translation unit** that includes the layer `.cpp` files. No implementation logic is allowed back into the facade.

This is intentionally temporary. The next build-isolation gate is to list every layer source independently in `EliteModelAsset` and remove all `.cpp` includes from the facade. That step improves compile/link fault isolation without changing the APIs defined here.

## v5 integration rule

v5 must enter below the controller, not through a second monolith. The expected path is separate v5 wire/container + manifest/LOD I/O implementations behind the same orchestration boundary. Domain ownership remains separated, and production v4 stays active until the v5 acceptance gate is complete.
