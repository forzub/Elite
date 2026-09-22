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
    const VehicleGuidanceEnvelope& envelope
)
{
    world::navigation::NavigationVehicleProfile profile;
    profile.collisionRadiusMeters = envelope.valid
        ? envelope.conservativeSafetyRadiusMeters()
        : 0.0;
    profile.maxSpeedMps = std::max(
        0.0,
        static_cast<double>(params.maxCombatSpeed)
    );

    const double forwardMain =
        game::ship::forwardMainAccelerationLimitMps2(params);
    const double reverseMain =
        game::ship::reverseMainAccelerationLimitMps2(params);
    const double manoeuvre =
        game::ship::manoeuvreAccelerationLimitMps2(params);

    profile.maxForwardAccelerationMps2 =
        std::max(forwardMain, manoeuvre);
    // This common trajectory projection may only claim acceleration that can
    // be produced WITHOUT first changing attitude. Flip-and-burn authority is
    // a maneuver-level fact and must be authored/proved by the physical
    // maneuver compiler, not smuggled into a scalar braking number.
    profile.maxBrakingAccelerationMps2 =
        std::max(reverseMain, manoeuvre);
    profile.maxLateralAccelerationMps2 = manoeuvre;

    profile.maxAngularVelocityRadPerSecond =
        game::ship::maximumAngularSpeedRadPerSec(params);
    profile.maxAngularAccelerationRadPerSecond2 =
        game::ship::angularAccelerationLimitRadPerSec2(params);
    return profile;
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
