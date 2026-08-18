#include "src/game/localization/LocalizationService.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#include <nlohmann/json.hpp>

namespace game::localization
{
namespace
{
using json = nlohmann::json;
namespace fs = std::filesystem;

bool readTranslationMap(
    const json& source,
    LocalizationService::TranslationMap& out
)
{
    if (!source.is_object())
        return false;

    for (auto it = source.begin(); it != source.end(); ++it)
    {
        if (it.value().is_string() && !it.value().get<std::string>().empty())
            out[it.key()] = it.value().get<std::string>();
    }

    return !out.empty();
}

bool readTranslationTable(
    const json& source,
    LocalizationService::TranslationTable& out,
    bool requireEnglish
)
{
    if (!source.is_object())
        return false;

    for (auto it = source.begin(); it != source.end(); ++it)
    {
        LocalizationService::TranslationMap translations;
        if (!readTranslationMap(it.value(), translations))
            return false;
        if (requireEnglish && translations.find("en") == translations.end())
            return false;
        out[it.key()] = std::move(translations);
    }

    return true;
}

std::string baseLocale(const std::string& locale)
{
    const std::size_t split = locale.find_first_of("-_");
    return split == std::string::npos ? locale : locale.substr(0, split);
}

std::string pathText(const fs::path& path)
{
    return path.generic_string();
}

bool readJsonFile(const fs::path& path, json& root, std::string& error)
{
    try
    {
        std::ifstream input(path);
        if (!input.is_open())
        {
            error = "cannot open file";
            return false;
        }
        input >> root;
        if (!root.is_object())
        {
            error = "root must be a JSON object";
            return false;
        }
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

bool validateSchema(const json& root)
{
    return root.value("schema_version", 0) == 1;
}
}

bool LocalizationService::loadDirectory(const std::string& rootPath)
{
    m_defaultLocale = "en";
    m_localeOrder = {"en"};
    m_uiStrings.clear();
    m_languages.clear();
    m_localeMetadata.clear();
    m_catalogNames.clear();
    m_uiSources.clear();
    m_catalogSources.clear();
    m_loadedFileCount = 0;
    m_skippedFileCount = 0;

    const fs::path root(rootPath);
    if (!fs::exists(root) || !fs::is_directory(root))
    {
        std::cerr << "[Localization] root not found: " << rootPath << '\n';
        return false;
    }

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
        std::cerr << "[Localization] directory scan failed: " << e.what() << '\n';
        return false;
    }

    // Filesystem iteration order is not a contract. Sort first so duplicate
    // handling is deterministic on every platform: first valid definition wins.
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b)
    {
        return a.generic_string() < b.generic_string();
    });

    bool languagesLoaded = false;

    auto reject = [&](const fs::path& path, const std::string& reason)
    {
        ++m_skippedFileCount;
        std::cerr << "[Localization] skipped " << pathText(path)
                  << ": " << reason << '\n';
    };

    auto mergeUi = [&](const fs::path& path, const TranslationTable& table)
    {
        for (const auto& [key, translations] : table)
        {
            const auto sourceIt = m_uiSources.find(key);
            if (sourceIt != m_uiSources.end())
            {
                std::cerr << "[Localization] duplicate UI key " << key
                          << " in " << pathText(path)
                          << "; keeping " << sourceIt->second << '\n';
                continue;
            }
            m_uiStrings.emplace(key, translations);
            m_uiSources.emplace(key, pathText(path));
        }
    };

    auto mergeCatalog = [&](const fs::path& path,
                            const std::string& domain,
                            const TranslationTable& table)
    {
        TranslationTable& target = m_catalogNames[domain];
        auto& sources = m_catalogSources[domain];
        for (const auto& [stableId, translations] : table)
        {
            const auto sourceIt = sources.find(stableId);
            if (sourceIt != sources.end())
            {
                std::cerr << "[Localization] duplicate catalog object "
                          << domain << '/' << stableId << " in " << pathText(path)
                          << "; keeping " << sourceIt->second << '\n';
                continue;
            }
            target.emplace(stableId, translations);
            sources.emplace(stableId, pathText(path));
        }
    };

