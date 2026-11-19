// Refuelling at a station, on the server (author 2026-11-19: «найди заправки, заправь корабль на них»;
// docs/FUEL_STATIONS.md). Filling the jump charge is an authority action: the client (Ctrl+U, the dock card, a route
// stop, the DEV verb `refuel`) only asks. The ship must stand at the station for the whole job:
//   - docked on a pad of the hub's dock (the dock dispatcher says DOCKED): the dock's fixed hose;
//   - or held nearly still (<= 5 m/s) in a hub's frame: a tender (bunker boat) comes alongside, like bunkering at
//     anchor - the coupling takes twice as long. This is also how a route stop's service stay is served.
// The station must sell jump charge (src/assets/data/fuel/fuel_stations.json) and have stock; the price comes from the
// economy interface (game::economy::service, src/game/economy/EconomyService.h - the ECONOMY agent's; it replaced
// this slice's stub with the same signatures): 0 while fuel is free of charge (author 2026-11-19).
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <iostream>

#include "src/game/docking/traffic/DockTrafficControllerApi.h"
#include "src/game/economy/EconomyService.h"
#include "src/game/fuel/FuelStationCatalog.h"
#include "src/game/traffic/DockServiceTraffic.h"

namespace
{
using game::fuel::RefuelPhase;
using game::fuel::RefuelRefusal;

void publish(game::navigation::DynamicMotionState& m, RefuelPhase phase, RefuelRefusal refusal, double deliveredT, double rateTpm)
{
    m.refuelState = static_cast<std::uint8_t>(phase);
    m.refuelRefusal = static_cast<std::uint8_t>(refusal);
    m.refuelDeliveredT = deliveredT;
    m.refuelRateTpm = rateTpm;
}
} // namespace

std::string GameSimulation::serviceStationOf(EntityId shipId, bool* tender) const
{
    if (tender)
        *tender = false;
    const Ship* ship = getShip(shipId);
    if (!ship)
        return {};
    if (const DockSite* site = pilotDockSite(shipId);
        site && dockDispatcherStatus(shipId).phase == game::docking::traffic::DockTrafficPhase::Docked)
        return site->hubId;
    if (isDockStationControlledShip(shipId))
        return {};   // landing / leaving under the station's control: not at a berth
    const auto& m = ship->core().transform().motion;
    if (m.hubId.empty() || m.freeFlight || m.jumpState != 0 ||
        glm::length(m.localVelocityMps) > game::fuel::TenderMaxRelativeSpeedMps)
        return {};
    if (tender)
        *tender = true;
    return m.hubId;
}

bool GameSimulation::requestRefuel(EntityId shipId, const std::string& hubId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& m = ship->core().transform().motion;
    const auto refuse = [&](RefuelRefusal why, const std::string& where) {
        publish(m, RefuelPhase::Refused, why, 0.0, 0.0);
        ++m.refuelSerial;
        std::cerr << "[Refuel] ship " << shipId.value << ": REFUSED at " << (where.empty() ? std::string("-") : where) << " ("
                  << game::fuel::refuelRefusalName(why) << ")" << std::endl;
        return false;
    };
    if (const auto it = m_refuels.find(shipId); it != m_refuels.end())
    {
        if (hubId.empty() || it->second.hubId == hubId)
            return true;   // already running there
        endRefuel(shipId, it->second, RefuelPhase::Cancelled, "a new request at another station");
        m_refuels.erase(it);
    }
    bool tender = false;
    const std::string at = serviceStationOf(shipId, &tender);
    if (at.empty() || (!hubId.empty() && at != hubId))
        return refuse(RefuelRefusal::NotAtStation, hubId.empty() ? m.hubId : hubId);
    if (m.jChargeCapacityT <= 0.0)
        return refuse(RefuelRefusal::NoJumpDrive, at);
    if (m.jumpState != 0)
        return refuse(RefuelRefusal::Busy, at);
    const auto* station = game::fuel::FuelStationCatalog::instance().find(at);
    if (!station || station->stockT <= 0.0)
        return refuse(RefuelRefusal::NoFuelHere, at);
    if (m.jChargeT < 0.0)
        m.jChargeT = m.jChargeCapacityT;   // never used yet: the tanks came full
    if (m.jChargeT >= m.jChargeCapacityT - game::fuel::FullEpsilonT)
        return refuse(RefuelRefusal::TankFull, at);
    auto stock = m_fuelStockT.try_emplace(at, station->stockT).first;
    if (stock->second <= game::fuel::FullEpsilonT)
        return refuse(RefuelRefusal::StationDry, at);

    RefuelService service;
    service.hubId = at;
    service.pump = station->pump;
    service.tender = tender;
    if (tender)
        service.pump.hookupS *= game::fuel::TenderHookupFactor;
    service.priceCrPerT = game::economy::service::fuelPriceCrPerTonne(at, "jump_charge") * station->priceFactor;
    const double need = m.jChargeCapacityT - m.jChargeT;
    std::cerr << "[Refuel] ship " << shipId.value << ": at " << station->name << " (" << at << ", " << station->kind
              << (station->illegal ? ", ILLEGAL" : "") << (tender ? ", tender alongside" : ", dock hose") << ") - "
              << need << " t to fill, pump " << service.pump.rateTPerMin << " t/min, ~"
              << game::fuel::refuelEtaSeconds(service.pump, need) << " s; price " << service.priceCrPerT
              << " Cr/t (economy service), station stock " << stock->second << " t" << std::endl;
    publish(m, RefuelPhase::Hookup, RefuelRefusal::None, 0.0, service.pump.rateTPerMin);
    m_refuels[shipId] = std::move(service);
    return true;
}

