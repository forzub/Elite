#include "src/game/economy/EconomyService.h"

#include <algorithm>
#include <cctype>

#include "src/game/economy/EconomyServer.h"

namespace game::economy::service
{
namespace
{
Bindings& bindings()
{
    static Bindings b;
    return b;
}

bool kindOf(const std::string& commodity, FuelKind& out)
{
    for (const FuelKind k : {FuelKind::JumpCharge, FuelKind::ReactorFuel, FuelKind::ReactionMass})
        if (commodity == fuelGoodId(k))
        {
            out = k;
            return true;
        }
    return false;
}

int systemOf(const std::string& hubId)
{
    const auto& b = bindings();
    return b.systemOfHub ? b.systemOfHub(hubId) : -1;
}

Purpose purposeOf(const std::string& reason)
{
    std::string r = reason;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto has = [&](const char* w) { return r.find(w) != std::string::npos; };
    if (has("fuel") || has("charge") || has("refuel") || has("заправ") || has("заряд") || has("топлив"))
        return Purpose::Fuel;
    if (has("tow") || has("буксир"))
        return Purpose::Tow;
    if (has("repair") || has("ремонт"))
        return Purpose::Repair;
    if (has("dock") || has("док"))
        return Purpose::Docking;
    return Purpose::Other;
}
} // namespace

void install(Bindings b)
{
    bindings() = std::move(b);
}

void uninstall(const EconomyServer* economy)
{
    if (bindings().economy == economy)
        bindings() = Bindings{};
}

bool installed()
{
    return bindings().economy != nullptr && bindings().economy->loaded();
}

double fuelPriceCrPerTonne(const std::string& stationId, const std::string& commodity)
{
    FuelKind k;
    if (!installed() || !kindOf(commodity, k))
        return 0.0;
    const auto q = bindings().economy->quoteFuel(stationId, systemOf(stationId), k);
    return q.available && !q.freeOfCharge ? q.listCrPerT : 0.0;
}

double fuelListPriceCrPerTonne(const std::string& stationId, const std::string& commodity)
{
    FuelKind k;
    if (!installed() || !kindOf(commodity, k))
        return 0.0;
    const auto q = bindings().economy->quoteFuel(stationId, systemOf(stationId), k);
    return q.available ? q.listCrPerT : 0.0;
}

bool chargeAccount(std::uint32_t shipEntityId, double amountCr, const std::string& reason)
{
    if (!(amountCr > 0.0))
        return true;
    if (!installed())
        return false;
    const auto& b = bindings();
    const std::uint64_t player = b.playerOfShip ? b.playerOfShip(shipEntityId) : 0;
    if (player == 0)
        return true;   // AI ships: no wallets yet
    const auto where = b.locationOfShip ? b.locationOfShip(shipEntityId) : std::pair<std::string, int>{std::string(), -1};
    return b.economy->chargeService(player, where.first, where.second, purposeOf(reason), amountCr, reason).ok;
}

void noteFuelDelivered(std::uint32_t shipEntityId, const std::string& stationId, const std::string& commodity, double tonnes)
{
    FuelKind k;
    if (!installed() || !kindOf(commodity, k))
        return;
    const auto& b = bindings();
    const std::uint64_t player = b.playerOfShip ? b.playerOfShip(shipEntityId) : 0;
    b.economy->recordFuelDelivery(player, stationId, systemOf(stationId), k, tonnes);
}
} // namespace game::economy::service
