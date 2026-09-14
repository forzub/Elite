#pragma once

#include <string>

#include "src/model_asset/ModelAsset.h"
#include "src/world/types/ObjectType.h"

namespace game::assets
{

// Thin game-runtime reader over the shared editor/runtime binary codec.
// No binary structs, chunk ids or format-version constants are duplicated here.
class CompiledModelAssetReader
{
public:
    static bool load(
        const std::string& path,
        ObjectType expectedType,
        elite::model_asset::ModelAsset& asset,
        std::string* error = nullptr);
};

} // namespace game::assets
