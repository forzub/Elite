#pragma once

#include "src/game/navigation/CubicNavigationGrid.h"

namespace game::navigation
{

class SystemNavigationGrid final : public CubicNavigationGrid
{
public:
    static constexpr int Subdivision = 27;
    static constexpr int MinimumLevel = 0;
    static constexpr int InitialLevel = 1;
    static constexpr int MaximumLevel = 6;

    static constexpr double AuPerLightYear = 63241.077084266;

    /*
        Same physical edge as the terminal Galaxy cell:
            27 ly / 9^4 ~= 260.25 AU.

        Six divisions by 27 lead to:
            S6 ~= 100.49 km.
    */
    static constexpr double BaseCellSizeAu =
        (27.0 / 6561.0) * AuPerLightYear;

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
