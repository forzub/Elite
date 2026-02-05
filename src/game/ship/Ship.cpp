#include "Ship.h"
#include <iostream>

#include "src/input/Input.h"
#include "core/StateStack.h"
#include "src/game/signals/SignalPatternLibrary.h"
#include "src/game/equipment/EquipmentSlot.h"

#include "src/game/ship/descriptors/EliteCobraMk1.h"




// -------------------------------------------------
//    ##                ##       ##
//                               ##
//   ###     #####     ###      #####
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ##
//    ##     ##  ##     ##       ## ##
//   ####    ##  ##    ####       ###
// -------------------------------------------------
void Ship::init(
    StateContext& context, 
    ShipRole inRole,
    const ShipDescriptor& descriptor, 
    glm::vec3 position,
    const ShipIdentity* customIdentity
)
{
    // устанавливаем роль корабля - ИГРОК / НПС
    // общий контекст - глобальные данные, viewport etc
    // геттер из файла описания корабля (f.e. Cobra MKIII)
    // позиция корабля в пространстве

    role = inRole;  
    desc = &descriptor;
    transform.position = position;
    
    if (role == ShipRole::Player){
        
        // камера
        camera.setAspect(context.aspect());
        camera.setPosition(transform.position);
        
        const Viewport& vp = context.viewport();

        // HUD boundary
        hudEdgeMapper.setBoundary(
            desc->hud.edgeBoundary.contour,
            vp.width,
            vp.height
        );
    }
    
    // собираем корабль
    // Устанавливаем оборудование по умолчанию из дескриптора
    initShipSlotsFromDescriptor(descriptor);
    installDefaultEquipment(descriptor);

    // Устанавливаем identity
    if (customIdentity) {
        // Используем кастомное имя (например, от игрока)
        this->identity = *customIdentity;
    } else {
        // Используем имя из дескриптора
        this->identity = descriptor.identity;
    }


    // 2. базовое состояние
    auto& tx  = equipment.transmitter;
    auto& sig = emittedSignal;

    sig.displayClass        = tx.displayClass;
    sig.power               = tx.txPower;
    sig.maxRange            = tx.baseRange;
    
    sig.position            = transform.position;
    sig.type                = SignalType::Transponder;
    sig.enabled             = true;
    sig.owner               = this;
    sig.label               = desc->identity.shipName;

    sig.address.actor       = 0xFFFFFFFF;
    sig.address.channel     = 0; // пока всегда публично

    // 3. контроллер сигналов
    signalController.init(emittedSignal);
}




//  ###                           ###              ##                                  ##
//   ##                            ##                                                  ##
//   ##       ####    #####        ##             ###     #####    ######   ##  ##    #####
//   #####       ##   ##  ##    #####              ##     ##  ##    ##  ##  ##  ##     ##
//   ##  ##   #####   ##  ##   ##  ##              ##     ##  ##    ##  ##  ##  ##     ##
//   ##  ##  ##  ##   ##  ##   ##  ##              ##     ##  ##    #####   ##  ##     ## ##
//  ###  ##   #####   ##  ##    ######            ####    ##  ##    ##       ######     ###
//                                                                 ####

