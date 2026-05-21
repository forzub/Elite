#include "ShipCore.h"
#include <iostream>
#include <algorithm>

#include "core/Log.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "src/world/modules/ObjectHitBuilder.h"
#include "src/world/modules/ObjectRuntimeHitBuilder.h"

#include "src/game/geometry/ObjLoader.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/world/modules/ObjectAssemblyTransformUtils.h"
#include "src/game/ship/view/ShipAttachmentResolver.h"
#include "src/game/ship/ShipAttachmentUtils.h"

#include "src/world/navigation/NavigationBodyAdapter.h"


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
    const ShipInitData&     initData,
    const glm::mat4&        orientation
)
{
    // устанавливаем роль корабля - ИГРОК / НПС
    // общий контекст - глобальные данные, viewport etc
    // геттер из файла описания корабля (f.e. Cobra MKIII)
    // позиция корабля в пространстве

    m_role = inRole;  
    m_desc = &descriptor;
    m_transform.setWorldPositionMeters(glm::dvec3(position));


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
       

    // m_transform.orientation = glm::mat4(1.0f);
    m_transform.orientation = orientation;

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


    // -------- DAMAGE / MODULE / ASSEMBLY -----------
    m_damageHandler.ship = this;

    initModuleRuntime();

    if (game::ship::geometry::AssemblyMeshLibrary::has(descriptor.typeId))
    {
        const auto& assembly = game::ship::geometry::AssemblyMeshLibrary::get(descriptor.typeId);
        m_assemblyRuntime.init(assembly);
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        descriptor.typeId,
        descriptor,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;
    
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
    
    
    // m_controller.update(
    //     dt,
    //     m_desc->physics,
    //     m_transform,
    //     world
    // );
    
  

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
            m_transform.worldPosition,
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
        m_transform.worldPosition,
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
    localEvent.position = m_transform.worldToLocal(event.position);

    auto result = m_hitComponent.resolve(localEvent);
    m_damageHandler.handleDamage(result);
    syncDestroyedModulesFromHitVolumes();

    if (m_desc)
    {
        world::modules::ObjectRuntimeHitBuilder::rebuild(
            m_hitComponent,
            m_desc->typeId,
            *m_desc,
            m_moduleRuntime,
            m_structuralLinkRuntime,
            m_assemblyRuntime
        );

        m_hitVolumesDirty = false;
    }
}



bool ShipCore::debugDestroyModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    auto* mod = m_moduleRuntime.findModule(moduleId);
    if (!mod)
    {
        std::cout
            << "[ShipCore] debugDestroyModuleById: module not found: "
            << moduleId << "\n";
        return false;
    }

    m_moduleRuntime.markModuleDestroyed(moduleId, 0.0f);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugDestroyModuleById: destroyed moduleId="
        << moduleId << "\n";

    return true;
}


bool ShipCore::debugRestoreModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    auto* mod = m_moduleRuntime.findModule(moduleId);
    if (!mod)
    {
        std::cout
            << "[ShipCore] debugRestoreModuleById: module not found: "
            << moduleId << "\n";
        return false;
    }

    m_moduleRuntime.restoreModuleBranch(moduleId);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugRestoreModuleById: restored moduleId="
        << moduleId << "\n";

    return true;
}

