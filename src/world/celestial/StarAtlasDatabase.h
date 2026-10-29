#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/world/celestial/CelestialTypes.h"

namespace world::celestial
{

class StarAtlasDatabase
{
public:
    // Bump only when the parser/semantic contract for the static physical
    // catalog changes incompatibly. Content changes are detected separately
    // through contentFingerprint().
    // 2: per-system physical catalog (systems/, distant_systems/) produced by
    //    src/world/generation, Keplerian orbits, barycenter nodes.
    static constexpr std::uint32_t CatalogSchemaVersion = 2;

    /*
        Loads the physical catalog (one directory per system, see
        galaxy_details/README.md):
            <galaxyDetailsRoot>/systems/<NNN>_<slug>/
            <galaxyDetailsRoot>/distant_systems/<id>_<slug>/
            <galaxyDetailsRoot>/objects_details
        This is the one runtime schema; the legacy flat catalog is read only
        by the migration tools in tools/universe_generator.

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

    // Runtime/source-tree bootstrap used independently by both client and
    // server. Static StarAtlas data is not a network payload.
    bool loadFromRuntimeOrSource();

    std::uint64_t contentFingerprint() const
    {
        return m_contentFingerprint;
    }

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
    std::uint64_t m_contentFingerprint = 0;
};

} // namespace world::celestial
