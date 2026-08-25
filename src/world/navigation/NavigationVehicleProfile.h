#pragma once

#include <algorithm>
#include <cmath>

namespace world::navigation
{

/*
    Canonical motion envelope consumed by shared path/trajectory code.

    This is intentionally independent from ShipParams, repair-drone runtime,
    client state and control laws. Adapters translate a concrete vehicle into
    this common contract; the same trajectory backend can therefore execute on
    a lone client's machine or on the server for a shared consistency domain.
*/
struct NavigationVehicleProfile
{
    double collisionRadiusMeters = 0.0;
    double preferredClearanceMeters = 0.0;

    double maxSpeedMps = 0.0;
    double maxForwardAccelerationMps2 = 0.0;
    double maxBrakingAccelerationMps2 = 0.0;
    double maxLateralAccelerationMps2 = 0.0;

    // Stage 5A records these for orientation diagnostics and the future
    // follower. Translational parameterization does not yet synthesize a full
    // attitude-control program.
    double maxAngularVelocityRadPerSecond = 0.0;
    double maxAngularAccelerationRadPerSecond2 = 0.0;

    bool finite() const noexcept
    {
        return
            std::isfinite(collisionRadiusMeters) &&
            std::isfinite(preferredClearanceMeters) &&
            std::isfinite(maxSpeedMps) &&
            std::isfinite(maxForwardAccelerationMps2) &&
            std::isfinite(maxBrakingAccelerationMps2) &&
            std::isfinite(maxLateralAccelerationMps2) &&
            std::isfinite(maxAngularVelocityRadPerSecond) &&
            std::isfinite(maxAngularAccelerationRadPerSecond2);
    }

    bool valid() const noexcept
    {
        return finite() &&
            collisionRadiusMeters >= 0.0 &&
            preferredClearanceMeters >= 0.0 &&
            maxSpeedMps > 0.0 &&
            maxForwardAccelerationMps2 > 0.0 &&
            maxBrakingAccelerationMps2 > 0.0 &&
            maxLateralAccelerationMps2 > 0.0;
    }
};

} // namespace world::navigation
