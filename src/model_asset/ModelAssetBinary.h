#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset
{

class ModelAssetBinary
{
public:
    // Reusable preflight used by wizard/checkpoints and serializers. It reports
    // the exact offending stable ID/index instead of delaying discovery until I/O.
    static bool validate(const ModelAsset& asset, std::string* error = nullptr);

    // v4 package API. Each .elmesh owns an independent render graph; the manifest owns semantic state. The semantic manifest and every heavy LOD payload can be
    // loaded/saved independently. This is the API used by the asset editor and
    // later by runtime streaming.
    static bool saveManifest(const std::string& path, const ModelAsset& asset, std::string* error = nullptr);
    static bool loadManifest(
        const std::string& path,
        ModelAsset& asset,
        bool* legacyPackage = nullptr,
        std::string* error = nullptr);
    static bool saveLod(const std::string& manifestPath, const ModelAsset& asset, std::size_t lodIndex, std::string* error = nullptr);
    static bool loadLod(const std::string& manifestPath, ModelAsset& asset, std::size_t lodIndex, std::string* error = nullptr);
    static bool pruneStaleLods(const std::string& manifestPath, const ModelAsset& asset, std::string* error = nullptr);
    static std::filesystem::path lodPayloadPath(const std::string& manifestPath, std::size_t lodIndex);

    // Convenience full-package wrappers retained for tests/batch tools. Editor
    // code should prefer the independent manifest/LOD operations above.
    static bool save(const std::string& path, const ModelAsset& asset, std::string* error = nullptr);
    static bool load(const std::string& path, ModelAsset& asset, std::string* error = nullptr);
};

} // namespace elite::model_asset
