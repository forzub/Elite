cmake_minimum_required(VERSION 3.20)

project(NavigationTrajectoryContinuousBenchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/trajectory"
    "${CMAKE_BINARY_DIR}/elite_navigation_trajectory"
)

add_executable(
    navigation_trajectory_continuous_benchmark
    main.cpp
)

target_include_directories(
    navigation_trajectory_continuous_benchmark
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_trajectory_continuous_benchmark PRIVATE cxx_std_17)
target_link_libraries(navigation_trajectory_continuous_benchmark PRIVATE EliteNavigationTrajectory)

if(MSVC)
    target_compile_options(navigation_trajectory_continuous_benchmark PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_trajectory_continuous_benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()
