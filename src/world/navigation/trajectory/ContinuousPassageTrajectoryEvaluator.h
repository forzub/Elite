#pragma once

#include "AttitudeReachabilityEvaluator.h"
#include "OrientedPassageEvaluator.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace world::navigation
{

// Backend-neutral continuous verifier for one bounded static passage maneuver.
//
// Translation is an analytic cubic-Hermite segment and attitude follows the
// shortest rotation arc with a smooth rest-to-rest timing law. Geometry is not
// accepted from discrete pose samples alone: every interval receives a
// conservative bound for center-curve deviation and oriented-hull rotation.
// The result therefore proves the complete analytic segment remains inside the
// supplied static extruded passage cross-section when status == Feasible.
//
// This class verifies one candidate segment; it does not discover passages,
// own world/scene data, allocate thrusters, or replace the flight controller.
class ContinuousPassageTrajectoryEvaluator final
{
public:
    using Vec3d = OrientedPassageEvaluator::Vec3d;
    using Basis3d = OrientedPassageEvaluator::Basis3d;
    using HullProxy = OrientedPassageEvaluator::HullProxy;
    using Pose = OrientedPassageEvaluator::Pose;
    using Passage = OrientedPassageEvaluator::Passage;
    using AngularCapability = AttitudeReachabilityEvaluator::AngularCapability;

    static constexpr std::size_t kPoseSamples = 33;

    enum class ControlMode : std::uint8_t
    {
        EliteAssisted = 0,
        Newtonian
    };

    struct LinearCapability
    {
        double maxForwardAccelerationMetersPerSec2 = 0.0;
        double maxReverseAccelerationMetersPerSec2 = 0.0;
        double maxLateralAccelerationMetersPerSec2 = 0.0;
        double maxVerticalAccelerationMetersPerSec2 = 0.0;
    };

    struct State
    {
        Pose pose {};
        Vec3d linearVelocityMapMetersPerSec {};
    };

    struct Query
    {
        HullProxy hull {};
        Passage passage {};
        State start {};
        State end {};

        double durationSeconds = 0.0;
        LinearCapability linearCapability {};
        AngularCapability angularCapability {};
        ControlMode controlMode = ControlMode::Newtonian;

        // Assisted mode may intentionally couple travel direction to the hull.
        // This is a controller-policy limit, not extra physical thrust.
        // Newtonian mode ignores this field.
        double assistedMaxVelocityToForwardAngleRad =
            3.141592653589793238462643383279502884;
    };

    enum class Status : std::uint8_t
    {
        Feasible = 0,
        GeometryBlocked,
        LinearAuthorityExceeded,
        AngularAuthorityExceeded,
        AssistedSlipExceeded,
        InvalidInput
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        bool feasible = false;

        std::size_t samplesEvaluated = 0;
        std::size_t intervalsProven = 0;
        std::size_t firstFailureIndex = std::numeric_limits<std::size_t>::max();

        double orientationChangeRad = 0.0;
        double requiredPeakAngularSpeedRadPerSec = 0.0;
        double requiredPeakAngularAccelerationRadPerSec2 = 0.0;

        double requiredPeakForwardAccelerationMetersPerSec2 = 0.0;
        double requiredPeakReverseAccelerationMetersPerSec2 = 0.0;
        double requiredPeakLateralAccelerationMetersPerSec2 = 0.0;
        double requiredPeakVerticalAccelerationMetersPerSec2 = 0.0;

        double maximumObservedAssistedSlipAngleRad = 0.0;

        // Minimum sample clearance is diagnostic. The continuous bound is the
        // authoritative geometry margin after interval inflation.
        double minimumSampleClearanceMeters =
            std::numeric_limits<double>::infinity();
        double minimumContinuousClearanceBoundMeters =
            std::numeric_limits<double>::infinity();

        double maximumCenterCurveDeviationBoundMeters = 0.0;
        double maximumRotationalSweepInflationMeters = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Query& query) noexcept;
};

} // namespace world::navigation
