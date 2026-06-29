#include "CelestialGeneratedAssetLibrary.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace world::celestial::visual
{

namespace
{

using json = nlohmann::json;

std::string normalizeFilterToken(std::string s)
{
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';

        if (c == ' ' || c == '-')
            c = '_';

        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))
        );
    }

    while (!s.empty() && s.front() == '/')
        s.erase(s.begin());

    while (!s.empty() && s.back() == '/')
        s.pop_back();

    return s;
}

std::filesystem::path resolveProjectPath(
    const std::string& pathText
)
{
    namespace fs = std::filesystem;

    const fs::path raw(pathText);

    if (raw.is_absolute())
        return raw.lexically_normal();

    const fs::path cwd =
        fs::current_path();

    const std::vector<fs::path> candidates =
    {
        cwd / raw,
        cwd.parent_path() / raw,
        cwd.parent_path().parent_path() / raw,
        fs::path("D:/__elite/work") / raw
    };

    for (const fs::path& p : candidates)
    {
        if (fs::exists(p))
            return p.lexically_normal();
    }

    return (cwd.parent_path() / raw).lexically_normal();
}

bool loadJsonFile(
    const std::filesystem::path& path,
    json& out
)
{
    std::ifstream f(path);

    if (!f.is_open())
        return false;

    try
    {
        f >> out;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[CelestialGeneratedAssetLibrary] JSON parse error: "
            << path.generic_string()
            << " : "
            << e.what()
            << "\n";

        return false;
    }

    return true;
}

std::string jsonString(
    const json& object,
    const std::string& key
)
{
    if (!object.is_object())
        return "";

    if (!object.contains(key))
        return "";

    if (!object[key].is_string())
        return "";

    return object[key].get<std::string>();
}

double jsonDouble(
    const json& object,
    const std::string& key,
    double fallback = 0.0
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    if (!object[key].is_number())
        return fallback;

    return object[key].get<double>();
}

int jsonInt(
    const json& object,
    const std::string& key,
    int fallback = 0
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    if (!object[key].is_number_integer())
        return fallback;

    return object[key].get<int>();
}

bool jsonBool(
    const json& object,
    const std::string& key,
    bool fallback = false
)
{
    if (!object.is_object())
        return fallback;

    if (!object.contains(key))
        return fallback;

    if (!object[key].is_boolean())
        return fallback;

    return object[key].get<bool>();
}

std::vector<std::string> jsonStringArray(
    const json& object,
    const std::string& key
)
{
    std::vector<std::string> out;

    if (!object.is_object())
        return out;

    if (!object.contains(key))
        return out;

    if (!object[key].is_array())
        return out;

    for (const auto& item : object[key])
    {
        if (item.is_string())
            out.push_back(item.get<std::string>());
    }

    return out;
}

std::filesystem::path resolveAssetFilePath(
    const std::string& assetPath
)
{
    return resolveProjectPath(assetPath);
}

bool assetFileExists(
    const std::string& assetPath
)
{
    if (assetPath.empty())
        return false;

    return std::filesystem::is_regular_file(
        resolveAssetFilePath(assetPath)
    );
}

bool assetDirectoryExists(
    const std::string& assetPath
)
{
    if (assetPath.empty())
        return false;

    return std::filesystem::is_directory(
        resolveAssetFilePath(assetPath)
    );
}

bool requireFile(
    const CelestialGeneratedAssetSet& asset,
    const char* label,
    const std::string& assetPath
)
{
    if (assetFileExists(assetPath))
        return true;

    std::cerr
        << "[CelestialGeneratedAssetLibrary] missing "
        << label
        << " for "
        << asset.systemFolderName
        << "/"
        << asset.bodyFolderName
        << " -> "
        << assetPath
        << "\n";

    return false;
}

bool requireDirectory(
    const CelestialGeneratedAssetSet& asset,
    const char* label,
    const std::string& assetPath
)
{
    if (assetDirectoryExists(assetPath))
        return true;

    std::cerr
        << "[CelestialGeneratedAssetLibrary] missing "
        << label
        << " for "
        << asset.systemFolderName
        << "/"
        << asset.bodyFolderName
        << " -> "
        << assetPath
        << "\n";

    return false;
}

