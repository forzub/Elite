#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/TrajectoryPredictor.h"
#include "src/game/system_map/TrajectoryMapAdapter.h"

namespace
{
using namespace game::navigation;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double actual, double expected, double tolerance = 1.0e-7)
{
    return std::abs(actual - expected) <= tolerance;
}

bool nearVec(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double tolerance = 1.0e-7
)
{
    return glm::length(actual - expected) <= tolerance;
}

TrajectoryPredictionRequest basicRequest()
{
    TrajectoryPredictionRequest request;
    request.systemId = 4;
    request.startUniverseTimeSeconds = 1000.0;
    request.horizonSeconds = 10.0;
    request.sampleIntervalSeconds = 1.0;
    request.maxIntegrationStepSeconds = 0.1;
    return request;
}

void testInertialCoast()
{
    auto request = basicRequest();
    request.initialState.positionMeters = glm::dvec3(10.0, 20.0, 30.0);
    request.initialState.velocityMps = glm::dvec3(3.0, -2.0, 1.0);

    const auto result = TrajectoryPredictor::predict(request);
    require(result.ok(), "inertial coast prediction failed");
    require(result.systemId == 4, "system identity was not preserved");
    require(result.samples.size() == 11, "sample cadence is not exact");

    const auto& last = result.samples.back();
    require(near(last.universeTimeSeconds, 1010.0), "final universe time mismatch");
    require(
        nearVec(last.state.positionMeters, glm::dvec3(40.0, 0.0, 40.0), 1.0e-6),
        "inertial position propagation mismatch"
    );
    require(
        nearVec(last.state.velocityMps, request.initialState.velocityMps, 1.0e-9),
        "inertial velocity changed without force"
    );
    require(near(result.diagnostics.totalProperDeltaVMps, 0.0), "coast invented delta-v");
}

void testConstantProperAcceleration()
{
    auto request = basicRequest();
    request.horizonSeconds = 4.0;
    request.sampleIntervalSeconds = 0.5;
    request.initialState.velocityMps = glm::dvec3(10.0, 0.0, 0.0);
    request.initialProperAccelerationMps2 = glm::dvec3(2.0, 0.0, 0.0);

    const auto result = TrajectoryPredictor::predict(request);
    require(result.ok(), "constant-acceleration prediction failed");

    const auto& last = result.samples.back();
    require(
        nearVec(last.state.positionMeters, glm::dvec3(56.0, 0.0, 0.0), 1.0e-6),
        "constant-acceleration position mismatch"
    );
    require(
        nearVec(last.state.velocityMps, glm::dvec3(18.0, 0.0, 0.0), 1.0e-6),
        "constant-acceleration velocity mismatch"
    );
    require(near(last.cumulativeProperDeltaVMps, 8.0, 1.0e-6), "delta-v integral mismatch");
}

void testGravityDoesNotBecomeCrewLoad()
{
    auto request = basicRequest();
    request.horizonSeconds = 0.0;
    request.initialState.positionMeters = glm::dvec3(7.0e6, 0.0, 0.0);

    GravityBody earthLike;
    earthLike.id = "earth-like";
    earthLike.centerMeters = glm::dvec3(0.0);
    earthLike.radiusMeters = 6.371e6;
    earthLike.gravitationalParameterM3s2 = 3.986004418e14;
    earthLike.influenceRadiusMeters = 1.0e9;
    request.gravityBodies.push_back(earthLike);

    const auto result = TrajectoryPredictor::predict(request);
    require(result.ok(), "gravity seed prediction failed");
    require(result.samples.size() == 1, "zero horizon must return exactly the seed sample");

    const auto& sample = result.samples.front();
    require(glm::length(sample.gravityAccelerationMps2) > 8.0, "gravity field was not sampled");
    require(near(sample.properLoadGs, 0.0), "free-fall gravity was incorrectly counted as crew G-load");
    require(
        nearVec(sample.state.accelerationMps2, sample.gravityAccelerationMps2, 1.0e-9),
        "total acceleration must still include gravity"
    );
}

