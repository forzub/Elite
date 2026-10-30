#include "src/game/navigation/GalaxyNavigationConfig.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace game::navigation
{
namespace
{
using json = nlohmann::json;

GalaxyNavigationConfig defaults()
{
    return GalaxyNavigationConfig {};
}

bool loadFile(
    const std::string& path,
    GalaxyNavigationConfig& result
)
{
    std::ifstream input(path);
    if (!input)
        return false;

    json root;

    try
    {
        input >> root;
    }
    catch (...)
    {
        return false;
    }

    result.subdivisionPerAxis =
        std::max(3, root.value(
            "subdivisionPerAxis",
            result.subdivisionPerAxis
        ));

    result.maximumDepth =
        std::max(1, root.value(
            "maximumDepth",
            result.maximumDepth
        ));

    result.terminalCellEdgeKm =
        std::max(0.001, root.value(
            "terminalCellEdgeKm",
            result.terminalCellEdgeKm
        ));

    const json galaxy =
        root.value("galaxy", json::object());

    // Level 0 (root cells) is the working level of the galaxy overview.
    result.minimumLevel =
        std::max(0, galaxy.value(
            "minimumLevel",
            result.minimumLevel
        ));

    result.initialLevel =
        std::max(
            result.minimumLevel,
            galaxy.value(
                "initialLevel",
                result.initialLevel
            )
        );

    result.systemEntryRequiredEdgeAu =
        std::max(0.001, galaxy.value(
            "systemEntryRequiredEdgeAu",
            result.systemEntryRequiredEdgeAu
        ));

    result.systemMembershipRadiusAu =
        std::max(0.001, galaxy.value(
            "systemMembershipRadiusAu",
            result.systemMembershipRadiusAu
        ));

    const json system =
        root.value("system", json::object());

    result.systemMaximumLevel =
        std::clamp(
            system.value(
                "maximumLevel",
                result.systemMaximumLevel
            ),
            0,
            result.maximumDepth
        );

    const json display =
        root.value("coordinateDisplay", json::object());

    result.defaultCoordinateFormat =
        display.value(
            "defaultFormat",
            result.defaultCoordinateFormat
        );

    const json rootNavigation =
        root.value("rootNavigation", json::object());

    result.localEnvelopeRadiusLy =
        std::max(
            1.0,
            rootNavigation.value(
                "localEnvelopeRadiusLy",
                result.localEnvelopeRadiusLy
            )
        );

    result.remoteEnvelopeRadiusLy =
        std::max(
            0.0,
            rootNavigation.value(
                "remoteEnvelopeRadiusLy",
                result.remoteEnvelopeRadiusLy
            )
        );

    result.initialLevel =
        std::clamp(
            result.initialLevel,
            result.minimumLevel,
            result.galaxyMaximumLevel()
        );

    return true;
}
}

double GalaxyNavigationConfig::rootEdgeLy() const
{
    double edgeKm = terminalCellEdgeKm;

    for (int i = 0; i < maximumDepth; ++i)
        edgeKm *= static_cast<double>(subdivisionPerAxis);

    return edgeKm / KilometersPerLightYear;
}

double GalaxyNavigationConfig::cellEdgeLy(int level) const
{
    level = std::clamp(level, 0, maximumDepth);

    double edgeLy = rootEdgeLy();

    for (int i = 0; i < level; ++i)
        edgeLy /= static_cast<double>(subdivisionPerAxis);

    return edgeLy;
}

int GalaxyNavigationConfig::galaxyMaximumLevel() const
{
    const double requiredEdgeLy =
        systemEntryRequiredEdgeAu *
        KilometersPerAu /
        KilometersPerLightYear;

    for (int level = minimumLevel;
         level < maximumDepth;
         ++level)
    {
        const double current = cellEdgeLy(level);
        const double child = cellEdgeLy(level + 1);

        if (current >= requiredEdgeLy &&
            child < requiredEdgeLy)
        {
            return level;
        }
    }

    return std::clamp(
        maximumDepth,
        minimumLevel,
        maximumDepth
    );
}

GalaxyNavigationConfig
GalaxyNavigationConfig::loadFromRuntimeOrSource(
    const std::string& runtimePath,
    const std::string& sourcePath
)
{
    GalaxyNavigationConfig result = defaults();

    if (!loadFile(runtimePath, result))
        loadFile(sourcePath, result);

    return result;
}

} // namespace game::navigation
