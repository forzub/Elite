#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::binary
{

bool writeLodPayload(const std::filesystem::path& path, const ModelAsset& asset, std::size_t lodIndex, std::string* error = nullptr);
bool readLodPayload(const std::filesystem::path& path, ModelAsset& asset, std::size_t expectedLodIndex, std::string* error = nullptr);

} // namespace elite::model_asset::binary
