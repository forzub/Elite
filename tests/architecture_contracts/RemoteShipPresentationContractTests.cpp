#include <cmath>
#include <cstdlib>
#include <iostream>

#include "src/game/client/ClientRemoteShipPresentation.h"

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        std::exit(1);
    }
}

bool near(double a, double b, double epsilon = 1.0e-9)
{
    return std::abs(a - b) <= epsilon;
}

void testPresentationPredictionDoesNotMutateTruth()
{
    ShipSnapshot authoritative;
    authoritative.id = EntityId{42};
    authoritative.motionFidelity =
        game::simulation::ShipMotionFidelity::RouteOnly;
    authoritative.transform.setWorldPositionMeters({100.0, 200.0, 300.0});
    authoritative.transform.motion.worldVelocityMps = {10.0, -2.0, 4.0};
    authoritative.transform.motion.localPositionMeters = {5.0, 6.0, 7.0};
    authoritative.transform.motion.localVelocityMps = {1.0, 2.0, 3.0};

    const auto originalWorldMeters =
        world::coordinates::fullMeters(
            authoritative.transform.worldPosition
        );
    const auto originalLocal =
        authoritative.transform.motion.localPositionMeters;

    const auto presentation =
        game::client::deadReckonRemoteShipPresentation(
            authoritative,
            10.0,
            10.25,
            0.75
        );

    const auto worldMeters =
        world::coordinates::fullMeters(presentation.worldPosition);

    require(near(presentation.deltaSeconds, 0.25), "prediction delta changed");
    require(near(worldMeters.x, 102.5), "world X prediction mismatch");
    require(near(worldMeters.y, 199.5), "world Y prediction mismatch");
    require(near(worldMeters.z, 301.0), "world Z prediction mismatch");
    require(
        near(presentation.localPositionMeters.x, 5.25) &&
        near(presentation.localPositionMeters.y, 6.5) &&
        near(presentation.localPositionMeters.z, 7.75),
        "local presentation prediction mismatch"
    );
    require(
        glm::length(
            world::coordinates::fullMeters(
                authoritative.transform.worldPosition
            ) - originalWorldMeters
        ) < 1.0e-12 &&
        glm::length(
            authoritative.transform.motion.localPositionMeters -
                originalLocal
        ) < 1.0e-12,
        "presentation dead reckoning mutated authoritative snapshot"
    );
}

void testPredictionWindowIsBounded()
{
    ShipSnapshot authoritative;
    authoritative.transform.setWorldPositionMeters({0.0, 0.0, 0.0});
    authoritative.transform.motion.worldVelocityMps = {100.0, 0.0, 0.0};

    const auto presentation =
        game::client::deadReckonRemoteShipPresentation(
            authoritative,
            2.0,
            20.0,
            0.75
        );

    require(
        near(presentation.deltaSeconds, 0.75),
        "remote presentation prediction exceeded correction window"
    );
    require(
        near(
            world::coordinates::fullMeters(
                presentation.worldPosition
            ).x,
            75.0
        ),
        "bounded prediction used wrong world delta"
    );
}

void testFutureOrInvalidTimeCannotRunBackward()
{
    ShipSnapshot authoritative;
    authoritative.transform.setWorldPositionMeters({10.0, 0.0, 0.0});
    authoritative.transform.motion.worldVelocityMps = {100.0, 0.0, 0.0};

    const auto presentation =
        game::client::deadReckonRemoteShipPresentation(
            authoritative,
            10.0,
            9.0,
            0.75
        );

    require(
        near(presentation.deltaSeconds, 0.0),
        "presentation extrapolated backward in time"
    );
    require(
        near(
            world::coordinates::fullMeters(
                presentation.worldPosition
            ).x,
            10.0
        ),
        "backward-time presentation moved authoritative position"
    );
}

} // namespace

int main()
{
    testPresentationPredictionDoesNotMutateTruth();
    testPredictionWindowIsBounded();
    testFutureOrInvalidTimeCannotRunBackward();

    std::cout
        << "[PASS] remote low-fidelity ships dead-reckon on client presentation only\n";
    return 0;
}
