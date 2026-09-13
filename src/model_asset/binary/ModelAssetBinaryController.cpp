#include "src/model_asset/binary/ModelAssetBinaryController.h"

#include "src/model_asset/ModelAssetMigration.h"
#include "src/model_asset/binary/ModelAssetBinaryLodIO.h"
#include "src/model_asset/binary/ModelAssetBinaryManifestIO.h"
#include "src/model_asset/binary/ModelAssetBinaryStorage.h"
#include "src/model_asset/binary/ModelAssetBinaryValidation.h"
#include "src/model_asset/binary/ModelAssetBinaryWire.h"

#include <exception>
#include <filesystem>
#include <set>
#include <utility>

namespace elite::model_asset::binary::controller
{

std::filesystem::path lodPayloadPath(const std::string& manifestPath, std::size_t lodIndex)
{
    return packageLodPayloadPath(std::filesystem::path(manifestPath), lodIndex);
}

bool validate(const ModelAsset& asset, std::string* error)
{
    if (error) error->clear();
    return validateSemanticAsset(asset, error);
}

bool saveManifest(const std::string& path, const ModelAsset& asset, std::string* error)
{
    if (error) error->clear();
    try
    {
        if (asset.renderLods.empty())
        {
            setError(error, "v4 asset has no independent render LODs");
            return false;
        }
        if (!validateSemanticAsset(asset, error)) return false;
        const std::filesystem::path manifestPath(path);
        if (manifestPath.has_parent_path())
            std::filesystem::create_directories(manifestPath.parent_path());
        return writeManifestV4(manifestPath, asset, error);
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

bool loadManifest(
    const std::string& path,
    ModelAsset& asset,
    bool* legacyPackage,
    std::string* error)
{
    if (error) error->clear();
    if (legacyPackage) *legacyPackage = false;
    return readManifestPackage(std::filesystem::path(path), asset, legacyPackage, error);
}

bool saveLod(
    const std::string& manifestPath,
    const ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    if (error) error->clear();
    try
    {
        const auto path = packageLodPayloadPath(std::filesystem::path(manifestPath), lodIndex);
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        return writeLodPayload(path, asset, lodIndex, error);
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

bool loadLod(
    const std::string& manifestPath,
    ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    if (error) error->clear();
    return readLodPayload(
        packageLodPayloadPath(std::filesystem::path(manifestPath), lodIndex),
        asset,
        lodIndex,
        error);
}

bool pruneStaleLods(
    const std::string& manifestPath,
    const ModelAsset& asset,
    std::string* error)
{
    if (error) error->clear();
    try
    {
        const std::filesystem::path path(manifestPath);
        std::set<std::filesystem::path> keep;
        for (std::size_t lodIndex = 0; lodIndex < packageLodCount(asset); ++lodIndex)
            keep.insert(packageLodPayloadPath(path, lodIndex));
        removeStaleLodPayloadFiles(path, keep);
        return true;
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

bool save(const std::string& path, const ModelAsset& asset, std::string* error)
{
    if (error) error->clear();
    ModelAsset working = asset;
    if (working.renderLods.empty())
        buildIndependentRenderLodsFromLegacy(working);
    working.formatVersion = ModelAssetFormatVersion;
    const std::size_t lodCount = packageLodCount(working);
    for (std::size_t lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        if (!saveLod(path, working, lodIndex, error))
            return false;

    // Manifest remains the package commit point.
    if (!saveManifest(path, working, error))
        return false;
    return pruneStaleLods(path, working, error);
}

bool load(const std::string& path, ModelAsset& asset, std::string* error)
{
    bool legacyPackage = false;
    ModelAsset result;
    if (!loadManifest(path, result, &legacyPackage, error))
        return false;

    if (result.formatVersion == 3u)
    {
        const std::size_t count = legacyRenderLodCount(result);
        for (std::size_t lodIndex = 0; lodIndex < count; ++lodIndex)
            if (!loadLod(path, result, lodIndex, error)) return false;
        buildIndependentRenderLodsFromLegacy(result);
        result.formatVersion = ModelAssetFormatVersion;
    }
    else if (result.formatVersion == 2u)
    {
        buildIndependentRenderLodsFromLegacy(result);
        result.formatVersion = ModelAssetFormatVersion;
    }
    else
    {
        for (std::size_t lodIndex = 0; lodIndex < result.renderLods.size(); ++lodIndex)
            if (!loadLod(path, result, lodIndex, error)) return false;
    }
    asset = std::move(result);
    return true;
}

} // namespace elite::model_asset::binary::controller
