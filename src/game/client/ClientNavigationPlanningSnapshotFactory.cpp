#include "src/game/client/ClientNavigationPlanningSnapshotFactory.h"

#include <algorithm>
#include <cmath>

#include "src/game/client/ClientWorldState.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/world/navigation/NavigationObstacleFactory.h"

namespace game::client
{
namespace
{
constexpr double CoordinateRoundTripToleranceMeters = 1.0e-3;
constexpr double DiagnosticHubInfrastructureClearanceMeters = 80.0;

bool finiteVec(const glm::dvec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

game::navigation::NavigationPlanningEpoch makeEpoch(
    const game::network::SnapshotMetadata& sourceMetadata,
    double serverTimeSeconds,
    double universeTimeSeconds
)
{
    game::navigation::NavigationPlanningEpoch epoch;
    epoch.sourceTick = sourceMetadata.serverTick;
    epoch.serverTimeSeconds = serverTimeSeconds;
    epoch.universeTimeSeconds = universeTimeSeconds;
    epoch.universeTimelineRevision =
        sourceMetadata.universeTimelineRevision;
    return epoch;
}

void predictUnattachedObject(
    DetailMapObjectRuntimeSample& object,
    double deltaGameplaySeconds
)
{
    const game::navigation::WorldKinematicState source {
        world::coordinates::fullMeters(object.worldPosition),
        object.linearVelocityMps,
        glm::dvec3(0.0)
    };
    const auto predicted =
        game::navigation::NavigationWorldPredictor::predictConstantVelocity(
            source,
            deltaGameplaySeconds
        );
    object.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            predicted.positionMeters
        );
    object.linearVelocityMps = predicted.velocityMps;
}
}

const char* clientNavigationPlanningSnapshotStatusName(
    ClientNavigationPlanningSnapshotStatus status
) noexcept
{
    switch (status)
    {
        case ClientNavigationPlanningSnapshotStatus::Ready:
            return "ready";
        case ClientNavigationPlanningSnapshotStatus::InvalidRequest:
            return "invalid_planning_request";
        case ClientNavigationPlanningSnapshotStatus::InvalidEpoch:
            return "invalid_planning_epoch";
        case ClientNavigationPlanningSnapshotStatus::RuntimeSampleUnavailable:
            return "authoritative_runtime_sample_unavailable";
        case ClientNavigationPlanningSnapshotStatus::ControlledShipNotFound:
            return "controlled_ship_not_found_at_source_epoch";
        case ClientNavigationPlanningSnapshotStatus::SystemMismatch:
            return "planning_system_mismatch";
        case ClientNavigationPlanningSnapshotStatus::TargetModuleNotFound:
            return "target_module_not_found_at_source_epoch";
        case ClientNavigationPlanningSnapshotStatus::HubNotFound:
            return "hub_not_found_at_source_epoch";
        case ClientNavigationPlanningSnapshotStatus::HubFrameInvalid:
            return "predicted_hub_frame_invalid";
        case ClientNavigationPlanningSnapshotStatus::ObjectPredictionFailed:
            return "hub_attached_object_prediction_failed";
        case ClientNavigationPlanningSnapshotStatus::TargetModulePredictionFailed:
            return "target_module_prediction_failed";
        case ClientNavigationPlanningSnapshotStatus::CoordinateRoundTripFailed:
            return "planning_frame_coordinate_roundtrip_failed";
    }
    return "unknown_planning_snapshot_status";
}

ClientNavigationPlanningSnapshot
ClientNavigationPlanningSnapshotFactory::buildPredictedHubSnapshot(
    const ClientWorldState& world,
    const game::network::SnapshotMetadata& sourceMetadata,
    double requestedPlanningServerTimeSeconds,
    double universeTimeScale,
    EntityId controlledShipId,
    int requestedSystemId,
    const std::string& targetModuleId
)
{
    ClientNavigationPlanningSnapshot out;
    out.systemId = requestedSystemId;

    out.sourceEpoch = makeEpoch(
        sourceMetadata,
        sourceMetadata.serverTimeSeconds,
        sourceMetadata.universeTimeSeconds
    );

    if (requestedSystemId < 0 || targetModuleId.empty())
    {
        out.status = ClientNavigationPlanningSnapshotStatus::InvalidRequest;
        return out;
    }
    if (!out.sourceEpoch.valid() ||
        !std::isfinite(requestedPlanningServerTimeSeconds) ||
        !std::isfinite(universeTimeScale))
    {
        out.status = ClientNavigationPlanningSnapshotStatus::InvalidEpoch;
        return out;
    }

    // Never predict backwards behind the authoritative seed. A lagging client
    // clock must collapse to the known source epoch rather than creating a
    // second, older navigation reality.
    const double planningServerTimeSeconds = std::max(
        sourceMetadata.serverTimeSeconds,
        requestedPlanningServerTimeSeconds
    );
    const double deltaGameplaySeconds =
        planningServerTimeSeconds - sourceMetadata.serverTimeSeconds;
    const double planningUniverseTimeSeconds =
        sourceMetadata.universeTimeSeconds +
        deltaGameplaySeconds * universeTimeScale;

    out.epoch = makeEpoch(
        sourceMetadata,
        planningServerTimeSeconds,
        planningUniverseTimeSeconds
    );
    if (!out.epoch.valid())
    {
        out.status = ClientNavigationPlanningSnapshotStatus::InvalidEpoch;
        return out;
    }

    // The seed itself is sampled at one exact canonical authoritative epoch.
    const auto runtime = world.sampleHubMapRuntimeAtServerTime(
        requestedSystemId,
        out.sourceEpoch.serverTimeSeconds
    );
    if (runtime.status != DetailMapRuntimeSampleStatus::Ready)
    {
        out.status =
            ClientNavigationPlanningSnapshotStatus::RuntimeSampleUnavailable;
        return out;
    }

    const auto shipIt = std::find_if(
        runtime.ships.begin(),
        runtime.ships.end(),
        [&](const DetailMapShipRuntimeSample& ship)
        {
            return ship.id == controlledShipId;
        }
    );
    if (shipIt == runtime.ships.end())
    {
        out.status =
            ClientNavigationPlanningSnapshotStatus::ControlledShipNotFound;
        return out;
    }
    if (shipIt->systemId != requestedSystemId)
    {
        out.status = ClientNavigationPlanningSnapshotStatus::SystemMismatch;
        return out;
    }

    const auto sourceTargetIt = std::find_if(
        runtime.objects.begin(),
        runtime.objects.end(),
        [&](const DetailMapObjectRuntimeSample& object)
        {
            return object.systemId == requestedSystemId &&
                object.hubAttachment.valid &&
                object.hubAttachment.moduleId == targetModuleId;
        }
    );
    if (sourceTargetIt == runtime.objects.end() ||
        sourceTargetIt->hubAttachment.hubId.empty())
    {
        out.status =
            ClientNavigationPlanningSnapshotStatus::TargetModuleNotFound;
        return out;
    }

    const std::string hubId = sourceTargetIt->hubAttachment.hubId;
    const auto sourceHubIt = std::find_if(
        runtime.hubs.begin(),
        runtime.hubs.end(),
        [&](const DetailMapHubRuntimeSample& hub)
        {
            return hub.systemId == requestedSystemId && hub.id == hubId;
        }
    );
    if (sourceHubIt == runtime.hubs.end())
    {
        out.status = ClientNavigationPlanningSnapshotStatus::HubNotFound;
        return out;
    }

    game::navigation::HubPredictionSource hubSource;
    hubSource.systemId = requestedSystemId;
    hubSource.hubId = sourceHubIt->id;
    hubSource.sourceUniverseTimeSeconds =
        out.sourceEpoch.universeTimeSeconds;
    hubSource.positionMeters =
        world::coordinates::fullMeters(sourceHubIt->worldPosition);
    hubSource.velocityMps = sourceHubIt->worldVelocityMps;
    hubSource.angularVelocityWorldRadPerSecond =
        sourceHubIt->angularVelocityWorldRadPerSecond;
    hubSource.orientation = sourceHubIt->orientation;
    hubSource.orbitalMotion = sourceHubIt->motion;

    out.planningFrame =
        game::navigation::NavigationWorldPredictor::predictHubFrameAt(
            hubSource,
            out.epoch.universeTimeSeconds
        );
    if (!out.planningFrame.valid)
    {
        out.status = ClientNavigationPlanningSnapshotStatus::HubFrameInvalid;
        return out;
    }

    out.hub = *sourceHubIt;
    out.hub.worldPosition = world::coordinates::makeWorldPositionFromMeters(
        out.planningFrame.originMeters
    );
    out.hub.worldVelocityMps = out.planningFrame.linearVelocityMps;
    out.hub.angularVelocityWorldRadPerSecond =
        out.planningFrame.angularVelocityWorldRadPerSecond;
    out.hub.orientation = game::navigation::hubVisualOrientation(
        glm::dvec3(out.planningFrame.localToWorldBasis[0]),
        glm::dvec3(out.planningFrame.localToWorldBasis[1]),
        glm::dvec3(out.planningFrame.localToWorldBasis[2])
    );

    // Resolve Hub-attached infrastructure from the predicted Hub frame. Do not
    // independently extrapolate module world positions; their attachment is
    // the canonical identity and guarantees that Hub/module/anchor cannot
    // drift into different epochs.
    out.objects = runtime.objects;
    for (auto& object : out.objects)
    {
        if (object.systemId != requestedSystemId)
            continue;

        if (object.hubAttachment.valid &&
            object.hubAttachment.hubId == hubId)
        {
            const auto resolved =
                game::navigation::NavigationWorldPredictor::
                    resolveHubAttachmentAt(
                        out.planningFrame,
                        out.epoch.universeTimeSeconds,
                        object.hubAttachment.localOffsetMeters,
                        object.hubAttachment.localRotationDeg,
                        object.hubAttachment.localAngularVelocityDegPerSecond
                    );
            if (!resolved.valid)
            {
                out.status =
                    ClientNavigationPlanningSnapshotStatus::ObjectPredictionFailed;
                return out;
            }

            object.worldPosition =
                world::coordinates::makeWorldPositionFromMeters(
                    resolved.positionMeters
                );
            object.linearVelocityMps = resolved.velocityMps;
            object.orientation = resolved.orientation;
        }
        else
        {
            predictUnattachedObject(object, deltaGameplaySeconds);
        }
    }

    const auto targetIt = std::find_if(
        out.objects.begin(),
        out.objects.end(),
        [&](const DetailMapObjectRuntimeSample& object)
        {
            return object.id == sourceTargetIt->id;
        }
    );
    if (targetIt == out.objects.end())
    {
        out.status =
            ClientNavigationPlanningSnapshotStatus::TargetModuleNotFound;
        return out;
    }
    out.targetObject = *targetIt;
    out.targetModuleKinematics =
        game::navigation::NavigationWorldPredictor::resolveHubAttachmentAt(
            out.planningFrame,
            out.epoch.universeTimeSeconds,
            out.targetObject.hubAttachment.localOffsetMeters,
            out.targetObject.hubAttachment.localRotationDeg,
            out.targetObject.hubAttachment.localAngularVelocityDegPerSecond
        );
    if (!out.targetModuleKinematics.valid)
    {
        out.status =
            ClientNavigationPlanningSnapshotStatus::TargetModulePredictionFailed;
        return out;
    }

    // Build route geometry from the same predicted object poses. Geometry
    // construction is shared with future server-side planning; SpaceState must
    // never invent per-object collision radii.
    out.navigationObstacles.clear();
    for (const auto& object : out.objects)
    {
        if (object.systemId != requestedSystemId)
            continue;

        const std::string obstacleId =
            "object:" + std::to_string(object.id.value);
        const auto obstacle =
            world::navigation::makeNavigationObstacleForObject(
                object.type,
                obstacleId,
                object.id.value,
                world::coordinates::fullMeters(object.worldPosition),
                glm::dmat3(glm::mat3(object.orientation)),
                DiagnosticHubInfrastructureClearanceMeters
            );
        if (!obstacle)
            continue;

        auto localObstacle = *obstacle;
        localObstacle.centerMeters =
            out.planningFrame.worldToLocalPosition(localObstacle.centerMeters);
        localObstacle.localToWorldBasis =
            glm::transpose(out.planningFrame.localToWorldBasis) *
            localObstacle.localToWorldBasis;
        out.navigationObstacles.push_back(std::move(localObstacle));

        if (object.id == out.targetObject.id)
            out.targetNavigationObstacleId = obstacleId;
    }

    out.controlledShip = *shipIt;
    const bool shipUsesPlanningHubFrame =
        out.controlledShip.motionMode ==
            game::navigation::MotionMode::HubTactical &&
        (out.controlledShip.hubId == hubId ||
         out.controlledShip.travelFrame.frameId == hubId);

    if (shipUsesPlanningHubFrame)
    {
        const auto predicted =
            game::navigation::NavigationWorldPredictor::
                predictHubLocalConstantVelocity(
                    out.planningFrame,
                    out.controlledShip.localPositionMeters,
                    out.controlledShip.localVelocityMps,
                    deltaGameplaySeconds
                );
        out.controlledShip.localPositionMeters +=
            out.controlledShip.localVelocityMps * deltaGameplaySeconds;
        out.controlledShip.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                predicted.positionMeters
            );
        out.controlledShip.worldVelocityMps = predicted.velocityMps;
        out.controlledShip.travelFrame = out.planningFrame;
    }
    else
    {
        const game::navigation::WorldKinematicState source {
            world::coordinates::fullMeters(out.controlledShip.worldPosition),
            out.controlledShip.worldVelocityMps,
            glm::dvec3(0.0)
        };
        const auto predicted =
            game::navigation::NavigationWorldPredictor::predictConstantVelocity(
                source,
                deltaGameplaySeconds
            );
        out.controlledShip.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                predicted.positionMeters
            );
        out.controlledShip.worldVelocityMps = predicted.velocityMps;
    }

    // Fail fast if the final planning frame no longer forms a reversible
    // coordinate contract at the predicted epoch.
    const glm::dvec3 shipWorld =
        world::coordinates::fullMeters(out.controlledShip.worldPosition);
    const glm::dvec3 shipLocal =
        out.planningFrame.worldToLocalPosition(shipWorld);
    const glm::dvec3 restoredShipWorld =
        out.planningFrame.localToWorldPosition(shipLocal);
    const double roundTripError = glm::length(restoredShipWorld - shipWorld);
    if (!finiteVec(shipLocal) ||
        !std::isfinite(roundTripError) ||
        roundTripError > CoordinateRoundTripToleranceMeters)
    {
        out.status =
            ClientNavigationPlanningSnapshotStatus::CoordinateRoundTripFailed;
        return out;
    }

    out.status = ClientNavigationPlanningSnapshotStatus::Ready;
    return out;
}

} // namespace game::client
