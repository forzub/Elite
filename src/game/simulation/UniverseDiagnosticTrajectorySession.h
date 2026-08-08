#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>

#include "src/scene/EntityID.h"

namespace game::simulation
{

/*
    Transactional state for accelerated universe-time trajectory diagnostics.

    Production ShipTransform/DynamicMotionState is deliberately not stored here
    and must not be mutated by this session. The session owns only the alternate
    diagnostic branch. Leaving the debug mode discards this branch instead of
    committing future positions back into gameplay.
*/
struct UniverseDiagnosticTrajectoryState
{
    int systemId = -1;
    std::string parentBodyId;
    std::string hubId;

    glm::dvec3 relativePositionMeters {0.0};
    glm::dvec3 relativeVelocityMps {0.0};

    double epochUniverseTimeSeconds = 0.0;
};

class UniverseDiagnosticTrajectorySession
{
public:
    void begin(double epochUniverseTimeSeconds)
    {
        m_states.clear();
        m_epochUniverseTimeSeconds = epochUniverseTimeSeconds;
        m_active = true;
    }

    void discard() noexcept
    {
        m_states.clear();
        m_epochUniverseTimeSeconds = 0.0;
        m_active = false;
    }

    bool active() const noexcept
    {
        return m_active;
    }

    double epochUniverseTimeSeconds() const noexcept
    {
        return m_epochUniverseTimeSeconds;
    }

    std::size_t size() const noexcept
    {
        return m_states.size();
    }

    bool add(
        EntityId shipId,
        UniverseDiagnosticTrajectoryState state
    )
    {
        if (!m_active)
            return false;

        m_states[shipId] = std::move(state);
        return true;
    }

    UniverseDiagnosticTrajectoryState* find(EntityId shipId)
    {
        const auto it = m_states.find(shipId);
        return it != m_states.end() ? &it->second : nullptr;
    }

    const UniverseDiagnosticTrajectoryState* find(EntityId shipId) const
    {
        const auto it = m_states.find(shipId);
        return it != m_states.end() ? &it->second : nullptr;
    }

    std::unordered_map<
        EntityId,
        UniverseDiagnosticTrajectoryState
    >& states() noexcept
    {
        return m_states;
    }

    const std::unordered_map<
        EntityId,
        UniverseDiagnosticTrajectoryState
    >& states() const noexcept
    {
        return m_states;
    }

private:
    bool m_active = false;
    double m_epochUniverseTimeSeconds = 0.0;
    std::unordered_map<
        EntityId,
        UniverseDiagnosticTrajectoryState
    > m_states;
};

} // namespace game::simulation
