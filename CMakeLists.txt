cmake_minimum_required(VERSION 3.16)

project(EliteGame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/glad)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(EliteGame
    src/main.cpp

    glad/src/gl.c

    src/core/Application.cpp
    src/core/StateStack.cpp

    src/window/Window.cpp

    src/ui/MainMenuState.cpp
    src/ui/ConfirmExitState.cpp
    # src/ui/TradeMenuState.cpp
    # src/ui/NavigationMenuState.cpp
    # src/ui/ShipyardState.cpp
    # src/ui/EquipmentState.cpp
    # src/ui/ContractsState.cpp

    src/input/Input.cpp
    src/game/SpaceState.cpp

    src/render/Camera.cpp
    src/render/DebugGrid.cpp

)

set_source_files_properties(
    glad/src/gl.c
    PROPERTIES
    LANGUAGE C
)

target_include_directories(EliteGame PRIVATE src glad/include)
target_link_libraries(EliteGame PRIVATE glfw opengl32)

