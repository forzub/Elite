#include "TestIdealRadarUnit.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

namespace game::radar
{
namespace
{
constexpr double MinScanPeriodSeconds = 1.0e-6;
constexpr double TimeEpsilon = 1.0e-9;

std::uint64_t mix64(std::uint64_t x) noexcept
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31U);
}
}

void TestIdealRadarUnit::configure(
    const game::RadarDesc& desc,
    std::uint64_t observerSeed
)
{
    m_desc = desc;
    m_observerSeed = observerSeed != 0 ? observerSeed : 1;
    m_configured = true;

    m_navigationPositionBiasMeters = stableVector(
        mix64(m_observerSeed ^ 0x4e4156504f53ULL),
        m_desc.ownPositionUncertaintyMeters * 0.65
    );
    m_navigationVelocityBiasMps = stableVector(
        mix64(m_observerSeed ^ 0x4e415656454cULL),
        m_desc.ownVelocityUncertaintyMps * 0.65
    );

    reset();
}

void TestIdealRadarUnit::reset()
{
    m_measurementScheduleStarted = false;
    m_nextMeasurementUniverseTimeSeconds = 0.0;
    m_nextScanSequence = 1;
    m_nextPublicTrackId = 1;
    m_tracks.clear();
    m_pendingReports.clear();
    m_hasAvailableScan = false;
    m_latestAvailableScan = {};
}

bool TestIdealRadarUnit::measurementDue(double universeTimeSeconds) const noexcept
{
    if (!m_configured)
        return false;

    if (!m_measurementScheduleStarted)
        return true;

    return universeTimeSeconds + TimeEpsilon >=
        m_nextMeasurementUniverseTimeSeconds;
}

void TestIdealRadarUnit::captureMeasurement(
    double universeTimeSeconds,
    const world::coordinates::WorldPosition& observerWorldPosition,
    const glm::dvec3& observerWorldVelocityMps,
    const glm::mat4& observerOrientation,
    const std::vector<TestIdealRadarTruthContact>& truthContacts
)
{
    if (!m_configured || !measurementDue(universeTimeSeconds))
        return;

    RadarScanReport report;
    report.scanSequence = m_nextScanSequence++;
    report.measuredAtUniverseTimeSeconds = universeTimeSeconds;
    report.availableAtUniverseTimeSeconds =
        universeTimeSeconds + std::max(0.0, m_desc.processingLatencySeconds);

    glm::dmat3 worldFromSensor(1.0);
    for (int column = 0; column < 3; ++column)
    {
        for (int row = 0; row < 3; ++row)
        {
            worldFromSensor[column][row] =
                static_cast<double>(observerOrientation[column][row]);
        }
    }
    const glm::dmat3 sensorFromWorld = glm::transpose(worldFromSensor);

    for (const auto& truth : truthContacts)
    {
        if (truth.sourceKey == 0)
            continue;

        const glm::dvec3 relativeWorld =
            world::coordinates::relativeMeters(
                truth.worldPosition,
                observerWorldPosition
            );
        const double trueRange = glm::length(relativeWorld);

        const double effectiveRange = std::min(
            std::max(0.0, m_desc.maxRange),
            std::max(0.0, m_desc.maxRange) *
                std::sqrt(std::max(0.0, truth.radarCrossSection))
        );

        if (trueRange > effectiveRange)
            continue;

        TrackState& track = trackFor(
            truth.sourceKey,
            universeTimeSeconds
        );

        const glm::dvec3 relativeVelocityWorld =
            truth.worldVelocityMps - observerWorldVelocityMps;

        RadarTrackReport measured;
        measured.trackId = track.publicId;
        measured.relativePositionMeters =
            sensorFromWorld * relativeWorld + track.positionBiasMeters;
        measured.relativeVelocityMps =
            sensorFromWorld * relativeVelocityWorld + track.velocityBiasMps;
        measured.rangeMeters = glm::length(measured.relativePositionMeters);
        measured.positionUncertaintyMeters =
            std::max(0.0, m_desc.positionUncertaintyMeters);
        measured.velocityUncertaintyMps =
            std::max(0.0, m_desc.velocityUncertaintyMps);
        measured.confidence = 1.0;
        report.tracks.push_back(measured);
    }

    std::sort(
        report.tracks.begin(),
        report.tracks.end(),
        [](const RadarTrackReport& a, const RadarTrackReport& b)
        {
            return a.trackId.value < b.trackId.value;
        }
    );

    m_pendingReports.push_back(std::move(report));

    const double period = std::max(
        MinScanPeriodSeconds,
        m_desc.scanInterval
    );
    m_nextMeasurementUniverseTimeSeconds = universeTimeSeconds + period;
    m_measurementScheduleStarted = true;

    expireOldTracks(universeTimeSeconds);
}

