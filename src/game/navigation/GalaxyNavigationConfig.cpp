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
    GalaxyNavigationConfig result;

    result.allowedRootCells =
    {
        {-1, 1,  0},
        {-1, 2, -1},
        {-1, 2,  0},
        { 0, 0,  0},
        { 0, 1,  0}
    };

    return result;
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

    result.minimumLevel =
        std::max(1, galaxy.value(
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

    const json display =
        root.value("coordinateDisplay", json::object());

    result.defaultCoordinateFormat =
        display.value(
            "defaultFormat",
            result.defaultCoordinateFormat
        );

    const json rootNavigation =
        root.value("rootNavigation", json::object());

    const json allowed =
        rootNavigation.value(
            "allowedCells",
            json::array()
        );

    if (!allowed.empty())
    {
        std::vector<std::array<std::int64_t, 3>> parsed;

        for (const json& item : allowed)
        {
            if (!item.is_array() || item.size() != 3)
                continue;

            parsed.push_back({
                item[0].get<std::int64_t>(),
                item[1].get<std::int64_t>(),
                item[2].get<std::int64_t>()
            });
        }

        if (!parsed.empty())
            result.allowedRootCells = std::move(parsed);
    }

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

bool GalaxyNavigationConfig::isRootAllowed(
    std::int64_t x,
    std::int64_t y,
    std::int64_t z
) const
{
    return std::find(
        allowedRootCells.begin(),
        allowedRootCells.end(),
        std::array<std::int64_t, 3>{x, y, z}
    ) != allowedRootCells.end();
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
