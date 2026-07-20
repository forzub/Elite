#pragma once

#include <iomanip>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "src/game/navigation/CubicNavigationGrid.h"

namespace game::navigation
{

inline std::string formatCellCoordinate(
    const std::string& mapPrefix,
    const CubicNavigationCell& cell
)
{
    std::ostringstream stream;

    stream
        << mapPrefix
        << cell.level
        << " · X"
        << std::showpos << cell.index.x
        << " Y"
        << std::showpos << cell.index.y
        << " Z"
        << std::showpos << cell.index.z;

    return stream.str();
}

template <typename Index>
inline std::string formatCellCoordinate(
    const std::string& mapPrefix,
    int level,
    const Index& index
)
{
    std::ostringstream stream;

    stream
        << mapPrefix
        << level
        << " · X"
        << std::showpos << index.x
        << " Y"
        << std::showpos << index.y
        << " Z"
        << std::showpos << index.z;

    return stream.str();
}

template <typename Index>
inline std::string formatHierarchicalAddress(
    const std::string& mapPrefix,
    int level,
    const Index& index,
    int subdivision
)
{
    struct Token
    {
        int x = 0;
        int y = 0;
        int z = 0;
    };

    std::vector<Token> reversed;
    Index child = index;
    const int centerDigit = subdivision / 2;

    for (int currentLevel = level; currentLevel > 0; --currentLevel)
    {
        Index parent;
        parent.x = static_cast<std::int64_t>(std::llround(
            static_cast<double>(child.x) / subdivision
        ));
        parent.y = static_cast<std::int64_t>(std::llround(
            static_cast<double>(child.y) / subdivision
        ));
        parent.z = static_cast<std::int64_t>(std::llround(
            static_cast<double>(child.z) / subdivision
        ));

        reversed.push_back({
            static_cast<int>(child.x - parent.x * subdivision) + centerDigit,
            static_cast<int>(child.y - parent.y * subdivision) + centerDigit,
            static_cast<int>(child.z - parent.z * subdivision) + centerDigit
        });

        child = parent;
    }

    int digitWidth = 1;
    for (int value = subdivision - 1; value >= 10; value /= 10)
        ++digitWidth;

    std::ostringstream stream;
    stream
        << mapPrefix
        << '[' << child.x << ',' << child.y << ',' << child.z << ']';

    for (auto it = reversed.rbegin(); it != reversed.rend(); ++it)
    {
        stream
            << '/'
            << std::setfill('0') << std::setw(digitWidth) << it->x << '.'
            << std::setfill('0') << std::setw(digitWidth) << it->y << '.'
            << std::setfill('0') << std::setw(digitWidth) << it->z;
    }

    return stream.str();
}

inline glm::dvec3 terminalOffsetKmFromAu(
    const glm::dvec3& pointAu,
    const CubicNavigationCell& terminalCell,
    double auKm = 149597870.7
)
{
    return (pointAu - terminalCell.center) * auKm;
}

inline std::string formatTerminalOffsetKm(
    const glm::dvec3& offsetKm,
    int precision = 3
)
{
    std::ostringstream stream;
    stream
        << std::fixed << std::setprecision(precision)
        << "dX=" << offsetKm.x << " km "
        << "dY=" << offsetKm.y << " km "
        << "dZ=" << offsetKm.z << " km";
    return stream.str();
}

} // namespace game::navigation
