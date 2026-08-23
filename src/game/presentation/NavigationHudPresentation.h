#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/navigation/ClientNavigationWorkspace.h"
#include "src/game/navigation/HubSemanticAnchorCatalog.h"
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

inline ResolvedTacticalKinematics resolveHubFromPlayerPresentationFrame(
    const ClientHubState& hub,
    const ClientShipState& player
)
{
    ResolvedTacticalKinematics out;
    const auto& frame = player.renderReferenceFrame;

    // frameId identifies the ship-owned travel frame (for example
    // "ship_travel_1").  Hub membership is carried separately by hubId.
    // Comparing frameId with hub.id silently rejects the co-frame path and
    // falls back to the newest discrete Hub snapshot, producing the visible
    // every-other-frame/snapshot staircase in the cockpit marker.
    if (!frame.valid ||
        frame.type != game::navigation::MotionMode::HubTactical ||
        frame.systemId != hub.systemId ||
        frame.hubId != hub.id)
    {
        return out;
    }

    // The local player has already been rebased through this render-time
    // reference frame.  Resolve the Hub origin from that exact same sample.
    // Mixing this player pose with hub.worldPosition (the newest replication
    // snapshot) produces a snapshot-rate sawtooth in the cockpit marker.
    out.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(frame.originMeters);
    out.worldVelocityMps = frame.velocityMetersPerSecond;
    out.valid = true;
    return out;
}

inline ResolvedTacticalKinematics resolveTacticalKinematics(
    const ClientWorldState& world,
    const ClientShipState& player,
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
        // Current Hub: use the same delayed/interpolated reference-frame
        // sample that produced the local player's renderTransform.  This is
        // the strongest co-frame guarantee and removes the visible staircase.
        out = resolveHubFromPlayerPresentationFrame(hub->second, player);
        if (out.valid)
            return out;

        // A Hub outside the player's current co-frame keeps the ordinary
        // replicated fallback.  HUD presentation must not acquire a second
        // timeline/transport dependency merely to resolve a marker.
        out.worldPosition = hub->second.worldPosition;
        out.worldVelocityMps = hub->second.worldVelocityMps;
        out.valid = true;
    }

    return out;
}

inline ResolvedTacticalKinematics resolveInfrastructureKinematics(
    const ClientWorldState& world,
    int systemId,
    const std::string& stableObjectId
)
{
    ResolvedTacticalKinematics out;
    if (stableObjectId.empty())
        return out;

    for (const auto& [entityId, object] : world.objects())
    {
        (void)entityId;
        if (object.systemId != systemId ||
            !object.hubAttachment.valid ||
            object.hubAttachment.moduleId != stableObjectId)
        {
            continue;
        }

        out.worldPosition = object.renderWorldPosition;
        out.worldVelocityMps = object.linearVelocityMps;
        out.valid = true;
        return out;
    }
    return out;
}

inline ResolvedTacticalKinematics resolveSemanticAnchorKinematics(
    const ClientWorldState& world,
    const game::navigation::HubSemanticAnchorCatalog* catalog,
    int systemId,
    const std::string& hubModuleId,
    const std::string& anchorId,
    double universeTimeSeconds
)
{
    ResolvedTacticalKinematics out;
    if (!catalog || hubModuleId.empty() || anchorId.empty())
        return out;

    const auto* definition = catalog->find(hubModuleId, anchorId);
    if (!definition)
        return out;

    for (const auto& [entityId, object] : world.objects())
    {
        (void)entityId;
        if (object.systemId != systemId ||
            !object.hubAttachment.valid ||
            object.hubAttachment.moduleId != hubModuleId)
        {
            continue;
        }

        const auto resolved = game::navigation::resolveHubSemanticAnchor(
            *definition,
            systemId,
            universeTimeSeconds,
            world::coordinates::fullMeters(object.renderWorldPosition),
            object.linearVelocityMps,
            object.renderOrientation,
            object.angularVelocityWorldRadPerSecond
        );
        out.worldPosition = world::coordinates::makeWorldPositionFromMeters(
            resolved.positionMeters
        );
        out.worldVelocityMps = resolved.velocityMps;
        out.valid = true;
        return out;
    }
    return out;
}

