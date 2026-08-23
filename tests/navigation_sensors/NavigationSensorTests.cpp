#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/geometric.hpp>

#include "src/game/equipment/radar/TestIdealRadarUnit.h"

namespace
{
using game::RadarBackendKind;
using game::RadarDesc;
using game::radar::TestIdealRadarTruthContact;
using game::radar::TestIdealRadarUnit;
using world::coordinates::makeWorldPositionFromMeters;
using world::coordinates::relativeMeters;

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}

bool near(double a, double b, double eps = 1.0e-9)
{
    return std::abs(a - b) <= eps;
}

bool nearVec(const glm::dvec3& a, const glm::dvec3& b, double eps = 1.0e-8)
{
    return glm::length(a - b) <= eps;
}

RadarDesc spec(double scanPeriod)
{
    RadarDesc desc {};
    desc.backendKind = RadarBackendKind::TestIdeal;
    desc.maxRange = 2'000'000.0;
    desc.scanInterval = scanPeriod;
    desc.processingLatencySeconds = 0.08;
    desc.trackHoldSeconds = 5.0;
    desc.positionUncertaintyMeters = 12.0;
    desc.velocityUncertaintyMps = 0.20;
    desc.ownPositionUncertaintyMeters = 250.0;
    desc.ownVelocityUncertaintyMps = 0.50;
    return desc;
}

TestIdealRadarTruthContact target(
    std::uint64_t key,
    double x,
    double vx
)
{
    TestIdealRadarTruthContact result;
    result.sourceKey = key;
    result.worldPosition = makeWorldPositionFromMeters(glm::dvec3(x, 0.0, 0.0));
    result.worldVelocityMps = glm::dvec3(vx, 0.0, 0.0);
    result.radarCrossSection = 1.0;
    return result;
}

void testDiscreteCadenceLatencyAndStableTrack()
{
    TestIdealRadarUnit radar;
    radar.configure(spec(0.50), 77u);

    const auto observer = makeWorldPositionFromMeters(glm::dvec3(0.0));
    const glm::dvec3 observerVelocity(0.0);
    const glm::mat4 orientation(1.0f);

    require(radar.measurementDue(10.0), "first scan is not due");
    radar.captureMeasurement(
        10.0,
        observer,
        observerVelocity,
        orientation,
        {target(123u, 100.0, 2.0)}
    );

    radar.advanceAvailability(10.079);
    require(!radar.hasAvailableScan(), "scan escaped before processing latency");
    radar.advanceAvailability(10.08);
    require(radar.hasAvailableScan(), "scan was not delivered at availableAt");

    const auto first = radar.latestAvailableScan();
    radar.advanceAvailability(10.20);
    require(radar.latestAvailableScan().scanSequence == first.scanSequence,
        "repeated transport epoch became a fake new measurement");
    require(first.scanSequence == 1u, "first scan sequence mismatch");
    require(near(first.measuredAtUniverseTimeSeconds, 10.0), "measurement epoch mismatch");
    require(near(first.availableAtUniverseTimeSeconds, 10.08), "availability epoch mismatch");
    require(first.tracks.size() == 1u, "expected one radar track");
    const auto firstTrack = first.tracks.front();
    require(firstTrack.trackId.value != 0u, "public track id is invalid");

    require(!radar.measurementDue(10.49), "scan cadence depends on update frequency");
    require(radar.measurementDue(10.50), "second scan was not due at its own cadence");

    radar.captureMeasurement(
        10.50,
        observer,
        observerVelocity,
        orientation,
        {target(123u, 101.0, 2.0)}
    );
    radar.advanceAvailability(10.58);
    const auto second = radar.latestAvailableScan();
    require(second.scanSequence == 2u, "second scan sequence mismatch");
    require(second.tracks.size() == 1u, "second scan lost target");
    require(second.tracks.front().trackId == firstTrack.trackId,
        "track id changed between scans of the same source");

    // Hidden measurement error is correlated for the lifetime of a track.
    const glm::dvec3 firstBias =
        firstTrack.relativePositionMeters - glm::dvec3(100.0, 0.0, 0.0);
    const glm::dvec3 secondBias =
        second.tracks.front().relativePositionMeters - glm::dvec3(101.0, 0.0, 0.0);
    require(nearVec(firstBias, secondBias),
        "radar regenerated position error on every scan");
}

void testPerRadarCadence()
{
    TestIdealRadarUnit fast;
    TestIdealRadarUnit slow;
    fast.configure(spec(0.20), 1u);
    slow.configure(spec(1.00), 2u);

    const auto observer = makeWorldPositionFromMeters(glm::dvec3(0.0));
    const glm::mat4 orientation(1.0f);
    const std::vector<TestIdealRadarTruthContact> noTargets;

    fast.captureMeasurement(20.0, observer, glm::dvec3(0.0), orientation, noTargets);
    slow.captureMeasurement(20.0, observer, glm::dvec3(0.0), orientation, noTargets);

    require(fast.measurementDue(20.20), "fast radar cadence ignored its device spec");
    require(!slow.measurementDue(20.20), "slow radar was forced onto another radar cadence");
    require(slow.measurementDue(21.00), "slow radar cadence did not mature");
}

void testStableNavigationBias()
{
    TestIdealRadarUnit radar;
    radar.configure(spec(0.5), 555u);

    const auto trueA = makeWorldPositionFromMeters(glm::dvec3(1000.0, 2000.0, 3000.0));
    const auto trueB = makeWorldPositionFromMeters(glm::dvec3(1125.0, 1990.0, 3020.0));

    const auto a = radar.navigationSolution(1.0, trueA, glm::dvec3(5.0, 0.0, 0.0));
    const auto b = radar.navigationSolution(2.0, trueB, glm::dvec3(5.0, 0.0, 0.0));

    const glm::dvec3 trueDelta = relativeMeters(trueB, trueA);
    const glm::dvec3 estimatedDelta =
        relativeMeters(b.estimatedWorldPosition, a.estimatedWorldPosition);

    require(nearVec(trueDelta, estimatedDelta),
        "own-position uncertainty is random noise instead of stable bias");
    require(a.fixRevision == b.fixRevision && a.fixRevision == 1u,
        "ordinary propagation changed navigation fix revision");
    require(near(a.positionUncertaintyMeters, 250.0),
        "navigation position uncertainty mismatch");
    require(near(a.velocityUncertaintyMps, 0.50),
        "navigation velocity uncertainty mismatch");
}
}

int main()
{
    testDiscreteCadenceLatencyAndStableTrack();
    testPerRadarCadence();
    testStableNavigationBias();
    std::cout << "[PASS] ideal radar cadence, latency, tracks and stable navigation bias\n";
    return 0;
}