void Ship::handleInput()
{
    if (role != ShipRole::Player)
        return;


    auto& ctrl = control;
    ctrl = ShipControlState{};

    ctrl.cruiseActive = Input::instance().isKeyPressed(GLFW_KEY_J);

    // --- Rotation (disabled in cruise) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.pitchInput =
            (Input::instance().isKeyPressed(GLFW_KEY_W) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_S) ? 1.0f : 0.0f);

        ctrl.rollInput =
            (Input::instance().isKeyPressed(GLFW_KEY_A) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_D) ? 1.0f : 0.0f);

        ctrl.yawInput =
            (Input::instance().isKeyPressed(GLFW_KEY_Q) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_E) ? 1.0f : 0.0f);
    }
    else
    {
        ctrl.pitchInput = 0.0f;
        ctrl.rollInput  = 0.0f;
        ctrl.yawInput   = 0.0f;
    }

    // --- Target speed control ---
    ctrl.targetSpeedRate = 0.0f;

    if (Input::instance().isKeyPressed(GLFW_KEY_KP_ADD) ||
        Input::instance().isKeyPressed(GLFW_KEY_EQUAL))
        ctrl.targetSpeedRate = +1.0f;

    if (Input::instance().isKeyPressed(GLFW_KEY_KP_SUBTRACT) ||
        Input::instance().isKeyPressed(GLFW_KEY_MINUS))
        ctrl.targetSpeedRate = -1.0f;

    // --- Manoeuvre thrusters (disabled in cruise) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.strafeInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_6) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_4) ? 1.0f : 0.0f);

        ctrl.forwardInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_8) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_2) ? 1.0f : 0.0f);

        ctrl.liftInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_9) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_3) ? 1.0f : 0.0f);
    }
    else
    {
        ctrl.strafeInput  = 0.0f;
        ctrl.forwardInput = 0.0f;
        ctrl.liftInput    = 0.0f;
    }

    if (Input::instance().isKeyPressed(GLFW_KEY_7)){
        std::cout
        << "[GLFW_KEY_7] start: "
        << "enabled=" << emittedSignal.enabled
        << " type=" << static_cast<int>(emittedSignal.type)
        << std::endl;

        signalController.requestSignal(SignalType::Transponder);

        std::cout
        << "[GLFW_KEY_7] done: "
        << "enabled=" << emittedSignal.enabled
        << " type=" << static_cast<int>(emittedSignal.type)
        << std::endl;
    }

    if (Input::instance().isKeyPressed(GLFW_KEY_8)){
        std::cout
        << "[GLFW_KEY_8] start: "
        << "enabled=" << emittedSignal.enabled
        << " type=" << static_cast<int>(emittedSignal.type)
        << std::endl;

        signalController.requestSignal(SignalType::Beacon);

        std::cout
        << "[GLFW_KEY_8] done: "
        << "enabled=" << emittedSignal.enabled
        << " type=" << static_cast<int>(emittedSignal.type)
        << std::endl;
    }

    if (Input::instance().isKeyPressed(GLFW_KEY_9)){
        std::cout
        << "[GLFW_KEY_9] start: "
        << "enabled=" << emittedSignal.enabled
        << " type=" << static_cast<int>(emittedSignal.type)
        << std::endl;

        signalController.requestSignal(SignalType::SOSModern);

        std::cout
        << "[GLFW_KEY_9] done: "
        << "enabled=" << emittedSignal.enabled
        << " type=" << static_cast<int>(emittedSignal.type)
        << std::endl;
        }

    if (Input::instance().isKeyPressed(GLFW_KEY_0))
        signalController.disableSignal();
}




// -------------------------------------------------
//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####

// -------------------------------------------------
void Ship::update(
    float dt,
    const WorldParams& world,
    const std::vector<WorldSignal>& worldSignals,
    const std::vector<Planet>& planets,
    const std::vector<InterferenceSource>& interferenceSources
)
{
    updateControlIntent();        // ① откуда intent
    updatePhysics(dt, world);     // ② движение
   
    // формирование сигнала передатчика
    signalController.update(dt, emittedSignal);


    if (emittedSignal.enabled)
    {
        emittedSignal.position     = transform.position;
    }


    if (equipment.receiver.enabled)
    {
        updateSignals(
            dt,
            worldSignals,
            planets,
            interferenceSources
        );
    }


    //  4. HUD / AI — по роли
    updatePerception(dt);         // ④ осмысление - ветвление игрок / нпс

    updateCamera(dt);             // ⑤ камера (только Player)
}





void Ship::updateSignals(
        float dt,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources
    )
{
    signalResults.clear();

    signalReceiver.update(
        dt,
        transform.position,
        equipment.receiver,
        worldSignals,
        planets,
        interferenceSources,
        signalResults,
        this
    );
}


void Ship::updateControlIntent()
{
    if (role == ShipRole::Player)
    {
        // control уже заполнен в handleInput()
        transform.cruiseActive    = control.cruiseActive;
        transform.pitchInput      = control.pitchInput;
        transform.yawInput        = control.yawInput;
        transform.rollInput       = control.rollInput;
        transform.targetSpeedRate = control.targetSpeedRate;
        transform.strafeInput     = control.strafeInput;
        transform.liftInput       = control.liftInput;
        transform.forwardInput    = control.forwardInput;
    }
    else
    {
        // NPC: control будет заполняться AI (пока нули)
        transform.cruiseActive    = false;
        transform.pitchInput      = 0.0f;
        transform.yawInput        = 0.0f;
        transform.rollInput       = 0.0f;
        transform.targetSpeedRate = 0.0f;
        transform.strafeInput     = 0.0f;
        transform.liftInput       = 0.0f;
        transform.forwardInput    = 0.0f;
    }
}


void Ship::updatePhysics(float dt, const WorldParams& world)
{
    controller.update(
        dt,
        desc->physics,
        transform,
        world
    );
}


