#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string_view>

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


[[nodiscard]] constexpr PhysicalControlModelId modelFor(
    PhysicalControlFamily family,
    PropulsionLayout layout
) noexcept
{
    if (family == PhysicalControlFamily::Assisted)
    {
        switch (layout)
        {
            case PropulsionLayout::Y: return PhysicalControlModelId::AssistedY;
            case PropulsionLayout::X: return PhysicalControlModelId::AssistedX;
            case PropulsionLayout::I:
            default: return PhysicalControlModelId::AssistedI;
        }
    }

    switch (layout)
    {
        case PropulsionLayout::Y: return PhysicalControlModelId::NewtonianY;
        case PropulsionLayout::X: return PhysicalControlModelId::NewtonianX;
        case PropulsionLayout::I:
        default: return PhysicalControlModelId::NewtonianI;
    }
}

[[nodiscard]] constexpr PropulsionLayout nextLayout(
    PropulsionLayout layout
) noexcept
{
    switch (layout)
    {
        case PropulsionLayout::I: return PropulsionLayout::Y;
        case PropulsionLayout::Y: return PropulsionLayout::X;
        case PropulsionLayout::X:
        default: return PropulsionLayout::I;
    }
}

[[nodiscard]] inline const char* physicalControlModelName(
    PhysicalControlModelId model
) noexcept
{
    switch (model)
    {
        case PhysicalControlModelId::AssistedI: return "Assisted-I";
        case PhysicalControlModelId::AssistedY: return "Assisted-Y";
        case PhysicalControlModelId::AssistedX: return "Assisted-X";
        case PhysicalControlModelId::NewtonianI: return "Newtonian-I";
        case PhysicalControlModelId::NewtonianY: return "Newtonian-Y";
        case PhysicalControlModelId::NewtonianX: return "Newtonian-X";
        default: return "Assisted-I";
    }
}

[[nodiscard]] inline bool parsePhysicalControlModel(
    std::string_view value,
    PhysicalControlModelId& out
) noexcept
{
    if (value == "Assisted-I") out = PhysicalControlModelId::AssistedI;
    else if (value == "Assisted-Y") out = PhysicalControlModelId::AssistedY;
    else if (value == "Assisted-X") out = PhysicalControlModelId::AssistedX;
    else if (value == "Newtonian-I") out = PhysicalControlModelId::NewtonianI;
    else if (value == "Newtonian-Y") out = PhysicalControlModelId::NewtonianY;
    else if (value == "Newtonian-X") out = PhysicalControlModelId::NewtonianX;
    else return false;
    return true;
}

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
    glm::dvec3 normalizedPositionBody {0.0, 0.0, 0.0};

    // Force direction at neutral gimbal. Manoeuvre clusters are omnidirectional
    // and therefore use {0,0,0}.
    glm::dvec3 nominalForceDirectionBody {0.0, 0.0, 0.0};

    EngineSlotFreedom freedom = EngineSlotFreedom::FixedAxis;
};

struct EngineSlotState
{
    // Runtime actuator availability. The first integrated plant only derives
    // these values from module health/state; power, fuel and thermal limits are
    // deliberately absent from this layer for now.
    bool operational = true;

    // Fraction of nominal thrust currently available.
    double thrust01 = 1.0;

    // Fraction of nominal thrust-vectoring authority currently available.
    // Manoeuvre clusters ignore this field.
    double vectoring01 = 1.0;

    // Time-varying thrust instability authored by the equipment model.
    // This is NOT another damage source: execution uses it to modulate the
    // already-degraded thrust01 deterministically at the physical-force seam.
    double thrustIrregularity01 = 0.0;
};

[[nodiscard]] inline double effectiveSlotThrust01(
    const EngineSlotState& state
) noexcept
{
    return state.operational
        ? std::clamp(state.thrust01, 0.0, 1.0)
        : 0.0;
}

[[nodiscard]] inline double effectiveSlotVectoring01(
    const EngineSlotState& state
) noexcept
{
    return state.operational
        ? std::clamp(
              std::min(state.thrust01, state.vectoring01),
              0.0,
              1.0
          )
        : 0.0;
}

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

// Additional control available specifically while a vectoring main bank is
// producing thrust. This is intentionally separate from independent/RCS-like
// authority: a Newtonian craft may bend its powered trajectory with TVC even
// though it cannot strafe freely at high authority while coasting.
struct PoweredMainVectoringEstimate
{
    double trajectoryStrafe01 = 0.0;
    double trajectoryLift01 = 0.0;
    double pitch01 = 0.0;
    double yaw01 = 0.0;
    double roll01 = 0.0;
};

// Propulsive utilization, NOT engine thermodynamic efficiency.
// usefulNetForce / sum(abs(generated engine thrust)) for representative
// translation modes. Values below 1 represent thrust components that cancel
// each other or are spent on simultaneous attitude compensation.
struct ThrustUtilizationEstimate
{
    double axialTranslation01 = 1.0;
    double lateralTranslation01 = 1.0;
    double cancelledDuringLateral01 = 0.0;
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
    PoweredMainVectoringEstimate poweredMainVectoring;
    ThrustUtilizationEstimate thrustUtilization;

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
