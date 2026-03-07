#pragma once

#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/ShipVisualIdentity.h"
#include "src/game/ship/ShipRegistry.h"

#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"

#include "src/game/ship/core/ShipInventory.h"
#include "src/game/ship/core/CargoBay.h"

#include "src/game/ship/core/ShipSignalController.h"
#include "src/game/ship/core/ShipSlotsState.h"
#include "src/game/ship/core/ShipRole.h"

#include "src/game/ship/sensors/ShipSignalPresentation.h"
#include "src/game/ship/sensors/NpcSignalAwareness.h"

#include "src/game/ship/ShipInitData.h"

#include "src/game/equipment/signalNode/processing/SignalReceiver.h"

#include "src/game/equipment/ShipEquipment.h"

#include "src/world/WorldParams.h"

#include "src/game/math/MathTransform.h"

#include "src/game/ship/ShipController.h"
#include "src/world/types/SignalReceptionResult.h"

#include "ReactorSystem.h"
#include "ThermalSystem.h"
#include "PowerBus.h"
#include "CoolingSystem.h"

#include "src/world/types/RadarContactInput.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "src/game/simulation/ShipCoreStatus.h"

#include "src/game/ship/core/systems/CoreSystem.h"
// #include "src/game/network/ClientShipCommand.h"

#include "game/damage/HitComponent.h"
#include "src/game/ship/damage/ShipDamageHandler.h"


namespace game::ship::core
{

class ShipCore
{
public:
    ShipCore() = default;
    
    void init(
        ShipRole role,
        const ShipDescriptor& descriptor,
        glm::vec3 position,
        const ShipInitData& initData
    );

    
    // // ───────────────
    // // Фазы апдейта
    // // ───────────────
 
    void updatePerception(
        float dt,
        const std::vector<world::RadarContactInput>& radarInputs
    );

    void updateSignals(
        float dt,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources,
        EntityId ownerId
    );

    // // ───────────────
    // // Cargo API
    // // ───────────────
    int  freeCargoMass() const;
    int  freeCargoVolume() const;
    bool addCargo(int mass, int volume);
    bool removeCargo(int mass, int volume);

    // // ───────────────
    // // Equipment API
    // // ───────────────
    void initShipSlotsFromDescriptor(const ShipDescriptor& descriptor);
    void installDefaultEquipment(const ShipDescriptor& descriptor);

    template<typename ModuleType, typename DescType>
    bool installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        std::vector<ModuleType>& modules,
        const DescType& desc
    );

    template<typename ModuleType, typename DescType>
    bool installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module,
        const DescType& desc
    );
    
    template<typename ModuleType>
    bool removeEquipment(
        const char*     equipmentName,
        QuantitySlot&   slot,
        ModuleType&     module
    );

    void updatePhysics(float dt, const WorldParams& world);
    bool canInstallRadar(
        const ShipDescriptor& shipDesc,
        const RadarDesc& radarDesc
    ) const;




    void damageRadiatorPanel(int index, double amount) {
        m_cooling.damagePanel(index, amount);
    }
        
    void repairAllRadiatorPanels() {
        m_cooling.repairAllPanels();
    }


    game::ShipCoreStatus getCoreStatus() const;


    
    // ───────────────
    // Доступ к состоянию
    // ───────────────
    ShipTransform&                              transform()                 { return m_transform; }
    const ShipTransform&                        transform() const           { return m_transform; }

    ShipControlState&                           control()                   { return m_control; }
    const ShipControlState&                     control() const             { return m_control; }

    game::ShipEquipment&                        equipment()                 { return m_equipment; }
    const game::ShipEquipment&                  equipment() const           { return m_equipment; }

    ShipInventory&                              inventory()                 { return m_inventory; }
    CargoBay&                                   cargo()                     { return m_cargo; }

    ShipRole                                    role() const                { return m_role; }

    const std::vector<SignalReceptionResult>&   signalResults() const       {return m_signalResults;} 

    const ShipDescriptor&                       desc() const                { return *m_desc; }
    const RadarModule&                          radar() const               { return m_equipment.radar; }   

    const ReactorSystem&                        reactor() const               { return m_reactor; }                                                           
                                                                    
    void                                        debugPrintCoreSystems() const; 

    ShipSignalController                        m_signalController;
    NpcSignalAwareness                          m_npcAwareness;    
    
    
    // --------- DAMAGE --------
    game::damage::HitComponent& getHitComponent(){ return m_hitComponent; }
    const game::damage::HitComponent& getHitComponent() const{ return m_hitComponent; }
    void applyDamage(const game::damage::DamageEvent& event);

    void exportHitVolumes() const;
    void restoreVolume(int index);



private:

    // ───────────────
    // Идентичность
    // ───────────────
    ShipRole                                    m_role = ShipRole::NPC;
    ShipVisualIdentity                          m_visual;
    ShipRegistry                                m_registry;
    const ShipDescriptor*                       m_desc = nullptr;

    // ───────────────
    // Состояние
    // ───────────────
    ShipTransform                               m_transform;
    ShipControlState                            m_control;
    ShipParams                                  m_params;
    ShipController                              m_controller;

    // ───────────────
    // Math
    // ───────────────
    game::math::MathTransform                   m_mathTransform;


    // ───────────────
    // Сигналы
    // ───────────────
    // RX
    SignalReceiver                              m_signalReceiver;
    std::vector<SignalReceptionResult>          m_signalResults;

    

    // ───────────────
    // Хранилища
    // ───────────────
    CargoBay                                    m_cargo;
    ShipInventory                               m_inventory;
    game::ShipEquipment                         m_equipment;
    ShipSystemState                             m_systems;
    ShipDockState                               m_docks;
    
    
    // ───────────────
    // CoreSystem
    // ───────────────
    ReactorSystem                               m_reactor;  // временный пустой дескриптор
    ThermalSystem                               m_thermal;        // значения по умолчанию
    CoolingSystem                               m_cooling;                        // будет инициализирован позже
    PowerBus                                    m_powerBus;                       // будет инициализирован позже

    double                                      m_radarDebugTimer = 0.0;
    
    CoreSystem                           m_lifeSupport{0.01, "lifeSupport", game::equipment::PowerPriority::Critical}; // 5 МВт
    CoreSystem                           m_avionics{0.02, "Avionics", game::equipment::PowerPriority::Critical};
    CoreSystem                           m_radiationShield{53.0, "Radiation Shield", game::equipment::PowerPriority::Combat};

    
    // --------- DAMAGE --------
    game::damage::HitComponent          m_hitComponent;
    ShipDamageHandler                   m_damageHandler;

};


}