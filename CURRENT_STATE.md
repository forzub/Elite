# Elite — CURRENT STATE

**Updated:** 2026-09-13  
**Authoritative working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Model Asset Editor line:** v0.10.75 candidate

This is the short hand-off state for the active branch. `CURRENT_STATE.md` + `CURRENT_TASK.md` are authoritative for the next iteration; `PROJECT_STATE.md` remains the longer historical journal.

## Accepted / frozen baseline

- SOURCE / LODS / GEOMETRY / SURFACES remain accepted unless a regression forces a targeted repair.
- SEMANTICS domain ownership is already physically decomposed into portable core + effect adapters.
- MODEL ROOT is the implicit identity root; it is not a serialized semantic node.
- semantic transform parentage and structural support/detach are different graphs and must not be conflated.
- semantic parts are asset-wide; every LOD owns independent visual RenderNodes/bindings.
- production model binary remains **v4**.
- Model Asset Binary v5 is still a **draft target**, not the production format.

## Binary subsystem

The former `ModelAssetBinary.cpp` monolith has been split by logical ownership under `src/model_asset/binary/`:

- facade: `ModelAssetBinary.cpp`;
- controller: `ModelAssetBinaryController.*`;
- validation: `ModelAssetBinaryValidation.*`;
- storage/path policy: `ModelAssetBinaryStorage.*`;
- manifest I/O: `ModelAssetBinaryManifestIO.*`;
- LOD I/O: `ModelAssetBinaryLodIO.*`;
- MeshLod codec: `ModelAssetBinaryMeshCodec.*`;
- FourCC registry: `ModelAssetBinaryChunkRegistry.cpp`;
- domain codecs: metadata, semantics/state, collision, sockets, damage/openings/repair, structural, LOD metadata, legacy compatibility;
- bounded v4 wire primitives: `ModelAssetBinaryWire.h`.

Dependency direction remains:

`facade -> controller -> validation/storage/I/O -> codecs/registry -> wire`

The dedicated binary-layer architecture contract passes on the user's normal checkout. The editor also configured, linked and launched successfully after that split. The old aggregate `check_model_asset_editor.py` is currently stale: it still expects `ManifestMagicV4` inside the former monolithic facade and therefore fails for the wrong architectural reason.

### Remaining binary isolation gate

The root CMake target still compiles the new binary implementation through a temporary composition translation unit. Final binary closure still requires:

1. list every `src/model_asset/binary/*.cpp` and `binary/chunks/*.cpp` directly in `EliteModelAsset`;
2. remove every `.cpp` include from `ModelAssetBinary.cpp`;
3. make the architecture contract reject `.cpp` aggregation;
4. rebuild and smoke-test v4 save/load.

Do not start production v5 work before that gate is closed.

## SEMANTICS workspace — v0.10.75 candidate

The visual master has now been changed toward the accepted compact workflow:

- TREE and GRAPH keep the existing two real state modes; no fake state modes were introduced;
- mode radio controls are grouped in one clean horizontal workflow bar;
- `CHECK` is in that same bar at the far right and is the final action;
- the long MODEL ROOT lead paragraph, summary/counter banners, preview prose and upper warning banners are removed from the primary TREE workspace;
- GRAPH no longer exposes its long lead/cleanup/new-link prose inline;
- long contextual explanations are retained only as compact `?` hover help where useful;
- `CLEAN LEGACY SEMANTICS` is a compact secondary action instead of a full-width explanatory banner;
- transform/graph preview controls are condensed into a bordered panel with one aligned slider row;
- existing control IDs and effect bindings are preserved, so this is a presentation/workflow change rather than a semantic state-model rewrite.

New contract:
`tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py`

Local MinGW build/UI smoke is still required for this candidate.

## Next application-level architecture direction

The user's architecture target applies to the **whole editor**, not only the binary format. After the current binary closure and SEMANTICS visual acceptance, return to whole-application decomposition with minimum blast radius:

- authoritative state;
- process/controller orchestration;
- pure domain/model logic;
- pure HTML/view builders;
- effect/DOM adapters;
- 3D/runtime adapters;
- persistence/transport;
- tests/contracts per layer.

A repair in one layer should invalidate the smallest practical surface instead of the whole editor.