bool validateAssetSet(
    const CelestialGeneratedAssetSet& asset
)
{
    bool ok = true;

    if (asset.referenceRadiusKm <= 0.0)
    {
        std::cerr
            << "[CelestialGeneratedAssetLibrary] invalid reference_radius_km for "
            << asset.systemFolderName
            << "/"
            << asset.bodyFolderName
            << " value="
            << asset.referenceRadiusKm
            << "\n";

        ok = false;
    }

    ok = requireFile(asset, "base albedo", asset.base.albedoPath) && ok;
    ok = requireFile(asset, "base normal", asset.base.normalPath) && ok;
    ok = requireFile(asset, "base material", asset.base.materialPath) && ok;
    ok = requireFile(asset, "base preview", asset.base.previewPath) && ok;
    ok = requireFile(asset, "base preview_flat", asset.base.previewFlatPath) && ok;

    if (!asset.base.cloudsPath.empty())
        ok = requireFile(asset, "base clouds", asset.base.cloudsPath) && ok;

    if (!asset.base.emissionPath.empty())
        ok = requireFile(asset, "base emission", asset.base.emissionPath) && ok;

    if (!asset.base.shapeModelPath.empty())
        ok = requireFile(asset, "shape_model", asset.base.shapeModelPath) && ok;

    ok = requireFile(asset, "map preview_512", asset.map.preview512Path) && ok;
    ok = requireFile(asset, "map flat_1024", asset.map.flat1024Path) && ok;

    ok = requireFile(asset, "global albedo", asset.global.albedoPath) && ok;
    ok = requireFile(asset, "global normal", asset.global.normalPath) && ok;
    ok = requireFile(asset, "global material", asset.global.materialPath) && ok;

    if (!asset.global.cloudsPath.empty())
        ok = requireFile(asset, "global clouds", asset.global.cloudsPath) && ok;

    if (!asset.global.emissionPath.empty())
        ok = requireFile(asset, "global emission", asset.global.emissionPath) && ok;

    if (asset.tiles.enabled)
    {
        ok = requireDirectory(asset, "tiles root", asset.tiles.rootPath) && ok;

        if (asset.tiles.tileSize <= 0)
        {
            std::cerr
                << "[CelestialGeneratedAssetLibrary] invalid tile_size for "
                << asset.systemFolderName
                << "/"
                << asset.bodyFolderName
                << " value="
                << asset.tiles.tileSize
                << "\n";

            ok = false;
        }

        if (asset.tiles.maxZoom < 0)
        {
            std::cerr
                << "[CelestialGeneratedAssetLibrary] invalid max_zoom for "
                << asset.systemFolderName
                << "/"
                << asset.bodyFolderName
                << " value="
                << asset.tiles.maxZoom
                << "\n";

            ok = false;
        }

        if (asset.tiles.channels.empty())
        {
            std::cerr
                << "[CelestialGeneratedAssetLibrary] tiles enabled but channels are empty for "
                << asset.systemFolderName
                << "/"
                << asset.bodyFolderName
                << "\n";

            ok = false;
        }

        for (const std::string& channel : asset.tiles.channels)
        {
            const std::string channelRoot =
                asset.tiles.rootPath + "/" + channel;

            ok = requireDirectory(
                    asset,
                    ("tiles channel " + channel).c_str(),
                    channelRoot
                ) && ok;
        }
    }

    return ok;
}

bool bodyMatchesFilter(
    const std::string& systemFolder,
    const std::string& bodyFolder,
    const CelestialGeneratedAssetLibraryOptions& options
)
{
    std::string systemFilter =
        normalizeFilterToken(options.systemFilter);

    std::string bodyFilter =
        normalizeFilterToken(options.bodyFilter);

    if (systemFilter.empty() &&
        !bodyFilter.empty())
    {
        const std::size_t slash =
            bodyFilter.find('/');

        if (slash != std::string::npos)
        {
            systemFilter = bodyFilter.substr(0, slash);
            bodyFilter = bodyFilter.substr(slash + 1);
        }
    }

    const std::string system =
        normalizeFilterToken(systemFolder);

    const std::string body =
        normalizeFilterToken(bodyFolder);

    if (!systemFilter.empty() &&
        system != systemFilter)
    {
        return false;
    }

    if (!bodyFilter.empty())
    {
        const bool ok =
            body == bodyFilter ||
            (system + "/" + body) == bodyFilter;

        if (!ok)
            return false;
    }

    return true;
}



std::string normalizeGeneratedAssetSlashes(
    std::string s
)
{
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';
    }

    return s;
}

