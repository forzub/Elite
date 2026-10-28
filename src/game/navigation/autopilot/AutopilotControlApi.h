#pragma once

#include <array>
#include <cstddef>

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::autopilot
{

inline constexpr std::size_t AutopilotPreviewCapacity = 8;

struct AutopilotReferencePreviewSample
{
    // Relative to the current control instant.
    double timeOffsetSeconds = 0.0;
    glm::dvec3 desiredPositionMapMeters {0.0};
    glm::dvec3 desiredVelocityMapMps {0.0};
    glm::dvec3 desiredLinearAccelerationMapMps2 {0.0};
};

// Hard boundary between route guidance and a flight-law-specific virtual pilot.
//
// The route layer may provide only desired/observed vehicle state.  It must not
// pass engine ids, propulsion topology, throttle-bank state, RCS selection, or
// any implementation-owned controller state through this structure.
struct AutopilotControlRequest
{
    glm::dvec3 desiredPositionMapMeters {0.0};
    glm::dvec3 desiredVelocityMapMps {0.0};
    glm::dvec3 desiredLinearAccelerationMapMps2 {0.0};
    glm::dvec3 actualPositionMapMeters {0.0};
    bool positionReferenceValid = false;
    // Authored corridor tracking tolerance around the position reference
    // (0 = unknown). A pilot may treat the reference as the corridor centre
    // rather than a rail.
    double corridorToleranceMeters = 0.0;

    // Law-neutral future reference preview. Assisted is free to ignore it.
    // Newtonian optimal control uses it to price future braking/turning before
    // the current station reaches that constraint.
    std::array<
        AutopilotReferencePreviewSample,
        AutopilotPreviewCapacity
    > preview {};
    std::size_t previewCount = 0;

    // Angular rate of the desired NAVIGATION course. A concrete pilot may use
    // it only when that is meaningful for its actuator doctrine.
    glm::dvec3 desiredCourseAngularRateMapRadPerSec {0.0};
    double desiredRollRateRadPerSec = 0.0;

    // Terminal hull pose.  While moving, the concrete pilot remains free to
    // orient the hull as required by its propulsion doctrine.
    glm::dvec3 desiredForwardMap {0.0, 0.0, -1.0};
    glm::dvec3 desiredUpMap {0.0, 1.0, 0.0};

    glm::dvec3 actualVelocityMapMps {0.0};
    glm::dvec3 forwardMap {0.0, 0.0, -1.0};
    glm::dvec3 rightMap {1.0, 0.0, 0.0};
    glm::dvec3 upMap {0.0, 1.0, 0.0};

    double pitchRateRadPerSec = 0.0;
    double yawRateRadPerSec = 0.0;
    double rollRateRadPerSec = 0.0;

    bool stopRequested = false;
    bool terminalAttitudeHold = false;
    double deltaSeconds = 0.0;

    // Terminal pose of a route that ends in a stop, with the route's finite
    // acceptance tolerances. "Inside the tolerance" is the goal, never a
    // mathematical zero. This is a navigation terminal state (staging,
    // waiting, rendezvous point), not a docking/capture state.
    bool terminalTargetValid = false;
    glm::dvec3 terminalPositionMapMeters {0.0};
    glm::dvec3 terminalForwardMap {0.0, 0.0, -1.0};
    glm::dvec3 terminalUpMap {0.0, 1.0, 0.0};
    double terminalPositionToleranceMeters = 0.0;
    double terminalSpeedToleranceMps = 0.0;
    double terminalAttitudeToleranceRad = 0.0;
    double terminalRollToleranceRad = 0.0;
    double terminalAngularRateToleranceRadPerSec = 0.0;

    // Progress of the position reference along the route stations handed to
    // a pilot that compiles its own manoeuvre program (see the route hook in
    // DedicatedClientRouteAutopilot). Other pilots ignore it.
    double programProgressMeters = 0.0;
    bool programProgressValid = false;

    // DEV diagnostic context for decision traces only; never a control input.
    double traceRouteProgressMeters = 0.0;
    std::size_t traceCurveIndex = 0;
    double traceCurvaturePerMeter = 0.0;
    double traceProfileSpeedMps = 0.0;
};

// Read-only, law-neutral feedback used by route-speed preview and diagnostics.
// The route layer is forbidden to reach into concrete pilot state.
struct AutopilotControlTelemetry
{
    bool initialized = false;
    glm::dvec3 previousVelocityMapMps {0.0};

    // Measured/estimated closed-loop values.  Names are deliberately neutral:
    // both Assisted and Newtonian pilots must be replaceable behind this API.
    double courseResponseSeconds = 2.0;
    double targetSpeedChangeRateMps2 = 0.0;
    double brakingResponseMps2 = 0.0;
    double commandResponsePerSecond = 0.0;
    // Coast time the pilot needs BEFORE a braking burn whose thrust axis is
    // opposite the current one (hull flip + settle + margin). 0 = none.
    double thrustReversalLeadSeconds = 0.0;
};

using AutopilotControlCommand = ShipControlState;

} // namespace game::navigation::autopilot
