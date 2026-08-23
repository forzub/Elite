cmake_minimum_required(VERSION 3.20)

project(NavigationSensorTests LANGUAGES CXX)
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
    navigation_sensor_tests
    NavigationSensorTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/equipment/radar/TestIdealRadarUnit.cpp"
)

target_include_directories(
    navigation_sensor_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(navigation_sensor_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_sensor_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME navigation_sensors COMMAND navigation_sensor_tests)
set_tests_properties(
    navigation_sensors
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
