
#include "Ship.h"
#include <iostream>
#include <cstdio>
#include <fstream>







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
    const ShipInitData& initData,
    const glm::mat4& orientation
)
{
    m_core.init(
        inRole,
        descriptor,
        position,
        initData,
        orientation
    );
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
}

void Ship::updateMotionPhysics(float dt, const WorldParams& world)
{
    m_core.updateMotionPhysics(dt, world);
}

void Ship::updateMotionControl(float dt, const WorldParams& world)
{
    m_core.updateMotionControl(dt, world);
}

void Ship::propagateMotionOrientation(float dt)
{
    m_core.propagateMotionOrientation(dt);
}

void Ship::updateTranslationControl(float dt)
{
    m_core.updateTranslationControl(dt);
}

bool Ship::propagateHubTacticalTranslation(double dt)
{
    return m_core.propagateHubTacticalTranslation(dt);
}

void Ship::updateSystems(float dt)
{
    m_core.updateSystems(dt);
}


void Ship::updateSignals(
    float dt,
    const std::vector<WorldSignal>& worldSignals,
    const std::vector<Planet>& planets,
    const std::vector<InterferenceSource>& interferenceSources
)
{
    m_core.updateSignals(dt, worldSignals, planets, interferenceSources, m_id);
}


void Ship::updatePerception(
    float dt,
    const std::vector<world::RadarContactInput>& radarInputs
)
{
    m_core.updatePerception(dt, radarInputs);
}






void Ship::updateControlIntent()
{
    // ShipControlState already lives in the canonical ShipState. This seam is
    // now responsible only for synchronizing module-derived actuator state
    // before the physics/control phases consume capability.
    m_core.refreshRuntimeState();
}






//              ###      ###              ##       ##
//               ##       ##                       ##
//   ####        ##       ##             ###      #####    ####    ##  ##    #####
//      ##    #####    #####              ##       ##     ##  ##   #######  ##
//   #####   ##  ##   ##  ##              ##       ##     ######   ## # ##   #####
//  ##  ##   ##  ##   ##  ##              ##       ## ##  ##       ##   ##       ##
//   #####    ######   ######            ####       ###    #####   ##   ##  ######


void Ship::setControlState(const ShipControlState& newControl)
{
    m_core.state().control = newControl;
}


std::optional<WorldSignal> Ship::emitSignal() const
{
    const auto& tx = m_core.equipment().transmitter;

    auto signal = tx.emit(
        m_core.transform().worldPosition,
        m_id
    );

    if (signal)
        signal->systemId = m_core.transform().motion.systemId;

    return signal;
}


void Ship::applyCommand(const ClientShipCommand& cmd){
    switch (cmd.type)
    {
        case ClientShipCommand::Type::DamageRadiator: // пример
        {
            core().damageRadiatorPanel(cmd.index, cmd.amount);
            break;
        }

        case ClientShipCommand::Type::RepairAllPanels: // пример
        {
            core().repairAllRadiatorPanels();
            break;
        }

        default:
            break;
    }
}