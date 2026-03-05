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

  
    // Устанавливаем запрос для насоса (охлаждения)
    // Пока нет прямого доступа к насосу, делаем через consumer
    
    // 1. Собираем запросы
    double totalRequested = 0.0;
    for (auto* c : m_consumers) {
        double req = c->getRequestedPower();
        totalRequested += req;
    }

    // 2. устанавливаем мощ реакора 
    double maxOutput = m_reactor->getMaxOutput();
    double throttle = totalRequested / maxOutput * 1.025; // что бы не было - оверлоад

    if (throttle > 1.0)
        throttle = 1.0;

    m_reactor->setThrottle(throttle);
    
    // 3. Распределяем энергию
    // double available = m_reactor->getCurrentOutput();    

    double available = m_reactor->getCurrentOutput();
    
    if (totalRequested <= available) {
        for (auto* c : m_consumers)
            c->setAvailablePower(c->getRequestedPower());
        m_overloaded = false;
    } else {
        double ratio = available / totalRequested;
        for (auto* c : m_consumers)
            c->setAvailablePower(c->getRequestedPower() * ratio);
        m_overloaded = true;
    }

    
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