bool ShipCore::debugResetStructure()
{
    if (!m_desc)
        return false;

    m_moduleRuntime.init(m_desc->moduleDescriptors());
    m_structuralLinkRuntime.init(m_desc->moduleDescriptors());
    m_detachedFragmentRuntime.clear();
    m_repairJobRuntime.clear();

    if (game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
    {
        const auto& assembly =
            game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);
        m_assemblyRuntime.init(assembly);
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugResetStructure\n";
    return true;
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









void ShipCore::initModuleRuntime()
{
    if (!m_desc)
        return;

    m_moduleRuntime.init(m_desc->moduleDescriptors());
    m_structuralLinkRuntime.init(m_desc->moduleDescriptors());

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
}






void ShipCore::syncDestroyedModulesFromHitVolumes()
{
    bool supportChanged = false;

    for (const auto& volume : m_hitComponent.volumes)
    {
        if (volume.supportLinkVolume)
        {
            if (m_structuralLinkRuntime.setLinkHealth(
                    volume.supportLinkId,
                    volume.health,
                    volume.destroyed))
            {
                supportChanged = true;
            }

            // legacy mirror: пока оставляем для совместимости
            if (m_moduleRuntime.setSupportLinkHealth(
                    volume.moduleId,
                    volume.supportLinkId,
                    volume.health,
                    volume.destroyed))
            {
                supportChanged = true;
            }

            continue;
        }

        m_moduleRuntime.setModuleHealth(volume.moduleId, volume.health);

        if (volume.destroyed)
        {
            m_moduleRuntime.markModuleDestroyed(volume.moduleId, volume.health);
        }
    }

    if (supportChanged)
{
    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
}
    syncDetachedFragmentsFromModuleRuntime();
}




void ShipCore::updateAssemblyRuntime(double dt)
{
    // Обычная анимация/вращение модулей не должна пересобирать hit-volumes.
    // Rebuild нужен только при структурных изменениях:
    // damage, detach, restore, descriptor/assembly edit.
    m_assemblyRuntime.update(dt);
}



bool ShipCore::debugReattachModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_detachedFragmentRuntime.hasFragment(moduleId))
    {
        std::cout
            << "[ShipCore] debugReattachModuleById failed: no detached fragment for moduleId="
            << moduleId << "\n";
        return false;
    }

    if (!m_moduleRuntime.reattachModule(moduleId))
    {
        std::cout
            << "[ShipCore] debugReattachModuleById failed: module runtime rejected moduleId="
            << moduleId << "\n";
        return false;
    }

    const int restoredLinks =
        m_structuralLinkRuntime.restoreLinksTouchingModule(moduleId);

    m_detachedFragmentRuntime.removeFragment(moduleId);

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugReattachModuleById moduleId="
        << moduleId
        << " restoredLinks=" << restoredLinks
        << "\n";

    return true;
}






bool ShipCore::startRepairJobForModule(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    // Repair runtime работает в локальной системе корабля.
    // Никакой глобальной трансляции сюда передавать нельзя.
    const glm::mat4 ownerModel =
        m_transform.orientation;

        
    glm::vec3 ownerLinearVelocity =
        m_transform.forward() * m_transform.forwardVelocity +
        m_transform.right()   * m_transform.localVelocity.x +
        m_transform.up()      * m_transform.localVelocity.y;

    glm::vec3 dockWorldPosition =
        glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

    if (const auto dock =
            resolveShipAttachment(
                m_desc,
                m_transform,
                "repair_drone_dock_main",
                nullptr,
                nullptr))
    {
        dockWorldPosition =
            glm::vec3(ownerModel * glm::vec4(dock->localPosition, 1.0f));
    }


    glm::vec3 launchWorldPosition = dockWorldPosition;
    glm::vec3 recoveryWorldPosition = dockWorldPosition;

if (const auto launch =
        resolveShipAttachment(
            m_desc,
            m_transform,
            "repair_drone_launch_main",
            nullptr,
            nullptr))
{
    launchWorldPosition =
        glm::vec3(ownerModel * glm::vec4(launch->localPosition, 1.0f));
}

if (const auto recovery =
        resolveShipAttachment(
            m_desc,
            m_transform,
            "repair_drone_recovery_main",
            nullptr,
            nullptr))
{
    recoveryWorldPosition =
        glm::vec3(ownerModel * glm::vec4(recovery->localPosition, 1.0f));
}



std::vector<glm::vec3> repairWorkWorldPoints;

for (const auto* point :
        findShipAttachmentPointsByKind(
            m_desc,
            ShipAttachmentKind::RepairWorkPoint
        ))
{
    if (!point)
        continue;

    if (const auto resolved =
            resolveShipAttachment(
                m_desc,
                m_transform,
                point->id,
                nullptr,
                nullptr))
    {
        repairWorkWorldPoints.push_back(
            glm::vec3(ownerModel * glm::vec4(resolved->localPosition, 1.0f))
        );
    }
}

    const bool ok = m_repairJobRuntime.startJob(
    moduleId,
    ownerModel,
    ownerLinearVelocity,
    dockWorldPosition,
    launchWorldPosition,
    recoveryWorldPosition,
    repairWorkWorldPoints,
    m_detachedFragmentRuntime
);

    std::cout
        << "[ShipCore] startRepairJobForModule moduleId="
        << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}










