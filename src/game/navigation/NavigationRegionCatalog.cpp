#include "src/game/navigation/NavigationRegionCatalog.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace game::navigation
{
namespace
{
using json = nlohmann::json;
namespace fs = std::filesystem;

std::string baseLanguage(const std::string& locale)
{
    const std::size_t separator = locale.find_first_of("-_");
    return separator == std::string::npos
        ? locale
        : locale.substr(0, separator);
}

int nameScore(
    const NavigationRegionName& name,
    const std::string& factionId,
    const std::string& locale
)
{
    int score = 0;

    if (name.factionId == factionId)
        score += 1000;
    else if (name.factionId == "common" ||
             name.factionId == "neutral")
        score += 500;
    else
        score += 100;

    if (name.language == locale)
        score += 100;
    else if (baseLanguage(name.language) == baseLanguage(locale))
        score += 70;
    else if (name.language == "en")
        score += 30;

    if (name.preferred)
        score += 10;

    if (name.kind == "official")
        score += 4;
    else if (name.kind == "common")
        score += 2;

    return score;
}

bool parseRegionRoot(const json& root, std::vector<NavigationRegionRecord>& parsed)
{
    if (!root.is_object() || root.value("schema_version", 0) != 1 ||
        root.value("kind", std::string()) != "navigation_regions" ||
        !root.contains("regions") || !root["regions"].is_array())
    {
        return false;
    }

    for (const json& region : root["regions"])
    {
        const json cellJson = region.value("cell", json::object());
        const json indexJson = cellJson.value("index", json::array());

        if (indexJson.size() != 3 ||
            !indexJson[0].is_number_integer() ||
            !indexJson[1].is_number_integer() ||
            !indexJson[2].is_number_integer())
        {
            return false;
        }

        NavigationRegionRecord record;
        record.cell.frameId = cellJson.value("frame_id", "");
        record.cell.level = cellJson.value("level", 0);
        record.cell.x = indexJson[0].get<std::int64_t>();
        record.cell.y = indexJson[1].get<std::int64_t>();
        record.cell.z = indexJson[2].get<std::int64_t>();

        if (record.cell.frameId.empty())
            return false;

        for (const json& nameJson : region.value("names", json::array()))
        {
            NavigationRegionName name;
            name.factionId = nameJson.value("faction_id", "common");
            name.language = nameJson.value("language", "en");
            name.script = nameJson.value("script", "Latn");
            name.name = nameJson.value("name", "");
            name.shortName = nameJson.value("short_name", name.name);
            name.kind = nameJson.value("kind", "common");
            name.preferred = nameJson.value("preferred", false);

            if (!name.name.empty())
                record.names.push_back(std::move(name));
        }

        if (record.names.empty())
            return false;

        parsed.push_back(std::move(record));
    }

    return true;
}

std::string cellKey(const NavigationCellId& cell)
{
    return cell.frameId + "|" + std::to_string(cell.level) + "|" +
           std::to_string(cell.x) + "|" + std::to_string(cell.y) + "|" +
           std::to_string(cell.z);
}
}

bool NavigationRegionCatalog::loadFromFile(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
        return false;

    json root;
    try
    {
        input >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[NavigationRegionCatalog] skipped " << path
                  << ": " << e.what() << '\n';
        return false;
    }

    std::vector<NavigationRegionRecord> parsed;
    if (!parseRegionRoot(root, parsed))
        return false;

    m_records = std::move(parsed);
    m_loaded = true;
    return true;
}

bool NavigationRegionCatalog::loadFromDirectory(const std::string& path)
{
    const fs::path root(path);
    if (!fs::exists(root) || !fs::is_directory(root))
        return false;

    std::vector<fs::path> files;
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(root))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
                files.push_back(entry.path());
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[NavigationRegionCatalog] scan failed: " << e.what() << '\n';
        return false;
    }

    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b)
    {
        return a.generic_string() < b.generic_string();
    });

    std::vector<NavigationRegionRecord> combined;
    std::unordered_set<std::string> cells;
    std::size_t skipped = 0;

    for (const fs::path& file : files)
    {
        json rootJson;
        try
        {
            std::ifstream input(file);
            if (!input)
            {
                ++skipped;
                continue;
            }
            input >> rootJson;
        }
        catch (const std::exception& e)
        {
            ++skipped;
            std::cerr << "[NavigationRegionCatalog] skipped "
                      << file.generic_string() << ": " << e.what() << '\n';
            continue;
        }

        std::vector<NavigationRegionRecord> parsed;
        if (!parseRegionRoot(rootJson, parsed))
        {
            ++skipped;
            std::cerr << "[NavigationRegionCatalog] skipped "
                      << file.generic_string() << ": invalid schema\n";
            continue;
        }

        for (NavigationRegionRecord& record : parsed)
        {
            const std::string key = cellKey(record.cell);
            if (!cells.emplace(key).second)
            {
                std::cerr << "[NavigationRegionCatalog] duplicate cell ignored: "
                          << key << " in " << file.generic_string() << '\n';
                continue;
            }
            combined.push_back(std::move(record));
        }
    }

    m_records = std::move(combined);
    m_loaded = !m_records.empty();
    if (m_loaded)
    {
        std::cout << "[NavigationRegionCatalog] root=" << path
                  << " records=" << m_records.size()
                  << " skipped=" << skipped << '\n';
    }
    return m_loaded;
}

