#include "ShipCore.h"
#include <iostream>

#include "core/Log.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/equipment/radar/RadarDesc.h"

namespace game::ship::core
{

// // -------------------------------------------------
// //    ##                ##       ##
// //                               ##
// //   ###     #####     ###      #####
// //    ##     ##  ##     ##       ##
// //    ##     ##  ##     ##       ##
// //    ##     ##  ##     ##       ## ##
// //   ####    ##  ##    ####       ###
// // -------------------------------------------------
void ShipCore::init(
    ShipRole                inRole,
    const ShipDescriptor&   descriptor, 
    glm::vec3               position,
    const ShipInitData&     initData
)
{
    // устанавливаем роль корабля - ИГРОК / НПС
    // общий контекст - глобальные данные, viewport etc
    // геттер из файла описания корабля (f.e. Cobra MKIII)
    // позиция корабля в пространстве

    m_role = inRole;  
    m_desc = &descriptor;
    m_transform.position = position;



    m_reactor = ReactorSystem(descriptor.core.reactorMaxOutputMW);
    m_thermal = ThermalSystem(
        descriptor.core.thermalMass,
        descriptor.core.maxSafeTemperature
    );
    m_thermal.setCoolingRate(descriptor.core.baseCoolingRate);
    m_powerBus = game::ship::core::PowerBus(m_reactor);
    
    m_reactor.setThrottle(1.0);
    
    // собираем корабль
    // Устанавливаем оборудование по умолчанию из дескриптора
    initShipSlotsFromDescriptor(descriptor);
    installDefaultEquipment(descriptor);

    
    // visual / UI
    // registry / legal
    // стартовый инвентарь
    
    m_visual = initData.visual;
    m_registry = initData.registry;
    for (Item* item : initData.initialInventory)
    {
        m_inventory.add(item);
    }

    // Автоматическая установка криптокарт в декрипторы
       

    m_transform.orientation = glm::mat4(1.0f);

    m_transform.pitchRate = 0.0f;
    m_transform.yawRate   = 0.0f;
    m_transform.rollRate  = 0.0f;

    m_transform.forwardVelocity = 0.0f;
    m_transform.localVelocity   = glm::vec3(0.0f);
    m_transform.targetSpeed     = 0.0f;


    // 2. базовое состояние
    SignalPayload payload;
    payload.type   = SignalType::Transponder;
    payload.actor  = m_registry.ownerActor;
    payload.channel = 0;
    payload.setMessage(m_desc->identity.shipName);

    equipment().transmitter.setPayload(payload);
    equipment().transmitter.enableSignal();
    
    

    debugPrintCoreSystems();
}



// // -------------------------------------------------
// //                       ###              ##
// //                        ##              ##
// //  ##  ##   ######       ##    ####     #####    ####
// //  ##  ##    ##  ##   #####       ##     ##     ##  ##
// //  ##  ##    ##  ##  ##  ##    #####     ##     ######
// //  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
// //   ######   ##       ######   #####      ###    #####
// //           ####

void ShipCore::updatePhysics(float dt, const WorldParams& world)
{
    
    
    m_controller.update(
        dt,
        m_desc->physics,
        m_transform,
        world
    );
    
  

    SharedShipPhysics::integrate(
        m_transform,
        m_desc->physics,
        m_control,
        world,
        dt
        );


    m_reactor.update(dt);
    m_powerBus.update();
    m_thermal.addHeat(m_reactor.heatGeneration());

    if (m_powerBus.overloaded()) 
    {    
        m_thermal.addHeat(5.0 * dt);
    }

    m_thermal.update(dt);

}



void ShipCore::updatePerception(
    float dt,
    const std::vector<world::RadarContactInput>& radarInputs
)
{
    if (m_equipment.radar.isOperational())
    {
        m_equipment.radar.update(
            dt,
            radarInputs,
            m_transform.position,
            m_transform.orientation
        );

        const auto& contacts = m_equipment.radar.getContacts();

        m_radarDebugTimer += dt;
    }

    if (m_role == ShipRole::Player)
    {
        // HUD / визуальная семантика
        // m_signalPresentation.update(dt, m_signalResults);
    }
    else
    {
        // AI-восприятие
        m_npcAwareness.update(m_signalResults);
    }
}



//                                        ##                                 ###
//                                                                            ##
//  ##  ##   ######             #####    ###      ### ##  #####     ####      ##      #####
//  ##  ##    ##  ##           ##         ##     ##  ##   ##  ##       ##     ##     ##
//  ##  ##    ##  ##            #####     ##     ##  ##   ##  ##    #####     ##      #####
//  ##  ##    #####                 ##    ##      #####   ##  ##   ##  ##     ##          ##
//   ######   ##               ######    ####        ##   ##  ##    #####    ####    ######
//           ####                                #####

void ShipCore::updateSignals(
         float dt,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources,
        EntityId ownerId
    )
{
    m_signalResults.clear();

    m_signalReceiver.update(
        dt,
        m_transform.position,
        m_equipment.receiver,
        worldSignals,
        planets,
        interferenceSources,
        m_signalResults,
        ownerId
    );
}

                                                                            

//                                                          ##     ######    ####
//                                                         ####     ##  ##    ##
//   ####     ####    ######    ### ##   ####             ##  ##    ##  ##    ##
//  ##  ##       ##    ##  ##  ##  ##   ##  ##            ##  ##    #####     ##
//  ##        #####    ##      ##  ##   ##  ##            ######    ##        ##
//  ##  ##   ##  ##    ##       #####   ##  ##            ##  ##    ##        ##
//   ####     #####   ####         ##    ####             ##  ##   ####      ####
//                             #####


int ShipCore::freeCargoMass() const
{
    return m_cargo.mass.free();
}

int ShipCore::freeCargoVolume() const
{
    return m_cargo.volume.free();
}

bool ShipCore::addCargo(int mass, int volume)
{
    if (!m_cargo.mass.canAdd(mass)) return false;
    if (!m_cargo.volume.canAdd(volume)) return false;

    m_cargo.mass.add(mass);
    m_cargo.volume.add(volume);
    return true;
}

bool ShipCore::removeCargo(int mass, int volume)
{
    if (!m_cargo.mass.canRemove(mass)) return false;
    if (!m_cargo.volume.canRemove(volume)) return false;

    m_cargo.mass.remove(mass);
    m_cargo.volume.remove(volume);
    return true;
}

// //            ###                ##                         ##                ##       ##
// //             ##                ##                                                    ##
// //   #####     ##      ####     #####    #####             ###     #####     ###      #####
// //  ##         ##     ##  ##     ##     ##                  ##     ##  ##     ##       ##
// //   #####     ##     ##  ##     ##      #####              ##     ##  ##     ##       ##
// //       ##    ##     ##  ##     ## ##       ##             ##     ##  ##     ##       ## ##
// //  ######    ####     ####       ###   ######             ####    ##  ##    ####       ###


void ShipCore::initShipSlotsFromDescriptor(const ShipDescriptor& descriptor)
{
    m_desc = &descriptor;

    // Cargo
    m_cargo.mass.capacity   = m_desc->storage.cargoMass;
    m_cargo.mass.value      = 0;
    m_cargo.volume.capacity = m_desc->storage.cargoVolume;
    m_cargo.volume.value    = 0;

    // System slots
    m_systems.reactor.capacity            = m_desc->systems.reactorSlots;
    m_systems.engine.capacity             = m_desc->systems.engineSlots;
    m_systems.radar.capacity              = m_desc->systems.radarSlots;
    m_systems.weapon.capacity             = m_desc->systems.weaponSlots;
    m_systems.dockingComputer.capacity    = m_desc->systems.dockingComputerSlots;
    m_systems.decryptor.capacity          = m_desc->systems.decryptorSlots;
    m_systems.jammer.capacity             = m_desc->systems.jammerSlots;
    m_systems.receiver.capacity           = m_desc->systems.receiverSlots;
    m_systems.transmitter.capacity        = m_desc->systems.transmitterSlots;
    m_systems.utility.capacity            = m_desc->systems.utilitySlots;
    m_systems.fuelScop.capacity           = m_desc->systems.fuelScopSlots;
    m_systems.tractorBeam.capacity        = m_desc->systems.tractorBeamSlots;

    // Dock slots
    m_docks.fighter.capacity              = m_desc->docks.fighterSlots;
    m_docks.shuttle.capacity              = m_desc->docks.shuttleSlots;
    m_docks.drone.capacity                = m_desc->docks.droneSlots;
}


// //    ##                         ##               ###      ###                                           ##                          ###              ###
// //                               ##                ##       ##                                                                        ##             ## ##
// //   ###     #####     #####    #####    ####      ##       ##               ####     ######  ##  ##    ###     ######                ##    ####      #
// //    ##     ##  ##   ##         ##         ##     ##       ##              ##  ##   ##  ##   ##  ##     ##      ##  ##            #####   ##  ##   ####
// //    ##     ##  ##    #####     ##      #####     ##       ##              ######   ##  ##   ##  ##     ##      ##  ##           ##  ##   ######    ##
// //    ##     ##  ##        ##    ## ##  ##  ##     ##       ##              ##        #####   ##  ##     ##      #####            ##  ##   ##        ##
// //   ####    ##  ##   ######      ###    #####    ####     ####              #####       ##    ######   ####     ##                ######   #####   ####
// //                                                                                      ####                    ####

