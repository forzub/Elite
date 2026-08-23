#pragma once

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "src/game/equipment/radar/RadarDesc.h"
#include "src/game/equipment/radar/RadarSensorTypes.h"
#include "src/game/navigation/NavigationSolution.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::radar
{

// Server-only truth input. sourceKey deliberately never appears in the public
// RadarScanReport. It exists only so this synthetic producer can preserve track
// continuity while reading the authoritative world directly.
struct TestIdealRadarTruthContact
{
    std::uint64_t sourceKey = 0;
    world::coordinates::WorldPosition worldPosition;
    glm::dvec3 worldVelocityMps {0.0};
    double radarCrossSection = 1.0;
};

class TestIdealRadarUnit
{
public:
    void configure(const game::RadarDesc& desc, std::uint64_t observerSeed);
    void reset();

    bool configured() const noexcept { return m_configured; }
    bool measurementDue(double universeTimeSeconds) const noexcept;

    void captureMeasurement(
        double universeTimeSeconds,
        const world::coordinates::WorldPosition& observerWorldPosition,
        const glm::dvec3& observerWorldVelocityMps,
        const glm::mat4& observerOrientation,
        const std::vector<TestIdealRadarTruthContact>& truthContacts
    );

    void advanceAvailability(double universeTimeSeconds);

    bool hasAvailableScan() const noexcept { return m_hasAvailableScan; }
    const RadarScanReport& latestAvailableScan() const noexcept
    {
        return m_latestAvailableScan;
    }

    game::navigation::NavigationSolution navigationSolution(
        double universeTimeSeconds,
        const world::coordinates::WorldPosition& trueWorldPosition,
        const glm::dvec3& trueWorldVelocityMps
    ) const;

private:
    struct TrackState
    {
        RadarTrackId publicId {};
        glm::dvec3 positionBiasMeters {0.0};
        glm::dvec3 velocityBiasMps {0.0};
        double lastSeenUniverseTimeSeconds = 0.0;
    };

    static double signedUnit(std::uint64_t seed) noexcept;
    static glm::dvec3 stableVector(
        std::uint64_t seed,
        double magnitude
    ) noexcept;

    TrackState& trackFor(
        std::uint64_t sourceKey,
        double universeTimeSeconds
    );
    void expireOldTracks(double universeTimeSeconds);

    game::RadarDesc m_desc {};
    bool m_configured = false;
    std::uint64_t m_observerSeed = 0;

    double m_nextMeasurementUniverseTimeSeconds = 0.0;
    bool m_measurementScheduleStarted = false;
    std::uint64_t m_nextScanSequence = 1;
    std::uint64_t m_nextPublicTrackId = 1;

    std::unordered_map<std::uint64_t, TrackState> m_tracks;
    std::deque<RadarScanReport> m_pendingReports;

    bool m_hasAvailableScan = false;
    RadarScanReport m_latestAvailableScan;

    glm::dvec3 m_navigationPositionBiasMeters {0.0};
    glm::dvec3 m_navigationVelocityBiasMps {0.0};
};

} // namespace game::radar
