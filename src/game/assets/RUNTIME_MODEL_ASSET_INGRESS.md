# Runtime Model Asset Ingress

**Started:** 2026-09-14  
**Status:** transition seam implemented and locally accepted; first consumer migration queued

## Authority

`src/model_asset/ModelAsset.h` is the single schema authority shared by EliteAssetEditor and game runtime. `ModelAssetFormatVersion` is defined there only. Chunk ids, binary wire layout, validation, migration and save/load logic remain under `src/model_asset` / `EliteModelAsset`; runtime code must not duplicate them.

## Runtime entry point

`EliteRuntimeModelAssets` is the CPU-side game ingress layer. `RuntimeModelAssetLibrary` selects the source per `ObjectType` and caches the resulting canonical `elite::model_asset::ModelAsset`.

```text
legacy OBJ files
      |
      v
AssemblyMeshLibrary
      |
      v
LegacyAssemblyModelAdapter ----+
                                |
                                v
                         ModelAsset
                                ^
                                |
.elmodel -> CompiledModelAssetReader
                |
                v
        ModelAssetBinary (shared)
```

The migration default is deliberately legacy. A type moves to the new format only through an explicit bootstrap choice such as:

```cpp
RuntimeModelAssetLibrary::useBinary(
    ObjectType::Station,
    "assets/compiled/models/station.elmodel"
);
```

Downstream runtime code should migrate toward `RuntimeModelAssetLibrary::get(type)` and `ModelAsset`. New systems must not add fresh dependencies on `ObjectAssembly` unless they are explicitly legacy adapters.

## Legacy adapter rule

The legacy path is one-way: old `ObjectAssembly` data is lifted into the new shared `ModelAsset` schema. The new binary path is never projected down into `ObjectAssembly`, because that would discard independent RenderLods, semantic state, sockets, collision, hit regions and structural graph data.

The adapter treats `AssemblyMeshLibrary` output as canonical game-meter geometry because the old loader has already applied authoring basis and descriptor-size normalization. It creates v4-compatible semantic/render nodes solely as a transition bridge.

## Acceptance status — ACCEPTED

Hosted architecture/build validation and the compiled-reader disk roundtrip pass. Local MinGW validation confirms:

- runtime-ingress architecture contracts: PASS;
- `EliteRuntimeModelAssets`: build PASS;
- `EliteServer`: build/link PASS;
- HTML UI resource-pack API contract: PASS;
- `EliteGame`: build/link PASS after propagating `resourcePackPath` through `HtmlUiManager -> HtmlUiBridge -> HtmlUiServer`.

The dual-source ingress seam is therefore accepted as the current runtime baseline. The `winsock2.h before windows.h` messages observed during the successful client build are warnings and are not model-ingress failures.

## Next migration

Do not globally switch production object types. The next asset step is deliberately narrow:

1. choose one CPU/read-only consumer that directly reads `AssemblyMeshLibrary`;
2. change that consumer to `RuntimeModelAssetLibrary::get(type)` while keeping the source `LegacyObjAssembly`;
3. verify no behavior/geometry change;
4. switch only the same object type to `CompiledBinary`;
5. compare bounds, RenderLods, semantic bindings and all metadata actually consumed by that runtime path;
6. remove that consumer's direct `AssemblyMeshLibrary` dependency only after parity.

The goal is to make downstream runtime code source-agnostic. It should consume canonical `ModelAsset` and not care whether the asset originated from legacy OBJ or `.elmodel`.

## First consumer (2026-10-31): station rendering

`render::geometry::ModelAssetGpuLibrary` renders `ObjectType::Station` from
the metric BUILD package `assets/compiled/models/station/station.elmodel`
(all components, all RenderLods). The selection is
`src/model_asset/ModelAssetPresentation` (representation ladder,
per-component screen-space LOD, module HLOD, interiors). The asset is
placed with the descriptor mesh basis and logical size, i.e. in the same
frame the legacy assembly, its hit volumes and docking ports use. Collision,
hits, damage and docking still read the legacy assembly; moving them to the
asset is a separate gated step. A missing / non-metric package falls back to
the legacy path; `ELITE_LEGACY_MODEL_ASSETS=1` forces it.
