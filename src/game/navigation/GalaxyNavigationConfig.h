#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace game::navigation
{

struct GalaxyNavigationConfig
{
    static constexpr double KilometersPerLightYear =
        9.4607304725808e12;

    static constexpr double KilometersPerAu =
        149597870.7;

    int subdivisionPerAxis = 25;
    int minimumLevel = 1;
    int initialLevel = 1;
    int maximumDepth = 9;

    double terminalCellEdgeKm = 100.0;
    double systemEntryRequiredEdgeAu = 3400.0;

    std::string defaultCoordinateFormat =
        "hierarchical";

    std::vector<std::array<std::int64_t, 3>>
        allowedRootCells;

    double rootEdgeLy() const;
    double cellEdgeLy(int level) const;
    int galaxyMaximumLevel() const;

    bool isRootAllowed(
        std::int64_t x,
        std::int64_t y,
        std::int64_t z
    ) const;

    static GalaxyNavigationConfig loadFromRuntimeOrSource(
        const std::string& runtimePath,
        const std::string& sourcePath
    );
};

} // namespace game::navigation