std::vector<world::modules::ObjectMissingPartRequest>
ShipCore::buildMissingPartRequests() const
{
    std::vector<world::modules::ObjectMissingPartRequest> out;

    if (!m_desc)
        return out;

    if (!game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
        return out;

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);

    for (const auto& module : m_moduleRuntime.modules())
    {
        const bool missing =
            module.state == world::modules::ObjectModuleRuntimeState::Detached ||
            module.state == world::modules::ObjectModuleRuntimeState::Destroyed;

        if (!missing)
            continue;

        if (!module.repairable && !module.salvageable)
            continue;

        const auto* assemblyModule =
            world::modules::findAssemblyModuleById(
                assembly,
                module.moduleId
            );

        if (!assemblyModule)
            continue;

        world::modules::ObjectMissingPartRequest req;
        req.targetModuleId = module.moduleId;

        req.moduleClass = module.moduleClass;
        if (req.moduleClass.empty())
            req.moduleClass = module.moduleId;

        req.acceptedReplacementTags = module.acceptedReplacementTags;
        if (req.acceptedReplacementTags.empty())
            req.acceptedReplacementTags.push_back(module.moduleId);

        req.homeLocalModel =
            world::modules::buildAssemblyModuleHierarchicalLocalModel(
                assembly,
                m_assemblyRuntime,
                module.moduleId
            );



            // ВАЖНО:
// Если модуль уже Detached, его active hit-volume мог быть удалён из m_hitComponent
// после rebuild. Поэтому сначала берём homeCenterLocal из уже созданного detached fragment.
if (const auto* existingFragment =
        m_detachedFragmentRuntime.findFragment(module.moduleId))
{
    req.homeCenterLocal = existingFragment->homeCenterLocal;

    std::cout
        << "[ShipCore] missing slot center from detached fragment moduleId="
        << module.moduleId
        << " center=("
        << req.homeCenterLocal.x << ", "
        << req.homeCenterLocal.y << ", "
        << req.homeCenterLocal.z << ")"
        << "\n";
}
else
{
    glm::vec3 accumulatedCenter(0.0f);
    int accumulatedCount = 0;

    for (const auto& v : m_hitComponent.volumes)
    {
        if (v.moduleId != module.moduleId)
            continue;

        if (v.supportLinkVolume)
            continue;

        accumulatedCenter += v.center;
        ++accumulatedCount;
    }

    if (accumulatedCount > 0)
    {
        req.homeCenterLocal =
            accumulatedCenter / static_cast<float>(accumulatedCount);
    }
    else
    {
        req.homeCenterLocal =
            glm::vec3(req.homeLocalModel * glm::vec4(0, 0, 0, 1));

        std::cout
            << "[ShipCore] WARNING missing slot fallback center used moduleId="
            << module.moduleId
            << " center=("
            << req.homeCenterLocal.x << ", "
            << req.homeCenterLocal.y << ", "
            << req.homeCenterLocal.z << ")"
            << "\n";
    }
}








        out.push_back(std::move(req));
    }

    return out;
}

