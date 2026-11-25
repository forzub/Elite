#pragma once

// A TURNING DECK under a ship (author 2026-11-25, memory ring-rim-landing: a ring hub's city turns with its wheel - its
// 1 g). The ship's physics stays in the non-turning hub frame (honest: no free co-rotation); the flight's CONTROL law
// holds the velocity relative to the deck at the ship's place (u = omega x (x - p)), and the physics adds the
// acceleration it takes to follow the deck (omega x v: at rest over the rim = ~1 g toward the axis, as hovering over a
// planet) - within the ship's stabilization authority (a weak ship cannot hold over the rim, honestly).

#include <glm/glm.hpp>

#include "src/game/navigation/DynamicMotionState.h"

namespace game::navigation
{

// the deck's velocity at the ship's place, in the travel frame's local axes (zero without a deck)
inline glm::dvec3 deckVelocityLocal(const DynamicMotionState& m)
{
    return m.deckActive ? glm::cross(m.deckOmegaLocal, m.localPositionMeters - m.deckAxisPointLocal) : glm::dvec3(0.0);
}

// the acceleration a body moving at the local velocity v needs to keep its velocity relative to the deck (d/dt of the
// deck velocity along the body's path: omega x v)
inline glm::dvec3 deckFollowAccelerationLocal(const DynamicMotionState& m)
{
    return m.deckActive ? glm::cross(m.deckOmegaLocal, m.localVelocityMps) : glm::dvec3(0.0);
}

// while the control law runs, it sees the velocity RELATIVE to the deck
class DeckRelativeVelocityScope
{
public:
    explicit DeckRelativeVelocityScope(DynamicMotionState& m) : m_motion(m), m_shift(deckVelocityLocal(m))
    {
        m_motion.localVelocityMps -= m_shift;
    }
    ~DeckRelativeVelocityScope() { m_motion.localVelocityMps += m_shift; }
    DeckRelativeVelocityScope(const DeckRelativeVelocityScope&) = delete;
    DeckRelativeVelocityScope& operator=(const DeckRelativeVelocityScope&) = delete;

private:
    DynamicMotionState& m_motion;
    glm::dvec3 m_shift;
};

} // namespace game::navigation