void testCallerSelectsManualOrAutopilotEnvelope()
{
    auto manual = basicRequest();
    manual.horizonSeconds = 0.0;
    manual.initialProperAccelerationMps2 =
        glm::dvec3(9.0 * StandardGravityMps2, 0.0, 0.0);
    manual.motionEnvelope.maxProperAccelerationMps2 =
        5.0 * StandardGravityMps2;

    auto automatic = manual;
    automatic.motionEnvelope.maxProperAccelerationMps2 =
        8.0 * StandardGravityMps2;

    const auto manualResult = TrajectoryPredictor::predict(manual);
    const auto autoResult = TrajectoryPredictor::predict(automatic);

    require(manualResult.ok() && autoResult.ok(), "load-envelope prediction failed");
    require(near(manualResult.samples.front().properLoadGs, 5.0, 1.0e-9), "manual G envelope was not enforced");
    require(near(autoResult.samples.front().properLoadGs, 8.0, 1.0e-9), "automatic G envelope was not independently selectable");
    require(manualResult.diagnostics.accelerationClamped, "manual clamp was not diagnosed");
    require(autoResult.diagnostics.accelerationClamped, "automatic clamp was not diagnosed");
}

void testJerkEnvelopeSmoothsAccelerationChanges()
{
    auto request = basicRequest();
    request.horizonSeconds = 2.0;
    request.sampleIntervalSeconds = 0.25;
    request.maxIntegrationStepSeconds = 0.05;
    request.initialProperAccelerationMps2 = glm::dvec3(0.0);
    request.accelerationProgram.push_back({
        0.0,
        glm::dvec3(4.0 * StandardGravityMps2, 0.0, 0.0)
    });
    request.motionEnvelope.maxProperAccelerationMps2 =
        6.0 * StandardGravityMps2;
    request.motionEnvelope.maxProperJerkMps3 =
        1.0 * StandardGravityMps2;

    const auto result = TrajectoryPredictor::predict(request);
    require(result.ok(), "jerk-limited prediction failed");
    require(result.diagnostics.jerkClamped, "jerk limiter never engaged");
    require(
        result.diagnostics.maxAppliedProperJerkMps3 <=
            request.motionEnvelope.maxProperJerkMps3 + 1.0e-8,
        "applied jerk exceeded the configured envelope"
    );

    const double finalGs = result.samples.back().properLoadGs;
    require(finalGs > 1.9 && finalGs < 2.1, "jerk ramp did not reach the expected 2 g after two seconds");
}

void testMapAdapterPreservesPredictionGeometry()
{
    auto request = basicRequest();
    request.horizonSeconds = 2.0;
    request.sampleIntervalSeconds = 1.0;
    request.initialState.velocityMps = glm::dvec3(5.0, 0.0, 0.0);

    const auto result = TrajectoryPredictor::predict(request);
    require(result.ok(), "adapter source prediction failed");

    const auto mapTrajectory = game::system_map::makeMapObjectTrajectory(
        "ship:42",
        game::system_map::MapTrajectoryKind::Prediction,
        result
    );

    require(mapTrajectory.objectId == "ship:42", "map adapter lost object identity");
    require(
        mapTrajectory.kind == game::system_map::MapTrajectoryKind::Prediction,
        "map adapter lost trajectory kind"
    );
    require(mapTrajectory.points.size() == result.samples.size(), "map adapter changed sample count");

    for (std::size_t i = 0; i < mapTrajectory.points.size(); ++i)
    {
        require(
            near(mapTrajectory.points[i].universeTimeSeconds,
                 result.samples[i].universeTimeSeconds),
            "map adapter changed sample time"
        );
        require(
            nearVec(mapTrajectory.points[i].position,
                    result.samples[i].state.positionMeters),
            "map adapter changed sample position"
        );
    }
}

} // namespace

int main()
{
    const struct
    {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"inertial coast", testInertialCoast},
        {"constant proper acceleration", testConstantProperAcceleration},
        {"gravity is not crew load", testGravityDoesNotBecomeCrewLoad},
        {"manual/autopilot envelopes are caller-selected", testCallerSelectsManualOrAutopilotEnvelope},
        {"jerk envelope smooths acceleration changes", testJerkEnvelopeSmoothsAccelerationChanges},
        {"map adapter preserves prediction geometry", testMapAdapterPreservesPredictionGeometry},
    };

    std::size_t passed = 0;
    for (const auto& test : tests)
    {
        try
        {
            test.fn();
            ++passed;
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << passed << "/" << (sizeof(tests) / sizeof(tests[0]))
              << " trajectory predictor tests passed\n";

    return passed == (sizeof(tests) / sizeof(tests[0])) ? EXIT_SUCCESS : EXIT_FAILURE;
}