bool ShipCore::startRepairJobForClaimedReplacement(
    const std::string& targetModuleId,
    world::modules::ObjectDetachedFragmentRuntime& sourceRuntime,
    const std::string& sourceModuleId
)
{
    if (!m_desc)
        return false;

    const auto requests = buildMissingPartRequests();

    const world::modules::ObjectMissingPartRequest* targetRequest = nullptr;

    for (const auto& req : requests)
    {
        if (req.targetModuleId == targetModuleId)
        {
            targetRequest = &req;
            break;
        }
    }

    if (!targetRequest)
    {
        std::cout
            << "[ShipCore] repair failed: no missing slot targetModuleId="
            << targetModuleId << "\n";
        return false;
    }

    if (!m_detachedFragmentRuntime.claimFragmentAsReplacement(
        sourceRuntime,
        sourceModuleId,
        targetModuleId,
        targetRequest->homeLocalModel,
        targetRequest->homeCenterLocal))
    {
        return false;
    }

    // Важно: repair job теперь всегда работает с targetModuleId
    // внутри runtime целевого корабля.
    return startRepairJobForModule(targetModuleId);
}







bool ShipCore::debugDetachModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_moduleRuntime.debugForceDetach(moduleId))
        return false;

    syncDetachedFragmentsFromModuleRuntime();

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugDetachModuleById: moduleId="
              << moduleId << "\n";
    return true;
}




bool ShipCore::ejectCockpitCapsule()
{
    if (!m_desc)
        return false;

    constexpr const char* COCKPIT_MODULE_ID = "ship_cockpit";

    auto* cockpit = m_moduleRuntime.findModule(COCKPIT_MODULE_ID);
    if (!cockpit)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: cockpit module not found\n";
        return false;
    }

    if (!cockpit->detachable)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: cockpit is not detachable\n";
        return false;
    }

    if (cockpit->state == world::modules::ObjectModuleRuntimeState::Detached)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule ignored: cockpit already detached\n";
        return false;
    }

    if (!m_moduleRuntime.debugForceDetach(COCKPIT_MODULE_ID))
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: detach rejected\n";
        return false;
    }

    syncDetachedFragmentsFromModuleRuntime();

    const glm::vec3 forward = m_transform.forward();

    // Скорость подбери потом по вкусу.
    // Сейчас это именно "катапульта", не медленный отпад.
    const glm::vec3 ejectVelocity = forward * 35.0f;

    const glm::vec3 ejectAngularVelocity =
        glm::vec3(0.0f, 0.15f, 0.0f);

    if (!m_detachedFragmentRuntime.setFragmentMotion(
            COCKPIT_MODULE_ID,
            ejectVelocity,
            ejectAngularVelocity))
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: detached fragment was not spawned\n";
        return false;
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] COCKPIT CAPSULE EJECTED forward=("
        << forward.x << ", "
        << forward.y << ", "
        << forward.z << ")"
        << "\n";

    return true;
}





bool ShipCore::debugHangModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_moduleRuntime.debugForceHang(moduleId))
        return false;

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugHangModuleById: moduleId="
              << moduleId << "\n";
    return true;
}

bool ShipCore::debugReevaluateStructure()
{
    if (!m_desc)
        return false;

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
syncDetachedFragmentsFromModuleRuntime();

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugReevaluateStructure\n";
    return true;
}




