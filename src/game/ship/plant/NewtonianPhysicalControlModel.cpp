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

    {"rcs_fore_left",  EngineSlotRole::ManoeuvreCluster, {-1.0, 0.0, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_right", EngineSlotRole::ManoeuvreCluster, { 1.0, 0.0, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_left",   EngineSlotRole::ManoeuvreCluster, {-1.0, 0.0,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_right",  EngineSlotRole::ManoeuvreCluster, { 1.0, 0.0,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

const std::vector<EngineSlotDefinition> NewtonianY = {
    {"main_top",         EngineSlotRole::AftMain, {0.0,          1.0, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_left",  EngineSlotRole::AftMain, {-Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_right", EngineSlotRole::AftMain, { Sqrt3Over2, -0.5, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_top",         EngineSlotRole::ManoeuvreCluster, {0.0,          1.0, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_left",  EngineSlotRole::ManoeuvreCluster, {-Sqrt3Over2, -0.5, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_right", EngineSlotRole::ManoeuvreCluster, { Sqrt3Over2, -0.5, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_top",          EngineSlotRole::ManoeuvreCluster, {0.0,          1.0,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_left",   EngineSlotRole::ManoeuvreCluster, {-Sqrt3Over2, -0.5,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_right",  EngineSlotRole::ManoeuvreCluster, { Sqrt3Over2, -0.5,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
};

const std::vector<EngineSlotDefinition> NewtonianX = {
    {"main_upper_left",  EngineSlotRole::AftMain, {-InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_upper_right", EngineSlotRole::AftMain, { InvSqrt2,  InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_left",  EngineSlotRole::AftMain, {-InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},
    {"main_lower_right", EngineSlotRole::AftMain, { InvSqrt2, -InvSqrt2, 1.0}, {0.0, 0.0, -1.0}, EngineSlotFreedom::VectoringMain},

    {"rcs_fore_upper_left",  EngineSlotRole::ManoeuvreCluster, {-InvSqrt2,  InvSqrt2, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_upper_right", EngineSlotRole::ManoeuvreCluster, { InvSqrt2,  InvSqrt2, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_left",  EngineSlotRole::ManoeuvreCluster, {-InvSqrt2, -InvSqrt2, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_fore_lower_right", EngineSlotRole::ManoeuvreCluster, { InvSqrt2, -InvSqrt2, -1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_upper_left",   EngineSlotRole::ManoeuvreCluster, {-InvSqrt2,  InvSqrt2,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_upper_right",  EngineSlotRole::ManoeuvreCluster, { InvSqrt2,  InvSqrt2,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_left",   EngineSlotRole::ManoeuvreCluster, {-InvSqrt2, -InvSqrt2,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
    {"rcs_aft_lower_right",  EngineSlotRole::ManoeuvreCluster, { InvSqrt2, -InvSqrt2,  1.0}, {0.0}, EngineSlotFreedom::OmnidirectionalCluster},
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

    for (std::size_t i = 0; i < definition.size(); ++i)
    {
        if (definition[i].role == EngineSlotRole::AftMain)
        {
            ++mainSlots;
            if (state[i].operational)
                ++operationalMain;
        }
        else if (definition[i].role == EngineSlotRole::ManoeuvreCluster)
        {
            ++manoeuvreSlots;
            if (state[i].operational)
                ++operationalManoeuvre;
        }
    }

    out.mainSlots = mainSlots;
    out.manoeuvreSlots = manoeuvreSlots;
    out.operationalMainSlots = operationalMain;
    out.operationalManoeuvreSlots = operationalManoeuvre;

    const double mainFraction = fraction(operationalMain, mainSlots);
    const double manoeuvreFraction =
        fraction(operationalManoeuvre, manoeuvreSlots);

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
    out.poweredMainVectoring.trajectoryStrafe01 = mainFraction;
    out.poweredMainVectoring.trajectoryLift01 = mainFraction;
    out.poweredMainVectoring.pitch01 = mainFraction;
    out.poweredMainVectoring.yaw01 = mainFraction;
    out.poweredMainVectoring.roll01 =
        layout == PropulsionLayout::I ? 0.0 : mainFraction;

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
        operationalMain == mainSlots &&
        operationalManoeuvre == manoeuvreSlots
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
