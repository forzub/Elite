#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/game/localization/LocalizedMessage.h"

namespace game::localization
{
class LocalizationService
{
public:
    struct LocaleMetadata
    {
        std::string nativeName;
        std::string englishName;
        std::string direction = "ltr";
        std::string script;
    };

    using TranslationMap = std::unordered_map<std::string, std::string>;
    using TranslationTable = std::unordered_map<std::string, TranslationMap>;

    // Development representation: recursively discover every *.json file below
    // one localization root. A malformed/unsupported file is logged and skipped;
    // it never prevents unrelated localization files from loading.
    bool loadDirectory(const std::string& rootPath);

    const std::string& locale() const { return m_locale; }
    const std::vector<std::string>& localeOrder() const { return m_localeOrder; }

    bool hasLocale(const std::string& locale) const;
    std::string nextLocale(const std::string& currentLocale) const;

    // Projection setter. Application/ClientModeState owns the selected mode.
    bool setLocale(const std::string& locale);

    std::string text(
        const std::string& key,
        const std::string& englishFallback = std::string()
    ) const;

    // a template with named parameters ("{pad}") in the current language (audit #7 D7.4)
    std::string format(
        const std::string& key,
        const std::vector<MessageParam>& params,
        const std::string& englishTemplateFallback = std::string()
    ) const;
    // a string from a producer (the server, the simulation): a language-neutral Message (LocalizedMessage.h) is rendered
    // in the current language, its message-valued parameters too; a plain string is returned as it is
    std::string render(const std::string& messageOrText) const;

    std::string catalogName(
        const std::string& domain,
        const std::string& stableId,
        const std::string& englishFallback
    ) const;

    std::string languageDisplayName() const;
    std::string languageIndicator() const;
    std::string localeDirection() const;
    std::string localeScript() const;
    const std::unordered_map<std::string, LocaleMetadata>& localeMetadata() const
    {
        return m_localeMetadata;
    }

    // WebUI receives a generated in-memory bundle from this same service; there
    // is no second editable UI translation table under assets/webui.
    std::string webUiBundleJson() const;

    std::size_t loadedFileCount() const { return m_loadedFileCount; }
    std::size_t skippedFileCount() const { return m_skippedFileCount; }

private:
    void applyDiscovererNames(const std::filesystem::path& path);

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
    std::unordered_map<std::string, LocaleMetadata> m_localeMetadata;
    std::unordered_map<std::string, TranslationTable> m_catalogNames;

    // Sources are retained only for deterministic duplicate diagnostics.
    std::unordered_map<std::string, std::string> m_uiSources;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
        m_catalogSources;

    std::size_t m_loadedFileCount = 0;
    std::size_t m_skippedFileCount = 0;
};
}

namespace game::localization
{
// THE CLIENT'S localization (audit #7 D7): registered by Application once its tables are loaded, for presentation
// helpers deep in the client that have no service reference. CLIENT ONLY - server / simulation code never asks it (it
// builds language-neutral Messages, LocalizedMessage.h). Null until registered: callers keep their English fallback.
inline const LocalizationService*& activeServiceSlot()
{
    static const LocalizationService* slot = nullptr;
    return slot;
}
inline const LocalizationService* activeService() { return activeServiceSlot(); }
inline void setActiveService(const LocalizationService* service) { activeServiceSlot() = service; }
// key + named parameters in the active language; `englishTemplate` when no service / no key
inline std::string tr(const std::string& key, const std::vector<MessageParam>& params, const std::string& englishTemplate)
{
    if (const auto* s = activeService())
        return s->format(key, params, englishTemplate);
    return fillTemplate(englishTemplate, params);
}
inline std::string tr(const std::string& key, const std::string& englishText)
{
    if (const auto* s = activeService())
        return s->text(key, englishText);
    return englishText;
}
// a producer's string (Message or plain) in the active language
inline std::string renderMessage(const std::string& messageOrText)
{
    if (const auto* s = activeService())
        return s->render(messageOrText);
    std::string key;
    std::vector<MessageParam> params;
    return parseMessage(messageOrText, key, params) ? key : messageOrText;
}
} // namespace game::localization
