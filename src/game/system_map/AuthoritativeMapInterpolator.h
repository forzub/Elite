#pragma once

#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{

/*
    Presentation-only interpolation between authoritative map snapshots.

    This class never advances orbital phase, integrates velocity or rebuilds
    reference frames. Every endpoint comes from the server. The only local
    operation is blending two confirmed snapshots for display continuity.
*/
class AuthoritativeMapInterpolator
{
public:
    void acceptDetail(
        const world::celestial::DetailMapSnapshot& snapshot,
        double blendDurationSeconds
    );

    void acceptHub(
        const world::celestial::HubMapSnapshot& snapshot,
        double blendDurationSeconds
    );

    void update(double wallDeltaSeconds);

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

private:
    void updateDetail();
    void updateHub();

private:
    bool m_hasDetail = false;
    world::celestial::DetailMapSnapshot m_detailFrom;
    world::celestial::DetailMapSnapshot m_detailTo;
    world::celestial::DetailMapSnapshot m_detailVisual;
    double m_detailElapsedSeconds = 0.0;
    double m_detailDurationSeconds = 0.0;

    bool m_hasHub = false;
    world::celestial::HubMapSnapshot m_hubFrom;
    world::celestial::HubMapSnapshot m_hubTo;
    world::celestial::HubMapSnapshot m_hubVisual;
    double m_hubElapsedSeconds = 0.0;
    double m_hubDurationSeconds = 0.0;
};

} // namespace game::system_map