    void ShipCore::installDefaultEquipment(const ShipDescriptor& descriptor)
    {
        const auto& presets = descriptor.defaultEquipment;
    
        // Устанавливаем оборудование, если оно есть в пресетах
        // if (presets.decryptor.has_value()) {

        //     installEquipment("decryptor", 
        //     m_systems.decryptor, 
        //     // equipment.decryptor,
        //     m_equipment.decryptors.modules, 
        //     *presets.decryptor);
        // }
        
        // if (presets.jammer.has_value()) {

        //     installEquipment("jammer", 
        //     m_systems.jammer, 
        //     m_equipment.jammer, 
        //     *presets.jammer);
        // }
    
        if (presets.receiver.has_value()) {
            
            installEquipment("receiver", 
            m_systems.receiver, 
            m_equipment.receiver, 
            *presets.receiver);
        }
        
        if (presets.transmitter.has_value()) {

            installEquipment("transmitter", 
            m_systems.transmitter, 
            m_equipment.transmitter, 
            *presets.transmitter);
        }




        if (presets.radar.has_value())
        {
            const RadarDesc& radarDesc = *presets.radar;

            if (canInstallRadar(descriptor, radarDesc))
            {
                installEquipment(
                    "radar",
                    m_systems.radar,
                    m_equipment.radar,
                    radarDesc
                );

                m_powerBus.registerConsumer(&m_equipment.radar);
            }
            else
            {
                LOG("[ShipCore] Radar incompatible with platform");
            }
        }


    }

// //    ##                         ##               ###      ###                                           ##                                           ##
// //                               ##                ##       ##                                                                                        ##
// //   ###     #####     #####    #####    ####      ##       ##               ####     ######  ##  ##    ###     ######   ##  ##    ####    #####     #####
// //    ##     ##  ##   ##         ##         ##     ##       ##              ##  ##   ##  ##   ##  ##     ##      ##  ##  #######  ##  ##   ##  ##     ##
// //    ##     ##  ##    #####     ##      #####     ##       ##              ######   ##  ##   ##  ##     ##      ##  ##  ## # ##  ######   ##  ##     ##
// //    ##     ##  ##        ##    ## ##  ##  ##     ##       ##              ##        #####   ##  ##     ##      #####   ##   ##  ##       ##  ##     ## ##
// //   ####    ##  ##   ######      ###    #####    ####     ####              #####       ##    ######   ####     ##      ##   ##   #####   ##  ##      ###
// //                                                                                      ####                    ####


