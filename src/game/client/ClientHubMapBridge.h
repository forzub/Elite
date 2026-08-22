#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <glm/glm.hpp>

#include "src/game/client/ClientDetailMapRuntimeSampler.h"
#include "src/game/client/ClientLocalAuthority.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/StarAtlasDatabase.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/types/ObjectType.h"

namespace game::client
{

struct ClientHubMapFrame
{
    glm::dvec3 originMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};

    glm::dvec3 progradeAxis {1.0, 0.0, 0.0};
    glm::dvec3 radialAxis {0.0, 1.0, 0.0};
    glm::dvec3 normalAxis {0.0, 0.0, 1.0};

    bool valid = false;

    glm::dvec3 worldToLocalPosition(const glm::dvec3& worldMeters) const
    {
        const glm::dvec3 delta = worldMeters - originMeters;
        return {
            glm::dot(delta, progradeAxis),
            glm::dot(delta, radialAxis),
            glm::dot(delta, normalAxis)
        };
    }

    glm::dvec3 worldToLocalVector(const glm::dvec3& worldVector) const
    {
        return {
            glm::dot(worldVector, progradeAxis),
            glm::dot(worldVector, radialAxis),
            glm::dot(worldVector, normalAxis)
        };
    }

    glm::dvec3 localToWorldVector(const glm::dvec3& localVector) const
    {
        return
            progradeAxis * localVector.x +
            radialAxis * localVector.y +
            normalAxis * localVector.z;
    }

    glm::dvec3 worldToLocalVelocity(
        const glm::dvec3& worldPositionMeters,
        const glm::dvec3& worldVelocity
    ) const
    {
        const glm::dvec3 worldOffset = worldPositionMeters - originMeters;
        const glm::dvec3 rotatingFrameVelocity = glm::cross(
            angularVelocityWorldRadPerSecond,
            worldOffset
        );
        return worldToLocalVector(
            worldVelocity - velocityMps - rotatingFrameVelocity
        );
    }
};

inline glm::dvec3 normalizeHubMapAxis(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double len2 = glm::dot(value, value);
    if (len2 <= 1.0e-18)
        return fallback;
    return value / std::sqrt(len2);
}

inline ClientHubMapFrame makeClientHubMapFrame(
    const DetailMapHubRuntimeSample& hub
)
{
    ClientHubMapFrame frame;
    frame.originMeters = world::coordinates::fullMeters(hub.worldPosition);
    frame.velocityMps = hub.worldVelocityMps;
    frame.angularVelocityWorldRadPerSecond =
        hub.angularVelocityWorldRadPerSecond;

    // Replicated hub.orientation is the visual/model basis:
    // X=normal, Y=radial, Z=-prograde. Hub Map and HubTactical coordinates
    // use X=prograde, Y=radial, Z=normal.
    frame.normalAxis = normalizeHubMapAxis(
        glm::dvec3(hub.orientation[0]),
        glm::dvec3(0.0, 0.0, 1.0)
    );
    frame.radialAxis = normalizeHubMapAxis(
        glm::dvec3(hub.orientation[1]),
        glm::dvec3(0.0, 1.0, 0.0)
    );
    frame.progradeAxis = normalizeHubMapAxis(
        -glm::dvec3(hub.orientation[2]),
        glm::dvec3(1.0, 0.0, 0.0)
    );

    // Quaternion interpolation preserves orthogonality, but normalize the
    // cross-product relationship explicitly so the coordinate contract is
    // stable even if a future source has small numeric drift.
    frame.normalAxis = normalizeHubMapAxis(
        glm::cross(frame.progradeAxis, frame.radialAxis),
        frame.normalAxis
    );
    frame.progradeAxis = normalizeHubMapAxis(
        glm::cross(frame.radialAxis, frame.normalAxis),
        frame.progradeAxis
    );
    frame.valid = true;
    return frame;
}