    for (const fs::path& path : files)
    {
        json fileRoot;
        std::string parseError;
        if (!readJsonFile(path, fileRoot, parseError))
        {
            reject(path, parseError);
            continue;
        }
        if (!validateSchema(fileRoot))
        {
            reject(path, "unsupported or missing schema_version");
            continue;
        }

        const std::string kind = fileRoot.value("kind", std::string());
        if (kind.empty())
        {
            reject(path, "missing kind");
            continue;
        }

        if (kind == "languages")
        {
            if (languagesLoaded)
            {
                reject(path, "duplicate languages configuration");
                continue;
            }

            const std::string defaultLocale =
                fileRoot.value("default_locale", std::string());
            std::vector<std::string> localeOrder;
            bool localeOrderValid =
                fileRoot.contains("locale_order") && fileRoot["locale_order"].is_array();
            if (localeOrderValid)
            {
                for (const auto& value : fileRoot["locale_order"])
                {
                    if (!value.is_string() || value.get<std::string>().empty())
                    {
                        localeOrderValid = false;
                        break;
                    }
                    localeOrder.push_back(value.get<std::string>());
                }
            }

            TranslationTable languages;
            std::unordered_map<std::string, LocaleMetadata> localeMetadata;
            bool metadataValid =
                fileRoot.contains("locale_metadata") &&
                fileRoot["locale_metadata"].is_object();
            if (metadataValid)
            {
                for (auto it = fileRoot["locale_metadata"].begin();
                     it != fileRoot["locale_metadata"].end(); ++it)
                {
                    if (!it.value().is_object())
                    {
                        metadataValid = false;
                        break;
                    }

                    LocaleMetadata metadata;
                    metadata.nativeName =
                        it.value().value("native_name", std::string());
                    metadata.englishName =
                        it.value().value("english_name", std::string());
                    metadata.direction =
                        it.value().value("direction", std::string());
                    metadata.script =
                        it.value().value("script", std::string());

                    if (metadata.nativeName.empty() ||
                        metadata.englishName.empty() ||
                        metadata.script.empty() ||
                        (metadata.direction != "ltr" && metadata.direction != "rtl"))
                    {
                        metadataValid = false;
                        break;
                    }
                    localeMetadata.emplace(it.key(), std::move(metadata));
                }
            }

            if (metadataValid)
            {
                for (const std::string& enabledLocale : localeOrder)
                {
                    if (localeMetadata.find(enabledLocale) == localeMetadata.end())
                    {
                        metadataValid = false;
                        break;
                    }
                }
            }

            const bool validLanguages =
                localeOrderValid &&
                metadataValid &&
                !defaultLocale.empty() &&
                !localeOrder.empty() &&
                std::find(localeOrder.begin(), localeOrder.end(), "en") != localeOrder.end() &&
                fileRoot.contains("languages") &&
                readTranslationTable(fileRoot["languages"], languages, true);

            if (!validLanguages)
            {
                reject(path, "invalid language/locale metadata registry or missing English fallback");
                continue;
            }

            std::sort(localeOrder.begin(), localeOrder.end());
            if (std::adjacent_find(localeOrder.begin(), localeOrder.end()) != localeOrder.end())
            {
                reject(path, "duplicate locale in locale_order");
                continue;
            }
            // Preserve authored cycle order after duplicate validation.
            localeOrder.clear();
            for (const auto& value : fileRoot["locale_order"])
                localeOrder.push_back(value.get<std::string>());

            m_defaultLocale = defaultLocale;
            m_localeOrder = std::move(localeOrder);
            m_languages = std::move(languages);
            m_localeMetadata = std::move(localeMetadata);
            languagesLoaded = true;
            ++m_loadedFileCount;
            continue;
        }

        if (kind == "ui_strings")
        {
            TranslationTable table;
            if (!fileRoot.contains("strings") ||
                !readTranslationTable(fileRoot["strings"], table, true))
            {
                reject(path, "invalid UI translation table or missing English fallback");
                continue;
            }
            mergeUi(path, table);
            ++m_loadedFileCount;
            continue;
        }

        if (kind == "catalog")
        {
            const std::string domain = fileRoot.value("domain", std::string());
            TranslationTable table;
            if (domain.empty() || !fileRoot.contains("entries") ||
                !readTranslationTable(fileRoot["entries"], table, true))
            {
                reject(path, "invalid catalog domain/entries");
                continue;
            }
            mergeCatalog(path, domain, table);
            ++m_loadedFileCount;
            continue;
        }

        if (kind == "star_system")
        {
            std::string systemId;
            if (fileRoot.contains("system_id") && fileRoot["system_id"].is_string())
                systemId = fileRoot["system_id"].get<std::string>();
            else if (fileRoot.contains("system_id") && fileRoot["system_id"].is_number_integer())
                systemId = std::to_string(fileRoot["system_id"].get<long long>());

            TranslationMap systemNames;
            if (systemId.empty() || !fileRoot.contains("names") ||
                !readTranslationMap(fileRoot["names"], systemNames) ||
                systemNames.find("en") == systemNames.end())
            {
                reject(path, "invalid star-system ID/names");
                continue;
            }

            TranslationTable bodyTable;
            TranslationTable hubTable;
            if (fileRoot.contains("bodies") &&
                !readTranslationTable(fileRoot["bodies"], bodyTable, true))
            {
                reject(path, "invalid celestial-body table");
                continue;
            }
            if (fileRoot.contains("hubs") &&
                !readTranslationTable(fileRoot["hubs"], hubTable, true))
            {
                reject(path, "invalid hub table");
                continue;
            }

            const std::string bodyPrefix = systemId + ':';
            bool wrongSystem = false;
            for (const auto& [stableId, _] : bodyTable)
            {
                if (stableId.rfind(bodyPrefix, 0) != 0)
                {
                    wrongSystem = true;
                    break;
                }
            }
            if (wrongSystem)
            {
                reject(path, "contains a body stable ID belonging to another system");
                continue;
            }

            // Star-system files are isolation units: a semantic conflict in one
            // file rejects that whole file rather than partially mixing two
            // authored versions of the same system.
            const auto systemDomainIt = m_catalogSources.find("systems");
            if (systemDomainIt != m_catalogSources.end() &&
                systemDomainIt->second.find(systemId) != systemDomainIt->second.end())
            {
                reject(path, "duplicate star-system ID " + systemId);
                continue;
            }

            bool duplicateContainedObject = false;
            const auto bodyDomainIt = m_catalogSources.find("bodies");
            if (bodyDomainIt != m_catalogSources.end())
            {
                for (const auto& [stableId, _] : bodyTable)
                {
                    if (bodyDomainIt->second.find(stableId) != bodyDomainIt->second.end())
                    {
                        duplicateContainedObject = true;
                        break;
                    }
                }
            }
            const auto hubDomainIt = m_catalogSources.find("hubs");
            if (!duplicateContainedObject && hubDomainIt != m_catalogSources.end())
            {
                for (const auto& [stableId, _] : hubTable)
                {
                    if (hubDomainIt->second.find(stableId) != hubDomainIt->second.end())
                    {
                        duplicateContainedObject = true;
                        break;
                    }
                }
            }
            if (duplicateContainedObject)
            {
                reject(path, "duplicates a celestial-body/hub stable ID from another file");
                continue;
            }

            TranslationTable oneSystem;
            oneSystem.emplace(systemId, std::move(systemNames));
            mergeCatalog(path, "systems", oneSystem);
            mergeCatalog(path, "bodies", bodyTable);
            mergeCatalog(path, "hubs", hubTable);
            ++m_loadedFileCount;
            continue;
        }

        if (kind == "sky_culture_names")
        {
            const std::string cultureId = fileRoot.value("culture_id", std::string());
            TranslationMap names;
            if (cultureId.empty() || !fileRoot.contains("names") ||
                !readTranslationMap(fileRoot["names"], names) ||
                names.find("en") == names.end())
            {
                reject(path, "invalid sky-culture names");
                continue;
            }
            TranslationTable one;
            one.emplace(cultureId, std::move(names));
            mergeCatalog(path, "sky_cultures", one);
            ++m_loadedFileCount;
            continue;
        }

        if (kind == "sky_constellation_names")
        {
            const std::string cultureId = fileRoot.value("culture_id", std::string());
            TranslationTable entries;
            if (cultureId.empty() || !fileRoot.contains("entries") ||
                !readTranslationTable(fileRoot["entries"], entries, true))
            {
                reject(path, "invalid constellation-name table");
                continue;
            }
            mergeCatalog(path, "sky_constellations/" + cultureId, entries);
            ++m_loadedFileCount;
            continue;
        }

        if (kind == "navigation_regions")
        {
            // NavigationRegionCatalog owns faction/cell resolution, but this
            // file is still parsed here so corrupt localization JSON is visible
            // during the same recursive scan.
            if (!fileRoot.contains("regions") || !fileRoot["regions"].is_array())
            {
                reject(path, "invalid navigation-region table");
                continue;
            }
            ++m_loadedFileCount;
            continue;
        }

        reject(path, "unknown localization kind: " + kind);
    }

