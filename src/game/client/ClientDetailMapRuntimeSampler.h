#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/client/ClientSpatialDomain.h"
#include "src/game/client/SnapshotPresentationWindow.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/world/types/ObjectType.h"

namespace game::client
{

enum class DetailMapRuntimeSampleStatus
{
    Ready,
    AwaitingNewerSnapshot,
    TooOld
};

struct DetailMapShipRuntimeSample
{
    EntityId id {};
    ShipRole role = ShipRole::NPC;
    ObjectType typeId = ObjectType::None;
    game::diagnostics::HubMotionLabActorKind motionLabKind =
        game::diagnostics::HubMotionLabActorKind::None;

    int systemId = -1;
    std::string parentBodyId;
    std::string hubId;
    game::navigation::MotionMode motionMode =
        game::navigation::MotionMode::Inertial;

    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    glm::dvec3 localPositionMeters {0.0};
    glm::dvec3 localVelocityMps {0.0};
    glm::mat4 orientation {1.0f};
};

struct DetailMapObjectRuntimeSample
{
    EntityId id {};
    ObjectType type = ObjectType::None;
    int systemId = -1;

    std::string displayName;
    std::string ownerName;
    std::string navigationParentBodyId;

    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 linearVelocityMps {0.0};
    glm::mat4 orientation {1.0f};

    game::simulation::HubAttachmentSnapshot hubAttachment;
    world::orbits::OrbitalMotion orbitalMotion;
};

struct DetailMapHubRuntimeSample
{
    std::string id;
    std::string name;
    std::string owner;

    int systemId = -1;
    std::string parentBodyId;

    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    glm::dvec3 angularVelocityWorldRadPerSecond {0.0};
    glm::mat4 orientation {1.0f};
    std::string primeModuleId;
    world::orbits::OrbitalMotion motion;
};

struct DetailMapRuntimeSampleResult
{
    DetailMapRuntimeSampleStatus status =
        DetailMapRuntimeSampleStatus::AwaitingNewerSnapshot;

