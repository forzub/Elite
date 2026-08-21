#include <cmath>
#include <cstdlib>
#include <iostream>

#include "src/game/diagnostics/InterplanetaryTransferLab.h"

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

bool near(double a, double b, double eps)
{
    return std::abs(a - b) <= eps;
}

}

int main()
{
    using namespace game::diagnostics;

    const auto initial = evaluateInterplanetaryTransferLab(0.0);
    const auto later = evaluateInterplanetaryTransferLab(10.0 * InterplanetarySecondsPerDay);

    const double initialRadiusAu =
        initial.heliocentricRadiusMeters / InterplanetaryAuMeters;
    const double laterRadiusAu =
        later.heliocentricRadiusMeters / InterplanetaryAuMeters;
    const double initialSpeedKmS =
        initial.heliocentricSpeedMetersPerSecond / 1000.0;

    require(initialRadiusAu > 1.0 && initialRadiusAu < 1.52,
            "test ship must start between Earth and Mars orbital radii");
    require(laterRadiusAu < initialRadiusAu,
            "Mars->Earth transfer must move inward toward Earth's orbit");
    require(initialSpeedKmS > 20.0 && initialSpeedKmS < 35.0,
            "heliocentric transfer speed must remain modern interplanetary scale");
    require(initial.transferDurationSeconds /
                InterplanetarySecondsPerDay > 240.0,
            "transfer must not be an implausibly short radial sprint");
    require(initial.transferDurationSeconds /
                InterplanetarySecondsPerDay < 280.0,
            "transfer duration should remain Hohmann-like for Earth/Mars radii");

    const auto arrival = evaluateInterplanetaryTransferLab(400.0 * InterplanetarySecondsPerDay);
    require(arrival.arrived, "trajectory should clamp at Earth-side perihelion after arrival");
    require(near(arrival.heliocentricRadiusMeters / InterplanetaryAuMeters,
                 InterplanetaryEarthOrbitAu,
                 1.0e-9),
            "arrival radius must be Earth's orbital radius");

    std::cout << "[PASS] Mars->Earth interplanetary transfer diagnostic\n";
    return 0;
}
