#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/game/navigation/NavigationPlanningSnapshot.h"
#include "src/game/navigation/TrajectoryPredictor.h"

namespace game::navigation
{

enum class TrajectoryConflictKind : std::uint8_t
{
    Obstacle = 0,
    ScheduledTraffic,
    RestrictedVolume
};

struct TrajectoryConflict
{
    TrajectoryConflictKind kind = TrajectoryConflictKind::Obstacle;
    std::string objectId;

    double universeTimeSeconds = 0.0;
    double separationMeters = 0.0;
    double requiredSeparationMeters = 0.0;

    glm::dvec3 shipPositionMeters {0.0};
    glm::dvec3 hazardPositionMeters {0.0};
};

struct TrajectorySafetyReport
{
    bool safe = true;
    double minimumSeparationMeters = 0.0;
    std::vector<TrajectoryConflict> conflicts;
};

/*
    Time-aware collision/safety validation for a proposed trajectory.

    It does not choose an avoidance side. Route/local planners propose a path,
    TrajectoryPredictor says where that path goes, and this evaluator says
    whether the time-dependent safety envelopes are violated.
*/
class TrajectorySafetyEvaluator
{
public:
    static TrajectorySafetyReport evaluate(
        const TrajectoryPredictionResult& trajectory,
        const NavigationPlanningSnapshot& environment,
        double shipSafetyRadiusMeters
    );
};

} // namespace game::navigation
