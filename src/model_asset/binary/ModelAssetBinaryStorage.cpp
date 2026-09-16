#include "src/model_asset/binary/ModelAssetBinaryStorage.h"
#include "src/model_asset/ModelAssetMigration.h"

#include <system_error>

namespace elite::model_asset::binary
{

std::filesystem::path packageLodPayloadPath(const std::filesystem::path& manifestPath, std::size_t lodIndex)
{
    const std::string stem = manifestPath.stem().string();
    return manifestPath.parent_path() /
        (stem + ".lod" + std::to_string(lodIndex) + ".elmesh");
}

std::size_t packageLodCount(const ModelAsset& asset)
{
    if (!asset.renderLods.empty())
        return asset.renderLods.size();
    return legacyRenderLodCount(asset);
}

void removeStaleLodPayloadFiles(
    const std::filesystem::path& manifestPath,
    const std::set<std::filesystem::path>& keep)
{
    std::error_code ec;
    const auto directory = manifestPath.parent_path();
    if (!std::filesystem::exists(directory, ec)) return;
    const std::string prefix = manifestPath.stem().string() + ".lod";
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (ec || !entry.is_regular_file()) continue;
        const auto name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0 || entry.path().extension() != ".elmesh") continue;
        if (keep.count(entry.path()) == 0)
            std::filesystem::remove(entry.path(), ec);
    }
}

} // namespace elite::model_asset::binary
