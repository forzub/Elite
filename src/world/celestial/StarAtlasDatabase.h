#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "src/world/celestial/CelestialTypes.h"

namespace world::celestial
{

class StarAtlasDatabase
{
public:
    /*
        Loads every JSON document from:
            <galaxyDetailsRoot>/systems_details
            <galaxyDetailsRoot>/distant_systems_details
            <galaxyDetailsRoot>/objects_details

        Local systems are published through systems(); distant quest and
        route targets are published through distantSystems(). Both are
        searchable by findSystem() and findSystemSummary().

        No manifest is used. Paths are sorted before parsing so startup is
        deterministic on every filesystem.

        Invalid individual JSON files are diagnosed and skipped. The valid
        subset is published as long as at least one local system survives.
        Missing/unreadable local directories and a catalog with zero valid
        local systems remain fatal so path errors can still trigger fallback.
    */
    bool load(
        const std::string& galaxyDetailsRoot
    );

    /*
        Lightweight catalog read used by the starfield. It reads only the
        top-level galactic summary from each system document and skips invalid
        sibling files instead of rejecting the complete starfield catalog.
    */
    static bool loadSystemSummariesFromDirectory(
        const std::string& galaxyDetailsRoot,
        std::vector<StarSystemSummary>& outSystems,
        std::string* errorMessage = nullptr
    );

    const std::vector<StarSystemSummary>& systems() const
    {
        return m_systems;
    }

    const std::vector<StarSystemSummary>& distantSystems() const
    {
        return m_distantSystems;
    }

    const std::vector<GalaxyObjectDefinition>& objects() const
    {
        return m_objects;
    }

    const CelestialSystemDefinition* findSystem(int systemId) const;
    const StarSystemSummary* findSystemSummary(int systemId) const;

private:
    std::vector<StarSystemSummary> m_systems;
    std::vector<StarSystemSummary> m_distantSystems;
    std::unordered_map<int, CelestialSystemDefinition> m_details;
    std::vector<GalaxyObjectDefinition> m_objects;
};

} // namespace world::celestial
