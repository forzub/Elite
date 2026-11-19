#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

// The economy's call-in point for server systems that do not own the GameServer (the refuel service in
// GameSimulationRefuel.cpp, later docking fees, rescue). GameServer installs its EconomyServer here for its
// lifetime; with nothing installed every price is 0 and only a free (0 Cr) charge succeeds - the same
// behaviour as src/game/fuel/FuelEconomyStub.h, whose two functions these replace with the SAME signatures:
//
//   game::fuel::economy_stub::fuelPriceCrPerTonne(station, commodity)  ->  game::economy::service::fuelPriceCrPerTonne
//   game::fuel::economy_stub::chargeAccount(ship, amountCr, reason)    ->  game::economy::service::chargeAccount
//
// docs/ECONOMY_BANKING.md §6. Server thread only (the simulation step and GameServer run on one thread).
namespace game::economy
{
class EconomyServer;
}

namespace game::economy::service
{
struct Bindings
{
    EconomyServer* economy = nullptr;
    // runtime ship entity -> persistent player id (0 = AI / unknown)
    std::function<std::uint64_t(std::uint32_t shipEntityId)> playerOfShip;
    // runtime hub id -> physical system id (-1 unknown) - runtime hubs (earth_orbital_hub) are not in the economy data
    std::function<int(const std::string& hubId)> systemOfHub;
    // runtime ship entity -> (hub id it is at / in the frame of, physical system id): whose bank is paid
    std::function<std::pair<std::string, int>(std::uint32_t shipEntityId)> locationOfShip;
};

void install(Bindings bindings);
void uninstall(const EconomyServer* economy);
[[nodiscard]] bool installed();

// Cr per tonne CHARGED for `commodity` ("jump_charge", "reactor_fuel", "tech_gases") at runtime hub `stationId`:
// 0 while fuel is free of charge (author 2026-11-19); the caller applies its station markup (grey / illegal sellers).
[[nodiscard]] double fuelPriceCrPerTonne(const std::string& stationId, const std::string& commodity);
// the list price the model computes even while fuel is free (for display / statistics)
[[nodiscard]] double fuelListPriceCrPerTonne(const std::string& stationId, const std::string& commodity);
// A refuel is PREPAID (audit 2026-11-19: it was charged after the pumping, and a refused payment left the fuel in the
// tank): buyFuel pays what the account can (overdraft; else the lifeline minimum on credit) and takes it out of the
// stock; the pump delivers at most grantedT; returnFuel gives back the unused part (money and stock).
struct FuelPurchase
{
    bool ok = false;
    bool economy = false;       // false: no economy running (tools / tests) - nothing charged, nothing reserved
    double grantedT = 0.0;
    double crPerT = 0.0;        // charged per tonne (0 while fuel is free of charge)
    double chargedCr = 0.0;
    double lifelineCr = 0.0;
    std::string reason;
};
[[nodiscard]] FuelPurchase buyFuel(std::uint32_t shipEntityId, const std::string& stationId, const std::string& commodity,
                                   double tonnes);
void returnFuel(std::uint32_t shipEntityId, const std::string& stationId, const std::string& commodity, double tonnes,
                double crPerT);
// the station's fuel stock of record (t); < 0 when the economy is not running or does not sell it there
[[nodiscard]] double fuelStockT(const std::string& stationId, const std::string& commodity);
// charge the account of the player flying `shipEntityId`; fuel / tow / repair reasons may draw lifeline credit.
// false = refused. AI ships (no player) and 0 Cr always succeed.
bool chargeAccount(std::uint32_t shipEntityId, double amountCr, const std::string& reason);
// an administrative fine (hub traffic control, navigation zones; docs/HUB_SECURITY.md) to the authority of `hubId`
// (empty = where the ship is): never refused - what the player cannot pay becomes a fine debt at that polity's bank.
// Result: what was paid now / booked as debt (Cr); AI ships and no economy: both 0.
struct FineResult
{
    bool charged = false;
    double paidCr = 0.0;
    double debtCr = 0.0;
    std::string bankId;
};
FineResult chargeFine(std::uint32_t shipEntityId, const std::string& hubId, double amountCr, const std::string& reason);
// optional: the fuel physically left station `stationId` (its stock and tomorrow's price feel it; the player's
// "received free of charge" list value grows). Call once per finished delivery.
void noteFuelDelivered(std::uint32_t shipEntityId, const std::string& stationId, const std::string& commodity, double tonnes);
} // namespace game::economy::service
