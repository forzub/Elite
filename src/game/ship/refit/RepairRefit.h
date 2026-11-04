#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/game/ship/ShipDescriptor.h"

// Repair & Refit (F7) - consequences of a service operation on a REAL ship.
//
// A preview never touches the live ship: it builds a throwaway ShipCore from
// the same descriptor, adopts the live instance state (module runtime, load,
// propulsion switch, service reactor / coolant / radiators), applies the
// proposed operation with the same ObjectModuleRuntime / ShipCore calls an
// applied operation uses, and measures BOTH states with the production
// capability / power / thermal / device code plus a short fixed-step
// full-forward burn. One physics, one capability truth, one equipment truth:
// the "after" of a preview is what applyRefitOperation() produces.
//
// Costs come from the passport product price and the repair doctrine of the
// installation (crew / station / yard / factory, replace hours) through the
// data-driven ship_market_rules.json.
namespace game::ship::core
{
class ShipCore;
}

namespace game::ship::refit
{
enum class RefitOperationKind : std::uint8_t
{
    Repair = 0,      // restore structure + damage causes (wear stays)
    Replace,         // new unit (same product unless productId names another)
    Remove,          // reserved (UI action later)
    Install          // reserved (UI action later)
};

struct RefitOperation
{
    RefitOperationKind kind = RefitOperationKind::Repair;
    std::string moduleId;
    std::string productId;   // Replace: another catalog product; empty = same product
};

// Whole-ship state measured by the production code.
struct RefitShipMetrics
{
    bool valid = false;
    double massTonnes = 0.0;
    double forwardAccelerationMps2 = 0.0;      // capability (installed, live mass)
    double reverseAccelerationMps2 = 0.0;
    double rcsAccelerationMps2 = 0.0;          // weakest translation axis
    double angularAccelerationRadPerSec2 = 0.0;
    int mainEnginesOperational = 0;
    int mainEnginesInstalled = 0;
    double propulsionPowerAvailableMW = 0.0;   // sum of generator groups now
    double propulsionPowerRatedMW = 0.0;
    double servicePowerOutputMW = 0.0;
    double burnAccelerationMps2 = 0.0;         // measured over a fixed-step full burn
    double burnSeconds = 0.0;
    double maxEngineTemperatureK = 0.0;        // after the burn
    double engineThermalMargin01 = 0.0;        // 1 - T / safe, weakest drive
    double maxReactorTemperatureK = 0.0;
    double reactorThermalMargin01 = 0.0;       // 1 - T / safe, weakest generator
    double serviceCoolantK = 0.0;
    double serviceCoolantCriticalK = 0.0;
    double propulsionCoolantK = 0.0;
    double emergencyCoolingMW = 0.0;           // PROPULSION -> SERVICES exchanger load
    bool radar = false;
    double radarRangeKm = 0.0;
    bool receiver = false;
    bool transmitter = false;
    bool transponder = false;
    bool lifeSupport = false;
    bool computer = false;
    bool jumpCore = false;
    bool jumpMassWithinRating = false;
    int docksOperational = 0;
    // the operated module
    std::string moduleState;                   // attached / disabled / destroyed / detached / hanging
    double moduleHealth01 = 0.0;
    double moduleDamage01 = 0.0;               // functional damage (all causes)
    double moduleWear01 = 0.0;
};

struct RefitDelta
{
    std::string key;            // stable metric key
    std::string labelRu;
    std::string unit;
    double before = 0.0;
    double after = 0.0;
    bool flag = false;          // boolean metric (before/after 0/1)
};

struct RefitCost
{
    std::string repairLevel;    // crew / station / yard / factory
    double labourHours = 0.0;
    double labourCr = 0.0;
    double materialCr = 0.0;
    double totalCr = 0.0;
};

struct RefitPreview
{
    bool ok = false;
    std::string error;          // why the operation is not possible / incompatible
    RefitOperation operation;
    RefitShipMetrics before;
    RefitShipMetrics after;
    std::vector<RefitDelta> deltas;   // only what changes
    RefitCost cost;
    bool applicable = false;    // can be applied now (DEV: same-product service)
    std::string applyNote;
};

// A compatible replacement candidate for a module (catalog product).
struct RefitReplacement
{
    std::string productId;
    std::string name;
    std::string manufacturer;
    double priceCr = 0.0;
    bool sameProduct = false;
};

// Module facts of the installation (passport): product, repair doctrine.
struct RefitModuleFacts
{
    std::string moduleId;
    std::string productId;
    std::string productName;
    std::string category;
    std::string repairLevel;
    double replaceHours = 0.0;
    bool lineReplaceable = false;
    bool repairable = false;
    double massTonnes = 0.0;
    double priceCr = 0.0;
    double powerMW = 0.0;
};

[[nodiscard]] RefitModuleFacts moduleFacts(const ShipDescriptor& descriptor, const std::string& moduleId);
[[nodiscard]] std::vector<RefitReplacement> compatibleReplacements(const ShipDescriptor& descriptor, const std::string& moduleId);
// Empty = compatible; otherwise the reason (category / rating / fit).
[[nodiscard]] std::string replacementIncompatibility(const ShipDescriptor& descriptor, const std::string& moduleId,
                                                     const std::string& productId);

[[nodiscard]] RefitShipMetrics measureShip(core::ShipCore& ship, const std::string& moduleId, double burnSeconds);
[[nodiscard]] RefitPreview previewRefit(const core::ShipCore& live, const RefitOperation& operation);
// Applies the operation to the live ship (DEV: repair / same-product new unit;
// another product needs the per-instance configuration layer). Returns the
// error or empty on success.
std::string applyRefitOperation(core::ShipCore& live, const RefitOperation& operation);
} // namespace game::ship::refit
