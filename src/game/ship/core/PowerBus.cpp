#include "PowerBus.h"
#include "ReactorSystem.h"
#include "src/game/equipment/capabilities/IPowerConsumer.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace game::ship::core
{

PowerBus::PowerBus(ReactorSystem& reactor)
    : m_reactor(&reactor)
{
}




void PowerBus::registerConsumer(game::equipment::IPowerConsumer* consumer)
{
    std::cout << "[PowerBus] consumer registered\n";
    m_consumers.push_back(consumer);
}

void PowerBus::setAuxiliaryDemandMW(double demandMW)
{
    m_auxiliaryDemandMW = std::isfinite(demandMW)
        ? std::max(0.0,demandMW) : 0.0;
}






// void PowerBus::update(bool reactorOnline)
{
    // A stopped reactor must not be silently restarted by demand from
    // cooling, avionics or last tick's actual Newtonian RCS draw.
    if (!m_reactor || !reactorOnline)
    {
        if (m_reactor)
            m_reactor->setThrottle(0.0);
        for (auto* consumer : m_consumers)
            if (consumer)
                consumer->setAvailablePower(0.0);
        m_overloaded = false;
        return;
    }

    // Consumers receive first claim on generated power. The auxiliary
    // physical RCS demand was ALREADY executed last control tick and
    // informs the service reactor's current load; it must not subtract
    // from allocations to critical cooling/life-support systems.
    double consumerRequestedMW = 0.0;
    for (const auto* consumer : m_consumers)
        if (consumer)
            consumerRequestedMW +=
                std::max(0.0,consumer->getRequestedPower());
    const double requestedMW =
        consumerRequestedMW + std::max(0.0,m_auxiliaryDemandMW);

    const double maximumMW = std::max(0.0,m_reactor->getMaxOutput());
    // Resolve desired production FIRST. Sampling output before this step
    // allocates last frame's power (including 0 MW on reactor startup).
    const double throttle = maximumMW > 1.0e-9
        ? std::clamp(requestedMW / maximumMW * 1.025,0.0,1.0)
        : 0.0;
    m_reactor->setThrottle(throttle);
    const double availableMW =
        std::max(0.0,m_reactor->getCurrentOutput());

    if (m_reactor->isEmergencyMode())
    {
        double criticalUsedMW = 0.0;
        for (auto* consumer : m_consumers)
        {
            if (!consumer) continue;
            double grantedMW = 0.0;
            if (consumer->getPriority()==
                    game::equipment::PowerPriority::Critical)
            {
                grantedMW = std::min(
                    std::max(0.0,consumer->getRequestedPower()),
                    std::max(0.0,availableMW-criticalUsedMW));
                criticalUsedMW += grantedMW;
            }
            consumer->setAvailablePower(grantedMW);
        }
        m_overloaded =
            requestedMW > criticalUsedMW + 1.0e-9;
        return;
    }

    // Share shortages between ordinary consumers; the auxiliary
    // engine load is constrained separately by the 24-nozzle executor.
    const double ratio = consumerRequestedMW > availableMW &&
                         consumerRequestedMW > 1.0e-9
        ? std::clamp(availableMW/consumerRequestedMW,0.0,1.0)
        : 1.0;
    for (auto* consumer : m_consumers)
        if (consumer)
            consumer->setAvailablePower(
                std::max(0.0,consumer->getRequestedPower())*ratio);
    m_overloaded = requestedMW > availableMW + 1.0e-9;
}

double PowerBus::availablePower() const
{
    return m_reactor->getCurrentOutput();
}







bool PowerBus::overloaded() const
{
    return m_overloaded;
}

}