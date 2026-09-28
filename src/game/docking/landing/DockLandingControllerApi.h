#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace game::docking::landing
{

enum class LandingControllerState : std::uint8_t
{
    Idle = 0,
    AlignOverPad,
    DeployGear,
    Settle,
    GearContact,
    Latched,
    Rejected
};

enum class LandingGearState : std::uint8_t
{
    Retracted = 0,
    Deploying,
    Deployed,
    Loaded,
    Retracting
};

struct LandingHandoff
{
    bool valid = false;
    std::uint64_t clearanceRevision = 0;
    std::string facilityId;
    std::string padId;
    std::string kinematicFrameId;

    glm::dvec3 approachPositionLocalMeters {0.0};
    glm::dvec3 padCenterLocalMeters {0.0};
    glm::dvec3 padNormalLocal {0.0, 1.0, 0.0};
    glm::dvec3 forwardLocal {0.0, 0.0, -1.0};

    double positionToleranceMeters = 0.0;
    double linearVelocityToleranceMps = 0.0;
    double angularVelocityToleranceRadPerSec = 0.0;
};

class DockLandingController final
{
public:
    [[nodiscard]] bool begin(const LandingHandoff& handoff) noexcept;
    void reset() noexcept;

    [[nodiscard]] LandingControllerState state() const noexcept
    {
        return m_state;
    }

    [[nodiscard]] const LandingHandoff& handoff() const noexcept
    {
        return m_handoff;
    }

    // Normal internal parking is precision manoeuvring. The main engine is
    // outside this controller's authority by contract.
    [[nodiscard]] static constexpr bool mainEnginePermitted() noexcept
    {
        return false;
    }

private:
    LandingControllerState m_state = LandingControllerState::Idle;
    LandingHandoff m_handoff {};
};

} // namespace game::docking::landing
