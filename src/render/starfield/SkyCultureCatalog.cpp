#include "SkyCultureCatalog.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

namespace
{
    using json = nlohmann::json;

    std::string localizedFallback(
        const std::unordered_map<std::string, std::string>& names,
        const std::string& locale,
        const std::string& fallback
    )
    {
        auto findName = [&](const std::string& key) -> const std::string*
        {
            const auto it = names.find(key);
            return it == names.end() ? nullptr : &it->second;
        };

        if (!locale.empty())
        {
            if (const std::string* exact = findName(locale))
                return *exact;

            const std::size_t separator = locale.find_first_of("-_ ");
            if (separator != std::string::npos)
            {
                if (const std::string* base = findName(locale.substr(0, separator)))
                    return *base;
            }
        }

        if (const std::string* english = findName("en"))
            return *english;
        if (const std::string* native = findName("native"))
            return *native;
        return fallback;
    }

    bool parseNameMap(
        const json& value,
        std::unordered_map<std::string, std::string>& out
    )
    {
        if (!value.is_object())
            return false;

        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (it.value().is_string() && !it.value().get<std::string>().empty())
                out[it.key()] = it.value().get<std::string>();
        }
        return !out.empty();
    }
}

std::string SkyCultureCatalog::Constellation::displayName(
    const std::string& locale
) const
{
    return localizedFallback(localizedNames, locale, name.empty() ? id : name);
}

std::string SkyCultureCatalog::Culture::displayName(
    const std::string& locale
) const
{
    return localizedFallback(localizedNames, locale, id);
}

const SkyCultureCatalog::Culture* SkyCultureCatalog::culture(
    std::size_t index
) const
{
    return index < m_cultures.size() ? &m_cultures[index] : nullptr;
}

const SkyCultureCatalog::Culture* SkyCultureCatalog::cultureById(
    const std::string& id
) const
{
    const auto it = std::find_if(
        m_cultures.begin(),
        m_cultures.end(),
        [&](const Culture& culture) { return culture.id == id; }
    );
    return it == m_cultures.end() ? nullptr : &*it;
}

bool SkyCultureCatalog::loadManifest(const std::string& manifestPath)
{
    std::ifstream input(manifestPath);
    if (!input.is_open())
        return false;

    json root;
    try
    {
        input >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[SkyCulture] failed to parse manifest "
                  << manifestPath << ": " << e.what() << std::endl;
        return false;
    }

    if (!root.contains("cultures") || !root["cultures"].is_array())
        return false;

    const std::filesystem::path base =
        std::filesystem::path(manifestPath).parent_path();

    std::vector<Culture> loaded;
    std::unordered_set<std::string> ids;

    for (const auto& item : root["cultures"])
    {
        if (!item.is_object() ||
            !item.contains("id") || !item["id"].is_string() ||
            !item.contains("file") || !item["file"].is_string())
        {
            return false;
        }

        const std::string id = item["id"].get<std::string>();
        if (id.empty() || !ids.emplace(id).second)
            return false;

        Culture culture;
        const std::filesystem::path path = base / item["file"].get<std::string>();
        if (!loadCultureFile(path.string(), id, culture))
            return false;

        loaded.push_back(std::move(culture));
    }

    if (loaded.empty())
        return false;

    const std::string defaultId = root.value("default_culture", loaded.front().id);
    auto defaultIt = std::find_if(
        loaded.begin(), loaded.end(),
        [&](const Culture& culture) { return culture.id == defaultId; }
    );
    if (defaultIt == loaded.end())
        return false;

    m_defaultCultureIndex =
        static_cast<std::size_t>(std::distance(loaded.begin(), defaultIt));
    m_cultures = std::move(loaded);
    return true;
}

bool SkyCultureCatalog::loadCultureFile(
    const std::string& path,
    const std::string& expectedId,
    Culture& outCulture
)
{
    std::ifstream input(path);
    if (!input.is_open())
        return false;

    json root;
    try
    {
        input >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[SkyCulture] failed to parse " << path
                  << ": " << e.what() << std::endl;
        return false;
    }

    if (root.value("culture_id", std::string()) != expectedId ||
        !root.contains("constellations") ||
        !root["constellations"].is_array())
    {
        return false;
    }

    Culture culture;
    culture.id = expectedId;
    if (root.contains("culture_names"))
        parseNameMap(root["culture_names"], culture.localizedNames);

    const std::string identifier = root.value("star_identifier", std::string());
    if (identifier == "hr")
        culture.starIdentifier = StarIdentifier::BrightStarHr;
    else if (identifier == "hip")
        culture.starIdentifier = StarIdentifier::Hipparcos;
    else
        return false;

    std::unordered_set<std::string> constellationIds;
    for (const auto& item : root["constellations"])
    {
        if (!item.is_object() ||
            !item.contains("id") || !item["id"].is_string() ||
            !item.contains("polylines") || !item["polylines"].is_array())
        {
            return false;
        }

        Constellation constellation;
        constellation.id = item["id"].get<std::string>();
        if (constellation.id.empty() || !constellationIds.emplace(constellation.id).second)
            return false;
        constellation.name = item.value("name", constellation.id);
        if (item.contains("names"))
            parseNameMap(item["names"], constellation.localizedNames);

        for (const auto& polylineJson : item["polylines"])
        {
            if (!polylineJson.is_array())
                return false;
            std::vector<int> polyline;
            for (const auto& id : polylineJson)
            {
                if (!id.is_number_integer() || id.get<int>() <= 0)
                    return false;
                polyline.push_back(id.get<int>());
            }
            if (polyline.size() < 2)
                return false;
            constellation.polylines.push_back(std::move(polyline));
        }

        if (constellation.polylines.empty())
            return false;
        culture.constellations.push_back(std::move(constellation));
    }

    if (culture.constellations.empty())
        return false;

    outCulture = std::move(culture);
    return true;
}
