#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/navigation/NavigationTrackingState.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/render/HUD/NavigationHudMarker.h"

namespace game::presentation
{

inline bool cockpitNavigationUsesGlobalSpeed(
    game::navigation::MotionMode motionMode
)
{
    return motionMode == game::navigation::MotionMode::Cruise ||
           motionMode == game::navigation::MotionMode::JumpTransit;
}

struct NavigationHudTargetSpeed
{
    NavigationHudSpeedMode mode = NavigationHudSpeedMode::Global;
    double speedMps = 0.0;
};

inline NavigationHudTargetSpeed resolveCockpitNavigationTargetSpeed(
    const game::navigation::DynamicMotionState& playerMotion,
    const glm::dvec3& targetRelativeMeters,
    const glm::dvec3& targetWorldVelocityMps
)
{
    if (cockpitNavigationUsesGlobalSpeed(playerMotion.mode))
    {
        return {
            NavigationHudSpeedMode::Global,
            glm::length(targetWorldVelocityMps)
        };
    }

    // The cockpit's own speed is |localVelocityMps|.  A tracked target must
    // therefore be expressed in the same travel frame, not as closing speed
    // against the player.  Otherwise two ships flying side-by-side can show
    // values hundreds of m/s apart even though their local speeds match.
    if (playerMotion.travelFrame.valid)
    {
        const glm::dvec3 playerSystemWorldMeters =
            playerMotion.travelFrame.localToWorldPosition(
                playerMotion.localPositionMeters
            );
        const glm::dvec3 targetSystemWorldMeters =
            playerSystemWorldMeters + targetRelativeMeters;
        const glm::dvec3 targetLocalVelocity =
            playerMotion.travelFrame.worldToLocalVelocity(
                targetSystemWorldMeters,
                targetWorldVelocityMps
            );

        return {
            NavigationHudSpeedMode::Relative,
            glm::length(targetLocalVelocity)
        };
    }

    // Without a valid common travel frame, do not manufacture a relative
    // number by subtracting unrelated vectors.  Fall back explicitly to the
    // inertial/global speed until a local frame is available.
    return {
        NavigationHudSpeedMode::Global,
        glm::length(targetWorldVelocityMps)
    };
}

namespace detail
{
inline bool parseEntityObjectId(
    const std::string& objectId,
    std::uint32_t& outEntityId
)
{
    constexpr const char* prefix = "entity:";
    if (objectId.rfind(prefix, 0) != 0)
        return false;

    try
    {
        const auto value = std::stoull(objectId.substr(7));
        if (value > static_cast<unsigned long long>(
                std::numeric_limits<std::uint32_t>::max()))
        {
            return false;
        }
        outEntityId = static_cast<std::uint32_t>(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

struct ResolvedTacticalKinematics
{
    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    bool valid = false;
};

inline ResolvedTacticalKinematics resolveTacticalKinematics(
    const ClientWorldState& world,
    const std::string& objectId
)
{
    ResolvedTacticalKinematics out;

    if (objectId.empty() || objectId == "player")
        return out;

    std::uint32_t entityId = 0;
    if (parseEntityObjectId(objectId, entityId))
    {
        const auto ship = world.ships().find(entityId);
        if (ship != world.ships().end())
        {
            out.worldPosition = ship->second.renderTransform.worldPosition;
            out.worldVelocityMps =
                ship->second.renderTransform.motion.worldVelocityMps;
            out.valid = true;
            return out;
        }

        const auto object = world.objects().find(entityId);
        if (object != world.objects().end())
        {
            out.worldPosition = object->second.renderWorldPosition;
            out.worldVelocityMps = object->second.linearVelocityMps;
            out.valid = true;
            return out;
        }
    }

    const auto hub = world.hubs().find(objectId);
    if (hub != world.hubs().end())
    {
        out.worldPosition = hub->second.worldPosition;
        out.worldVelocityMps = hub->second.worldVelocityMps;
        out.valid = true;
    }

    return out;
}
} // namespace detail

inline std::vector<NavigationHudMarker> buildNavigationHudMarkers(
    const game::navigation::NavigationTrackingState& tracking,
    const ClientWorldState& world,
    const ClientShipState& player,
    const NavigationHudVocabulary& vocabulary = NavigationHudVocabulary{}
)
{
    std::vector<NavigationHudMarker> out;
    out.reserve(
        tracking.tacticalObjects().size() +
        tracking.celestialBodies().size() +
        tracking.waypoints().size()
    );

    const auto& playerPosition = player.renderTransform.worldPosition;

    for (const auto& [id, tracked] : tracking.tacticalObjects())
    {
        const auto resolved = detail::resolveTacticalKinematics(world, id);
        if (!resolved.valid)
            continue;

        const glm::dvec3 relative = world::coordinates::relativeMeters(
            resolved.worldPosition,
            playerPosition
        );
        const double distance = glm::length(relative);
        if (distance <= 0.01)
            continue;

        NavigationHudMarker marker;
        marker.stableId = id;
        marker.shape = NavigationHudMarkerShape::TacticalTriangle;
        marker.relativePositionMeters = relative;
        marker.distanceMeters = distance;
        marker.typeText = tracked.typeName.empty()
            ? vocabulary.objectText
            : tracked.typeName;
        marker.nameText = tracked.displayName;
        marker.displayIndex = tracked.displayIndex;
        marker.color = tracked.color;

        const auto speed = resolveCockpitNavigationTargetSpeed(
            player.renderTransform.motion,
            relative,
            resolved.worldVelocityMps
        );
        marker.speedMode = speed.mode;
        marker.speedPrefixText =
            speed.mode == NavigationHudSpeedMode::Relative
                ? vocabulary.relativeSpeedShort
                : vocabulary.globalSpeedShort;
        marker.speedMps = speed.speedMps;

        out.push_back(std::move(marker));
    }

    for (const auto& [id, tracked] : tracking.celestialBodies())
    {
        NavigationHudMarker marker;
        marker.stableId = id;
        marker.shape = NavigationHudMarkerShape::CelestialDiamond;
        marker.relativePositionMeters = world::coordinates::relativeMeters(
            tracked.worldPosition,
            playerPosition
        );
        marker.distanceMeters = glm::length(marker.relativePositionMeters);
        marker.typeText = tracked.typeName.empty()
            ? vocabulary.celestialText
            : tracked.typeName;
        marker.nameText = tracked.displayName;
        marker.speedMode = NavigationHudSpeedMode::None;
        marker.displayIndex = tracked.displayIndex;
        marker.color = tracked.color;

        if (marker.distanceMeters > 0.01)
            out.push_back(std::move(marker));
    }

    for (const auto& waypoint : tracking.waypoints())
    {
        NavigationHudMarker marker;
        marker.stableId = "waypoint:" + std::to_string(waypoint.id);
        marker.shape = NavigationHudMarkerShape::WaypointCorners;
        marker.relativePositionMeters = world::coordinates::relativeMeters(
            waypoint.worldPosition,
            playerPosition
        );
        marker.distanceMeters = glm::length(marker.relativePositionMeters);
        marker.displayIndex =
            waypoint.role == game::navigation::NavigationWaypointRole::Intermediate
                ? waypoint.sequence
                : 0;
        marker.typeText =
            waypoint.role == game::navigation::NavigationWaypointRole::Finish
                ? vocabulary.finishText
                : vocabulary.waypointText;
        marker.nameText = waypoint.address.empty()
            ? waypoint.displayName
            : waypoint.address;
        marker.speedMode = NavigationHudSpeedMode::None;
        marker.color =
            waypoint.role == game::navigation::NavigationWaypointRole::Finish
                ? glm::vec4(1.00f, 0.82f, 0.30f, 0.88f)
                : waypoint.role == game::navigation::NavigationWaypointRole::Intermediate
                    ? glm::vec4(0.45f, 0.95f, 0.62f, 0.88f)
                    : glm::vec4(0.32f, 0.66f, 1.00f, 0.82f);

        if (marker.distanceMeters > 0.01)
            out.push_back(std::move(marker));
    }

    return out;
}

} // namespace game::presentation
