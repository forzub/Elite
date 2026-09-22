#pragma once

#include <algorithm>

#include "src/game/navigation/VehicleGuidanceEnvelope.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/world/navigation/NavigationAgentProfile.h"
#include "src/world/navigation/NavigationVehicleProfile.h"

namespace game::navigation
{

inline world::navigation::NavigationVehicleProfile
makeNavigationVehicleProfile(
    const ShipParams& params,
    double collisionRadiusMeters,
    double preferredClearanceMeters
)
{
    world::navigation::NavigationVehicleProfile profile;
    profile.collisionRadiusMeters =
        std::max(0.0, collisionRadiusMeters);
    profile.preferredClearanceMeters =
        std::max(0.0, preferredClearanceMeters);
    profile.maxSpeedMps =
        game::ship::controlledSpeedLimitMps(params);

    const double forwardMain =
        game::ship::forwardMainAccelerationLimitMps2(params);
    const double reverseMain =
        game::ship::reverseMainAccelerationLimitMps2(params);
    const double manoeuvre =
        game::ship::manoeuvreAccelerationLimitMps2(params);

    profile.maxForwardAccelerationMps2 =
        std::max(forwardMain, manoeuvre);
    profile.maxBrakingAccelerationMps2 =
        std::max(reverseMain, manoeuvre);
    profile.maxLateralAccelerationMps2 =
        manoeuvre;
    profile.maxAngularVelocityRadPerSecond =
        game::ship::maximumAngularSpeedRadPerSec(params);
    profile.maxAngularAccelerationRadPerSecond2 =
        game::ship::angularAccelerationLimitRadPerSec2(params);
    return profile;
}

inline world::navigation::NavigationVehicleProfile
makeNavigationVehicleProfile(
    const ShipParams& params,
    const VehicleGuidanceEnvelope& envelope
)
{
    return makeNavigationVehicleProfile(
        params,
        envelope.valid
            ? envelope.conservativeSafetyRadiusMeters()
            : 0.0,
        0.0
    );
}

inline world::navigation::NavigationVehicleProfile
makeNavigationVehicleProfile(
    const world::navigation::NavigationAgentProfile& agent
)
{
    world::navigation::NavigationVehicleProfile profile;
    profile.collisionRadiusMeters = std::max(
        0.0,
        static_cast<double>(agent.bodyRadius)
    );
    profile.maxSpeedMps = std::max(
        0.0,
        static_cast<double>(agent.maxSpeed)
    );
    const double acceleration = std::max(
        0.0,
        static_cast<double>(agent.maxAcceleration)
    );
    profile.maxForwardAccelerationMps2 = acceleration;
    profile.maxBrakingAccelerationMps2 = acceleration;
    profile.maxLateralAccelerationMps2 = acceleration;
    return profile;
}

} // namespace game::navigation
