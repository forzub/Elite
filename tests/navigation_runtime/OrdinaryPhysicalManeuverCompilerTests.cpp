#include "src/game/navigation/OrdinaryPhysicalManeuverCompiler.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Compiler = game::navigation::OrdinaryPhysicalManeuverCompiler;
using Candidate = game::navigation::OrdinaryPhysicalManeuverCandidate;
using Law = game::navigation::LocalFlightControlLaw;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Compiler::Query baseQuery()
{
    Compiler::Query q;
    q.controlLaw = Law::Newtonian;

    q.state.positionMapMeters = {0.0, 0.0, 0.0};
    q.state.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    q.state.forwardMap = {0.0, 0.0, -1.0};
    q.state.rightMap = {1.0, 0.0, 0.0};
    q.state.upMap = {0.0, 1.0, 0.0};
    q.state.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};

    q.capability.maxForwardAccelerationMps2 = 73.5;
    q.capability.maxReverseAccelerationMps2 = 2.0;
    q.capability.maxLateralAccelerationMps2 = 2.0;
    q.capability.maxVerticalAccelerationMps2 = 2.0;
    q.capability.maxAngularAccelerationRadPerSec2 = 1.5;
    q.capability.maxAngularSpeedRadPerSec = 1.0;

    q.geometricTargetPositionMapMeters = {0.0, 0.0, -1000.0};
    q.desiredVelocityMapMetersPerSecond = {0.0, 0.0, -20.0};
    q.velocityResponsePerSecond = 0.75;

    q.linearFeedbackReserveMps2 = 0.5;
    q.angularFeedbackReserveRadPerSec2 = 0.1;
    q.controlResponseReserveSeconds = 0.18;
    q.maximumProgramSeconds = 4.0;
    return q;
}

const Candidate* findFamily(
    const Compiler::Result& result,
    Candidate::Family family
)
{
    for (std::size_t i = 0; i < result.candidateCount; ++i)
    {
        if (result.candidates[i].family == family)
            return &result.candidates[i];
    }
    return nullptr;
}

void testForwardRequestCompilesAsDirectTrim()
{
    auto q = baseQuery();

    const auto result = Compiler::compile(q);
    require(
        result.status == Compiler::Status::Compiled,
        "forward Newtonian request must compile"
    );
    require(
        result.directBodyAxisFeasible,
        "forward request should fit current body-axis authority"
    );

    const Candidate* trim =
        findFamily(result, Candidate::Family::Trim);
    require(trim && trim->valid, "direct trim candidate missing");
    require(trim->requiresContinuousProof,
            "B5 candidate must still require B6 proof");

    require(
        trim->required.peakForwardAccelerationMps2 <=
            q.capability.maxForwardAccelerationMps2 -
                q.linearFeedbackReserveMps2 + 1.0e-9,
        "direct trim consumed reserved forward authority"
    );

    const auto& last =
        trim->samples[trim->sampleCount - 1];
    require(
        last.velocityMapMetersPerSecond.z < -1.0,
        "direct trim did not produce forward progress"
    );
    requireNear(
        glm::dot(last.forwardMap, glm::dvec3(0.0, 0.0, -1.0)),
        1.0,
        1.0e-9,
        "direct trim changed attitude unnecessarily"
    );
}

