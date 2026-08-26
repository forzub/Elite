#pragma once

#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset
{

class ModelAssetBinary
{
public:
    static bool save(const std::string& path, const ModelAsset& asset, std::string* error = nullptr);
    static bool load(const std::string& path, ModelAsset& asset, std::string* error = nullptr);
};

} // namespace elite::model_asset