void TestIdealRadarUnit::advanceAvailability(double universeTimeSeconds)
{
    while (!m_pendingReports.empty() &&
           m_pendingReports.front().availableAtUniverseTimeSeconds <=
               universeTimeSeconds + TimeEpsilon)
    {
        m_latestAvailableScan = std::move(m_pendingReports.front());
        m_pendingReports.pop_front();
        m_hasAvailableScan = true;
    }
}

game::navigation::NavigationSolution
TestIdealRadarUnit::navigationSolution(
    double universeTimeSeconds,
    const world::coordinates::WorldPosition& trueWorldPosition,
    const glm::dvec3& trueWorldVelocityMps
) const
{
    game::navigation::NavigationSolution solution;
    solution.estimatedWorldPosition = world::coordinates::translated(
        trueWorldPosition,
        m_navigationPositionBiasMeters
    );
    solution.estimatedWorldVelocityMps =
        trueWorldVelocityMps + m_navigationVelocityBiasMps;
    solution.epochUniverseTimeSeconds = universeTimeSeconds;
    solution.positionUncertaintyMeters =
        std::max(0.0, m_desc.ownPositionUncertaintyMeters);
    solution.velocityUncertaintyMps =
        std::max(0.0, m_desc.ownVelocityUncertaintyMps);
    solution.fixRevision = 1;
    return solution;
}

double TestIdealRadarUnit::signedUnit(std::uint64_t seed) noexcept
{
    const std::uint64_t mixed = mix64(seed);
    const double unit = static_cast<double>(mixed >> 11U) *
        (1.0 / 9007199254740992.0);
    return unit * 2.0 - 1.0;
}

glm::dvec3 TestIdealRadarUnit::stableVector(
    std::uint64_t seed,
    double magnitude
) noexcept
{
    if (!(magnitude > 0.0))
        return glm::dvec3(0.0);

    glm::dvec3 v(
        signedUnit(seed ^ 0x243f6a8885a308d3ULL),
        signedUnit(seed ^ 0x13198a2e03707344ULL),
        signedUnit(seed ^ 0xa4093822299f31d0ULL)
    );

    const double length = glm::length(v);
    if (length <= std::numeric_limits<double>::epsilon())
        return glm::dvec3(magnitude, 0.0, 0.0);

    // Keep the actual hidden error inside the declared uncertainty envelope.
    const double radialScale = 0.35 +
        0.65 * std::abs(signedUnit(seed ^ 0x082efa98ec4e6c89ULL));
    return (v / length) * magnitude * radialScale;
}

TestIdealRadarUnit::TrackState& TestIdealRadarUnit::trackFor(
    std::uint64_t sourceKey,
    double universeTimeSeconds
)
{
    const auto [it, inserted] = m_tracks.try_emplace(sourceKey);
    TrackState& state = it->second;

    if (inserted)
    {
        state.publicId.value = m_nextPublicTrackId++;
        state.positionBiasMeters = stableVector(
            mix64(m_observerSeed ^ sourceKey ^ 0x524144504f53ULL),
            m_desc.positionUncertaintyMeters * 0.65
        );
        state.velocityBiasMps = stableVector(
            mix64(m_observerSeed ^ sourceKey ^ 0x52414456454cULL),
            m_desc.velocityUncertaintyMps * 0.65
        );
    }

    state.lastSeenUniverseTimeSeconds = universeTimeSeconds;
    return state;
}

void TestIdealRadarUnit::expireOldTracks(double universeTimeSeconds)
{
    const double hold = std::max(0.0, m_desc.trackHoldSeconds);

    for (auto it = m_tracks.begin(); it != m_tracks.end(); )
    {
        if (universeTimeSeconds - it->second.lastSeenUniverseTimeSeconds > hold)
            it = m_tracks.erase(it);
        else
            ++it;
    }
}

} // namespace game::radar
