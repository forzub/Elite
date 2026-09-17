#pragma once

#include "OrientedPassageEvaluator.h"

#include <cstdint>

namespace world::navigation
{

// Evaluates one candidate ship terminal docking state against one predicted
// moving/rotating dock frame at capture time.
//
// This class does not plan the approach corridor, synthesize controls, or own
// collision/contact response. It answers only whether the two explicit docking
// interface frames satisfy the terminal 6DoF capture contract.
class DockingTerminalEvaluator final
{
public:
    using Vec3d = OrientedPassageEvaluator::Vec3d;
    using Basis3d = OrientedPassageEvaluator::Basis3d;
    using Pose = OrientedPassageEvaluator::Pose;

    enum class SurfaceSemantic : std::uint8_t
    {
        Unspecified = 0,
        Top,
        Bottom,
        Side
    };

    struct LocalPortFrame
    {
        SurfaceSemantic surface = SurfaceSemantic::Unspecified;
        Vec3d positionLocalMeters {};

        // Unit vectors in the owning rigid body's local frame. The mating
        // normal points outward from the physical docking face. referenceUp is
        // tangent to that face and fixes roll; it is never inferred from world-up.
        Vec3d matingNormalLocal {0.0, -1.0, 0.0};
        Vec3d referenceUpLocal {0.0, 0.0, 1.0};
    };

    struct RigidBodyState
    {
        Pose pose {};
        Vec3d linearVelocityMapMetersPerSec {};
        Vec3d angularVelocityMapRadPerSec {};
    };

    struct MovingDockState
    {
        Pose originPoseAtReferenceTime {};
        Vec3d originLinearVelocityMapMetersPerSec {};
        Vec3d originLinearAccelerationMapMetersPerSec2 {};

        // First accepted docking slice assumes constant world-space angular
        // velocity during the bounded capture prediction.
        Vec3d angularVelocityMapRadPerSec {};
    };

    struct MatingContract
    {
        SurfaceSemantic requiredShipSurface = SurfaceSemantic::Bottom;
        SurfaceSemantic requiredDockSurface = SurfaceSemantic::Bottom;

        double maxPositionErrorMeters = 0.0;
        double maxRelativeLinearSpeedMetersPerSec = 0.0;
        double maxNormalAlignmentErrorRad = 0.0;
        double maxRollAlignmentErrorRad = 0.0;
        double maxRelativeAngularSpeedRadPerSec = 0.0;
    };

    struct Query
    {
        // This is the candidate ship state at the same future capture instant
        // at which the dock frame is predicted.
        RigidBodyState shipAtCapture {};
        LocalPortFrame shipPort {};

        MovingDockState dock {};
        LocalPortFrame dockPort {};

        double captureTimeSeconds = 0.0;
        MatingContract contract {};
    };

    enum class Status : std::uint8_t
    {
        Capturable = 0,
        PortSemanticMismatch,
        PositionMismatch,
        RelativeLinearVelocityMismatch,
        MatingNormalMismatch,
        RollAlignmentMismatch,
        RelativeAngularVelocityMismatch,
        InvalidInput
    };

    struct PortWorldState
    {
        Vec3d positionMapMeters {};
        Vec3d linearVelocityMapMetersPerSec {};
        Vec3d angularVelocityMapRadPerSec {};
        Vec3d matingNormalMap {0.0, -1.0, 0.0};
        Vec3d referenceUpMap {0.0, 0.0, 1.0};
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        bool capturable = false;

        PortWorldState shipPortWorld {};
        PortWorldState dockPortWorld {};

        double positionErrorMeters = 0.0;
        double axialPositionErrorMeters = 0.0;
        double lateralPositionErrorMeters = 0.0;

        double relativeLinearSpeedMetersPerSec = 0.0;
        double relativeNormalSpeedMetersPerSec = 0.0;
        double relativeTangentialSpeedMetersPerSec = 0.0;

        double matingNormalAlignmentErrorRad = 0.0;
        double rollAlignmentErrorRad = 0.0;
        double relativeAngularSpeedRadPerSec = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Query& query) noexcept;
};

} // namespace world::navigation
