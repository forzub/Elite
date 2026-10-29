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
    int systemMaximumLevel = 5;

    double terminalCellEdgeKm = 100.0;
    double systemEntryRequiredEdgeAu = 3400.0;

    // Technical navigation-domain radius, not an astrophysical boundary.
    // Outside it PlayerNavigationState is published as interstellar.
    double systemMembershipRadiusAu = 100.0;

    std::string defaultCoordinateFormat =
        "hierarchical";

    // Navigation domain (world/coordinates/GalacticGrids.h): a connected
    // local envelope around Sol plus sparse remote envelopes and corridors.
    // Empty interstellar space is navigable; content is a separate notion.
    double localEnvelopeRadiusLy = 140.0;
    double remoteEnvelopeRadiusLy = 20.0;

    double rootEdgeLy() const;
    double cellEdgeLy(int level) const;
    int galaxyMaximumLevel() const;

    static GalaxyNavigationConfig loadFromRuntimeOrSource(
        const std::string& runtimePath,
        const std::string& sourcePath
    );
};

} // namespace game::navigation
