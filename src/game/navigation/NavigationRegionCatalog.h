#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game::navigation
{

struct NavigationCellId
{
    std::string frameId;
    int level = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator==(const NavigationCellId& other) const
    {
        return
            frameId == other.frameId &&
            level == other.level &&
            x == other.x &&
            y == other.y &&
            z == other.z;
    }
};

struct NavigationRegionName
{
    std::string factionId;
    std::string language;
    std::string script;
    std::string name;
    std::string shortName;
    std::string kind;
    bool preferred = false;
};

struct NavigationRegionRecord
{
    NavigationCellId cell;
    std::vector<NavigationRegionName> names;
};

struct ResolvedNavigationRegionName
{
    NavigationCellId sourceCell;
    NavigationRegionName value;
    bool inherited = false;
};

class NavigationRegionCatalog
{
public:
    bool loadFromFile(const std::string& path);

    bool loadFromRuntimeOrSource(
        const std::string& runtimePath,
        const std::string& sourcePath
    );

    std::optional<ResolvedNavigationRegionName> resolve(
        const NavigationCellId& cell,
        int subdivision,
        const std::string& factionId,
        const std::string& locale
    ) const;

    bool loaded() const;
    const std::vector<NavigationRegionRecord>& records() const;

private:
    const NavigationRegionRecord* findExact(
        const NavigationCellId& cell
    ) const;

    static const NavigationRegionName* chooseName(
        const NavigationRegionRecord& record,
        const std::string& factionId,
        const std::string& locale
    );

private:
    bool m_loaded = false;
    std::vector<NavigationRegionRecord> m_records;
};

} // namespace game::navigation
