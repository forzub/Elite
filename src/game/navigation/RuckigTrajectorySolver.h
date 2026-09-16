#pragma once

#include <cstddef>

#include "src/game/navigation/TrajectoryPredictor.h"

namespace game::navigation
{

/*
    State-to-state local trajectory request for the optional Ruckig backend.

    The public seam deliberately exposes only Elite navigation types. Ruckig
    headers stay private to the implementation translation unit so the rest of
    the game remains C++17 even though Ruckig v0.19.4 requires C++20.
*/
struct RuckigTrajectoryRequest
{
    int systemId = -1;
    double startUniverseTimeSeconds = 0.0;

    WorldKinematicState initialState;
    glm::dvec3 initialProperAccelerationMps2 {0.0};

    std::vector<GravityBody> gravityBodies;
    TrajectoryMotionEnvelope motionEnvelope;

    double horizonSeconds = 0.0;
    double sampleIntervalSeconds = 0.5;
    double validationStepSeconds = 0.05;

    glm::dvec3 targetPositionMeters {0.0};
    glm::dvec3 targetVelocityMps {0.0};
};

struct RuckigTrajectoryDiagnostics
{
    double ruckigDurationSeconds = 0.0;
    std::size_t validationSteps = 0;

    bool accelerationLimitExceeded = false;
    bool jerkLimitExceeded = false;
};

struct RuckigTrajectoryResult
{
    TrajectoryPredictionResult prediction;
    RuckigTrajectoryDiagnostics solverDiagnostics;

    bool ok() const noexcept
    {
        return prediction.ok();
    }
};

/*
    Thin offline Ruckig adapter used for local state-to-state manoeuvres.

    It solves in an accelerating co-moving frame to avoid orbital-scale world
    coordinates and to absorb the dominant local gravity term. The generated
    world-space trajectory is then checked against Elite's scalar proper-
    acceleration and proper-jerk envelope at a fine validation cadence.

    This class does not move the ship and does not replace safety evaluation.
*/
class RuckigTrajectorySolver
{
public:
    static RuckigTrajectoryResult solve(
        const RuckigTrajectoryRequest& request
    );
};

} // namespace game::navigation
