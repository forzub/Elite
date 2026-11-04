// EliteRepairRefitCheck: Repair & Refit previews on the production ShipCore.
//   - a destroyed radar is reported failed; its repair preview restores it;
//   - a damaged drive / generator changes the real capability snapshot;
//   - incompatible replacements are rejected, a compatible one gives the
//     correct after state;
//   - a preview never mutates the live ship;
//   - after apply the live ship equals the preview's after state.
// usage: EliteRepairRefitCheck
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "src/game/ship/ShipInitData.h"
#include "src/game/ship/core/ShipCore.h"
#include "src/game/ship/descriptors/CatalogShipRegistry.h"
#include "src/game/ship/refit/RepairRefit.h"

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS " : "FAIL ") << what << "\n";
    if (!ok)
        ++failures;
}

std::string num(double v, int digits = 2)
{
    std::ostringstream s;
    s << std::fixed << std::setprecision(digits) << v;
    return s.str();
}

std::unique_ptr<game::ship::core::ShipCore> spawn(const ShipDescriptor& d)
{
    auto core = std::make_unique<game::ship::core::ShipCore>();
    core->init(ShipRole::NPC, d, glm::vec3(0.0f), ShipInitData {}, glm::mat4(1.0f));
    core->refreshRuntimeState();
    return core;
}

// A fingerprint of everything a preview could accidentally change.
std::string fingerprint(const game::ship::core::ShipCore& core)
{
    std::ostringstream s;
    s << std::setprecision(12) << core.currentMassKg() << "|" << core.moduleRuntime().revision() << "|";
    for (const auto& m : core.moduleRuntime().modules())
        s << m.moduleId << ":" << static_cast<int>(m.state) << ":" << m.health << ":" << m.equipmentDamage.accumulated.impact01 << ";";
    return s.str();
}

void printDeltas(const game::ship::refit::RefitPreview& p)
{
    for (const auto& d : p.deltas)
        std::cout << "       " << d.labelRu << ": " << (d.flag ? (d.before > 0.5 ? "есть" : "нет") : num(d.before)) << " -> "
                  << (d.flag ? (d.after > 0.5 ? "есть" : "нет") : num(d.after)) << " " << d.unit << "\n";
    std::cout << "       стоимость: " << p.cost.repairLevel << ", " << num(p.cost.labourHours, 1) << " ч, работа "
              << num(p.cost.labourCr, 0) << " + материалы " << num(p.cost.materialCr, 0) << " = " << num(p.cost.totalCr, 0) << " Cr\n";
}
} // namespace