    std::vector<DetailMapShipRuntimeSample> ships;
    std::vector<DetailMapObjectRuntimeSample> objects;
    std::vector<DetailMapHubRuntimeSample> hubs;
};

inline glm::mat4 interpolateDetailMapOrientation(
    const glm::mat4& older,
    const glm::mat4& newer,
    double alpha
)
{
    if (alpha <= 0.0)
        return older;
    if (alpha >= 1.0)
        return newer;

    glm::quat a = glm::normalize(glm::quat_cast(glm::mat3(older)));
    glm::quat b = glm::normalize(glm::quat_cast(glm::mat3(newer)));

    // Keep interpolation on the shortest quaternion arc. q and -q represent
    // the same orientation but naïve interpolation can otherwise take the
    // long way around exactly when a map request samples between snapshots.
    if (glm::dot(a, b) < 0.0f)
        b = -b;

    return glm::mat4_cast(
        glm::normalize(
            glm::slerp(a, b, static_cast<float>(alpha))
        )
    );
}

inline const ShipSnapshot* findDetailShipSnapshot(
    const SimulationSnapshot& snapshot,
    EntityId id
) noexcept
{
    const auto it = std::find_if(
        snapshot.ships.begin(),
        snapshot.ships.end(),
        [&](const ShipSnapshot& ship)
        {
            return ship.id == id;
        }
    );

    return it == snapshot.ships.end() ? nullptr : &*it;
}

inline const ObjectSnapshot* findDetailObjectSnapshot(
    const SimulationSnapshot& snapshot,
    EntityId id
) noexcept
{
    const auto it = std::find_if(
        snapshot.objects.begin(),
        snapshot.objects.end(),
        [&](const ObjectSnapshot& object)
        {
            return object.id == id;
        }
    );

    return it == snapshot.objects.end() ? nullptr : &*it;
}

inline const game::simulation::OrbitalHubSnapshot* findDetailHubSnapshot(
    const SimulationSnapshot& snapshot,
    const std::string& id
) noexcept
{
    const auto it = std::find_if(
        snapshot.hubs.begin(),
        snapshot.hubs.end(),
        [&](const game::simulation::OrbitalHubSnapshot& hub)
        {
            return hub.id == id;
        }
    );

    return it == snapshot.hubs.end() ? nullptr : &*it;
}

inline DetailMapShipRuntimeSample makeDetailShipSample(
    const ShipSnapshot& ship
)
{
    DetailMapShipRuntimeSample out;
    out.id = ship.id;
    out.role = ship.role;
    out.typeId = ship.typeId;
    out.motionLabKind = ship.motionLabKind;
    out.systemId = ship.transform.motion.systemId;
    out.parentBodyId = ship.transform.motion.parentBodyId;
    out.hubId = ship.transform.motion.hubId;
    out.motionMode = ship.transform.motion.mode;
    out.worldPosition = ship.transform.worldPosition;
    out.worldVelocityMps = ship.transform.motion.worldVelocityMps;
    out.localPositionMeters = ship.transform.motion.localPositionMeters;
    out.localVelocityMps = ship.transform.motion.localVelocityMps;
    out.orientation = ship.transform.orientation;
    return out;
}

inline DetailMapObjectRuntimeSample makeDetailObjectSample(
    const ObjectSnapshot& object
)
{
    DetailMapObjectRuntimeSample out;
    out.id = object.id;
    out.type = object.type;
    out.systemId = object.systemId;
    out.displayName = object.displayName;
    out.ownerName = object.ownerName;
    out.navigationParentBodyId = object.navigationParentBodyId;
    out.worldPosition = object.worldPosition;
    out.linearVelocityMps = object.linearVelocityMps;
    out.orientation = object.orientation;
    out.hubAttachment = object.hubAttachment;
    out.orbitalMotion = object.orbitalMotion;
    return out;
}

inline DetailMapHubRuntimeSample makeDetailHubSample(
    const game::simulation::OrbitalHubSnapshot& hub
)
{
    DetailMapHubRuntimeSample out;
    out.id = hub.id;
    out.name = hub.name;
    out.owner = hub.owner;
    out.systemId = hub.systemId;
    out.parentBodyId = hub.parentBodyId;
    out.worldPosition = hub.worldPosition;
    out.worldVelocityMps = hub.worldVelocityMps;
    out.angularVelocityWorldRadPerSecond =
        hub.angularVelocityWorldRadPerSecond;
    out.orientation = hub.orientation;
    out.primeModuleId = hub.primeModuleId;
    out.motion = hub.motion;
    return out;
}

inline void appendDetailRuntimeEndpoint(
    DetailMapRuntimeSampleResult& out,
    const SimulationSnapshot& snapshot,
    int requestedSystemId
)
{
    for (const auto& ship : snapshot.ships)
    {
        if (ship.transform.motion.systemId != requestedSystemId)
            continue;
        out.ships.push_back(makeDetailShipSample(ship));
    }

    // Details needs the complete local infrastructure inventory, including
    // hub child modules that are intentionally not top-level System-map
    // markers. Do not filter on navigationVisible here.
    for (const auto& object : snapshot.objects)
    {
        if (object.systemId != requestedSystemId)
            continue;
        out.objects.push_back(makeDetailObjectSample(object));
    }

    for (const auto& hub : snapshot.hubs)
    {
        if (hub.systemId != requestedSystemId)
            continue;
        out.hubs.push_back(makeDetailHubSample(hub));
    }
}

/*
    Sample all ordinary replicated runtime entities at the exact server-time
    epoch of a Details response. The response itself carries no entity/map DTO;
    it is only an authoritative epoch/target acknowledgement.

    A system transition changes the meaning of system-local WorldPosition.
    Intermediate samples across different systemId domains are therefore
    omitted rather than inventing a cross-system interpolation.
*/
inline DetailMapRuntimeSampleResult sampleDetailMapRuntimeAtServerTime(
    const std::deque<SimulationSnapshot>& snapshots,
    int requestedSystemId,
    double serverTimeSeconds
)
{
    DetailMapRuntimeSampleResult out;

    if (snapshots.empty() ||
        requestedSystemId < 0 ||
        !std::isfinite(serverTimeSeconds))
    {
        return out;
    }

    constexpr double TimeToleranceSeconds = 1.0e-9;

    const double oldestTime = snapshots.front().metadata.serverTimeSeconds;
    const double newestTime = snapshots.back().metadata.serverTimeSeconds;

    if (serverTimeSeconds > newestTime + TimeToleranceSeconds)
    {
        out.status = DetailMapRuntimeSampleStatus::AwaitingNewerSnapshot;
        return out;
    }

    if (serverTimeSeconds < oldestTime - TimeToleranceSeconds)
    {
        out.status = DetailMapRuntimeSampleStatus::TooOld;
        return out;
    }

    if (snapshots.size() == 1)
    {
        appendDetailRuntimeEndpoint(out, snapshots.front(), requestedSystemId);
        out.status = DetailMapRuntimeSampleStatus::Ready;
        return out;
    }

    const auto window = resolveSnapshotPresentationWindow(
        snapshots,
        serverTimeSeconds,
        [](const SimulationSnapshot& snapshot)
        {
            return snapshot.metadata.serverTimeSeconds;
        }
    );

    if (!window.hasInterpolationBracket)
    {
        out.status = DetailMapRuntimeSampleStatus::TooOld;
        return out;
    }

    const auto& older = snapshots[window.olderIndex];
    const auto& newer = snapshots[window.newerIndex];
    const double alpha = window.interpolationAlpha;

    if (alpha <= TimeToleranceSeconds)
    {
        appendDetailRuntimeEndpoint(out, older, requestedSystemId);
        out.status = DetailMapRuntimeSampleStatus::Ready;
        return out;
    }

    if (alpha >= 1.0 - TimeToleranceSeconds)
    {
        appendDetailRuntimeEndpoint(out, newer, requestedSystemId);
        out.status = DetailMapRuntimeSampleStatus::Ready;
        return out;
    }

    out.ships.reserve(std::min(older.ships.size(), newer.ships.size()));
    for (const auto& oldShip : older.ships)
    {
        const auto* newShip = findDetailShipSnapshot(newer, oldShip.id);
        if (!newShip)
            continue;

        const int oldSystem = oldShip.transform.motion.systemId;
        const int newSystem = newShip->transform.motion.systemId;
        if (!canInterpolateSystemLocalState(oldSystem, newSystem) ||
            oldSystem != requestedSystemId)
        {
            continue;
        }

        auto sample = makeDetailShipSample(*newShip);
        const glm::dvec3 deltaMeters = world::coordinates::relativeMeters(
            newShip->transform.worldPosition,
            oldShip.transform.worldPosition
        );
        sample.worldPosition = world::coordinates::translated(
            oldShip.transform.worldPosition,
            deltaMeters * alpha
        );
        sample.worldVelocityMps =
            oldShip.transform.motion.worldVelocityMps * (1.0 - alpha) +
            newShip->transform.motion.worldVelocityMps * alpha;

        if (oldShip.transform.motion.hubId ==
                newShip->transform.motion.hubId &&
            oldShip.transform.motion.mode ==
                newShip->transform.motion.mode)
        {
            sample.localPositionMeters =
                oldShip.transform.motion.localPositionMeters * (1.0 - alpha) +
                newShip->transform.motion.localPositionMeters * alpha;
            sample.localVelocityMps =
                oldShip.transform.motion.localVelocityMps * (1.0 - alpha) +
                newShip->transform.motion.localVelocityMps * alpha;
        }
        else if (alpha < 0.5)
        {
            sample.motionMode = oldShip.transform.motion.mode;
            sample.localPositionMeters =
                oldShip.transform.motion.localPositionMeters;
            sample.localVelocityMps =
                oldShip.transform.motion.localVelocityMps;
        }

        sample.orientation = interpolateDetailMapOrientation(
            oldShip.transform.orientation,
            newShip->transform.orientation,
            alpha
        );

        if (oldShip.transform.motion.parentBodyId !=
            newShip->transform.motion.parentBodyId)
        {
            sample.parentBodyId = alpha < 0.5
                ? oldShip.transform.motion.parentBodyId
                : newShip->transform.motion.parentBodyId;
        }
        if (oldShip.transform.motion.hubId != newShip->transform.motion.hubId)
        {
            sample.hubId = alpha < 0.5
                ? oldShip.transform.motion.hubId
                : newShip->transform.motion.hubId;
        }

        out.ships.push_back(std::move(sample));
    }

    out.objects.reserve(std::min(older.objects.size(), newer.objects.size()));
    for (const auto& oldObject : older.objects)
    {
        const auto* newObject = findDetailObjectSnapshot(newer, oldObject.id);
        if (!newObject)
            continue;

        if (!canInterpolateSystemLocalState(
                oldObject.systemId,
                newObject->systemId) ||
            oldObject.systemId != requestedSystemId)
        {
            continue;
        }

        auto sample = makeDetailObjectSample(*newObject);
        const glm::dvec3 deltaMeters = world::coordinates::relativeMeters(
            newObject->worldPosition,
            oldObject.worldPosition
        );
        sample.worldPosition = world::coordinates::translated(
            oldObject.worldPosition,
            deltaMeters * alpha
        );
        sample.linearVelocityMps =
            oldObject.linearVelocityMps * (1.0 - alpha) +
            newObject->linearVelocityMps * alpha;
        sample.orientation = interpolateDetailMapOrientation(
            oldObject.orientation,
            newObject->orientation,
            alpha
        );
        sample.orbitalMotion.centerMeters =
            oldObject.orbitalMotion.centerMeters * (1.0 - alpha) +
            newObject->orbitalMotion.centerMeters * alpha;

        // Attachment is stable metadata in normal operation. If a rebind
        // occurs inside the bracket, choose a real endpoint rather than
        // synthesizing a hybrid parent/local offset.
        if (oldObject.hubAttachment.hubId !=
                newObject->hubAttachment.hubId ||
            oldObject.hubAttachment.valid !=
                newObject->hubAttachment.valid)
        {
            sample.hubAttachment = alpha < 0.5
                ? oldObject.hubAttachment
                : newObject->hubAttachment;
        }

        out.objects.push_back(std::move(sample));
    }

    out.hubs.reserve(std::min(older.hubs.size(), newer.hubs.size()));
    for (const auto& oldHub : older.hubs)
    {
        const auto* newHub = findDetailHubSnapshot(newer, oldHub.id);
        if (!newHub)
            continue;

        if (!canInterpolateSystemLocalState(
                oldHub.systemId,
                newHub->systemId) ||
            oldHub.systemId != requestedSystemId)
        {
            continue;
        }

        auto sample = makeDetailHubSample(*newHub);
        const glm::dvec3 deltaMeters = world::coordinates::relativeMeters(
            newHub->worldPosition,
            oldHub.worldPosition
        );
        sample.worldPosition = world::coordinates::translated(
            oldHub.worldPosition,
            deltaMeters * alpha
        );
        sample.worldVelocityMps =
            oldHub.worldVelocityMps * (1.0 - alpha) +
            newHub->worldVelocityMps * alpha;
        sample.angularVelocityWorldRadPerSecond =
            oldHub.angularVelocityWorldRadPerSecond * (1.0 - alpha) +
            newHub->angularVelocityWorldRadPerSecond * alpha;
        sample.orientation = interpolateDetailMapOrientation(
            oldHub.orientation,
            newHub->orientation,
            alpha
        );
        sample.motion.centerMeters =
            oldHub.motion.centerMeters * (1.0 - alpha) +
            newHub->motion.centerMeters * alpha;

        out.hubs.push_back(std::move(sample));
    }

    out.status = DetailMapRuntimeSampleStatus::Ready;
    return out;
}

} // namespace game::client