    template<typename ModuleType, typename DescType>
    bool ShipCore::installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        std::vector<ModuleType>& modules,
        const DescType& desc
    ) {

        // 1. Проверяем наличие слота
        if (slot.capacity <= 0) {
            printf("[ERROR] No slot for %s\n", equipmentName);
            return false;
        }
        
        // 2. Проверяем свободный слот
        if (slot.value >= slot.capacity) {
            printf("[ERROR] No free slots for %s\n", equipmentName);
            return false;
        }
        
        
        // 3. Устанавливаем
        // module.init(desc);
        modules.emplace_back();
        modules.back().init(desc);
        slot.value++;
        
        printf("[INFO] %s installed successfully\n", equipmentName);
        return true;
    }




    template<typename ModuleType, typename DescType>
    bool ShipCore::installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module,
        const DescType& desc
    ) {

        // 1. Проверяем наличие слота
        if (slot.capacity <= 0) {
            printf("[ERROR] No slot for %s\n", equipmentName);
            return false;
        }
        
        // 2. Проверяем свободный слот
        if (slot.value >= slot.capacity) {
            printf("[ERROR] No free slots for %s\n", equipmentName);
            return false;
        }
        
        
        // 3. Устанавливаем
        module.init(desc);
        slot.value++;
        
        printf("[INFO] %s installed successfully\n", equipmentName);
        return true;
    }

