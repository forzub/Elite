#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/KinematicFrame.h"

namespace game::navigation
{

inline constexpr double StandardGravityMps2 = 9.80665;

enum class TrajectoryPredictionStatus : std::uint8_t
{
    Ok = 0,
    InvalidRequest,
    NumericalFailure
};

/*
    A future propulsion/autopilot program expressed in system-local world axes.

    Keys are targets, not impulses. The predictor linearly interpolates between
    them. Before the first future key it interpolates from the request's
    initialProperAccelerationMps2; after the last key it holds the last target.

    "Proper" acceleration is deliberately separated from gravity. It is the
    non-gravitational acceleration produced by engines/thrusters and is the
    quantity that creates translational crew load in this predictor.
*/
struct TrajectoryAccelerationKey
{
    double timeOffsetSeconds = 0.0;
    glm::dvec3 properAccelerationMps2 {0.0};
};

/*
    Optional planning/prediction envelope.

    Values <= 0 disable that particular limit. The caller owns policy: a manual
    flight predictor may pass the normal crew envelope, while a route/autopilot
    solver may pass a higher but still safe automatic-flight envelope. The
    predictor does not hard-code a gameplay G multiplier.
*/
struct TrajectoryMotionEnvelope
{
    double maxProperAccelerationMps2 = 0.0;
    double maxProperJerkMps3 = 0.0;
};

struct TrajectoryPredictionRequest
{
    int systemId = -1;
    double startUniverseTimeSeconds = 0.0;

    WorldKinematicState initialState;

    // Applied non-gravitational acceleration at t=0. If no future program is
    // supplied, this acceleration is held for the whole prediction horizon.
    glm::dvec3 initialProperAccelerationMps2 {0.0};

    // Static gravity sources for this first predictor layer. Moving ephemeris
    // sources can be introduced later without changing the result contract.
    std::vector<GravityBody> gravityBodies;

    // Future proper-acceleration targets. Input order does not matter; the
    // predictor canonicalizes a private copy and never mutates the request.
    std::vector<TrajectoryAccelerationKey> accelerationProgram;

    TrajectoryMotionEnvelope motionEnvelope;

    double horizonSeconds = 0.0;
    double sampleIntervalSeconds = 1.0;
    double maxIntegrationStepSeconds = 0.25;
};

struct TrajectoryPredictionSample
{
    double universeTimeSeconds = 0.0;
    double timeOffsetSeconds = 0.0;

    WorldKinematicState state;

    glm::dvec3 properAccelerationMps2 {0.0};
    glm::dvec3 gravityAccelerationMps2 {0.0};

    // Translational crew load from non-gravitational acceleration only.
    double properLoadGs = 0.0;

    // Integral of |proper acceleration|. This is useful to the future solver
    // as a propulsion/cost primitive and is intentionally ignored by render.
    double cumulativeProperDeltaVMps = 0.0;
};

struct TrajectoryPredictionDiagnostics
{
    std::size_t integrationSteps = 0;

    double maxSpeedMps = 0.0;
    double maxRequestedProperAccelerationMps2 = 0.0;
    double maxAppliedProperAccelerationMps2 = 0.0;
    double maxProperLoadGs = 0.0;
    double maxAppliedProperJerkMps3 = 0.0;

    double totalProperDeltaVMps = 0.0;
    double travelledDistanceMeters = 0.0;

    bool accelerationClamped = false;
    bool jerkClamped = false;
};

struct TrajectoryPredictionResult
{
    TrajectoryPredictionStatus status = TrajectoryPredictionStatus::InvalidRequest;
    int systemId = -1;
    std::string message;
    std::vector<TrajectoryPredictionSample> samples;
    TrajectoryPredictionDiagnostics diagnostics;

    bool ok() const noexcept
    {
        return status == TrajectoryPredictionStatus::Ok;
    }
};

/*
    Shared translational trajectory predictor.

    It owns no ship, route, map, renderer or server state. The caller supplies a
    kinematic seed, gravity sources, acceleration program and motion envelope;
    the predictor returns a reusable time-series product. Route solving,
    obstacle avoidance and autopilot execution remain separate layers.
*/
class TrajectoryPredictor
{
public:
    static TrajectoryPredictionResult predict(
        const TrajectoryPredictionRequest& request
    );
};

} // namespace game::navigation
