#include "PowerBus.h"
#include "ReactorSystem.h"
#include "src/game/equipment/capabilities/IPowerConsumer.h"
#include <iostream>

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






void PowerBus::update()
{
    double available = m_reactor->currentOutput();
    double totalRequested = 0.0;

    for (auto* c : m_consumers)
        totalRequested += c->getRequestedPower();

    if (totalRequested <= available)
    {
        for (auto* c : m_consumers)
            c->setAvailablePower(c->getRequestedPower());

        m_overloaded = false;
    }
    else
    {
        double ratio = available / totalRequested;

        for (auto* c : m_consumers)
            c->setAvailablePower(c->getRequestedPower() * ratio);

        m_overloaded = true;
    }
  
}






double PowerBus::availablePower() const
{
    return m_reactor->currentOutput();
}







bool PowerBus::overloaded() const
{
    return m_overloaded;
}

}