    if (!languagesLoaded)
    {
        std::cerr << "[Localization] no valid languages.json found under "
                  << rootPath << '\n';
        return false;
    }

    if (std::find(m_localeOrder.begin(), m_localeOrder.end(), m_locale) ==
        m_localeOrder.end())
    {
        m_locale = m_defaultLocale;
    }

    std::cout << "[Localization] root=" << rootPath
              << " loaded=" << m_loadedFileCount
              << " skipped=" << m_skippedFileCount
              << " ui_keys=" << m_uiStrings.size()
              << " catalog_domains=" << m_catalogNames.size() << '\n';

    return !m_uiStrings.empty();
}

bool LocalizationService::setLocale(const std::string& locale)
{
    const auto it = std::find(m_localeOrder.begin(), m_localeOrder.end(), locale);
    if (it == m_localeOrder.end())
        return false;
    m_locale = *it;
    return true;
}

const std::string& LocalizationService::cycleLocale()
{
    if (m_localeOrder.empty())
        return m_locale;

    auto it = std::find(m_localeOrder.begin(), m_localeOrder.end(), m_locale);
    std::size_t index = 0;
    if (it != m_localeOrder.end())
    {
        index = static_cast<std::size_t>(std::distance(m_localeOrder.begin(), it));
        index = (index + 1) % m_localeOrder.size();
    }

    m_locale = m_localeOrder[index];
    return m_locale;
}

