#pragma once

#include <cstdint>
#include <string>

#include "src/game/navigation/GuidanceCorridor.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/NavigationPlanningSnapshot.h"
#include "src/game/navigation/TrajectoryPredictor.h"
#include "src/game/navigation/TrajectorySafetyEvaluator.h"

namespace game::navigation
{

enum class LocalGuidanceStatus : std::uint8_t
{
    Ready = 0,
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
    double shipSafetyRadiusMeters = 0.0;

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

    bool ready() const noexcept
    {
        return status == LocalGuidanceStatus::Ready;
    }
};

/*
    Short-range guidance producer for docking/approach/transit operations.

    V1 first plans one direct time-parameterized candidate, predicts it through
    the shared TrajectoryPredictor and validates it through the shared 4D safety
    evaluator. If a known spherical hazard blocks that candidate, it tries a
    small pair of lateral detour candidates through the same predictor+safety
    pipeline. More sophisticated graph/continuous optimization remains a later
    planner strategy and is never hidden inside TrajectoryPredictor.
*/
class LocalGuidancePlanner
{
public:
    static LocalGuidanceResult plan(const LocalGuidanceRequest& request);
};

} // namespace game::navigation
