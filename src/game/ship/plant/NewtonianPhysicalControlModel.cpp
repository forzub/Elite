#include "src/game/ship/plant/NewtonianPhysicalControlModel.h"

#include <algorithm>

#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/plant/ShipPhysicalAuthorityMatrix.h"

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


AuthorityContributionRow newtonianMainContributionRow(
    bool supportsPoweredRoll
)
{
    AuthorityContributionRow row;
    row.thrust.longitudinal = {1.0, 0.0};

    row.poweredMainVectoring = {
        1.0, // trajectory strafe
        1.0, // trajectory lift
        1.0, // pitch
        1.0, // yaw
        supportsPoweredRoll ? 1.0 : 0.0
    };
    return row;
}

AuthorityContributionRow newtonianRcsContributionRow()
{
    AuthorityContributionRow row;

    // RCS can trim all three translation axes and all attitude axes. Negative
    // longitudinal authority is rescaled later to the real RCS/main ratio so
    // it never masquerades as a reverse main engine.
    const AxisAuthority01 symmetric {1.0, 1.0};
    row.thrust.longitudinal = {0.0, 1.0};
    row.thrust.strafe = symmetric;
    row.thrust.lift = symmetric;
    row.thrust.pitch = symmetric;
    row.thrust.yaw = symmetric;
    row.thrust.roll = symmetric;
    return row;
}

std::vector<AuthorityContributionRow> makeNewtonianContributionMatrix(
    const std::vector<EngineSlotDefinition>& slots,
    bool supportsPoweredRoll
)
{
    std::vector<AuthorityContributionRow> rows;
    rows.reserve(slots.size());

    for (const auto& slot : slots)
    {
        if (slot.role == EngineSlotRole::AftMain)
            rows.push_back(newtonianMainContributionRow(supportsPoweredRoll));
        else
            rows.push_back(newtonianRcsContributionRow());
    }

    return rows;
}

const std::vector<AuthorityContributionRow>& newtonianContributionMatrix(
    PropulsionLayout layout
)
{
    static const auto I =
        makeNewtonianContributionMatrix(NewtonianI, false);
    static const auto Y =
        makeNewtonianContributionMatrix(NewtonianY, true);
    static const auto X =
        makeNewtonianContributionMatrix(NewtonianX, true);

    switch (layout)
    {
        case PropulsionLayout::Y:
            return Y;
        case PropulsionLayout::X:
            return X;
        case PropulsionLayout::I:
        default:
            return I;
    }
}

double newtonianRcsFloor(
    const AuthorityMatrixResult& matrix
) noexcept
{
    return std::min({
        weakestAxisAuthority(matrix.authority.strafe),
        weakestAxisAuthority(matrix.authority.lift),
        weakestAxisAuthority(matrix.authority.pitch),
        weakestAxisAuthority(matrix.authority.yaw),
        weakestAxisAuthority(matrix.authority.roll)
    });
}

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

    const auto matrix = evaluateAuthorityContributionMatrix(
        newtonianContributionMatrix(layout),
        state
    );
    if (!matrix.valid)
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
            if (effectiveSlotThrust01(state[i]) > 1.0e-9)
                ++operationalMain;
        }
        else if (definition[i].role == EngineSlotRole::ManoeuvreCluster)
        {
            ++manoeuvreSlots;
            if (effectiveSlotThrust01(state[i]) > 1.0e-9)
                ++operationalManoeuvre;
        }
    }

    out.mainSlots = mainSlots;
    out.manoeuvreSlots = manoeuvreSlots;
    out.operationalMainSlots = operationalMain;
    out.operationalManoeuvreSlots = operationalManoeuvre;
    out.authority = matrix.authority;
    out.poweredMainVectoring = matrix.poweredMainVectoring;

    const double nominalForward =
        game::ship::forwardMainAccelerationLimitMps2(nominalPhysics);
    const double nominalRcs =
        game::ship::manoeuvreAccelerationLimitMps2(nominalPhysics);

    if (!nominalPhysics.forwardMainEngineAvailable)
        out.authority.longitudinal.positive = 0.0;

    const double mainFraction =
        out.authority.longitudinal.positive;
    const double manoeuvreFraction =
        newtonianRcsFloor(matrix);
    const double mainVectoringFraction =
        std::min({
            matrix.poweredMainVectoring.trajectoryStrafe01,
            matrix.poweredMainVectoring.trajectoryLift01,
            matrix.poweredMainVectoring.pitch01,
            matrix.poweredMainVectoring.yaw01
        });

    // Newtonian has no fore main bank. Small negative/forward-axis correction
    // may still be produced by its RCS clusters. Preserve the historical
    // capability convention by expressing that trim relative to the main
    // longitudinal authority rather than inventing a reverse main engine.
    out.authority.longitudinal.negative =
        nominalForward > 1.0e-9
            ? std::clamp(
                  (nominalRcs * manoeuvreFraction) / nominalForward,
                  0.0,
                  1.0
              )
            : manoeuvreFraction;

    // TVC redirects the main thrust instead of cancelling an opposed bank.
    // Resource accounting later may model the axial/lateral split from the
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
    out.effectivePhysics.manoeuvreThrusterAuthorityExplicit = true;
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
