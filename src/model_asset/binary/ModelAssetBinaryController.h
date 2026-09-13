#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::binary::controller
{

bool validate(const ModelAsset& asset, std::string* error = nullptr);
bool saveManifest(const std::string& path, const ModelAsset& asset, std::string* error = nullptr);
bool loadManifest(const std::string& path, ModelAsset& asset, bool* legacyPackage = nullptr, std::string* error = nullptr);
bool saveLod(const std::string& manifestPath, const ModelAsset& asset, std::size_t lodIndex, std::string* error = nullptr);
bool loadLod(const std::string& manifestPath, ModelAsset& asset, std::size_t lodIndex, std::string* error = nullptr);
bool pruneStaleLods(const std::string& manifestPath, const ModelAsset& asset, std::string* error = nullptr);
bool save(const std::string& path, const ModelAsset& asset, std::string* error = nullptr);
bool load(const std::string& path, ModelAsset& asset, std::string* error = nullptr);
std::filesystem::path lodPayloadPath(const std::string& manifestPath, std::size_t lodIndex);

} // namespace elite::model_asset::binary::controller
