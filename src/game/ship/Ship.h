// game/ship/Ship.h
#pragma once
#include <vector>


#include "game/ship/CargoBay.h"
#include "game/ship/ShipInventory.h"

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


// #include "game/equipment/decryptor/DecryptorModule.h"
#include "src/game/equipment/decryptor/DecryptorDesc.h"

#include "game/ship/sensors/ShipSignalPresentation.h"
#include "game/ship/sensors/NpcSignalAwareness.h"

#include "src/game/equipment/signalNode/processing/SignalReceiver.h"

#include "world/WorldSignal.h" 

#include "src/render/Camera.h"

#include "src/ui/hud/HudEdgeMapper.h"

#include "core/StateContext.h"
#include "game/core/QuantitySlot.h"




struct ShipSystemState
{
    QuantitySlot reactor;
    QuantitySlot engine;
    QuantitySlot radar;
    QuantitySlot weapon;
    QuantitySlot decryptor;
    QuantitySlot jammer;
    QuantitySlot dockingComputer;
    QuantitySlot receiver;
    QuantitySlot transmitter;
    QuantitySlot utility;
    QuantitySlot fuelScop;
    QuantitySlot tractorBeam;
};

struct ShipDockState
{
    QuantitySlot fighter;
    QuantitySlot shuttle;
    QuantitySlot drone;
};

// главный класс описания корабля, общий для всех летающих
// описание находится в папке description
// связь с файлом описания через структуру - ShipDecription

struct Ship
{
    // ShipRole role = ShipRole::Player;
    ShipRole                                role = ShipRole::NPC;

    // персональные данные корабля. тип, имя и т.п.
    ShipIdentity                            identity;

    // --- описание типа ---
    const ShipDescriptor*                   desc = nullptr;

    // ───────────────
    // Хранилища
    // ───────────────
    CargoBay                                cargo;
    ShipInventory                           inventory;

    // --- состояние ---
    ShipTransform                           transform;          // - перемещение
    ShipControlState                        control;            // - управление
    
    // --- подсистемы ---
    ShipController                          controller;         // - функции движения
    Camera                                  camera;             // камера, следующая за кораблём
    ShipCameraController                    cameraController;   // - контроль камеры
    
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
    
    ShipSystemState                         systems;
    ShipDockState                           docks;

    // восприятие связи NPC
    NpcSignalAwareness                      npcAwareness;

    // --- lifecycle ---
    void init(
        StateContext& context, 
        ShipRole role,
        const ShipDescriptor& descriptor, 
        glm::vec3 position,
        const ShipIdentity* customIdentity = nullptr
    );
    
    void handleInput();

    void update(
        float dt,
        const WorldParams&                          world,
        const std::vector<WorldSignal>&             worldSignals,
        const std::vector<Planet>&                  planets,
        const std::vector<InterferenceSource>&      interferenceSources
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


    // ───────────────
    // Cargo API
    // ───────────────
    int  freeCargoMass() const;
    int  freeCargoVolume() const;
    bool addCargo(int mass, int volume);
    bool removeCargo(int mass, int volume);
    
    
    // ───────────────
    // Инициализация корабля
    // ───────────────
    void initShipSlotsFromDescriptor(const ShipDescriptor& descriptor);

    // ───────────────
    // Инициализация оборудования
    // ───────────────   
    void installDefaultEquipment(const ShipDescriptor& descriptor);

    template<typename ModuleType, typename DescType>
    bool installEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module,
        const DescType& desc
    );
    
    template<typename ModuleType>
    bool removeEquipment(
        const char* equipmentName,
        QuantitySlot& slot,
        ModuleType& module
    );


    // ───────────────
    // перенос криптокарт из shipInventory в decryptor и обратно
    // ─────────────── 
    bool installCryptoCard(CryptoCard* card);
    bool removeCryptoCard(CryptoCard* card);

};

