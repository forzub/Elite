#pragma once

#include "ContinuousPassageTrajectoryEvaluator.h"
#include "DockingTerminalEvaluator.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace world::navigation
{

// Verifies one final precision docking segment in the moving/rotating dock
// frame. Continuous corridor geometry is delegated to the already accepted
// ContinuousPassageTrajectoryEvaluator in dock-local coordinates; inertial
// vehicle authority is then checked explicitly in world coordinates.
class DockingApproachEvaluator final
{
public:
    using GeometryEvaluator = ContinuousPassageTrajectoryEvaluator;
    using TerminalEvaluator = DockingTerminalEvaluator;

    using Vec3d = GeometryEvaluator::Vec3d;
    using Basis3d = GeometryEvaluator::Basis3d;
    using HullProxy = GeometryEvaluator::HullProxy;
    using Pose = GeometryEvaluator::Pose;
    using State = GeometryEvaluator::State;
    using LinearCapability = GeometryEvaluator::LinearCapability;
    using AngularCapability = GeometryEvaluator::AngularCapability;
    using ControlMode = GeometryEvaluator::ControlMode;

    static constexpr std::size_t kPoseSamples = GeometryEvaluator::kPoseSamples;
    static constexpr std::size_t kIntervals = kPoseSamples - 1;

    struct Corridor
    {
        double halfWidthMeters = 0.0;
        double halfHeightMeters = 0.0;
    };

    struct Query
    {
        HullProxy hull {};
        State start {};
        State end {};
        double durationSeconds = 0.0;

        LinearCapability linearCapability {};
        AngularCapability angularCapability {};
        ControlMode controlMode = ControlMode::Newtonian;
        double assistedMaxVelocityToForwardAngleRad =
            3.141592653589793238462643383279502884;

        TerminalEvaluator::LocalPortFrame shipPort {};
        TerminalEvaluator::MovingDockState dock {};
        TerminalEvaluator::LocalPortFrame dockPort {};
        TerminalEvaluator::MatingContract terminalContract {};

        Corridor corridor {};
    };

    enum class Status : std::uint8_t
    {
        FeasibleForCapture = 0,
        CorridorBlocked,
        LinearAuthorityExceeded,
        AngularAuthorityExceeded,
        AssistedSlipExceeded,
        TerminalNotCapturable,
        InvalidInput
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        bool feasible = false;

        std::size_t samplesEvaluated = 0;
        std::size_t intervalsProven = 0;
        std::size_t firstFailureIndex = std::numeric_limits<std::size_t>::max();

        double relativeOrientationChangeRad = 0.0;
        double requiredPeakRelativeAngularSpeedRadPerSec = 0.0;
        double requiredPeakRelativeAngularAccelerationRadPerSec2 = 0.0;
        double requiredPeakWorldAngularSpeedBoundRadPerSec = 0.0;
        double requiredPeakWorldAngularAccelerationBoundRadPerSec2 = 0.0;

        double requiredPeakForwardAccelerationMetersPerSec2 = 0.0;
        double requiredPeakReverseAccelerationMetersPerSec2 = 0.0;
        double requiredPeakLateralAccelerationMetersPerSec2 = 0.0;
        double requiredPeakVerticalAccelerationMetersPerSec2 = 0.0;
        double maximumObservedAssistedSlipAngleRad = 0.0;

        double minimumSampleCorridorClearanceMeters =
            std::numeric_limits<double>::infinity();
        double minimumContinuousCorridorClearanceMeters =
            std::numeric_limits<double>::infinity();

        double maximumWorldAccelerationMagnitudeMetersPerSec2 = 0.0;

        GeometryEvaluator::Result dockLocalGeometry {};
        TerminalEvaluator::Result terminal {};
    };

    [[nodiscard]] static Result evaluate(const Query& query) noexcept;
};

} // namespace world::navigation
