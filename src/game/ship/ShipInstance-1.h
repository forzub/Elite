// #pragma once

// #include "game/ship/ShipDescriptor.h"
// #include "game/ship/ShipTransform.h"
// #include "game/ship/ShipController.h"
// #include "game/ship/ShipControlState.h"
// #include "game/ship/ShipCameraController.h"



// #include "ui/hud/HudEdgeMapper.h"

// struct ShipInstance
// {
//     // --- описание типа ---
//     const ShipDescriptor*                       desc = nullptr;

//     // --- состояние ---
//     ShipTransform                               transform;    // - перемещение
//     ShipControlState                            control;   // - управление

//     // --- подсистемы ---
//     ShipController                              controller;        // - функции движения
//     ShipCameraController                        cameraController;  // - контроль камеры

//     // --- HUD ---
//     HudEdgeMapper hudEdgeMapper;

//     // -------------------------------------------------
//     // Инициализация корабля
//     // -------------------------------------------------
//     void initialize(
//         const ShipDescriptor& descriptor,
//         int viewportW,
//         int viewportH
//     )
//     {
//         desc = &descriptor;

//         // HUD boundary из описания корабля
//         hudEdgeMapper.setBoundary(
//             desc->hud.edgeBoundary.contour,
//             viewportW,
//             viewportH
//         );
//     }

//     // -------------------------------------------------
//     // Update
//     // -------------------------------------------------
//     void update(
//         float dt,
//         const WorldParams& world
//     )
//     {
//        // -------------------------------------------------
//         // 1. Control → Transform (intent → physics)
//         // -------------------------------------------------
//         transform.cruiseActive    = control.cruiseActive;
//         transform.jumpActive      = control.jumpActive;

//         transform.pitchInput      = control.pitchInput;
//         transform.yawInput        = control.yawInput;
//         transform.rollInput       = control.rollInput;

//         transform.targetSpeedRate = control.targetSpeedRate;

//         transform.strafeInput     = control.strafeInput;
//         transform.liftInput       = control.liftInput;
//         transform.forwardInput    = control.forwardInput;

//         // -------------------------------------------------
//         // 2. Physics
//         // -------------------------------------------------
//         controller.update(
//             dt,
//             desc->physics,
//             transform,
//             world
//         );
//     }

//     // -------------------------------------------------
//     // Camera update
//     // -------------------------------------------------
//     void updateCamera(Camera& camera)
//     {
//         cameraController.update(
//             0.0f, // dt пока не нужен
//             transform,
//             camera
//         );
//     }
// };
