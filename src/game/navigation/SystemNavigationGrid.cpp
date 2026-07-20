#include "src/game/navigation/SystemNavigationGrid.h"

#include <string>

namespace game::navigation
{

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

    result.frame.id =
        "system_barycentric:" + std::to_string(systemId);

    result.frame.unit = "AU";
    result.frame.origin = glm::dvec3(0.0);
    result.frame.axisX = glm::dvec3(1.0, 0.0, 0.0);
    result.frame.axisY = glm::dvec3(0.0, 1.0, 0.0);
    result.frame.axisZ = glm::dvec3(0.0, 0.0, 1.0);

    result.subdivision = Subdivision;
    result.minimumLevel = MinimumLevel;
    result.initialLevel = InitialLevel;
    result.maximumLevel = MaximumLevel;
    result.baseCellSize = BaseCellSizeAu;

    return result;
}

} // namespace game::navigation
