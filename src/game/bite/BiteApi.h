#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/game/bite/BiteTypes.h"

namespace game::bite
{

struct Thresholds
{
    // Slow degradation below caution is intentionally silent.
    double cautionLoss01 = 0.30;
    double warningLoss01 = 0.40;
};

class BiteApi final
{
public:
    explicit BiteApi(Thresholds thresholds = {}) noexcept;

    [[nodiscard]] State evaluate(
        std::uint64_t sourceRevision,
        const std::vector<EquipmentSample>& equipment
    ) const;

    [[nodiscard]] static std::vector<std::string> terminalLines(
        const State& state
    );

private:
    Thresholds m_thresholds;
};

} // namespace game::bite