inline world::celestial::LocalSceneAxes hubMapAxesToLocal(
    const glm::mat4& worldOrientation,
    const ClientHubMapFrame& frame
)
{
    world::celestial::LocalSceneAxes axes;

    const auto convert = [&](const glm::dvec3& worldAxis)
    {
        return glm::dvec3(
            glm::dot(worldAxis, frame.progradeAxis),
            glm::dot(worldAxis, frame.radialAxis),
            glm::dot(worldAxis, frame.normalAxis)
        );
    };

    axes.x = convert(glm::dvec3(worldOrientation[0]));
    axes.y = convert(glm::dvec3(worldOrientation[1]));
    axes.z = convert(glm::dvec3(worldOrientation[2]));
    return axes;
}

inline glm::dvec3 hubMapAssemblySizeMeters(ObjectType typeId)
{
    using game::ship::geometry::AssemblyMeshLibrary;

    if (typeId == ObjectType::None || !AssemblyMeshLibrary::has(typeId))
        return glm::dvec3(1.0);

    const auto& assembly = AssemblyMeshLibrary::get(typeId);
    const glm::vec3 size = assembly.maxBounds - assembly.minBounds;
    return glm::dvec3(
        std::max(1.0f, size.x),
        std::max(1.0f, size.y),
        std::max(1.0f, size.z)
    );
}

