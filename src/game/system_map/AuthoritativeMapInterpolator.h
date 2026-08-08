#pragma once

#include <cstddef>
#include <deque>

#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{

/*
    Presentation-only sampling of authoritative map snapshots.

    The snapshots themselves remain server-owned. The client only samples the
    buffered authoritative history at the same delayed server timestamp used by
    the rest of client rendering. This prevents map motion from depending on
    WebSocket/request arrival cadence or on a second locally-integrated clock.
*/
class AuthoritativeMapInterpolator
{
public:
    void acceptDetail(
        const world::celestial::DetailMapSnapshot& snapshot,
        double serverTimeSeconds
    );

    void acceptHub(
        const world::celestial::HubMapSnapshot& snapshot,
        double serverTimeSeconds
    );

    void update(
        double renderServerTimeSeconds,
        double renderUniverseTimeSeconds
    );

    bool hasDetail() const noexcept { return m_hasDetail; }
    bool hasHub() const noexcept { return m_hasHub; }

    const world::celestial::DetailMapSnapshot& detail() const noexcept
    {
        return m_detailVisual;
    }

    const world::celestial::HubMapSnapshot& hub() const noexcept
    {
        return m_hubVisual;
    }

    const char* detailSampleModeName() const noexcept;
    const char* hubSampleModeName() const noexcept;
    double detailNewestGapSeconds() const noexcept
    {
        return m_detailNewestGapSeconds;
    }
    double hubNewestGapSeconds() const noexcept
    {
        return m_hubNewestGapSeconds;
    }
    std::size_t detailBufferedSnapshotCount() const noexcept
    {
        return m_detailHistory.size();
    }
    std::size_t hubBufferedSnapshotCount() const noexcept
    {
        return m_hubHistory.size();
    }

private:
    enum class SampleMode
    {
        None,
        Holding,
        Interpolating,
        Extrapolating
    };

    struct TimedDetailSnapshot
    {
        double serverTimeSeconds = 0.0;
        world::celestial::DetailMapSnapshot snapshot;
    };

    struct TimedHubSnapshot
    {
        double serverTimeSeconds = 0.0;
        world::celestial::HubMapSnapshot snapshot;
    };

    void sampleDetail(
        double renderServerTimeSeconds,
        double renderUniverseTimeSeconds
    );
    void sampleHub(
        double renderServerTimeSeconds,
        double renderUniverseTimeSeconds
    );

private:
    static constexpr std::size_t MaxBufferedSnapshots = 32u;

    bool m_hasDetail = false;
    std::deque<TimedDetailSnapshot> m_detailHistory;
    world::celestial::DetailMapSnapshot m_detailVisual;
    SampleMode m_detailSampleMode = SampleMode::None;
    double m_detailNewestGapSeconds = 0.0;

    bool m_hasHub = false;
    std::deque<TimedHubSnapshot> m_hubHistory;
    world::celestial::HubMapSnapshot m_hubVisual;
    SampleMode m_hubSampleMode = SampleMode::None;
    double m_hubNewestGapSeconds = 0.0;
};

} // namespace game::system_map
