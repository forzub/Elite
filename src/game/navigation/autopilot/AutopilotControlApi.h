#pragma once

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::autopilot
{

// Hard boundary between route guidance and a flight-law-specific virtual pilot.
//
// The route layer may provide only desired/observed vehicle state.  It must not
// pass engine ids, propulsion topology, throttle-bank state, RCS selection, or
// any implementation-owned controller state through this structure.
struct AutopilotControlRequest
{
    glm::dvec3 desiredVelocityMapMps {0.0};
    glm::dvec3 desiredLinearAccelerationMapMps2 {0.0};

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
};

using AutopilotControlCommand = ShipControlState;

} // namespace game::navigation::autopilot