bool NavigationRegionCatalog::loadFromRuntimeOrSource(
    const std::string& runtimePath,
    const std::string& sourcePath
)
{
    return loadFromDirectory(runtimePath) || loadFromDirectory(sourcePath) ||
           loadFromFile(runtimePath) || loadFromFile(sourcePath);
}

std::optional<ResolvedNavigationRegionName>
NavigationRegionCatalog::resolve(
    const NavigationCellId& requestedCell,
    int subdivision,
    const std::string& factionId,
    const std::string& locale
) const
{
    if (!m_loaded || subdivision < 3)
        return std::nullopt;

    NavigationCellId candidate = requestedCell;

    while (candidate.level >= 0)
    {
        if (const NavigationRegionRecord* record = findExact(candidate))
        {
            if (const NavigationRegionName* name =
                    chooseName(*record, factionId, locale))
            {
                ResolvedNavigationRegionName result;
                result.sourceCell = candidate;
                result.value = *name;
                result.inherited = candidate.level != requestedCell.level;
                return result;
            }
        }

        if (candidate.level == 0)
            break;

        candidate.x = static_cast<std::int64_t>(
            std::llround(
                static_cast<double>(candidate.x) /
                static_cast<double>(subdivision)
            )
        );
        candidate.y = static_cast<std::int64_t>(
            std::llround(
                static_cast<double>(candidate.y) /
                static_cast<double>(subdivision)
            )
        );
        candidate.z = static_cast<std::int64_t>(
            std::llround(
                static_cast<double>(candidate.z) /
                static_cast<double>(subdivision)
            )
        );
        --candidate.level;
    }

    return std::nullopt;
}

bool NavigationRegionCatalog::loaded() const
{
    return m_loaded;
}

const std::vector<NavigationRegionRecord>&
NavigationRegionCatalog::records() const
{
    return m_records;
}

const NavigationRegionRecord* NavigationRegionCatalog::findExact(
    const NavigationCellId& cell
) const
{
    for (const NavigationRegionRecord& record : m_records)
    {
        if (record.cell == cell)
            return &record;
    }

    return nullptr;
}

const NavigationRegionName* NavigationRegionCatalog::chooseName(
    const NavigationRegionRecord& record,
    const std::string& factionId,
    const std::string& locale
)
{
    const NavigationRegionName* best = nullptr;
    int bestScore = std::numeric_limits<int>::min();

    for (const NavigationRegionName& name : record.names)
    {
        const int score = nameScore(name, factionId, locale);
        if (score > bestScore)
        {
            best = &name;
            bestScore = score;
        }
    }

    return best;
}

} // namespace game::navigation
