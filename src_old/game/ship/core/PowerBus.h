#pragma once

#include <vector>

namespace game::equipment
{
    class IPowerConsumer;
}

namespace game::ship::core
{

class ReactorSystem;

class PowerBus
    {
    public:
        PowerBus() = default;
        PowerBus(ReactorSystem& reactor);

        void registerConsumer(game::equipment::IPowerConsumer* consumer);

        void update();

        double availablePower() const;
        bool overloaded() const;
        const std::vector<game::equipment::IPowerConsumer*>& getConsumers() const { return m_consumers;}

    private:
        ReactorSystem*                                  m_reactor = nullptr;
        std::vector<game::equipment::IPowerConsumer*>   m_consumers;
        bool                                            m_overloaded = false;
    
    };

}