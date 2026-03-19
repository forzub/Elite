#include "ShipCore.h"
#include <iostream>

#include "core/Log.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "game/ship/core/ShipHitBuilder.h"

#include "src/game/geometry/ObjLoader.h"
using namespace game::ship::geometry;

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


    // собираем корабль
    // Устанавливаем оборудование по умолчанию из дескриптора
    initShipSlotsFromDescriptor(descriptor);
    installDefaultEquipment(descriptor);




    // ===============================================
    // --------------- сердце корабля ----------------
    // ===============================================
    m_reactor = ReactorSystem(descriptor.reactor);
    m_reactor.setThrottle(0.8);

    m_thermal = ThermalSystem(descriptor.thermal);

    m_cooling.init(
        descriptor.cooling.radiatorArea,    // 1600 м²
        descriptor.cooling.panelCount,      // 160 панелей
        descriptor.cooling.emissivity,      // 0.85
        descriptor.cooling.maxTransferPower // 90 МВт
    );

    m_powerBus = game::ship::core::PowerBus(m_reactor);
    m_powerBus.registerConsumer(&m_cooling);
    m_powerBus.registerConsumer(&m_lifeSupport);
    m_powerBus.registerConsumer(&m_avionics);
    m_powerBus.registerConsumer(&m_radiationShield);

    m_powerBus.registerConsumer(&m_equipment.radar);
    
    m_powerBus.registerConsumer(&m_equipment.transmitter);
    m_powerBus.registerConsumer(&m_equipment.receiver);



    
    
    
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

    
    
    // -------- SHIP MESH -------------
    std::string objPath = "assets/models/cobra-mk1.obj";
    // std::string objPath = "assets/models/cobramk1T.obj";
    // std::string objPath = "assets/models/sphere.obj";
    // std::string objPath = "assets/models/cube.obj";

    if (ObjLoader::load(objPath, m_mesh.mesh))
    {
        m_mesh.loaded = true;
    }

    // -------- DAMAGE ZONE -----------
    m_damageHandler.ship = this;
    ShipHitBuilder::build(m_hitComponent, descriptor);  
    
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


    // ================================================
    // --------------- сердце корабля ----------------
    // ================================================

    // 1. Сбрасываем счетчики
    m_thermal.resetHeatVolume();


    // 2. определение желаний потребителей.
    m_cooling.setReactorState(
        m_reactor.getTemperature(),
        m_reactor.getWorkTemp(),
        m_reactor.getDeltaTemp(),
        m_reactor.getWarningTemp(),
        m_thermal.getTemperature(),
        m_thermal.getCriticalTemp()
    );
    m_cooling.calcRequestedPower();
    // 3. PowerBus распределяет энергию (определяет, сколько получит насос)
    // опрашивает потребителей
    // задает throttle
    // распределяет энергию согласно возможностей
    m_powerBus.update();

    //    ВНУТРИ reactor.update() уже вызывается m_thermal.addHeat()
    m_reactor.update(dt, m_thermal, m_cooling.getPumpCapacity());

    // 4. Тепло от ВСЕХ потребителей (кроме реактора и охлаждения)
    //    НЕ включая охлаждение - его тепло от насосов учтем отдельно
    for (auto* consumer : m_powerBus.getConsumers())
    {
        // Пропускаем охлаждение - уже учтено в update
        if (consumer == &m_cooling) continue;
        
        auto* heatSource = dynamic_cast<game::equipment::IHeatSource*>(consumer);
        if (heatSource) {
            double heatMW = heatSource->getHeatGeneration();
            if (heatMW > 0) {
                m_thermal.addHeat(heatMW * dt);
            }
        }
    }

    // 6. Система охлаждения забирает тепло из антифриза
    //    ВНУТРИ cooling.update() вызывается m_thermal.removeHeat()
    m_cooling.update(dt, m_thermal);

    // 7. Обновляем остальные системы (они могут менять свое состояние)
    m_lifeSupport.update(dt);
    m_avionics.update(dt);
    m_radiationShield.update(dt);

    // 8. НЕ добавляем тепло от них снова! Они уже учтены в п.4

    
    
    





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
        std::cout << "       throttle : " << m_reactor.getThrottle() << "\n";
        std::cout << "       integrity: " << m_reactor.structuralIntegrity() << "\n";
        std::cout << "       temp     : " << m_reactor.getTemperature() << " K\n";

        std::cout << "  |- ThermalSystem\n";
        std::cout << "       temperature : " << m_thermal.getTemperature() << " K\n";
        // Убираем вызовы maxSafeTemperature и integrity - их больше нет!
        
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
                radarDesc.type
            ) == mount.allowedProfiles.end())
            return false;

        return true;
    }