std::string rebaseGeneratedAssetPathToMetaBodyRoot(
    const std::filesystem::path& metaPath,
    const std::string& assetPath
)
{
    namespace fs = std::filesystem;

    if (assetPath.empty())
        return "";

    const fs::path bodyRoot =
        metaPath.parent_path();

    const std::string systemFolder =
        bodyRoot.parent_path().filename().generic_string();

    const std::string bodyFolder =
        bodyRoot.filename().generic_string();

    if (systemFolder.empty() || bodyFolder.empty())
        return assetPath;

    const std::string normalized =
        normalizeGeneratedAssetSlashes(assetPath);

    const std::string marker =
        "/" + systemFolder + "/" + bodyFolder + "/";

    std::size_t pos =
        normalized.find(marker);

    std::string suffix;

    if (pos != std::string::npos)
    {
        suffix =
            normalized.substr(pos + marker.size());
    }
    else
    {
        const std::string markerAtStart =
            systemFolder + "/" + bodyFolder + "/";

        if (normalized.rfind(markerAtStart, 0) == 0)
        {
            suffix =
                normalized.substr(markerAtStart.size());
        }
    }

    if (suffix.empty())
        return assetPath;

    return
        (bodyRoot / fs::path(suffix))
            .lexically_normal()
            .generic_string();
}



bool readAssetMeta(
    const std::filesystem::path& metaPath,
    CelestialGeneratedAssetSet& out
)
{
    json root;

    if (!loadJsonFile(metaPath, root))
    {
        std::cerr
            << "[CelestialGeneratedAssetLibrary] failed to read meta: "
            << metaPath.generic_string()
            << "\n";

        return false;
    }

    out.systemFolderName =
        jsonString(root, "system_folder_name");

    out.bodyFolderName =
        jsonString(root, "body_folder_name");

    out.displayName =
        jsonString(root, "display_name");

    if (out.systemFolderName.empty())
        out.systemFolderName =
            metaPath.parent_path().parent_path().filename().generic_string();

    if (out.bodyFolderName.empty())
        out.bodyFolderName =
            metaPath.parent_path().filename().generic_string();

    out.referenceRadiusKm =
        jsonDouble(root, "reference_radius_km", 0.0);

    const json generatedTextures =
        root.value("generated_textures", json::object());

    out.base.albedoPath =
        jsonString(generatedTextures, "albedo");

    out.base.normalPath =
        jsonString(generatedTextures, "normal");

    out.base.materialPath =
        jsonString(generatedTextures, "material");

    out.base.cloudsPath =
        jsonString(generatedTextures, "clouds");

    out.base.emissionPath =
        jsonString(generatedTextures, "emission");

    out.base.previewPath =
        jsonString(generatedTextures, "preview");

    out.base.previewFlatPath =
        jsonString(generatedTextures, "preview_flat");

    out.base.shapeModelPath =
        jsonString(generatedTextures, "shape_model");

    out.base.metaPath =
        jsonString(generatedTextures, "meta");

    const json renderLod =
        root.value("render_lod", json::object());

    const json map =
        renderLod.value("map", json::object());

    out.map.preview512Path =
        jsonString(map, "preview_512");

    out.map.flat1024Path =
        jsonString(map, "flat_1024");

    const json global =
        renderLod.value("global", json::object());

    out.global.albedoPath =
        jsonString(global, "albedo");

    out.global.normalPath =
        jsonString(global, "normal");

    out.global.materialPath =
        jsonString(global, "material");

    out.global.cloudsPath =
        jsonString(global, "clouds");

    out.global.emissionPath =
        jsonString(global, "emission");

    const json tiles =
        renderLod.value("tiles", json::object());

    out.tiles.enabled =
        jsonBool(tiles, "enabled", false);

    out.tiles.rootPath =
        jsonString(tiles, "root");

    out.tiles.tileSize =
        jsonInt(tiles, "tile_size", 0);

    out.tiles.maxZoom =
        jsonInt(tiles, "max_zoom", 0);

    out.tiles.channels =
        jsonStringArray(tiles, "channels");

        auto rebase =
        [&](const std::string& path) -> std::string
        {
            return rebaseGeneratedAssetPathToMetaBodyRoot(
                metaPath,
                path
            );
        };

    out.base.albedoPath =
        rebase(out.base.albedoPath);

    out.base.normalPath =
        rebase(out.base.normalPath);

    out.base.materialPath =
        rebase(out.base.materialPath);

    out.base.cloudsPath =
        rebase(out.base.cloudsPath);

    out.base.emissionPath =
        rebase(out.base.emissionPath);

    out.base.previewPath =
        rebase(out.base.previewPath);

    out.base.previewFlatPath =
        rebase(out.base.previewFlatPath);

    out.base.shapeModelPath =
        rebase(out.base.shapeModelPath);

    out.base.metaPath =
        rebase(out.base.metaPath);

    out.map.preview512Path =
        rebase(out.map.preview512Path);

    out.map.flat1024Path =
        rebase(out.map.flat1024Path);

    out.global.albedoPath =
        rebase(out.global.albedoPath);

    out.global.normalPath =
        rebase(out.global.normalPath);

    out.global.materialPath =
        rebase(out.global.materialPath);

    out.global.cloudsPath =
        rebase(out.global.cloudsPath);

    out.global.emissionPath =
        rebase(out.global.emissionPath);

    out.tiles.rootPath =
        rebase(out.tiles.rootPath);

    return true;
}

} // namespace