void Ship::updatePerception(float dt)
{
    if (role == ShipRole::Player)
    {
        // HUD / визуальная семантика
        signalPresentation.update(dt, signalResults);
    }
    else
    {
        // AI-восприятие
        npcAwareness.update(signalResults);
    }
}




//                       ###              ##
                    //    ##              ##
//  ##  ##   ######       ##    ####     #####    ####              ####     ####    ##  ##    ####    ######    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##            ##  ##       ##   #######  ##  ##    ##  ##      ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######            ##        #####   ## # ##  ######    ##       #####
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##                ##  ##   ##  ##   ##   ##  ##        ##      ##  ##
//   ######   ##       ######   #####      ###    #####             ####     #####   ##   ##   #####   ####      #####
//           ####

void Ship::updateCamera(float dt)
{
    if (role != ShipRole::Player)
        return;

    cameraController.update(
        dt,
        transform,
        camera
    );
}



// ─────────────────────────
// Cargo API
// ─────────────────────────

int Ship::freeCargoMass() const
{
    return cargo.mass.free();
}

int Ship::freeCargoVolume() const
{
    return cargo.volume.free();
}

bool Ship::addCargo(int mass, int volume)
{
    if (!cargo.mass.canAdd(mass)) return false;
    if (!cargo.volume.canAdd(volume)) return false;

    cargo.mass.add(mass);
    cargo.volume.add(volume);
    return true;
}

bool Ship::removeCargo(int mass, int volume)
{
    if (!cargo.mass.canRemove(mass)) return false;
    if (!cargo.volume.canRemove(volume)) return false;

    cargo.mass.remove(mass);
    cargo.volume.remove(volume);
    return true;
}



//            ###                ##                         ##                ##       ##
//             ##                ##                                                    ##
//   #####     ##      ####     #####    #####             ###     #####     ###      #####
//  ##         ##     ##  ##     ##     ##                  ##     ##  ##     ##       ##
//   #####     ##     ##  ##     ##      #####              ##     ##  ##     ##       ##
//       ##    ##     ##  ##     ## ##       ##             ##     ##  ##     ##       ## ##
//  ######    ####     ####       ###   ######             ####    ##  ##    ####       ###


void Ship::initShipSlotsFromDescriptor(const ShipDescriptor& descriptor)
{
    desc = &descriptor;

    // Cargo
    cargo.mass.capacity   = desc->storage.cargoMass;
    cargo.mass.value      = 0;
    cargo.volume.capacity = desc->storage.cargoVolume;
    cargo.volume.value    = 0;

    // System slots
    systems.reactor.capacity            = desc->systems.reactorSlots;
    systems.engine.capacity             = desc->systems.engineSlots;
    systems.radar.capacity              = desc->systems.radarSlots;
    systems.weapon.capacity             = desc->systems.weaponSlots;
    systems.dockingComputer.capacity    = desc->systems.dockingComputerSlots;
    systems.decryptor.capacity          = desc->systems.decryptorSlots;
    systems.jammer.capacity             = desc->systems.jammerSlots;
    systems.receiver.capacity           = desc->systems.receiverSlots;
    systems.transmitter.capacity        = desc->systems.transmitterSlots;
    systems.utility.capacity            = desc->systems.utilitySlots;
    systems.fuelScop.capacity           = desc->systems.fuelScopSlots;
    systems.tractorBeam.capacity        = desc->systems.tractorBeamSlots;

    // Dock slots
    docks.fighter.capacity              = desc->docks.fighterSlots;
    docks.shuttle.capacity              = desc->docks.shuttleSlots;
    docks.drone.capacity                = desc->docks.droneSlots;


}




// bool Ship::installDecryptor(const DecryptorDesc& desc)
// {
//     // 1. есть ли вообще слот
//     if (systems.decryptor.capacity <= 0)
//         return false;

//     // 2. есть ли свободный
//     if (systems.decryptor.value >= systems.decryptor.capacity)
//         return false;

//     // 3. не установлен ли уже
//     if (equipment.decryptor.enabled)
//         return false;

//     // 4. установка
//     equipment.decryptor.init(desc);
//     systems.decryptor.value++;

//     return true;
// }



// bool Ship::removeDecryptor()
// {
//     if (!equipment.decryptor.enabled)
//         return false;

//     equipment.decryptor.enabled = false;
//     equipment.decryptor.health  = 0.0f;

//     systems.decryptor.value--;
//     return true;
// }




// bool Ship::hasDecryptor() const
// {
//     return equipment.decryptor.enabled;
// }