int main()
{
    using namespace game::ship::refit;
    using game::ship::descriptors::findCatalogShip;
    const auto* c120 = findCatalogShip("bharat.ship.c-120.konveyer");
    check(c120 != nullptr, "C-120 catalog ship builds");
    if (!c120)
        return 1;

    // ---- radar: destroyed -> failed; repair preview -> restored
    auto ship = spawn(*c120);
    const std::string radar = c120->catalog.radarModuleIds.front();
    ship->debugDestroyModuleById(radar);
    ship->refreshRuntimeState();
    ship->refreshDeviceState();
    check(!ship->devices().radar, "destroyed radar module " + radar + " -> radar failed on the live ship");
    const std::string liveBefore = fingerprint(*ship);
    const auto repairRadar = previewRefit(*ship, {RefitOperationKind::Repair, radar, {}});
    check(repairRadar.ok && !repairRadar.before.radar && repairRadar.after.radar &&
              repairRadar.after.radarRangeKm == c120->catalog.radarRangeMeters / 1000.0,
          "repair preview: radar " + std::string(repairRadar.before.radar ? "on" : "off") + " -> " +
              (repairRadar.after.radar ? "on" : "off") + ", range " + num(repairRadar.after.radarRangeKm, 0) + " km");
    printDeltas(repairRadar);
    check(fingerprint(*ship) == liveBefore, "the preview did not mutate the live ShipCore (modules, damage, mass, revision)");

    // ---- apply -> live ship == preview after
    const auto error = applyRefitOperation(*ship, {RefitOperationKind::Repair, radar, {}});
    auto measuredBack = spawn(*c120);
    measuredBack->adoptInstanceState(*ship);
    const auto live = measureShip(*measuredBack, radar, 8.0);
    check(error.empty() && ship->devices().radar && live.radar == repairRadar.after.radar &&
              std::abs(live.massTonnes - repairRadar.after.massTonnes) < 1e-9 &&
              std::abs(live.forwardAccelerationMps2 - repairRadar.after.forwardAccelerationMps2) < 1e-9 &&
              std::abs(live.burnAccelerationMps2 - repairRadar.after.burnAccelerationMps2) < 1e-9 &&
              std::abs(live.moduleHealth01 - repairRadar.after.moduleHealth01) < 1e-9,
          "apply repair: the live ship equals the preview after state (radar, mass, capability, 8 s burn)");

    // ---- drive destroyed -> capability; repair preview restores it
    {
        auto s = spawn(*c120);
        const std::string drive = c120->mainPropulsion.aftEngineModuleIds.front();
        s->debugDestroyModuleById(drive);
        s->refreshRuntimeState();
        const auto p = previewRefit(*s, {RefitOperationKind::Repair, drive, {}});
        check(p.ok && p.after.forwardAccelerationMps2 > p.before.forwardAccelerationMps2 * 1.2 &&
                  p.after.mainEnginesOperational == p.before.mainEnginesOperational + 1 &&
                  p.after.burnAccelerationMps2 > p.before.burnAccelerationMps2,
              "drive " + drive + " destroyed: repair preview forward " + num(p.before.forwardAccelerationMps2) + " -> " +
                  num(p.after.forwardAccelerationMps2) + " m/s2, burn " + num(p.before.burnAccelerationMps2) + " -> " +
                  num(p.after.burnAccelerationMps2) + " m/s2");
        printDeltas(p);
    }

    // ---- generator damaged 57 % -> propulsion power and acceleration
    {
        auto s = spawn(*c120);
        const std::string reactor = c120->powerThermalInstallation.engineReactorModuleIds.front();
        s->moduleRuntime().addEquipmentDamage(reactor, game::equipment::EquipmentDamageKind::Impact, 0.57);
        s->refreshRuntimeState();
        const auto p = previewRefit(*s, {RefitOperationKind::Repair, reactor, {}});
        check(p.ok && p.after.propulsionPowerAvailableMW > p.before.propulsionPowerAvailableMW + 1.0,
              "generator " + reactor + " at 57 % damage: repair preview propulsion power " + num(p.before.propulsionPowerAvailableMW, 0) +
                  " -> " + num(p.after.propulsionPowerAvailableMW, 0) + " MW, forward " + num(p.before.forwardAccelerationMps2) + " -> " +
                  num(p.after.forwardAccelerationMps2) + " m/s2");
        printDeltas(p);
    }

    // ---- incompatible replacements are rejected
    {
        auto s = spawn(*c120);
        const std::string drive = c120->mainPropulsion.aftEngineModuleIds.front();
        const auto wrongCategory = previewRefit(*s, {RefitOperationKind::Replace, radar, "ofu.drive.u43"});
        const auto tooHungry = previewRefit(*s, {RefitOperationKind::Replace, drive, "th.drive.tz23"});
        check(!wrongCategory.ok && !wrongCategory.error.empty() && !tooHungry.ok && !tooHungry.error.empty(),
              "incompatible replacements rejected: radar <- drive (" + wrongCategory.error + "); drive <- TZ-23 (" + tooHungry.error + ")");
    }

    // ---- a compatible replacement: radar N-2 -> N-10 gives its range
    {
        auto s = spawn(*c120);
        const auto list = compatibleReplacements(*c120, radar);
        bool hasN10 = false;
        for (const auto& r : list)
            hasN10 = hasN10 || r.productId == "xz.radar.n10";
        const auto p = previewRefit(*s, {RefitOperationKind::Replace, radar, "xz.radar.n10"});
        check(hasN10 && p.ok && p.after.radar && std::abs(p.after.radarRangeKm - 10000.0) < 1e-6 && p.before.radarRangeKm == 2000.0 &&
                  !p.applicable,
              "compatible replacement radar N-2 -> N-10: range " + num(p.before.radarRangeKm, 0) + " -> " + num(p.after.radarRangeKm, 0) +
                  " km (" + std::to_string(list.size()) + " compatible radars; apply needs the instance configuration layer)");
    }

    std::cout << (failures ? "REPAIR REFIT CHECK: FAILED (" + std::to_string(failures) + ")" : "REPAIR REFIT CHECK: PASS") << "\n";
    return failures ? 1 : 0;
}
