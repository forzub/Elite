cmake_minimum_required(VERSION 3.20)

project(NavigationRuntimeControlTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/control"
    "${CMAKE_BINARY_DIR}/elite_navigation_control"
)

add_library(EliteNavigationRuntimeControl STATIC
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationRuntimeControlBridge.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DynamicMotionSystem.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/shared/SharedShipPhysics.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/ship/ShipController.cpp"
)

target_include_directories(EliteNavigationRuntimeControl PUBLIC
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(EliteNavigationRuntimeControl PUBLIC cxx_std_17)
target_link_libraries(EliteNavigationRuntimeControl PUBLIC EliteNavigationControl)

add_executable(
    navigation_runtime_control_tests
    NavigationRuntimeControlTests.cpp
)

target_include_directories(navigation_runtime_control_tests PRIVATE
    "${ELITE_SOURCE_ROOT}"
    "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_runtime_control_tests PRIVATE cxx_std_17)
target_link_libraries(
    navigation_runtime_control_tests
    PRIVATE EliteNavigationRuntimeControl
)

if(MSVC)
    target_compile_options(EliteNavigationRuntimeControl PRIVATE /W4 /permissive-)
    target_compile_options(navigation_runtime_control_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationRuntimeControl PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(navigation_runtime_control_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME navigation_runtime_control
    COMMAND navigation_runtime_control_tests
)

set_tests_properties(
    navigation_runtime_control
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
