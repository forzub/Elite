#pragma once

#include <string>
#include <unordered_map>
#include <utility>

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/SystemMapConversion.h"
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
    Stage-3 System-map seam.

    The response carries only the authoritative map epoch/system identity.
    Celestial bodies are fully deterministic for that epoch, so the client
    rebuilds this presentation layer from its own catalog/runtime. Production
    ships, hubs and infrastructure are joined separately from ordinary
    SimulationSnapshot history at the same server-time response epoch.
*/
inline bool rebuildSystemMapCelestialLayer(
    world::celestial::SystemMapSnapshot& map,
    const world::celestial::CelestialSystemDefinition& definition,
    const world::celestial::CelestialSystemSnapshot& celestial
)
{
    using namespace world::celestial;

    if (map.systemId < 0 ||
        definition.systemId != map.systemId ||
        celestial.systemId != map.systemId ||
        celestial.simTimeSeconds != map.universeTimeSeconds)
    {
        return false;
    }

    std::unordered_map<
        std::string,
        const CelestialBodyState*
    > runtimeById;

    runtimeById.reserve(celestial.bodies.size());
    for (const auto& state : celestial.bodies)
        runtimeById[state.id] = &state;

    std::vector<SystemMapBody> rebuiltBodies;
    rebuiltBodies.reserve(definition.bodies.size());

    for (const auto& body : definition.bodies)
    {
        SystemMapBody item;

        item.id = body.id;
        item.name = body.name;
        item.alternativeNames = body.alternativeNames;
        item.parentId = body.parentId;
        item.environmentPresetId = body.environmentPresetId;
        item.type = body.type;
        item.radiusKm = body.radiusKm;
        item.orbitalPeriodDays = body.orbitalPeriodDays;
        item.orbitalDirection = body.orbitalDirection;
        item.orbitalPhaseOffsetRad =
            body.orbitalPhaseOffsetDeg *
            3.14159265358979323846 /
            180.0;
        item.rotationPhaseRad =
            body.rotationOffsetDeg *
            3.14159265358979323846 /
            180.0;
        item.dayLengthHours = body.dayLengthHours;
        item.rotationDirection = body.rotationDirection;
        item.axialTiltDeg = body.axialTiltDeg;
        item.axisNodeDeg = body.axisNodeDeg;
        item.textureLongitudeOffsetDeg =
            body.textureLongitudeOffsetDeg;

        const auto stateIt = runtimeById.find(body.id);
        if (stateIt != runtimeById.end())
        {
            const auto& state = *stateIt->second;
            item.positionAu = state.positionAu;
            item.rotationPhaseRad = state.rotationPhaseRad;
            item.dayLengthHours = state.dayLengthHours;
            item.rotationDirection = state.rotationDirection;
            item.axialTiltDeg = state.axialTiltDeg;
            item.axisNodeDeg = state.axisNodeDeg;
            item.textureLongitudeOffsetDeg =
                state.textureLongitudeOffsetDeg;
        }
        else
        {
            item.positionAu = body.staticPositionAu;
        }

        if (!body.parentId.empty())
        {
            const auto parentIt = runtimeById.find(body.parentId);
            if (parentIt != runtimeById.end())
                item.orbitCenterAu = parentIt->second->positionAu;
        }

        item.orbitRadiusAu = body.distanceAu;
        item.drawOrbit = body.distanceAu > 0.0;
        item.ringPlaneInclinationOffsetDeg =
            body.ringPlaneInclinationOffsetDeg;
        item.ringVisual =
            toSystemMapRingVisualProfile(body.ringVisual);

        item.rings.reserve(body.rings.size());
        for (const auto& ring : body.rings)
            item.rings.push_back(toSystemMapRing(ring));

        rebuiltBodies.push_back(std::move(item));
    }

    map.bodies = std::move(rebuiltBodies);
    return true;
}

inline bool rebuildSystemMapCelestialLayer(
    world::celestial::SystemMapSnapshot& map,
    const world::celestial::StarAtlasDatabase& atlas,
    const world::celestial::CelestialSystemSnapshot& celestial
)
{
    const auto* definition = atlas.findSystem(map.systemId);
    if (!definition ||
        !rebuildSystemMapCelestialLayer(map, *definition, celestial))
    {
        return false;
    }

    // Static System-map identity/galactic placement is local catalog data too.
    // The authoritative map response supplies only the requested system id and
    // dynamic epoch; it does not serialize duplicate StarAtlas fields.
    map.systemName = definition->name;
    if (const auto* summary = atlas.findSystemSummary(map.systemId))
        map.systemPositionLy = summary->positionLy;

    return true;
}

/*
    Migration bridge: predictable celestial presentation is reconstructed on
    the client from canonical universe time. Dynamic Detail/Hub geometry
    remains on the existing server snapshot path until its own Stage-3 slice.

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
