#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/scene/EntityID.h"
#include "src/game/identity/ShipInstanceId.h"
#include "src/world/celestial/DetailMapTypes.h"
#include "src/world/types/ObjectType.h"

namespace world::celestial
{

using LocalSceneObjectClass = DetailObjectClass;
using LocalSceneObjectOrigin = DetailObjectOrigin;

enum class LocalSceneCoordinateSpace
{
    SystemWorldMeters = 0,
    AnchorLocalMeters
};

enum class LocalSceneObjectRole
{
    Context = 0,
    Anchor,
    Focus,
    Component,
    Participant
};

struct LocalSceneAxes
{
    glm::dvec3 x {1.0, 0.0, 0.0};
    glm::dvec3 y {0.0, 1.0, 0.0};
    glm::dvec3 z {0.0, 0.0, 1.0};
};

/*
    Canonical object record for local map scenes.

    There are exactly three top-level object classes:
      - CelestialBody: stars, planets, moons and asteroids;
      - Ship: self-propelled craft;
      - Hub: infrastructure bound to an orbit, trajectory or surface.

    A station module is a Hub component, not a fourth class. A mine or base
    built on an asteroid is a Hub whose parentStableId references that body.
*/
struct LocalSceneObject
{
    EntityId id {};
    ShipInstanceId shipInstanceId = 0;
    ObjectType typeId = ObjectType::None;

    std::string stableId;
    std::string parentStableId;
    std::string name;
    std::string kind;
    std::string typeName;

    LocalSceneObjectClass objectClass = LocalSceneObjectClass::None;
    LocalSceneObjectOrigin origin = LocalSceneObjectOrigin::Runtime;
    LocalSceneObjectRole role = LocalSceneObjectRole::Context;
    LocalSceneCoordinateSpace coordinateSpace =
        LocalSceneCoordinateSpace::SystemWorldMeters;

    std::optional<ProceduralObjectKey> proceduralKey;

    glm::dvec3 positionMeters {0.0};
    // velocityMps remains the velocity expressed in coordinateSpace for
    // existing local-map rendering. The explicit fields below let the
    // tactical overlay choose local vs global semantics without guessing.
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 globalVelocityMps {0.0};
    glm::dvec3 relativeVelocityMps {0.0};
    glm::dvec3 relativeVelocityWorldMps {0.0};
    bool hasGlobalVelocity = false;
    bool hasRelativeVelocity = false;
    LocalSceneAxes axes;

    // the PHYSICAL extents of the object in its own axes (3D map fit, picking, scale)
    glm::dvec3 sizeMeters {1.0, 1.0, 1.0};
    // a hub city's PLAN SHEET extents (the 2D plan view: the strip unrolled - a ring city's sheet is 7.6 km long while
    // the ring is 2.4 km across, re-audit A8 2026-11-27); zero: the same as sizeMeters
    glm::dvec3 planSheetSizeMeters {0.0};
    double boundingRadiusMeters = 0.0;

    // Hub dock traffic ship: the remaining route the traffic model executes,
    // as offsets from positionMeters in this scene's coordinate axes.
    std::vector<glm::dvec3> routeOffsetsMeters;

    bool player = false;
    bool prime = false;
    bool valid = false;
};

struct LocalSceneInventory
{
    LocalSceneObjectClass anchorClass = LocalSceneObjectClass::None;
    std::string anchorId;
    std::string focusId;

    LocalSceneCoordinateSpace coordinateSpace =
        LocalSceneCoordinateSpace::SystemWorldMeters;

    glm::dvec3 originWorldMeters {0.0};
    double halfExtentMeters = 0.0;

    std::vector<LocalSceneObject> objects;

    bool empty() const noexcept
    {
        return objects.empty();
    }
};

} // namespace world::celestial