inline const world::celestial::CelestialBodyState* findHubMapParentBody(
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

inline std::string hubMapShipDisplayName(
    const DetailMapShipRuntimeSample& ship,
    bool isLocalPlayer
)
{
    if (isLocalPlayer)
        return "Player";

    if (ship.motionLabKind != game::diagnostics::HubMotionLabActorKind::None)
        return game::diagnostics::hubMotionLabLabel(ship.motionLabKind);

    const auto& descriptor = ShipDescriptorRegistry::get(ship.typeId);
    return descriptor.identity.shipName.empty()
        ? "Ship " + std::to_string(ship.id.value)
        : descriptor.identity.shipName;
}

inline bool rebuildHubMapFromClientState(
    world::celestial::HubMapSnapshot& out,
    int systemId,
    const std::string& hubId,
    const world::celestial::StarAtlasDatabase& atlas,
    const world::celestial::CelestialSystemSnapshot& celestial,
    const DetailMapRuntimeSampleResult& runtime,
    double serverTimeSeconds,
    double universeTimeSeconds,
    EntityId localControlledEntityId
)
{
    using namespace world::celestial;

    out = {};

    if (systemId < 0 || hubId.empty() ||
        celestial.systemId != systemId ||
        runtime.status != DetailMapRuntimeSampleStatus::Ready)
    {
        return false;
    }

    const auto hubIt = std::find_if(
        runtime.hubs.begin(),
        runtime.hubs.end(),
        [&](const DetailMapHubRuntimeSample& hub)
        {
            return hub.systemId == systemId && hub.id == hubId;
        }
    );
    if (hubIt == runtime.hubs.end())
        return false;

    const auto& hub = *hubIt;
    const auto frame = makeClientHubMapFrame(hub);
    if (!frame.valid)
        return false;

    const auto* parent = findHubMapParentBody(celestial, hub.parentBodyId);
    if (!parent)
        return false;

    out.systemId = systemId;
    out.hubId = hubId;
    out.parentBodyId = hub.parentBodyId;
    out.parentEnvironmentPresetId = parent->environmentPresetId;
    out.displayName = hub.name.empty() ? hub.id : hub.name;
    out.universeTimeSeconds = universeTimeSeconds;

    if (const auto* summary = atlas.findSystemSummary(systemId))
        out.systemPositionLy = summary->positionLy;

    out.hubWorldPositionMeters = frame.originMeters;
    out.hubWorldVelocityMps = frame.velocityMps;

    // Hub Map's own coordinates are already tactical/navigation-local.
    out.hubAxes.x = glm::dvec3(1.0, 0.0, 0.0);
    out.hubAxes.y = glm::dvec3(0.0, 1.0, 0.0);
    out.hubAxes.z = glm::dvec3(0.0, 0.0, 1.0);

    out.hubWorldAxes.x = frame.progradeAxis;
    out.hubWorldAxes.y = frame.radialAxis;
    out.hubWorldAxes.z = frame.normalAxis;

    out.parentPlanetWorldPositionMeters = parent->worldMeters;
    out.parentPlanetWorldVelocityMps = parent->worldVelocityMetersPerSecond;
    out.parentPlanetCenterLocalMeters =
        frame.worldToLocalPosition(parent->worldMeters);
    out.parentPlanetRadiusMeters = std::max(0.0, parent->radiusKm * 1000.0);
    out.parentPlanetRotationPhaseRad = parent->rotationPhaseRad;
    out.parentPlanetAxialTiltDeg = parent->axialTiltDeg;
    out.parentPlanetAxisNodeDeg = parent->axisNodeDeg;
    out.parentPlanetTextureLongitudeOffsetDeg =
        parent->textureLongitudeOffsetDeg;

    out.hubAltitudeMeters = hub.motion.altitudeMeters;
    out.hubOrbitRadiusMeters = std::max(
        1.0,
        out.parentPlanetRadiusMeters + hub.motion.altitudeMeters
    );

    out.scene.anchorClass = DetailObjectClass::Hub;
    out.scene.anchorId = hubId;
    out.scene.focusId = hubId;
    out.scene.coordinateSpace = LocalSceneCoordinateSpace::AnchorLocalMeters;
    out.scene.originWorldMeters = frame.originMeters;

    // Hub components are ordinary replicated static objects joined to the
    // requested hub through HubAttachmentSnapshot.
    for (const auto& source : runtime.objects)
    {
        if (source.systemId != systemId ||
            !source.hubAttachment.valid ||
            source.hubAttachment.hubId != hubId)
        {
            continue;
        }

        HubMapModule module;
        module.id = source.id;
        module.typeId = source.type;
        module.stableId = std::to_string(source.id.value);
        module.name = source.displayName.empty()
            ? "Hub module"
            : source.displayName;
        module.kind = source.type == ObjectType::Station
            ? "station"
            : "module";
        module.objectClass = DetailObjectClass::Hub;
        module.origin = DetailObjectOrigin::Runtime;
        module.role = LocalSceneObjectRole::Component;
        module.coordinateSpace = LocalSceneCoordinateSpace::AnchorLocalMeters;
        module.parentStableId = hubId;

        if (source.hubAttachment.inheritHubOrientation)
        {
            // Attachment offsets are stable hub-model facts. Reconstruct the
            // module from the same sampled hub frame instead of interpolating
            // an independently sampled world pose; this is the map equivalent
            // of the gameplay co-frame jitter fix.
            const glm::dvec3 worldMeters =
                game::navigation::hubVisualLocalToWorldPosition(
                    frame.originMeters,
                    frame.progradeAxis,
                    frame.radialAxis,
                    frame.normalAxis,
                    source.hubAttachment.localOffsetMeters
                );
            const glm::mat4 worldOrientation =
                game::navigation::hubAttachedVisualOrientation(
                    frame.progradeAxis,
                    frame.radialAxis,
                    frame.normalAxis,
                    source.hubAttachment.localRotationDeg
                );
            module.positionMeters = frame.worldToLocalPosition(worldMeters);
            module.axes = hubMapAxesToLocal(worldOrientation, frame);
        }
        else
        {
            const glm::dvec3 worldMeters =
                world::coordinates::fullMeters(source.worldPosition);
            module.positionMeters = frame.worldToLocalPosition(worldMeters);
            module.axes = hubMapAxesToLocal(source.orientation, frame);
        }
        module.sizeMeters = hubMapAssemblySizeMeters(source.type);
        module.prime =
            !hub.primeModuleId.empty() &&
            source.hubAttachment.moduleId == hub.primeModuleId;
        module.valid = true;
        out.scene.objects.push_back(std::move(module));
    }

    // Ships are also ordinary replication. Preserve the established Hub Map
    // membership rule: non-player ships belong only to their actual hub; the
    // player marker may remain visible while a hub transition is in flight.
    for (const auto& source : runtime.ships)
    {
        if (source.systemId != systemId)
            continue;

        const bool isLocalPlayer = game::client::isLocalControlledEntity(
            source.id,
            localControlledEntityId
        );
        const bool usesThisHubFrame = source.hubId == hubId;
        if (!usesThisHubFrame && !isLocalPlayer)
            continue;

        const glm::dvec3 worldMeters =
            world::coordinates::fullMeters(source.worldPosition);

        HubMapShip ship;
        ship.id = source.id;
        ship.shipInstanceId = source.instanceId;
        ship.typeId = source.typeId;
        ship.stableId = isLocalPlayer
            ? "player"
            : "entity:" + std::to_string(source.id.value);
        ship.name = hubMapShipDisplayName(source, isLocalPlayer);
        const auto& shipDescriptor = ShipDescriptorRegistry::get(source.typeId);
        ship.typeName = shipDescriptor.identity.shipType.empty()
            ? "Ship"
            : shipDescriptor.identity.shipType;
        ship.kind = "ship";
        ship.objectClass = DetailObjectClass::Ship;
        ship.origin = DetailObjectOrigin::Runtime;
        ship.role = LocalSceneObjectRole::Participant;
        ship.coordinateSpace = LocalSceneCoordinateSpace::AnchorLocalMeters;
        ship.parentStableId = hubId;

        if (usesThisHubFrame &&
            source.motionMode == game::navigation::MotionMode::HubTactical)
        {
            // Local HubTactical state is authoritative in this exact frame.
            ship.positionMeters = source.localPositionMeters;
            ship.velocityMps = source.localVelocityMps;
            ship.relativeVelocityMps = source.localVelocityMps;
            ship.relativeVelocityWorldMps =
                frame.localToWorldVector(source.localVelocityMps);
            ship.hasRelativeVelocity = true;
            ship.globalVelocityMps = source.worldVelocityMps;
            ship.hasGlobalVelocity = true;
        }
        else
        {
            ship.positionMeters = frame.worldToLocalPosition(worldMeters);
            ship.velocityMps =
                usesThisHubFrame &&
                source.motionMode == game::navigation::MotionMode::Docked
                    ? glm::dvec3(0.0)
                    : frame.worldToLocalVelocity(
                        worldMeters,
                        source.worldVelocityMps
                    );
            ship.relativeVelocityMps = ship.velocityMps;
            ship.relativeVelocityWorldMps =
                frame.localToWorldVector(ship.velocityMps);
            ship.hasRelativeVelocity = true;
            ship.globalVelocityMps = source.worldVelocityMps;
            ship.hasGlobalVelocity = true;
        }

        ship.axes = hubMapAxesToLocal(source.orientation, frame);
        ship.sizeMeters = hubMapAssemblySizeMeters(source.typeId);
        ship.player = isLocalPlayer;
        ship.valid = true;
        out.scene.objects.push_back(std::move(ship));
    }

    if (game::diagnostics::HubMotionLabEnabled &&
        systemId == game::diagnostics::HubMotionLabSystemId &&
        hubId == game::diagnostics::HubMotionLabHubId)
    {
        const auto pose =
            game::diagnostics::evaluateHubMotionLabCube(serverTimeSeconds);

        HubMapShip cube;
        cube.stableId = "diagnostic:hub_motion_lab_cube";
        cube.name = "LAB ANALYTIC CUBE";
        cube.kind = "diagnostic_probe";
        cube.objectClass = DetailObjectClass::Ship;
        cube.origin = DetailObjectOrigin::Runtime;
        cube.role = LocalSceneObjectRole::Participant;
        cube.coordinateSpace = LocalSceneCoordinateSpace::AnchorLocalMeters;
        cube.parentStableId = hubId;
        cube.positionMeters = pose.localPositionMeters;
        cube.sizeMeters = glm::dvec3(pose.halfExtentMeters * 2.0);
        cube.valid = true;
        out.scene.objects.push_back(std::move(cube));
    }

    out.scene.halfExtentMeters = 1000.0;
    for (const auto& object : out.scene.objects)
    {
        if (!object.valid)
            continue;

        const double objectExtent = std::max(
            object.boundingRadiusMeters,
            glm::length(object.sizeMeters) * 0.5
        );
        out.scene.halfExtentMeters = std::max(
            out.scene.halfExtentMeters,
            glm::length(object.positionMeters) + objectExtent
        );
    }

    out.valid = true;
    return true;
}

} // namespace game::client
