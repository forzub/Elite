#include "src/game/ship/plant/AssistedPhysicalControlModel.h"

#include <algorithm>

#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/plant/ShipPhysicalAuthorityMatrix.h"

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


AuthorityContributionRow assistedContributionRow(
    EngineSlotRole role
)
{
    AuthorityContributionRow row;

    if (role == EngineSlotRole::AftMain)
        row.thrust.longitudinal = {1.0, 0.0};
    else if (role == EngineSlotRole::ForeMain)
        row.thrust.longitudinal = {0.0, 1.0};

    const AxisAuthority01 symmetricVectoring {1.0, 1.0};
    row.vectoring.strafe = symmetricVectoring;
    row.vectoring.lift = symmetricVectoring;
    row.vectoring.pitch = symmetricVectoring;
    row.vectoring.yaw = symmetricVectoring;
    row.vectoring.roll = symmetricVectoring;

    row.poweredMainVectoring = {
        1.0, // trajectory strafe
        1.0, // trajectory lift
        1.0, // pitch
        1.0, // yaw
        1.0  // roll
    };

    return row;
}

std::vector<AuthorityContributionRow> makeAssistedContributionMatrix(
    const std::vector<EngineSlotDefinition>& slots
)
{
    std::vector<AuthorityContributionRow> rows;
    rows.reserve(slots.size());
    for (const auto& slot : slots)
        rows.push_back(assistedContributionRow(slot.role));
    return rows;
}

const std::vector<AuthorityContributionRow>& assistedContributionMatrix(
    PropulsionLayout layout
)
{
    static const auto I = makeAssistedContributionMatrix(AssistedI);
    static const auto Y = makeAssistedContributionMatrix(AssistedY);
    static const auto X = makeAssistedContributionMatrix(AssistedX);

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

double assistedVectoringFloor(
    const AuthorityMatrixResult& matrix
) noexcept
{
    return std::min({
        weakestAxisAuthority(matrix.authority.strafe),
        weakestAxisAuthority(matrix.authority.lift),
        weakestAxisAuthority(matrix.authority.pitch),
        weakestAxisAuthority(matrix.authority.yaw),
        weakestAxisAuthority(matrix.authority.roll),
        matrix.poweredMainVectoring.trajectoryStrafe01,
        matrix.poweredMainVectoring.trajectoryLift01,
        matrix.poweredMainVectoring.pitch01,
        matrix.poweredMainVectoring.yaw01,
        matrix.poweredMainVectoring.roll01
    });
}

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

    const auto matrix = evaluateAuthorityContributionMatrix(
        assistedContributionMatrix(layout),
        state
    );
    if (!matrix.valid)
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
            if (effectiveSlotThrust01(state[i]) > 1.0e-9)
                ++operationalAft;
        }
        else if (role == EngineSlotRole::ForeMain)
        {
            ++foreSlots;
            if (effectiveSlotThrust01(state[i]) > 1.0e-9)
                ++operationalFore;
        }
    }

    out.mainSlots = aftSlots + foreSlots;
    out.operationalMainSlots = operationalAft + operationalFore;
    out.authority = matrix.authority;
    out.poweredMainVectoring = matrix.poweredMainVectoring;

    if (!nominalPhysics.forwardMainEngineAvailable)
        out.authority.longitudinal.positive = 0.0;
    if (!nominalPhysics.reverseMainEngineAvailable)
        out.authority.longitudinal.negative = 0.0;

    const double aftFraction =
        out.authority.longitudinal.positive;
    const double foreFraction =
        out.authority.longitudinal.negative;
    const double vectoringFraction =
        assistedVectoringFloor(matrix);

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
    out.effectivePhysics.manoeuvreThrusterAuthorityExplicit = true;

    // Assisted translation uses main-nozzle vectoring, not the Newtonian RCS
    // working-fluid accumulator. Keep the accepted motion backend but make its
    // old gas bookkeeping inert for this physical family.
    out.effectivePhysics.manoeuvreGasUsePerSecond = 0.0f;
    out.effectivePhysics.manoeuvreGasRechargePerSecond = 0.0f;
    out.effectivePhysics.manoeuvreGasRestartFraction = 0.0f;

    // Transitional projection: the accepted motion backend still consumes one
    // scalar lateral/angular envelope. Use the weakest matrix axis so damage
    // can only remove capability. Healthy matrices normalize to exactly 1.0,
    // preserving the pre-matrix flight response.
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
        const bool nominalAuthority =
            aftFraction >= 1.0 - 1.0e-9 &&
            foreFraction >= 1.0 - 1.0e-9 &&
            vectoringFraction >= 1.0 - 1.0e-9;

        out.regime =
            nominalAuthority
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
