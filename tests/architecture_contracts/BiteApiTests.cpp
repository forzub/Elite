#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "src/game/bite/BiteApi.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "BITE TEST: FAIL: " << message << "\n";
        std::exit(1);
    }
}
}

int main()
{
    game::bite::BiteApi bite;

    game::bite::EquipmentSample engine;
    engine.equipmentId = "engine_L";
    engine.displayName = "ENGINE L";

    auto state = bite.evaluate(1, {engine});
    require(state.summary.nominal(), "healthy engine not nominal");
    require(state.faults.empty(), "healthy engine emitted fault");

    engine.functionalAuthority01 = 0.71;
    state = bite.evaluate(2, {engine});
    require(state.faults.empty(), "slow degradation emitted premature fault");

    engine.functionalAuthority01 = 0.70;
    state = bite.evaluate(3, {engine});
    require(state.faults.size() == 1, "30% loss did not emit caution");
    require(state.faults.front().severity == game::bite::Severity::Caution,
            "30% loss wrong severity");
    require(state.faults.front().hudAttention, "caution did not request HUD attention");

    engine.functionalAuthority01 = 0.60;
    engine.powerCostMultiplier = 1.25;
    engine.heatMultiplier = 0.25;
    state = bite.evaluate(4, {engine});
    require(state.faults.front().severity == game::bite::Severity::Warning,
            "40% loss did not emit warning");

    engine.wear01 = 0.20;
    engine.maintenanceWearThreshold01 = 0.20;
    state = bite.evaluate(5, {engine});
    require(state.summary.maintenanceRequiredCount == 1,
            "maintenance threshold not reflected in summary");

    engine.operational = false;
    engine.functionalAuthority01 = 0.0;
    state = bite.evaluate(6, {engine});
    require(state.summary.failedCount == 1, "failed engine missing from summary");
    require(state.faults.front().severity == game::bite::Severity::Failed,
            "failed engine wrong severity");

    const auto lines = game::bite::BiteApi::terminalLines(state);
    require(!lines.empty(), "failed engine produced no terminal line");
    require(lines.front().find("ENGINE L") != std::string::npos,
            "terminal line lost equipment identity");

    std::cout << "BITE TEST: PASS\n";
    return 0;
}
