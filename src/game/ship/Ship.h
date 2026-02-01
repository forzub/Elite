// game/ship/Ship.h
#pragma once

#pragma once

#include "src/game/ship/ShipCameraController.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/ShipParams.h"
#include "src/game/ship/ShipTransform.h"
#include "src/game/ship/ShipHudProfile.h"
#include "src/game/ship/ShipControlState.h"
#include "src/game/ship/ShipController.h"
#include "src/game/ship/ShipRole.h"
#include "src/game/ship/ShipSignalController.h"


#include "src/game/equipment/ShipEquipment.h"
#include "src/game/equipment/ShipEquipmentDesc.h"


#include "game/ship/sensors/ShipSignalPresentation.h"
#include "game/ship/sensors/NpcSignalAwareness.h"

#include "src/game/equipment/signalNode/processing/SignalReceiver.h"

#include "world/WorldSignal.h" 

#include "src/render/Camera.h"

#include "src/ui/hud/HudEdgeMapper.h"

#include "core/StateContext.h"

// главный класс описания корабля, общий для всех летающих
// описание находится в папке description
// связь с файлом описания через структуру - ShipDecription

struct Ship
{
    // ShipRole role = ShipRole::Player;
    ShipRole                                role = ShipRole::NPC;


    // --- описание типа ---
    const ShipDescriptor*                   desc = nullptr;


    // --- состояние ---
    ShipTransform                           transform;    // - перемещение
    ShipControlState                        control;   // - управление


    // --- подсистемы ---
    ShipController                          controller;        // - функции движения
    Camera                                  camera;            // камера, следующая за кораблём
    ShipCameraController                    cameraController;  // - контроль камеры


    // --- HUD ---
    HudEdgeMapper                           hudEdgeMapper;
    ShipHudProfile                          hudProfile;


    // работа со связью
    // RX
    SignalReceiver                          signalReceiver;
    std::vector<SignalReceptionResult>      signalResults;
    ShipSignalPresentation                  signalPresentation;
    // TX
    WorldSignal                             emittedSignal; 
    ShipSignalController                    signalController;

    // оборудование
    ShipEquipment                           equipment;


    // восприятие связи NPC
    NpcSignalAwareness                      npcAwareness;


    // --- lifecycle ---
    void init(
        StateContext& context, 
        ShipRole role,
        const ShipDescriptor& descriptor, 
        glm::vec3 position
    );
    
    void handleInput();

    void update(
        float dt,
        const WorldParams& world,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources
    );

    void updateControlIntent();
    void updatePhysics(float dt, const WorldParams& world);
    void updatePerception(float dt);
    void updateCamera(float dt);

    void updateSignals(
        float dt,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources
    );

    const WorldSignal& getEmittedSignal() const { return emittedSignal; }

};

