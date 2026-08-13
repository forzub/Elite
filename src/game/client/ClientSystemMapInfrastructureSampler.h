#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "src/game/client/ClientSpatialDomain.h"
#include "src/game/client/SnapshotPresentationWindow.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/world/types/ObjectType.h"

namespace game::client
{

enum class SystemMapInfrastructureSampleStatus
{
    Ready,
    AwaitingNewerSnapshot,
    TooOld
};

struct SystemMapStaticObjectSample
{
    EntityId id {};
    ObjectType type = ObjectType::None;
    int systemId = -1;

    std::string displayName;
    std::string ownerName;
    std::string parentBodyId;
    bool navigationVisible = false;

    std::string hubId;
    world::coordinates::WorldPosition worldPosition;
    world::orbits::OrbitalMotion orbitalMotion;
};

struct SystemMapHubSample
{
    std::string id;
    std::string name;
    std::string owner;
    int systemId = -1;
    std::string parentBodyId;
    world::coordinates::WorldPosition worldPosition;
    world::orbits::OrbitalMotion motion;
};

struct SystemMapInfrastructureSampleResult
{
    SystemMapInfrastructureSampleStatus status =
        SystemMapInfrastructureSampleStatus::AwaitingNewerSnapshot;
    std::vector<SystemMapStaticObjectSample> objects;
    std::vector<SystemMapHubSample> hubs;
};

inline const ObjectSnapshot* findObjectSnapshot(
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

    return it == snapshot.objects.end()
        ? nullptr
        : &*it;
}

inline const game::simulation::OrbitalHubSnapshot* findHubSnapshot(
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

    return it == snapshot.hubs.end()
        ? nullptr
        : &*it;
}

inline SystemMapStaticObjectSample makeSystemMapStaticObjectSample(
    const ObjectSnapshot& object
)
{
    SystemMapStaticObjectSample out;
    out.id = object.id;
    out.type = object.type;
    out.systemId = object.systemId;
    out.displayName = object.displayName;
    out.ownerName = object.ownerName;
    out.parentBodyId = object.navigationParentBodyId;
    out.navigationVisible = object.navigationVisible;
    out.hubId = object.hubAttachment.valid
        ? object.hubAttachment.hubId
        : std::string{};
    out.worldPosition = object.worldPosition;
    out.orbitalMotion = object.orbitalMotion;
    return out;
}

inline SystemMapHubSample makeSystemMapHubSample(
    const game::simulation::OrbitalHubSnapshot& hub
)
{
    SystemMapHubSample out;
    out.id = hub.id;
    out.name = hub.name;
    out.owner = hub.owner;
    out.systemId = hub.systemId;
    out.parentBodyId = hub.parentBodyId;
    out.worldPosition = hub.worldPosition;
    out.motion = hub.motion;
    return out;
}

inline void appendInfrastructureAtEndpoint(
    SystemMapInfrastructureSampleResult& out,
    const SimulationSnapshot& snapshot,
    int requestedSystemId
)
{
    for (const auto& object : snapshot.objects)
    {
        if (object.systemId != requestedSystemId ||
            !object.navigationVisible)
        {
            continue;
        }

        out.objects.push_back(
            makeSystemMapStaticObjectSample(object)
        );
    }

    for (const auto& hub : snapshot.hubs)
    {
        if (hub.systemId != requestedSystemId)
            continue;

        out.hubs.push_back(makeSystemMapHubSample(hub));
    }
}

/*
    Sample ordinary replicated infrastructure at the exact server-time epoch of
    a System-map response. Hubs and station/module instances therefore share the
    same authoritative timeline/history rule as real ships; the map protocol no
    longer forms a second dynamic-state channel for infrastructure.
*/
inline SystemMapInfrastructureSampleResult
sampleSystemMapInfrastructureAtServerTime(
    const std::deque<SimulationSnapshot>& snapshots,
    int requestedSystemId,
    double serverTimeSeconds
)
{
    SystemMapInfrastructureSampleResult out;

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
        out.status =
            SystemMapInfrastructureSampleStatus::AwaitingNewerSnapshot;
        return out;
    }

    if (serverTimeSeconds < oldestTime - TimeToleranceSeconds)
    {
        out.status = SystemMapInfrastructureSampleStatus::TooOld;
        return out;
    }

    if (snapshots.size() == 1)
    {
        appendInfrastructureAtEndpoint(
            out,
            snapshots.front(),
            requestedSystemId
        );
        out.status = SystemMapInfrastructureSampleStatus::Ready;
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
        out.status = SystemMapInfrastructureSampleStatus::TooOld;
        return out;
    }

    const auto& older = snapshots[window.olderIndex];
    const auto& newer = snapshots[window.newerIndex];
    const double alpha = window.interpolationAlpha;

    if (alpha <= TimeToleranceSeconds)
    {
        appendInfrastructureAtEndpoint(out, older, requestedSystemId);
        out.status = SystemMapInfrastructureSampleStatus::Ready;
        return out;
    }

    if (alpha >= 1.0 - TimeToleranceSeconds)
    {
        appendInfrastructureAtEndpoint(out, newer, requestedSystemId);
        out.status = SystemMapInfrastructureSampleStatus::Ready;
        return out;
    }

    out.objects.reserve(std::min(older.objects.size(), newer.objects.size()));
    for (const auto& olderObject : older.objects)
    {
        const auto* newerObject = findObjectSnapshot(newer, olderObject.id);
        if (!newerObject)
            continue;

        if (!canInterpolateSystemLocalState(
                olderObject.systemId,
                newerObject->systemId))
        {
            continue;
        }

        if (olderObject.systemId != requestedSystemId ||
            !newerObject->navigationVisible)
        {
            continue;
        }

        auto sample = makeSystemMapStaticObjectSample(*newerObject);
        const glm::dvec3 deltaMeters =
            world::coordinates::relativeMeters(
                newerObject->worldPosition,
                olderObject.worldPosition
            );
        sample.worldPosition = world::coordinates::translated(
            olderObject.worldPosition,
            deltaMeters * alpha
        );

        // Orbit centers can follow moving parent bodies. Keep the exact map
        // epoch coherent instead of taking an arbitrary endpoint center.
        sample.orbitalMotion.centerMeters =
            olderObject.orbitalMotion.centerMeters * (1.0 - alpha) +
            newerObject->orbitalMotion.centerMeters * alpha;

        out.objects.push_back(std::move(sample));
    }

    out.hubs.reserve(std::min(older.hubs.size(), newer.hubs.size()));
    for (const auto& olderHub : older.hubs)
    {
        const auto* newerHub = findHubSnapshot(newer, olderHub.id);
        if (!newerHub)
            continue;

        if (!canInterpolateSystemLocalState(
                olderHub.systemId,
                newerHub->systemId))
        {
            continue;
        }

        if (olderHub.systemId != requestedSystemId)
            continue;

        auto sample = makeSystemMapHubSample(*newerHub);
        const glm::dvec3 deltaMeters =
            world::coordinates::relativeMeters(
                newerHub->worldPosition,
                olderHub.worldPosition
            );
        sample.worldPosition = world::coordinates::translated(
            olderHub.worldPosition,
            deltaMeters * alpha
        );
        sample.motion.centerMeters =
            olderHub.motion.centerMeters * (1.0 - alpha) +
            newerHub->motion.centerMeters * alpha;

        out.hubs.push_back(std::move(sample));
    }

    out.status = SystemMapInfrastructureSampleStatus::Ready;
    return out;
}

} // namespace game::client
