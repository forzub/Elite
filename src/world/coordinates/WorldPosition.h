#pragma once

#include <cstdint>
#include <cmath>

#include <glm/glm.hpp>

namespace world::coordinates
{

inline constexpr double MetersPerLightYear = 9460730472580800.0;
inline constexpr double LightYearsPerMeter = 1.0 / MetersPerLightYear;

inline constexpr double GalacticCellSizeLy = 1.0;
inline constexpr double GalacticCellSizeM  = MetersPerLightYear * GalacticCellSizeLy;

struct GalacticCell
{
    int64_t x = 0;
    int64_t y = 0;
    int64_t z = 0;
};

struct WorldPosition
{
    GalacticCell cell;
    glm::dvec3 localMeters {0.0, 0.0, 0.0};
};

inline void normalize(WorldPosition& p)
{
    auto normalizeAxis = [](int64_t& cellCoord, double& local)
    {
        const double shiftD = std::floor(local / GalacticCellSizeM + 0.5);

        if (shiftD == 0.0)
            return;

        const int64_t shift = static_cast<int64_t>(shiftD);
        cellCoord += shift;
        local -= static_cast<double>(shift) * GalacticCellSizeM;
    };

    normalizeAxis(p.cell.x, p.localMeters.x);
    normalizeAxis(p.cell.y, p.localMeters.y);
    normalizeAxis(p.cell.z, p.localMeters.z);
}

inline WorldPosition makeWorldPositionFromMeters(const glm::dvec3& meters)
{
    WorldPosition p;
    p.localMeters = meters;
    normalize(p);
    return p;
}




inline glm::dvec3 fullMeters(const WorldPosition& p)
{
    return glm::dvec3(
        static_cast<double>(p.cell.x),
        static_cast<double>(p.cell.y),
        static_cast<double>(p.cell.z)
    ) * GalacticCellSizeM + p.localMeters;
}

inline glm::vec3 legacyFloatMeters(const WorldPosition& p)
{
    // Только временный bridge для старого кода.
    // Не использовать для рендера и точной логики.
    return glm::vec3(p.localMeters);
}







inline WorldPosition translated(const WorldPosition& p, const glm::dvec3& deltaMeters)
{
    WorldPosition out = p;
    out.localMeters += deltaMeters;
    normalize(out);
    return out;
}

inline void addMeters(WorldPosition& p, const glm::dvec3& deltaMeters)
{
    p.localMeters += deltaMeters;
    normalize(p);
}

inline glm::dvec3 relativeMeters(
    const WorldPosition& object,
    const WorldPosition& origin
)
{
    const glm::dvec3 cellDelta(
        static_cast<double>(object.cell.x - origin.cell.x),
        static_cast<double>(object.cell.y - origin.cell.y),
        static_cast<double>(object.cell.z - origin.cell.z)
    );

    return cellDelta * GalacticCellSizeM +
           (object.localMeters - origin.localMeters);
}

inline glm::vec3 relativeMetersFloat(
    const WorldPosition& object,
    const WorldPosition& origin
)
{
    return glm::vec3(relativeMeters(object, origin));
}

inline glm::dvec3 toGalacticLy(const WorldPosition& p)
{
    return glm::dvec3(
        static_cast<double>(p.cell.x),
        static_cast<double>(p.cell.y),
        static_cast<double>(p.cell.z)
    ) * GalacticCellSizeLy + p.localMeters * LightYearsPerMeter;
}

inline double distanceMeters(
    const WorldPosition& a,
    const WorldPosition& b
)
{
    return glm::length(relativeMeters(a, b));
}

} // namespace world::coordinates