//    ##                         ##               ###      ###                                           ##                                           ##
//                               ##                ##       ##                                                                                        ##
//   ###     #####     #####    #####    ####      ##       ##               ####     ######  ##  ##    ###     ######   ##  ##    ####    #####     #####
//    ##     ##  ##   ##         ##         ##     ##       ##              ##  ##   ##  ##   ##  ##     ##      ##  ##  #######  ##  ##   ##  ##     ##
//    ##     ##  ##    #####     ##      #####     ##       ##              ######   ##  ##   ##  ##     ##      ##  ##  ## # ##  ######   ##  ##     ##
//    ##     ##  ##        ##    ## ##  ##  ##     ##       ##              ##        #####   ##  ##     ##      #####   ##   ##  ##       ##  ##     ## ##
//   ####    ##  ##   ######      ###    #####    ####     ####              #####       ##    ######   ####     ##      ##   ##   #####   ##  ##      ###
//                                                                                      ####                    ####


template<typename ModuleType, typename DescType>
bool Ship::installEquipment(
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

//                                                                                              ##                                           ##
//                                                                                                                                           ##
//  ######    ####    ##  ##    ####    ##  ##    ####              ####     ######  ##  ##    ###     ######   ##  ##    ####    #####     #####
//   ##  ##  ##  ##   #######  ##  ##   ##  ##   ##  ##            ##  ##   ##  ##   ##  ##     ##      ##  ##  #######  ##  ##   ##  ##     ##
//   ##      ######   ## # ##  ##  ##   ##  ##   ######            ######   ##  ##   ##  ##     ##      ##  ##  ## # ##  ######   ##  ##     ##
//   ##      ##       ##   ##  ##  ##    ####    ##                ##        #####   ##  ##     ##      #####   ##   ##  ##       ##  ##     ## ##
//  ####      #####   ##   ##   ####      ##      #####             #####       ##    ######   ####     ##      ##   ##   #####   ##  ##      ###
//                                                                             ####                    ####

template<typename ModuleType>
bool Ship::removeEquipment(
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



//    ##                         ##               ###      ###                                           ##                          ###              ###
//                               ##                ##       ##                                                                        ##             ## ##
//   ###     #####     #####    #####    ####      ##       ##               ####     ######  ##  ##    ###     ######                ##    ####      #
//    ##     ##  ##   ##         ##         ##     ##       ##              ##  ##   ##  ##   ##  ##     ##      ##  ##            #####   ##  ##   ####
//    ##     ##  ##    #####     ##      #####     ##       ##              ######   ##  ##   ##  ##     ##      ##  ##           ##  ##   ######    ##
//    ##     ##  ##        ##    ## ##  ##  ##     ##       ##              ##        #####   ##  ##     ##      #####            ##  ##   ##        ##
//   ####    ##  ##   ######      ###    #####    ####     ####              #####       ##    ######   ####     ##                ######   #####   ####
//                                                                                      ####                    ####

void Ship::installDefaultEquipment(const ShipDescriptor& descriptor)
{
    const auto& presets = descriptor.defaultEquipment;
    
    // Устанавливаем оборудование, если оно есть в пресетах
    if (presets.decryptor.has_value()) {

        installEquipment("decryptor", 
        systems.decryptor, 
        equipment.decryptor, 
        *presets.decryptor);
    }
    
    if (presets.jammer.has_value()) {

        installEquipment("jammer", 
        systems.jammer, 
        equipment.jammer, 
        *presets.jammer);
    }
    
    if (presets.receiver.has_value()) {
        
        installEquipment("receiver", 
        systems.receiver, 
        equipment.receiver, 
        *presets.receiver);
    }
    
    if (presets.transmitter.has_value()) {

        installEquipment("transmitter", 
        systems.transmitter, 
        equipment.transmitter, 
        *presets.transmitter);
    }

}


    // ───────────────
    // перенос криптокарт из shipInventory в decryptor и обратно
    // ──────────────

    bool Ship::installCryptoCard(CryptoCard* card)
{
    if (!card) return false;

    // карта должна быть в инвентаре
    if (!inventory.contains(card))
        return false;

    // decryptor — объект, проверяем состояние
    if (!equipment.decryptor.enabled)
        return false;

    if (!equipment.decryptor.install(card))
        return false;

    inventory.remove(card);
    return true;
}

bool Ship::removeCryptoCard(CryptoCard* card)
{
    if (!card) return false;

    if (!equipment.decryptor.enabled)
        return false;

    if (!equipment.decryptor.remove(card))
        return false;

    inventory.add(card);
    return true;
}
