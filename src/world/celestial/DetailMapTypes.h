#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace world::celestial
{

/*
    Details is a scene, not a synonym for "planet map".

    SpatialVolume  - a terminal navigation cube, including empty space.
    CelestialBody  - a body-centred scene: star, planet, moon or asteroid.
    LocalObject    - a scene centred on a ship or infrastructure object.
*/
enum class DetailSceneKind
{
    None = 0,
    SpatialVolume,
    CelestialBody,
    LocalObject
};

/*
    Top-level simulation classes used by Details.

    Asteroids belong to CelestialBody. A mine, base, beacon, relay or station
    installed on an asteroid belongs to Hub: infrastructure that follows its
    assigned orbit/trajectory and cannot depart under its own propulsion.
*/
enum class DetailObjectClass
{
    None = 0,
    CelestialBody,
    Ship,
    Hub
};

enum class DetailObjectOrigin
{
    Authored = 0,
    Runtime,
    Procedural,
    MaterializedProcedural
};

/*
    Stable identity for a procedural object without a database row.
    A persistent override is created only after discovery, ownership,
    construction, mining, damage or another durable modification.
*/
struct ProceduralObjectKey
{
    std::string generatorId;

    std::int64_t chunkX = 0;
    std::int64_t chunkY = 0;
    std::int64_t chunkZ = 0;

    std::uint32_t localIndex = 0;
    std::uint32_t generationVersion = 0;

    bool valid() const
    {
        return !generatorId.empty();
    }

    std::string stableId() const
    {
        if (!valid())
            return {};

        return
            "proc:" + generatorId + ":v" +
            std::to_string(generationVersion) + ":" +
            std::to_string(chunkX) + ":" +
            std::to_string(chunkY) + ":" +
            std::to_string(chunkZ) + ":" +
            std::to_string(localIndex);
    }
};

struct DetailSpatialCell
{
    int level = -1;
    int maximumLevel = -1;

    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    // System-barycentric coordinates.
    glm::dvec3 centerAu {0.0};
    double edgeAu = 0.0;

    bool valid() const
    {
        return level >= 0 &&
            maximumLevel >= level &&
            edgeAu > 0.0;
    }

    bool terminal() const
    {
        return valid() && level == maximumLevel;
    }
};

struct DetailTarget
{
    DetailSceneKind sceneKind = DetailSceneKind::None;
    DetailObjectClass focusClass = DetailObjectClass::None;

    int systemId = -1;
    glm::dvec3 systemPositionLy {0.0};

    // Scene anchor: body id for CelestialBody, ship/hub id for LocalObject.
    std::string anchorId;

    // Selected object inside the scene. It may differ from anchorId, e.g. an
    // orbital hub focused inside its parent planet scene.
    std::string focusId;

    DetailSpatialCell spatialCell;

    bool valid() const
    {
        if (systemId < 0)
            return false;

        switch (sceneKind)
        {
            case DetailSceneKind::SpatialVolume:
                return spatialCell.terminal();

            case DetailSceneKind::CelestialBody:
            case DetailSceneKind::LocalObject:
                return !anchorId.empty();

            default:
                return false;
        }
    }
};

inline bool operator==(
    const DetailSpatialCell& a,
    const DetailSpatialCell& b
)
{
    return
        a.level == b.level &&
        a.maximumLevel == b.maximumLevel &&
        a.x == b.x &&
        a.y == b.y &&
        a.z == b.z &&
        a.centerAu.x == b.centerAu.x &&
        a.centerAu.y == b.centerAu.y &&
        a.centerAu.z == b.centerAu.z &&
        a.edgeAu == b.edgeAu;
}

inline bool operator==(
    const DetailTarget& a,
    const DetailTarget& b
)
{
    return
        a.sceneKind == b.sceneKind &&
        a.focusClass == b.focusClass &&
        a.systemId == b.systemId &&
        a.systemPositionLy.x == b.systemPositionLy.x &&
        a.systemPositionLy.y == b.systemPositionLy.y &&
        a.systemPositionLy.z == b.systemPositionLy.z &&
        a.anchorId == b.anchorId &&
        a.focusId == b.focusId &&
        a.spatialCell == b.spatialCell;
}

inline bool operator!=(
    const DetailTarget& a,
    const DetailTarget& b
)
{
    return !(a == b);
}

} // namespace world::celestial
