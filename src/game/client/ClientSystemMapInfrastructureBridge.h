#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/game/client/ClientSystemMapInfrastructureSampler.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::client
{

inline world::celestial::SystemMapObjectKind systemMapObjectKindFor(
    ObjectType type
) noexcept
{
    switch (type)
    {
        case ObjectType::Station:
            return world::celestial::SystemMapObjectKind::Station;
        default:
            return world::celestial::SystemMapObjectKind::Unknown;
    }
}

inline void applySystemMapOrbit(
    world::celestial::SystemMapObject& object,
    const world::orbits::OrbitalMotion& motion
)
{
    if (!motion.enabled)
        return;

    object.hasOrbit = true;
    object.orbitCenterAu =
        motion.centerMeters /
        world::celestial::MetersPerAu;
    object.orbitRadiusAu =
        (motion.parentRadiusMeters + motion.altitudeMeters) /
        world::celestial::MetersPerAu;
    object.orbitInclinationDeg = motion.inclinationDeg;
    object.orbitLongitudeOfAscendingNodeDeg =
        motion.longitudeOfAscendingNodeDeg;
    object.orbitArgumentOfPeriapsisDeg =
        motion.argumentOfPeriapsisDeg;
}

/*
    Compose the System-map infrastructure layer from normal authoritative
    replication. The server supplies world facts (instance identity, owner,
    parent binding and transforms); this bridge decides how those facts become
    SystemMapObject presentation rows.
*/
inline void rebuildSystemMapInfrastructureLayer(
    world::celestial::SystemMapSnapshot& map,
    const SystemMapInfrastructureSampleResult& sample
)
{
    using world::celestial::SystemMapObject;
    using world::celestial::SystemMapObjectKind;

    std::unordered_map<std::string, const SystemMapHubSample*> hubById;
    hubById.reserve(sample.hubs.size());
    for (const auto& hub : sample.hubs)
        hubById[hub.id] = &hub;

    for (const auto& objectState : sample.objects)
    {
        if (!objectState.navigationVisible ||
            objectState.systemId != map.systemId)
        {
            continue;
        }

        SystemMapObject object;
        object.id = objectState.id;
        object.stableId =
            "entity:" + std::to_string(objectState.id.value);
        object.name = objectState.displayName;
        object.owner = objectState.ownerName;
        object.parentBodyId = objectState.parentBodyId;
        object.parentHubId = objectState.hubId;
        object.kind = systemMapObjectKindFor(objectState.type);
        object.positionAu =
            world::coordinates::fullMeters(objectState.worldPosition) /
            world::celestial::MetersPerAu;
        object.systemId = objectState.systemId;
        object.velocityMps = objectState.linearVelocityMps;
        object.forwardWorld = glm::dvec3(glm::vec3(
            objectState.orientation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)
        ));
        object.sizeMeters = glm::dvec3(120.0, 80.0, 120.0);
        object.typeName = objectState.type == ObjectType::Station
            ? "Station"
            : "Infrastructure";

        if (!objectState.hubId.empty())
        {
            const auto hubIt = hubById.find(objectState.hubId);
            if (hubIt != hubById.end() && hubIt->second)
                applySystemMapOrbit(object, hubIt->second->motion);
        }
        else
        {
            applySystemMapOrbit(object, objectState.orbitalMotion);
        }

        map.objects.push_back(std::move(object));
    }

    for (const auto& hubState : sample.hubs)
    {
        if (hubState.systemId != map.systemId)
            continue;

        SystemMapObject hub;
        hub.stableId = hubState.id;
        hub.name = hubState.name.empty()
            ? hubState.id
            : hubState.name;
        hub.owner = hubState.owner;
        hub.parentBodyId = hubState.parentBodyId;
        hub.parentHubId = hubState.id;
        hub.kind = SystemMapObjectKind::Hub;
        hub.positionAu =
            world::coordinates::fullMeters(hubState.worldPosition) /
            world::celestial::MetersPerAu;
        hub.systemId = hubState.systemId;
        hub.velocityMps = hubState.worldVelocityMps;
        hub.forwardWorld = glm::dvec3(glm::vec3(
            hubState.orientation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)
        ));
        hub.sizeMeters = glm::dvec3(4000.0, 1500.0, 4000.0);
        hub.typeName = "Hub";
        applySystemMapOrbit(hub, hubState.motion);

        map.objects.push_back(std::move(hub));
    }
}

} // namespace game::client
