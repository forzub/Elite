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
    bool load(
        const std::string& starSystemsPath,
        const std::string& systemDetailsPath
    );

    const std::vector<StarSystemSummary>& systems() const
    {
        return m_systems;
    }

    const CelestialSystemDefinition* findSystem(int systemId) const;
    const StarSystemSummary* findSystemSummary(int systemId) const;

private:
    bool loadStarSystems(const std::string& path);
    bool loadSystemDetails(const std::string& path);

private:
    std::vector<StarSystemSummary> m_systems;
    std::unordered_map<int, CelestialSystemDefinition> m_details;
};

} // namespace world::celestial