void testLargeLateralDeltaVRequiresLeadRotateMainBurn()
{
    auto q = baseQuery();
    q.geometricTargetPositionMapMeters = {0.0, -700.0, 300.0};
    q.desiredVelocityMapMetersPerSecond = {0.0, -54.8, 24.5};

    const auto result = Compiler::compile(q);
    require(
        result.status == Compiler::Status::Compiled,
        "large lateral Newtonian delta-v must produce a physical candidate"
    );
    require(
        !result.directBodyAxisFeasible,
        "41 m/s2-class lateral request must not be labeled direct-feasible with 2 m/s2 RCS"
    );
    require(
        result.leadRotateRequired,
        "large lateral request must require lead rotation"
    );

    const Candidate* rotateBurn =
        findFamily(result, Candidate::Family::LeadRotateMainBurn);
    require(rotateBurn && rotateBurn->valid,
            "lead-rotate/main-burn candidate missing");

    const double forwardAvailable =
        q.capability.maxForwardAccelerationMps2 -
        q.linearFeedbackReserveMps2;
    const double angularAvailable =
        q.capability.maxAngularAccelerationRadPerSec2 -
        q.angularFeedbackReserveRadPerSec2;

    require(
        rotateBurn->required.peakForwardAccelerationMps2 <=
            forwardAvailable + 1.0e-6,
        "lead-rotate burn exceeded reserved forward authority"
    );
    require(
        rotateBurn->required.peakLateralAccelerationMps2 <= 1.0e-9,
        "lead-rotate candidate used lateral feed-forward instead of main engine"
    );
    require(
        rotateBurn->required.peakAngularAccelerationRadPerSec2 <=
            angularAvailable + 1.0e-6,
        "lead rotation exceeded reserved angular acceleration"
    );
    require(
        rotateBurn->required.peakAngularSpeedRadPerSec <=
            q.capability.maxAngularSpeedRadPerSec + 1.0e-6,
        "lead rotation exceeded angular speed"
    );

    const glm::dvec3 desiredDirection =
        glm::normalize(
            q.desiredVelocityMapMetersPerSecond -
            q.state.velocityMapMetersPerSecond
        );
    const auto& last =
        rotateBurn->samples[rotateBurn->sampleCount - 1];

    require(
        glm::dot(last.forwardMap, desiredDirection) > 0.999,
        "lead rotation did not finish aligned with main-burn direction"
    );

    const double initialVelocityError =
        glm::length(
            q.desiredVelocityMapMetersPerSecond -
            q.state.velocityMapMetersPerSecond
        );
    const double finalVelocityError =
        glm::length(
            q.desiredVelocityMapMetersPerSecond -
            last.velocityMapMetersPerSecond
        );
    require(
        finalVelocityError < initialVelocityError,
        "lead-rotate/main-burn primitive did not reduce delta-v"
    );

    bool sawRotationWithoutMainBurn = false;
    bool sawMainBurn = false;
    for (std::size_t i = 0; i < rotateBurn->sampleCount; ++i)
    {
        const auto& sample = rotateBurn->samples[i];
        const double alpha =
            glm::length(
                sample.angularAccelerationFeedForwardMapRadPerSec2
            );
        const double accel =
            glm::length(
                sample.linearAccelerationFeedForwardMapMps2
            );

        sawRotationWithoutMainBurn =
            sawRotationWithoutMainBurn ||
            (alpha > 1.0e-4 && accel < 1.0e-6);
        sawMainBurn = sawMainBurn || accel > 1.0;
    }

    require(
        sawRotationWithoutMainBurn,
        "candidate skipped the lead-rotation phase"
    );
    require(
        sawMainBurn,
        "candidate never entered main-engine burn phase"
    );
}

void testNoAngularAuthorityDoesNotFallBackToImpossibleLateralDemand()
{
    auto q = baseQuery();
    q.desiredVelocityMapMetersPerSecond = {0.0, -40.0, 0.0};
    q.geometricTargetPositionMapMeters = {0.0, -500.0, 0.0};
    q.capability.maxAngularAccelerationRadPerSec2 = 0.0;
    q.capability.maxAngularSpeedRadPerSec = 0.0;

    const auto result = Compiler::compile(q);
    require(
        result.status == Compiler::Status::NoPhysicalCandidate,
        "without angular authority B5 must reject an impossible lateral delta-v"
    );
    require(
        result.candidateCount == 0,
        "impossible request leaked a maneuver candidate"
    );
}

void testFeedbackReserveCanMakeMarginalDirectDemandInfeasible()
{
    auto q = baseQuery();
    q.capability.maxLateralAccelerationMps2 = 2.0;
    q.linearFeedbackReserveMps2 = 0.5;

    // 2.0 m/s2 desired lateral acceleration: physically within raw RCS, but
    // not within the 1.5 m/s2 feed-forward authority left after reserve.
    q.desiredVelocityMapMetersPerSecond = {2.0 / 0.75, 0.0, 0.0};
    q.geometricTargetPositionMapMeters = {100.0, 0.0, 0.0};

    const auto result = Compiler::compile(q);
    require(
        result.status == Compiler::Status::Compiled,
        "reserve-limited request should still have a rotate/main-burn option"
    );
    require(
        !result.directBodyAxisFeasible,
        "B5 ignored B10 linear feedback reserve"
    );
    require(
        findFamily(result, Candidate::Family::LeadRotateMainBurn) != nullptr,
        "reserve-limited direct request did not escalate to main-engine maneuver"
    );
}

