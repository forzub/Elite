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

include("${ELITE_SOURCE_ROOT}/cmake/EliteRuckigNavigation.cmake")
elite_add_ruckig_navigation(EliteNavigationRuckig "${ELITE_SOURCE_ROOT}")

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

add_executable(
    guidance_tunnel_local_horizon_tests
    GuidanceTunnelLocalHorizonTests.cpp
    "${ELITE_SOURCE_ROOT}/src/world/navigation/NavigationObstacleGeometry.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/SmoothPathOptimizer.cpp"
    "${ELITE_SOURCE_ROOT}/src/world/navigation/GuidanceTunnel.cpp"
)

foreach(target navigation_guidance_tests guidance_tunnel_local_horizon_tests)
    target_include_directories(
        ${target}
        PRIVATE
            "${ELITE_SOURCE_ROOT}"
            "${ELITE_SOURCE_ROOT}/src"
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

target_link_libraries(
    navigation_guidance_tests
    PRIVATE
        EliteNavigationRuckig
        nlohmann_json::nlohmann_json
)

target_link_libraries(
    guidance_tunnel_local_horizon_tests
    PRIVATE
        nlohmann_json::nlohmann_json
)

add_test(NAME navigation_guidance COMMAND navigation_guidance_tests)
add_test(
    NAME guidance_tunnel_local_horizon
    COMMAND guidance_tunnel_local_horizon_tests
)

set_tests_properties(
    navigation_guidance
    guidance_tunnel_local_horizon
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