// //                                                                                              ##                                           ##
// //                                                                                                                                           ##
// //  ######    ####    ##  ##    ####    ##  ##    ####              ####     ######  ##  ##    ###     ######   ##  ##    ####    #####     #####
// //   ##  ##  ##  ##   #######  ##  ##   ##  ##   ##  ##            ##  ##   ##  ##   ##  ##     ##      ##  ##  #######  ##  ##   ##  ##     ##
// //   ##      ######   ## # ##  ##  ##   ##  ##   ######            ######   ##  ##   ##  ##     ##      ##  ##  ## # ##  ######   ##  ##     ##
// //   ##      ##       ##   ##  ##  ##    ####    ##                ##        #####   ##  ##     ##      #####   ##   ##  ##       ##  ##     ## ##
// //  ####      #####   ##   ##   ####      ##      #####             #####       ##    ######   ####     ##      ##   ##   #####   ##  ##      ###
// //                                                                             ####                    ####

    template<typename ModuleType>
    bool ShipCore::removeEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module
    ) {
        if (!module.enabled) {
            printf("[ERROR] %s {} not installed\n", equipmentName);
            return false;
        }
        
        module.enabled = false;
        slot.value--;
        printf("[ERROR] %s {} removed successfully\n", equipmentName);
        return true;
    }




    void ShipCore::debugPrintCoreSystems() const
    {
        std::cout << "\n[ShipCore] CoreSystems initialized:\n";

        std::cout << "  |- Reactor\n";
        std::cout << "       maxOutput: " << m_reactor.maxOutput() << " MW\n";
        std::cout << "       throttle : " << m_reactor.throttle() << "\n";
        std::cout << "       integrity: " << m_reactor.structuralIntegrity() << "\n";

        std::cout << "  |- ThermalSystem\n";
        std::cout << "       temperature : " << m_thermal.temperature() << "\n";
        std::cout << "       maxSafeTemp : " << m_thermal.maxSafeTemperature() << "\n";
        std::cout << "       integrity   : " << m_thermal.integrity() << "\n";

        std::cout << "  |- PowerBus\n";
        std::cout << "       availablePower: " << m_powerBus.availablePower() << "\n";
        std::cout << "       overloaded    : "
                << (m_powerBus.overloaded() ? "YES" : "NO") << "\n";

        std::cout << "[Radar] Installed\n";
        std::cout << "  |- maxRange: " << m_equipment.radar.getDesc().maxRange << "\n";
        std::cout << "  |- power   : " << m_equipment.radar.getDesc().powerConsumption << "\n";

        std::cout << std::endl;
    }



    bool ShipCore::canInstallRadar(
        const ShipDescriptor& shipDesc,
        const RadarDesc& radarDesc
    ) const
    {
        const auto& mount = shipDesc.radarMount;

        if (radarDesc.requiredMountSize > mount.maxSupportedMountSize)
            return false;

        if (std::find(
                mount.allowedProfiles.begin(),
                mount.allowedProfiles.end(),
                radarDesc.visualProfile
            ) == mount.allowedProfiles.end())
            return false;

        return true;
    }


}