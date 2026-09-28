#pragma once

#include <cstddef>
#include <cstdint>

#include "src/game/navigation/AcceptedManeuverProgram.h"

namespace game::navigation
{

// Pure bounded program interpretation block.
//
// TimeScheduled:
//   AcceptedManeuverProgram + universe time -> reference/feed-forward sample.
//
// SpatialCorridor:
//   AcceptedManeuverProgram + real vehicle position -> nearest monotonic
//   reference/feed-forward sample on the accepted polyline.
//
// It owns no world query, obstacle search, feedback control or replanning.
class ManeuverProgramSampler final
{
public:
    enum class Status : std::uint8_t
    {
        InvalidInput = 0,
        BeforeStart,
        Active,
        AfterEnd
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        AcceptedManeuverProgram::ReferenceSample reference {};

        double elapsedSeconds = 0.0;
        std::size_t lowerSampleIndex = 0;
        std::size_t upperSampleIndex = 0;
        double interpolation01 = 0.0;

        bool spatialReference = false;
        double spatialDistanceMeters = 0.0;
    };

    [[nodiscard]] static Result sample(
        const AcceptedManeuverProgram& program,
        double universeTimeSeconds
    ) noexcept;

    // Spatial-corridor sampling never lets nominal time run away from the
    // vehicle. The caller supplies a monotonic minimum segment index so a
    // self-near path cannot make execution jump backwards.
    [[nodiscard]] static Result sampleSpatial(
        const AcceptedManeuverProgram& program,
        double universeTimeSeconds,
        const glm::dvec3& positionMapMeters,
        std::size_t minimumSegmentIndex = 0
    ) noexcept;
};

} // namespace game::navigation
