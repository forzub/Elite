
#include "Ship.h"
#include <iostream>
#include <cstdio>

#include "src/input/Input.h"

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
    ShipRole inRole,
    const ShipDescriptor& descriptor, 
    glm::vec3 position,
    const ShipInitData& initData
)
{

    m_core.init(
        inRole,
        descriptor,
        position,
        initData
    );

    m_core.emittedSignal().owner = m_id;
    
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


void Ship::applyControl()
{
    updateControlIntent();
}

void Ship::updatePhysics(float dt, const WorldParams& world)
{
    m_core.updatePhysics(dt, world);

    m_core.m_signalController.update(dt, m_core.emittedSignal());

    if (m_core.emittedSignal().enabled)
        m_core.emittedSignal().position = m_core.transform().position;
}


void Ship::updateSignals(
    float dt,
    const std::vector<WorldSignal>& worldSignals,
    const std::vector<Planet>& planets,
    const std::vector<InterferenceSource>& interferenceSources
)
{
    m_core.updateSignals(dt, worldSignals, planets, interferenceSources);
}


void Ship::updatePerception(float dt)
{
    m_core.updatePerception(dt);
}







void Ship::updateControlIntent()
{
        
        // control уже заполнен в handleInput()
        m_core.transform().cruiseActive    = m_control.cruiseActive;
        m_core.transform().pitchInput      = m_control.pitchInput;
        m_core.transform().yawInput        = m_control.yawInput;
        m_core.transform().rollInput       = m_control.rollInput;
        m_core.transform().targetSpeedRate = m_control.targetSpeedRate;
        m_core.transform().strafeInput     = m_control.strafeInput;
        m_core.transform().liftInput       = m_control.liftInput;
        m_core.transform().forwardInput    = m_control.forwardInput;
    
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
    if (!m_core.inventory().contains(card)) return false;

    for (auto& d : m_core.equipment().decryptors.modules)
    {
        if (!d.enabled) continue;

        if (d.install(card))
        {
            m_core.inventory().remove(card);
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

    for (auto& d : m_core.equipment().decryptors.modules)
    {
        if (d.remove(card))
        {
            m_core.inventory().add(card);
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
        if (!m_core.addCargo(item->cargoMass(), item->cargoVolume()))
            return false;
    }

    m_core.inventory().add(item);
    return true;
}


bool Ship::removeItem(Item* item)
{
    if (!item) return false;

    if (!m_core.inventory().remove(item))
        return false;

    if (item->usesCargoSpace())
    {
        m_core.removeCargo(item->cargoMass(), item->cargoVolume());
    }

    return true;
}

void Ship::setControlState(const ShipControlState& newControl)
{
    

    m_control = newControl;
}
