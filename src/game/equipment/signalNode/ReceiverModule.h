#pragma once

#include "src/game/equipment/EquipmentModule.h"
#include "src/game/equipment/capabilities/IPowerConsumer.h"
#include "src/game/equipment/capabilities/IHeatSource.h"
#include "ReceiverDesc.h"

namespace game {

class ReceiverModule :
    public EquipmentModule,
    public game::equipment::IPowerConsumer,
    public game::equipment::IHeatSource
{
public:
    void init(const ReceiverDesc& desc);

    void update(double dt) override;

    double getRequestedPower() const override;
    void   setAvailablePower(double power) override;

    double getHeatGeneration() const override;

    double sensitivity() const { return m_desc.sensitivity; }
    std::string getLabel() const override { return m_label; }

    game::equipment::PowerPriority getPriority() const override { 
        return m_priority; 
    }

private:
    ReceiverDesc m_desc;
    double m_availablePower = 0.0;

    std::string                     m_label = "receiver";
    game::equipment::PowerPriority  m_priority = game::equipment::PowerPriority::Critical;
};

}
