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
    engine.outputInstability01 = 0.40;
    state = bite.evaluate(4, {engine});
    require(state.faults.front().severity == game::bite::Severity::Warning,
            "40% loss did not emit warning");
    require(state.faults.front().outputInstability01 == 0.40,
            "BITE lost output instability symptom");
    const auto unstableLines =
        game::bite::BiteApi::terminalLines(state);
    require(!unstableLines.empty() &&
            unstableLines.front().find("output instability 40%") !=
                std::string::npos,
            "BITE terminal omitted output instability");

    engine.wear01 = 0.20;
    engine.maintenanceWearThreshold01 = 0.20;
    state = bite.evaluate(5, {engine});
    require(state.summary.maintenanceRequiredCount == 1,
            "maintenance threshold not reflected in summary");

    engine.functionalAuthority01 = 0.0;
    engine.functionalFailed = true;
    state = bite.evaluate(6, {engine});
    require(state.summary.failedCount == 1,
            "failed thrust function missing from summary");
    require(state.faults.front().kind ==
                game::bite::FaultKind::FunctionalDegradation,
            "failed thrust function was reported as whole equipment loss");
    require(state.faults.front().severity == game::bite::Severity::Failed,
            "failed thrust function wrong severity");

    engine.functionalAuthority01 = 1.0;
    engine.functionalFailed = false;
    engine.secondaryAuthority01 = 0.0;
    engine.secondaryFailed = true;
    state = bite.evaluate(7, {engine});
    require(state.summary.failedCount == 1,
            "failed nozzle function missing from summary");
    require(state.faults.front().kind ==
                game::bite::FaultKind::NozzleDegradation,
            "failed nozzle function was not diagnosed independently");
    require(state.faults.front().severity == game::bite::Severity::Failed,
            "failed nozzle function wrong severity");

    engine.operational = false;
    engine.secondaryAuthority01 = 1.0;
    engine.secondaryFailed = false;
    state = bite.evaluate(8, {engine});
    require(state.faults.front().kind ==
                game::bite::FaultKind::EquipmentFailed,
            "physically unavailable equipment lost whole-equipment failure");

    const auto lines = game::bite::BiteApi::terminalLines(state);
    require(!lines.empty(), "failed engine produced no terminal line");
    require(lines.front().find("ENGINE L") != std::string::npos,
            "terminal line lost equipment identity");

    std::cout << "BITE TEST: PASS\n";
    return 0;
}