inline ResolvedTacticalKinematics resolveRouteTargetKinematics(
    const ClientWorldState& world,
    const ClientShipState& player,
    const game::navigation::RouteTargetRef& target
)
{
    if (target.kind == game::navigation::NavigationRouteAnchorKind::Ship &&
        target.shipInstanceId != 0)
    {
        for (const auto& [entityId, ship] : world.ships())
        {
            (void)entityId;
            if (ship.instanceId != target.shipInstanceId)
                continue;
            ResolvedTacticalKinematics out;
            out.worldPosition = ship.renderTransform.worldPosition;
            out.worldVelocityMps = ship.renderTransform.motion.worldVelocityMps;
            out.valid = true;
            return out;
        }
        return {};
    }

    if (target.kind == game::navigation::NavigationRouteAnchorKind::Hub &&
        !target.stableObjectId.empty())
    {
        return resolveTacticalKinematics(world, player, target.stableObjectId);
    }

    return {};
}
} // namespace detail

inline std::vector<NavigationHudMarker> buildNavigationHudMarkers(
    const game::navigation::ClientNavigationWorkspace& navigation,
    const ClientWorldState& world,
    const ClientShipState& player,
    const NavigationHudVocabulary& vocabulary = NavigationHudVocabulary{},
    const game::navigation::HubSemanticAnchorCatalog* semanticAnchors = nullptr,
    double universeTimeSeconds = 0.0
)
{
    std::vector<NavigationHudMarker> out;
    out.reserve(
        navigation.targets().tacticalObjects().size() +
        navigation.targets().celestialBodies().size() +
        navigation.targets().infrastructure().size() +
        navigation.targets().semanticAnchors().size() +
        navigation.routePlan().routeSize() +
        (navigation.routePlan().hasStart() ? 1u : 0u)
    );

    const auto& playerPosition = player.renderTransform.worldPosition;

    const bool showTargetMarkers = navigation.modules().enabled(
        game::navigation::NavigationModuleId::HudTargetMarkers
    );
    const bool showRouteMarkers = navigation.modules().enabled(
        game::navigation::NavigationModuleId::HudRouteMarkers
    );

    if (showTargetMarkers)
    for (const auto& [id, tracked] : navigation.targets().tacticalObjects())
    {
        if (const auto* route = navigation.routePlan().findBySourceObjectId(id);
            route && route->role !=
                game::navigation::NavigationWaypointRole::None)
        {
            continue;
        }

        const auto resolved =
            detail::resolveTacticalKinematics(world, player, id);
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

    if (showTargetMarkers)
    for (const auto& [id, tracked] : navigation.targets().celestialBodies())
    {
        if (const auto* route = navigation.routePlan().findBySourceObjectId(id);
            route && route->role !=
                game::navigation::NavigationWaypointRole::None)
        {
            continue;
        }

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
        marker.displayIndex = 0;
        marker.color = tracked.color;

        if (marker.distanceMeters > 0.01)
            out.push_back(std::move(marker));
    }

    if (showTargetMarkers)
    for (const auto& [id, tracked] : navigation.targets().infrastructure())
    {
        const auto resolved = detail::resolveInfrastructureKinematics(
            world,
            tracked.systemId,
            tracked.stableObjectId
        );
        if (!resolved.valid)
            continue;

        NavigationHudMarker marker;
        marker.stableId = id;
        marker.shape = NavigationHudMarkerShape::CelestialDiamond;
        marker.relativePositionMeters = world::coordinates::relativeMeters(
            resolved.worldPosition,
            playerPosition
        );
        marker.distanceMeters = glm::length(marker.relativePositionMeters);
        marker.typeText = tracked.typeName.empty()
            ? vocabulary.objectText
            : tracked.typeName;
        marker.nameText = tracked.displayName;
        marker.speedMode = NavigationHudSpeedMode::None;
        marker.displayIndex = 0;
        marker.color = tracked.color;
        if (marker.distanceMeters > 0.01)
            out.push_back(std::move(marker));
    }

    if (showTargetMarkers)
    for (const auto& [id, tracked] : navigation.targets().semanticAnchors())
    {
        const auto resolved = detail::resolveSemanticAnchorKinematics(
            world,
            semanticAnchors,
            tracked.systemId,
            tracked.hubModuleId,
            tracked.anchorId,
            universeTimeSeconds
        );
        if (!resolved.valid)
            continue;

        NavigationHudMarker marker;
        marker.stableId = id;
        marker.shape = NavigationHudMarkerShape::WaypointCorners;
        marker.relativePositionMeters = world::coordinates::relativeMeters(
            resolved.worldPosition,
            playerPosition
        );
        marker.distanceMeters = glm::length(marker.relativePositionMeters);
        marker.typeText = tracked.typeName.empty()
            ? vocabulary.objectText
            : tracked.typeName;
        marker.nameText = tracked.displayName;
        marker.speedMode = NavigationHudSpeedMode::None;
        marker.displayIndex = 0;
        marker.color = tracked.color;
        if (marker.distanceMeters > 0.01)
            out.push_back(std::move(marker));
    }

    if (!showRouteMarkers || !navigation.routePlan().routeVisibleOnHud())
        return out;

    // START is part of the route model, but drawing the occupied player ship as
    // its own navigation target is pure noise. A remote owned ship/drone stays
    // visible because that is exactly the executor the player is dispatching.
    if (navigation.routePlan().hasStart() &&
        !game::navigation::sameNavigationAsset(
            navigation.routePlan().start().executor,
            navigation.localControlledAsset()))
    {
        const auto* startAsset = navigation.ownedAssets().find(
            navigation.routePlan().start().executor
        );
        if (startAsset && startAsset->kinematicsValid)
        {
            NavigationHudMarker marker;
            marker.stableId = "route:start";
            marker.shape = NavigationHudMarkerShape::WaypointCorners;
            marker.relativePositionMeters = world::coordinates::relativeMeters(
                startAsset->worldPosition,
                playerPosition
            );
            marker.distanceMeters = glm::length(marker.relativePositionMeters);
            marker.typeText = vocabulary.startText;
            marker.nameText = startAsset->displayName;
            marker.displayIndex = 0;
            const auto speed = resolveCockpitNavigationTargetSpeed(
                player.renderTransform.motion,
                marker.relativePositionMeters,
                startAsset->worldVelocityMps
            );
            marker.speedMode = speed.mode;
            marker.speedPrefixText =
                speed.mode == NavigationHudSpeedMode::Relative
                    ? vocabulary.relativeSpeedShort
                    : vocabulary.globalSpeedShort;
            marker.speedMps = speed.speedMps;
            marker.color = glm::vec4(0.40f, 0.72f, 1.00f, 0.88f);

            if (marker.distanceMeters > 0.01)
                out.push_back(std::move(marker));
        }
    }

    for (const auto* waypointPtr : navigation.routePlan().orderedRouteWaypoints())
    {
        const auto& waypoint = *waypointPtr;
        if (!waypoint.showOnHud)
            continue;

        world::coordinates::WorldPosition routeTargetPosition =
            waypoint.worldPosition;
        glm::dvec3 routeTargetVelocityMps {0.0};
        bool hasLiveRouteKinematics = false;
        if (waypoint.dynamicTarget)
        {
            const auto resolved = detail::resolveRouteTargetKinematics(
                world,
                player,
                waypoint.target
            );
            if (resolved.valid)
            {
                routeTargetPosition = resolved.worldPosition;
                routeTargetVelocityMps = resolved.worldVelocityMps;
                hasLiveRouteKinematics = true;
            }
        }

        NavigationHudMarker marker;
        marker.stableId = "waypoint:" + std::to_string(waypoint.id);
        marker.shape = NavigationHudMarkerShape::WaypointCorners;
        marker.relativePositionMeters = world::coordinates::relativeMeters(
            routeTargetPosition,
            playerPosition
        );
        marker.distanceMeters = glm::length(marker.relativePositionMeters);
        // Route order is cockpit-critical. FINISH is still the last route
        // point, so it receives the next visible number instead of becoming
        // an anonymous symbol.
        marker.displayIndex =
            waypoint.role == game::navigation::NavigationWaypointRole::Intermediate
                ? waypoint.sequence
                : waypoint.role == game::navigation::NavigationWaypointRole::Finish
                    ? static_cast<int>(navigation.routePlan().routeSize())
                    : 0;
        marker.typeText =
            (waypoint.role == game::navigation::NavigationWaypointRole::Finish
                ? vocabulary.finishText
                : vocabulary.waypointText) +
            std::string(" ") +
            std::to_string(marker.displayIndex);
        marker.nameText = waypoint.address.empty()
            ? waypoint.displayName
            : waypoint.address;
        marker.speedMode = NavigationHudSpeedMode::None;
        if (hasLiveRouteKinematics)
        {
            const auto speed = resolveCockpitNavigationTargetSpeed(
                player.renderTransform.motion,
                marker.relativePositionMeters,
                routeTargetVelocityMps
            );
            marker.speedMode = speed.mode;
            marker.speedPrefixText =
                speed.mode == NavigationHudSpeedMode::Relative
                    ? vocabulary.relativeSpeedShort
                    : vocabulary.globalSpeedShort;
            marker.speedMps = speed.speedMps;
        }
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
