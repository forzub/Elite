// DEV check: emergency thermal coupling PROPULSION -> SHIP SERVICES on the
// production ShipCore (Newtonian-I Cobra). Pre-registered expectations:
//  1. ER-5 well below its safe temperature (1500 K), intact pad: the exchanger stays
//     closed (normal flight unchanged).
//  2. ER-5 overheated (2050 K: above the 1950 K engage point, below safe
//     2100 K) with a damaged pad (30 %): the exchanger
//     opens, transfers <= its 60 MW rating, the hot reactor cools faster than
//     on a twin ship whose exchanger is destroyed, and the service coolant
//     receives that heat (warmer than the twin's).
//  3. Every tick the heat the reactors gave up (sum of their overflow) is the
//     heat the exchanger moved - exactly once.
//  4. Exchanger destroyed: transfer 0 (heat stays in the ER-5).
//  5. Service coolant at 98 % of critical: interlock, transfer 0.
//
//   EliteShipThermalCheck  -> PASS/FAIL lines, exit 0/1

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"

namespace
{
int failures = 0;

void check(bool condition, const std::string& what)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", what.c_str());
    if (!condition)
        ++failures;
}

std::unique_ptr<game::ship::core::ShipCore> newtonianShip()
{
    auto core = std::make_unique<game::ship::core::ShipCore>();
    core->init(ShipRole::Player, EliteCobraMk1::EliteCobraMk1Descriptor(), glm::vec3(0.0f), ShipInitData {},
               glm::mat4(1.0f));
    core->selectPhysicalConstructionForDiagnostic(game::ship::plant::PhysicalControlModelId::NewtonianI);
    core->refreshRuntimeState();
    return core;
}

world::modules::ObjectModuleState* er5(game::ship::core::ShipCore& core, std::size_t g)
{
    const auto& ids = EliteCobraMk1::EliteCobraMk1Descriptor().powerThermalInstallation.engineReactorModuleIds;
    return g < ids.size() ? core.moduleRuntime().findModule(ids[g]) : nullptr;
}

// Reactor groups of the active model (ids present in the runtime).
std::size_t reactorCount(game::ship::core::ShipCore& core)
{
    std::size_t n = 0;
    while (er5(core, n))
        ++n;
    return n;
}

double overflowSumMW(game::ship::core::ShipCore& core)
{
    double sum = 0.0;
    for (std::size_t g = 0; er5(core, g); ++g)
        sum += er5(core, g)->engineReactorOperating.lastCoolantHeatMW;
    return sum;
}
} // namespace

int main()
{
    static std::string modelEnv = "ELITE_SHIP_PHYSICAL_TEST_MODEL=Newtonian-I";
    putenv(modelEnv.data());
    constexpr float dt = 0.1f;
    const auto& rating = EliteCobraMk1::EliteCobraMk1Descriptor().powerThermalInstallation.emergencyHeatExchanger;
    const std::string exchangerId =
        EliteCobraMk1::EliteCobraMk1Descriptor().powerThermalInstallation.emergencyHeatExchangerModuleId;

    // 1. Normal: operating temperature, intact pad.
    {
        auto core = newtonianShip();
        check(reactorCount(*core) > 0 && core->moduleRuntime().findModule(exchangerId),
              "installed: ER-5 reactors and the emergency heat exchanger module exist");
        for (std::size_t g = 0; er5(*core, g); ++g)
            er5(*core, g)->engineReactorOperating.temperatureK = 1500.0;
        double peak = 0.0;
        for (int i = 0; i < 300; ++i)
        {
            core->updateSystems(dt);
            peak = std::max(peak, core->emergencyBridgeTransferMW());
        }
        check(peak == 0.0, "normal (ER-5 1500 K, intact pad): exchanger closed, transfer 0");
    }

    // 2-4. Overheat with a damaged pad: exchanger vs destroyed exchanger.
    auto withBridge = newtonianShip();
    auto without = newtonianShip();
    without->moduleRuntime().markModuleDestroyed(exchangerId, 0.0f);
    for (auto* core : {withBridge.get(), without.get()})
        for (std::size_t g = 0; er5(*core, g); ++g)
        {
            er5(*core, g)->engineReactorOperating.temperatureK = 2050.0;
            er5(*core, g)->engineReactorOperating.padIntegrity01 = 0.3;
        }
    double peak = 0.0, peakWithout = 0.0, movedMJ = 0.0;
    bool exactlyOnce = true;
    for (int i = 0; i < 600; ++i)
    {
        withBridge->updateSystems(dt);
        without->updateSystems(dt);
        const double moved = withBridge->emergencyBridgeTransferMW();
        exactlyOnce = exactlyOnce && std::abs(moved - overflowSumMW(*withBridge)) <= 1.0e-9 * std::max(1.0, moved);
        peak = std::max(peak, moved);
        peakWithout = std::max(peakWithout, without->emergencyBridgeTransferMW());
        movedMJ += moved * dt;
    }
    const double tWith = er5(*withBridge, 0)->engineReactorOperating.temperatureK;
    const double tWithout = er5(*without, 0)->engineReactorOperating.temperatureK;
    std::printf("     60 s: ER-5 %.1f K (exchanger) vs %.1f K (destroyed); service coolant %.1f K vs %.1f K; "
                "peak %.2f MW, moved %.0f MJ\n",
                tWith, tWithout, withBridge->serviceCoolantTemperatureK(), without->serviceCoolantTemperatureK(),
                peak, movedMJ);
    check(peak > 0.0 && peak <= rating.maxTransferMW + 1.0e-9,
          "overheat + damaged pad: exchanger opens, transfer within its rating");
    check(tWith < tWithout, "overheat: the ER-5 cools faster with the exchanger");
    check(withBridge->serviceCoolantTemperatureK() > without->serviceCoolantTemperatureK(),
          "overheat: the service coolant receives the heat");
    check(exactlyOnce, "every tick: heat given up by the reactors == heat moved (exactly once)");
    check(peakWithout == 0.0, "exchanger destroyed: transfer 0, heat stays in the ER-5");

    // 5. Saturated service coolant.
    {
        auto core = newtonianShip();
        for (std::size_t g = 0; er5(*core, g); ++g)
        {
            er5(*core, g)->engineReactorOperating.temperatureK = 2050.0;
            er5(*core, g)->engineReactorOperating.padIntegrity01 = 0.3;
        }
        double peakSaturated = 0.0;
        for (int i = 0; i < 50; ++i)
        {
            core->debugSetServiceCoolantTemperatureK(0.98 * core->serviceCoolantCriticalTemperatureK());
            core->updateSystems(dt);
            peakSaturated = std::max(peakSaturated, core->emergencyBridgeTransferMW());
        }
        check(peakSaturated == 0.0, "service coolant at 98 % of critical: interlock, transfer 0");
    }

    std::printf("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