void testAssistedIsExplicitlyUnsupportedInFirstB5Slice()
{
    auto q = baseQuery();
    q.controlLaw = Law::Assisted;

    const auto result = Compiler::compile(q);
    require(
        result.status == Compiler::Status::UnsupportedControlLaw,
        "first B5 slice must not pretend Newtonian compiler is Assisted"
    );
    require(
        result.candidateCount == 0,
        "unsupported Assisted law leaked Newtonian candidate"
    );
}

void testFixtureLikeSeventyFiveDegreeDemandIsNotAcceptedAsOmnidirectional()
{
    auto q = baseQuery();

    // Reconstruct the live failure class: current hull is not aligned with the
    // steep visibility delta-v, RCS is ~2 m/s2, main engine is ~73.5 m/s2.
    q.state.forwardMap = {-0.151819, 0.18446, 0.971044};
    q.state.upMap = {0.0, 1.0, 0.0};
    q.state.rightMap =
        glm::normalize(
            glm::cross(q.state.forwardMap, q.state.upMap)
        );
    q.state.upMap =
        glm::normalize(
            glm::cross(q.state.rightMap, q.state.forwardMap)
        );

    q.desiredVelocityMapMetersPerSecond =
        {-3.10856e-05, -54.7735, 24.4921};
    q.geometricTargetPositionMapMeters =
        {0.0, -693.34, 310.03};

    const auto result = Compiler::compile(q);
    require(
        result.status == Compiler::Status::Compiled,
        "fixture-like bypass request should compile to a bounded physical primitive"
    );
    require(
        !result.directBodyAxisFeasible,
        "fixture-like bypass was incorrectly treated as omnidirectional acceleration"
    );

    const Candidate* rotateBurn =
        findFamily(result, Candidate::Family::LeadRotateMainBurn);
    require(
        rotateBurn != nullptr,
        "fixture-like bypass did not produce lead-rotate/main-burn"
    );

    for (std::size_t i = 0; i < rotateBurn->sampleCount; ++i)
    {
        const auto& sample = rotateBurn->samples[i];
        const glm::dvec3 a =
            sample.linearAccelerationFeedForwardMapMps2;

        if (glm::length(a) <= 1.0e-9)
            continue;

        const double alongForward =
            glm::dot(a, sample.forwardMap);
        const glm::dvec3 lateral =
            a - sample.forwardMap * alongForward;

        require(
            glm::length(lateral) <= 1.0e-5,
            "compiled main-burn feed-forward leaked material lateral acceleration"
        );
    }
}

} // namespace

int main()
{
    try
    {
        testForwardRequestCompilesAsDirectTrim();
        testLargeLateralDeltaVRequiresLeadRotateMainBurn();
        testNoAngularAuthorityDoesNotFallBackToImpossibleLateralDemand();
        testFeedbackReserveCanMakeMarginalDirectDemandInfeasible();
        testAssistedIsExplicitlyUnsupportedInFirstB5Slice();
        testFixtureLikeSeventyFiveDegreeDemandIsNotAcceptedAsOmnidirectional();

        std::cout << "ORDINARY PHYSICAL MANEUVER COMPILER TESTS: PASS\n";
        std::cout << " - direct body-axis demand stays direct only when authority permits\n";
        std::cout << " - material Newtonian lateral delta-v becomes lead-rotate + main burn\n";
        std::cout << " - B10 reserve is removed before B5 feed-forward authority\n";
        std::cout << " - missing angular authority fails closed instead of inventing lateral thrust\n";
        std::cout << " - Assisted remains explicit unsupported work, not fake Newtonian behavior\n";
        std::cout << " - live 75-degree failure class compiles without omnidirectional main thrust\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "ORDINARY PHYSICAL MANEUVER COMPILER TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