bool ShipCore::debugSetStructuralLinkHealth(
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    if (!m_desc)
        return false;

    // if (!m_structuralLinkRuntime.setLinkHealth(linkId, health, destroyed))
    //     return false;

    if (!m_structuralLinkRuntime.setLinkHealth(linkId, health, destroyed))
{
    std::cout
        << "[ShipCore] debugSetStructuralLinkHealth FAILED linkId="
        << linkId << "\n";
    return false;
}

if (const auto* link = m_structuralLinkRuntime.findById(linkId))
{
    std::cout
        << "[ShipCore] set structural link health:"
        << " id=" << link->id
        << " A=" << link->moduleAId
        << " B=" << link->moduleBId
        << " kind=" << static_cast<int>(link->kind)
        << " health=" << link->health
        << " destroyed=" << link->destroyed
        << " loadBearing=" << link->loadBearing
        << "\n";
}


    for (auto& v : m_hitComponent.volumes)
    {
        if (v.supportLinkVolume && v.supportLinkId == linkId)
        {
            v.health = std::max(0.0f, health);
            v.destroyed = destroyed || v.health <= 0.0f;
        }
    }

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
    syncDetachedFragmentsFromModuleRuntime();


    if (const auto* link = m_structuralLinkRuntime.findById(linkId))
        {
            const auto* a = m_moduleRuntime.findModule(link->moduleAId);
            const auto* b = m_moduleRuntime.findModule(link->moduleBId);

            if (a)
            {
                std::cout
                    << "[ShipCore] module after link damage:"
                    << " module=" << a->moduleId
                    << " state=" << static_cast<int>(a->state)
                    << " aliveSupports=" << a->aliveSupportCount
                    << " detachable=" << a->detachable
                    << "\n";
            }

            if (b)
            {
                std::cout
                    << "[ShipCore] module after link damage:"
                    << " module=" << b->moduleId
                    << " state=" << static_cast<int>(b->state)
                    << " aliveSupports=" << b->aliveSupportCount
                    << " detachable=" << b->detachable
                    << "\n";
            }
        }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    return true;
}







void ShipCore::syncDetachedFragmentsFromModuleRuntime()
{
    if (!m_desc)
        return;

    if (!game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
        return;

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);

    const glm::mat4 ownerModel =    m_transform.orientation;

    m_detachedFragmentRuntime.syncFromDetachedModules(
        ownerModel,
        assembly,
        m_assemblyRuntime,
        m_moduleRuntime,
        m_hitComponent
    );
}

void ShipCore::updateDetachedFragments(float dt)
{
    m_detachedFragmentRuntime.update(dt);
}


void ShipCore::updateRepairJobs(float dt)
{
    if (!m_desc)
        return;

    if (m_repairJobRuntime.activeJobCount() <= 0)
        return;

    const glm::mat4 ownerModel =   m_transform.orientation;

    std::vector<world::navigation::NavigationObstacle> obstacles =
        buildNavigationObstaclesForRepair("");

    const auto completed =
        m_repairJobRuntime.update(
            dt,
            ownerModel,
            m_detachedFragmentRuntime,
            obstacles
        );

    for (const auto& moduleId : completed)
    {
        debugReattachModuleById(moduleId);
    }
}





std::vector<game::simulation::ObjectRepairJobSnapshot>
ShipCore::buildRepairJobSnapshots() const
{
    const glm::mat4 ownerModel =
    m_transform.orientation;

    return m_repairJobRuntime.buildSnapshots(
        ownerModel,
        m_transform.worldPosition,
        m_detachedFragmentRuntime
    );
}

int ShipCore::activeRepairJobCount() const
{
    return m_repairJobRuntime.activeJobCount();
}






std::vector<world::navigation::NavigationObstacle>
ShipCore::buildNavigationObstaclesForRepair(
    const std::string& ignoredModuleId
) const
{
    (void)ignoredModuleId;

    std::vector<world::navigation::NavigationObstacle> out;

    const auto bodies =
        world::navigation::buildNavigationBodiesForShip(*this);

    const glm::mat4 ownerModel =
    m_transform.orientation;

    const glm::mat3 ownerRotation =
        glm::mat3(ownerModel);

    for (const auto& body : bodies)
    {
        world::navigation::NavigationObstacle o;

        o.shape =
            world::navigation::NavigationObstacleShape::Box;

        o.center =
            glm::vec3(ownerModel * glm::vec4(body.center, 1.0f));

        o.halfExtents =
            body.halfExtents;

        o.rotation =
            ownerRotation;

        // Fallback sphere radius for old code/debug.
        o.radius =
            glm::length(body.halfExtents);

        o.entityId = body.entityId;

        out.push_back(o);
    }

    if (out.empty())
    {
        world::navigation::NavigationObstacle o;
        o.shape = world::navigation::NavigationObstacleShape::Sphere;
        o.center = glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));
        o.radius = 8.0f;
        out.push_back(o);
    }

    return out;
}


}