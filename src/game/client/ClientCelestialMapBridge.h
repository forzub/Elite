#pragma once

#include <string>

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::client
{

inline const world::celestial::CelestialBodyState*
findCelestialBodyState(
    const world::celestial::CelestialSystemSnapshot& celestial,
    const std::string& bodyId
)
{
    for (const auto& body : celestial.bodies)
    {
        if (body.id == bodyId)
            return &body;
    }

    return nullptr;
}

/*
    Migration bridge: predictable celestial presentation is reconstructed on
    the client from canonical universe time. Dynamic map geometry remains on
    the existing server snapshot path until its own migration stage.

    Intentionally update only time/orientation fields here. Position/velocity
    stay at the dynamic map snapshot epoch so a partially migrated frame can
    never mix a new planet translation with old hub/ship positions.
*/
inline bool applyClientCelestialPresentation(
    world::celestial::DetailMapSnapshot& detail,
    const world::celestial::CelestialSystemSnapshot& celestial
)
{
    if (!detail.valid ||
        detail.systemId != celestial.systemId ||
        detail.planetBodyId.empty())
    {
        return false;
    }

    const auto* body =
        findCelestialBodyState(
            celestial,
            detail.planetBodyId
        );

    if (!body)
        return false;

    detail.universeTimeSeconds = celestial.simTimeSeconds;
    detail.planetRotationPhaseRad = body->rotationPhaseRad;
    detail.planetDayLengthHours = body->dayLengthHours;
    detail.planetRotationDirection = body->rotationDirection;
    detail.planetAxialTiltDeg = body->axialTiltDeg;
    detail.planetAxisNodeDeg = body->axisNodeDeg;
    detail.planetTextureLongitudeOffsetDeg =
        body->textureLongitudeOffsetDeg;

    return true;
}

inline bool applyClientCelestialPresentation(
    world::celestial::HubMapSnapshot& hub,
    const world::celestial::CelestialSystemSnapshot& celestial
)
{
    if (!hub.valid ||
        hub.systemId != celestial.systemId ||
        hub.parentBodyId.empty())
    {
        return false;
    }

    const auto* body =
        findCelestialBodyState(
            celestial,
            hub.parentBodyId
        );

    if (!body)
        return false;

    hub.universeTimeSeconds = celestial.simTimeSeconds;
    hub.parentPlanetRotationPhaseRad = body->rotationPhaseRad;
    hub.parentPlanetAxialTiltDeg = body->axialTiltDeg;
    hub.parentPlanetAxisNodeDeg = body->axisNodeDeg;
    hub.parentPlanetTextureLongitudeOffsetDeg =
        body->textureLongitudeOffsetDeg;

    return true;
}

} // namespace game::client
