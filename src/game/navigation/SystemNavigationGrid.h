#pragma once

#include "src/game/navigation/CubicNavigationGrid.h"

namespace game::navigation
{

class SystemNavigationGrid final : public CubicNavigationGrid
{
public:
    

    static constexpr int MinimumLevel = 0;
    static constexpr int InitialLevel = 0;

    static constexpr double AuPerLightYear =
        63241.077084266;










public:
    SystemNavigationGrid();

    void activateSystem(int systemId);
    int systemId() const;

private:
    static CubicNavigationGridDefinition makeDefinition(int systemId);

private:
    int m_systemId = -1;
};

} // namespace game::navigation
