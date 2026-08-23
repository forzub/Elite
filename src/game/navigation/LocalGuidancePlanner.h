#pragma once

#include <cstdint>
#include <string>

#include "src/game/navigation/GuidanceCorridor.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/NavigationPlanningSnapshot.h"
#include "src/game/navigation/TrajectoryPredictor.h"
#include "src/game/navigation/TrajectorySafetyEvaluator.h"
#include "src/game/navigation/VehicleGuidanceEnvelope.h"

namespace game::navigation
{

enum class LocalGuidanceStatus : std::uint8_t
{
    Ready = 0,
    EmergencyEscapeReady,
    Blocked,
    InvalidRequest,
    PredictionFailure
};

struct LocalGuidanceProfile
{
    GuidancePurpose purpose = GuidancePurpose::Approach;

    // Local planner horizon is intentionally short. A long-range RouteSolver
    // hands off to this layer when the destination becomes a local operation.
    double horizonSeconds = 12.0;
    double frameIntervalSeconds = 0.5;
    double predictorIntegrationStepSeconds = 0.05;

    // Visual/safety corridor dimensions. Zero width/height means derive a
    // useful opening from the target semantic anchor plus clearance.
    double corridorWidthMeters = 0.0;
    double corridorHeightMeters = 0.0;

    // Canonical body dimensions for terminal docking pose and conservative
    // swept-volume safety. shipSafetyRadiusMeters remains a compatibility
    // fallback when the caller has no physical vehicle envelope yet.
    VehicleGuidanceEnvelope vehicleEnvelope;
    double shipSafetyRadiusMeters = 0.0;

    // Zero means derive from vehicle length + dock clearance.  Standoff is
    // outside the entrance plane; terminal depth is inside the dock.
    double dockingApproachStandoffMeters = 0.0;
    double dockingTerminalDepthMeters = 0.0;
    double emergencyEscapeDistanceMeters = 0.0;

    double recommendedSpeedMps = 0.0;
    double maxClosureRateMps = 0.0;

    TrajectoryMotionEnvelope motionEnvelope;
};

struct LocalGuidanceRequest
{
    std::string corridorId;
    int systemId = -1;
    double startUniverseTimeSeconds = 0.0;

    WorldKinematicState actorState;
    glm::dvec3 actorProperAccelerationMps2 {0.0};
    glm::dquat actorOrientation {1.0, 0.0, 0.0, 0.0};
    glm::dvec3 actorAngularVelocityWorldRadPerSecond {0.0};

    ResolvedHubSemanticAnchor target;
    NavigationPlanningSnapshot environment;
    LocalGuidanceProfile profile;
};

struct LocalGuidanceResult
{
    LocalGuidanceStatus status = LocalGuidanceStatus::InvalidRequest;
    std::string message;

    GuidanceCorridor corridor;
    TrajectoryPredictionResult prediction;
    TrajectorySafetyReport safety;
    bool detourUsed = false;
    bool emergencyEscapeUsed = false;

    bool ready() const noexcept
    {
        return status == LocalGuidanceStatus::Ready;
    }

    bool hasGuidance() const noexcept
    {
        return status == LocalGuidanceStatus::Ready ||
               status == LocalGuidanceStatus::EmergencyEscapeReady;
    }
};

/*
    Short-range guidance producer for docking/approach/transit operations.

    Docking mode is 6-DOF advisory guidance: it plans an approach point outside
    the moving entrance plane, then an ingress leg whose terminal velocity is
    perpendicular to that plane. Corridor frames carry the required hull pose
    and converge on dock up/down orientation. The planner never moves the ship.

    Every translational candidate is predicted by TrajectoryPredictor and
    validated by TrajectorySafetyEvaluator. If docking cannot be made safe, the
    planner tries a separate EmergencyEscape corridor while leaving the primary
    docking intent intact for rolling replanning.
*/
class LocalGuidancePlanner
{
public:
    static LocalGuidanceResult plan(const LocalGuidanceRequest& request);
};

} // namespace game::navigation
