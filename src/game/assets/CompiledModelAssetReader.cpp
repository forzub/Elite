#include "src/game/assets/CompiledModelAssetReader.h"

#include <cstdint>

#include "src/model_asset/ModelAssetBinary.h"

namespace game::assets
{
namespace
{
void setError(std::string* error, const std::string& message)
{
    if (error)
        *error = message;
}
}

bool CompiledModelAssetReader::load(
    const std::string& path,
    ObjectType expectedType,
    elite::model_asset::ModelAsset& asset,
    std::string* error)
{
    if (path.empty())
    {
        setError(error, "compiled model asset path is empty");
        return false;
    }

    std::string binaryError;
    if (!elite::model_asset::ModelAssetBinary::load(path, asset, &binaryError))
    {
        setError(error, "failed to load compiled model asset '" + path + "': " + binaryError);
        return false;
    }

    // This intentionally references the one shared format-version authority in
    // ModelAsset.h. Runtime code must never carry a private copy of this number.
    if (asset.formatVersion != elite::model_asset::ModelAssetFormatVersion)
    {
        setError(
            error,
            "compiled model asset version mismatch after shared decoder load: " +
                std::to_string(asset.formatVersion));
        return false;
    }

    if (expectedType != ObjectType::None &&
        asset.sourceObjectType != 0 &&
        asset.sourceObjectType != static_cast<std::uint16_t>(expectedType))
    {
        setError(
            error,
            "compiled model asset ObjectType mismatch for '" + path + "': expected " +
                std::to_string(static_cast<std::uint16_t>(expectedType)) + ", got " +
                std::to_string(asset.sourceObjectType));
        return false;
    }

    return true;
}

} // namespace game::assets
