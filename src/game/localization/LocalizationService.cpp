#include "src/game/localization/LocalizationService.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace game::localization
{
namespace
{
using json = nlohmann::json;

bool readTranslationMap(
    const json& source,
    LocalizationService::TranslationMap& out
)
{
    if (!source.is_object())
        return false;

    for (auto it = source.begin(); it != source.end(); ++it)
    {
        if (it.value().is_string())
            out[it.key()] = it.value().get<std::string>();
    }

    return !out.empty();
}

bool readTranslationTable(
    const json& source,
    LocalizationService::TranslationTable& out
)
{
    if (!source.is_object())
        return false;

    for (auto it = source.begin(); it != source.end(); ++it)
    {
        LocalizationService::TranslationMap translations;
        if (readTranslationMap(it.value(), translations))
            out[it.key()] = std::move(translations);
    }

    return true;
}

std::string baseLocale(const std::string& locale)
{
    const std::size_t split = locale.find_first_of("-_");
    return split == std::string::npos ? locale : locale.substr(0, split);
}
}

bool LocalizationService::load(
    const std::string& uiStringsPath,
    const std::string& catalogNamesPath
)
{
    json uiRoot;
    json catalogRoot;

    try
    {
        std::ifstream uiInput(uiStringsPath);
        std::ifstream catalogInput(catalogNamesPath);
        if (!uiInput.is_open() || !catalogInput.is_open())
            return false;

        uiInput >> uiRoot;
        catalogInput >> catalogRoot;
    }
    catch (const std::exception&)
    {
        return false;
    }

    const std::string defaultLocale =
        uiRoot.value("default_locale", std::string("en"));
    if (defaultLocale.empty())
        return false;

    std::vector<std::string> localeOrder;
    if (uiRoot.contains("locale_order") && uiRoot["locale_order"].is_array())
    {
        for (const auto& locale : uiRoot["locale_order"])
        {
            if (locale.is_string() && !locale.get<std::string>().empty())
                localeOrder.push_back(locale.get<std::string>());
        }
    }
    if (localeOrder.empty())
        localeOrder.push_back(defaultLocale);
    if (std::find(localeOrder.begin(), localeOrder.end(), "en") == localeOrder.end())
        return false;

    TranslationTable uiStrings;
    TranslationTable languages;
    if (!uiRoot.contains("strings") ||
        !readTranslationTable(uiRoot["strings"], uiStrings))
    {
        return false;
    }
    if (uiRoot.contains("languages"))
        readTranslationTable(uiRoot["languages"], languages);

    std::unordered_map<std::string, TranslationTable> catalogNames;
    if (catalogRoot.contains("domains") && catalogRoot["domains"].is_object())
    {
        for (auto it = catalogRoot["domains"].begin();
             it != catalogRoot["domains"].end(); ++it)
        {
            TranslationTable table;
            if (readTranslationTable(it.value(), table))
                catalogNames[it.key()] = std::move(table);
        }
    }

    m_defaultLocale = defaultLocale;
    m_localeOrder = std::move(localeOrder);
    m_uiStrings = std::move(uiStrings);
    m_languages = std::move(languages);
    m_catalogNames = std::move(catalogNames);

    if (std::find(m_localeOrder.begin(), m_localeOrder.end(), m_locale) ==
        m_localeOrder.end())
    {
        m_locale = m_defaultLocale;
    }

    return true;
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

    // English is the mandatory fallback language for all player-facing UI.
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
}
