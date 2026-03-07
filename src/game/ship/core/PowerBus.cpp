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






// void PowerBus::update()
// {
    
//     // 1. Собираем запросы
//     double totalRequested = 0.0;
//     for (auto* c : m_consumers) {
//         double req = c->getRequestedPower();
//         totalRequested += req;
//     }

//     bool emergencyMode = m_reactor->isEmergencyMode();
//     double maxOutput = m_reactor->getMaxOutput();

//     double available = m_reactor->getCurrentOutput();


//     // 3. Устанавливаем throttle реактора
//     double throttle = totalRequested / maxOutput * 1.025;
//     if (throttle > 1.0)
//         throttle = 1.0;
//     m_reactor->setThrottle(throttle);
    
   


// if (emergencyMode)
//     {
//         // АВАРИЙНЫЙ РЕЖИМ - питаем только критически важные системы
//         // std::cout << "[PowerBus] EMERGENCY MODE: Powering only critical systems\n";
        
//         double emergencyPowerUsed = 0.0;
        
//         // Сначала распределяем энергию критическим потребителям
//         for (auto* c : m_consumers)
//         {
//             if (c->getPriority() == game::equipment::PowerPriority::Critical)
//             {
//                 double requested = c->getRequestedPower();
//                 double allocated = std::min(requested, available - emergencyPowerUsed);
//                 c->setAvailablePower(allocated);
//                 emergencyPowerUsed += allocated;
                
//             }
//             else
//             {
                
//                 c->setAvailablePower(0.0);
//             }
//         }
        
//         m_overloaded = (emergencyPowerUsed < totalRequested);
//     }
//     else
//     {
//         // НОРМАЛЬНЫЙ РЕЖИМ - обычное распределение
//         if (totalRequested <= available) {
//             for (auto* c : m_consumers)
//                 c->setAvailablePower(c->getRequestedPower());
//             m_overloaded = false;
//         } else {
//             double ratio = available / totalRequested;
//             for (auto* c : m_consumers)
//                 c->setAvailablePower(c->getRequestedPower() * ratio);
//             m_overloaded = true;
//         }
//     }

    
// }
void PowerBus::update()
{
    static bool wasEmergencyMode = false;  // Запоминаем предыдущее состояние
    
    // 1. Собираем запросы
    double totalRequested = 0.0;
    for (auto* c : m_consumers) {
        double req = c->getRequestedPower();
        totalRequested += req;
    }

    // 2. Получаем мощность реактора и проверяем аварийный режим
    double available = m_reactor->getCurrentOutput();
    bool emergencyMode = m_reactor->isEmergencyMode();
    
    // 3. Устанавливаем throttle реактора
    double maxOutput = m_reactor->getMaxOutput();
    double throttle = totalRequested / maxOutput * 1.025;
    if (throttle > 1.0)
        throttle = 1.0;
    m_reactor->setThrottle(throttle);
    

    
    // 4. Распределяем энергию
    if (emergencyMode)
    {
        // АВАРИЙНЫЙ РЕЖИМ - питаем только критически важные систем
        
        double emergencyPowerUsed = 0.0;
        
        // Сначала распределяем энергию критическим потребителям
        for (auto* c : m_consumers)
        {
            if (c->getPriority() == game::equipment::PowerPriority::Critical)
            {
                double requested = c->getRequestedPower();
                double allocated = std::min(requested, available - emergencyPowerUsed);
                c->setAvailablePower(allocated);
                emergencyPowerUsed += allocated;
            }
            else
            {
                // Некритичные потребители отключаются полностью
                c->setAvailablePower(0.0);
            }
        }
        
        m_overloaded = (emergencyPowerUsed < totalRequested);
    }
    else 
    {
        // НОРМАЛЬНЫЙ РЕЖИМ - обычное распределение
        
        // Если только что вышли из аварийного режима - выводим сообщение
        
        if (totalRequested <= available) {
            // Энергии достаточно - всем даем сколько запросили
            for (auto* c : m_consumers)
                c->setAvailablePower(c->getRequestedPower());
            m_overloaded = false;
        } else {
            // Энергии недостаточно - пропорциональное урезание
            double ratio = available / totalRequested;
            for (auto* c : m_consumers)
                c->setAvailablePower(c->getRequestedPower() * ratio);
            m_overloaded = true;
        }
    }
    
    // Запоминаем состояние для следующего кадра
    wasEmergencyMode = emergencyMode;
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