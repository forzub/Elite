#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <glm/glm.hpp>

#include "src/game/client/ClientDetailMapRuntimeSampler.h"
#include "src/game/client/ClientLocalAuthority.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/SystemMapConversion.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/types/ObjectType.h"

namespace game::client
{

inline glm::dvec3 safeNormalizeDetailMap(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double len2 = glm::dot(value, value);
    return len2 < 1.0e-12 ? fallback : value / std::sqrt(len2);
}

inline world::celestial::LocalSceneAxes detailAxesFromOrientation(
    const glm::mat4& orientation
)
{
    world::celestial::LocalSceneAxes axes;
    axes.x = glm::dvec3(orientation[0]);
    axes.y = glm::dvec3(orientation[1]);
    axes.z = glm::dvec3(orientation[2]);
    return axes;
}

inline const world::celestial::CelestialBodyState* findDetailBodyState(
    const world::celestial::CelestialSystemSnapshot& celestial,
    const std::string& bodyId
) noexcept
{
    const auto it = std::find_if(
        celestial.bodies.begin(),
        celestial.bodies.end(),
        [&](const world::celestial::CelestialBodyState& body)
        {
            return body.id == bodyId;
        }
    );
    return it == celestial.bodies.end() ? nullptr : &*it;
}

inline const world::celestial::CelestialBodyDefinition* findDetailBodyDefinition(
    const world::celestial::CelestialSystemDefinition& system,
    const std::string& bodyId
) noexcept
{
    const auto it = std::find_if(
        system.bodies.begin(),
        system.bodies.end(),
        [&](const world::celestial::CelestialBodyDefinition& body)
        {
            return body.id == bodyId;
        }
    );
    return it == system.bodies.end() ? nullptr : &*it;
}

inline const DetailMapHubRuntimeSample* findDetailRuntimeHub(
    const DetailMapRuntimeSampleResult& runtime,
    const std::string& hubId
) noexcept
{
    const auto it = std::find_if(
        runtime.hubs.begin(),
        runtime.hubs.end(),
        [&](const DetailMapHubRuntimeSample& hub)
        {
            return hub.id == hubId;
        }
    );
    return it == runtime.hubs.end() ? nullptr : &*it;
}

inline glm::dvec3 detailHubLocalToWorldVector(
    const DetailMapHubRuntimeSample& hub,
    const glm::dvec3& localMeters
)
{
    // HubMotionLab local coordinates are navigation/tactical coordinates:
    // X=prograde, Y=radial, Z=normal. Replicated hub.orientation is the
    // visual/model basis X=normal, Y=radial, Z=-prograde, so recover the
    // navigation basis before applying the local offset.
    const auto axes = detailAxesFromOrientation(hub.orientation);
    const glm::dvec3 prograde = -axes.z;
    const glm::dvec3 radial = axes.y;
    const glm::dvec3 normal = axes.x;
    return
        prograde * localMeters.x +
        radial * localMeters.y +
        normal * localMeters.z;
}

template <typename BoundsPredicate>
inline void appendDetailDiagnosticCube(
    world::celestial::DetailMapSnapshot& out,
    const DetailMapRuntimeSampleResult& runtime,
    double serverTimeSeconds,
    const BoundsPredicate& intersectsBounds,
    bool setSize
)
{
    if (!game::diagnostics::HubMotionLabEnabled ||
        out.systemId != game::diagnostics::HubMotionLabSystemId)
    {
        return;
    }

    const auto* hub = findDetailRuntimeHub(
        runtime,
        std::string(game::diagnostics::HubMotionLabHubId)
    );
    if (!hub)
        return;

    const auto pose =
        game::diagnostics::evaluateHubMotionLabCube(serverTimeSeconds);

    const glm::dvec3 positionMeters =
        world::coordinates::fullMeters(hub->worldPosition) +
        detailHubLocalToWorldVector(*hub, pose.localPositionMeters);

    if (!intersectsBounds(positionMeters, 0.0))
        return;

    world::celestial::LocalSceneObject cube;
    cube.stableId = "diagnostic:hub_motion_lab_cube";
    cube.name = "LAB ANALYTIC CUBE";
    cube.kind = "diagnostic_probe";
    cube.parentStableId =
        std::string(game::diagnostics::HubMotionLabHubId);
    cube.objectClass = world::celestial::DetailObjectClass::Ship;
    cube.origin = world::celestial::DetailObjectOrigin::Runtime;
    cube.role = world::celestial::LocalSceneObjectRole::Participant;
    cube.positionMeters = positionMeters;
    if (setSize)
        cube.sizeMeters = glm::dvec3(pose.halfExtentMeters * 2.0);
    cube.valid = true;
    out.scene.objects.push_back(std::move(cube));
}

inline std::string detailShipDisplayName(
    const DetailMapShipRuntimeSample& ship,
    bool identifyLocalPlayer,
    EntityId localControlledEntityId
)
{
    if (identifyLocalPlayer &&
        game::client::isLocalControlledEntity(
            ship.id,
            localControlledEntityId))
    {
        return "Player";
    }

    if (ship.motionLabKind !=
        game::diagnostics::HubMotionLabActorKind::None)
    {
        return game::diagnostics::hubMotionLabLabel(ship.motionLabKind);
    }

    const auto& descriptor = ShipDescriptorRegistry::get(ship.typeId);
    return descriptor.identity.shipName.empty()
        ? "Ship " + std::to_string(ship.id.value)
        : descriptor.identity.shipName;
}

inline world::celestial::LocalSceneObject makeDetailShipObject(
    const DetailMapShipRuntimeSample& ship,
    bool identifyLocalPlayer,
    EntityId localControlledEntityId
)
{
    world::celestial::LocalSceneObject object;
    object.id = ship.id;
    object.typeId = ship.typeId;
    const bool playerPresentation =
        identifyLocalPlayer &&
        game::client::isLocalControlledEntity(
            ship.id,
            localControlledEntityId
        );
    object.stableId = playerPresentation
        ? "player"
        : "entity:" + std::to_string(ship.id.value);
    object.name = detailShipDisplayName(
        ship,
        identifyLocalPlayer,
        localControlledEntityId
    );
    const auto& descriptor = ShipDescriptorRegistry::get(ship.typeId);
    object.typeName = descriptor.identity.shipType.empty()
        ? "Ship"
        : descriptor.identity.shipType;
    object.sizeMeters = glm::dvec3(descriptor.getMeshSizeMeters());
    object.kind = playerPresentation ? "player" : "ship";
    object.parentStableId = ship.hubId;
    object.objectClass = world::celestial::DetailObjectClass::Ship;
    object.origin = world::celestial::DetailObjectOrigin::Runtime;
    object.role = world::celestial::LocalSceneObjectRole::Participant;
    object.positionMeters =
        world::coordinates::fullMeters(ship.worldPosition);
    object.velocityMps = ship.worldVelocityMps;
    object.globalVelocityMps = ship.worldVelocityMps;
    object.hasGlobalVelocity = true;
    object.relativeVelocityMps = ship.localVelocityMps;
    if (ship.travelFrame.valid)
    {
        object.relativeVelocityWorldMps =
            ship.travelFrame.localToWorldVector(ship.localVelocityMps);
        object.hasRelativeVelocity = true;
    }
    object.axes = detailAxesFromOrientation(ship.orientation);
    object.valid = true;
    return object;
}

inline world::celestial::LocalSceneObject makeDetailHubObject(
    const DetailMapHubRuntimeSample& hub,
    const std::string& parentBodyId
)
{
    world::celestial::LocalSceneObject object;
    object.stableId = hub.id;
    object.name = hub.name.empty() ? hub.id : hub.name;
    object.kind = "hub";
    object.parentStableId = parentBodyId;
    object.objectClass = world::celestial::DetailObjectClass::Hub;
    object.origin = world::celestial::DetailObjectOrigin::Runtime;
    // Hub runtime replication currently carries transform/orbit facts but not
    // an authored aggregate envelope. Keep the Detail presentation envelope
    // aligned with the existing System-map Hub envelope until aggregate Hub
    // bounds become a first-class replicated/static-definition fact. Besides
    // glyph scaling, this value is the semantic size used when several map
    // objects collapse into the same click cluster.
    object.sizeMeters = glm::dvec3(4000.0, 1500.0, 4000.0);
    object.positionMeters =
        world::coordinates::fullMeters(hub.worldPosition);
    object.velocityMps = hub.worldVelocityMps;
    object.globalVelocityMps = hub.worldVelocityMps;
    object.hasGlobalVelocity = true;
    object.axes = detailAxesFromOrientation(hub.orientation);
    object.valid = true;
    return object;
}

inline world::celestial::LocalSceneObject makeDetailStaticObject(
    const DetailMapObjectRuntimeSample& source,
    bool localSceneStableId
)
{
    world::celestial::LocalSceneObject object;
    object.id = source.id;
    object.typeId = source.type;
    object.stableId = localSceneStableId
        ? "entity:" + std::to_string(source.id.value)
        : std::to_string(source.id.value);
    object.name = localSceneStableId
        ? source.displayName
        : (source.displayName.empty() ? "Station" : source.displayName);
    object.kind = "station";
    if (source.hubAttachment.valid && !source.hubAttachment.hubId.empty())
    {
        object.parentStableId = source.hubAttachment.hubId;
    }
    else if (!localSceneStableId)
    {
        object.parentStableId = source.navigationParentBodyId;
    }
    object.objectClass = world::celestial::DetailObjectClass::Hub;
    object.origin = world::celestial::DetailObjectOrigin::Runtime;
    object.positionMeters =
        world::coordinates::fullMeters(source.worldPosition);
    object.velocityMps = source.linearVelocityMps;
    object.globalVelocityMps = source.linearVelocityMps;
    object.hasGlobalVelocity = true;
    // Planet Details historically showed authoritative station orientation.
    // Free-space/local-volume context kept the neutral axis glyph; preserve
    // that presentation while moving ownership to the client.
    if (!localSceneStableId)
        object.axes = detailAxesFromOrientation(source.orientation);
    object.valid = true;
    return object;
}

inline void resolveDetailOrbitalAxes(
    const glm::dvec3& positionMeters,
    const glm::dvec3& worldVelocityMps,
    const glm::dvec3& planetCenterMeters,
    const glm::dvec3& planetVelocityMps,
    const glm::mat4& hubOrientation,
    glm::dvec3& radial,
    glm::dvec3& prograde,
    glm::dvec3& normal
)
{
    const auto hubAxes = detailAxesFromOrientation(hubOrientation);
    const glm::dvec3 fallbackNormal = hubAxes.x;
    const glm::dvec3 fallbackRadial = hubAxes.y;
    const glm::dvec3 fallbackPrograde = -hubAxes.z;

    radial = safeNormalizeDetailMap(
        positionMeters - planetCenterMeters,
        safeNormalizeDetailMap(fallbackRadial, glm::dvec3(0.0, 1.0, 0.0))
    );

    const glm::dvec3 relativeVelocity =
        worldVelocityMps - planetVelocityMps;
    const glm::dvec3 tangentialVelocity =
        relativeVelocity - radial * glm::dot(relativeVelocity, radial);
    const glm::dvec3 fallbackTangential =
        fallbackPrograde - radial * glm::dot(fallbackPrograde, radial);

    prograde = safeNormalizeDetailMap(
        tangentialVelocity,
        safeNormalizeDetailMap(
            fallbackTangential,
            glm::dvec3(1.0, 0.0, 0.0)
        )
    );

    normal = safeNormalizeDetailMap(
        glm::cross(prograde, radial),
        safeNormalizeDetailMap(fallbackNormal, glm::dvec3(0.0, 0.0, 1.0))
    );

    prograde = safeNormalizeDetailMap(
        glm::cross(radial, normal),
        prograde
    );
}

inline void appendDetailLocalContext(
    world::celestial::DetailMapSnapshot& out,
    const world::celestial::CelestialSystemSnapshot& celestial,
    const DetailMapRuntimeSampleResult& runtime,
    double serverTimeSeconds,
    double extentMeters,
    bool cubicBounds,
    EntityId localControlledEntityId
)
{
    using namespace world::celestial;

    const auto intersectsBounds =
        [&](const glm::dvec3& positionMeters, double objectRadiusMeters)
        {
            const glm::dvec3 delta = positionMeters - out.planetCenterMeters;
            if (cubicBounds)
            {
                return
                    std::abs(delta.x) <= extentMeters + objectRadiusMeters &&
                    std::abs(delta.y) <= extentMeters + objectRadiusMeters &&
                    std::abs(delta.z) <= extentMeters + objectRadiusMeters;
            }
            return glm::length(delta) <= extentMeters + objectRadiusMeters;
        };

    for (const auto& hub : runtime.hubs)
    {
        if (hub.systemId != out.systemId)
            continue;

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(hub.worldPosition);
        if (!intersectsBounds(positionMeters, 0.0))
            continue;

        out.scene.objects.push_back(
            makeDetailHubObject(hub, hub.parentBodyId)
        );
    }

    for (const auto& body : celestial.bodies)
    {
        const glm::dvec3 positionMeters = body.worldMeters;
        const double radiusMeters = std::max(0.0, body.radiusKm * 1000.0);
        if (!intersectsBounds(positionMeters, radiusMeters))
            continue;

        LocalSceneObject contextBody;
        contextBody.stableId = body.id;
        contextBody.name = body.name;
        contextBody.kind = "celestial";
        contextBody.objectClass = DetailObjectClass::CelestialBody;
        contextBody.origin = DetailObjectOrigin::Authored;
        contextBody.positionMeters = positionMeters;
        contextBody.velocityMps = body.worldVelocityMetersPerSecond;
        contextBody.boundingRadiusMeters = radiusMeters;
        contextBody.valid = true;
        out.scene.objects.push_back(std::move(contextBody));
    }

    for (const auto& object : runtime.objects)
    {
        if (object.systemId != out.systemId)
            continue;

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(object.worldPosition);
        if (!intersectsBounds(positionMeters, 0.0))
            continue;

        out.scene.objects.push_back(
            makeDetailStaticObject(object, true)
        );
    }

    for (const auto& ship : runtime.ships)
    {
        if (ship.systemId != out.systemId)
            continue;

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(ship.worldPosition);
        if (!intersectsBounds(positionMeters, 0.0))
            continue;

        out.scene.objects.push_back(
            makeDetailShipObject(ship, false, localControlledEntityId)
        );
    }

    appendDetailDiagnosticCube(
        out,
        runtime,
        serverTimeSeconds,
        intersectsBounds,
        false
    );
}

inline bool buildClientCelestialBodyDetail(
    world::celestial::DetailMapSnapshot& out,
    int systemId,
    const std::string& bodyId,
    const world::celestial::StarAtlasDatabase& atlas,
    const world::celestial::CelestialSystemSnapshot& celestial,
    const DetailMapRuntimeSampleResult& runtime,
    double serverTimeSeconds,
    double universeTimeSeconds,
    EntityId localControlledEntityId
)
{
    using namespace world::celestial;

    const auto* system = atlas.findSystem(systemId);
    const auto* body = findDetailBodyState(celestial, bodyId);
    if (!system || !body)
        return false;

    out = {};
    out.systemId = systemId;
    out.planetBodyId = bodyId;
    out.planetName = body->name;
    out.environmentPresetId = body->environmentPresetId;
    out.planetCenterMeters = body->worldMeters;
    out.planetVelocityMps = body->worldVelocityMetersPerSecond;
    out.planetRadiusMeters = body->radiusKm * 1000.0;
    out.universeTimeSeconds = universeTimeSeconds;
    out.planetRotationPhaseRad = body->rotationPhaseRad;
    out.planetDayLengthHours = body->dayLengthHours;
    out.planetRotationDirection = body->rotationDirection;
    out.planetAxialTiltDeg = body->axialTiltDeg;
    out.planetAxisNodeDeg = body->axisNodeDeg;
    out.planetTextureLongitudeOffsetDeg = body->textureLongitudeOffsetDeg;

    if (const auto* summary = atlas.findSystemSummary(systemId))
        out.systemPositionLy = summary->positionLy;

    if (const auto* definition = findDetailBodyDefinition(*system, bodyId))
    {
        out.gravitationalParameterM3s2 =
            definition->gravitationalParameterM3s2;
        out.ringPlaneInclinationOffsetDeg =
            definition->ringPlaneInclinationOffsetDeg;
        out.ringVisual = toSystemMapRingVisualProfile(definition->ringVisual);
        out.rings.reserve(definition->rings.size());
        for (const auto& ring : definition->rings)
            out.rings.push_back(toSystemMapRing(ring));
    }

    out.valid = true;
    out.hasCentralBody = true;
    out.scene.anchorClass = DetailObjectClass::CelestialBody;
    out.scene.anchorId = bodyId;
    out.scene.focusId = bodyId;
    out.scene.coordinateSpace = LocalSceneCoordinateSpace::SystemWorldMeters;
    out.scene.originWorldMeters = out.planetCenterMeters;

    for (const auto& hub : runtime.hubs)
    {
        if (hub.systemId != systemId || hub.parentBodyId != bodyId)
            continue;

        auto hubObject = makeDetailHubObject(hub, bodyId);
        out.scene.objects.push_back(hubObject);

        glm::dvec3 radial;
        glm::dvec3 prograde;
        glm::dvec3 normal;
        resolveDetailOrbitalAxes(
            hubObject.positionMeters,
            hubObject.velocityMps,
            out.planetCenterMeters,
            out.planetVelocityMps,
            hub.orientation,
            radial,
            prograde,
            normal
        );

        DetailMapOrbit orbit;
        orbit.id = hub.id + "_rail_orbit";
        orbit.name = hubObject.name + " rail orbit";
        orbit.parentBodyId = bodyId;
        orbit.centerMeters = out.planetCenterMeters;
        orbit.positionMeters = hubObject.positionMeters;
        orbit.velocityMps = hubObject.velocityMps;
        orbit.radiusMeters = glm::length(
            orbit.positionMeters - out.planetCenterMeters
        );
        orbit.altitudeMeters = orbit.radiusMeters - out.planetRadiusMeters;
        orbit.speedMps = glm::length(
            orbit.velocityMps - out.planetVelocityMps
        );
        orbit.radialAxis = radial;
        orbit.progradeAxis = prograde;
        orbit.normalAxis = normal;
        orbit.valid = true;
        out.hubOrbits.push_back(std::move(orbit));
    }

    for (const auto& object : runtime.objects)
    {
        if (object.systemId != systemId ||
            object.navigationParentBodyId != bodyId ||
            object.type != ObjectType::Station)
        {
            continue;
        }

        out.scene.objects.push_back(
            makeDetailStaticObject(object, false)
        );
    }

    constexpr double PlanetMapObjectRadiusMeters = 100000000.0;
    for (const auto& ship : runtime.ships)
    {
        if (ship.systemId != systemId)
            continue;

        const glm::dvec3 shipPosition =
            world::coordinates::fullMeters(ship.worldPosition);
        if (glm::length(shipPosition - out.planetCenterMeters) >
            PlanetMapObjectRadiusMeters)
        {
            continue;
        }

        out.scene.objects.push_back(
            makeDetailShipObject(ship, true, localControlledEntityId)
        );
    }

    const auto intersectsPlanetBounds =
        [&](const glm::dvec3& positionMeters, double objectRadiusMeters)
        {
            return glm::length(positionMeters - out.planetCenterMeters) <=
                PlanetMapObjectRadiusMeters + objectRadiusMeters;
        };

    appendDetailDiagnosticCube(
        out,
        runtime,
        serverTimeSeconds,
        intersectsPlanetBounds,
        true
    );

    // Free ships do not receive a fabricated circular orbit. A real future
    // trajectory renderer will own planned/predicted/historical trajectories.
    out.playerOrbits.clear();
    return true;
}

inline bool rebuildUnboundSpatialDetailMap(
    world::celestial::DetailMapSnapshot& out,
    const world::celestial::DetailTarget& target,
    double universeTimeSeconds
)
{
    using namespace world::celestial;

    if (target.sceneKind != DetailSceneKind::SpatialVolume ||
        target.systemId >= 0 ||
        !target.valid())
    {
        out = {};
        return false;
    }

    out = {};
    out.valid = true;
    out.hasCentralBody = false;
    out.systemId = target.systemId;
    out.systemPositionLy = target.systemPositionLy;
    out.detailTarget = target;
    out.detailHalfExtentMeters =
        target.spatialCell.edgeAu * MetersPerAu * 0.5;
    out.planetName = "Local Space";
    out.universeTimeSeconds = universeTimeSeconds;
    out.planetCenterMeters =
        target.spatialCell.centerAu * MetersPerAu;

    out.scene.anchorClass = DetailObjectClass::None;
    out.scene.coordinateSpace = LocalSceneCoordinateSpace::SystemWorldMeters;
    out.scene.originWorldMeters = out.planetCenterMeters;
    out.scene.halfExtentMeters = out.detailHalfExtentMeters;
    return out.detailHalfExtentMeters > 0.0;
}

inline bool rebuildDetailMapFromClientState(
    world::celestial::DetailMapSnapshot& out,
    const world::celestial::DetailTarget& requestedTarget,
    const world::celestial::StarAtlasDatabase& atlas,
    const world::celestial::CelestialSystemSnapshot& celestial,
    const DetailMapRuntimeSampleResult& runtime,
    double serverTimeSeconds,
    double universeTimeSeconds,
    EntityId localControlledEntityId
)
{
    using namespace world::celestial;

    if (!requestedTarget.valid() ||
        celestial.systemId != requestedTarget.systemId ||
        runtime.status != DetailMapRuntimeSampleStatus::Ready)
    {
        out = {};
        return false;
    }

    DetailTarget effectiveTarget = requestedTarget;

    switch (requestedTarget.sceneKind)
    {
        case DetailSceneKind::CelestialBody:
        {
            if (!buildClientCelestialBodyDetail(
                    out,
                    requestedTarget.systemId,
                    requestedTarget.anchorId,
                    atlas,
                    celestial,
                    runtime,
                    serverTimeSeconds,
                    universeTimeSeconds,
                    localControlledEntityId))
            {
                return false;
            }
            break;
        }

        case DetailSceneKind::LocalObject:
        {
            if (requestedTarget.focusClass != DetailObjectClass::Hub)
                return false;

            const auto* anchor = findDetailRuntimeHub(
                runtime,
                requestedTarget.anchorId
            );
            if (!anchor || anchor->systemId != requestedTarget.systemId)
                return false;

            out = {};
            out.systemId = requestedTarget.systemId;
            out.hasCentralBody = false;
            out.detailAnchorHubId = anchor->id;
            out.planetName = "Deep Space";
            out.universeTimeSeconds = universeTimeSeconds;
            out.planetCenterMeters =
                world::coordinates::fullMeters(anchor->worldPosition);
            out.planetVelocityMps = anchor->worldVelocityMps;
            if (const auto* summary = atlas.findSystemSummary(out.systemId))
                out.systemPositionLy = summary->positionLy;

            constexpr double LocalDetailRadiusMeters = 5.0e9;
            out.valid = true;
            out.scene.anchorClass = DetailObjectClass::Hub;
            out.scene.anchorId = anchor->id;
            out.scene.focusId = requestedTarget.focusId.empty()
                ? anchor->id
                : requestedTarget.focusId;
            out.scene.coordinateSpace = LocalSceneCoordinateSpace::SystemWorldMeters;
            out.scene.originWorldMeters = out.planetCenterMeters;
            // Preserve the established LocalObject Details presentation:
            // the radius bounds content selection, while scene.halfExtentMeters
            // remains reserved for explicit SpatialVolume navigation cubes.

            appendDetailLocalContext(
                out,
                celestial,
                runtime,
                serverTimeSeconds,
                LocalDetailRadiusMeters,
                false,
                localControlledEntityId
            );
            break;
        }

        case DetailSceneKind::SpatialVolume:
        {
            out = {};
            out.systemId = requestedTarget.systemId;
            out.systemPositionLy = requestedTarget.systemPositionLy;
            if (const auto* summary = atlas.findSystemSummary(out.systemId))
                out.systemPositionLy = summary->positionLy;
            out.hasCentralBody = false;
            out.planetName = "Local Space";
            out.universeTimeSeconds = universeTimeSeconds;
            out.planetCenterMeters =
                requestedTarget.spatialCell.centerAu * MetersPerAu;

            // A terminal cube is an address. If its centre is inside a body,
            // the semantic Details scene becomes that body, matching the
            // established Galaxy/System/Details navigation behavior.
            for (const auto& body : celestial.bodies)
            {
                const double radiusMeters =
                    std::max(0.0, body.radiusKm * 1000.0);
                if (radiusMeters <= 0.0)
                    continue;

                if (glm::length(out.planetCenterMeters - body.worldMeters) >=
                    radiusMeters)
                {
                    continue;
                }

                effectiveTarget.sceneKind = DetailSceneKind::CelestialBody;
                effectiveTarget.focusClass = DetailObjectClass::CelestialBody;
                effectiveTarget.anchorId = body.id;
                effectiveTarget.focusId = body.id;

                if (!buildClientCelestialBodyDetail(
                        out,
                        requestedTarget.systemId,
                        body.id,
                        atlas,
                        celestial,
                        runtime,
                        serverTimeSeconds,
                        universeTimeSeconds,
                        localControlledEntityId))
                {
                    return false;
                }
                break;
            }

            if (!out.valid)
            {
                out.detailHalfExtentMeters =
                    requestedTarget.spatialCell.edgeAu * MetersPerAu * 0.5;
                out.valid = out.detailHalfExtentMeters > 0.0;
                if (!out.valid)
                    return false;

                appendDetailLocalContext(
                    out,
                    celestial,
                    runtime,
                    serverTimeSeconds,
                    out.detailHalfExtentMeters,
                    true,
                    localControlledEntityId
                );
            }
            break;
        }

        default:
            return false;
    }

    if (!out.valid)
        return false;

    out.detailTarget = effectiveTarget;
    out.scene.anchorClass =
        effectiveTarget.sceneKind == DetailSceneKind::CelestialBody
            ? DetailObjectClass::CelestialBody
            : effectiveTarget.focusClass;
    out.scene.anchorId = effectiveTarget.anchorId;
    out.scene.focusId = effectiveTarget.focusId;
    out.scene.coordinateSpace = LocalSceneCoordinateSpace::SystemWorldMeters;
    out.scene.originWorldMeters = out.planetCenterMeters;
    if (effectiveTarget.sceneKind == DetailSceneKind::SpatialVolume)
        out.scene.halfExtentMeters = out.detailHalfExtentMeters;

    return true;
}

} // namespace game::client
