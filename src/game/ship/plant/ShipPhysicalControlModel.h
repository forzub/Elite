#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::ship::plant
{

// Simplified local-flight physical topology.
//
// IMPORTANT: these are the only supported ordinary-propulsion layouts.
// A ship chooses one model; descriptors may tune ratings but may not change
// the number or semantic placement of propulsion slots.
//
// Canonical runtime body basis:
//   +X = right
//   +Y = up
//   -Z = forward
//   +Z = aft
enum class PropulsionLayout : std::uint8_t
{
    I = 0,
    Y = 1,
    X = 2
};

enum class PhysicalControlFamily : std::uint8_t
{
    Assisted = 0,
    Newtonian = 1
};

enum class PhysicalControlModelId : std::uint8_t
{
    AssistedI = 0,
    AssistedY,
    AssistedX,
    NewtonianI,
    NewtonianY,
    NewtonianX
};

enum class EngineSlotRole : std::uint8_t
{
    AftMain = 0,
    ForeMain,
    ManoeuvreCluster
};

enum class EngineSlotFreedom : std::uint8_t
{
    FixedAxis = 0,
    VectoringMain,
    OmnidirectionalCluster
};

enum class RuntimeFlightRegime : std::uint8_t
{
    FullAssisted = 0,
    DegradedAssisted,
    ThrustAxisFallback,
    FullNewtonian,
    DegradedNewtonian,
    SurvivalControl,
    Uncontrollable
};

struct EngineSlotDefinition
{
    const char* id = "";
    EngineSlotRole role = EngineSlotRole::AftMain;

    // Dimensionless functional placement. It is NOT CAD geometry.
    // Values describe only the sign and relative lever arm needed by the
    // simplified flight model.
    glm::dvec3 normalizedPositionBody {0.0};

    // Force direction at neutral gimbal. Manoeuvre clusters are omnidirectional
    // and therefore use {0,0,0}.
    glm::dvec3 nominalForceDirectionBody {0.0};

    EngineSlotFreedom freedom = EngineSlotFreedom::FixedAxis;
};

struct EngineSlotState
{
    // First plant iteration is deliberately binary. Resource starvation,
    // thermal derating and partial health belong to later layers.
    bool operational = true;
};

struct AxisAuthority01
{
    double positive = 0.0;
    double negative = 0.0;
};

struct KinematicAuthorityEstimate
{
    AxisAuthority01 longitudinal; // +forward / -reverse
    AxisAuthority01 strafe;       // +right / -left
    AxisAuthority01 lift;         // +up / -down
    AxisAuthority01 pitch;
    AxisAuthority01 yaw;
    AxisAuthority01 roll;
};

struct PhysicalControlSnapshot
{
    bool valid = false;
    PhysicalControlModelId model = PhysicalControlModelId::AssistedI;
    RuntimeFlightRegime regime = RuntimeFlightRegime::Uncontrollable;

    std::size_t mainSlots = 0;
    std::size_t manoeuvreSlots = 0;
    std::size_t operationalMainSlots = 0;
    std::size_t operationalManoeuvreSlots = 0;

    KinematicAuthorityEstimate authority;

    // Compatibility projection for the current motion stack. This is the
    // bridge we will feed into ShipControlExecutionGateway while migrating
    // away from aggregate magic parameters.
    ShipParams effectivePhysics {};
};

[[nodiscard]] constexpr PhysicalControlFamily familyOf(
    PhysicalControlModelId model
) noexcept
{
    return
        model == PhysicalControlModelId::AssistedI ||
        model == PhysicalControlModelId::AssistedY ||
        model == PhysicalControlModelId::AssistedX
            ? PhysicalControlFamily::Assisted
            : PhysicalControlFamily::Newtonian;
}

[[nodiscard]] constexpr PropulsionLayout layoutOf(
    PhysicalControlModelId model
) noexcept
{
    switch (model)
    {
        case PhysicalControlModelId::AssistedY:
        case PhysicalControlModelId::NewtonianY:
            return PropulsionLayout::Y;

        case PhysicalControlModelId::AssistedX:
        case PhysicalControlModelId::NewtonianX:
            return PropulsionLayout::X;

        case PhysicalControlModelId::AssistedI:
        case PhysicalControlModelId::NewtonianI:
        default:
            return PropulsionLayout::I;
    }
}

[[nodiscard]] constexpr game::navigation::LocalFlightControlLaw nativeFlightLaw(
    PhysicalControlModelId model
) noexcept
{
    return familyOf(model) == PhysicalControlFamily::Assisted
        ? game::navigation::LocalFlightControlLaw::Assisted
        : game::navigation::LocalFlightControlLaw::Newtonian;
}

[[nodiscard]] inline std::vector<EngineSlotState> healthySlotState(
    const std::vector<EngineSlotDefinition>& slots
)
{
    return std::vector<EngineSlotState>(slots.size(), EngineSlotState{true});
}

} // namespace game::ship::plant
