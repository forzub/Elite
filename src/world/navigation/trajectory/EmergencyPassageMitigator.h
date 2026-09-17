#pragma once

#include "AttitudeReachabilityEvaluator.h"
#include "OrientedPassageEvaluator.h"

#include <cstddef>
#include <cstdint>

namespace world::navigation
{

// Best-effort emergency passage intent used only when the normal collision-free
// path cannot be demonstrated in time.
//
// The class does not claim that contact is safe. It keeps navigation/control
// active by selecting the best physically reachable entry attitude on the arc
// toward a preferred passage pose, recommends braking when useful, and exposes
// an explicit contact-expected result for the downstream physics/damage system.
class EmergencyPassageMitigator final
{
public:
    using Vec3d = OrientedPassageEvaluator::Vec3d;
    using Basis3d = OrientedPassageEvaluator::Basis3d;
    using HullProxy = OrientedPassageEvaluator::HullProxy;
    using Passage = OrientedPassageEvaluator::Passage;
    using PassageResult = OrientedPassageEvaluator::Result;
    using AngularCapability = AttitudeReachabilityEvaluator::AngularCapability;

    static constexpr std::size_t kOrientationSamples = 17;

    struct Query
    {
        HullProxy hull {};
        Passage passage {};

        // Estimated center crossing the passage entry plane. The later full
        // 6DoF solver remains responsible for proving/achieving this position.
        Vec3d predictedEntryCenterMapMeters {};

        Basis3d currentBodyToMap {};
        Basis3d preferredBodyToMap {};
        Vec3d currentAngularVelocityMapRadPerSec {};
        AngularCapability angularCapability {};

        double distanceToEntryMeters = 0.0;
        double closingSpeedMetersPerSec = 0.0;
        double maxBrakingAccelerationMetersPerSec2 = 0.0;
    };

    enum class Status : std::uint8_t
    {
        SafeEntryPose = 0,
        EmergencyStopBeforeEntry,
        EmergencyMitigatedContact,
        InvalidInput
    };

    struct Result
    {
        Status status = Status::InvalidInput;

        // commandValid means navigation should keep producing control intent.
        // SafeEntryPose proves only entry cross-section geometry/reachability;
        // it is not a complete continuous swept-body proof.
        bool commandValid = false;
        bool collisionFreeEntryPoseProven = false;
        bool contactExpected = false;
        bool maximumBrakingRecommended = false;
        bool canStopBeforeEntry = false;

        Vec3d recommendedAimPointMapMeters {};
        Vec3d desiredTravelDirectionMap {0.0, 0.0, 1.0};
        Basis3d bestEffortBodyToMap {};

        double preferredAttitudeErrorRad = 0.0;
        double availableTimeBeforeEntrySeconds = 0.0;
        double angularSettleTimeSeconds = 0.0;
        double maximumCorrectiveAngleRad = 0.0;
        double appliedCorrectionRad = 0.0;
        double appliedCorrectionFraction = 0.0;
        double remainingAttitudeErrorRad = 0.0;

        double estimatedEntrySpeedMetersPerSec = 0.0;
        double minimumClearanceMeters = 0.0;
        double clearanceDeficitMeters = 0.0;

        PassageResult passageAtEntry {};
    };

    [[nodiscard]] static Result evaluate(const Query& query) noexcept;
};

} // namespace world::navigation
