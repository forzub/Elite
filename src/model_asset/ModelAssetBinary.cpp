#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/binary/ModelAssetBinaryController.h"

namespace elite::model_asset
{

std::filesystem::path ModelAssetBinary::lodPayloadPath(const std::string& manifestPath, std::size_t lodIndex)
{
    return binary::controller::lodPayloadPath(manifestPath, lodIndex);
}

bool ModelAssetBinary::validate(const ModelAsset& asset, std::string* error)
{
    return binary::controller::validate(asset, error);
}

bool ModelAssetBinary::saveManifest(const std::string& path, const ModelAsset& asset, std::string* error)
{
    return binary::controller::saveManifest(path, asset, error);
}

bool ModelAssetBinary::loadManifest(
    const std::string& path,
    ModelAsset& asset,
    bool* legacyPackage,
    std::string* error)
{
    return binary::controller::loadManifest(path, asset, legacyPackage, error);
}

bool ModelAssetBinary::saveLod(
    const std::string& manifestPath,
    const ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    return binary::controller::saveLod(manifestPath, asset, lodIndex, error);
}

bool ModelAssetBinary::loadLod(
    const std::string& manifestPath,
    ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    return binary::controller::loadLod(manifestPath, asset, lodIndex, error);
}

bool ModelAssetBinary::pruneStaleLods(
    const std::string& manifestPath,
    const ModelAsset& asset,
    std::string* error)
{
    return binary::controller::pruneStaleLods(manifestPath, asset, error);
}

bool ModelAssetBinary::save(const std::string& path, const ModelAsset& asset, std::string* error)
{
    return binary::controller::save(path, asset, error);
}

bool ModelAssetBinary::load(const std::string& path, ModelAsset& asset, std::string* error)
{
    return binary::controller::load(path, asset, error);
}

} // namespace elite::model_asset
