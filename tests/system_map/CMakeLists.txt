cmake_minimum_required(VERSION 3.20)

project(SystemMapBehaviorTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_executable(
    system_map_behavior_tests
    SystemMapBehaviorTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/system_map/SystemMapInteraction.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/system_map/SystemMapSceneRenderer.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/system_map/SystemMapView.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/system_map/LocalMapInteraction.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/system_map/DetailMapSceneRenderer.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/system_map/HubMapSceneRenderer.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/CubicNavigationGrid.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/GalaxyNavigationConfig.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/SystemNavigationGrid.cpp"
)

target_include_directories(
    system_map_behavior_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        system_map_behavior_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        system_map_behavior_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME system_map_behavior
    COMMAND system_map_behavior_tests
)

set_tests_properties(
    system_map_behavior
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
