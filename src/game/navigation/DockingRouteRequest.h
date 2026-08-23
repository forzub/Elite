#pragma once

#include <cstdint>

#include "src/game/navigation/RoutePlan.h"

namespace game::navigation
{

struct DockingRouteRequest
{
    std::uint64_t serial = 0;
    RouteTargetRef target;

    bool valid() const noexcept
    {
        return serial != 0 &&
               target.kind == NavigationRouteAnchorKind::SemanticAnchor &&
               target.valid();
    }
};

class DockingRouteRequestState
{
public:
    const DockingRouteRequest& pending() const noexcept
    {
        return m_pending;
    }

    std::uint64_t request(const RouteTargetRef& target)
    {
        if (target.kind != NavigationRouteAnchorKind::SemanticAnchor ||
            !target.valid())
        {
            return 0;
        }

        m_pending.serial = m_nextSerial++;
        m_pending.target = target;
        return m_pending.serial;
    }

    void clear() noexcept
    {
        m_pending = {};
    }

private:
    DockingRouteRequest m_pending;
    std::uint64_t m_nextSerial = 1;
};

} // namespace game::navigation
