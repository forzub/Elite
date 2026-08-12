#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class SkyCultureCatalog
{
public:
    enum class StarIdentifier
    {
        BrightStarHr,
        Hipparcos
    };

    struct Constellation
    {
        std::string id;
        std::string name;
        std::unordered_map<std::string, std::string> localizedNames;
        std::vector<std::vector<int>> polylines;

        std::string displayName(const std::string& locale) const;
    };

    struct Culture
    {
        std::string id;
        std::unordered_map<std::string, std::string> localizedNames;
        StarIdentifier starIdentifier = StarIdentifier::BrightStarHr;
        std::vector<Constellation> constellations;

        std::string displayName(const std::string& locale) const;
    };

    bool loadManifest(const std::string& manifestPath);
    bool loadLocalizationDirectory(const std::string& rootPath);

    const std::vector<Culture>& cultures() const { return m_cultures; }
    const Culture* culture(std::size_t index) const;
    const Culture* cultureById(const std::string& id) const;
    std::size_t defaultCultureIndex() const { return m_defaultCultureIndex; }

private:
    static bool loadCultureFile(
        const std::string& path,
        const std::string& expectedId,
        Culture& outCulture
    );

private:
    std::vector<Culture> m_cultures;
    std::size_t m_defaultCultureIndex = 0;
};
