#pragma once

#include "ContinuousPassageTrajectoryEvaluator.h"
#include "MovingGapPredictor.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace world::navigation
{

// Verifies one bounded ship maneuver against one already-predicted moving gap.
//
// Pair discovery and boundary prediction stay upstream in BoundedGapCandidateBuilder
// and MovingGapPredictor. Exact CCD/contact response stays downstream in physics.
// This evaluator proves that the supplied analytic ship segment remains inside
// the time-varying oriented passage while respecting the same declared vehicle
// authority semantics as the accepted static continuous verifier.
class MovingPassageTrajectoryEvaluator final
{
public:
    using StaticEvaluator = ContinuousPassageTrajectoryEvaluator;
    using Vec3d = StaticEvaluator::Vec3d;
    using Basis3d = StaticEvaluator::Basis3d;
    using HullProxy = StaticEvaluator::HullProxy;
    using Pose = StaticEvaluator::Pose;
    using State = StaticEvaluator::State;
    using LinearCapability = StaticEvaluator::LinearCapability;
    using AngularCapability = StaticEvaluator::AngularCapability;
    using ControlMode = StaticEvaluator::ControlMode;
    using MovingGap = MovingGapPredictor::Result;

    static constexpr std::size_t kPoseSamples = MovingGapPredictor::kSamples;
    static constexpr std::size_t kIntervals = MovingGapPredictor::kIntervals;

    struct Query
    {
        HullProxy hull {};
        const MovingGap* movingGap = nullptr;
        State start {};
        State end {};
        double durationSeconds = 0.0;
        LinearCapability linearCapability {};
        AngularCapability angularCapability {};
        ControlMode controlMode = ControlMode::Newtonian;
        double assistedMaxVelocityToForwardAngleRad =
            3.141592653589793238462643383279502884;

        // A precision moving passage needs a stable transverse frame. The
        // upstream moving-gap predictor may be configured more loosely for
        // diagnostics; this verifier fails closed when its accepted continuous
        // alignment bound exceeds this limit.
        double maximumAcceptedGapTravelAlignment = 0.5;
    };

    enum class Status : std::uint8_t
    {
        Feasible = 0,
        GapUnavailable,
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

        double minimumSampleClearanceMeters =
            std::numeric_limits<double>::infinity();
        double minimumContinuousClearanceBoundMeters =
            std::numeric_limits<double>::infinity();

        double maximumRelativeCenterMotionBoundMeters = 0.0;
        double maximumPassageAxisInflationRad = 0.0;
        double maximumRelativeOrientationSweepInflationMeters = 0.0;

        // First acceleration sample of the exact Hermite segment that was
        // continuously verified above. Valid only when feasible == true.
        // Runtime receding-horizon control may execute this sample instead of
        // re-deriving a different trajectory after the proof.
        Vec3d initialLinearAccelerationMapMetersPerSec2 {};
    };

    [[nodiscard]] static Result evaluate(const Query& query) noexcept;
};

} // namespace world::navigation
