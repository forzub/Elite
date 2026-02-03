cmake_minimum_required(VERSION 3.16)

project(EliteGame LANGUAGES C CXX)

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

    src/game/equipment/signalNode/processing/SignalReceiver.cpp
    src/game/equipment/signalNode/processing/WorldSignalTxSystem.cpp

    src/game/equipment/data/transmitters.cpp
    src/game/equipment/data/receivers.cpp
    src/game/equipment/data/jammers.cpp

    src/game/ship/hud/worldlabels/WorldLabelSystem.cpp
    src/game/ship/hud/worldlabels/WorldSignalWaves.cpp

    src/game/signals/SignalPattern.cpp
    src/game/signals/SignalPatternLibrary.cpp


    src/game/ship/descriptors/EliteCobraMk1.cpp

    src/game/ship/ShipController.cpp
    src/game/ship/ShipCameraController.cpp
    src/game/ship/Ship.cpp
    src/game/ship/ShipSignalController.cpp
 

    src/game/ship/sensors/ShipSignalPresentation.cpp

    src/galaxy/GalaxyDatabase.cpp
    


    src/render/Camera.cpp
    src/render/DebugGrid.cpp
    src/render/Renderer.cpp
    src/render/ScreenUtils.cpp
    src/render/ShaderUtils.cpp
    src/render/Font.cpp
    src/render/HUD/TextRenderer.cpp
    src/render/HUD/WorldLabelRenderer.cpp

)

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
)
