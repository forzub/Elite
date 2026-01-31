#include "Ship.h"

#include "src/input/Input.h"
#include "core/StateStack.h"


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
    StateContext& context, 
    const ShipDescriptor& descriptor, 
    glm::vec3 position
)
{
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

    // оборудование
    equipment.receiver    = desc->receiver;
    equipment.transmitter = desc->transmitter;

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
    updateSignals(                // ③ физика сигналов
        dt,
        worldSignals,
        planets,
        interferenceSources
    );
    updatePerception(dt);         // ④ осмысление
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
            signalResults
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