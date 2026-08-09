#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include "src/game/ship/core/ShipControlState.h"

namespace game::server
{

/*
    Ordered stream of fixed-step player input samples.

    ShipControlState contains axes/rates that are integrated over one fixed
    simulation step. Therefore its sequence number is also the reconciliation
    sequence for that predicted step: an acknowledgement may advance only when
    the authoritative simulation has actually consumed that sample.

    Coalescing several samples into the newest state is not equivalent. It lets
    acknowledgement jump across client-predicted simulation steps that never
    existed on the server, so snapshot reconciliation can rewind the predicted
    pose and create a forward/backward saw in the camera.
*/
class FixedStepControlQueue
{
public:
    enum class EnqueueResult
    {
        Accepted,
        Stale
    };

    EnqueueResult enqueue(const ShipControlState& control)
    {
        if (control.controlTick == 0 ||
            control.controlTick <= m_lastReceivedTick)
        {
            return EnqueueResult::Stale;
        }

        m_lastReceivedTick = control.controlTick;

        m_queue.push_back(control);
        return EnqueueResult::Accepted;
    }

    bool consumeNext(ShipControlState& outControl)
    {
        if (m_queue.empty())
            return false;

        outControl = m_queue.front();
        m_queue.pop_front();
        m_lastProcessedTick = outControl.controlTick;
        return true;
    }

    bool discardPendingAndAcknowledgeNewest() noexcept
    {
        if (m_queue.empty())
            return false;

        m_lastProcessedTick = m_queue.back().controlTick;
        m_queue.clear();
        return true;
    }

    void clearPending() noexcept
    {
        m_queue.clear();
    }

    std::uint64_t lastReceivedTick() const noexcept
    {
        return m_lastReceivedTick;
    }

    std::uint64_t lastProcessedTick() const noexcept
    {
        return m_lastProcessedTick;
    }

    std::size_t pendingCount() const noexcept
    {
        return m_queue.size();
    }

private:
    std::deque<ShipControlState> m_queue;
    std::uint64_t m_lastReceivedTick = 0;
    std::uint64_t m_lastProcessedTick = 0;
};

} // namespace game::server
