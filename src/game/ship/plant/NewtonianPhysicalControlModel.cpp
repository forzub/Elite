#include "src/game/ship/plant/NewtonianPhysicalControlModel.h"

#include <algorithm>

#include "src/game/ship/core/ShipDynamics.h"

namespace game::ship::plant
{
namespace
{

constexpr double Sqrt3Over2 = 0.86602540378443864676;
constexpr double InvSqrt2 = 0.70710678118654752440;

const std::vector<EngineSlotDefinition> NewtonianI = {
    {"main_center", EngineSlotRole::AftMain, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_left",  EngineSlotRole::ManoeuvreCluster, {-1.0, 0.0, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_right", EngineSlotRole::ManoeuvreCluster, { 1.0, 0.0, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_left",   EngineSlotRole::ManoeuvreCluster, {-1.0, 0.0,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_right",  EngineSlotRole::ManoeuvreCluster, { 1.0, 0.0,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

const std::vector<EngineSlotDefinition> NewtonianY = {
    {"main_top",         EngineSlotRole::AftMain, {0.0,          1.0, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_left",  EngineSlotRole::AftMain, {-Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_right", EngineSlotRole::AftMain, { Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_top",         EngineSlotRole::ManoeuvreCluster, {0.0,          1.0, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_left",  EngineSlotRole::ManoeuvreCluster, {-Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_right", EngineSlotRole::ManoeuvreCluster, { Sqrt3Over2, -0.5, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_top",          EngineSlotRole::ManoeuvreCluster, {0.0,          1.0,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_left",   EngineSlotRole::ManoeuvreCluster, {-Sqrt3Over2, -0.5,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_right",  EngineSlotRole::ManoeuvreCluster, { Sqrt3Over2, -0.5,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

const std::vector<EngineSlotDefinition> NewtonianX = {
    {"main_upper_left",  EngineSlotRole::AftMain, {-InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_upper_right", EngineSlotRole::AftMain, { InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_left",  EngineSlotRole::AftMain, {-InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_right", EngineSlotRole::AftMain, { InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_upper_left",  EngineSlotRole::ManoeuvreCluster, {-InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_upper_right", EngineSlotRole::ManoeuvreCluster, { InvSqrt2,  InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_left",  EngineSlotRole::ManoeuvreCluster, {-InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_right", EngineSlotRole::ManoeuvreCluster, { InvSqrt2, -InvSqrt2, -1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_upper_left",   EngineSlotRole::ManoeuvreCluster, {-InvSqrt2,  InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_upper_right",  EngineSlotRole::ManoeuvreCluster, { InvSqrt2,  InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_left",   EngineSlotRole::ManoeuvreCluster, {-InvSqrt2, -InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_right",  EngineSlotRole::ManoeuvreCluster, { InvSqrt2, -InvSqrt2,  1.0}, {0.0, 0.0, 0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

PhysicalControlModelId modelId(PropulsionLayout layout) noexcept
{
    switch (layout)
    {
        case PropulsionLayout::Y:
            return PhysicalControlModelId::NewtonianY;
        case PropulsionLayout::X:
            return PhysicalControlModelId::NewtonianX;
        case PropulsionLayout::I:
        default:
            return PhysicalControlModelId::NewtonianI;
    }
}

AxisAuthority01 symmetric(double authority) noexcept
{
    const double a = std::clamp(authority, 0.0, 1.0);
    return {a, a};
}

} // namespace

const std::vector<EngineSlotDefinition>& NewtonianPhysicalControlModel::slots(
    PropulsionLayout layout
)
{
    switch (layout)
    {
        case PropulsionLayout::Y:
            return NewtonianY;
        case PropulsionLayout::X:
            return NewtonianX;
        case PropulsionLayout::I:
        default:
            return NewtonianI;
    }
}

PhysicalControlSnapshot NewtonianPhysicalControlModel::evaluate(
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

    std::size_t mainSlots = 0;
    std::size_t manoeuvreSlots = 0;
    std::size_t operationalMain = 0;
    std::size_t operationalManoeuvre = 0;
    double mainThrustSum01 = 0.0;
    double mainVectoringSum01 = 0.0;
    double manoeuvreThrustSum01 = 0.0;

    for (std::size_t i = 0; i < definition.size(); ++i)
    {
        if (definition[i].role == EngineSlotRole::AftMain)
        {
            ++mainSlots;
            mainThrustSum01 += effectiveSlotThrust01(state[i]);
            mainVectoringSum01 += effectiveSlotVectoring01(state[i]);
            if (effectiveSlotThrust01(state[i]) > 1.0e-9)
                ++operationalMain;
        }
        else if (definition[i].role == EngineSlotRole::ManoeuvreCluster)
        {
            ++manoeuvreSlots;
            manoeuvreThrustSum01 += effectiveSlotThrust01(state[i]);
            if (effectiveSlotThrust01(state[i]) > 1.0e-9)
                ++operationalManoeuvre;
        }
    }

    out.mainSlots = mainSlots;
    out.manoeuvreSlots = manoeuvreSlots;
    out.operationalMainSlots = operationalMain;
    out.operationalManoeuvreSlots = operationalManoeuvre;

    const double mainFraction =
        mainSlots > 0
            ? mainThrustSum01 / static_cast<double>(mainSlots)
            : 0.0;
    const double mainVectoringFraction =
        mainSlots > 0
            ? mainVectoringSum01 / static_cast<double>(mainSlots)
            : 0.0;
    const double manoeuvreFraction =
        manoeuvreSlots > 0
            ? manoeuvreThrustSum01 / static_cast<double>(manoeuvreSlots)
            : 0.0;

    const double nominalForward =
        game::ship::forwardMainAccelerationLimitMps2(nominalPhysics);
    const double nominalRcs =
        game::ship::manoeuvreAccelerationLimitMps2(nominalPhysics);

    out.authority.longitudinal.positive =
        nominalPhysics.forwardMainEngineAvailable ? mainFraction : 0.0;

    // Newtonian has no fore main bank. Small negative/forward-axis correction
    // may still be produced by its RCS clusters.
    out.authority.longitudinal.negative =
        nominalForward > 1.0e-9
            ? std::clamp(
                  (nominalRcs * manoeuvreFraction) / nominalForward,
                  0.0,
                  1.0
              )
            : manoeuvreFraction;

    out.authority.strafe = symmetric(manoeuvreFraction);
    out.authority.lift = symmetric(manoeuvreFraction);
    out.authority.pitch = symmetric(manoeuvreFraction);
    out.authority.yaw = symmetric(manoeuvreFraction);
    out.authority.roll = symmetric(manoeuvreFraction);

    // Newtonian main nozzles are thrust-vectoring as well. Unlike Assisted,
    // this authority is coupled to an aft-main burn rather than being a
    // high-authority independent velocity-control channel.
    out.poweredMainVectoring.trajectoryStrafe01 = mainVectoringFraction;
    out.poweredMainVectoring.trajectoryLift01 = mainVectoringFraction;
    out.poweredMainVectoring.pitch01 = mainVectoringFraction;
    out.poweredMainVectoring.yaw01 = mainVectoringFraction;
    out.poweredMainVectoring.roll01 =
        layout == PropulsionLayout::I ? 0.0 : mainVectoringFraction;

    // TVC redirects the main thrust instead of cancelling an opposed bank.
    // Resource accounting later must model the axial/lateral split from the
    // commanded vector angle; there is no Assisted-style cancellation penalty.
    out.thrustUtilization.axialTranslation01 = 1.0;
    out.thrustUtilization.lateralTranslation01 = 1.0;
    out.thrustUtilization.cancelledDuringLateral01 = 0.0;

    out.effectivePhysics.forwardMainEngineAvailable =
        operationalMain > 0 && nominalForward > 0.0;
    out.effectivePhysics.forwardMainEngineAccelerationMps2 =
        static_cast<float>(nominalForward * mainFraction);

    // TVC is real main-engine hardware. Damage to thrust/vectoring authority
    // continuously shrinks both travel and slew instead of turning into a
    // hidden lateral/RCS budget.
    out.effectivePhysics.mainGimbalPitchLimitRad =
        static_cast<float>(
            std::max(
                0.0,
                static_cast<double>(nominalPhysics.mainGimbalPitchLimitRad)
            ) * mainVectoringFraction
        );
    out.effectivePhysics.mainGimbalYawLimitRad =
        static_cast<float>(
            std::max(
                0.0,
                static_cast<double>(nominalPhysics.mainGimbalYawLimitRad)
            ) * mainVectoringFraction
        );
    out.effectivePhysics.mainGimbalRateRadPerSec =
        static_cast<float>(
            std::max(
                0.0,
                static_cast<double>(nominalPhysics.mainGimbalRateRadPerSec)
            ) * mainVectoringFraction
        );

    // This is the defining physical distinction from Assisted.
    out.effectivePhysics.reverseMainEngineAvailable = false;
    out.effectivePhysics.reverseMainEngineAccelerationMps2 = 0.0f;

    out.effectivePhysics.manoeuvreThrusterAccel =
        static_cast<float>(nominalRcs * manoeuvreFraction);
    out.effectivePhysics.strafeAccel =
        static_cast<float>(
            std::max(0.0, static_cast<double>(nominalPhysics.strafeAccel)) *
            manoeuvreFraction
        );
    out.effectivePhysics.angularAccel =
        static_cast<float>(
            std::max(0.0, static_cast<double>(nominalPhysics.angularAccel)) *
            manoeuvreFraction
        );

    if (operationalMain == 0 && operationalManoeuvre == 0)
    {
        out.regime = RuntimeFlightRegime::Uncontrollable;
    }
    else if (operationalMain == 0 || operationalManoeuvre == 0)
    {
        out.regime = RuntimeFlightRegime::SurvivalControl;
    }
    else if (
        mainFraction >= 1.0 - 1.0e-9 &&
        mainVectoringFraction >= 1.0 - 1.0e-9 &&
        manoeuvreFraction >= 1.0 - 1.0e-9
    )
    {
        out.regime = RuntimeFlightRegime::FullNewtonian;
    }
    else
    {
        out.regime = RuntimeFlightRegime::DegradedNewtonian;
    }

    out.valid = true;
    return out;
}

} // namespace game::ship::plant
