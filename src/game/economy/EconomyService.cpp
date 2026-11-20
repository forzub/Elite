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

FuelPurchase buyFuel(std::uint32_t shipEntityId, const std::string& stationId, const std::string& commodity, double tonnes)
{
    FuelPurchase out;
    FuelKind k;
    if (!installed() || !kindOf(commodity, k))
    {
        out.ok = true;
        out.grantedT = tonnes;
        return out;
    }
    out.economy = true;
    const auto& b = bindings();
    const std::uint64_t player = b.playerOfShip ? b.playerOfShip(shipEntityId) : 0;
    const auto stock = b.economy->quoteFuel(stationId, systemOf(stationId), k);
    const double t = stock.available ? std::min(tonnes, std::max(0.0, stock.stockT)) : tonnes;
    const FuelCharge c = b.economy->chargeFuel(player, stationId, systemOf(stationId), k, t);
    out.ok = c.ok && c.tonnesGranted > 0.0;
    out.grantedT = c.tonnesGranted;
    out.chargedCr = c.chargedCr;
    out.lifelineCr = c.lifelineCr;
    out.crPerT = c.tonnesGranted > 0.0 ? c.chargedCr / c.tonnesGranted : 0.0;
    out.reason = c.reason;
    return out;
}

void returnFuel(std::uint32_t shipEntityId, const std::string& stationId, const std::string& commodity, double tonnes,
                double crPerT)
{
    FuelKind k;
    if (!installed() || !kindOf(commodity, k) || !(tonnes > 0.0))
        return;
    const auto& b = bindings();
    const std::uint64_t player = b.playerOfShip ? b.playerOfShip(shipEntityId) : 0;
    b.economy->refundFuel(player, stationId, systemOf(stationId), k, tonnes, crPerT);
}

double fuelStockT(const std::string& stationId, const std::string& commodity)
{
    FuelKind k;
    if (!installed() || !kindOf(commodity, k))
        return -1.0;
    const auto q = bindings().economy->quoteFuel(stationId, systemOf(stationId), k);
    return q.available ? q.stockT : -1.0;
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

namespace game::economy::service
{
FineResult chargeFine(std::uint32_t shipEntityId, const std::string& hubId, double amountCr, const std::string& reason)
{
    FineResult out;
    if (!(amountCr > 0.0) || !installed())
        return out;
    const auto& b = bindings();
    const std::uint64_t player = b.playerOfShip ? b.playerOfShip(shipEntityId) : 0;
    if (player == 0)
        return out;
    auto where = b.locationOfShip ? b.locationOfShip(shipEntityId) : std::pair<std::string, int>{std::string(), -1};
    if (!hubId.empty())
    {
        where.first = hubId;
        const int sys = systemOf(hubId);
        if (sys >= 0)
            where.second = sys;
    }
    const PaymentResult r = b.economy->chargeFine(player, where.first, where.second, amountCr, reason);
    out.charged = r.ok;
    out.debtCr = r.ok ? r.lifelineCr : 0.0;
    out.paidCr = r.ok ? amountCr - out.debtCr : 0.0;
    return out;
}

namespace
{
template <class F> PortFeeResult portFee(std::uint32_t shipEntityId, const std::string& hubId, F charge)
{
    PortFeeResult out;
    if (!installed())
        return out;
    const auto& b = bindings();
    const std::uint64_t player = b.playerOfShip ? b.playerOfShip(shipEntityId) : 0;
    if (player == 0)
        return out;
    auto where = b.locationOfShip ? b.locationOfShip(shipEntityId) : std::pair<std::string, int>{std::string(), -1};
    if (!hubId.empty())
    {
        where.first = hubId;
        const int sys = systemOf(hubId);
        if (sys >= 0)
            where.second = sys;
    }
    const EconomyServer::PortCharge c = charge(*b.economy, player, where.first, where.second);
    out.charged = c.booked;
    out.duplicate = c.duplicate;
    out.feeCr = c.feeCr;
    out.paidCr = c.paidCr;
    out.debtCr = c.debtCr;
    out.hours = c.hours;
    return out;
}
} // namespace

std::uint64_t shipKeyOf(std::uint32_t shipEntityId)
{
    const auto& b = bindings();
    return b.shipKeyOf ? b.shipKeyOf(shipEntityId) : 0;
}

PortFeeResult chargeDockingFee(std::uint32_t shipEntityId, const std::string& hubId, const std::string& callId, double dryMassT,
                               const std::string& dockModuleId)
{
    return portFee(shipEntityId, hubId, [&](EconomyServer& e, std::uint64_t player, const std::string& hub, int sys) {
        e.openPortCall(shipKeyOf(shipEntityId), callId, hub, dockModuleId, player);   // saved: survives a restart
        const auto c = e.chargeDocking(player, callId, hub, sys, dryMassT);
        if (c.booked && !c.duplicate)   // the customs inspection: once per port call (TRADE_DESIGN §2.8, step 9)
            (void)e.customsInspection(player, shipKeyOf(shipEntityId), callId, hub, sys);
        return c;
    });
}

std::string resumePortCall(std::uint32_t shipEntityId, const std::string& hubId, const std::string& dockModuleId)
{
    if (!installed())
        return {};
    const auto* c = bindings().economy->openPortCall(shipKeyOf(shipEntityId));
    return c && c->hubId == hubId && c->dockModuleId == dockModuleId ? c->callId : std::string();
}

PortFeeResult closeStalePortCall(std::uint32_t shipEntityId, double berthTimeScale)
{
    if (!installed())
        return {};
    const auto* c = bindings().economy->openPortCall(shipKeyOf(shipEntityId));
    if (!c)
        return {};
    const std::string callId = c->callId, hubId = c->hubId;
    PortFeeResult out = chargeBerthFee(shipEntityId, hubId, callId, 0.0, berthTimeScale);
    out.callId = callId;
    out.hubId = hubId;
    return out;
}

void notifyShipDocked(std::uint32_t shipEntityId, const std::string& hubId)
{
    if (!installed())
        return;
    const auto& b = bindings();
    if (b.shipDocked)
        b.shipDocked(shipEntityId, hubId);
}

PortFeeResult chargeBerthFee(std::uint32_t shipEntityId, const std::string& hubId, const std::string& callId, double fallbackHours,
                             double berthTimeScale)
{
    return portFee(shipEntityId, hubId, [&](EconomyServer& e, std::uint64_t player, const std::string& hub, int sys) {
        // the saved call's universe-clock hours (they run on across a restart); the caller's own count when none
        const double saved = e.closePortCall(shipKeyOf(shipEntityId), berthTimeScale);
        return e.chargeBerth(player, callId, hub, sys, saved >= 0.0 ? saved : fallbackHours);
    });
}
} // namespace game::economy::service