std::string CelestialGeneratedAssetLibrary::makeKey(
    const std::string& systemFolderName,
    const std::string& bodyFolderName
)
{
    return
        normalizeFilterToken(systemFolderName) +
        "/" +
        normalizeFilterToken(bodyFolderName);
}

bool CelestialGeneratedAssetLibrary::load(
    const CelestialGeneratedAssetLibraryOptions& options
)
{
    namespace fs = std::filesystem;

    m_assets.clear();
    m_index.clear();
    m_failedCount = 0;

    const fs::path root =
        resolveProjectPath(options.generatedRoot);

    if (!fs::exists(root) ||
        !fs::is_directory(root))
    {
        std::cerr
            << "[CelestialGeneratedAssetLibrary] generated root not found: "
            << root.generic_string()
            << "\n";

        return false;
    }

    int scanned = 0;

    for (const auto& systemEntry : fs::directory_iterator(root))
    {
        if (!systemEntry.is_directory())
            continue;

        const std::string systemFolder =
            systemEntry.path().filename().generic_string();

        if (!options.systemFilter.empty() &&
            normalizeFilterToken(systemFolder) != normalizeFilterToken(options.systemFilter))
        {
            continue;
        }

        for (const auto& bodyEntry : fs::directory_iterator(systemEntry.path()))
        {
            if (!bodyEntry.is_directory())
                continue;

            const std::string bodyFolder =
                bodyEntry.path().filename().generic_string();

            if (!bodyMatchesFilter(systemFolder, bodyFolder, options))
                continue;

            const fs::path metaPath =
                bodyEntry.path() / "meta.json";

            ++scanned;

            CelestialGeneratedAssetSet asset;

            if (!fs::exists(metaPath))
            {
                std::cerr
                    << "[CelestialGeneratedAssetLibrary] missing meta.json for "
                    << systemFolder
                    << "/"
                    << bodyFolder
                    << "\n";

                ++m_failedCount;
                continue;
            }

            if (!readAssetMeta(metaPath, asset))
            {
                ++m_failedCount;
                continue;
            }

            bool ok = true;

            if (options.validateFiles)
                ok = validateAssetSet(asset);

            if (!ok)
            {
                ++m_failedCount;
                continue;
            }

            const std::string key =
                makeKey(
                    asset.systemFolderName,
                    asset.bodyFolderName
                );

            m_index[key] =
                m_assets.size();

            if (options.verbose)
            {
                std::cout
                    << "[CelestialGeneratedAssetLibrary] loaded "
                    << asset.systemFolderName
                    << "/"
                    << asset.bodyFolderName
                    << " radius_km="
                    << asset.referenceRadiusKm
                    << " map="
                    << (asset.hasUsableMapLod() ? "OK" : "NO")
                    << " global="
                    << (asset.hasUsableGlobalLod() ? "OK" : "NO")
                    << " tiles="
                    << (asset.tiles.enabled ? "ON" : "OFF")
                    << " channels="
                    << asset.tiles.channels.size();

                if (asset.hasShapeModel())
                {
                    std::cout
                        << " shape_model=OK";
                }

                std::cout << "\n";
            }

            m_assets.push_back(std::move(asset));
        }
    }

    if (scanned == 0)
    {
        std::cerr
            << "[CelestialGeneratedAssetLibrary] no generated bodies matched filters:"
            << " system=\""
            << options.systemFilter
            << "\" body=\""
            << options.bodyFilter
            << "\""
            << "\n";

        return false;
    }

    std::cout
        << "[CelestialGeneratedAssetLibrary] done"
        << " scanned=" << scanned
        << " loaded=" << m_assets.size()
        << " failed=" << m_failedCount
        << "\n";

    return m_failedCount == 0;
}

const CelestialGeneratedAssetSet* CelestialGeneratedAssetLibrary::find(
    const std::string& systemFolderName,
    const std::string& bodyFolderName
) const
{
    const std::string key =
        makeKey(systemFolderName, bodyFolderName);

    auto it =
        m_index.find(key);

    if (it == m_index.end())
        return nullptr;

    return &m_assets[it->second];
}

} // namespace world::celestial::visual