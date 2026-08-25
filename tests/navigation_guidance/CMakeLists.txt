cmake_minimum_required(VERSION 3.20)

project(NavigationGuidanceTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json REQUIRED)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_executable(
    navigation_guidance_tests
    NavigationGuidanceTests.cpp
    "${ELITE_SOURCE_ROOT}/src/game/navigation/TrajectoryPredictor.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/TrajectorySafetyEvaluator.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/LocalGuidancePlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/NavigationWorldPredictor.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DockingPathPlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/NavigationObstacleGeometry.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/GeometricPathPlanner.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/SmoothPathOptimizer.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/TrajectoryGenerator.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/GuidanceTunnel.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/GalacticReferenceFrame.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/HubSemanticAnchorCatalog.cpp"
    "${ELITE_SOURCE_ROOT}/src/game/navigation/DockingPortRuntimeStateCatalog.cpp"
)

target_include_directories(
    navigation_guidance_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_link_libraries(
    navigation_guidance_tests
    PRIVATE
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(navigation_guidance_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_guidance_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME navigation_guidance COMMAND navigation_guidance_tests)
set_tests_properties(
    navigation_guidance
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
