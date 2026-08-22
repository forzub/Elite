#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "src/game/client/ClientSpatialDomain.h"
#include "src/game/client/SnapshotPresentationWindow.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/simulation/SimulationSnapshot.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/types/ObjectType.h"

namespace game::client
{

enum class SystemMapShipSampleStatus
{
    Ready,
    AwaitingNewerSnapshot,
    TooOld
};

struct SystemMapShipSample
{
    EntityId id {};
    ShipInstanceId instanceId = 0;
    ShipRole role = ShipRole::NPC;
    ObjectType typeId = ObjectType::None;
    game::diagnostics::HubMotionLabActorKind motionLabKind =
        game::diagnostics::HubMotionLabActorKind::None;
    int systemId = -1;
    std::string parentBodyId;
    std::string hubId;
    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    glm::mat4 orientation {1.0f};
};

struct SystemMapShipSampleResult
{
    SystemMapShipSampleStatus status =
        SystemMapShipSampleStatus::AwaitingNewerSnapshot;
    std::vector<SystemMapShipSample> ships;
};

inline const ShipSnapshot* findShipSnapshot(
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

    return it == snapshot.ships.end()
        ? nullptr
        : &*it;
}

inline SystemMapShipSample makeSystemMapShipSample(
    const ShipSnapshot& ship
)
{
    SystemMapShipSample out;
    out.id = ship.id;
    out.instanceId = ship.instanceId;
    out.role = ship.role;
    out.typeId = ship.typeId;
    out.motionLabKind = ship.motionLabKind;
    out.systemId = ship.transform.motion.systemId;
    out.parentBodyId = ship.transform.motion.parentBodyId;
    out.hubId = ship.transform.motion.hubId;
    out.worldPosition = ship.transform.worldPosition;
    out.worldVelocityMps = ship.transform.motion.worldVelocityMps;
    out.orientation = ship.transform.orientation;
    return out;
}

/*
    Sample ordinary replicated ships at the exact server-time epoch carried by
    a System-map response. The map service must never mix a server-built map
    epoch with "whatever ship transform happens to be newest on the client".

    System-local WorldPosition values from different systemId domains are never
    interpolated. During such a discontinuity the ship is omitted from an
    in-between sample rather than drawing a physically meaningless cross-system
    path; an exact endpoint sample remains valid.
*/
inline SystemMapShipSampleResult sampleSystemMapShipsAtServerTime(
    const std::deque<SimulationSnapshot>& snapshots,
    int requestedSystemId,
    double serverTimeSeconds
)
{
    SystemMapShipSampleResult out;

    if (snapshots.empty() ||
        requestedSystemId < 0 ||
        !std::isfinite(serverTimeSeconds))
    {
        return out;
    }

    constexpr double TimeToleranceSeconds = 1.0e-9;

    const double oldestTime =
        snapshots.front().metadata.serverTimeSeconds;
    const double newestTime =
        snapshots.back().metadata.serverTimeSeconds;

    if (serverTimeSeconds > newestTime + TimeToleranceSeconds)
    {
        out.status = SystemMapShipSampleStatus::AwaitingNewerSnapshot;
        return out;
    }

    if (serverTimeSeconds < oldestTime - TimeToleranceSeconds)
    {
        out.status = SystemMapShipSampleStatus::TooOld;
        return out;
    }

    if (snapshots.size() == 1)
    {
        for (const auto& ship : snapshots.front().ships)
        {
            if (ship.transform.motion.systemId != requestedSystemId)
                continue;

            out.ships.push_back(makeSystemMapShipSample(ship));
        }

        out.status = SystemMapShipSampleStatus::Ready;
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
        // The requested epoch is inside retained history but no valid adjacent
        // time pair can represent it. Treat the response as stale/broken rather
        // than silently sampling a different epoch.
        out.status = SystemMapShipSampleStatus::TooOld;
        return out;
    }

    const auto& older = snapshots[window.olderIndex];
    const auto& newer = snapshots[window.newerIndex];
    const double alpha = window.interpolationAlpha;

    if (alpha <= TimeToleranceSeconds)
    {
        for (const auto& ship : older.ships)
        {
            if (ship.transform.motion.systemId != requestedSystemId)
                continue;

            out.ships.push_back(makeSystemMapShipSample(ship));
        }

        out.status = SystemMapShipSampleStatus::Ready;
        return out;
    }

    if (alpha >= 1.0 - TimeToleranceSeconds)
    {
        for (const auto& ship : newer.ships)
        {
            if (ship.transform.motion.systemId != requestedSystemId)
                continue;

            out.ships.push_back(makeSystemMapShipSample(ship));
        }

        out.status = SystemMapShipSampleStatus::Ready;
        return out;
    }

    out.ships.reserve(std::min(older.ships.size(), newer.ships.size()));

    for (const auto& olderShip : older.ships)
    {
        const auto* newerShip = findShipSnapshot(newer, olderShip.id);
        if (!newerShip)
            continue;

        const int olderSystemId = olderShip.transform.motion.systemId;
        const int newerSystemId = newerShip->transform.motion.systemId;

        if (!canInterpolateSystemLocalState(olderSystemId, newerSystemId))
        {
            // A system transfer changes the meaning of WorldPosition. Never
            // fabricate a line between two unrelated system-local domains.
            continue;
        }

        if (olderSystemId != requestedSystemId)
            continue;

        SystemMapShipSample sample = makeSystemMapShipSample(*newerShip);

        const glm::dvec3 deltaMeters =
            world::coordinates::relativeMeters(
                newerShip->transform.worldPosition,
                olderShip.transform.worldPosition
            );

        sample.worldPosition =
            world::coordinates::translated(
                olderShip.transform.worldPosition,
                deltaMeters * alpha
            );
        sample.worldVelocityMps =
            olderShip.transform.motion.worldVelocityMps * (1.0 - alpha) +
            newerShip->transform.motion.worldVelocityMps * alpha;

        glm::quat olderOrientation = glm::normalize(
            glm::quat_cast(glm::mat3(olderShip.transform.orientation))
        );
        glm::quat newerOrientation = glm::normalize(
            glm::quat_cast(glm::mat3(newerShip->transform.orientation))
        );
        if (glm::dot(olderOrientation, newerOrientation) < 0.0f)
            newerOrientation = -newerOrientation;
        sample.orientation = glm::mat4_cast(
            glm::normalize(
                glm::slerp(
                    olderOrientation,
                    newerOrientation,
                    static_cast<float>(alpha)
                )
            )
        );

        if (olderShip.transform.motion.parentBodyId !=
            newerShip->transform.motion.parentBodyId)
        {
            sample.parentBodyId =
                alpha < 0.5
                    ? olderShip.transform.motion.parentBodyId
                    : newerShip->transform.motion.parentBodyId;
        }

        if (olderShip.transform.motion.hubId !=
            newerShip->transform.motion.hubId)
        {
            sample.hubId =
                alpha < 0.5
                    ? olderShip.transform.motion.hubId
                    : newerShip->transform.motion.hubId;
        }

        out.ships.push_back(std::move(sample));
    }

    out.status = SystemMapShipSampleStatus::Ready;
    return out;
}

} // namespace game::client