void GameSimulation::cancelRefuel(EntityId shipId, const char* why)
{
    const auto it = m_refuels.find(shipId);
    if (it == m_refuels.end())
        return;
    endRefuel(shipId, it->second, RefuelPhase::Cancelled, why);
    m_refuels.erase(it);
}

void GameSimulation::endRefuel(EntityId shipId, RefuelService& service, RefuelPhase end, const char* why)
{
    Ship* ship = getShip(shipId);
    const double cost = service.job.deliveredT * service.priceCrPerT;
    bool paid = true;
    if (cost > 0.0)
        paid = game::economy::service::chargeAccount(shipId.value, cost, "jump charge at " + service.hubId);
    // the fuel left the station: the economy's stock / price and the "received free of charge" list value feel it
    if (service.job.deliveredT > 0.0)
        game::economy::service::noteFuelDelivered(shipId.value, service.hubId, "jump_charge", service.job.deliveredT);
    if (!ship)
        return;
    auto& m = ship->core().transform().motion;
    publish(m, end, RefuelRefusal::None, service.job.deliveredT, service.pump.rateTPerMin);
    ++m.refuelSerial;
    std::cerr << "[Refuel] ship " << shipId.value << ": " << game::fuel::refuelPhaseName(end) << " at " << service.hubId << " ("
              << why << "): delivered " << service.job.deliveredT << " t, J charge " << m.jChargeT << " / " << m.jChargeCapacityT
              << " t, cost " << cost << " Cr" << (paid ? "" : " - PAYMENT REFUSED (PROPOSAL: a debt to the station)")
              << "; station stock left " << m_fuelStockT[service.hubId] << " t" << std::endl;
}

void GameSimulation::updateRefuels(double dt)
{
    // NPC ships use the same service: one held at a station with less than half a tank asks for it (a scan every 5 s)
    m_npcRefuelScanSeconds += dt;
    if (m_npcRefuelScanSeconds >= 5.0)
    {
        m_npcRefuelScanSeconds = 0.0;
        for (const auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr || isPlayerControlled(id) || m_refuels.count(id) || !passOwns(shipPtr.get()))
                continue;
            const auto& m = shipPtr->core().transform().motion;
            if (m.jChargeCapacityT <= 0.0 || m.jChargeT < 0.0 || m.jChargeT >= 0.5 * m.jChargeCapacityT)
                continue;
            const std::string at = serviceStationOf(id);
            if (!at.empty() && game::fuel::FuelStationCatalog::instance().find(at))
                (void)requestRefuel(id, at);
        }
    }

    for (auto it = m_refuels.begin(); it != m_refuels.end();)
    {
        Ship* ship = getShip(it->first);
        if (!ship)
        {
            it = m_refuels.erase(it);
            continue;
        }
        if (!passOwns(ship))
        {
            ++it;
            continue;
        }
        auto& service = it->second;
        auto& m = ship->core().transform().motion;
        // still at the berth? A dock job ends when the ship leaves the pad (departure); a tender job when the ship
        // leaves the hub's frame; drifting too fast only pauses the tender's boom
        bool tender = false;
        const std::string at = serviceStationOf(it->first, &tender);
        const bool inFrame = !m.freeFlight && m.hubId == service.hubId && m.jumpState == 0;
        if ((!service.tender && at != service.hubId) || (service.tender && !inFrame))
        {
            endRefuel(it->first, service, RefuelPhase::Cancelled, service.tender ? "left the hub's frame" : "left the pad");
            it = m_refuels.erase(it);
            continue;
        }
        const bool attached = !service.tender || at == service.hubId;
        double& stock = m_fuelStockT[service.hubId];
        if (m.jChargeT < 0.0)
            m.jChargeT = m.jChargeCapacityT;
        const auto before = service.job.phase;
        game::fuel::refuelStep(service.job, service.pump, dt, attached, m.jChargeT, m.jChargeCapacityT, stock);
        if (service.job.phase != before)
            std::cerr << "[Refuel] ship " << it->first.value << ": " << game::fuel::refuelPhaseName(before) << " -> "
                      << game::fuel::refuelPhaseName(service.job.phase) << " (delivered " << service.job.deliveredT << " t, J charge "
                      << m.jChargeT << " t)" << std::endl;
        if (service.job.phase == RefuelPhase::Done)
        {
            endRefuel(it->first, service, RefuelPhase::Done, stock <= game::fuel::FullEpsilonT ? "the station ran dry" : "full");
            it = m_refuels.erase(it);
            continue;
        }
        publish(m, service.job.phase, RefuelRefusal::None, service.job.deliveredT, service.pump.rateTPerMin);
        ++it;
    }
}
