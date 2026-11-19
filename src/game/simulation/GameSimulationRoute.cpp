// A route stop's service at a hub, on the server (NAVIGATION_V2 §1, 2026-11-06: the CLIENT flies
// the route - SpaceStateRouteRun.cpp - and asks for the service; filling the J charge is an authority
// action). The ship must stay in that hub's frame for the whole timed stay.
#include "src/game/simulation/GameSimulation.h"

#include <iostream>

void GameSimulation::startHubService(EntityId shipId, const std::string& hubId, const std::string& action)
{
    if (!getShip(shipId) || hubId.empty())
        return;
    m_hubServices[shipId] = HubService {hubId, action, 0.0};
    std::cerr << "[Service] ship " << shipId.value << ": at " << hubId << " - docking for " << action << std::endl;
}

void GameSimulation::updateHubServices(double dt)
{
    for (auto it = m_hubServices.begin(); it != m_hubServices.end();)
    {
        Ship* ship = getShip(it->first);
        if (!ship)
        {
            it = m_hubServices.erase(it);
            continue;
        }
        if (!passOwns(ship))
        {
            ++it;
            continue;
        }
        auto& m = ship->core().transform().motion;
        auto& service = it->second;
        // PROPOSAL: the dock is not flown yet - the service is a timed stay in the hub's frame
        if (m.freeFlight || m.hubId != service.hubId)
        {
            std::cerr << "[Service] ship " << it->first.value << ": left " << service.hubId << " - " << service.action << " cancelled"
                      << std::endl;
            it = m_hubServices.erase(it);
            continue;
        }
        service.seconds += dt;
        // the refuel is the station's refuel service (GameSimulationRefuel.cpp: a tender alongside, the station's
        // pump rate, its stock) - no longer an instant fill. It is asked once the ship holds still at the station
        // (checked every 2 s; after 60 s it is asked anyway and the refusal is logged)
        const bool refuel = service.action.find("refuel") != std::string::npos;
        if (refuel && !service.refuelAsked)
        {
            service.refuelRetryS -= dt;
            if (service.refuelRetryS <= 0.0)
            {
                service.refuelRetryS = 2.0;
                if (serviceStationOf(it->first) == service.hubId || service.seconds > 60.0)
                {
                    service.refuelAsked = true;
                    (void)requestRefuel(it->first, service.hubId);
                }
            }
        }
        // the stay lasts at least 20 s and as long as the station's refuel job runs
        if (service.seconds < 20.0 || m_refuels.count(it->first) || (refuel && !service.refuelAsked))
        {
            ++it;
            continue;
        }
        if (service.action.find("repair") != std::string::npos)
            std::cerr << "[Service] ship " << it->first.value << ": repair at " << service.hubId
                      << " (PROPOSAL: a timed stay, no repair model yet)" << std::endl;
        std::cerr << "[Service] ship " << it->first.value << ": " << service.action << " at " << service.hubId << " done (J charge "
                  << m.jChargeT << " t), undocked" << std::endl;
        it = m_hubServices.erase(it);
    }
}
