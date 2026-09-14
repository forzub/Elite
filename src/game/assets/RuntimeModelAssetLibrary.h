#pragma once

#include <cstdint>
#include <string>

#include "src/model_asset/ModelAsset.h"
#include "src/world/types/ObjectType.h"

namespace game::assets
{

enum class RuntimeModelAssetSourceKind : std::uint8_t
{
    LegacyObjAssembly = 0,
    CompiledBinary = 1,
};

struct RuntimeModelAssetSource
{
    RuntimeModelAssetSourceKind kind = RuntimeModelAssetSourceKind::LegacyObjAssembly;
    std::string path;
};

// Single CPU-side model entry point for game runtime code.
//
// Source selection is intentionally centralized here. Existing object types use
// AssemblyMeshLibrary by default and are lifted into the canonical ModelAsset
// schema. A type can be switched to a compiled .elmodel without changing
// downstream consumers. Both paths therefore converge on the exact same
// ModelAsset structures shared with the standalone editor.
class RuntimeModelAssetLibrary
{
public:
    static void useLegacy(ObjectType typeId);
    static void useBinary(ObjectType typeId, std::string path);

    static RuntimeModelAssetSource source(ObjectType typeId);
    static bool isLoaded(ObjectType typeId);

    static const elite::model_asset::ModelAsset& get(ObjectType typeId);

    // Bootstrap/test utility. Runtime code should choose a source before first
    // get() and keep it stable for the lifetime of active instances.
    static void clear(ObjectType typeId);
};

} // namespace game::assets
