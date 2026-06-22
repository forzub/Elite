#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace world::celestial::visual
{

struct CelestialGeneratedMapLod
{
    std::string preview512Path;
    std::string flat1024Path;
};

struct CelestialGeneratedGlobalLod
{
    std::string albedoPath;
    std::string normalPath;
    std::string materialPath;
    std::string cloudsPath;
    std::string emissionPath;

    bool hasClouds() const
    {
        return !cloudsPath.empty();
    }

    bool hasEmission() const
    {
        return !emissionPath.empty();
    }
};

struct CelestialGeneratedTileLod
{
    bool enabled = false;

    std::string rootPath;

    int tileSize = 0;
    int maxZoom = 0;

    std::vector<std::string> channels;
};

struct CelestialGeneratedBaseTextures
{
    std::string albedoPath;
    std::string normalPath;
    std::string materialPath;

    std::string cloudsPath;
    std::string emissionPath;

    std::string previewPath;
    std::string previewFlatPath;

    std::string shapeModelPath;

    std::string metaPath;
};

struct CelestialGeneratedAssetSet
{
    std::string systemFolderName;
    std::string bodyFolderName;
    std::string displayName;

    double referenceRadiusKm = 0.0;

    CelestialGeneratedBaseTextures base;
    CelestialGeneratedMapLod map;
    CelestialGeneratedGlobalLod global;
    CelestialGeneratedTileLod tiles;

    bool hasShapeModel() const
    {
        return !base.shapeModelPath.empty();
    }

    bool hasUsableMapLod() const
    {
        return
            !map.preview512Path.empty() &&
            !map.flat1024Path.empty();
    }

    bool hasUsableGlobalLod() const
    {
        return
            !global.albedoPath.empty() &&
            !global.normalPath.empty() &&
            !global.materialPath.empty();
    }
};

struct CelestialGeneratedAssetLibraryOptions
{
    std::string generatedRoot =
        "src/assets/generated/celestial";

    std::string systemFilter;
    std::string bodyFilter;

    bool validateFiles = true;
    bool verbose = false;
};

class CelestialGeneratedAssetLibrary
{
public:
    bool load(
        const CelestialGeneratedAssetLibraryOptions& options
    );

    const CelestialGeneratedAssetSet* find(
        const std::string& systemFolderName,
        const std::string& bodyFolderName
    ) const;

    const std::vector<CelestialGeneratedAssetSet>& assets() const
    {
        return m_assets;
    }

    int failedCount() const
    {
        return m_failedCount;
    }

private:
    static std::string makeKey(
        const std::string& systemFolderName,
        const std::string& bodyFolderName
    );

private:
    std::vector<CelestialGeneratedAssetSet> m_assets;
    std::unordered_map<std::string, std::size_t> m_index;

    int m_failedCount = 0;
};

} // namespace world::celestial::visual