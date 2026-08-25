#pragma once

#include <algorithm>

#include "src/game/navigation/VehicleGuidanceEnvelope.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/navigation/NavigationAgentProfile.h"
#include "src/world/navigation/NavigationVehicleProfile.h"

namespace game::navigation
{

inline constexpr double NavigationStandardGravityMps2 = 9.80665;

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

    const double linearGs = params.maxLinearGs > 0.0f
        ? static_cast<double>(params.maxLinearGs)
        : static_cast<double>(params.maxGs);
    const double mainAcceleration = std::max(
        0.0,
        linearGs * NavigationStandardGravityMps2
    );
    const double lateralAcceleration = std::max(
        0.0,
        std::min(
            mainAcceleration,
            static_cast<double>(params.strafeAccel)
        )
    );

    profile.maxForwardAccelerationMps2 = mainAcceleration;
    profile.maxBrakingAccelerationMps2 = mainAcceleration;
    profile.maxLateralAccelerationMps2 = lateralAcceleration > 0.0
        ? lateralAcceleration
        : mainAcceleration;

    profile.maxAngularVelocityRadPerSecond = std::max({
        0.0,
        static_cast<double>(params.maxPitchRate),
        static_cast<double>(params.maxYawRate),
        static_cast<double>(params.maxRollRate)
    });
    profile.maxAngularAccelerationRadPerSecond2 = std::max(
        0.0,
        static_cast<double>(params.angularAccel)
    );
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
