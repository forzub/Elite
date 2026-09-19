#pragma once

#include <cstddef>
#include <cstdint>

#include "src/game/navigation/AcceptedManeuverProgram.h"

namespace game::navigation
{

// Pure O(1)-bounded program interpretation block.
//
// AcceptedManeuverProgram + universe time -> one reference/feed-forward sample.
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
    };

    [[nodiscard]] static Result sample(
        const AcceptedManeuverProgram& program,
        double universeTimeSeconds
    ) noexcept;
};

} // namespace game::navigation
