#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/binary/ModelAssetBinaryController.h"

// Composition-only aggregation for the current explicit CMake source list.
// Logical ownership is physically split below; this file remains the only
// EliteModelAsset source known to the existing target until the next build-list
// isolation step converts the layers into independent translation units.
#include "src/model_asset/binary/ModelAssetBinaryMeshCodec.cpp"
#include "src/model_asset/binary/chunks/MetadataChunks.cpp"
#include "src/model_asset/binary/chunks/SemanticsChunks.cpp"
#include "src/model_asset/binary/chunks/CollisionChunks.cpp"
#include "src/model_asset/binary/chunks/SocketChunks.cpp"
#include "src/model_asset/binary/chunks/DamageChunks.cpp"
#include "src/model_asset/binary/chunks/StructuralChunks.cpp"
#include "src/model_asset/binary/chunks/LodChunks.cpp"
#include "src/model_asset/binary/chunks/LegacyChunks.cpp"
#include "src/model_asset/binary/ModelAssetBinaryChunkRegistry.cpp"
#include "src/model_asset/binary/ModelAssetBinaryValidation.cpp"
#include "src/model_asset/binary/ModelAssetBinaryStorage.cpp"
#include "src/model_asset/binary/ModelAssetBinaryManifestIO.cpp"
#include "src/model_asset/binary/ModelAssetBinaryLodIO.cpp"
#include "src/model_asset/binary/ModelAssetBinaryController.cpp"

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
