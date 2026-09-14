# Runtime Model Asset Ingress

**Started:** 2026-09-14  
**Status:** transition seam implemented; consumer migration pending

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

## Next migration

Do not switch a production object type yet. First migrate one read-only consumer to `RuntimeModelAssetLibrary`, verify legacy parity, then switch that same object type to compiled binary and compare geometry/bounds/semantic results. After that, remove the corresponding direct `AssemblyMeshLibrary` dependency from the migrated consumer.
