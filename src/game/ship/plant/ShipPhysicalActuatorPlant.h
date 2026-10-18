#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/equipment/engine/Engine.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"

namespace game::ship::plant
{

// Deterministic instantaneous thrust after equipment-authored instability.
[[nodiscard]] double instantaneousSlotThrust01(
    const EngineSlotState& state,
    std::size_t slotIndex,
    std::uint64_t controlTick,
    // Real installed engine views, indexed by the placement/topology table.
    // Null is allowed ONLY for legacy/diagnostic actors with no engine mount.
    const std::vector<game::equipment::engine::MountedEngine>* mountedEngines =
        nullptr
) noexcept;

// Scale applied to the net main acceleration for the currently active bank.
// Healthy/stable slots return exactly 1.0.
[[nodiscard]] double evaluateMainPropulsionInstantaneousScale(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick
) noexcept;

struct MainPropulsionReaction
{
    bool valid = false;

    // Sum of r x F for the currently firing longitudinal main bank, expressed
    // in canonical ship-body axes (+X right, +Y up, +Z aft).
    glm::dvec3 torqueBodyNm {0.0};

    // Principal-axis angular acceleration derived from the real torque and
    // current mass properties. Z is converted to ShipTransform roll sign,
    // whose positive axis is ship forward (-body Z).
    glm::dvec3 angularAccelerationBodyRadPerSec2 {0.0};
};

struct MainPropulsionTrimResult
{
    bool valid = false;

    // Actual main-engine acceleration after instantaneous damage, throttle
    // balancing and the small automatic common-TVC correction.
    glm::dvec3 accelerationBodyMps2 {0.0};

    MainPropulsionReaction reaction;

    // Diagnostics for HUD/logging. 1 = no thrust sacrificed by balancing.
    double thrustScale01 = 1.0;
    double trimLoad01 = 0.0;
    double automaticYawTrimRad = 0.0;
    double automaticPitchTrimRad = 0.0;

    // Per-slot delivered force fraction of that engine's HEALTHY rated
    // thrust. This is produced by the SAME physical allocator after actual
    // damage, stochastic dropouts, bank selection and balancing. No inference
    // from user throttle or a fake actuator test is allowed for operating age.
    static constexpr std::size_t MaxReportedSlots = 16;
    std::array<double, MaxReportedSlots> deliveredThrust01 {};
    std::size_t reportedSlotCount = 0;
};

// Cheap damaged-main compensation.
//
// The ordinary flight law still decides the requested maneuver. The active
// main bank is balanced directly to its weakest surviving instantaneous engine
// in one pass; damage asymmetry is never used as steering authority. A small
// common TVC correction may then reduce residual torque from missing geometry.
// The helper never increases any engine above its current damage-limited/
// instantaneous authority and never engages the opposite main bank.
[[nodiscard]] MainPropulsionTrimResult evaluateMainPropulsionFeedbackTrim(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& requestedAccelerationBodyMps2,
    const glm::dvec3& attitudeIntent01,
    double remainingYawTrimRad,
    double remainingPitchTrimRad,
    std::uint64_t controlTick
) noexcept;

// Convert the already-authorized net main-engine acceleration back into the
// forces carried by the surviving physical slots and calculate their reaction
// torque about the current center of mass.
//
// The accepted motion backend still determines the commanded net main force.
// This function adds the rigid-body consequence that was previously lost when
// per-slot topology was projected into aggregate ShipParams.
//
// normalizedPositionBody is deliberately dimensionless. Production capability
// projection maps it through the ship's real logical half-extents; turnRadius
// remains only a legacy fallback for descriptors/tests that lack geometry.
[[nodiscard]] MainPropulsionReaction evaluateMainPropulsionReaction(
    PhysicalControlModelId model,
    const std::vector<EngineSlotState>& state,
    const ShipParams& physics,
    const glm::dvec3& mainAccelerationBodyMps2,
    std::uint64_t controlTick = 0
) noexcept;

} // namespace game::ship::plant