std::string LocalizationService::resolve(
    const TranslationMap& translations,
    const std::string& fallback
) const
{
    auto find = [&](const std::string& locale) -> const std::string*
    {
        const auto it = translations.find(locale);
        return it == translations.end() ? nullptr : &it->second;
    };

    if (const std::string* exact = find(m_locale))
        return *exact;

    const std::string base = baseLocale(m_locale);
    if (base != m_locale)
    {
        if (const std::string* value = find(base))
            return *value;
    }

    if (const std::string* english = find("en"))
        return *english;

    return fallback;
}

std::string LocalizationService::text(
    const std::string& key,
    const std::string& englishFallback
) const
{
    const auto it = m_uiStrings.find(key);
    if (it == m_uiStrings.end())
        return englishFallback.empty() ? key : englishFallback;
    return resolve(it->second, englishFallback.empty() ? key : englishFallback);
}

std::string LocalizationService::catalogName(
    const std::string& domain,
    const std::string& stableId,
    const std::string& englishFallback
) const
{
    const auto domainIt = m_catalogNames.find(domain);
    if (domainIt == m_catalogNames.end())
        return englishFallback;

    const auto itemIt = domainIt->second.find(stableId);
    if (itemIt == domainIt->second.end())
        return englishFallback;

    return resolve(itemIt->second, englishFallback);
}

std::string LocalizationService::languageDisplayName() const
{
    const auto it = m_languages.find(m_locale);
    return it == m_languages.end()
        ? m_locale
        : resolve(it->second, m_locale);
}

std::string LocalizationService::languageIndicator() const
{
    std::string value = m_locale;
    const std::size_t split = value.find_first_of("-_");
    if (split != std::string::npos)
        value.resize(split);

    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string LocalizationService::localeDirection() const
{
    auto find = [&](const std::string& locale) -> const LocaleMetadata*
    {
        const auto it = m_localeMetadata.find(locale);
        return it == m_localeMetadata.end() ? nullptr : &it->second;
    };

    if (const LocaleMetadata* exact = find(m_locale))
        return exact->direction;

    const std::string base = baseLocale(m_locale);
    if (base != m_locale)
    {
        if (const LocaleMetadata* value = find(base))
            return value->direction;
    }
    return "ltr";
}

std::string LocalizationService::localeScript() const
{
    auto find = [&](const std::string& locale) -> const LocaleMetadata*
    {
        const auto it = m_localeMetadata.find(locale);
        return it == m_localeMetadata.end() ? nullptr : &it->second;
    };

    if (const LocaleMetadata* exact = find(m_locale))
        return exact->script;

    const std::string base = baseLocale(m_locale);
    if (base != m_locale)
    {
        if (const LocaleMetadata* value = find(base))
            return value->script;
    }
    return std::string();
}

std::string LocalizationService::webUiBundleJson() const
{
    json root;
    root["version"] = 2;
    root["default_locale"] = m_defaultLocale;
    root["locale_order"] = m_localeOrder;
    root["languages"] = m_languages;

    json metadata = json::object();
    for (const auto& [locale, value] : m_localeMetadata)
    {
        metadata[locale] = {
            {"native_name", value.nativeName},
            {"english_name", value.englishName},
            {"direction", value.direction},
            {"script", value.script}
        };
    }
    root["locale_metadata"] = std::move(metadata);
    root["strings"] = m_uiStrings;
    return root.dump();
}
}
