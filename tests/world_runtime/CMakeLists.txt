cmake_minimum_required(VERSION 3.20)

project(WorldRuntimeTests LANGUAGES CXX)

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
    world_runtime_tests
    WorldRuntimeTests.cpp
    "${ELITE_SOURCE_ROOT}/src/world/celestial/StarAtlasDatabase.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/celestial/CelestialSystemRuntime.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/celestial/CelestialRuntimeRegistry.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DynamicMotionSystem.cpp"
)

target_include_directories(
    world_runtime_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        world_runtime_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        world_runtime_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME world_runtime
    COMMAND world_runtime_tests
)

set_tests_properties(
    world_runtime
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)

add_executable(
    clock_sync_tests
    ClockSyncTests.cpp
)

target_include_directories(
    clock_sync_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(
        clock_sync_tests
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(
        clock_sync_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME clock_sync
    COMMAND clock_sync_tests
)

set_tests_properties(
    clock_sync
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
