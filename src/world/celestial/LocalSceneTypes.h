#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/scene/EntityID.h"
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
    ObjectType typeId = ObjectType::None;

    std::string stableId;
    std::string parentStableId;
    std::string name;
    std::string kind;

    LocalSceneObjectClass objectClass = LocalSceneObjectClass::None;
    LocalSceneObjectOrigin origin = LocalSceneObjectOrigin::Runtime;
    LocalSceneObjectRole role = LocalSceneObjectRole::Context;
    LocalSceneCoordinateSpace coordinateSpace =
        LocalSceneCoordinateSpace::SystemWorldMeters;

    std::optional<ProceduralObjectKey> proceduralKey;

    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    LocalSceneAxes axes;

    glm::dvec3 sizeMeters {1.0, 1.0, 1.0};
    double boundingRadiusMeters = 0.0;

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
