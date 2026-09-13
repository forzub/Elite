#pragma once

#include <cstddef>
#include <filesystem>
#include <set>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::binary
{

std::filesystem::path packageLodPayloadPath(const std::filesystem::path& manifestPath, std::size_t lodIndex);
std::size_t packageLodCount(const ModelAsset& asset);
void removeStaleLodPayloadFiles(const std::filesystem::path& manifestPath, const std::set<std::filesystem::path>& keep);

} // namespace elite::model_asset::binary
