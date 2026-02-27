// #pragma once

// #include "src/game/equipment/EquipmentModule.h"
// #include "ReceiverDesc.h"

// namespace game
// {

// struct ReceiverModule : public EquipmentModule
// {
//     EquipmentDescriptor desc;   // ← ТЫ ЗАБЫЛ ЭТО ПОЛЕ
//     float sensitivity = 1.0f;

//     void init(const ReceiverDesc& d)
//     {
//         desc = d.base;
//         sensitivity = d.sensitivity;

//         enabled = true;
//     }
// };

// }


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

private:
    ReceiverDesc m_desc;
    double m_availablePower = 0.0;
};

}
