#pragma once

#include <filesystem>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::binary
{

bool writeManifestV4(const std::filesystem::path& path, const ModelAsset& asset, std::string* error = nullptr);
bool readManifestPackage(const std::filesystem::path& path, ModelAsset& asset, bool* legacyPackage = nullptr, std::string* error = nullptr);

} // namespace elite::model_asset::binary
