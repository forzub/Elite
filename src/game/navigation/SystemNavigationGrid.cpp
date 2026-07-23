#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/navigation/GalaxyNavigationConfig.h"

#include <algorithm>
#include <string>

namespace game::navigation
{


namespace
{

const GalaxyNavigationConfig& navigationConfig()
{
    static const GalaxyNavigationConfig config =
        GalaxyNavigationConfig::loadFromRuntimeOrSource(
            "assets/data/navigation/navigation_grid.json",
            "src/assets/data/navigation/navigation_grid.json"
        );

    return config;
}

int calculateSystemMaximumLevel(
    const GalaxyNavigationConfig& config,
    double rootEdgeAu
)
{
    double edgeKm =
        rootEdgeAu *
        GalaxyNavigationConfig::KilometersPerAu;

    int level = 0;

    while (
        level < config.maximumDepth &&
        edgeKm >
            config.terminalCellEdgeKm * 1.000001
    )
    {
        edgeKm /=
            static_cast<double>(
                config.subdivisionPerAxis
            );

        ++level;
    }

    return level;
}

} // namespace




SystemNavigationGrid::SystemNavigationGrid()
    : CubicNavigationGrid(makeDefinition(-1))
{
}

void SystemNavigationGrid::activateSystem(int systemIdValue)
{
    if (m_systemId == systemIdValue)
        return;

    m_systemId = systemIdValue;
    configure(makeDefinition(systemIdValue));
}

int SystemNavigationGrid::systemId() const
{
    return m_systemId;
}

CubicNavigationGridDefinition
SystemNavigationGrid::makeDefinition(int systemId)
{
    CubicNavigationGridDefinition result;
    const GalaxyNavigationConfig& config =
        navigationConfig();

    const int galaxyBoundaryLevel =
        config.galaxyMaximumLevel();

    const double rootEdgeAu =
        config.cellEdgeLy(galaxyBoundaryLevel) *
        AuPerLightYear;

    result.frame.id =
        "system_barycentric:" + std::to_string(systemId);

    result.frame.unit = "AU";
    result.frame.origin = glm::dvec3(0.0);
    result.frame.axisX = glm::dvec3(1.0, 0.0, 0.0);
    result.frame.axisY = glm::dvec3(0.0, 1.0, 0.0);
    result.frame.axisZ = glm::dvec3(0.0, 0.0, 1.0);

    result.subdivision =
        config.subdivisionPerAxis;

    result.minimumLevel =
        MinimumLevel;

    result.maximumLevel =
        calculateSystemMaximumLevel(
            config,
            rootEdgeAu
        );

    result.initialLevel =
        std::clamp(
            InitialLevel,
            result.minimumLevel,
            result.maximumLevel
        );

    result.baseCellSize =
        rootEdgeAu;

    return result;
}

} // namespace game::navigation
