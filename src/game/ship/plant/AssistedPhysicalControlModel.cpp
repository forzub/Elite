#include "src/game/ship/plant/AssistedPhysicalControlModel.h"

#include <algorithm>

#include "src/game/ship/core/ShipDynamics.h"

namespace game::ship::plant
{
namespace
{

constexpr double Sqrt3Over2 = 0.86602540378443864676;
constexpr double InvSqrt2 = 0.70710678118654752440;

const std::vector<EngineSlotDefinition> AssistedI = {
    {"aft_left",  EngineSlotRole::AftMain, {-1.0, 0.0,  1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_right", EngineSlotRole::AftMain, { 1.0, 0.0,  1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_left",  EngineSlotRole::ForeMain, {-1.0, 0.0, -1.0}, {0.0, 0.0,  1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_right", EngineSlotRole::ForeMain, { 1.0, 0.0, -1.0}, {0.0, 0.0,  1.0}, EngineSlotFreedom::VectoringMain},
};

const std::vector<EngineSlotDefinition> AssistedY = {
    {"aft_top",         EngineSlotRole::AftMain, {0.0,          1.0,  1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_left",  EngineSlotRole::AftMain, {-Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_right", EngineSlotRole::AftMain, { Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_top",         EngineSlotRole::ForeMain, {0.0,          1.0, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_left",  EngineSlotRole::ForeMain, {-Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_right", EngineSlotRole::ForeMain, { Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
};

const std::vector<EngineSlotDefinition> AssistedX = {
    {"aft_upper_left",  EngineSlotRole::AftMain, {-InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_upper_right", EngineSlotRole::AftMain, { InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_left",  EngineSlotRole::AftMain, {-InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"aft_lower_right", EngineSlotRole::AftMain, { InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_upper_left",  EngineSlotRole::ForeMain, {-InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_upper_right", EngineSlotRole::ForeMain, { InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_left",  EngineSlotRole::ForeMain, {-InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
    {"fore_lower_right", EngineSlotRole::ForeMain, { InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 1.0}, EngineSlotFreedom::VectoringMain},
};

PhysicalControlModelId modelId(PropulsionLayout layout) noexcept
{
    switch (layout)
    {
        case PropulsionLayout::Y:
            return PhysicalControlModelId::AssistedY;
        case PropulsionLayout::X:
            return PhysicalControlModelId::AssistedX;
        case PropulsionLayout::I:
        default:
            return PhysicalControlModelId::AssistedI;
    }
}

double fraction(std::size_t operational, std::size_t total) noexcept
{
    return total > 0
        ? static_cast<double>(operational) / static_cast<double>(total)
        : 0.0;
}

AxisAuthority01 symmetric(double authority) noexcept
{
    const double a = std::clamp(authority, 0.0, 1.0);
    return {a, a};
}

} // namespace

const std::vector<EngineSlotDefinition>& AssistedPhysicalControlModel::slots(
    PropulsionLayout layout
)
{
    switch (layout)
    {
        case PropulsionLayout::Y:
            return AssistedY;
        case PropulsionLayout::X:
            return AssistedX;
        case PropulsionLayout::I:
        default:
            return AssistedI;
    }
}

PhysicalControlSnapshot AssistedPhysicalControlModel::evaluate(
    PropulsionLayout layout,
    const ShipParams& nominalPhysics,
    const std::vector<EngineSlotState>& state
) noexcept
{
    PhysicalControlSnapshot out;
    out.model = modelId(layout);
    out.effectivePhysics = nominalPhysics;

    const auto& definition = slots(layout);
    if (state.size() != definition.size())
        return out;

    std::size_t aftSlots = 0;
    std::size_t foreSlots = 0;
    std::size_t operationalAft = 0;
    std::size_t operationalFore = 0;

    for (std::size_t i = 0; i < definition.size(); ++i)
    {
        const auto role = definition[i].role;
        if (role == EngineSlotRole::AftMain)
        {
            ++aftSlots;
            if (state[i].operational)
                ++operationalAft;
        }
        else if (role == EngineSlotRole::ForeMain)
        {
            ++foreSlots;
            if (state[i].operational)
                ++operationalFore;
        }
    }

    out.mainSlots = aftSlots + foreSlots;
    out.operationalMainSlots = operationalAft + operationalFore;

    const double aftFraction = fraction(operationalAft, aftSlots);
    const double foreFraction = fraction(operationalFore, foreSlots);
    const double vectoringFraction =
        fraction(out.operationalMainSlots, out.mainSlots);

    out.authority.longitudinal.positive =
        nominalPhysics.forwardMainEngineAvailable ? aftFraction : 0.0;
    out.authority.longitudinal.negative =
        nominalPhysics.reverseMainEngineAvailable ? foreFraction : 0.0;
    out.authority.strafe = symmetric(vectoringFraction);
    out.authority.lift = symmetric(vectoringFraction);
    out.authority.pitch = symmetric(vectoringFraction);
    out.authority.yaw = symmetric(vectoringFraction);
    out.authority.roll = symmetric(vectoringFraction);

    out.poweredMainVectoring.trajectoryStrafe01 = vectoringFraction;
    out.poweredMainVectoring.trajectoryLift01 = vectoringFraction;
    out.poweredMainVectoring.pitch01 = vectoringFraction;
    out.poweredMainVectoring.yaw01 = vectoringFraction;
    out.poweredMainVectoring.roll01 = vectoringFraction;

    const double nominalForward =
        game::ship::forwardMainAccelerationLimitMps2(nominalPhysics);
    const double nominalReverse =
        game::ship::reverseMainAccelerationLimitMps2(nominalPhysics);

    // Coarse propulsive-utilization estimate for the velocity-control family.
    // Axial acceleration uses one longitudinal bank directly. Pure lateral
    // translation uses opposed fore/aft vectoring: their longitudinal
    // components cancel, so useful net lateral acceleration is only a fraction
    // of gross generated thrust. This is not engine efficiency.
    const double nominalLateral =
        std::max(0.0, static_cast<double>(nominalPhysics.strafeAccel));
    const double grossOpposedAuthority = nominalForward + nominalReverse;
    const double lateralUtilization =
        grossOpposedAuthority > 1.0e-9
            ? std::clamp(
                  nominalLateral / grossOpposedAuthority,
                  0.0,
                  1.0
              )
            : 0.0;

    out.thrustUtilization.axialTranslation01 = 1.0;
    out.thrustUtilization.lateralTranslation01 = lateralUtilization;
    out.thrustUtilization.cancelledDuringLateral01 =
        1.0 - lateralUtilization;

    out.effectivePhysics.forwardMainEngineAvailable =
        operationalAft > 0 && nominalForward > 0.0;
    out.effectivePhysics.reverseMainEngineAvailable =
        operationalFore > 0 && nominalReverse > 0.0;
    out.effectivePhysics.forwardMainEngineAccelerationMps2 =
        static_cast<float>(nominalForward * aftFraction);
    out.effectivePhysics.reverseMainEngineAccelerationMps2 =
        static_cast<float>(nominalReverse * foreFraction);

    // Assisted has no dedicated ordinary RCS in the new model. Translation
    // and attitude are produced by the vectoring main engines.
    out.effectivePhysics.manoeuvreThrusterAccel = 0.0f;
    out.effectivePhysics.strafeAccel =
        static_cast<float>(
            std::max(0.0, static_cast<double>(nominalPhysics.strafeAccel)) *
            vectoringFraction
        );
    out.effectivePhysics.angularAccel =
        static_cast<float>(
            std::max(0.0, static_cast<double>(nominalPhysics.angularAccel)) *
            vectoringFraction
        );

    if (out.operationalMainSlots == 0)
    {
        out.regime = RuntimeFlightRegime::Uncontrollable;
    }
    else if (operationalAft > 0 && operationalFore > 0)
    {
        out.regime =
            out.operationalMainSlots == out.mainSlots
                ? RuntimeFlightRegime::FullAssisted
                : RuntimeFlightRegime::DegradedAssisted;
    }
    else
    {
        // The hull remains an Assisted construction, but without one
        // longitudinal bank it must use thrust-axis / flip-and-burn doctrine.
        out.regime = RuntimeFlightRegime::ThrustAxisFallback;
    }

    out.valid = true;
    return out;
}

} // namespace game::ship::plant
