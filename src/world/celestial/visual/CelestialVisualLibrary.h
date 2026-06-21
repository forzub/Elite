#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "CelestialVisualTypes.h"

namespace world::celestial::visual
{

class CelestialVisualLibrary
{
public:
    bool load(
        const std::string& presetsPath,
        const std::string& bodyVisualsRoot
    );

    const std::vector<CelestialBodyVisualDescriptor>& bodies() const
    {
        return m_bodies;
    }

private:
    std::unordered_map<std::string, CelestialVisualPreset> m_presets;
    std::vector<CelestialBodyVisualDescriptor> m_bodies;
};

} // namespace world::celestial::visual