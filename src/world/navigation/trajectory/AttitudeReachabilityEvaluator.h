#pragma once

#include "OrientedPassageEvaluator.h"

#include <cstdint>

namespace world::navigation
{

// Constant-size conservative precheck for a required passage attitude.
//
// It does not generate thruster commands or a swept trajectory. It answers:
// "given this current attitude/angular rate and this much longitudinal room,
//  can the vehicle become attitude-ready before crossing the passage entry?"
//
// The later full 6DoF solver must still prove the complete swept-body maneuver.
class AttitudeReachabilityEvaluator final
{
public:
    using Vec3d = OrientedPassageEvaluator::Vec3d;
    using Basis3d = OrientedPassageEvaluator::Basis3d;

    struct AngularCapability
    {
        double maxAngularAccelerationRadPerSec2 = 0.0;
        double maxAngularSpeedRadPerSec = 0.0;
    };

    struct ApproachState
    {
        Basis3d currentBodyToMap {};
        Basis3d requiredBodyToMap {};
        Vec3d currentAngularVelocityMapRadPerSec {};

        // Longitudinal distance/speed toward the passage entry plane.
        double distanceToEntryMeters = 0.0;
        double closingSpeedMetersPerSec = 0.0;

        // Maximum physically available deceleration opposing closing motion.
        // Zero means the reachability proof may use coasting only.
        double maxBrakingAccelerationMetersPerSec2 = 0.0;
    };

    enum class Status : std::uint8_t
    {
        AlreadyReady = 0,
        ReachableCoast,
        ReachableWithBraking,
        UnreachableBeforeEntry,
        InvalidInput
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        bool reachable = false;
        bool brakingRequired = false;

        double attitudeErrorRad = 0.0;
        double conservativeSettleAngleRad = 0.0;
        double angularSettleTimeSeconds = 0.0;
        double minimumRotationTimeSeconds = 0.0;

        double coastDistanceBeforeReadyMeters = 0.0;
        double minimumDistanceBeforeReadyMeters = 0.0;
        double availableDistanceMeters = 0.0;
        double distanceMarginMeters = 0.0;
    };

    [[nodiscard]] static Result evaluate(
        const AngularCapability& capability,
        const ApproachState& approach
    ) noexcept;
};

} // namespace world::navigation