game::ShipCoreStatus ShipCore::getCoreStatus() const
{
    game::ShipCoreStatus status;
    
    // ----- Реактор (все методы есть) -----
    status.reactor.temperature = m_reactor.getTemperature();
    status.reactor.criticalTemp = m_reactor.getCriticalTemperature();
    status.reactor.outputMW = m_reactor.getCurrentOutput();
    status.reactor.maxOutputMW = m_reactor.getMaxOutput();
    status.reactor.throttle = m_reactor.getThrottle();
    status.reactor.instability = m_reactor.getInstability();
    status.reactor.status = static_cast<uint8_t>(m_reactor.getStatus());
    status.reactor.integrity = m_reactor.structuralIntegrity();
    status.reactor.generatedHeat = m_reactor.getHeatVolume();
    status.reactor.heatGenerationMW = m_reactor.getGeneratedHeatMJ();
    
    // ----- Thermal (все методы есть) -----
    status.thermal.temperature = m_thermal.getTemperature();
    status.thermal.thermalMass = m_thermal.getThermalMass();
    status.thermal.heatVolume = m_thermal.getHeatVolume();
    status.thermal.storedHeat = m_thermal.getStoredHeat(); 
    status.thermal.thermalCriticalTemp = m_thermal.getCriticalTemp(); 

    
    // ----- Cooling (только то, что реально есть) -----
    status.cooling.coolantTemp = m_thermal.getTemperature(); // берем из thermal
    status.cooling.totalEfficiency = m_cooling.getTotalEfficiency();
    status.cooling.allocatedPowerMW = m_cooling.getAllocatedPower();
    status.cooling.requestedPowerMW = m_cooling.getRequestedPower();
    status.cooling.radiatedPowerMW = m_cooling.getLastRadiatedPower();
    
    status.cooling.pumpCapacity = m_cooling.getPumpCapacity();
    status.cooling.pumpHeatMJ = m_cooling.getPumpHeatMJ();
    status.cooling.dt = m_cooling.getDT();
    
    // Панели
    int panelCount = m_cooling.getPanelCount();
    status.cooling.damagedPanelCount = 0;
    status.cooling.criticalPanelCount = 0;
    
    for (int i = 0; i < panelCount; ++i) {
        game::RadiatorPanelStatus panel;
        panel.health = m_cooling.getPanelHealth(i);
        panel.efficiency = m_cooling.getPanelEfficiency(i);
        status.cooling.panels.push_back(panel);
        
        if (panel.health < 0.7 && panel.health >= 0.3) 
            status.cooling.damagedPanelCount++;
        if (panel.health < 0.3) {
            status.cooling.criticalPanelCount++;
            status.cooling.failedPanelIndices.push_back(i);
        }
    }
    
    // ----- PowerBus -----
    status.powerBus.availablePowerMW = m_powerBus.availablePower();
    status.powerBus.overloaded = m_powerBus.overloaded();
    
    // Считаем totalRequested сами
    status.powerBus.totalRequestedMW = 0.0;
    for (auto* c : m_powerBus.getConsumers()) {
        game::PowerConsumerStatus cs;
        cs.name = c->getLabel();
        cs.requestedPowerMW = c->getRequestedPower();
        cs.allocatedPowerMW = c->getAvailablePower();
        cs.priority = static_cast<int>(c->getPriority());
        cs.operational = true;
        
        // Пытаемся привести к IHeatSource
        auto* heatSource = dynamic_cast<game::equipment::IHeatSource*>(c);
        if (heatSource) {
            cs.heatTransfer = heatSource->getHeatGeneration();
        } else {
            cs.heatTransfer = 1.0;
        }

        status.powerBus.consumers.push_back(cs);
        status.powerBus.totalRequestedMW += cs.requestedPowerMW;
    }
    
    // ----- Алерты -----
    
    // Реактор горячий
    if (status.reactor.temperature > status.reactor.criticalTemp * 0.85) {
        game::AlertStatus alert;
        alert.severity = status.reactor.temperature > status.reactor.criticalTemp * 0.95 ? 2 : 1;
        alert.system = "Reactor";
        alert.message = status.reactor.temperature > status.reactor.criticalTemp * 0.95 
            ? "CRITICAL TEMPERATURE" : "High temperature";
        alert.value = status.reactor.temperature;
        alert.threshold = status.reactor.criticalTemp * 0.85;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Reactor");
        
        if (alert.severity == 2) {
            status.criticalSystems.push_back("Reactor");
        }
    }
    
    // Антифриз горячий (лимит 900K)
    if (status.thermal.temperature > 1100.0) {
        game::AlertStatus alert;
        alert.severity = status.thermal.temperature > 1250.0 ? 2 : 1;
        alert.system = "Coolant";
        alert.message = status.thermal.temperature > 1250.0 
            ? "COOLANT SATURATED" : "Coolant high";
        alert.value = status.thermal.temperature;
        alert.threshold = 1250.0;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Coolant");
        
        if (alert.severity == 2) {
            status.criticalSystems.push_back("Coolant");
        }
    }
    
    // Повреждения панелей
    if (status.cooling.criticalPanelCount > 0) {
        game::AlertStatus alert;
        alert.severity = 2;
        alert.system = "Radiators";
        alert.message = std::to_string(status.cooling.criticalPanelCount) + " panels critical";
        alert.value = status.cooling.criticalPanelCount;
        alert.threshold = 0;
        status.alerts.push_back(alert);
        status.criticalSystems.push_back("Radiators");
    } else if (status.cooling.damagedPanelCount > 0) {
        game::AlertStatus alert;
        alert.severity = 1;
        alert.system = "Radiators";
        alert.message = std::to_string(status.cooling.damagedPanelCount) + " panels damaged";
        alert.value = status.cooling.damagedPanelCount;
        alert.threshold = 0;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Radiators");
    }
    
    // Перегрузка энергосистемы
    if (status.powerBus.overloaded) {
        game::AlertStatus alert;
        alert.severity = 1;
        alert.system = "Power";
        alert.message = "Power bus overloaded";
        alert.value = status.powerBus.totalRequestedMW;
        alert.threshold = status.powerBus.availablePowerMW;
        status.alerts.push_back(alert);
        status.warningSystems.push_back("Power");
    }
    
    return status;
}



// ------------ DAMAGE -----------
void ShipCore::applyDamage(const game::damage::DamageEvent& event)
{
    using namespace game::damage;

    DamageEvent localEvent = event;

    // перевод world → local
    localEvent.position = m_mathTransform.worldToLocal(event.position);

    auto result = m_hitComponent.resolve(localEvent);

    m_damageHandler.handleDamage(result);
}


void ShipCore::exportHitVolumes() const
{
    for (const auto& v : m_hitComponent.volumes)
    {
        std::cout
            << "HIT_VOLUME "
            << v.m_label << " "
            << v.center.x << " "
            << v.center.y << " "
            << v.center.z << " "
            << v.halfSize.x << " "
            << v.halfSize.y << " "
            << v.halfSize.z
            << std::endl;
    }
}



void ShipCore::restoreVolume(int index)
{
    if (index < 0 || index >= m_hitComponent.volumes.size())
        return;

    auto& v = m_hitComponent.volumes[index];

    v.health = 1.0f;
    v.destroyed = false;
}






}