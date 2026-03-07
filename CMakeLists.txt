cmake_minimum_required(VERSION 3.16)

project(EliteGame LANGUAGES C CXX)

include_directories(
    ${CMAKE_SOURCE_DIR}/third_party/websocketpp
    ${CMAKE_SOURCE_DIR}/third_party/asio/asio/include  # добавили /asio
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(Freetype REQUIRED)

find_package(nlohmann_json REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/glad)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})


add_executable(EliteGame
    src/main.cpp

    glad/src/gl.c

    src/core/Application.cpp
    src/core/StateStack.cpp
    src/core/StateContext.cpp


    src/window/Window.cpp

    src/ui/MainMenuState.cpp
    src/ui/ConfirmExitState.cpp
    src/ui/HudRenderer.cpp
    src/ui/hud/HudEdgeMapper.cpp


    src/input/Input.cpp

    src/game/SpaceState.cpp
    src/game/SpaceState-Init.cpp

    src/game/equipment/signalNode/processing/SignalReceiver.cpp
    src/world/WorldSignalTxSystem.cpp

    src/game/equipment/data/transmitters.cpp
    src/game/equipment/data/receivers.cpp
    src/game/equipment/data/jammers.cpp
    src/game/equipment/data/decryptors.cpp

    src/game/ship/hud/worldlabels/WorldLabelSystem.cpp
    src/game/ship/hud/worldlabels/WorldSignalWaves.cpp

    src/game/signals/SignalPattern.cpp
    src/game/signals/SignalPatternLibrary.cpp


    src/game/ship/descriptors/EliteCobraMk1.cpp
    src/game/ship/descriptors/EliteCobraMk1_Cockpit.cpp

    src/game/ship/controller/PlayerInputMapper.cpp
    
    src/game/ship/ShipController.cpp
    src/game/ship/Ship.cpp
    src/game/ship/ShipDescriptorRegistry.cpp
    
    src/game/ship/core/ShipSignalController.cpp
    src/game/ship/core/ShipCore.cpp
    
    src/game/ship/view/PlayerShipView.cpp
    src/game/ship/view/ShipCameraController.cpp

    src/game/ship/sensors/ShipSignalPresentation.cpp

    src/galaxy/GalaxyDatabase.cpp

    src/game/ship/cockpit/CockpitMeshBuilder.cpp
    src/game/ship/cockpit/CockpitStrokeBuilder.cpp
    src/game/ship/cockpit/ShipCockpitUpdater.cpp
    
    src/render/Camera.cpp
    src/render/DebugGrid.cpp
    src/render/Renderer.cpp
    src/render/ScreenUtils.cpp
    src/render/ShaderUtils.cpp
    src/render/Font.cpp
    src/render/HUD/TextRenderer.cpp
    src/render/HUD/WorldLabelRenderer.cpp
    src/render/RenderCockpitPass.cpp
    src/render/RenderCockpitBitmapPass.cpp
    src/render/bitmap/TextureLoader.cpp
    src/render/camera/RenderCameraViewport.cpp

    src/ui/components/UICameraView.cpp
    src/ui/components/UIComponent.cpp
    src/ui/components/UIContainer.cpp
    src/ui/components/UIText.cpp

    src/game/simulation/GameSimulation.cpp
    src/game/simulation/NpcAiSystem.cpp

    src/game/client/ClientWorldState.cpp
    src/game/server/GameServer.cpp

    src/game/client/GameClient.cpp
    src/game/network/LocalLoopbackTransport.cpp
    src/game/shared/SharedShipPhysics.cpp

    src/game/equipment/DamageModel.cpp
    src/game/equipment/radar/RadarModule.cpp

    src/game/equipment/signalNode/ReceiverModule.cpp
    src/game/equipment/EquipmentModule.cpp

    src/game/ship/core/PowerBus.cpp
    src/game/ship/core/ThermalSystem.cpp
    src/game/ship/core/ReactorSystem.cpp

    src/game/equipment/data/radars.cpp
    src/game/equipment/signalNode/SignalTransmitterModule.cpp

    src/scene/SceneRenderer.cpp
    
    src/ui/components/radar/RadarWidgetBase.cpp
    src/ui/components/radar/RadarWidgetFactory.cpp
    src/ui/components/radar/RadarRenderTarget.cpp
    src/ui/components/radar/ppi/RadarPPIWidget.cpp

    src/ui/components/radar/ppi/effects/LCDEffect.cpp
    src/ui/components/radar/ppi/effects/CRTEffect.cpp

    src/game/ship/core/CoolingSystem.cpp


    src/WebSocket/DebugServer.cpp
    src/game/ship/core/systems/CoreSystem.cpp

    src/game/damage/DamageTestObject.cpp
    src/game/damage/DamageSystem.cpp
    src/game/ship/damage/ShipDamageHandler.cpp
    src/game/ship/core/ShipHitBuilder.cpp

    src/game/damage/TestDamageHandler.cpp

)

add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/src/assets
            ${CMAKE_BINARY_DIR}/assets)

add_dependencies(EliteGame copy_assets)


set_source_files_properties(
    glad/src/gl.c
    PROPERTIES
    LANGUAGE C
)

target_compile_definitions(EliteGame PRIVATE GLFW_INCLUDE_NONE)

target_include_directories(EliteGame PRIVATE
    src
    glad/include
)

target_link_libraries(EliteGame PRIVATE
    glfw
    opengl32
    Freetype::Freetype
    nlohmann_json::nlohmann_json
    
    # Библиотеки для ASIO/WebSocket
    ws2_32      # уже есть
    mswsock     # НУЖНО ДОБАВИТЬ (для AcceptEx)
    iphlpapi    # можно добавить на всякий случай
    pthread     # уже есть
    
    # Порядок важен - сначала наши, потом системные
    ${FREETYPE_LIBRARY}
    glfw3
    opengl32
)
