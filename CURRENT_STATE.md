# Elite — CURRENT STATE

**Updated:** 2026-09-13  
**Authoritative working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Model Asset Editor line:** v0.10.75 candidate

This file is the short hand-off state for the next chat/iteration. `PROJECT_STATE.md` remains the long chronological architecture journal; its current head is older than this working branch and is therefore not sufficient by itself to recover the active task.

## Accepted / frozen baseline

- SOURCE / LODS / GEOMETRY / SURFACES remain accepted unless a regression forces a targeted repair.
- SEMANTICS domain ownership has been physically decomposed into portable core + effect adapters.
- MODEL ROOT is the implicit identity root; it is not a serialized semantic node.
- semantic transform parentage and structural support/detach are different graphs and must not be conflated.
- semantic parts are asset-wide; every LOD owns independent visual RenderNodes/bindings.
- production model binary remains **v4**.
- Model Asset Binary v5 container architecture is a **DRAFT implementation target**, not a production format switch.

## Current binary-subsystem candidate — layer split

The former `ModelAssetBinary.cpp` monolith mixed wire primitives, all domain codecs, legacy compatibility, validation, manifest I/O, LOD I/O, filesystem policy and package orchestration in one implementation file.

The current candidate separates those responsibilities under `src/model_asset/binary/`:

- `ModelAssetBinary.cpp` — public compatibility facade only;
- `ModelAssetBinaryController.*` — operation sequencing / package orchestration;
- `ModelAssetBinaryValidation.*` — model/reference invariants;
- `ModelAssetBinaryStorage.*` — paths, LOD count and stale-file pruning;
- `ModelAssetBinaryManifestIO.*` — manifest framing and generic chunk dispatch;
- `ModelAssetBinaryLodIO.*` — `.elmesh` framing and RenderLOD load/save;
- `ModelAssetBinaryMeshCodec.*` — MeshLod arrays only;
- `ModelAssetBinaryChunkRegistry.cpp` — FourCC-to-codec dispatch only;
- `chunks/MetadataChunks.cpp` — META / MATL / SIZE;
- `chunks/SemanticsChunks.cpp` — SEMN / STAT;
- `chunks/CollisionChunks.cpp` — COLL;
- `chunks/SocketChunks.cpp` — SOCK / SMET;
- `chunks/DamageChunks.cpp` — HITR / OPEN / REPR;
- `chunks/StructuralChunks.cpp` — STRL;
- `chunks/LodChunks.cpp` — LODS / LERR;
- `chunks/LegacyChunks.cpp` — v2/v3 compatibility only;
- `ModelAssetBinaryWire.h` — lowest v4 bounded wire primitives.

Dependency direction is one-way: facade -> controller -> validation/storage/I/O -> codecs/registry -> wire. Lower layers may not depend on the controller or public facade.

Detailed ownership contract: `src/model_asset/MODEL_ASSET_BINARY_LAYERS.md`.

### Build isolation status

Source ownership is physically split, but the root CMake target still explicitly names only `ModelAssetBinary.cpp`. To keep this candidate buildable without an unrelated root-build rewrite, `ModelAssetBinary.cpp` is temporarily a **composition-only translation unit** that includes the new layer `.cpp` files and otherwise contains delegation only.

This is not the final isolation level. The next binary-architecture gate is to add every layer as an independent `EliteModelAsset` translation unit and remove all `.cpp` includes from the facade. No layer API redesign should be needed for that step.

## Current SEMANTICS state

The branch contains the accepted five-step workflow direction:

`STRUCTURE -> VISUAL BINDINGS -> KINEMATICS -> STRUCTURAL LINKS -> CHECK`

The existing SEMANTICS workspace still visually exposes too many old summaries, diagnostics and controls at once. The final visual master remains pending. Tests, counters and engineering diagnostics should move under `?`; the active stage should dominate the screen and present one obvious next action.

## Model Asset Binary v5

The v5 wire/container draft remains unchanged in principle:

- explicit little-endian encoding;
- 64-byte common header;
- 48-byte end-of-file chunk directory;
- per-chunk schema versions/flags;
- 128-bit package identity shared by manifest and LOD payloads;
- optional/required unknown-chunk policy;
- manifest string table;
- independently evolvable semantic/runtime and RenderLOD chunks.

The detailed format contract remains `src/model_asset/MODEL_ASSET_BINARY_V5_DRAFT.md`.
`ModelAssetFormatVersion` remains `4` until v5 migration/round-trip/corruption/runtime acceptance is complete.

## Immediate risk / debt

- the new layer split needs repository compile/runtime verification on the user's normal MinGW/CMake build;
- independent CMake translation-unit isolation is the next architecture gate;
- v5 implementation must be built on these boundaries rather than recreating a second monolith;
- SEMANTICS visual-master cleanup remains queued after the binary architecture gate;
- `PROJECT_STATE.md` remains historical; `CURRENT_STATE.md` + `CURRENT_TASK.md` are the authoritative hand-off pair for the active branch.
