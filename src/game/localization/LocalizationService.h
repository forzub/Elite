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

    // Development representation: recursively discover every *.json file below
    // one localization root. A malformed/unsupported file is logged and skipped;
    // it never prevents unrelated localization files from loading.
    bool loadDirectory(const std::string& rootPath);

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

    // WebUI receives a generated in-memory bundle from this same service; there
    // is no second editable UI translation table under assets/webui.
    std::string webUiBundleJson() const;

    std::size_t loadedFileCount() const { return m_loadedFileCount; }
    std::size_t skippedFileCount() const { return m_skippedFileCount; }

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

    // Sources are retained only for deterministic duplicate diagnostics.
    std::unordered_map<std::string, std::string> m_uiSources;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
        m_catalogSources;

    std::size_t m_loadedFileCount = 0;
    std::size_t m_skippedFileCount = 0;
};
}
