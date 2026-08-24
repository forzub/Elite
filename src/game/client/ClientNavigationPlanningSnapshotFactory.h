#pragma once

#include <string>
#include <vector>

#include "src/game/client/ClientDetailMapRuntimeSampler.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/NavigationPlanningEpoch.h"
#include "src/game/navigation/NavigationWorldPredictor.h"
#include "src/game/network/ProtocolMetadata.h"
#include "src/scene/EntityID.h"

class ClientWorldState;

namespace game::client
{

enum class ClientNavigationPlanningSnapshotStatus
{
    Ready,
    InvalidRequest,
    InvalidEpoch,
    RuntimeSampleUnavailable,
    ControlledShipNotFound,
    SystemMismatch,
    TargetModuleNotFound,
    HubNotFound,
    HubFrameInvalid,
    ObjectPredictionFailed,
    TargetModulePredictionFailed,
    CoordinateRoundTripFailed
};

const char* clientNavigationPlanningSnapshotStatusName(
    ClientNavigationPlanningSnapshotStatus status
) noexcept;

/*
    Client-side immutable planning picture for one Hub problem.

    sourceEpoch identifies the exact canonical authoritative snapshot used as
    the seed. epoch identifies the single prediction/planning time to which the
    complete problem has been resolved. Presentation interpolation is never a
    legal source for either epoch.
*/
struct ClientNavigationPlanningSnapshot
{
    ClientNavigationPlanningSnapshotStatus status =
        ClientNavigationPlanningSnapshotStatus::RuntimeSampleUnavailable;

    game::navigation::NavigationPlanningEpoch sourceEpoch;
    game::navigation::NavigationPlanningEpoch epoch;
    int systemId = -1;

    DetailMapShipRuntimeSample controlledShip;
    DetailMapObjectRuntimeSample targetObject;
    DetailMapHubRuntimeSample hub;
    game::navigation::KinematicFrame planningFrame;
    game::navigation::HubAttachedKinematicState targetModuleKinematics;

    std::vector<DetailMapObjectRuntimeSample> objects;

    bool ready() const noexcept
    {
        return status == ClientNavigationPlanningSnapshotStatus::Ready &&
            sourceEpoch.valid() && epoch.valid() && planningFrame.valid &&
            targetModuleKinematics.valid;
    }
};

class ClientNavigationPlanningSnapshotFactory
{
public:
    /*
        Resolve one authoritative snapshot forward to the requested current
        server time. Hub/orbit motion uses universe time; normal ship/object
        translation uses gameplay/server time. Every output is stamped with the
        same planning epoch.
    */
    static ClientNavigationPlanningSnapshot buildPredictedHubSnapshot(
        const ClientWorldState& world,
        const game::network::SnapshotMetadata& sourceMetadata,
        double requestedPlanningServerTimeSeconds,
        double universeTimeScale,
        EntityId controlledShipId,
        int requestedSystemId,
        const std::string& targetModuleId
    );

    // Deterministic validation baseline: planning time equals source time.
    static ClientNavigationPlanningSnapshot buildAuthoritativeHubSnapshot(
        const ClientWorldState& world,
        const game::network::SnapshotMetadata& sourceMetadata,
        EntityId controlledShipId,
        int requestedSystemId,
        const std::string& targetModuleId
    )
    {
        return buildPredictedHubSnapshot(
            world,
            sourceMetadata,
            sourceMetadata.serverTimeSeconds,
            1.0,
            controlledShipId,
            requestedSystemId,
            targetModuleId
        );
    }
};

} // namespace game::client
