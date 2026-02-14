
#include "Ship.h"
#include <iostream>

#include "src/input/Input.h"
#include "core/StateStack.h"
#include "src/game/signals/SignalPatternLibrary.h"
#include "src/game/equipment/EquipmentSlot.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/render/bitmap/TextureLoader.h"





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
    const ShipInitData& initData
)
{

    core.init(
        context,
        inRole,
        descriptor,
        position,
        initData
    );

    if (core.role() == ShipRole::Player)
    {
        view.init(context, &descriptor, core.transform());
    }

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
    if (core.role() != ShipRole::Player)
        return;

    inputMapper.update(control);

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
    core.updatePhysics(dt, world);     // ② движение
   
    // формирование сигнала передатчика
    core.m_signalController.update(dt, core.emittedSignal());


    if (core.emittedSignal().enabled)
        {
            core.emittedSignal().position = core.transform().position;
        }

    // ⬇ ВОТ ЭТОГО НЕ ХВАТАЕТ
    core.updateSignals(
        dt,
        worldSignals,
        planets,
        interferenceSources
    );

    //  4. HUD / AI — по роли
    core.updatePerception(dt);          // ④ осмысление - ветвление игрок / нпс
    updateCamera(dt);                   // ⑤ камера (только Player)

    view.updateCockpitState(core.role(), &core.desc(), core.transform(), core.equipment());
}




void Ship::updateControlIntent()
{
    if (core.role() == ShipRole::Player)
    {
        // control уже заполнен в handleInput()
        core.transform().cruiseActive    = control.cruiseActive;
        core.transform().pitchInput      = control.pitchInput;
        core.transform().yawInput        = control.yawInput;
        core.transform().rollInput       = control.rollInput;
        core.transform().targetSpeedRate = control.targetSpeedRate;
        core.transform().strafeInput     = control.strafeInput;
        core.transform().liftInput       = control.liftInput;
        core.transform().forwardInput    = control.forwardInput;
    }
    else
    {
        // NPC: control будет заполняться AI (пока нули)
        core.transform().cruiseActive    = false;
        core.transform().pitchInput      = 0.0f;
        core.transform().yawInput        = 0.0f;
        core.transform().rollInput       = 0.0f;
        core.transform().targetSpeedRate = 0.0f;
        core.transform().strafeInput     = 0.0f;
        core.transform().liftInput       = 0.0f;
        core.transform().forwardInput    = 0.0f;
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
    view.update(dt, core.role(), core.transform());
}




//    ##                         ##                                                    ##                                            ###
//                               ##                                                    ##                                             ##
//   ###     #####     #####    #####             ####    ######   ##  ##   ######    #####             ####     ####    ######       ##
//    ##     ##  ##   ##         ##              ##  ##    ##  ##  ##  ##    ##  ##    ##              ##  ##       ##    ##  ##   #####
//    ##     ##  ##    #####     ##              ##        ##      ##  ##    ##  ##    ##              ##        #####    ##      ##  ##
//    ##     ##  ##        ##    ## ##           ##  ##    ##       #####    #####     ## ##           ##  ##   ##  ##    ##      ##  ##
//   ####    ##  ##   ######      ###             ####    ####         ##    ##         ###             ####     #####   ####      ######
//                                                                 #####    ####

bool Ship::installCryptoCard(CryptoCard* card)
{
    if (!card) return false;
    if (!core.inventory().contains(card)) return false;

    for (auto& d : core.equipment().decryptors.modules)
    {
        if (!d.enabled) continue;

        if (d.install(card))
        {
            core.inventory().remove(card);
            return true;
        }
    }

    return false;
}



//                                                                                                       ##                                            ###
//                                                                                                       ##                                             ##
//  ######    ####    ##  ##    ####    ##  ##    ####              ####    ######   ##  ##   ######    #####             ####     ####    ######       ##
//   ##  ##  ##  ##   #######  ##  ##   ##  ##   ##  ##            ##  ##    ##  ##  ##  ##    ##  ##    ##              ##  ##       ##    ##  ##   #####
//   ##      ######   ## # ##  ##  ##   ##  ##   ######            ##        ##      ##  ##    ##  ##    ##              ##        #####    ##      ##  ##
//   ##      ##       ##   ##  ##  ##    ####    ##                ##  ##    ##       #####    #####     ## ##           ##  ##   ##  ##    ##      ##  ##
//  ####      #####   ##   ##   ####      ##      #####             ####    ####         ##    ##         ###             ####     #####   ####      ######
//                                                                                   #####    ####

bool Ship::removeCryptoCard(CryptoCard* card)
{
    if (!card) return false;

    for (auto& d : core.equipment().decryptors.modules)
    {
        if (d.remove(card))
        {
            core.inventory().add(card);
            return true;
        }
    }

    return false;
}


//              ###      ###              ##       ##
//               ##       ##                       ##
//   ####        ##       ##             ###      #####    ####    ##  ##    #####
//      ##    #####    #####              ##       ##     ##  ##   #######  ##
//   #####   ##  ##   ##  ##              ##       ##     ######   ## # ##   #####
//  ##  ##   ##  ##   ##  ##              ##       ## ##  ##       ##   ##       ##
//   #####    ######   ######            ####       ###    #####   ##   ##  ######


bool Ship::addItem(Item* item)
{
    if (!item) return false;

    if (item->usesCargoSpace())
    {
        if (!core.addCargo(item->cargoMass(), item->cargoVolume()))
            return false;
    }

    core.inventory().add(item);
    return true;
}


bool Ship::REMOVEItem(Item* item)
{
    if (!item) return false;

    if (!core.inventory().remove(item))
        return false;

    if (item->usesCargoSpace())
    {
        core.removeCargo(item->cargoMass(), item->cargoVolume());
    }

    return true;
}

