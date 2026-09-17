#pragma once

#include <cstdint>

namespace world::navigation
{

// Backend-neutral O(1) precision geometry check used only after cheap
// broadphase/local avoidance has reduced the problem to a small number of
// plausible narrow-passage candidates.
//
// This class does NOT discover gaps, integrate vehicle dynamics, or prove a
// complete time-varying trajectory. It answers the smaller question:
// "does this oriented box-like hull fit this oriented passage cross-section
//  at this attitude and lateral offset?"
class OrientedPassageEvaluator final
{
public:
    struct Vec3d
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct Basis3d
    {
        Vec3d right {1.0, 0.0, 0.0};
        Vec3d up {0.0, 1.0, 0.0};
        Vec3d forward {0.0, 0.0, 1.0};
    };

    // First precision proxy: body-local oriented box. More complex convex
    // compounds may be introduced behind the later trajectory boundary without
    // changing the broadphase representation.
    struct HullProxy
    {
        Vec3d halfExtentsBodyMeters {1.0, 1.0, 1.0};
        double additionalClearanceMeters = 0.0;
    };

    struct Pose
    {
        Vec3d centerMapMeters {};
        Basis3d bodyToMap {};
    };

    enum class PassageSource : std::uint8_t
    {
        AuthoredAperture = 0,
        ObstacleGap,
        DockingCorridor
    };

    // Passage frame: forward is travel through the opening, right/up span the
    // aperture plane. halfWidth/halfHeight describe the clear cross-section.
    struct Passage
    {
        PassageSource source = PassageSource::AuthoredAperture;
        Vec3d centerMapMeters {};
        Basis3d passageToMap {};
        double halfWidthMeters = 0.0;
        double halfHeightMeters = 0.0;
    };

    // Compact descriptor produced by a bounded precision-gap candidate builder.
    // It represents free space between nearby boundaries/objects without
    // requiring the evaluator to own scene geometry or perform all-pairs work.
    struct ObstacleGap
    {
        Vec3d centerMapMeters {};
        Vec3d travelDirectionMap {0.0, 0.0, 1.0};
        Vec3d separationAxisMap {1.0, 0.0, 0.0};
        double clearSeparationMeters = 0.0;
        double secondaryClearanceMeters = 0.0;
    };

    enum class Status : std::uint8_t
    {
        Fits = 0,
        TooWide,
        TooTall,
        OffsetOutside,
        InvalidInput
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        bool fits = false;

        double projectedHalfWidthMeters = 0.0;
        double projectedHalfHeightMeters = 0.0;
        double lateralOffsetMeters = 0.0;
        double verticalOffsetMeters = 0.0;
        double widthClearanceMeters = 0.0;
        double heightClearanceMeters = 0.0;

        // Diagnostic only: whether the old conservative bounding sphere would
        // fit the same cross-section. A precision fit may legitimately be true
        // while this is false (flat ship through flat slot).
        bool conservativeSphereFits = false;
    };

    [[nodiscard]] static Passage makeObstacleGapPassage(
        const ObstacleGap& gap
    ) noexcept;

    [[nodiscard]] static Result evaluate(
        const HullProxy& hull,
        const Pose& pose,
        const Passage& passage
    ) noexcept;
};

} // namespace world::navigation
