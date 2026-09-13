# Model Asset Binary v5 — DRAFT wire contract

**Status:** DRAFT / implementation target, not a frozen compatibility promise.  
**Production format remains v4.** This document defines the next binary package shape so implementation can start without silently turning the first design into permanent ABI.

## 1. Design goals

v5 keeps the ownership model already established by v4:

- `.elmodel` is the lightweight manifest and owns semantic/runtime asset state.
- every `.elmesh` owns one independently loadable RenderLOD payload.
- manifest and LOD payloads remain independently readable/writable so runtime streaming does not require loading all geometry.

The v5 change is the **wire container**, not a redesign of `ModelAsset`.

The container must provide:

1. deterministic, explicit little-endian encoding;
2. direct chunk lookup without scanning every previous chunk;
3. per-chunk schema versions so one subsystem can evolve without bumping the whole package;
4. a package identity shared by the manifest and all `.elmesh` files, so stale/mixed LOD payloads are rejected;
5. forward-compatible optional chunks;
6. room for checksums and compression without making either mandatory in the first implementation;
7. transactional file replacement (`temp -> validate -> atomic rename`) when the writer is implemented.

## 2. Common file header

Both `.elmodel` and `.elmesh` use a fixed **64-byte** header. All integral fields are serialized little-endian, field-by-field. Native C++ struct bytes are never written directly.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | magic: `ELMDL005` or `ELMSH005` |
| 8 | 2 | `headerBytes` = 64 |
| 10 | 2 | `versionMajor` = 5 |
| 12 | 2 | `versionMinor` |
| 14 | 2 | `directoryEntryBytes` = 48 |
| 16 | 4 | endian tag = `0x01020304` |
| 20 | 4 | header flags |
| 24 | 4 | directory entry count |
| 28 | 4 | reserved = 0 |
| 32 | 8 | directory offset |
| 40 | 8 | complete file size |
| 48 | 16 | package id |

The directory is written at the **end** of the file. A writer can therefore stream payload chunks, append the directory, then patch the fixed header once.

### Package identity

The same 128-bit `packageId` must be present in the manifest and every LOD payload belonging to it. A loader must reject an `.elmesh` whose package id does not match the loaded manifest even if its filename and LOD index look correct.

The exact identity-generation policy is deliberately **not frozen yet**. It can later be a reproducible content-derived identity or a generated package/revision identity. What is frozen for the draft is the 16-byte comparison field and the mismatch rejection rule.

## 3. Chunk directory

Each directory entry is **48 bytes**:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | FourCC chunk id |
| 4 | 2 | chunk schema version |
| 6 | 2 | chunk flags |
| 8 | 8 | payload offset |
| 16 | 8 | stored byte count |
| 24 | 8 | decoded byte count |
| 32 | 8 | checksum |
| 40 | 4 | logical index |
| 44 | 4 | reserved = 0 |

`logicalIndex = 0xffffffff` means “not indexed”. Repeated chunk families such as `MESH` may use a real logical index.

Initial flags:

- `Required`: an unknown chunk with this flag makes the file unsupported.
- `Compressed`: stored bytes are encoded; the algorithm is defined by that chunk schema revision, not assumed globally.
- `Streamable`: the payload is independently useful and safe to range-read.
- `EditorOnly`: runtime may ignore it.

Unknown **optional** chunks are skipped. Unknown **required** chunks fail loading with a precise error.

The checksum algorithm is intentionally left open in this draft; `checksum == 0` means no checksum declared until the implementation patch chooses the first algorithm.

## 4. String references

v5 introduces a manifest string table chunk `STRS`. UTF-8 strings are interned once and referenced by `u32` indexes. `0xffffffff` means “no string”.

This is a storage optimization and a deterministic reference mechanism, not a gameplay identity system. Stable model ids remain explicit model data; they are not replaced by opaque string hashes.

## 5. Manifest chunks (`.elmodel`)

The first implementation should preserve v4 ownership and map it into independently versioned chunks:

| Chunk | Responsibility |
|---|---|
| `STRS` | UTF-8 string table |
| `META` | asset metadata |
| `MATL` | materials |
| `SEMN` | semantic nodes / transform forest / joint data |
| `STAT` | state variants |
| `RBOD` | rigid-body/runtime physics metadata |
| `COLL` | collision volumes |
| `SOCK` | sockets |
| `SMET` | socket metadata |
| `HITR` | hit regions |
| `OPEN` | openings |
| `REPR` | repair targets |
| `STRL` | structural graph links |
| `LODS` | authored RenderLOD index/metadata |
| `LERR` | LOD generation/import errors |

The exact payload schema inside each chunk remains independently reviewable. A later change to `SEMN` does not imply rewriting the `MESH` schema.

`SEMN` and `STRL` remain separate by design: the semantic transform forest answers transform inheritance/kinematics; the structural graph answers physical support/detach/break topology.

## 6. RenderLOD chunks (`.elmesh`)

An `.elmesh` uses the same common header and package identity.

Initial chunk families:

| Chunk | Responsibility |
|---|---|
| `LINF` | LOD index, source/generated provenance, bounds and payload metadata |
| `RGRF` | RenderNode graph / enablement / semantic-owner references |
| `MESH` | one geometry payload; repeatable, keyed by `logicalIndex` |

Splitting `RGRF` from repeated `MESH` payloads is intentional. It leaves room for later GPU-ready mesh encodings, mesh compression, BVH/meshlet data or partial geometry streaming without changing semantic/runtime chunks.

The first reader/writer does **not** need to implement every future extension. It needs to make their addition possible without another container rewrite.

## 7. Reader validation order

Before exposing data to `ModelAsset`, a v5 reader must validate, in order:

1. magic, major version, header size and endian tag;
2. declared file size and directory bounds;
3. every directory entry range, overflow and overlap with forbidden header/directory space;
4. singleton chunk uniqueness;
5. supported schema revisions for all required chunks;
6. checksum when a chunk declares one;
7. package id equality between manifest and LOD;
8. requested/declared LOD index equality for `.elmesh`;
9. model-level semantic/index/reference validation after decoding.

Error messages must name the file, FourCC and logical index where applicable. “Failed to load model” is not an acceptable diagnostic.

## 8. Writer transaction

The planned writer sequence is:

1. validate the in-memory model;
2. write each changed LOD to a temporary file using one package id;
3. close and re-open/validate the temporary payload;
4. atomically replace that LOD file;
5. write/validate/replace the manifest **last**.

This preserves the v4 rule that the manifest is the commit point. A crash may leave an unreferenced new payload, but must not make a manifest point to half-written bytes.

## 9. Deliberately unresolved before implementation

These are **not** compatibility promises yet:

- package-id generation policy;
- checksum algorithm;
- compression algorithm(s);
- exact per-chunk payload field order beyond the common container;
- whether `STRS` is mandatory for every file or manifest-only;
- optional GPU/runtime acceleration chunks;
- exact minor-version compatibility rules.

Those decisions should be made against actual runtime/editor needs and tests. v5 is a development target, not a stone tablet.

## 10. Acceptance gate for switching production to v5

`ModelAssetFormatVersion` must remain `4` until all of the following exist:

- explicit little-endian reader/writer helpers;
- v5 manifest round-trip tests;
- independent v5 LOD round-trip tests;
- stale/mismatched package-id rejection;
- unknown optional chunk skip + unknown required chunk rejection;
- corruption/bounds/truncation tests;
- v4 -> in-memory -> v5 migration coverage;
- runtime load/streaming smoke on at least Cobra and Zenith assets.

Only a later reviewed patch may change the production format constant to `5`.
