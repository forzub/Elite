#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::localization
{
class LocalizationService
{
public:
    using TranslationMap = std::unordered_map<std::string, std::string>;
    using TranslationTable = std::unordered_map<std::string, TranslationMap>;

    bool load(
        const std::string& uiStringsPath,
        const std::string& catalogNamesPath
    );

    const std::string& locale() const { return m_locale; }
    const std::vector<std::string>& localeOrder() const { return m_localeOrder; }

    bool setLocale(const std::string& locale);
    const std::string& cycleLocale();

    std::string text(
        const std::string& key,
        const std::string& englishFallback = std::string()
    ) const;

    std::string catalogName(
        const std::string& domain,
        const std::string& stableId,
        const std::string& englishFallback
    ) const;

    std::string languageDisplayName() const;
    std::string languageIndicator() const;

private:
    std::string resolve(
        const TranslationMap& translations,
        const std::string& fallback
    ) const;

private:
    std::string m_defaultLocale = "en";
    std::string m_locale = "en";
    std::vector<std::string> m_localeOrder {"en"};

    TranslationTable m_uiStrings;
    TranslationTable m_languages;
    std::unordered_map<std::string, TranslationTable> m_catalogNames;
};
}
