#include "src/game/docking/landing/DockLandingControllerApi.h"

#include <cmath>

namespace game::docking::landing
{
namespace
{

bool finite(const glm::dvec3& value) noexcept
{
    return std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

} // namespace

bool DockLandingController::begin(const LandingHandoff& handoff) noexcept
{
    const bool acceptable =
        handoff.valid &&
        handoff.clearanceRevision != 0 &&
        !handoff.facilityId.empty() &&
        !handoff.padId.empty() &&
        !handoff.kinematicFrameId.empty() &&
        finite(handoff.approachPositionLocalMeters) &&
        finite(handoff.padCenterLocalMeters) &&
        finite(handoff.padNormalLocal) &&
        finite(handoff.forwardLocal) &&
        glm::length(handoff.padNormalLocal) > 0.9 &&
        glm::length(handoff.forwardLocal) > 0.9 &&
        std::isfinite(handoff.positionToleranceMeters) &&
        handoff.positionToleranceMeters > 0.0 &&
        std::isfinite(handoff.linearVelocityToleranceMps) &&
        handoff.linearVelocityToleranceMps >= 0.0 &&
        std::isfinite(handoff.angularVelocityToleranceRadPerSec) &&
        handoff.angularVelocityToleranceRadPerSec >= 0.0;

    if (!acceptable)
    {
        m_handoff = {};
        m_state = LandingControllerState::Rejected;
        return false;
    }

    m_handoff = handoff;
    m_state = LandingControllerState::AlignOverPad;
    return true;
}

void DockLandingController::reset() noexcept
{
    m_handoff = {};
    m_state = LandingControllerState::Idle;
}

} // namespace game::docking::landing
